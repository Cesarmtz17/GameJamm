#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AI/JamMinion.h"
#include "JamWaveSpawner.generated.h"

/**
 * Oleadas de minions (GDD 3) y bots heroe (GDD 11). Lo crea AJamGameMode al empezar una partida con bases.
 * Cada 30 s (la primera a 0:30) sale de cada base: 3 cuerpo a cuerpo + 1 a distancia; desde el minuto 8, +1 pesado.
 * Bots: Rojo (equipo del jugador) 2, Azul 3 = 3 contra 3.
 */
UCLASS()
class GAMEJAMM_API AJamWaveSpawner : public AActor
{
	GENERATED_BODY()

public:
	AJamWaveSpawner();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jam|Waves")
	float WaveInterval = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jam|Waves")
	float FirstWaveDelay = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jam|Waves")
	float HeavyFromSeconds = 8.f * 60.f;

	/** Tope de minions vivos por equipo (rendimiento). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jam|Waves")
	int32 MaxAlivePerTeam = 24;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jam|Bots")
	TArray<EJamAgentKind> RedBots = { EJamAgentKind::Builder, EJamAgentKind::Fighter };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jam|Bots")
	TArray<EJamAgentKind> BlueBots = { EJamAgentKind::Builder, EJamAgentKind::Fighter, EJamAgentKind::Shooter };

	/** Saca una oleada ya (Jam.Wave). */
	UFUNCTION(BlueprintCallable, Category = "Jam|Waves")
	void SpawnWave();

protected:
	virtual void BeginPlay() override;

	void SpawnTeamWave(EJamTeam Team, bool bHeavy);
	void SpawnBots(EJamTeam Team, const TArray<EJamAgentKind>& Kinds);
	AJamMinion* SpawnAgent(UClass* Class, EJamAgentKind Kind, EJamTeam Team, const FVector& Where);

	FTimerHandle WaveTimer;
};
