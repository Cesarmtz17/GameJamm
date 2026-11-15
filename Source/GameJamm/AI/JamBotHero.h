#pragma once

#include "CoreMinimal.h"
#include "AI/JamMinion.h"
#include "JamBotHero.generated.h"

class AJamBuildSpawner;
class AJamBuildPart;

/**
 * Bot heroe (GDD 11): completa el 3v3. Misma maquina de estados que el minion mas:
 *   Retirarse (vida < 30 %: vuelve a su base y se cura) -> Defender (si atacan su torre) -> Empujar con la oleada.
 * Constructor: pide al asistente de construccion frases fijas que rotan y, si lo armado tiene asiento, se sube y lo maneja
 *   (ruedas con giro diferencial y par de guinada, como el jugador): va por el carril o hacia el enemigo cercano, enciende
 *   las armas (lanzallamas, canon...) solo cuando lo tiene enfrente, frena antes de un hueco y se baja si se atora o se vuelca.
 * Peleador: pone una trampa (TrapPrompts, p. ej. torreta con lanzallamas) a un lado del carril en su mitad del mapa; las armas
 *   de la trampa solo se encienden con un enemigo a menos de TrapRange. Una a la vez, dura TrapLife segundos.
 * Al morir reaparece en su base con el mismo tiempo que el jugador. No da energia al morir (no es minion).
 * Pendiente: lootear (llega con la tarea 4) y la espada/arco de Combate (tarea 2); hoy pega con el ataque base de su rol.
 */
UCLASS()
class GAMEJAMM_API AJamBotHero : public AJamMinion
{
	GENERATED_BODY()

public:
	AJamBotHero();

	virtual void InitAgent(EJamAgentKind InKind, EJamTeam InTeam) override;

	/** Frases que rota el Constructor (GDD 11). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jam|Bot")
	TArray<FString> BuildPrompts;

	/** Segundos entre construcciones del Constructor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jam|Bot")
	float BuildEvery = 60.f;

	/** Frases de trampa que rota el Peleador. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jam|Bot")
	TArray<FString> TrapPrompts;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jam|Bot")
	float TrapEvery = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jam|Bot")
	float TrapLife = 90.f;

	/** Distancia a la que la trampa enciende sus armas. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jam|Bot")
	float TrapRange = 700.f;

	/** Mitad del largo de la pradera central (|X|): ahi arma y maneja el carro; mas alla el piso es angosto. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jam|Bot")
	float DriveHalfX = 3900.f;

	/** true mientras va montado en su construccion. */
	UFUNCTION(BlueprintPure, Category = "Jam|Bot")
	bool IsDriving() const { return bMounted; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void Think() override;
	virtual void HandleDied(UHealthComponent* HealthComponent, AActor* Killer) override;

	void Respawn();
	void BuildSomething();
	void BuildTrap();
	void TickTrap();
	void MountBuild();
	void Dismount(const TCHAR* Why);
	void Drive(float DeltaSeconds);
	void ClearLater(AJamBuildSpawner* S, float Seconds);
	FVector HomeLocation() const;

	bool bRetreating = false;
	int32 PromptIndex = 0;
	float NextBuildTime = 0.f;
	TWeakObjectPtr<AJamBuildSpawner> MyBuild;
	TWeakObjectPtr<AJamBuildSpawner> MyTrap;
	TWeakObjectPtr<AJamBuildPart> Seat;
	bool bMounted = false;
	int32 TrapIndex = 0;
	float NextTrapTime = 0.f;
	float TrapUntil = 0.f;
	float MountAt = -1.f;
	float SmoothThrottle = 0.f;
	float SmoothSteer = 0.f;
	float SlowSince = -1.f;
	float ReverseUntil = 0.f;
	float DetourUntil = 0.f;
	float LastCarHop = -100.f;
	float DetourY = 0.f;
	float UpsideSince = -1.f;
	int32 Reverses = 0;
	float DriveLogAt = 0.f;
	FTimerHandle RespawnTimer;
};
