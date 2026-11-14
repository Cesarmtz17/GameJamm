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

/** Arquetipo base (esqueleto de la construccion). */
struct FJamArchetypeDef
{
	FName Name;
	FString Description; // texto para el modelo de decision
};

/** Extra elegible sí/no por el modelo; la tabla es la unica fuente para palabras clave, Jev y fal. */
struct FJamExtraDef
{
	FName Key;
	FString TrueText;
	FString FalseText;
	FName PartType;                    // NAME_None = modificador (no agrega pieza)
	TArray<FName> Sockets;             // una pieza por socket (cara del cuerpo, token secundario opcional: "Top_Front")
	TArray<FName> FlyerSockets;        // si no esta vacio, reemplaza a Sockets en el arquetipo Flyer
	TArray<FName> Allowed;             // arquetipos permitidos (vacio = todos)
	TArray<FString> Keywords;          // respaldo offline
};

/** Resultado de las "decisiones" sobre un prompt: un arquetipo + extras elegidos. */
USTRUCT(BlueprintType)
struct GAMEJAMM_API FJamBuildChoice
{
	GENERATED_BODY()

	/** Car, Flyer, Turret, Rocket, Tank, Fortress, Hover */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	FName Archetype = "Car";

	/** Claves de FJamExtraDef elegidas (fan, jet, wing, wall, ...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	TArray<FName> Extras;

	/**
	 * Opcional (vacío por defecto): tipos de pieza que el jugador pide y que no son un extra de la tabla
	 * ("Head", "Battery"...; repetir = varias). Solo lo usa UJamSolver; ComposeRecipe por tabla lo ignora.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	TArray<FName> WantTypes;
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
	static const TArray<FJamArchetypeDef>& ArchetypeTable();
	static const TArray<FJamExtraDef>& ExtraTable();

	/** Palabras clave -> decisiones (offline, respaldo cuando no hay API key). */
	UFUNCTION(BlueprintCallable, Category = "Jam|Building")
	static void ChoiceFromKeywords(const FString& Prompt, FJamBuildChoice& OutChoice);

	/** Decisiones -> receta válida (el código arma las piezas; el modelo solo elige). */
	UFUNCTION(BlueprintCallable, Category = "Jam|Building")
	static bool ComposeRecipe(const FJamBuildChoice& Choice, FBuildRecipe& OutRecipe, FString& OutNote);

	UFUNCTION(BlueprintCallable, Category = "Jam|Building")
	static bool GenerateRecipe(const FString& Prompt, FBuildRecipe& OutRecipe, FString& OutNote);
};
