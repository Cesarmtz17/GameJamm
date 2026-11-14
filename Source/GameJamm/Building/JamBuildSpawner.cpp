#include "Building/JamBuildSpawner.h"

#include "Building/BuildRecipe.h"
#include "Building/JamAssembler.h"
#include "Building/JamBuildPart.h"
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
	TArray<AJamBuildPart*> Old;
	for (AJamBuildPart* P : Parts) { Old.Add(P); }
	UJamAssembler::DestroyBuild(Old);
	Parts.Reset();

	FBuildRecipe Recipe;
	FString Note, Err;
	bool bOk;
	if (!RecipeJsonOverride.IsEmpty())
	{
		bOk = UBuildRecipeLibrary::ParseRecipeJson(RecipeJsonOverride, Recipe, Err);
		Note = TEXT("source=json");
	}
	else
	{
		bOk = UJamBuildGenerator::GenerateRecipe(Prompt, Recipe, Note);
	}
	if (!bOk)
	{
		LastStatus = FString::Printf(TEXT("FAIL recipe: %s %s"), *Err, *Note);
		UE_LOG(LogJamBuild, Warning, TEXT("%s"), *LastStatus);
		return;
	}
	LastRecipeJson = UBuildRecipeLibrary::RecipeToJson(Recipe);

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
