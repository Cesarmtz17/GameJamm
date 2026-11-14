#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JamEnergyComponent.generated.h"

class UJamEnergyComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FJamOnEnergyChanged, UJamEnergyComponent*, EnergyComponent, float, NewEnergy, float, Delta);

/**
 * Energía tipo maná del jugador (research 13 §6). Va en el personaje (o en su Controller).
 * Las piezas encendidas de sus construcciones llaman Spend() cada tick; si el dueño de la construcción
 * no tiene este componente, las piezas funcionan sin gastar.
 * - Recarga sola (+RegenPerSecond) solo en reposo: tras RegenDelay segundos sin gastar.
 * - Reserva de baterías pegadas a la construcción: se gasta antes que la barra.
 * - OnEnergyChanged para el HUD (como mucho una vez por frame mientras se gasta).
 */
UCLASS(ClassGroup = (Jam), meta = (BlueprintSpawnableComponent))
class GAMEJAMM_API UJamEnergyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJamEnergyComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Energy", meta = (ClampMin = "1"))
	float MaxEnergy = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Energy", meta = (ClampMin = "0"))
	float StartEnergy = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Energy", meta = (ClampMin = "0"))
	float RegenPerSecond = 2.f;

	/** Segundos sin gastar antes de que empiece la recarga. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Energy", meta = (ClampMin = "0"))
	float RegenDelay = 3.f;

	/** PoC: escribe la barra y el gasto por segundo en pantalla (lo enciende el comando Jam.Energy). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy")
	bool bDebugOnScreen = false;

	/** NewEnergy = barra (sin la reserva); Delta = cambio de barra + reserva desde el último aviso. */
	UPROPERTY(BlueprintAssignable, Category = "Energy")
	FJamOnEnergyChanged OnEnergyChanged;

	UFUNCTION(BlueprintPure, Category = "Energy")
	float GetEnergy() const { return Current; }

	UFUNCTION(BlueprintPure, Category = "Energy")
	float GetEnergyPercent() const { return MaxEnergy > 0.f ? Current / MaxEnergy : 0.f; }

	/** Reserva de las baterías pegadas (segmentos de otro color en el HUD). */
	UFUNCTION(BlueprintPure, Category = "Energy")
	float GetReserve() const { return Reserve; }

	/** Gasto actual por segundo (para mostrar "-2.3/s"). */
	UFUNCTION(BlueprintPure, Category = "Energy")
	float GetDrainPerSecond() const { return DrainPerSecond; }

	UFUNCTION(BlueprintPure, Category = "Energy")
	bool IsEmpty() const { return Current + Reserve <= 0.f; }

	/** Gasta reserva y luego barra. false si no alcanza (y deja todo en 0: los dispositivos se apagan). */
	UFUNCTION(BlueprintCallable, Category = "Energy")
	bool Spend(float Amount);

	/** Suma a la barra (loot de energía), sin pasar de MaxEnergy. */
	UFUNCTION(BlueprintCallable, Category = "Energy")
	void Add(float Amount);

	/** Fija la barra directamente (respawn, depuración). */
	UFUNCTION(BlueprintCallable, Category = "Energy")
	void SetEnergy(float NewEnergy);

	/** Convierte un item "Energy.Small"/"Energy.Large" en energía. false si el item no es de energía. */
	UFUNCTION(BlueprintCallable, Category = "Energy")
	bool AddFromItem(FName ItemId);

	UFUNCTION(BlueprintCallable, Category = "Energy")
	void AddReserve(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Energy")
	void RemoveReserve(float Amount);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	void BroadcastIfChanged();

	float Current = 60.f;
	float Reserve = 0.f;
	float TimeSinceSpend = 1000.f;
	float SpentThisFrame = 0.f;
	float DrainPerSecond = 0.f;
	float LastBroadcastTotal = 60.f;
};
