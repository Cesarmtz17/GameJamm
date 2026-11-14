#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JamTrainingDummy.generated.h"

class UHealthComponent;
class UStaticMeshComponent;

/** Muneco de practica (GDD 8, prueba de armas y fusion): enemigo quieto que registra cada golpe y revive a los 2 s. */
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

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleHealthChanged(UHealthComponent* HealthComponent, float NewHealth, float Delta);

	UFUNCTION()
	void HandleDied(UHealthComponent* HealthComponent, AActor* Killer);

	FTimerHandle ReviveTimer;
};
