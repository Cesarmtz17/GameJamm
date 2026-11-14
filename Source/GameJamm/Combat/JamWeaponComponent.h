#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JamWeaponComponent.generated.h"

class UStaticMeshComponent;
class UStaticMesh;

UENUM(BlueprintType)
enum class EJamWeapon : uint8
{
	Sword,
	Bow
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FJamOnWeaponHit, UJamWeaponComponent*, Weapon, AActor*, Target, float, Damage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FJamOnWeaponAttack, UJamWeaponComponent*, Weapon, EJamWeapon, Kind);

/**
 * Armas del jugador (GDD 8.1): espada (20 cada 0.6 s, 2 m) y arco (15 por flecha cada 0.8 s, 40 m, flecha fisica).
 * El dano va por ApplyDamage a UHealthComponent y nunca a aliados. Valores ajustables en BP sin recompilar.
 * Fusion (GDD 8.2) se engancha despues sobre OnWeaponHit / BonusDamage.
 */
UCLASS(ClassGroup = (Jam), meta = (BlueprintSpawnableComponent))
class GAMEJAMM_API UJamWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJamWeaponComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	EJamWeapon Current = EJamWeapon::Sword;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Sword")
	float SwordDamage = 20.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Sword")
	float SwordInterval = 0.6f;

	/** Alcance del golpe (cm) medido desde el centro del personaje. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Sword")
	float SwordRange = 200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Sword")
	float SwordKnockback = 450.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Bow")
	float BowDamage = 15.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Bow")
	float BowInterval = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Bow")
	float ArrowSpeed = 4500.f;

	/** Distancia de apuntado (cm): a donde mira la camara, hasta 40 m. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Bow")
	float BowRange = 4000.f;

	/** Dano extra de un material fusionado (lo llena Fusion). */
	UPROPERTY(BlueprintReadWrite, Category = "Weapon")
	float BonusDamage = 0.f;

	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FJamOnWeaponHit OnWeaponHit;

	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FJamOnWeaponAttack OnWeaponAttack;

	/** Ataca con el arma en mano si ya paso su intervalo. true si ataco. */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	bool Attack();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void NextWeapon();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void SetWeapon(EJamWeapon Weapon);

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsReady() const;

	/** Lo llaman la espada y la flecha al pegar: aplica dano (sin aliados) y avisa. true si hubo dano. */
	bool DealDamage(AActor* Target, float Damage, const FVector& Push);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	void SwingSword();
	void ShootArrow();
	void GetAim(FVector& OutFrom, FVector& OutDir) const;
	void RefreshHandMesh();

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> HandMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SwordMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> BowMesh;

	float LastAttackTime = -100.f;
	float SwingAlpha = 1.f; // 0..1 durante el tajo
};
