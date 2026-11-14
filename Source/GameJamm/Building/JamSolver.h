#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Building/BuildRecipe.h"
#include "Building/JamAssembler.h"
#include "JamSolver.generated.h"

class UInventoryComponent;

/** "Usé Fan en vez de Rocket": el slot o extra pedía Wanted y se colocó Used. */
USTRUCT(BlueprintType)
struct GAMEJAMM_API FJamSolverSubstitution
{
	GENERATED_BODY()

	/** Rol del slot (ground_drive, weapon...) o clave del extra (rocket, wall...). */
	UPROPERTY(BlueprintReadOnly, Category = "Build")
	FString Role;

	UPROPERTY(BlueprintReadOnly, Category = "Build")
	FName Wanted;

	UPROPERTY(BlueprintReadOnly, Category = "Build")
	FName Used;

	UPROPERTY(BlueprintReadOnly, Category = "Build")
	int32 Count = 0;
};

/** "Reduje ground_drive de 4 a 2": no alcanzó para llenar el slot completo. */
USTRUCT(BlueprintType)
struct GAMEJAMM_API FJamSolverReduction
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Build")
	FString Role;

	UPROPERTY(BlueprintReadOnly, Category = "Build")
	int32 Wanted = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Build")
	int32 Got = 0;
};

/** Material que falta ("pedir materiales"): tipo de pieza y cuántas. */
USTRUCT(BlueprintType)
struct GAMEJAMM_API FJamSolverMissing
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Build")
	FName Type;

	UPROPERTY(BlueprintReadOnly, Category = "Build")
	int32 Count = 0;
};

/** Entrada del solver: lo que eligió el modelo de decisión + el inventario del jugador. */
USTRUCT(BlueprintType)
struct GAMEJAMM_API FJamSolverRequest
{
	GENERATED_BODY()

	/** Arquetipo (de ArchetypeTable o id de plantilla del JSON: Spinner, Roller, Airship...), extras y piezas deseadas. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	FJamBuildChoice Choice;

	/** upright / flat / inverted. None = la de la plantilla. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	FName Orientation;

	/** Tipo de pieza -> cantidad ("Wheel" -> 4). Ver UJamSolver::InventoryFromComponent. El Seat no hace falta: es gratis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	TMap<FName, int32> Inventory;

	/** true = no mirar el inventario (POC sin inventario: sale la versión completa de la plantilla). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	bool bUnlimitedInventory = false;

	/** Energía actual de la barra del jugador. < 0 = usar energy.start del JSON. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	float Energy = -1.f;
};

/** Salida del solver: receta válida (si bBuildable) y todo lo que el asistente tiene que contarle al jugador. */
USTRUCT(BlueprintType)
struct GAMEJAMM_API FJamSolverResult
{
	GENERATED_BODY()

	/** false = BuildKnowledge.json no cargó: el llamador debe usar el camino anterior (ComposeRecipe por tabla). */
	UPROPERTY(BlueprintReadOnly, Category = "Build")
	bool bKnowledgeLoaded = false;

	/** true = Recipe es válida y cumple lo que el arquetipo exige (moverse / atacar). false = Recipe vacía; ver Missing. */
	UPROPERTY(BlueprintReadOnly, Category = "Build")
	bool bBuildable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Build")
	FBuildRecipe Recipe;

	/** Plantilla que se armó y la que se pidió. Distintas si bDegraded (Car sin ruedas -> Hoverbike). */
	UPROPERTY(BlueprintReadOnly, Category = "Build")
	FName Template;

	UPROPERTY(BlueprintReadOnly, Category = "Build")
	FName RequestedTemplate;

	UPROPERTY(BlueprintReadOnly, Category = "Build")
	bool bDegraded = false;

	UPROPERTY(BlueprintReadOnly, Category = "Build")
	TArray<FJamSolverSubstitution> Substitutions;

	UPROPERTY(BlueprintReadOnly, Category = "Build")
	TArray<FJamSolverReduction> Reductions;

	/** Materiales que faltan para la versión completa de lo pedido (en orden estable). */
	UPROPERTY(BlueprintReadOnly, Category = "Build")
	TArray<FJamSolverMissing> Missing;

	/** Piezas pedidas que todavía no existen en UJamPartCatalog (no se pueden lootear aún). */
	UPROPERTY(BlueprintReadOnly, Category = "Build")
	TArray<FName> Unsupported;

	/** Piezas pedidas que no cupieron en el límite de UJamAssembler::MaxParts. */
	UPROPERTY(BlueprintReadOnly, Category = "Build")
	TArray<FName> Trimmed;

	/** Piezas que el solver agregó por su cuenta para que la construcción se mueva. */
	UPROPERTY(BlueprintReadOnly, Category = "Build")
	TArray<FName> Added;

	/** upright / flat / inverted: el llamador rota el Origin al ensamblar (flat = 90° sobre X). */
	UPROPERTY(BlueprintReadOnly, Category = "Build")
	FName Orientation;

	UPROPERTY(BlueprintReadOnly, Category = "Build")
	bool bAnchored = false;

	/** Suma de energy_cost_per_s de lo que se enciende. */
	UPROPERTY(BlueprintReadOnly, Category = "Build")
	float EnergyCostPerSecond = 0.f;

	/** Barra + reserva de las baterías pegadas. */
	UPROPERTY(BlueprintReadOnly, Category = "Build")
	float EnergyAvailable = 0.f;

	/** Segundos que dura encendida. -1 = no gasta. */
	UPROPERTY(BlueprintReadOnly, Category = "Build")
	float DurationSeconds = -1.f;

	/** Dura menos que energy.warn_duration_s. */
	UPROPERTY(BlueprintReadOnly, Category = "Build")
	bool bLowEnergy = false;

	UPROPERTY(BlueprintReadOnly, Category = "Build")
	float MassKg = 0.f;

	/** Mensaje del asistente para el jugador (español). */
	UPROPERTY(BlueprintReadOnly, Category = "Build")
	FString Message;

	/** Línea corta para logs. */
	UPROPERTY(BlueprintReadOnly, Category = "Build")
	FString Note;
};

/**
 * Solver por capacidades (docs/mechanics-research/13 §3.3). El modelo de decisión solo ELIGE; este código decide lo
 * posible: llena los slots de la plantilla con el inventario, sustituye, reduce, degrada la forma, recorta al límite,
 * comprueba que se mueva/ataque y lista los materiales que faltan.
 * Datos: Content/Game/Building/Data/BuildKnowledge.json. Prototipo y casos espejo: solver_proto.py (misma carpeta).
 */
UCLASS()
class GAMEJAMM_API UJamSolver : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** <Proyecto>/Content/Game/Building/Data/BuildKnowledge.json */
	static FString GetKnowledgePath();

	/** Carga el JSON (una vez; se queda en memoria). false si falta o es inválido: usar el camino anterior. */
	UFUNCTION(BlueprintCallable, Category = "Jam|Building")
	static bool LoadKnowledge(bool bForceReload = false);

	/** Carga desde texto (pruebas). false y OutError si es inválido; en ese caso el solver queda sin conocimiento. */
	static bool LoadKnowledgeFromString(const FString& Json, FString& OutError);

	/** Pruebas: deja el solver sin conocimiento hasta LoadKnowledge(true), como si faltara el JSON. */
	static void UnloadKnowledge();

	/** true si el JSON cargó y tiene una plantilla con ese id o code_archetype. */
	UFUNCTION(BlueprintPure, Category = "Jam|Building")
	static bool HasTemplate(FName Archetype);

	/** Piezas gratis e infinitas (Seat): no se descuentan del inventario. */
	UFUNCTION(BlueprintPure, Category = "Jam|Building")
	static bool IsInfiniteType(FName Type);

	/** Decisiones + inventario -> receta. Devuelve OutResult.bBuildable. Determinista. */
	UFUNCTION(BlueprintCallable, Category = "Jam|Building")
	static bool Solve(const FJamSolverRequest& Request, FJamSolverResult& OutResult);

	/** {"Part.Wheel": 3, "Mat.Fire": 1} -> {"Wheel": 3}. Solo lee UInventoryComponent::Items. */
	UFUNCTION(BlueprintPure, Category = "Jam|Building")
	static TMap<FName, int32> InventoryFromComponent(const UInventoryComponent* Inventory);

	/**
	 * Ajusta las decisiones con el texto del jugador para lo que ArchetypeTable no cubre: plantillas solo del JSON
	 * (rueda acostada -> Spinner, rueda infernal -> Roller, globo -> Airship) y "que apunte sola" -> quiere Head.
	 * No hace nada si el JSON no cargó.
	 */
	UFUNCTION(BlueprintCallable, Category = "Jam|Building")
	static void RefineChoiceFromText(const FString& Prompt, UPARAM(ref) FJamBuildChoice& Choice);
};
