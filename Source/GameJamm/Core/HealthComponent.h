#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/JamTypes.h"
#include "HealthComponent.generated.h"

class UDamageType;
class AController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FJamOnHealthChanged, UHealthComponent*, HealthComponent, float, NewHealth, float, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FJamOnDied, UHealthComponent*, HealthComponent, AActor*, Killer);

/**
 * Vida + equipo. CONTRATO COMPARTIDO: todo lo que recibe daño (personajes, bots, torres, base, piezas)
 * lleva este componente. El daño entra por UGameplayStatics::ApplyDamage / ApplyPointDamage normal de UE.
 */
UCLASS(ClassGroup = (Jam), meta = (BlueprintSpawnableComponent))
class GAMEJAMM_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	EJamTeam Team = EJamTeam::Neutral;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health", meta = (ClampMin = "1"))
	float MaxHealth = 100.f;

	/** Ignora todo el daño (p. ej. torre protegida por otra torre). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	bool bInvulnerable = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	bool bAllowFriendlyFire = false;

	/** Destruye al dueño al morir (útil para piezas/monstruos). Personajes: false y manejar respawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	bool bDestroyOwnerOnDeath = false;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FJamOnHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FJamOnDied OnDied;

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealthPercent() const { return MaxHealth > 0.f ? Health / MaxHealth : 0.f; }

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const { return bDead; }

	UFUNCTION(BlueprintCallable, Category = "Health")
	void Heal(float Amount);

	/** Vida al máximo y revive (respawn). */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void ResetHealth();

	UFUNCTION(BlueprintCallable, Category = "Health")
	void Kill(AActor* Killer);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	UFUNCTION()
	void OnRep_Health(float OldHealth);

	UPROPERTY(ReplicatedUsing = OnRep_Health)
	float Health = 100.f;

	UPROPERTY(Replicated)
	bool bDead = false;
};
