#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/JamTypes.h"
#include "TeamStructure.generated.h"

class UStaticMeshComponent;
class UHealthComponent;

/**
 * Torre o base de un equipo. Hacer BP hijo (BP_Tower, BP_Base) para mesh/VFX.
 * - Equipo y vida se configuran en el componente Health.
 * - Prerequisites: torres que deben caer antes de que esta reciba daño (estilo MOBA).
 * - Ataca automáticamente al enemigo (equipo contrario) más cercano en rango.
 * - Si bIsBase y muere -> el GameMode termina la partida.
 */
UCLASS()
class GAMEJAMM_API ATeamStructure : public AActor
{
	GENERATED_BODY()

public:
	ATeamStructure();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Structure")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Structure")
	TObjectPtr<UHealthComponent> Health;

	/** Rayo visible del disparo (cilindro emisivo que se estira hasta el objetivo ~0.15 s). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Structure")
	TObjectPtr<UStaticMeshComponent> Bolt;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structure")
	bool bIsBase = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Structure")
	TArray<TObjectPtr<ATeamStructure>> Prerequisites;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structure|Attack")
	bool bCanAttack = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structure|Attack", meta = (ClampMin = "0"))
	float AttackRange = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structure|Attack", meta = (ClampMin = "0"))
	float AttackDamage = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structure|Attack", meta = (ClampMin = "0.1"))
	float AttackInterval = 1.5f;

	/** Dano del primer disparo a jugadores y bots (AttackDamage queda para minions). Con 150 HP y la rampa aguantas ~15 s parado debajo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structure|Attack", meta = (ClampMin = "0"))
	float HeroDamage = 10.f;

	/** Cada disparo seguido al mismo heroe suma esto al multiplicador (castiga quedarse, no entrar). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structure|Attack", meta = (ClampMin = "0"))
	float HeroRampPerShot = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structure|Attack", meta = (ClampMin = "1"))
	float HeroRampMax = 1.5f;

	UFUNCTION(BlueprintPure, Category = "Structure")
	EJamTeam GetTeam() const;

	/** GDD 3: quien golpea la torre sin minions aliados cerca (este radio) hace 50 % de dano. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structure")
	float EscortRadius = 1500.f;

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	/** false mientras algún prerequisito siga vivo. */
	UFUNCTION(BlueprintPure, Category = "Structure")
	bool IsVulnerable() const;

	/** Para VFX/SFX del disparo (el daño ya se aplicó). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Structure")
	void OnAttack(AActor* Target);

	UFUNCTION(BlueprintImplementableEvent, Category = "Structure")
	void OnBecameVulnerable();

	/** Para cambiar el mesh a ruinas, explosión, etc. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Structure")
	void OnStructureDestroyed(AActor* Killer);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleDied(UHealthComponent* HealthComponent, AActor* Killer);

	UFUNCTION()
	void HandlePrerequisiteDied(UHealthComponent* HealthComponent, AActor* Killer);

	void RefreshVulnerability();
	void TryAttack();
	AActor* FindTarget() const;

	TWeakObjectPtr<AActor> LastTarget;
	int32 ShotsOnTarget = 0;

	void HideBolt();

	FTimerHandle AttackTimer;
	FTimerHandle BoltTimer;
};
