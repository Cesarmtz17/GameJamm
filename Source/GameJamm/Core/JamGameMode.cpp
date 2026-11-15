#include "Core/JamGameMode.h"
#include "AI/JamWaveSpawner.h"
#include "Core/JamCharacter.h"
#include "Core/JamGameState.h"
#include "Core/JamHUD.h"
#include "Core/InventoryComponent.h"
#include "Core/JamMessages.h"
#include "Core/JamLootDirector.h"
#include "Core/JamStatics.h"
#include "Core/HealthComponent.h"
#include "Core/TeamStructure.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "GameJamm.h"

AJamGameMode::AJamGameMode()
{
	GameStateClass = AJamGameState::StaticClass();
	DefaultPawnClass = AJamCharacter::StaticClass();
	HUDClass = AJamHUD::StaticClass();
}

void AJamGameMode::StartPlay()
{
	Super::StartPlay();

	bool bHasBase = false;
	for (TActorIterator<ATeamStructure> It(GetWorld()); It; ++It)
	{
		bHasBase |= It->bIsBase;
	}
	if (bHasBase)
	{
		if (AJamGameState* GS = GetGameState<AJamGameState>())
		{
			GS->bMatchRunning = true;
		}
		GetWorldTimerManager().SetTimer(MatchTimer, this, &AJamGameMode::MatchTick, 1.f, true);
		if (bSpawnWavesAndBots)
		{
			GetWorld()->SpawnActor<AJamWaveSpawner>();
		}
		GetWorld()->SpawnActor<AJamLootDirector>(); // GDD 4: cofres, chatarra y cofre raro (Content/Game/Core/Data/Loot.json)
	}
}

void AJamGameMode::MatchTick()
{
	AJamGameState* GS = GetGameState<AJamGameState>();
	if (!GS || GS->IsMatchOver())
	{
		GetWorldTimerManager().ClearTimer(MatchTimer);
		return;
	}

	GS->MatchSeconds += 1.f;

	if (!GS->bSuddenDeath && GS->MatchSeconds >= SuddenDeathSeconds)
	{
		GS->bSuddenDeath = true;
		for (TActorIterator<ATeamStructure> It(GetWorld()); It; ++It)
		{
			It->AttackDamage *= 0.5f;
		}
		UE_LOG(LogJam, Log, TEXT("Muerte subita"));
		OnSuddenDeath();
	}

	if (GS->MatchSeconds >= MatchLengthSeconds)
	{
		const float Red = GetTeamStructureHealth(EJamTeam::Red);
		const float Blue = GetTeamStructureHealth(EJamTeam::Blue);
		EndMatch(Red > Blue ? EJamTeam::Red : (Blue > Red ? EJamTeam::Blue : EJamTeam::Neutral));
	}
}

float AJamGameMode::GetTeamStructureHealth(EJamTeam Team) const
{
	float Total = 0.f;
	for (TActorIterator<ATeamStructure> It(GetWorld()); It; ++It)
	{
		if (It->GetTeam() == Team && It->Health)
		{
			Total += It->Health->GetHealth();
		}
	}
	return Total;
}

void AJamGameMode::NotifyStructureDestroyed(ATeamStructure* Structure, AActor* Killer)
{
	if (!Structure)
	{
		return;
	}

	UE_LOG(LogJam, Log, TEXT("Structure destroyed: %s (base=%d)"), *Structure->GetName(), Structure->bIsBase ? 1 : 0);
	OnStructureDestroyed(Structure, Killer);

	if (Structure->bIsBase)
	{
		EndMatch(UJamStatics::GetOpposingTeam(Structure->GetTeam()));
	}
}

void AJamGameMode::EndMatch(EJamTeam Winner)
{
	AJamGameState* JamGameState = GetGameState<AJamGameState>();
	if (!JamGameState || JamGameState->IsMatchOver())
	{
		return;
	}

	UE_LOG(LogJam, Log, TEXT("Match over. Winner: %s"), *UEnum::GetValueAsString(Winner));
	GetWorldTimerManager().ClearTimer(MatchTimer);
	JamGameState->SetMatchResult(Winner);
	OnMatchEnded(Winner);
}

float AJamGameMode::GetRespawnDelay() const
{
	const AJamGameState* GS = GetGameState<AJamGameState>();
	if (GS && GS->bSuddenDeath)
	{
		return RespawnMaxSeconds;
	}
	const float Minutes = GS ? FMath::FloorToFloat(GS->MatchSeconds / 60.f) : 0.f;
	return FMath::Min(RespawnBaseSeconds + RespawnPerMinuteSeconds * Minutes, RespawnMaxSeconds);
}

float AJamGameMode::GetRespawnRemaining(const AController* Controller) const
{
	if (const FTimerHandle* Handle = RespawnTimers.Find(Controller))
	{
		return FMath::Max(0.f, GetWorldTimerManager().GetTimerRemaining(*Handle));
	}
	return 0.f;
}

void AJamGameMode::NotifyPlayerDied(APawn* Pawn, AActor* Killer)
{
	AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Controller)
	{
		return;
	}
	ControllerTeams.Add(Controller, UJamStatics::GetActorTeam(Pawn));

	const AJamGameState* GS = GetGameState<AJamGameState>();
	if (GS && GS->IsMatchOver())
	{
		return;
	}

	FTimerHandle& Handle = RespawnTimers.FindOrAdd(Controller);
	FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &AJamGameMode::Respawn, Controller);
	GetWorldTimerManager().SetTimer(Handle, Delegate, GetRespawnDelay(), false);
	UE_LOG(LogJam, Log, TEXT("%s murio; reaparece en %.0f s"), *Pawn->GetName(), GetRespawnDelay());
}

void AJamGameMode::Respawn(AController* Controller)
{
	RespawnTimers.Remove(Controller);
	if (!IsValid(Controller))
	{
		return;
	}
	// Renacer con la mitad (GDD 4): el peon viejo ya solto la otra mitad en la bolsa al morir; lo que le quedo se guarda para el nuevo.
	const bool bPlayer = Controller->IsPlayerController();
	TMap<FName, int32> Carried;
	if (APawn* Old = Controller->GetPawn())
	{
		if (const UInventoryComponent* OldInv = bPlayer ? Old->FindComponentByClass<UInventoryComponent>() : nullptr)
		{
			for (const auto& Item : OldInv->Items) { if (Item.Value > 0) { Carried.Add(Item.Key, Item.Value); } }
		}
		Controller->UnPossess();
		Old->Destroy();
	}
	RestartPlayer(Controller);
	if (bPlayer)
	{
		UInventoryComponent* NewInv = Controller->GetPawn() ? Controller->GetPawn()->FindComponentByClass<UInventoryComponent>() : nullptr;
		if (NewInv)
		{
			// Reemplaza (no suma): el peon nuevo ya recibio sus StartingItems en BeginPlay. Si no traia nada, se queda con ellos.
			if (Carried.Num() > 0)
			{
				NewInv->ClearInventory();
				for (const auto& Item : Carried) { NewInv->AddItem(Item.Key, Item.Value); }
			}
			int32 Pieces = 0;
			for (const auto& Item : NewInv->Items) { if (Item.Key.ToString().StartsWith(TEXT("Part."))) { Pieces += FMath::Max(Item.Value, 0); } }
			JamMsg::Post(FString::Printf(TEXT("Reapareciste con %d %s"), Pieces, Pieces == 1 ? TEXT("pieza") : TEXT("piezas")), JamMsg::Info, 5.f, FName(TEXT("respawn")));
		}
	}
}

EJamTeam AJamGameMode::GetControllerTeam(const AController* Controller) const
{
	if (const EJamTeam* Found = ControllerTeams.Find(Controller))
	{
		return *Found;
	}
	return Controller && Controller->IsPlayerController() ? PlayerTeam : UJamStatics::GetOpposingTeam(PlayerTeam);
}

void AJamGameMode::SetPlayerDefaults(APawn* PlayerPawn)
{
	Super::SetPlayerDefaults(PlayerPawn);
	if (UHealthComponent* Health = PlayerPawn ? PlayerPawn->FindComponentByClass<UHealthComponent>() : nullptr)
	{
		Health->Team = GetControllerTeam(PlayerPawn->GetController());
	}
}

AActor* AJamGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// PlayerStart con PlayerStartTag = "Red" / "Blue" segun el equipo; si no hay, el de UE.
	const FName Tag = GetControllerTeam(Player) == EJamTeam::Blue ? FName(TEXT("Blue")) : FName(TEXT("Red"));
	TArray<APlayerStart*> Starts;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		if (It->PlayerStartTag == Tag)
		{
			Starts.Add(*It);
		}
	}
	if (Starts.Num() > 0)
	{
		return Starts[FMath::RandHelper(Starts.Num())];
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}
