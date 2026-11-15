#pragma once

#include "CoreMinimal.h"

class USoundBase;
class UAudioComponent;

/**
 * Sonidos y musica (GDD 10). Los WAV de SourceArt/Audio se importan a /Game/Game/Building/Audio (carpeta que siempre se
 * cocina; la misma que usa AJamBuildPart). Si un sonido no esta importado no suena y no falla.
 * Log para verificar sin oir: "JamSfx: suena <Nombre>" la primera vez que suena cada uno y "JamSfx: falta <Nombre>" si
 * no se encontro. jam.Build.Sfx 0 los apaga todos (la misma variable de las piezas).
 */
namespace JamAudio
{
	/** El sonido por nombre (SFX_Hit, MUS_Match_Loop...) o nullptr. */
	GAMEJAMM_API USoundBase* Find(const TCHAR* Name);

	/** Sonido sin posicion (interfaz, avisos del jugador). */
	GAMEJAMM_API void Play2D(const UObject* WorldContext, const TCHAR* Name, float Volume = 1.f);

	/** Sonido en un punto del mundo (golpes, disparos, cofres). */
	GAMEJAMM_API void PlayAt(const UObject* WorldContext, const TCHAR* Name, const FVector& Where, float Volume = 1.f);

	/** Cambia la musica de fondo (una sola pista a la vez, con fundido). Name vacio = sin musica. */
	GAMEJAMM_API void Music(const UObject* WorldContext, const TCHAR* Name, float Volume = 0.5f);
}
