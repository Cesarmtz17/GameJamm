#pragma once

#include "CoreMinimal.h"
#include "Building/JamPlayConsole.h"
#include "JamHUD.generated.h"

/**
 * HUD de la partida (Core): dibuja encima del HUD de construccion (AJamTestHUD) el reloj, la vida de torres y base
 * por equipo, la vida del jugador, la cuenta de respawn, barras sobre las torres cercanas y la pantalla de victoria o derrota.
 */
UCLASS()
class GAMEJAMM_API AJamHUD : public AJamTestHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawCentered(const FString& Text, const FLinearColor& Color, float Y, float Scale, bool bBackground = true);

	float LastHealth = -1.f;
	float HurtFlash = 0.f;
};
