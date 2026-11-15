#pragma once

#include "CoreMinimal.h"

/**
 * Avisos al jugador (GDD 9). Reemplazan a GEngine->AddOnScreenDebugMessage, que la build Shipping no muestra.
 * Los dibuja AJamHUD con Canvas:
 *   - Post: linea del asistente, centro abajo; cola de 4 que se borran solas (lo que entendio, que armo, que falto, loot).
 *   - Big:  aviso grande arriba al centro, debajo del objetivo (torre caida, muerte subita, cofre raro).
 * Key opcional: un aviso con la misma Key reemplaza al anterior (p. ej. "Pensando..." -> "Armaste ...").
 * Cada aviso tambien va al log como "JamMsg: <texto>" para poder verificarlo sin ver la pantalla.
 * Textos para el jugador: en espanol, cortos, sin jerga de depuracion.
 */
namespace JamMsg
{
	struct FEntry
	{
		FString Text;
		FLinearColor Color = FLinearColor::White;
		double Born = 0.0;
		double Expire = 0.0;
		FName Key;
	};

	// Colores de uso comun (iguales a los del HUD).
	GAMEJAMM_API extern const FLinearColor Info;    // blanco hueso: informacion normal
	GAMEJAMM_API extern const FLinearColor Good;    // verde: algo salio bien (armaste, recogiste)
	GAMEJAMM_API extern const FLinearColor Warn;    // ambar: falto algo, sin energia, mochila llena
	GAMEJAMM_API extern const FLinearColor Bad;     // rojo: fallo, caiste
	GAMEJAMM_API extern const FLinearColor Loot;    // dorado: cofres y loot

	/** Linea del asistente / aviso corto (centro abajo). */
	GAMEJAMM_API void Post(const FString& Text, const FLinearColor& Color = FLinearColor(0.95f, 0.95f, 0.92f, 1.f), float Seconds = 5.f, FName Key = NAME_None);

	/** Aviso grande (arriba al centro, debajo del objetivo). */
	GAMEJAMM_API void Big(const FString& Text, const FLinearColor& Color = FLinearColor(1.f, 0.82f, 0.35f, 1.f), float Seconds = 4.f, FName Key = NAME_None);

	/** Quita el aviso con esa Key (de las dos colas). */
	GAMEJAMM_API void Remove(FName Key);

	/** Avisos vivos (quita los vencidos). Los usa el HUD. */
	GAMEJAMM_API const TArray<FEntry>& Lines();
	GAMEJAMM_API const TArray<FEntry>& Bigs();

	/** Vacia las dos colas (al empezar una partida nueva). */
	GAMEJAMM_API void Clear();
}
