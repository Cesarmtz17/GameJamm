#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Building/BuildRecipe.h"
#include "JamAssembler.generated.h"

class AJamBuildPart;

/** Arma una FBuildRecipe en el mundo: piezas físicas + constraints. */
UCLASS()
class GAMEJAMM_API UJamAssembler : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Límite de piezas por construcción (valor de diseño). */
	static constexpr int32 MaxParts = 21;

	UFUNCTION(BlueprintCallable, Category = "Jam|Building", meta = (WorldContext = "WorldContextObject"))
	static bool Assemble(UObject* WorldContextObject, const FBuildRecipe& Recipe, const FTransform& Origin,
		TArray<AJamBuildPart*>& OutParts, FString& OutError);

	/** Enciende/apaga las piezas cuyo RecipeId esté en la lista. */
	UFUNCTION(BlueprintCallable, Category = "Jam|Building")
	static void SetBuildActive(const TArray<AJamBuildPart*>& Parts, const TArray<FName>& Ids, bool bActive);

	UFUNCTION(BlueprintCallable, Category = "Jam|Building")
	static void DestroyBuild(UPARAM(ref) TArray<AJamBuildPart*>& Parts);
};

/** Resultado de las "decisiones" sobre un prompt: un arquetipo + extras sí/no. */
USTRUCT(BlueprintType)
struct GAMEJAMM_API FJamBuildChoice
{
	GENERATED_BODY()

	/** Car, Flyer, Turret, Rocket */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	FName Archetype = "Car";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	bool bFan = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	bool bRocket = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	bool bFlame = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	bool bCannon = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	bool bBalloon = false;
};

/**
 * Prompt -> receta. POC: generador por palabras clave (offline) con arquetipos.
 * Punto de enganche para Decisions: elegir arquetipo y extras entre opciones finitas
 * (ver ChooseArchetype) y dejar que el código arme la receta válida.
 */
UCLASS()
class GAMEJAMM_API UJamBuildGenerator : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Palabras clave -> decisiones (offline, respaldo cuando no hay API key). */
	UFUNCTION(BlueprintCallable, Category = "Jam|Building")
	static void ChoiceFromKeywords(const FString& Prompt, FJamBuildChoice& OutChoice);

	/** Decisiones -> receta válida (el código arma las piezas; el modelo solo elige). */
	UFUNCTION(BlueprintCallable, Category = "Jam|Building")
	static bool ComposeRecipe(const FJamBuildChoice& Choice, FBuildRecipe& OutRecipe, FString& OutNote);

	UFUNCTION(BlueprintCallable, Category = "Jam|Building")
	static bool GenerateRecipe(const FString& Prompt, FBuildRecipe& OutRecipe, FString& OutNote);
};
