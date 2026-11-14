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

	/** Energia inicial de una construccion (BuildKnowledge.json: energy.max); cada Battery suma su capacidad. */
	static constexpr float BaseEnergy = 100.f;

	UFUNCTION(BlueprintCallable, Category = "Jam|Building", meta = (WorldContext = "WorldContextObject"))
	static bool Assemble(UObject* WorldContextObject, const FBuildRecipe& Recipe, const FTransform& Origin,
		TArray<AJamBuildPart*>& OutParts, FString& OutError);

	/** Enciende/apaga las piezas cuyo RecipeId esté en la lista. */
	UFUNCTION(BlueprintCallable, Category = "Jam|Building")
	static void SetBuildActive(const TArray<AJamBuildPart*>& Parts, const TArray<FName>& Ids, bool bActive);

	UFUNCTION(BlueprintCallable, Category = "Jam|Building")
	static void DestroyBuild(UPARAM(ref) TArray<AJamBuildPart*>& Parts);

	/**
	 * Layout sin mundo: true si la pieza PartId (o cualquiera, si es None) se encima con otra.
	 * Lo usan el generador (para elegir una cara libre) y los tests.
	 */
	static bool HasOverlap(const FBuildRecipe& Recipe, FName PartId, FString& OutInfo);
};

/**
 * Arquetipo = PLANTILLA de piezas basicas (una receta), sin logica propia: el comportamiento sale
 * del Kind de cada pieza y de la cara donde va pegada.
 */
struct FJamArchetypeDef
{
	FName Name;
	FString Description;               // texto para el modelo de decision
	TArray<FString> Keywords;          // respaldo offline (el primero de la tabla que coincide gana)
	TArray<FBuildRecipePart> Parts;    // receta base; la raiz se llama "body"; "mount" = pieza que gira/apunta
};

/** Extra elegible sí/no por el modelo; la tabla es la unica fuente para palabras clave, Jev y fal. */
struct FJamExtraDef
{
	FName Key;
	FString TrueText;
	FString FalseText;
	FName PartType;                    // NAME_None = modificador (no agrega pieza)
	/**
	 * Una pieza por entrada. Cada entrada lista caras candidatas separadas por '|' (se usa la primera libre).
	 * Cara = "Cara[_Borde|Center[_HaciaDondeMira]]", con padre opcional "id:" (por defecto "body").
	 */
	TArray<FString> Sockets;
	TArray<FString> FlyerSockets;      // si no esta vacio, reemplaza a Sockets en el arquetipo Flyer
	TArray<FName> Allowed;             // arquetipos permitidos (vacio = todos)
	TArray<FString> Keywords;          // respaldo offline
	bool bWeapon = false;              // si la construccion tiene "mount" (cabeza/plato), se monta ahi primero
	FString MountSockets;              // no vacio = esta pieza ES un "mount"; caras (de la pieza) para lo que se le monte
	FName ChildType;                   // pieza extra pegada encima de esta (p. ej. la hoja de la bisagra)
	FName ChildSocket;
};

/** Resultado de las "decisiones" sobre un prompt: un arquetipo + extras elegidos. */
USTRUCT(BlueprintType)
struct GAMEJAMM_API FJamBuildChoice
{
	GENERATED_BODY()

	/** Nombre de UJamBuildGenerator::ArchetypeTable (Car, Tank, Flyer, Hover, Turret, ...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	FName Archetype = "Car";

	/** Claves de FJamExtraDef elegidas (fan, jet, wing, wall, ...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	TArray<FName> Extras;
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
