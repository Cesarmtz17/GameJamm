#include "Building/JamBuildSpawner.h"

#include "Building/BuildRecipe.h"
#include "Building/JamAssembler.h"
#include "Building/JamBuildPart.h"
#include "Building/JamDecisionClient.h"
#include "Building/JamInventorySolver.h"
#include "Core/InventoryComponent.h"
#include "Core/JamLoot.h"
#include "Core/JamAudio.h"
#include "Core/JamMessages.h"
#include "Components/BillboardComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogJamBuild, Log, All);

namespace
{
	// Avisos al jugador del constructor (solo si bAnnounceOnScreen; los bots no avisan). El texto de depuracion va al log.
	void JamBSPost(const AJamBuildSpawner* S, const FString& Text, const FLinearColor& Color, const TCHAR* Key, float Seconds = 8.f)
	{
		if (S && S->bAnnounceOnScreen) { JamMsg::Post(Text, Color, Seconds, FName(Key)); }
	}

	void JamBSFail(const AJamBuildSpawner* S, const TCHAR* Reason)
	{
		JamBSPost(S, FString::Printf(TEXT("Couldn't build that: %s"), Reason), JamMsg::Bad, TEXT("build"));
	}

	/** Contenido entre corchetes tras una etiqueta de la nota ("skipped=[Wall x1]" -> "Wall x1"). */
	bool JamBSBracket(const FString& Src, const TCHAR* Tag, FString& Out)
	{
		const int32 A = Src.Find(Tag);
		if (A == INDEX_NONE) { return false; }
		FString Rest = Src.Mid(A + FCString::Strlen(Tag));
		int32 End = INDEX_NONE;
		if (!Rest.FindChar(TEXT(']'), End)) { return false; }
		Out = Rest.Left(End);
		return !Out.IsEmpty();
	}

	/** "Wheel x2, Wall x1" -> "Rueda x2, Pared x1" (nombres en espanol). */
	FString JamBSPrettyList(const FString& Raw)
	{
		TArray<FString> Items;
		Raw.ParseIntoArray(Items, TEXT(","), true);
		for (FString& It : Items)
		{
			It.TrimStartAndEndInline();
			FString Type = It, Rest;
			int32 Cut = INDEX_NONE;
			if (It.FindLastChar(TEXT(' '), Cut)) { Type = It.Left(Cut); Rest = It.Mid(Cut); }
			It = JamLoot::PartName(Type) + Rest;
		}
		return FString::Join(Items, TEXT(", "));
	}

	/** Lo que faltaba segun la nota del ajuste de inventario ("faltan Wheel x2; ... skipped=[Wall x1]") y/o la nota de la composicion. */
	FString JamBSMissing(const FString& InventoryNote, const FString& ComposeNote)
	{
		TArray<FString> Lacks;
		TArray<FString> Segs;
		InventoryNote.ParseIntoArray(Segs, TEXT(";"), true);
		for (FString& Seg : Segs)
		{
			Seg.TrimStartAndEndInline();
			if (Seg.StartsWith(TEXT("faltan "))) { Lacks.Add(Seg.RightChop(7)); }
		}
		FString Skipped;
		if (JamBSBracket(InventoryNote.IsEmpty() ? ComposeNote : InventoryNote, TEXT("skipped=["), Skipped)) { Lacks.Add(Skipped); }
		return Lacks.Num() > 0 ? JamBSPrettyList(FString::Join(Lacks, TEXT(", "))) : FString();
	}

	/** Nombre para "Armaste ...": el de la receta si es util; si es generico ("Construccion") o un arquetipo en ingles, el pedido del jugador entre comillas. */
	FString JamBSName(const FString& RecipeName, const FString& Prompt)
	{
		const bool bGeneric = RecipeName.IsEmpty() || RecipeName == TEXT("Construccion") || RecipeName == TEXT("Construction")
			|| UJamBuildGenerator::ArchetypeTable().ContainsByPredicate([&RecipeName](const FJamArchetypeDef& A) { return A.Name == FName(*RecipeName); });
		if (!bGeneric) { return RecipeName; }
		FString P = Prompt.TrimStartAndEnd();
		if (P.IsEmpty()) { return TEXT("your build"); }
		if (P.Len() > 36) { P = P.Left(34) + TEXT("..."); }
		return TEXT("\"") + P + TEXT("\"");
	}
}

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
	// Solo al log: lo que ve el jugador sale por JamMsg (JamBSPost) con textos en espanol, no con estas notas de depuracion.
	UE_LOG(LogJamBuild, Log, TEXT("Aviso%s: %s"), bGood ? TEXT("") : TEXT(" (mal)"), *Msg);
}

void AJamBuildSpawner::SetBuildActive(bool bOn)
{
	TArray<AJamBuildPart*> Raw;
	for (AJamBuildPart* P : Parts) { Raw.Add(P); }
	// Apagar (tecla C): las piezas ya suenan Power_On al encenderse; el apagado lo suena el constructor, una vez, solo al del jugador.
	if (!bOn && bAnnounceOnScreen && IsBuildActive())
	{
		for (const AJamBuildPart* P : Parts)
		{
			if (IsValid(P)) { JamAudio::PlayAt(this, TEXT("SFX_Power_Off"), P->GetActorLocation(), 0.7f); break; }
		}
	}
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

int32 AJamBuildSpawner::CountAliveParts() const
{
	int32 Alive = 0;
	for (const AJamBuildPart* P : Parts)
	{
		if (IsValid(P) && !P->IsActorBeingDestroyed() && !P->bBroken) { ++Alive; }
	}
	return Alive;
}

TMap<FName, int32> AJamBuildSpawner::GetRefundableItems() const
{
	TMap<FName, int32> Out;
	if (!PaidInventory.IsValid() || PaidItems.Num() == 0) { return Out; }
	// Piezas vivas por id de inventario ("Part.<Tipo>", el mismo que cobra GetRequiredItems): lo destruido no se devuelve.
	TMap<FName, int32> Alive;
	for (const AJamBuildPart* P : Parts)
	{
		if (IsValid(P) && !P->IsActorBeingDestroyed() && !P->bBroken && !P->PartType.IsNone())
		{
			Alive.FindOrAdd(FName(*(FString(TEXT("Part.")) + P->PartType.ToString()))) += 1;
		}
	}
	for (const auto& Item : PaidItems)
	{
		const int32 N = FMath::Min(Item.Value, Alive.FindRef(Item.Key));
		if (N > 0) { Out.Add(Item.Key, N); }
	}
	return Out;
}

void AJamBuildSpawner::RebindOwner(AActor* NewOwner)
{
	SetOwner(NewOwner);
	UInventoryComponent* Inv = NewOwner ? NewOwner->FindComponentByClass<UInventoryComponent>() : nullptr;
	if (Inv) { PaidInventory = Inv; } else { PaidInventory.Reset(); }
	for (AJamBuildPart* P : Parts)
	{
		if (IsValid(P) && P->Build.IsValid()) { P->Build->Owner = NewOwner; break; } // el estado es compartido por todas las piezas
	}
}

void AJamBuildSpawner::ClearBuild()
{
	++RequestGeneration; // una respuesta tardia no puede reconstruir lo que se desarmo
	const TMap<FName, int32> Refund = GetRefundableItems(); // solo lo que sigue vivo
	PaidItems.Reset();
	if (UInventoryComponent* Inventory = PaidInventory.Get())
	{
		for (const auto& Item : Refund) { Inventory->AddItem(Item.Key, Item.Value); }
	}
	PaidInventory.Reset();
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
	const uint64 Generation = ++RequestGeneration;
	if (bAnnounceOnScreen) { JamMsg::Remove(FName(TEXT("build2"))); JamMsg::Remove(FName(TEXT("buildai"))); } // las lineas "Falto: ..." y "... eligio: ..." del pedido anterior

	FBuildRecipe Recipe;
	FString Note, Err;

	// 1) Receta JSON ya hecha (salida de un LLM).
	if (!RecipeJsonOverride.IsEmpty())
	{
		if (!UBuildRecipeLibrary::ParseRecipeJson(RecipeJsonOverride, Recipe, Err))
		{
			LastStatus = FString::Printf(TEXT("FAIL recipe: %s"), *Err);
			UE_LOG(LogJamBuild, Warning, TEXT("%s"), *LastStatus);
			JamBSFail(this, TEXT("invalid recipe"));
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
		JamBSFail(this, TEXT("I couldn't understand the request"));
		return;
	}
	if (bUseDecisionModel && (!FJamDecisionClient::GetTypeSafeKey().IsEmpty() || !FJamDecisionClient::GetApiKey().IsEmpty()
		|| !FJamDecisionClient::GetOpenAIKey().IsEmpty() || !FJamDecisionClient::GetFalKey().IsEmpty()))
	{
		LastStatus = TEXT("waiting for decision model...");
		Announce(FString::Printf(TEXT("Jam: \"%s\" -> preguntando al modelo de decision (Jev / OpenAI)..."), *Prompt), true);
		JamBSPost(this, TEXT("Thinking..."), JamMsg::Info, TEXT("build"), 10.f);
		const FString PromptCopy = Prompt;
		TWeakObjectPtr<AJamBuildSpawner> Weak(this);
		// Composicion comun a Jev y al planificador: bolsa -> numerales del texto -> raiz señalada -> compositor con sentido -> Realize.
		auto Finish = [Weak, PromptCopy, Generation](bool bOk, const FJamBuildChoice& C, const FString& N)
		{
			AJamBuildSpawner* Self = Weak.Get();
			if (!Self || Self->RequestGeneration != Generation)
			{
				return;
			}
			FBuildRecipe R;
			FString Note2 = N;
			FString Picked; // lo que ELIGIO el modelo, con nombres del catalogo, para la linea "OpenAI eligio: ..."
			bool bComposed = false;
			if (bOk && C.Pieces.Num() > 0)
			{
				// Ruta principal: el modelo decidio piezas y cantidades. Los numerales del texto ("25 llantas") mandan sobre
				// la cantidad. Una bolsa pequena se completa salvo que ya tenga una base y un arma sin movilidad pendiente.
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
					const bool bHasBase = Bag.ContainsByPredicate([](const FJamPieceCount& I)
					{
						FJamPartDef Def;
						return I.Count > 0 && UJamPartCatalog::Find(I.Type, Def)
							&& (Def.Kind == EJamPartKind::Structural || Def.Kind == EJamPartKind::Slide);
					});
					const bool bHasWeapon = Bag.ContainsByPredicate([](const FJamPieceCount& I)
					{
						const FJamPieceRule* Rule = UJamBuildGenerator::PieceTable().FindByPredicate([&I](const FJamPieceRule& R) { return R.Type == I.Type; });
						FJamPartDef Def;
						return I.Count > 0 && Rule && Rule->bWeapon && UJamPartCatalog::Find(I.Type, Def) && Def.Kind != EJamPartKind::Light;
					});
					const bool bConceptNeedsMobility = Seed.ContainsByPredicate([&KindOf](const FJamPieceCount& I)
					{
						const EJamPartKind Kd = KindOf(I.Type);
						return I.Count > 0 && (Kd == EJamPartKind::Wheel || Kd == EJamPartKind::Fan || Kd == EJamPartKind::Rocket
							|| Kd == EJamPartKind::Balloon || Kd == EJamPartKind::Hover || Kd == EJamPartKind::Sail || Kd == EJamPartKind::Wing);
					});
					// Beam + Block ya es una torreta laser: no agregar canon ni bateria del arquetipo por tener solo dos tipos.
					// Plank solo sigue incompleto; Plank + Flame en un pedido de carro aun necesita las ruedas del concepto.
					const bool bKeepSmallWeaponBuild = Bag.Num() == 2 && bHasBase && bHasWeapon && !bConceptNeedsMobility;
					if (!bKeepSmallWeaponBuild)
					{
						for (const FJamPieceCount& S : Seed)
						{
							const EJamPartKind Kd = KindOf(S.Type);
							const bool bSameKind = (Kd == EJamPartKind::Wheel || Kd == EJamPartKind::Fan) && Bag.ContainsByPredicate([&](const FJamPieceCount& X) { return KindOf(X.Type) == Kd; });
							if (!bSameKind && !Bag.ContainsByPredicate([&S](const FJamPieceCount& X) { return X.Type == S.Type; })) { Bag.Add(S); }
						}
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
				// Que se vea la IA: los tipos que eligio el modelo (no los que agrego el codigo ni los que puso el respaldo por
				// palabras tras una negativa: "kw=[...]" de la nota), con la cantidad final. El cuerpo va primero.
				{
					FString KwRaw;
					TArray<FString> KwTypes, Shown;
					JamBSBracket(N, TEXT("kw=["), KwRaw);
					KwRaw.ParseIntoArray(KwTypes, TEXT(","), true);
					for (const FJamPieceCount& I : Bag)
					{
						const bool bByModel = I.Type == C.Root || C.Pieces.ContainsByPredicate([&I](const FJamPieceCount& X) { return X.Type == I.Type; });
						if (!bByModel || I.Count <= 0 || KwTypes.Contains(I.Type.ToString())) { continue; }
						const FString Text = JamLoot::PartName(I.Type.ToString()) + (I.Count > 1 ? FString::Printf(TEXT(" x%d"), I.Count) : FString());
						if (I.Mode == "root") { Shown.Insert(Text, 0); } else { Shown.Add(Text); }
					}
					Picked = FString::Join(Shown, TEXT(", "));
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
				if (!Picked.IsEmpty())
				{
					const TCHAR* Who = Backend.StartsWith(TEXT("openai")) ? TEXT("OpenAI") : Backend.StartsWith(TEXT("jev")) ? TEXT("Jev") : TEXT("AI");
					JamBSPost(Self, FString::Printf(TEXT("%s picked: %s"), Who, *Picked.Left(110)), JamMsg::Info, TEXT("buildai"));
				}
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
			// Fuera de vocabulario: el modelo (Jev u OpenAI) no reconocio casi nada (menos de 2 piezas) -> planificador LLM, si hay clave.
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
		JamBSFail(this, TEXT("I couldn't understand the request"));
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
			Msg.Add((Coh.Score < UJamAssembler::MinCoherence() ? TEXT("Unstable build: ") : TEXT("Note: ")) + FString::Join(Why, TEXT("; ")));
		}
		LastCoherenceNote = FString::Join(Msg, TEXT(". "));
		if (!LastCoherenceNote.IsEmpty()) { Announce(FString::Printf(TEXT("Jam: %s"), *LastCoherenceNote), Coh.Score >= UJamAssembler::MinCoherence()); }
	}

	UInventoryComponent* Inventory = bUseInventory && GetOwner() ? GetOwner()->FindComponentByClass<UInventoryComponent>() : nullptr;
	FString MissingText = JamBSMissing(FString(), Note); // lo que no cupo / falto, para la linea "Falto: ..." del jugador
	if (bUseInventory)
	{
		if (!Inventory) { LastStatus = TEXT("FAIL inventory: owner has no inventory"); Announce(LastStatus, false); JamBSFail(this, TEXT("you have no backpack")); return; }
		TMap<FName, int32> Available = Inventory->Items;
		if (PaidInventory.Get() == Inventory) { for (const auto& Item : GetRefundableItems()) { Available.FindOrAdd(Item.Key) += Item.Value; } } // solo lo vivo: ClearBuild solo devuelve eso
		FBuildRecipe Fitted;
		FString InventoryNote;
		if (!JamInventorySolver::Fit(Recipe, Available, Fitted, InventoryNote))
		{
			LastStatus = TEXT("Not enough parts for a useful build: ") + InventoryNote;
			Announce(LastStatus, false);
			const FString Lacking = JamBSMissing(InventoryNote, FString());
			JamBSFail(this, Lacking.IsEmpty() ? TEXT("your parts can't make a stable build") : TEXT("you need more parts"));
			if (!Lacking.IsEmpty()) { JamBSPost(this, TEXT("Missing: ") + Lacking.Left(90), JamMsg::Warn, TEXT("build2")); }
			return; // conserva la construccion anterior y sus piezas
		}
		Recipe = MoveTemp(Fitted);
		Note += TEXT(" inventory=[") + InventoryNote + TEXT("]");
		LastCoherence = UJamAssembler::Coherence(Recipe).Score;
		if (!InventoryNote.IsEmpty())
		{
			Announce(InventoryNote, true);
			MissingText = JamBSMissing(InventoryNote, FString()); // el ajuste rehizo la receta: lo de antes ya no vale
		}
	}
	LastRecipeJson = UBuildRecipeLibrary::RecipeToJson(Recipe);

	// Apoyo: una construccion larga sobre terreno desigual nacia medio enterrada y la fisica la disparaba al separarla.
	// Se mira el suelo bajo toda su huella (rejilla 5 x 3) y se sube hasta que su punto mas bajo quede sobre el punto mas alto.
	FTransform At = GetActorTransform();
	{
		const FBox Box = UJamAssembler::LayoutBounds(Recipe);
		if (Box.IsValid)
		{
			FCollisionQueryParams Q(NAME_None, false, this);
			Q.AddIgnoredActor(GetOwner());
			double Top = -TNumericLimits<double>::Max();
			for (int32 ix = 0; ix < 5; ++ix)
			{
				for (int32 iy = 0; iy < 3; ++iy)
				{
					const FVector P = At.TransformPosition(FVector(FMath::Lerp(Box.Min.X, Box.Max.X, ix / 4.0), FMath::Lerp(Box.Min.Y, Box.Max.Y, iy / 2.0), 0.0));
					FHitResult Hit;
					if (World->LineTraceSingleByChannel(Hit, P + FVector(0, 0, 400), P - FVector(0, 0, 600), ECC_Visibility, Q) && !Cast<AJamBuildPart>(Hit.GetActor()))
					{
						Top = FMath::Max(Top, Hit.ImpactPoint.Z);
					}
				}
			}
			if (Top > At.GetLocation().Z) { At.AddToTranslation(FVector(0, 0, Top - At.GetLocation().Z + 3.0)); }
		}
	}

	FString Err;
	TArray<AJamBuildPart*> Built;
	if (!UJamAssembler::Assemble(this, Recipe, At, Built, Err))
	{
		LastStatus = FString::Printf(TEXT("FAIL assemble: %s"), *Err);
		UE_LOG(LogJamBuild, Warning, TEXT("%s"), *LastStatus);
		Announce(TEXT("Jam: ") + LastStatus, false);
		JamBSFail(this, TEXT("the parts couldn't be assembled"));
		return;
	}
	// Solo sustituir la construccion anterior cuando la nueva ya existe.
	ClearBuild();
	if (Inventory)
	{
		const TMap<FName, int32> Cost = UBuildRecipeLibrary::GetRequiredItems(Recipe);
		if (!Inventory->ConsumeItems(Cost))
		{
			UJamAssembler::DestroyBuild(Built);
			LastStatus = TEXT("FAIL inventory: inventory changed while building");
			Announce(LastStatus, false);
			JamBSFail(this, TEXT("your backpack changed while building"));
			return;
		}
		PaidInventory = Inventory;
		PaidItems = Cost;
	}
	for (AJamBuildPart* P : Built) { Parts.Add(P); }
	if (Built.Num() > 0 && Built[0]->Build.IsValid()) { Built[0]->Build->Team = Team; Built[0]->Build->Owner = GetOwner(); }
	ActivateIds = Recipe.Activate;
	LastStatus = FString::Printf(TEXT("OK %d parts (%s) '%s'"), Built.Num(), *Note, *Recipe.Name);
	UE_LOG(LogJamBuild, Log, TEXT("%s %s"), *LastStatus, *LastRecipeJson);
	if (bAnnounceOnScreen)
	{
		// Un solo pop al aparecer la construccion del jugador (los bots no suenan para no hacer ruido).
		if (Built.Num() > 0 && IsValid(Built[0])) { JamAudio::PlayAt(this, TEXT("SFX_Build_Pop"), Built[0]->GetActorLocation(), 0.8f); }
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
		// Lo que ve el jugador: "Armaste ...: N piezas" y, si algo no cupo o falto, "Falto: ...". Misma Key que "Pensando...".
		JamBSPost(this, FString::Printf(TEXT("Built %s: %d %s"), *JamBSName(Recipe.Name, Prompt), Built.Num(), Built.Num() == 1 ? TEXT("part") : TEXT("parts")), JamMsg::Good, TEXT("build"));
		if (!MissingText.IsEmpty()) { JamBSPost(this, TEXT("Missing: ") + MissingText.Left(90), JamMsg::Warn, TEXT("build2")); }
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
