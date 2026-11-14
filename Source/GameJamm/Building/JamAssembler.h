#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Building/BuildRecipe.h"
#include "JamAssembler.generated.h"

class AJamBuildPart;

/**
 * Puntaje de coherencia 0..100 de una receta (sin mundo): que lo armado TENGA SENTIDO.
 * 85+ pasa, 70-84 se construye con aviso, <70 se repara recomponiendo (nunca bloquea).
 */
struct FJamCoherence
{
	int32 Score = 0;
	float Sub[8] = {};               // conexion, solapes, simetria, apoyo, empuje, tiro, masa, potencia (0..1)
	TArray<FString> Grave, Minor;    // textos para HUD y reporte
	TArray<FName> Offenders;         // piezas culpables
	bool bNoLift = false;            // volador que no despega (< 1.2 x peso): la reparacion agrega sustentacion
	/** "coh=97" o "coh=69 GRAVE tapado: gun; ..." */
	FString ToText() const;
};

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

	/** Caja del layout en el marco de la construccion (X adelante, Z arriba), sin mundo. Para apoyarla bien sobre el suelo al aparecer. */
	static FBox LayoutBounds(const FBuildRecipe& Recipe);

	/** Ocho chequeos sin mundo ni render: conexion, solapes, simetria, apoyo, empuje, tiro y aire, centro de masa, potencia. */
	static FJamCoherence Coherence(const FBuildRecipe& Recipe);

	/** jam.Build.Coherence: 0 apagado, 1 puntua y avisa, 2 puntua y repara (por defecto). */
	static int32 CoherenceMode();
	/** jam.Build.MinCoherence (70): por debajo se repara. */
	static int32 MinCoherence();
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
	FName Mode;   // None, "lift" (helice/cohete hacia abajo: sustenta), "launch" (resorte hacia arriba: lanza), "inline" (ruedas en linea) o "root" (esta pieza es el cuerpo)
};

/**
 * Regla de colocacion de un tipo de pieza: la funcion sale de la capacidad de la pieza (EJamPartKind);
 * aqui solo esta DONDE va y como preguntarle por ella al modelo.
 */
struct FJamPlaceMode
{
	TArray<FString> Faces;
	FString Sym, Order;
};

struct FJamPieceRule
{
	FName Type;
	TArray<FString> Slots;      // (compositor viejo por extras) caras candidatas
	TArray<FString> LiftSlots;  // no vacio = la pieza tiene modo "lift"/"launch" (se le pregunta al modelo)
	// --- colocacion con sentido (BuildKnowledge.json: bloque "place" de cada elemento) ---
	int32 PlaceOrder = 999;     // orden de colocacion: apoyo, sustentacion, asiento, monturas, empuje, armas, alas, escudo, equilibrio, anclas
	TArray<FString> Faces;      // caras candidatas en orden; solo lado izquierdo/delantero: el espejo las completa
	TArray<FString> Odd;        // caras de linea central para la pieza impar
	FString Sym;                // "xy", "y" o vacio: la cara solo vale junto con sus espejos
	FString Order;              // tramos del chasis: outer | center | front | back | balance
	float ClearCm = 0.f;        // distancia libre hacia donde mira (linea de tiro, chorro, tobera)
	float UpCm = 0.f;           // columna libre hacia arriba (asiento, globo)
	FString Ground;             // touch (rueda) | below (puede ir bajo las ruedas) | lift (puede ir abajo si mira al suelo)
	TArray<FName> Avoid;        // tipos que no pueden quedar a menos de 30 cm
	TMap<FName, FJamPlaceMode> Modes; // lift | launch | inline
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

	/** Respuestas globales del modelo: pieza raiz (Plank, Block, Cylinder, Sled, Wall; None = que decida el codigo) y forma general. */
	FName Root, Pattern;
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
	static void BagFromKeywords(const FString& Prompt, TArray<FJamPieceCount>& OutBag, bool bDefaultCar = true);

	/** Negaciones ("sin ruedas", "que no tenga cañon", "no quiero armas"): devuelve el texto sin esos tramos y las piezas negadas. */
	static FString StripNegations(const FString& Prompt, TArray<FName>& OutNegated);

	/** Reglas sobre la bolsa ya decidida: cantidad implicita (triciclo = 3), quitar lo negado, alas sin empuje +1 cohete, torre de un bloque -> 3. */
	static void RefineBag(const FString& CleanPrompt, const TArray<FName>& Negated, TArray<FJamPieceCount>& InOutBag);

	/**
	 * Numerales del texto ("25 llantas", "cuatro cañones") mandan sobre la cantidad que haya en la bolsa.
	 * bResetCounts: si el texto trae algun numero, las cantidades que dio el modelo se descartan primero (Jev
	 * "contagia" el numero dicho a otras piezas) y solo cuentan los numerales leidos por codigo.
	 */
	static void ApplyNumerals(const FString& Prompt, TArray<FJamPieceCount>& InOutBag, bool bResetCounts = false);

	/**
	 * Bolsa de piezas -> receta CON SENTIDO: el chasis se decide completo (1, 3, 5... tramos), cada pieza va en la primera cara
	 * que pasa Fits (sin solape, tiro/chorro/columna libres, nada bajo las ruedas) y en grupos espejados. Si un volador no
	 * despega se agrega sustentacion (hasta 4). La nota trae pieces=[], skipped=[], aviso=[] y coh=NN.
	 */
	static bool ComposeFromBag(const TArray<FJamPieceCount>& Bag, FBuildRecipe& OutRecipe, FString& OutNote);

	/** Receta ajena (JSON fijo, plantilla, planificador) -> bolsa (tipo, cantidad, modo) para recomponerla con sentido. */
	static void BagOfRecipe(const FBuildRecipe& Recipe, TArray<FJamPieceCount>& OutBag);

	/**
	 * Validador de Realize: si la receta puntua menos de MinCoherence, recompone su bolsa y se queda con la mejor.
	 * Devuelve la coherencia final; OutWhat dice que cambio (vacio = sin cambios).
	 */
	static FJamCoherence Repair(FBuildRecipe& InOutRecipe, FString& OutWhat);

	UFUNCTION(BlueprintCallable, Category = "Jam|Building")
	static bool GenerateRecipe(const FString& Prompt, FBuildRecipe& OutRecipe, FString& OutNote);
};
