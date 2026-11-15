#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JamStatusComponent.generated.h"

class AController;

/**
 * Estados que deja la fusion (GDD 8.2) sobre quien recibe el golpe: quemadura (dano por segundo), lentitud, congelado y aturdido.
 * Se agrega solo al primer efecto (UJamStatusComponent::Get). Congelado y aturdido dejan la velocidad en 0; lentitud la baja.
 * Sirve tambien para las construcciones (Flamethrower, Frost) si quieren reusarlo.
 */
UCLASS(ClassGroup = (Jam))
class GAMEJAMM_API UJamStatusComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJamStatusComponent();

	/** El de ese actor, creandolo si no tiene. */
	static UJamStatusComponent* Get(AActor* Target);

	void Burn(float DamagePerSecond, float Seconds, AController* By, AActor* Causer);
	void Slow(float SpeedFactor, float Seconds);
	void Freeze(float Seconds);
	void Stun(float Seconds);

	/** Cuenta golpes de hielo seguidos (menos de 1.5 s entre ellos); devuelve cuantos van. */
	int32 AddIceHit();

	UFUNCTION(BlueprintPure, Category = "Status")
	bool IsBurning() const;

	UFUNCTION(BlueprintPure, Category = "Status")
	bool IsFrozen() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	float Now() const;
	void ApplySpeed();

	float BurnUntil = -1.f;
	float BurnDps = 0.f;
	float NextBurnTick = 0.f;
	TWeakObjectPtr<AController> BurnBy;
	TWeakObjectPtr<AActor> BurnCauser;

	float SlowUntil = -1.f;
	float SlowFactor = 1.f;
	float FrozenUntil = -1.f;
	float StunUntil = -1.f;

	int32 IceHits = 0;
	float LastIceHit = -10.f;

	float BaseSpeed = -1.f; // velocidad original mientras dura un efecto de movimiento
};
