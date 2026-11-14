#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/JamGameMode.h"
#include "JamPlayConsole.generated.h"

class UCameraComponent;

/**
 * Banco de pruebas jugable de construccion (rol Building). Comandos de consola (tecla ~), en JamPlayConsole.cpp:
 *   Jam.Say <texto libre>  -> prompt -> modelo de decision (Jev) -> receta -> ensamblador, ~6 m delante del jugador
 *   Jam.Clear              -> borra todo lo construido
 *   Jam.Seat               -> se sube al asiento de la construccion mas cercana / se baja
 * No es el personaje final del juego: es lo minimo para caminar y probar sin assets ni input mappings.
 */
UCLASS()
class GAMEJAMM_API AJamTestCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AJamTestCharacter();
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	/** Grados por unidad de movimiento del mouse. */
	UPROPERTY(EditAnywhere, Category = "Camera")
	float MouseSensitivity = 0.2f;
};

/** GameMode del nivel de pruebas: el de la partida + AJamTestCharacter (WASD, mouse, espacio). */
UCLASS()
class GAMEJAMM_API AJamTestGameMode : public AJamGameMode
{
	GENERATED_BODY()

public:
	AJamTestGameMode();
};
