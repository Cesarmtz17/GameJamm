#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JamArrow.generated.h"

class UJamWeaponComponent;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;
class UStaticMesh;

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

	/** Material fusionado en la punta (None = flecha normal) y empuje al pegar. Se fijan antes de Launch. */
	FName Fused;
	float Push = 150.f;

	/** Muestra el material en la punta. */
	void ShowFuse(UStaticMesh* FuseMesh);

	/**
	 * La convierte en disparo del arma pedida (GDD 5.1), antes de Launch: malla y tamano propios (MeshInShot = transform
	 * local de la malla), color si bGlow, gravedad (0 = linea recta, laser), segundos de vida (alcance / velocidad) y
	 * bAreaEffect = como aplica el efecto de Fused (true = de flecha, en area; false = de golpe, solo al blanco).
	 * Un disparo no se queda clavado: desaparece al pegar.
	 */
	void SetupShot(UStaticMesh* ShotMesh, const FTransform& MeshInShot, const FLinearColor& Color, bool bGlow, float GravityScale, float LifeSeconds, bool bAreaEffect);

	/** Dos disparos de la misma rafaga (perdigones) no chocan entre si. */
	void IgnoreShot(AJamArrow* Other);

private:
	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);

	void Strike(AActor* Other);

	TWeakObjectPtr<UJamWeaponComponent> Weapon;
	float Damage = 15.f;
	bool bDone = false;
	bool bShot = false;      // disparo del arma pedida: no se clava
	bool bFuseArea = true;   // el efecto de Fused se aplica como flecha (en area)
};
