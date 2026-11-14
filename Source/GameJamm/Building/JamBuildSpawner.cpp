#include "Building/JamBuildSpawner.h"

#include "Building/BuildRecipe.h"
#include "Building/JamAssembler.h"
#include "Building/JamBuildPart.h"
#include "Building/JamDecisionClient.h"
#include "Components/BillboardComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogJamBuild, Log, All);

AJamBuildSpawner::AJamBuildSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AJamBuildSpawner::BeginPlay()
{
	Super::BeginPlay();
	if (bBuildOnBeginPlay)
	{
		BuildNow();
	}
}

void AJamBuildSpawner::SetPromptAndBuild(const FString& NewPrompt)
{
	Prompt = NewPrompt;
	RecipeJsonOverride.Reset();
	BuildNow();
}

void AJamBuildSpawner::BuildNow()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	World->GetTimerManager().ClearTimer(ActivateTimer);

	FBuildRecipe Recipe;
	FString Note, Err;

	// 1) Receta JSON ya hecha (salida de un LLM).
	if (!RecipeJsonOverride.IsEmpty())
	{
		if (!UBuildRecipeLibrary::ParseRecipeJson(RecipeJsonOverride, Recipe, Err))
		{
			LastStatus = FString::Printf(TEXT("FAIL recipe: %s"), *Err);
			UE_LOG(LogJamBuild, Warning, TEXT("%s"), *LastStatus);
			return;
		}
		Realize(Recipe, TEXT("source=json"));
		return;
	}

	// 2) Modelo de decisión: respuesta simulada o llamada real a Jev.
	FJamBuildChoice Choice;
	if (!DecisionResponseOverride.IsEmpty())
	{
		if (FJamDecisionClient::ParseResponse(DecisionResponseOverride, Choice, Note) && UJamBuildGenerator::ComposeRecipe(Choice, Recipe, Note))
		{
			Realize(Recipe, Note);
			return;
		}
		LastStatus = FString::Printf(TEXT("FAIL decision override: %s"), *Note);
		UE_LOG(LogJamBuild, Warning, TEXT("%s"), *LastStatus);
		return;
	}
	if (bUseDecisionModel && !FJamDecisionClient::GetApiKey().IsEmpty())
	{
		LastStatus = TEXT("waiting for decision model...");
		const FString PromptCopy = Prompt;
		TWeakObjectPtr<AJamBuildSpawner> Weak(this);
		FJamDecisionClient::RequestChoice(PromptCopy, [Weak, PromptCopy](bool bOk, const FJamBuildChoice& C, const FString& N)
		{
			AJamBuildSpawner* Self = Weak.Get();
			if (!Self)
			{
				return;
			}
			FBuildRecipe R;
			FString Note2 = N;
			if (bOk && UJamBuildGenerator::ComposeRecipe(C, R, Note2))
			{
				Self->Realize(R, Note2);
				return;
			}
			// Respaldo: palabras clave.
			UE_LOG(LogJamBuild, Warning, TEXT("decision model failed (%s); using keyword fallback"), *N);
			FString Note3;
			UJamBuildGenerator::GenerateRecipe(PromptCopy, R, Note3);
			Self->Realize(R, TEXT("fallback-keywords ") + Note3);
		});
		return;
	}

	// 3) Respaldo offline: palabras clave.
	if (!UJamBuildGenerator::GenerateRecipe(Prompt, Recipe, Note))
	{
		LastStatus = FString::Printf(TEXT("FAIL recipe: %s"), *Note);
		UE_LOG(LogJamBuild, Warning, TEXT("%s"), *LastStatus);
		return;
	}
	Realize(Recipe, TEXT("keywords ") + Note);
}

void AJamBuildSpawner::Realize(const FBuildRecipe& Recipe, const FString& Note)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	TArray<AJamBuildPart*> Old;
	for (AJamBuildPart* P : Parts) { Old.Add(P); }
	UJamAssembler::DestroyBuild(Old);
	Parts.Reset();

	LastRecipeJson = UBuildRecipeLibrary::RecipeToJson(Recipe);

	FString Err;
	TArray<AJamBuildPart*> Built;
	if (!UJamAssembler::Assemble(this, Recipe, GetActorTransform(), Built, Err))
	{
		LastStatus = FString::Printf(TEXT("FAIL assemble: %s"), *Err);
		UE_LOG(LogJamBuild, Warning, TEXT("%s"), *LastStatus);
		return;
	}
	for (AJamBuildPart* P : Built) { Parts.Add(P); }
	ActivateIds = Recipe.Activate;
	LastStatus = FString::Printf(TEXT("OK %d parts (%s) '%s'"), Built.Num(), *Note, *Recipe.Name);
	UE_LOG(LogJamBuild, Log, TEXT("%s %s"), *LastStatus, *LastRecipeJson);

	World->GetTimerManager().SetTimer(ActivateTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		TArray<AJamBuildPart*> Raw;
		for (AJamBuildPart* P : Parts) { Raw.Add(P); }
		UJamAssembler::SetBuildActive(Raw, ActivateIds, true);
	}), FMath::Max(ActivateDelay, 0.01f), false);
}
