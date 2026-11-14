#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "JamPoc.generated.h"

/**
 * PoC visible de "pedido en lenguaje natural -> construcción", sin assets nuevos. Funciona en cualquier mapa en Play (PIE).
 * Comandos de consola (tecla ~):
 *   Jam.Build <pedido libre>   arma la construcción ~5 m delante de la cámara, la enciende a los 1.5 s y muestra el resumen
 *   Jam.Inv <Tipo> <n>         pone n piezas de ese tipo en el inventario de depuración
 *   Jam.InvAll [n]             inventario de depuración rico (n de cada pieza del catálogo; 10 por defecto)
 *   Jam.InvClear               vacía el inventario de depuración (sin él y sin inventario del jugador = ilimitado)
 *   Jam.InvShow                lista el inventario de depuración
 *   Jam.Clear                  destruye lo construido y devuelve las piezas
 *   Jam.Prompts                imprime pedidos de ejemplo
 *   Jam.Reload                 vuelve a leer BuildKnowledge.json (para afinar datos sin recompilar)
 * Las mismas acciones están aquí como funciones Blueprint/Python (p. ej. desde el MCP de Unreal).
 */
UCLASS()
class GAMEJAMM_API UJamPoc : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Igual que `Jam.Build <Prompt>`. Usa el modelo de decisión si hay API key; si no, palabras clave. */
	UFUNCTION(BlueprintCallable, Category = "Jam|PoC", meta = (WorldContext = "WorldContextObject"))
	static void Build(UObject* WorldContextObject, const FString& Prompt);

	/** Igual que `Jam.Clear`. */
	UFUNCTION(BlueprintCallable, Category = "Jam|PoC")
	static void ClearBuilds();

	/** Igual que `Jam.Inv <Tipo> <n>`. false si el tipo no existe en UJamPartCatalog. */
	UFUNCTION(BlueprintCallable, Category = "Jam|PoC")
	static bool SetDebugInventory(FName Type, int32 Count);

	/** Igual que `Jam.InvAll [n]`. */
	UFUNCTION(BlueprintCallable, Category = "Jam|PoC")
	static void FillDebugInventory(int32 Count = 10);

	/** Igual que `Jam.InvClear`. */
	UFUNCTION(BlueprintCallable, Category = "Jam|PoC")
	static void ClearDebugInventory();

	UFUNCTION(BlueprintPure, Category = "Jam|PoC")
	static TMap<FName, int32> GetDebugInventory();

	/** Resumen (varias líneas) de la última llamada a Build: decisión, piezas, sustituciones, faltantes, energía. */
	UFUNCTION(BlueprintPure, Category = "Jam|PoC")
	static FString GetLastSummary();

	UFUNCTION(BlueprintPure, Category = "Jam|PoC")
	static TArray<FString> GetExamplePrompts();
};
