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
	static constexpr int32 MaxParts = 60;

	/** Limite vigente: jam.Build.MaxParts (por defecto MaxParts, hasta 100). */
	static int32 GetMaxParts();

	/** Reserva propia de una construccion cuando jam.Build.Energy = 1; cada Battery suma su capacidad en cualquier modo. */
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

/** Una entrada de la bolsa de piezas que decide el modelo: que pieza, cuantas y en que modo. */
struct FJamPieceCount
{
	FName Type;
	int32 Count = 1;
	FName Mode;   // None, "lift" (helice/cohete hacia abajo: sustenta) o "launch" (resorte hacia arriba: lanza)
};

/**
 * Regla de colocacion de un tipo de pieza: la funcion sale de la capacidad de la pieza (EJamPartKind);
 * aqui solo esta DONDE va y como preguntarle por ella al modelo.
 */
struct FJamPieceRule
{
	FName Type;
	TArray<FString> Slots;      // caras candidatas; la pieza i usa Slots[i % Num] (dos listas = pares espejados)
	TArray<FString> LiftSlots;  // caras en modo "lift"/"launch"
	FName Chain;                // si no es None: las repetidas se encadenan por esa cara (fila, pila)
	int32 DefaultCount = 1;     // cuantas van si se pide sin numero
	bool bCountable = false;    // se le pregunta al modelo cuantas
	bool bWeapon = false;
	FString Ask, NotAsk;        // criterios si/no para el modelo de decision
	TArray<FString> Words;      // palabras (raices) para numerales y respaldo sin red
	FName ChildType, ChildSocket;
	FString MountSockets;
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

	/** Ruta principal: bolsa de piezas (tipo, cantidad, modo). Si no esta vacia manda sobre Archetype/Extras. */
	TArray<FJamPieceCount> Pieces;
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

	/** Reglas por pieza (donde va, como preguntar por ella, palabras). Orden = orden de colocacion. */
	static const TArray<FJamPieceRule>& PieceTable();

	/** Sin red: conceptos (bolsas con nombre: carro, catapulta...) + palabras de piezas + numerales ("25 llantas"). */
	static void BagFromKeywords(const FString& Prompt, TArray<FJamPieceCount>& OutBag);

	/**
	 * Numerales del texto ("25 llantas", "cuatro cañones") mandan sobre la cantidad que haya en la bolsa.
	 * bResetCounts: si el texto trae algun numero, las cantidades que dio el modelo se descartan primero (Jev
	 * "contagia" el numero dicho a otras piezas) y solo cuentan los numerales leidos por codigo.
	 */
	static void ApplyNumerals(const FString& Prompt, TArray<FJamPieceCount>& InOutBag, bool bResetCounts = false);

	/** Bolsa de piezas -> receta: elige chasis, lo alarga si hace falta y coloca cada pieza en una cara libre. */
	static bool ComposeFromBag(const TArray<FJamPieceCount>& Bag, FBuildRecipe& OutRecipe, FString& OutNote);

	UFUNCTION(BlueprintCallable, Category = "Jam|Building")
	static bool GenerateRecipe(const FString& Prompt, FBuildRecipe& OutRecipe, FString& OutNote);
};
