#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Core/JamTypes.h"
#include "JamGameMode.generated.h"

class ATeamStructure;

/**
 * Reglas de la partida (GDD 9-10): Rojo contra Azul; cae la base de un equipo -> gana el otro.
 * 12:00 muerte subita (torres a la mitad de dano, respawn 15 s); 15:00 gana quien tenga mas vida en torres + base.
 * El reloj solo corre si el nivel tiene alguna base (ATeamStructure con bIsBase): los niveles de prueba no terminan solos.
 */
UCLASS()
class GAMEJAMM_API AJamGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AJamGameMode();

	/** Equipo de los jugadores humanos (los bots usan el otro). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jam|Match")
	EJamTeam PlayerTeam = EJamTeam::Red;

	/** Minions cada 30 s y bots heroe (AI/JamWaveSpawner). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jam|Match")
	bool bSpawnWavesAndBots = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jam|Match", meta = (ClampMin = "0"))
	float SuddenDeathSeconds = 12.f * 60.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jam|Match", meta = (ClampMin = "0"))
	float MatchLengthSeconds = 15.f * 60.f;

	/** Respawn = Base + PerMinute * minutos jugados, tope Max (en muerte subita siempre Max). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jam|Respawn")
	float RespawnBaseSeconds = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jam|Respawn")
	float RespawnPerMinuteSeconds = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jam|Respawn")
	float RespawnMaxSeconds = 15.f;

	UFUNCTION(BlueprintCallable, Category = "Jam|Match")
	void NotifyStructureDestroyed(ATeamStructure* Structure, AActor* Killer);

	/** Lo llama el personaje al morir: programa el respawn en su base. */
	UFUNCTION(BlueprintCallable, Category = "Jam|Match")
	void NotifyPlayerDied(APawn* Pawn, AActor* Killer);

	UFUNCTION(BlueprintCallable, Category = "Jam|Match")
	void EndMatch(EJamTeam Winner);

	UFUNCTION(BlueprintPure, Category = "Jam|Respawn")
	float GetRespawnDelay() const;

	/** Segundos que faltan para que reaparezca este controlador (0 si no esta esperando). */
	UFUNCTION(BlueprintPure, Category = "Jam|Respawn")
	float GetRespawnRemaining(const AController* Controller) const;

	/** Vida total (torres + base) de un equipo. */
	UFUNCTION(BlueprintPure, Category = "Jam|Match")
	float GetTeamStructureHealth(EJamTeam Team) const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Jam|Match")
	void OnStructureDestroyed(ATeamStructure* Structure, AActor* Killer);

	UFUNCTION(BlueprintImplementableEvent, Category = "Jam|Match")
	void OnMatchEnded(EJamTeam Winner);

	UFUNCTION(BlueprintImplementableEvent, Category = "Jam|Match")
	void OnSuddenDeath();

	virtual void StartPlay() override;
	virtual void SetPlayerDefaults(APawn* PlayerPawn) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual bool ShouldSpawnAtStartSpot(AController* Player) override { return false; }

protected:
	void MatchTick();
	void Respawn(AController* Controller);
	EJamTeam GetControllerTeam(const AController* Controller) const;

	FTimerHandle MatchTimer;
	TMap<TWeakObjectPtr<AController>, FTimerHandle> RespawnTimers;
	TMap<TWeakObjectPtr<AController>, EJamTeam> ControllerTeams;
};
