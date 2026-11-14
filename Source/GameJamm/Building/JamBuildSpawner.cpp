#include "Building/JamBuildSpawner.h"

#include "Building/BuildRecipe.h"
#include "Building/JamAssembler.h"
#include "Building/JamBuildPart.h"
#include "Building/JamDecisionClient.h"
#include "Components/BillboardComponent.h"
#include "Engine/Engine.h"
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

void AJamBuildSpawner::Announce(const FString& Msg, bool bGood) const
{
	if (bAnnounceOnScreen && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 8.f, bGood ? FColor::Green : FColor::Orange, Msg);
	}
}

void AJamBuildSpawner::ClearBuild()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ActivateTimer);
	}
	TArray<AJamBuildPart*> Old;
	for (AJamBuildPart* P : Parts) { Old.Add(P); }
	UJamAssembler::DestroyBuild(Old);
	Parts.Reset();
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
	if (bUseDecisionModel && (!FJamDecisionClient::GetTypeSafeKey().IsEmpty() || !FJamDecisionClient::GetApiKey().IsEmpty() || !FJamDecisionClient::GetFalKey().IsEmpty()))
	{
		LastStatus = TEXT("waiting for decision model...");
		Announce(FString::Printf(TEXT("Jam: \"%s\" -> preguntando al modelo de decision (Jev)..."), *Prompt), true);
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
			Self->Announce(FString::Printf(TEXT("Jam: el modelo de decision fallo (%s). Uso el respaldo por palabras clave."), *N.Left(120)), false);
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
	Announce(TEXT("Jam: sin clave del modelo de decision (o desactivado). Uso el respaldo por palabras clave."), false);
	Realize(Recipe, TEXT("keywords ") + Note);
}

void AJamBuildSpawner::Realize(const FBuildRecipe& Recipe, const FString& Note)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	ClearBuild();

	LastRecipeJson = UBuildRecipeLibrary::RecipeToJson(Recipe);

	FString Err;
	TArray<AJamBuildPart*> Built;
	if (!UJamAssembler::Assemble(this, Recipe, GetActorTransform(), Built, Err))
	{
		LastStatus = FString::Printf(TEXT("FAIL assemble: %s"), *Err);
		UE_LOG(LogJamBuild, Warning, TEXT("%s"), *LastStatus);
		Announce(TEXT("Jam: ") + LastStatus, false);
		return;
	}
	for (AJamBuildPart* P : Built) { Parts.Add(P); }
	ActivateIds = Recipe.Activate;
	LastStatus = FString::Printf(TEXT("OK %d parts (%s) '%s'"), Built.Num(), *Note, *Recipe.Name);
	UE_LOG(LogJamBuild, Log, TEXT("%s %s"), *LastStatus, *LastRecipeJson);
	if (bAnnounceOnScreen)
	{
		// Mecanicas = capacidades (Kind) de las piezas que se van a encender.
		TArray<FString> Mechs;
		for (const AJamBuildPart* P : Built)
		{
			if (ActivateIds.Contains(P->RecipeId))
			{
				Mechs.AddUnique(StaticEnum<EJamPartKind>()->GetNameStringByValue((int64)P->Kind));
			}
		}
		Announce(FString::Printf(TEXT("Jam: \"%s\"\n  decision: %s\n  %d piezas | mecanicas activas: %s"), *Prompt, *Note, Built.Num(), *FString::Join(Mechs, TEXT(", "))), true);
	}

	World->GetTimerManager().SetTimer(ActivateTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		TArray<AJamBuildPart*> Raw;
		for (AJamBuildPart* P : Parts) { Raw.Add(P); }
		UJamAssembler::SetBuildActive(Raw, ActivateIds, true);
	}), FMath::Max(ActivateDelay, 0.01f), false);
}
