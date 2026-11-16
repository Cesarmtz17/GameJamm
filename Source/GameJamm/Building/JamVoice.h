#pragma once

#include "CoreMinimal.h"

/**
 * Voz -> texto para pedir construcciones hablando (push-to-talk).
 * Graba el microfono mientras se mantiene la tecla (AudioCaptureCore), recorta el silencio y arma un WAV mono de 16 kHz.
 * Transcribe en cadena, siempre con tiempo limite y aviso claro:
 *   1) OpenAI Whisper (whisper-1, 20 s), con el token configurado de FJamDecisionClient::GetOpenAIAuthToken().
 *      Multipart WAV, vocabulario ingles y deteccion automatica de idioma (ingles/espanol).
 *   2) LOCAL si ya esta listo: whisper.cpp en 127.0.0.1:8177. Sin OpenAI, WarmUp puede arrancar lo ya instalado.
 *   3) fal (storage/upload -> fal-ai/wizper) con FAL_KEY o [Jam] FalApiKey como respaldo.
 *   4) Si nada responde: motivo seguro en ingles y B para escribir. Nunca registra claves ni cuerpos de error HTTP.
 * Solo una grabacion/transcripcion simultanea; todos los callbacks terminan en el hilo del juego.
 * Instalacion de la voz local: docs/building/README-DECISION.md.
 */
class GAMEJAMM_API FJamVoice
{
public:
	typedef TFunction<void(bool bOk, const FString& TextOrError)> FDone;

	static FJamVoice& Get();

	/** Empieza a grabar. Rechaza mientras transcribe; false + motivo seguro si no puede abrir el microfono. */
	bool Start(FString& OutError);

	/** Deja de grabar y transcribe lo grabado. Done se llama en el hilo del juego. */
	void StopAndTranscribe(FDone Done);

	/** Transcribe un WAV valido no vacio de hasta 25 MiB (pruebas: Jam.VoiceFile). Rechaza llamadas simultaneas. */
	static void TranscribeWav(const TArray<uint8>& Wav, FDone Done);

	/** Con clave OpenAI no hace probes locales; sin ella, calienta el servidor local ya instalado. */
	void WarmUp();

	bool IsRecording() const { return bRecording; }

	/** Para el HUD: segundos que tardo la ultima transcripcion y quien la hizo ("openai-whisper" / "local" / "fal"). */
	float LastSeconds = 0.f;
	FString LastBackend;

	/** Para el HUD: que esta haciendo ("escuchando", "transcribiendo") y el ultimo resultado. */
	FString Status;
	FString LastText;
	bool bLastOk = true;
	double LastTime = -100.0;

private:
	bool bRecording = false;
	bool bTranscribing = false;
	int32 SampleRate = 48000;
	TArray<float> Samples;      // mono
	FCriticalSection Lock;
	TSharedPtr<class FJamVoiceCapture> Capture;
};
