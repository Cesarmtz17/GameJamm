#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JamLootDirector.generated.h"

class AJamLootChest;

/**
 * Pone el loot de la partida desde Loot.json (GDD 4 y 12): cofre garantizado junto al spawn, cofres de las 4 islas,
 * montones de chatarra en el carril y el cofre raro de la pradera central (minuto 5 y cada 3 min). Lo crea AJamGameMode.
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

	TWeakObjectPtr<AJamLootChest> RareChest;
	FTimerHandle RareTimer;
};
