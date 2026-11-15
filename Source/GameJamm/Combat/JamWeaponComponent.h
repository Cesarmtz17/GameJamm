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
 * Fusion (GDD 8.2): Fuse / FuseNext (tecla F) pegan un material; sus efectos viven en UJamStatusComponent.
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

	/**
	 * Fusion (GDD 8.2, valores en Content/Game/Combat/Data/Fusion.json): pega al arma en mano el material Mat.<Id> del
	 * inventario. Espada: gasta 1 y dura 20 golpes (Bomba, 1); reemplaza al anterior. Arco: elige la punta y cada flecha gasta 1.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Fusion")
	bool Fuse(FName MaterialId);

	/** Tecla F: fusiona el siguiente material que tengas (Fuego, Hielo, Rayo, Roca, Bomba). */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Fusion")
	bool FuseNext();

	/** Material del arma en mano (None si va sola). */
	UFUNCTION(BlueprintPure, Category = "Weapon|Fusion")
	FName GetFused() const;

	/** Golpes que le quedan a la espada (en el arco: materiales en la mochila). */
	UFUNCTION(BlueprintPure, Category = "Weapon|Fusion")
	int32 GetFusedUses() const;

	/** Intervalo real del arma en mano (la Roca hace lenta la espada). */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	float GetInterval() const;

	/** Efecto del material al pegar (Target puede ser nulo si la flecha dio en el suelo). Lo llaman la espada y AJamArrow. */
	void ApplyFusionHit(FName MaterialId, AActor* Target, const FVector& Where, bool bArrow);

	/**
	 * Pega el arma en mano a un hueso de la figura rigeada (UJamCharacterLook, campo Weapon de Characters.json).
	 * Con mano, el tajo lo hace la animacion Attack y no el balanceo por codigo. Sword/Bow = posicion local en el hueso.
	 */
	void AttachToHand(USceneComponent* Skeleton, FName SwordSocket, const FTransform& Sword, FName BowSocket, const FTransform& Bow);

	/** true si el arma va pegada a un hueso (figura rigeada). */
	bool IsInHand() const { return HandParent.IsValid(); }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	void SwingSword();
	void ShootArrow();
	void GetAim(FVector& OutFrom, FVector& OutDir) const;
	void RefreshHandMesh();
	void RefreshFuseMesh();
	void Explode(const FVector& Where, float Damage, float Radius);
	void Notify(const FString& Text) const;
	float MatNumber(FName MaterialId, const TCHAR* Field, float Default) const;
	TArray<AActor*> EnemiesNear(const FVector& Where, float Radius) const;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> HandMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SwordMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> BowMesh;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> FuseMesh;

	UPROPERTY()
	TMap<FName, TObjectPtr<UStaticMesh>> FuseMeshes;

	FName FusedSword;
	FName FusedBow;
	int32 SwordUses = 0;

	float LastAttackTime = -100.f;
	float SwingAlpha = 1.f; // 0..1 durante el tajo

	TWeakObjectPtr<USceneComponent> HandParent;
	FName SwordSocket, BowSocket;
	FTransform SwordInHand, BowInHand;
};
