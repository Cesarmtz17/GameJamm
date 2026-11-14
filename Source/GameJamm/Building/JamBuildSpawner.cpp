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

void AJamBuildSpawner::SetBuildActive(bool bOn)
{
	TArray<AJamBuildPart*> Raw;
	for (AJamBuildPart* P : Parts) { Raw.Add(P); }
	UJamAssembler::SetBuildActive(Raw, ActivateIds, bOn);
}

bool AJamBuildSpawner::IsBuildActive() const
{
	for (const AJamBuildPart* P : Parts)
	{
		if (IsValid(P) && P->bActive) { return true; }
	}
	return false;
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
		// Composicion comun a Jev y al planificador: bolsa -> numerales del texto -> raiz señalada -> compositor con sentido -> Realize.
		auto Finish = [Weak, PromptCopy](bool bOk, const FJamBuildChoice& C, const FString& N)
		{
			AJamBuildSpawner* Self = Weak.Get();
			if (!Self)
			{
				return;
			}
			FBuildRecipe R;
			FString Note2 = N;
			bool bComposed = false;
			if (bOk && C.Pieces.Num() > 0)
			{
				// Ruta principal: el modelo decidio piezas y cantidades. Los numerales del texto ("25 llantas") mandan sobre
				// la cantidad, y una decision muy pobre (menos de 3 tipos de pieza) se completa con el concepto del respaldo.
				TArray<FName> Negated;
				const FString Clean = UJamBuildGenerator::StripNegations(PromptCopy, Negated);
				TArray<FJamPieceCount> Bag = C.Pieces;
				if (Bag.Num() < 3)
				{
					// Se completa SOLO con lo que el respaldo reconocio (nunca con el "carro basico": "una silla" no lleva llantas),
					// y sin duplicar una capacidad que el modelo ya eligio (Wheel vs BigWheel, Fan vs Turbine).
					auto KindOf = [](FName T) { FJamPartDef D; UJamPartCatalog::Find(T, D); return D.Kind; };
					TArray<FJamPieceCount> Seed;
					UJamBuildGenerator::BagFromKeywords(Clean, Seed, false);
					for (const FJamPieceCount& S : Seed)
					{
						const EJamPartKind Kd = KindOf(S.Type);
						const bool bSameKind = (Kd == EJamPartKind::Wheel || Kd == EJamPartKind::Fan) && Bag.ContainsByPredicate([&](const FJamPieceCount& X) { return KindOf(X.Type) == Kd; });
						if (!bSameKind && !Bag.ContainsByPredicate([&S](const FJamPieceCount& X) { return X.Type == S.Type; })) { Bag.Add(S); }
					}
				}
				UJamBuildGenerator::ApplyNumerals(Clean, Bag, true);
				UJamBuildGenerator::RefineBag(Clean, Negated, Bag);
				if (!C.Root.IsNone())
				{
					// Pieza raiz señalada por el modelo (pregunta "root" / planificador): si no venia en la bolsa, se agrega una.
					FJamPieceCount* RootItem = Bag.FindByPredicate([&C](const FJamPieceCount& X) { return X.Type == C.Root; });
					if (!RootItem) { RootItem = &Bag.AddDefaulted_GetRef(); RootItem->Type = C.Root; RootItem->Count = 1; }
					RootItem->Mode = "root";
				}
				bComposed = UJamBuildGenerator::ComposeFromBag(Bag, R, Note2);
			}
			else if (bOk)
			{
				bComposed = UJamBuildGenerator::ComposeRecipe(C, R, Note2); // formato viejo (plantilla + extras): respuestas simuladas
			}
			if (bComposed)
			{
				// N = "jev ... conf=..." (o "fal-plan ..."): se conserva el backend delante de la nota de la receta.
				FString Backend = N, Rest;
				N.Split(TEXT(" "), &Backend, &Rest);
				Self->Realize(R, Backend + TEXT(" ") + Note2);
				return;
			}
			// Respaldo: palabras clave.
			UE_LOG(LogJamBuild, Warning, TEXT("decision model failed (%s); using keyword fallback"), *N);
			Self->Announce(FString::Printf(TEXT("Jam: el modelo de decision fallo (%s). Uso el respaldo por palabras clave."), *N.Left(120)), false);
			FString Note3;
			UJamBuildGenerator::GenerateRecipe(PromptCopy, R, Note3);
			Self->Realize(R, TEXT("fallback-keywords ") + Note3);
		};
		FJamDecisionClient::RequestChoice(PromptCopy, [Finish, PromptCopy](bool bOk, const FJamBuildChoice& C, const FString& N)
		{
			// Fuera de vocabulario: Jev no reconocio casi nada (menos de 2 piezas) -> planificador LLM, si hay clave.
			// Su bolsa pasa por la misma validacion; si falla o tampoco trae piezas, se sigue con lo de Jev y el respaldo.
			if ((!bOk || C.Pieces.Num() < 2) && !N.StartsWith(TEXT("fal")) && !FJamDecisionClient::GetFalKey().IsEmpty())
			{
				FJamDecisionClient::RequestPlan(PromptCopy, [Finish, bOk, C, N](bool bPlanOk, const FJamBuildChoice& Plan, const FString& PlanNote)
				{
					if (bPlanOk && Plan.Pieces.Num() >= 2) { Finish(true, Plan, PlanNote); }
					else { Finish(bOk, C, N); }
				});
				return;
			}
			Finish(bOk, C, N);
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

void AJamBuildSpawner::Realize(const FBuildRecipe& InRecipe, const FString& InNote)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	ClearBuild();

	// Validador de sentido: toda receta (compositor, JSON fijo, plantilla, planificador) se puntua 0-100; por debajo del minimo
	// se recompone su bolsa con las reglas. Nunca bloquea: se construye lo mejor que haya y se le dice al jugador que paso.
	FBuildRecipe Recipe = InRecipe;
	FString Note = InNote;
	LastCoherence = -1;
	LastCoherenceNote.Reset();
	if (UJamAssembler::CoherenceMode() > 0)
	{
		FString What;
		const FJamCoherence Coh = UJamBuildGenerator::Repair(Recipe, What);
		LastCoherence = Coh.Score;
		TArray<FString> Msg;
		if (!What.IsEmpty()) { Msg.Add(What); Note += FString::Printf(TEXT(" repair=[%s] %s"), *What, *Coh.ToText()); }
		else if (!Note.Contains(TEXT("coh="))) { Note += TEXT(" ") + Coh.ToText(); }
		FString Left, Warn;
		if (Note.Split(TEXT("aviso=["), &Left, &Warn) && Warn.Split(TEXT("]"), &Warn, &Left)) { Msg.Add(Warn); }
		if (Coh.Score < 85)
		{
			TArray<FString> Why = Coh.Grave;
			Why.Append(Coh.Minor);
			if (Why.Num() > 2) { Why.SetNum(2); }
			Msg.Add((Coh.Score < UJamAssembler::MinCoherence() ? TEXT("No quedo bien: ") : TEXT("Ojo: ")) + FString::Join(Why, TEXT("; ")));
		}
		LastCoherenceNote = FString::Join(Msg, TEXT(". "));
		if (!LastCoherenceNote.IsEmpty()) { Announce(FString::Printf(TEXT("Jam: %s"), *LastCoherenceNote), Coh.Score >= UJamAssembler::MinCoherence()); }
	}

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
	if (Built.Num() > 0 && Built[0]->Build.IsValid()) { Built[0]->Build->Team = Team; Built[0]->Build->Owner = GetOwner(); }
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

	if (!bAutoActivate)
	{
		return;
	}
	World->GetTimerManager().SetTimer(ActivateTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		TArray<AJamBuildPart*> Raw;
		for (AJamBuildPart* P : Parts) { Raw.Add(P); }
		UJamAssembler::SetBuildActive(Raw, ActivateIds, true);
	}), FMath::Max(ActivateDelay, 0.01f), false);
}
