#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Character.h"
#include "GameFramework/HUD.h"
#include "Core/JamGameMode.h"
#include "JamPlayConsole.generated.h"

class UCameraComponent;

/**
 * Banco de pruebas jugable de construccion (rol Building). Comandos de consola (tecla ~), en JamPlayConsole.cpp:
 *   Jam.Say <texto libre>  -> prompt -> modelo de decision (Jev) -> receta -> ensamblador, ~6 m delante del jugador
 *   Jam.Clear              -> borra todo lo construido
 *   Jam.Seat               -> se sube al asiento de la construccion mas cercana / se baja
 *   Jam.Energy [valor]     -> muestra o fija la barra de energia del jugador
 *   Jam.Test [seg]         -> bateria de frases; escribe docs/building/REPORTE-POC.md
 * En el juego: mantener G = hablar (voz -> texto -> construir); T, B o Enter = escribir la frase; 1..9 = ejemplos; C = encender/apagar; X = limpiar; E = subir/bajar (montado: WASD dirige);
 * V = primera/tercera persona; H = ayuda. Mando: sticks, A salta, X sube/baja. Ajustes: jam.Mouse.Sens, jam.Mouse.InvertY, jam.Shake.
 * No es el personaje final del juego: es lo minimo para caminar y probar sin assets ni input mappings.
 */
UCLASS()
class GAMEJAMM_API AJamTestCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AJamTestCharacter();
	virtual void Tick(float DeltaSeconds) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void Landed(const FHitResult& Hit) override;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<class USpringArmComponent> Arm;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	/** Cuerpo provisional (cilindro + "nariz") hasta que Arte entregue el personaje. */
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<class UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<class UStaticMeshComponent> Nose;

	/** Abre la caja de texto para escribir que construir. */
	void OpenSayBox();

	/** Sacudida de camara por trauma (0..1) para el jugador local, atenuada por distancia al epicentro. */
	static void AddTrauma(UWorld* World, const FVector& Epicenter, float Amount, float InnerRadius, float OuterRadius);

	UPROPERTY()
	TObjectPtr<class UJamSayWidget> SayWidget;

private:
	void DriveMount(class AJamBuildPart* Seat, float Throttle, float Steer, bool bFire, bool bBoost, float DeltaSeconds);

	bool bFirstPerson = false;
	float Trauma = 0.f;
	float FovExtra = 0.f;
	float LastGroundTime = -1.f;
	float JumpBufferedAt = -1.f;
	float SmoothThrottle = 0.f;
	float SmoothSteer = 0.f;
	TWeakObjectPtr<class AJamBuildPart> Mount;
};

/**
 * HUD del banco de pruebas: barra de energia (abajo, junto a la vida), ayuda (H) y construccion actual. La lista de elementos,
 * las mecanicas y la linea de consola solo salen con jam.Hud.Debug 1.
 */
UCLASS()
class GAMEJAMM_API AJamTestHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	bool bShowHelp = false;   // H la abre; la ayuda no tapa la pantalla por defecto (GDD 9)
	float K = 1.f;   // escala del HUD segun la resolucion

	/** Y (en pixeles) donde termina hacia arriba el bloque de abajo (vida, energia, voz): los avisos de JamMsg se apilan encima. */
	float MsgBaseY = 0.f;

private:
	float Panel(float X, float Y, float W, const FString& Title, const TArray<FString>& Lines, const FLinearColor& TitleColor);
};

/** Caja de texto en el juego (T o Enter): se escribe la frase y Enter construye; Esc cierra. */
UCLASS()
class GAMEJAMM_API UJamSayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY()
	TObjectPtr<class UMultiLineEditableTextBox> Box;

	/** Enter mete un salto de linea: ahi se construye. (Asi no hace falta el delegado con enum de SlateCore.) */
	UFUNCTION()
	void OnChanged(const FText& Text);

	void Close(const FString& Phrase);

	float OpenTime = 0.f;
};

/** GameMode del nivel de pruebas: el de la partida + AJamTestCharacter (WASD, mouse, espacio). */
UCLASS()
class GAMEJAMM_API AJamTestGameMode : public AJamGameMode
{
	GENERATED_BODY()

public:
	AJamTestGameMode();
};
