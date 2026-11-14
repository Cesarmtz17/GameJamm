#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JamArrow.generated.h"

class UJamWeaponComponent;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

/** Flecha fisica del arco: vuela con gravedad, pega una vez a un enemigo y se queda clavada unos segundos. */
UCLASS()
class GAMEJAMM_API AJamArrow : public AActor
{
	GENERATED_BODY()

public:
	AJamArrow();

	UPROPERTY(VisibleAnywhere, Category = "Arrow")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, Category = "Arrow")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Arrow")
	TObjectPtr<UProjectileMovementComponent> Movement;

	void Launch(UJamWeaponComponent* InWeapon, const FVector& Velocity, float InDamage);

private:
	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);

	void Strike(AActor* Other);

	TWeakObjectPtr<UJamWeaponComponent> Weapon;
	float Damage = 15.f;
	bool bDone = false;
};
