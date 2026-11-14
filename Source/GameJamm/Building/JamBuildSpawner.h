#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JamBuildSpawner.generated.h"

class AJamBuildPart;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	bool bBuildOnBeginPlay = true;

	/** Segundos antes de encender motores/ventiladores/cañones. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	float ActivateDelay = 1.5f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Build")
	FString LastRecipeJson;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Build")
	FString LastStatus;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Build")
	void BuildNow();

	UFUNCTION(BlueprintCallable, Category = "Build")
	void SetPromptAndBuild(const FString& NewPrompt);

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TArray<TObjectPtr<AJamBuildPart>> Parts;

	TArray<FName> ActivateIds;
	FTimerHandle ActivateTimer;
};
