#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BuildRecipe.generated.h"

class UInventoryComponent;

/**
 * CONTRATO COMPARTIDO entre el asistente IA (genera JSON) y el sistema de construcción (lo arma).
 * JSON:
 * {
 *   "name": "Rueda Infernal",
 *   "parts": [
 *     {"id": "p0", "type": "Wheel"},
 *     {"id": "p1", "type": "Flamethrower", "attach_to": "p0", "socket": "Side"}
 *   ],
 *   "activate": ["p0", "p1"]
 * }
 * Cada "type" consume 1 item "Part.<type>" del inventario.
 */
USTRUCT(BlueprintType)
struct GAMEJAMM_API FBuildRecipePart
{
	GENERATED_BODY()

	/** Id local dentro de la receta ("p0", "p1"...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	FName Id;

	/** Tipo de pieza: Wheel, Plank, Flamethrower, Rocket, Rock... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	FName Type;

	/** Id de la pieza a la que se pega. None = pieza raíz. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	FName AttachTo;

	/** Punto de anclaje en la pieza padre (Top, Bottom, Side, Front, Back...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	FName Socket;
};

USTRUCT(BlueprintType)
struct GAMEJAMM_API FBuildRecipe
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	TArray<FBuildRecipePart> Parts;

	/** Ids de piezas que se encienden al activar (motores, lanzallamas...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	TArray<FName> Activate;
};

UCLASS()
class GAMEJAMM_API UBuildRecipeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Parsea el JSON (tolera texto/```json alrededor, como suelen responder los LLM). */
	UFUNCTION(BlueprintCallable, Category = "Jam|Building")
	static bool ParseRecipeJson(const FString& Json, FBuildRecipe& OutRecipe, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Jam|Building")
	static FString RecipeToJson(const FBuildRecipe& Recipe);

	/** Items de inventario que consume la receta: {"Part.Wheel": 3, ...}. */
	UFUNCTION(BlueprintPure, Category = "Jam|Building")
	static TMap<FName, int32> GetRequiredItems(const FBuildRecipe& Recipe);

	/** Estructura válida (ids únicos, una raíz, padres existentes) y, si se pasa inventario, que alcance. */
	UFUNCTION(BlueprintCallable, Category = "Jam|Building")
	static bool ValidateRecipe(const FBuildRecipe& Recipe, const UInventoryComponent* Inventory, FString& OutError);
};
