#pragma once

#include "CoreMinimal.h"
#include "Building/BuildRecipe.h"
#include "Building/JamBuildPart.h"

/** Una pieza colocada en el marco del vehículo (X adelante, Y derecha, Z arriba), antes de spawnear. */
struct FJamPlacedPart
{
	FVector Center = FVector::ZeroVector;
	FVector Half = FVector(50.0);
	/** Cara del padre en la que va (dirección hacia afuera). */
	FVector OutwardDir = FVector::UpVector;
	FName Parent;
	FJamPartDef Def;
};

/**
 * Layout de una receta (sin mundo, testeable). Lo usa UJamAssembler y los tests de Building/Tests.
 * Contrato de sockets: docs/assets/CATALOGO-ELEMENTOS.md §0.1.
 */
namespace JamLayout
{
	/** Separación entre piezas que no son padre/hijo (cm), para que no nazcan encimadas. */
	constexpr double Gap = 1.0;

	/**
	 * "Left", "Left_Front", "Attach_Left", "left side", "Rear"... -> dirección (+ desplazamiento a un borde).
	 * false si no reconoce ninguna cara.
	 */
	GAMEJAMM_API bool ParseSocket(const FString& Socket, FVector& OutDir, FVector& OutShift, bool& bOutHasShift);

	/** Forma canónica: "Left" o "Left_Front". */
	GAMEJAMM_API FString SocketToString(const FVector& Dir, const FVector& Shift, bool bHasShift);

	/** true si las cajas se meten una en otra más que Tolerance (cm). Tocarse no cuenta. */
	GAMEJAMM_API bool Overlaps(const FJamPlacedPart& A, const FJamPlacedPart& B, double Tolerance = 0.5);

	/**
	 * Coloca cada pieza pegada a la cara de su padre. Si choca con otra pieza ya colocada (p. ej. dos piezas
	 * en la misma cara), la empuja hacia afuera hasta que quede libre. Socket desconocido = Top (con warning).
	 */
	GAMEJAMM_API bool Compute(const FBuildRecipe& Recipe, TMap<FName, FJamPlacedPart>& OutPlaced, FString& OutError);
}
