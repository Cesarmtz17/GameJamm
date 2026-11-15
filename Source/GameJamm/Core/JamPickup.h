#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JamPickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;

/**
 * Pickup en el suelo (GDD 4 y 5): pieza, material, orbe de energia o bolsa de muerte. Se toma al pasar encima;
 * lo que no cabe en la mochila se queda. Desaparece a los LifeSeconds (0 = nunca).
 */
UCLASS()
class GAMEJAMM_API AJamPickup : public AActor
{
	GENERATED_BODY()

public:
	AJamPickup();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	TMap<FName, int32> Items;

	/** Energia que da (orbe). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	float Energy = 0.f;

	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<USphereComponent> Trigger;

	/** Suelta un pickup con estas piezas y/o energia (bolsa de muerte, sobrantes de un cofre, orbes). */
	static AJamPickup* SpawnBundle(UWorld* World, const FVector& At, const TMap<FName, int32>& Items, float Energy, float LifeSeconds);

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);

	void RefreshLook();

	UPROPERTY()
	TObjectPtr<class UStaticMesh> PartMesh;

	UPROPERTY()
	TObjectPtr<class UStaticMesh> MaterialMesh;

	UPROPERTY()
	TObjectPtr<class UStaticMesh> OrbMesh;

	FVector Home = FVector::ZeroVector;
};
