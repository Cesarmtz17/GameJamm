#pragma once

#include "CoreMinimal.h"

/**
 * Voz -> texto para pedir construcciones hablando (push-to-talk).
 * Graba el microfono mientras se mantiene la tecla (AudioCaptureCore), recorta el silencio y arma un WAV mono de 16 kHz.
 * Transcribe en cadena, siempre con tiempo limite y aviso claro:
 *   1) LOCAL: whisper.cpp (`whisper-server` en 127.0.0.1:8177, modelo ggml-base, español forzado, con el vocabulario
 *      del juego como pista). Sin red; ~0.1-0.4 s por frase. El juego lo arranca solo si esta instalado.
 *   2) NUBE: fal (storage/upload -> fal-ai/wizper) con FAL_KEY o [Jam] FalApiKey. Puede tardar varios segundos.
 *   3) Si nada responde: motivo en pantalla; la caja de texto (T) sigue funcionando.
 * Instalacion de la voz local: docs/building/README-DECISION.md.
 */
class GAMEJAMM_API FJamVoice
{
public:
	typedef TFunction<void(bool bOk, const FString& TextOrError)> FDone;

	static FJamVoice& Get();

	/** Empieza a grabar. false + motivo si no se pudo abrir el microfono. */
	bool Start(FString& OutError);

	/** Deja de grabar y transcribe lo grabado. Done se llama en el hilo del juego. */
	void StopAndTranscribe(FDone Done);

	/** Transcribe un WAV ya hecho (pruebas: Jam.VoiceFile con un archivo de `say`). */
	static void TranscribeWav(const TArray<uint8>& Wav, FDone Done);

	/** Arranca (si hace falta) y calienta el servidor local de voz. Llamar al empezar a jugar. */
	void WarmUp();

	bool IsRecording() const { return bRecording; }

	/** Para el HUD: segundos que tardo la ultima transcripcion y quien la hizo ("local" / "nube"). */
	float LastSeconds = 0.f;
	FString LastBackend;

	/** Para el HUD: que esta haciendo ("escuchando", "transcribiendo") y el ultimo resultado. */
	FString Status;
	FString LastText;
	bool bLastOk = true;
	double LastTime = -100.0;

private:
	bool bRecording = false;
	int32 SampleRate = 48000;
	TArray<float> Samples;      // mono
	FCriticalSection Lock;
	TSharedPtr<class FJamVoiceCapture> Capture;
};
