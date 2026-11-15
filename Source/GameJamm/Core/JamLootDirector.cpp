#include "Core/JamLootDirector.h"
#include "Core/JamLoot.h"
#include "Core/JamLootChest.h"
#include "Core/JamMessages.h"
#include "AI/JamCampGuard.h"
#include "Combat/JamTrainingDummy.h"
#include "GameJamm.h"
#include "Core/HealthComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"

AJamLootDirector::AJamLootDirector()
{
	PrimaryActorTick.bCanEverTick = false;
}

AJamLootChest* AJamLootDirector::PlaceChest(float X, float Y, const FString& Table, float Respawn, bool bRare, bool bScrap)
{
	FVector Ground;
	bool bFound = JamLoot::GroundAt(GetWorld(), X, Y, Ground, this);
	// Sin suelo justo ahi (hueco, puente): busca en anillos de 2, 4 y 6 m.
	for (int32 Ring = 1; Ring <= 3 && !bFound; ++Ring)
	{
		for (int32 k = 0; k < 8 && !bFound; ++k)
		{
			const float A = k * PI / 4.f;
			bFound = JamLoot::GroundAt(GetWorld(), X + 200.f * Ring * FMath::Cos(A), Y + 200.f * Ring * FMath::Sin(A), Ground, this);
		}
	}
	if (!bFound)
	{
		UE_LOG(LogJam, Warning, TEXT("Loot: sin suelo en (%.0f, %.0f), no pongo %s"), X, Y, *Table);
		return nullptr;
	}
	// Mirando hacia el centro del mapa, para que la tapa abra hacia el carril.
	const FRotator Face(0.f, FMath::RadiansToDegrees(FMath::Atan2(-Y, -X)) - 90.f, 0.f);
	const FTransform T(Face, Ground);
	AJamLootChest* C = GetWorld()->SpawnActorDeferred<AJamLootChest>(AJamLootChest::StaticClass(), T, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!C)
	{
		return nullptr;
	}
	C->Table = Table;
	C->RespawnSeconds = Respawn;
	C->SetLook(bRare, bScrap);
	C->FinishSpawning(T);
	UE_LOG(LogJam, Log, TEXT("Loot: %s en %s"), bScrap ? TEXT("chatarra") : *(TEXT("cofre ") + Table), *Ground.ToCompactString());
	return C;
}

void AJamLootDirector::BeginPlay()
{
	Super::BeginPlay();
	const TSharedPtr<FJsonObject> Cfg = JamLoot::Config();

	const TSharedPtr<FJsonObject>* Starter = nullptr;
	if (Cfg->TryGetObjectField(TEXT("starter"), Starter))
	{
		if (AJamLootChest* C = PlaceChest((*Starter)->GetNumberField(TEXT("x")), (*Starter)->GetNumberField(TEXT("y")), TEXT("starter"), -1.f, false, false))
		{
			const TSharedPtr<FJsonObject>* Items = nullptr;
			if ((*Starter)->TryGetObjectField(TEXT("items"), Items))
			{
				for (const auto& It : (*Items)->Values) { C->FixedItems.Add(FName(*It.Key), int32(It.Value->AsNumber())); }
			}
			(*Starter)->TryGetStringField(TEXT("message"), C->Message);
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* Chests = nullptr;
	if (Cfg->TryGetArrayField(TEXT("chests"), Chests))
	{
		for (const auto& V : *Chests)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			PlaceChest(O->GetNumberField(TEXT("x")), O->GetNumberField(TEXT("y")), O->GetStringField(TEXT("tier")), O->GetNumberField(TEXT("respawn")), false, false);
		}
	}

	double ScrapRespawn = 45.0;
	Cfg->TryGetNumberField(TEXT("scrap_respawn"), ScrapRespawn);
	const TArray<TSharedPtr<FJsonValue>>* Scrap = nullptr;
	if (Cfg->TryGetArrayField(TEXT("scrap"), Scrap))
	{
		for (const auto& V : *Scrap)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			PlaceChest(O->GetNumberField(TEXT("x")), O->GetNumberField(TEXT("y")), TEXT("scrap"), ScrapRespawn, false, true);
		}
	}

	// Campamentos neutrales: guardias y lanzadores en anillo alrededor de (x, y).
	double Respawn = 90.0;
	Cfg->TryGetNumberField(TEXT("camp_respawn"), Respawn);
	CampRespawn = float(Respawn);
	const TArray<TSharedPtr<FJsonValue>>* CampList = nullptr;
	if (Cfg->TryGetArrayField(TEXT("camps"), CampList))
	{
		for (const auto& V : *CampList)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			const float X = O->GetNumberField(TEXT("x")), Y = O->GetNumberField(TEXT("y"));
			const int32 NG = O->GetIntegerField(TEXT("guards")), NT = O->GetIntegerField(TEXT("throwers"));
			for (int32 i = 0; i < NG + NT; ++i)
			{
				const float A = 2.f * PI * i / FMath::Max(1, NG + NT);
				const float R = i < NG ? 350.f : 600.f; // los lanzadores atras
				FVector Ground;
				if (!JamLoot::GroundAt(GetWorld(), X + R * FMath::Cos(A), Y + R * FMath::Sin(A), Ground, this)
					&& !JamLoot::GroundAt(GetWorld(), X + 0.5f * R * FMath::Cos(A), Y + 0.5f * R * FMath::Sin(A), Ground, this))
				{
					continue;
				}
				FCampSlot& Slot = Camps.AddDefaulted_GetRef();
				Slot.Home = Ground + FVector(0, 0, 100.f);
				Slot.bThrower = i >= NG;
				SpawnGuard(Slot);
			}
		}
		UE_LOG(LogJam, Log, TEXT("Campamentos: %d guardias en %d islas"), Camps.Num(), CampList->Num());
		GetWorldTimerManager().SetTimer(CampTimer, this, &AJamLootDirector::CheckCamps, 5.f, true);
	}

	// Munecos de practica junto al spawn rojo (GDD 8). Se crean por codigo desde "dummies"; nunca van guardados en L_Main.
	const TArray<TSharedPtr<FJsonValue>>* Dummies = nullptr;
	if (Cfg->TryGetArrayField(TEXT("dummies"), Dummies))
	{
		int32 Placed = 0;
		for (const auto& V : *Dummies)
		{
			const TSharedPtr<FJsonObject> O = V.IsValid() ? V->AsObject() : nullptr;
			double DX = 0.0, DY = 0.0;
			if (!O || !O->TryGetNumberField(TEXT("x"), DX) || !O->TryGetNumberField(TEXT("y"), DY)) { continue; }
			FVector Ground;
			if (!JamLoot::GroundAt(GetWorld(), float(DX), float(DY), Ground, this))
			{
				UE_LOG(LogJam, Warning, TEXT("Loot: sin suelo para el muneco en (%.0f, %.0f), no lo pongo"), DX, DY);
				continue;
			}
			// El cilindro mide 180 cm con el pivote al centro: se sube 90 cm para que no quede enterrado. Mira hacia el spawn (X = -10800).
			const FVector At = Ground + FVector(0.f, 0.f, 90.f);
			const FTransform T(FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(-At.Y, -10800.f - At.X)), 0.f), At);
			if (AJamTrainingDummy* D = GetWorld()->SpawnActorDeferred<AJamTrainingDummy>(AJamTrainingDummy::StaticClass(), T, this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn))
			{
				// Neutral: el dano del jugador cuenta (AreEnemies) y los minions, bots y torres ignoran a los neutrales.
				D->Health->Team = EJamTeam::Neutral;
				D->FinishSpawning(T);
				++Placed;
			}
		}
		UE_LOG(LogJam, Log, TEXT("Loot: %d munecos de practica"), Placed);
	}

	const TSharedPtr<FJsonObject>* Rare = nullptr;
	if (Cfg->TryGetObjectField(TEXT("rare_event"), Rare))
	{
		const float First = (*Rare)->GetNumberField(TEXT("first"));
		const float Every = (*Rare)->GetNumberField(TEXT("every"));
		GetWorldTimerManager().SetTimer(RareTimer, this, &AJamLootDirector::SpawnRareChest, FMath::Max(Every, 10.f), true, FMath::Max(First, 1.f));
	}
}

void AJamLootDirector::SpawnGuard(FCampSlot& Slot)
{
	const FTransform T(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Slot.Home);
	AJamCampGuard* G = GetWorld()->SpawnActorDeferred<AJamCampGuard>(AJamCampGuard::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!G)
	{
		return;
	}
	G->InitGuard(Slot.bThrower, Slot.Home);
	G->FinishSpawning(T);
	G->SpawnDefaultController();
	Slot.Guard = G;
	Slot.DeadSince = -1.f;
}

void AJamLootDirector::CheckCamps()
{
	const float Now = GetWorld()->GetTimeSeconds();
	for (FCampSlot& Slot : Camps)
	{
		const AJamCampGuard* G = Slot.Guard.Get();
		const bool bAlive = G && G->Health && !G->Health->IsDead();
		if (bAlive) { continue; }
		if (Slot.DeadSince < 0.f) { Slot.DeadSince = Now; }
		else if (Now - Slot.DeadSince >= CampRespawn) { SpawnGuard(Slot); }
	}
}

void AJamLootDirector::SpawnRareChest()
{
	if (AJamLootChest* Old = RareChest.Get())
	{
		if (!Old->IsOpen()) { return; } // nadie lo ha tomado todavia
		Old->Destroy();
	}
	const TSharedPtr<FJsonObject>* Rare = nullptr;
	if (!JamLoot::Config()->TryGetObjectField(TEXT("rare_event"), Rare)) { return; }
	RareChest = PlaceChest((*Rare)->GetNumberField(TEXT("x")), (*Rare)->GetNumberField(TEXT("y")), TEXT("rare"), -1.f, true, false);
	if (RareChest.IsValid())
	{
		JamMsg::Big(TEXT("Cofre raro en la pradera central"), JamMsg::Loot, 6.f, TEXT("rare"));
	}
}

namespace
{
	void JamRare(const TArray<FString>&, UWorld* World)
	{
		for (TActorIterator<AJamLootDirector> It(World); It; ++It) { It->SpawnRareChest(); return; }
	}
	FAutoConsoleCommandWithWorldAndArgs CmdRare(TEXT("Jam.Rare"), TEXT("Pone ya el cofre raro de la pradera central."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamRare));
}
