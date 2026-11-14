#pragma once

#include "CoreMinimal.h"
#include "Building/JamAssembler.h"

/**
 * Cliente del modelo de decisión Jev (TypeSafe AI) vía OpenRouter: POST /api/alpha/decisions.
 * El modelo solo ELIGE (arquetipo y extras sí/no); el código arma y valida la receta.
 * Esquema verificado en la guía de OpenRouter (model "typesafe/jev-1.13", state + questions;
 * choice/noul -> answers.<q>.choice / .noul).
 * La API key: variable de entorno OPENROUTER_API_KEY o Config/DefaultSecrets.ini ([Jam] OpenRouterApiKey).
 */
class GAMEJAMM_API FJamDecisionClient
{
public:
	/** Vacío si no hay clave configurada (entonces se usa el respaldo por palabras clave). */
	static FString GetApiKey();

	/** Clave de fal (env FAL_KEY o [Jam] FalApiKey): respaldo de decisión vía fal.run/openrouter/router (LLM que elige en JSON). */
	static FString GetFalKey();

	/** Respuesta de fal -> decisiones (el texto del modelo trae un JSON, a veces con ```). */
	static bool ParseFalResponse(const FString& ResponseJson, FJamBuildChoice& OutChoice, FString& OutNote);

	/** Cuerpo JSON de la petición para un prompt (público para pruebas). */
	static FString BuildRequestBody(const FString& Prompt);

	/** Respuesta de Jev -> decisiones. noul > 0.5 = sí. */
	static bool ParseResponse(const FString& ResponseJson, FJamBuildChoice& OutChoice, FString& OutNote);

	/** Llamada asíncrona. Done se invoca en el hilo del juego. */
	static void RequestChoice(const FString& Prompt, TFunction<void(bool bOk, const FJamBuildChoice& Choice, const FString& Note)> Done);
};
