#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Building/BuildRecipe.h"
#include "Core/JamTypes.h"
#include "JamBuildSpawner.generated.h"

class AJamBuildPart;
class UInventoryComponent;

/**
 * POC "prompt -> estructura jugable". Coloca este actor en el nivel, escribe un Prompt (o un JSON
 * de receta, p. ej. la salida de Decisions/LLM) y al jugar construye y activa la estructura.
 */
UCLASS()
class GAMEJAMM_API AJamBuildSpawner : public AActor
{
	GENERATED_BODY()

public:
	AJamBuildSpawner();

	/** Texto/voz del jugador: "un coche con ventilador y cañón". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	FString Prompt = TEXT("un coche con ventilador");

	/** Si no está vacío, se usa este JSON de receta en lugar de generarla del Prompt (salida del LLM). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build", meta = (MultiLine = "true"))
	FString RecipeJsonOverride;

	/** Para pruebas/demos sin red: respuesta de Jev ya hecha (JSON). Si no está vacía, se usa en vez de llamar a la API. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build", meta = (MultiLine = "true"))
	FString DecisionResponseOverride;

	/** Usar el modelo de decisión (Jev) si hay API key; si no, respaldo por palabras clave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	bool bUseDecisionModel = true;

	/** Partida: exige y consume inventario del propietario. False para escenas de pruebas/bots provisionales. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	bool bUseInventory = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	bool bBuildOnBeginPlay = true;

	/** true = se enciende sola tras ActivateDelay (pruebas, consola). false = sale apagada y se enciende con SetBuildActive (tecla C). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	bool bAutoActivate = true;

	/** Prende o apaga todos los dispositivos de la construccion (GDD 6.3). */
	UFUNCTION(BlueprintCallable, Category = "Build")
	void SetBuildActive(bool bOn);

	UFUNCTION(BlueprintPure, Category = "Build")
	bool IsBuildActive() const;

	/** Segundos antes de encender motores/ventiladores/cañones. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	float ActivateDelay = 1.5f;

	/** Equipo del dueño: las cabezas de esta construccion no apuntan a los de su equipo. Neutral = apunta a todo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	EJamTeam Team = EJamTeam::Neutral;

	/** Muestra en pantalla (8 s) que decidio el modelo y que se construyo. Lo usa el comando Jam.Say. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	bool bAnnounceOnScreen = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Build")
	FString LastRecipeJson;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Build")
	FString LastStatus;

	/** Coherencia 0..100 de lo ultimo construido (-1 = sin puntuar). 85+ pasa, 70-84 con aviso, <70 no quedo bien. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Build")
	int32 LastCoherence = -1;

	/** Aviso corto para el HUD: que se cambio o que no quedo bien (vacio = nada que decir). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Build")
	FString LastCoherenceNote;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Build")
	void BuildNow();

	UFUNCTION(BlueprintCallable, Category = "Build")
	void SetPromptAndBuild(const FString& NewPrompt);

	const TArray<TObjectPtr<AJamBuildPart>>& GetParts() const { return Parts; }

	/** Destruye las piezas de la construccion actual. */
	UFUNCTION(BlueprintCallable, Category = "Build")
	void ClearBuild();

	/** Piezas pagadas que se devolverian ahora: por tipo, min(pagado, vivas). Las destruidas no se devuelven. Solo cuenta si hay inventario pagador. */
	TMap<FName, int32> GetRefundableItems() const;

	/** Cuantas piezas de esta construccion siguen existiendo. */
	int32 CountAliveParts() const;

	/** El jugador renacio: la construccion pasa a su peon nuevo (dueno, inventario pagador y dueno del estado compartido). */
	void RebindOwner(AActor* NewOwner);

protected:
	virtual void BeginPlay() override;

private:
	void Realize(const FBuildRecipe& Recipe, const FString& Note);
	void Announce(const FString& Msg, bool bGood) const;

	UPROPERTY()
	TArray<TObjectPtr<AJamBuildPart>> Parts;

	TArray<FName> ActivateIds;
	FTimerHandle ActivateTimer;
	TWeakObjectPtr<UInventoryComponent> PaidInventory;
	TMap<FName, int32> PaidItems;
	uint64 RequestGeneration = 0;
};
