#include "Core/JamLootDirector.h"
#include "Core/JamLoot.h"
#include "Core/JamLootChest.h"
#include "GameJamm.h"
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

	const TSharedPtr<FJsonObject>* Rare = nullptr;
	if (Cfg->TryGetObjectField(TEXT("rare_event"), Rare))
	{
		const float First = (*Rare)->GetNumberField(TEXT("first"));
		const float Every = (*Rare)->GetNumberField(TEXT("every"));
		GetWorldTimerManager().SetTimer(RareTimer, this, &AJamLootDirector::SpawnRareChest, FMath::Max(Every, 10.f), true, FMath::Max(First, 1.f));
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
	if (RareChest.IsValid() && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 6.f, FColor(255, 170, 60), TEXT("Cofre raro en la pradera central!"));
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
