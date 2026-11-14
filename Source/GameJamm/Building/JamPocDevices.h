#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JamPocDevices.generated.h"

class UStaticMeshComponent;
class UHealthComponent;
class UWorld;

/**
 * PoC para VER los dispositivos en el editor sin assets nuevos. Comandos de consola (PIE, tecla ~), en JamPocDevices.cpp:
 *   Jam.Dummy [n]                 blancos con vida (equipo enemigo) delante de la cámara
 *   Jam.Energy [valor|full|empty] barra de energía del jugador en pantalla
 *   Jam.Seat                      monta/desmonta la construcción más cercana; WASD/flechas, Space/Ctrl
 *   Jam.Power [0|1]               prende/apaga la construcción más cercana
 *   Jam.Debug <0|1>               conos, ejes de empuje, torque y objetivo de Head
 */
namespace JamPoc
{
	/** Teclas de depuración del asiento: W/S o flechas = acelerador, A/D o flechas = giro, Space/Ctrl = subir/bajar. */
	GAMEJAMM_API void ReadDriveKeys(const UWorld* World, float& OutThrottle, float& OutSteer, float& OutLift);
}

/** Blanco de prueba: cilindro con UHealthComponent que pasea de lado a lado y dibuja su vida encima. */
UCLASS()
class GAMEJAMM_API AJamPocDummy : public AActor
{
	GENERATED_BODY()

public:
	AJamPocDummy();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PoC")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PoC")
	TObjectPtr<UHealthComponent> Health;

	/** Pasea de lado a lado (así se nota el "lento" de Frost). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PoC")
	bool bPatrol = true;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleHealthChanged(UHealthComponent* HealthComponent, float NewHealth, float Delta);

private:
	FVector Home = FVector::ZeroVector;
	FVector PatrolAxis = FVector::RightVector;
	float Phase = 0.f;
};
