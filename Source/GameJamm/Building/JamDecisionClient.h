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

	/** Clave directa de TypeSafe (env TYPESAFE_API_KEY o [Jam] DecisionsApiKey): llama a Jev en api.typesafe.ai/v1/systemone. */
	static FString GetTypeSafeKey();

	/** Clave de fal (env FAL_KEY o [Jam] FalApiKey): respaldo de decisión vía fal.run/openrouter/router (LLM que elige en JSON). */
	static FString GetFalKey();

	/** Respuesta de fal -> decisiones (el texto del modelo trae un JSON, a veces con ```). */
	static bool ParseFalResponse(const FString& ResponseJson, FJamBuildChoice& OutChoice, FString& OutNote);

	/** Catalogo de conexiones y colocacion generado desde las reglas del ensamblador. */
	static FString BuildConnectionGuide();

	/** Cuerpo JSON de la petición para un prompt (público para pruebas). */
	static FString BuildRequestBody(const FString& Prompt, const TCHAR* ModelOverride = nullptr);

	/** Respuesta de Jev -> decisiones. noul > 0.5 = sí. */
	static bool ParseResponse(const FString& ResponseJson, FJamBuildChoice& OutChoice, FString& OutNote);

	/**
	 * Planificador LLM (fal) para pedidos fuera de vocabulario ("una pizza voladora"): devuelve una BOLSA de piezas del catalogo
	 * (tipo, cantidad, modo, raiz). No coloca nada: la bolsa pasa por el mismo compositor y validador que la de Jev.
	 */
	static void RequestPlan(const FString& Prompt, TFunction<void(bool bOk, const FJamBuildChoice& Choice, const FString& Note)> Done);

	/** Llamada asíncrona. Done se invoca en el hilo del juego. */
	static void RequestChoice(const FString& Prompt, TFunction<void(bool bOk, const FJamBuildChoice& Choice, const FString& Note)> Done);
};
