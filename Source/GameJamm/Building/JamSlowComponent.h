#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JamSlowComponent.generated.h"

/**
 * Estado "lento" (Frost: 50 % por 3 s). Se agrega solo al actor afectado y se quita al expirar.
 * Mecanismo mínimo: AActor::CustomTimeDilation, que frena movimiento, animación y Tick del actor
 * sin tocar UHealthComponent ni el código de movimiento de nadie.
 */
UCLASS(ClassGroup = (Jam))
class GAMEJAMM_API UJamSlowComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJamSlowComponent();

	/** Aplica o renueva el estado. Factor 0.5 = mitad de velocidad. */
	UFUNCTION(BlueprintCallable, Category = "Jam|Status")
	static UJamSlowComponent* ApplySlow(AActor* Target, float Factor = 0.5f, float Duration = 3.f);

	UFUNCTION(BlueprintPure, Category = "Jam|Status")
	static bool IsSlowed(const AActor* Target);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void Restore();

	float EndTime = 0.f;
	float PreviousDilation = 1.f;
	bool bApplied = false;
};
