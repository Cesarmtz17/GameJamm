#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/JamTypes.h"
#include "JamMinion.generated.h"

class UHealthComponent;
class UStaticMeshComponent;

/** Tipo de agente: minions del carril (GDD 3) y roles de bot heroe (GDD 11). */
UENUM(BlueprintType)
enum class EJamAgentKind : uint8
{
	Melee,
	Ranged,
	Heavy,
	Fighter,   // bot heroe: pelea cuerpo a cuerpo
	Shooter,   // bot heroe: dispara desde lejos
	Builder    // bot heroe: construye con el asistente y empuja
};

/**
 * Minion del carril. Maquina de estados simple en C++ (sin StateTree, por tiempo):
 *   avanzar por el carril (Y = 0, de su base a la base enemiga) -> pelear con el enemigo mas cercano en rango -> torre.
 * GDD 3: cuerpo a cuerpo 60 HP / 8, a distancia 40 HP / 6, pesado 200 HP / 15; matar uno da +3 de energia al jugador.
 * Todo el dano va por ApplyDamage -> UHealthComponent (nunca a aliados: lo filtra el componente).
 */
UCLASS()
class GAMEJAMM_API AJamMinion : public ACharacter
{
	GENERATED_BODY()

public:
	AJamMinion();

	/** Llamar antes de FinishSpawning (SpawnActorDeferred): pone vida, dano, color y equipo. */
	virtual void InitAgent(EJamAgentKind InKind, EJamTeam InTeam);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Jam")
	TObjectPtr<UHealthComponent> Health;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Jam")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Jam")
	TObjectPtr<UStaticMeshComponent> Head;

	/** Rayo visible del ataque a distancia. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Jam")
	TObjectPtr<UStaticMeshComponent> Bolt;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Jam")
	EJamAgentKind Kind = EJamAgentKind::Melee;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jam")
	float AttackRange = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jam")
	float AttackDamage = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jam")
	float AttackInterval = 1.f;

	/** Distancia a la que ve enemigos y deja el carril para pelear. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jam")
	float AggroRange = 1200.f;

	/** Energia que recibe el jugador que lo mata. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jam")
	float EnergyReward = 3.f;

	UFUNCTION(BlueprintPure, Category = "Jam")
	EJamTeam GetTeam() const;

	virtual void Tick(float DeltaSeconds) override;

	/** X de la base de un equipo (map_layout.json). */
	static float BaseX(EJamTeam Team) { return Team == EJamTeam::Blue ? 12400.f : -12400.f; }

protected:
	virtual void BeginPlay() override;

	/** Decide cada 0.2 s. Los bots heroe la extienden. */
	virtual void Think();

	UFUNCTION()
	virtual void HandleDied(UHealthComponent* HealthComponent, AActor* Killer);

	AActor* FindTarget(float Range) const;
	void MoveTo(const FVector& Goal);
	void StopMoving() { bHasGoal = false; }
	bool TryAttack(AActor* Target);
	FVector LaneGoal() const;
	void SetColor(const FLinearColor& Color);
	void HideBolt();

	bool bHasGoal = false;
	FVector Goal = FVector::ZeroVector;
	float LastAttack = -100.f;
	FTimerHandle ThinkTimer;
	FTimerHandle BoltTimer;
};
