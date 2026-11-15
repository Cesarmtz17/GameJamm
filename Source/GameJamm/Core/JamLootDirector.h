#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JamLootDirector.generated.h"

class AJamLootChest;
class AJamCampGuard;

/**
 * Pone el loot de la partida desde Loot.json (GDD 4 y 12): cofre garantizado junto al spawn, cofres de las 4 islas,
 * montones de chatarra en el carril, el cofre raro de la pradera central (minuto 5 y cada 3 min) y los guardias neutrales
 * de campamento junto a los cofres de las islas (GDD 11), que reaparecen. Lo crea AJamGameMode.
 */
UCLASS()
class GAMEJAMM_API AJamLootDirector : public AActor
{
	GENERATED_BODY()

public:
	AJamLootDirector();

	/** Pone (o repone) el cofre raro central ahora. */
	void SpawnRareChest();

protected:
	virtual void BeginPlay() override;

private:
	AJamLootChest* PlaceChest(float X, float Y, const FString& Table, float Respawn, bool bRare, bool bScrap);

	struct FCampSlot
	{
		FVector Home = FVector::ZeroVector;
		bool bThrower = false;
		TWeakObjectPtr<AJamCampGuard> Guard;
		float DeadSince = -1.f;
	};
	void SpawnGuard(FCampSlot& Slot);
	void CheckCamps();

	TArray<FCampSlot> Camps;
	float CampRespawn = 90.f;
	FTimerHandle CampTimer;

	TWeakObjectPtr<AJamLootChest> RareChest;
	FTimerHandle RareTimer;
};
