#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JamProjectile.generated.h"

class UStaticMeshComponent;
class UPrimitiveComponent;

/**
 * Bala del Cannon: esfera física que al tocar algo con UHealthComponent de otro equipo le aplica daño
 * (UGameplayStatics::ApplyDamage) y desaparece. El equipo sale de su Instigator/Owner (UJamStatics::GetActorTeam).
 */
UCLASS()
class GAMEJAMM_API AJamProjectile : public AActor
{
	GENERATED_BODY()

public:
	AJamProjectile();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float Damage = 25.f;

	/** Velocidad inicial en cm/s (mundo). */
	UFUNCTION(BlueprintCallable, Category = "Projectile")
	void Launch(const FVector& Velocity);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

private:
	bool bSpent = false;
};
