#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JamTrainingDummy.generated.h"

class UHealthComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Muneco de practica (GDD 8, prueba de armas y fusion): enemigo quieto que registra cada golpe y revive a los 2 s.
 * Muestra los efectos de estado de la fusion (quemado, lento, aturdido, congelado) con color y un aviso corto,
 * porque no se mueve y de otro modo no se veria nada. AJamLootDirector los pone junto al spawn rojo (Loot.json, "dummies").
 */
UCLASS()
class GAMEJAMM_API AJamTrainingDummy : public AActor
{
	GENERATED_BODY()

public:
	AJamTrainingDummy();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dummy")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dummy")
	TObjectPtr<UHealthComponent> Health;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleHealthChanged(UHealthComponent* HealthComponent, float NewHealth, float Delta);

	UFUNCTION()
	void HandleDied(UHealthComponent* HealthComponent, AActor* Killer);

	void Paint(const FLinearColor& Color);

	FTimerHandle ReviveTimer;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Tint;

	/** 0 normal, 1 quemado, 2 lento, 3 aturdido, 4 congelado, 5 derribado. */
	int32 ShownState = 0;
	double LastFxPost[6] = { -10.0, -10.0, -10.0, -10.0, -10.0, -10.0 };
};
