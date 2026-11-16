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

	/** Token del proxy HTTPS configurado; solo sin proxy se usa OPENAI_API_KEY / OpenAIApiKey. Vacio = respaldo offline. */
	static FString GetOpenAIAuthToken();

	/** Endpoint para decisiones o voz, compartiendo la seleccion de credencial y proxy. */
	static FString GetOpenAIEndpoint(const TCHAR* Path);

	/** Corte en segundos de un proveedor ("typesafe", "openrouter", "openai", "fal"): BuildKnowledge.json, bloque decision_timeouts. */
	static float GetTimeout(const TCHAR* Provider, float Default);

	/**
	 * Cuerpo para la Decisions API de OpenAI: las MISMAS preguntas de BuildRequestBody traducidas a su esquema
	 * (noul -> predicate, choice -> choice con choices[]). Los textos hablan de piezas de juguete para que el modelo no se niegue.
	 */
	static FString BuildOpenAIRequestBody(const FString& Prompt);

	/**
	 * Respuesta de OpenAI (answers[]) -> decisiones, con la misma lectura que la de Jev. Una negativa ("refusal") deja esa pregunta
	 * sin respuesta: si era un use_<Tipo>, decide el respaldo por palabras para ese tipo. false si todo viene negado o sin piezas.
	 */
	static bool ParseOpenAIResponse(const FString& ResponseJson, const FString& Prompt, FJamBuildChoice& OutChoice, FString& OutNote);

	/** Construccion por la Decisions API de OpenAI. Done en el hilo del juego; la nota empieza con "openai-decisions". */
	static void RequestOpenAIChoice(const FString& Prompt, TFunction<void(bool bOk, const FJamBuildChoice& Choice, const FString& Note)> Done);

	/**
	 * Arma de mano pedida (GDD 5.1) por la Decisions API de OpenAI: el modelo solo ELIGE un id de bodies, uno de muzzles y los extras
	 * de Weapons.json (preguntas y lectura en Combat/JamGunSpec). bOk false = sin clave, error, corte o todo negado: decide la palabra.
	 */
	static void RequestGunChoice(const FString& Phrase, TFunction<void(bool bOk, const FString& BodyId, const FString& MuzzleId, const TArray<FString>& ExtraIds)> Done);

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

	/**
	 * Llamada asíncrona. Done se invoca en el hilo del juego. Orden: Jev directo (TypeSafe) > Jev por OpenRouter >
	 * OpenAI Decisions (tambien si Jev falla) > planificador fal. Sin ninguna clave, bOk = false (respaldo por palabras).
	 */
	static void RequestChoice(const FString& Prompt, TFunction<void(bool bOk, const FJamBuildChoice& Choice, const FString& Note)> Done);
};
