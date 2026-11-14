#include "Building/JamPoc.h"

#include "Building/BuildRecipe.h"
#include "Building/JamAssembler.h"
#include "Building/JamBuildPart.h"
#include "Building/JamDecisionClient.h"
#include "Building/JamSolver.h"
#include "Core/InventoryComponent.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogJamPoc, Log, All);

// Namespace con nombre (no anónimo): el módulo compila en unity build.
namespace JamPocImpl
{
	const float SpawnDistance = 500.f;
	const float ActivateDelay = 1.5f;
	const float MessageSeconds = 20.f;

	/** Una construcción hecha con Jam.Build: sus piezas y lo que consumió (para devolverlo con Jam.Clear). */
	struct FBuiltRecord
	{
		TArray<TWeakObjectPtr<AJamBuildPart>> Parts;
		TMap<FName, int32> ConsumedTypes;
		bool bFromDebugInventory = false;
		TWeakObjectPtr<UInventoryComponent> PlayerInventory;
	};

	struct FPocState
	{
		TMap<FName, int32> DebugInventory;
		TArray<FBuiltRecord> Builds;
		FString LastSummary;
	};

	FPocState& State()
	{
		static FPocState Instance;
		return Instance;
	}

	/** Líneas a pantalla (la primera arriba) y al log. */
	void Show(const TArray<FString>& Lines, const FColor& Color)
	{
		for (const FString& Line : Lines)
		{
			UE_LOG(LogJamPoc, Display, TEXT("%s"), *Line);
		}
		if (GEngine)
		{
			// Los mensajes nuevos salen encima: se agregan de la última a la primera para leerlos de arriba abajo.
			for (int32 i = Lines.Num() - 1; i >= 0; --i)
			{
				GEngine->AddOnScreenDebugMessage(-1, MessageSeconds, Color, Lines[i]);
			}
		}
	}

	void ShowLine(const FString& Line, const FColor& Color)
	{
		TArray<FString> Lines;
		Lines.Add(Line);
		Show(Lines, Color);
	}

	FString NamesToText(const TArray<FName>& Names)
	{
		TArray<FString> Out;
		for (const FName& N : Names)
		{
			Out.Add(N.ToString());
		}
		return FString::Join(Out, TEXT(", "));
	}

	/** "Plank x1, Wheel x4" en el orden en que aparecen en la receta. */
	FString PartsToText(const FBuildRecipe& Recipe)
	{
		TArray<FName> Order;
		TMap<FName, int32> Counts;
		for (const FBuildRecipePart& P : Recipe.Parts)
		{
			Order.AddUnique(P.Type);
			Counts.FindOrAdd(P.Type) += 1;
		}
		TArray<FString> Out;
		for (const FName& T : Order)
		{
			Out.Add(FString::Printf(TEXT("%s x%d"), *T.ToString(), Counts.FindRef(T)));
		}
		return FString::Join(Out, TEXT(", "));
	}

	UInventoryComponent* FindPlayerInventory(UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		return Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
	}

	/** ~5 m delante de la cámara, sobre el suelo, mirando hacia donde mira el jugador. flat = acostada (90° sobre X). */
	FTransform ComputeOrigin(UWorld* World, FName Orientation)
	{
		FVector ViewLocation = FVector::ZeroVector;
		FRotator ViewRotation = FRotator::ZeroRotator;
		APlayerController* PC = World->GetFirstPlayerController();
		if (PC)
		{
			PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
		}
		const FRotator YawOnly(0.0, ViewRotation.Yaw, 0.0);
		FVector Target = ViewLocation + YawOnly.Vector() * SpawnDistance;

		// Suelo: solo geometría estática (no las construcciones anteriores).
		FHitResult Hit;
		const FVector Start = Target + FVector(0.0, 0.0, 300.0);
		const FVector End = Target - FVector(0.0, 0.0, 5000.0);
		if (World->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic)))
		{
			Target.Z = Hit.ImpactPoint.Z;
		}
		else
		{
			Target.Z = ViewLocation.Z - 100.0;
		}

		FQuat Rotation = YawOnly.Quaternion();
		if (Orientation == FName(TEXT("flat")))
		{
			Rotation = Rotation * FQuat(FVector::ForwardVector, UE_HALF_PI);
			Target.Z += 100.0; // acostada, el "abajo" de la receta queda de lado: que no nazca dentro del suelo
		}
		else if (Orientation == FName(TEXT("inverted")))
		{
			Rotation = Rotation * FQuat(FVector::ForwardVector, UE_PI);
			Target.Z += 150.0;
		}
		return FTransform(Rotation, Target);
	}

	/** Decisiones ya tomadas (modelo o palabras clave) -> solver -> ensamblar -> resumen en pantalla. */
	void BuildFromChoice(UWorld* World, const FString& Prompt, const FJamBuildChoice& Choice, const FString& Source)
	{
		FPocState& S = State();
		TArray<FString> Lines;
		Lines.Add(FString::Printf(TEXT("[Jam.Build] \"%s\""), *Prompt));
		Lines.Add(FString::Printf(TEXT("Decisión: %s -> arquetipo=%s extras=[%s] quiere=[%s]"), *Source, *Choice.Archetype.ToString(),
			*NamesToText(Choice.Extras), *NamesToText(Choice.WantTypes)));

		// Inventario: el de depuración (Jam.Inv) si tiene algo; si no, el del jugador; si no hay ninguno, ilimitado.
		UInventoryComponent* PlayerInventory = FindPlayerInventory(World);
		FJamSolverRequest Request;
		Request.Choice = Choice;
		bool bFromDebugInventory = false;
		if (S.DebugInventory.Num() > 0)
		{
			Request.Inventory = S.DebugInventory;
			bFromDebugInventory = true;
			Lines.Add(TEXT("Inventario: de depuración (Jam.Inv / Jam.InvAll; Jam.InvClear lo quita)"));
		}
		else if (PlayerInventory)
		{
			Request.Inventory = UJamSolver::InventoryFromComponent(PlayerInventory);
			Lines.Add(TEXT("Inventario: UInventoryComponent del jugador"));
		}
		else
		{
			Request.bUnlimitedInventory = true;
			PlayerInventory = nullptr;
			Lines.Add(TEXT("Inventario: ilimitado (no hay inventario de depuración ni del jugador)"));
		}

		FBuildRecipe Recipe;
		FName Orientation;
		FColor Color = FColor::Green;
		const bool bUsedSolver = UJamSolver::LoadKnowledge();
		if (bUsedSolver)
		{
			FJamSolverResult Result;
			UJamSolver::Solve(Request, Result);
			if (!Result.bBuildable)
			{
				Lines.Add(FString::Printf(TEXT("Solver: NO se arma (%s)"), *Result.Note));
				Lines.Add(FString::Printf(TEXT("Asistente: %s"), *Result.Message));
				S.LastSummary = FString::Join(Lines, TEXT("\n"));
				Show(Lines, FColor::Red);
				return;
			}
			Recipe = Result.Recipe;
			Orientation = Result.Orientation;

			Lines.Add(FString::Printf(TEXT("Solver: plantilla %s (pedida %s)%s | %d piezas | orientación %s"), *Result.Template.ToString(),
				*Result.RequestedTemplate.ToString(), Result.bDegraded ? TEXT(" DEGRADADA") : TEXT(""), Recipe.Parts.Num(), *Result.Orientation.ToString()));
			Lines.Add(FString::Printf(TEXT("Piezas: %s"), *PartsToText(Recipe)));
			for (const FJamSolverSubstitution& Sub : Result.Substitutions)
			{
				Lines.Add(FString::Printf(TEXT("Sustitución (%s): %s en vez de %s x%d"), *Sub.Role, *Sub.Used.ToString(), *Sub.Wanted.ToString(), Sub.Count));
			}
			for (const FJamSolverReduction& Red : Result.Reductions)
			{
				Lines.Add(FString::Printf(TEXT("Reducción: %s de %d a %d"), *Red.Role, Red.Wanted, Red.Got));
			}
			if (Result.Added.Num() > 0)
			{
				Lines.Add(FString::Printf(TEXT("Agregado para que se mueva: %s"), *NamesToText(Result.Added)));
			}
			if (Result.Trimmed.Num() > 0)
			{
				Lines.Add(FString::Printf(TEXT("Recorte: %d piezas fuera por el límite de %d (%s)"), Result.Trimmed.Num(), static_cast<int32>(UJamAssembler::MaxParts), *NamesToText(Result.Trimmed)));
			}
			if (Result.Missing.Num() > 0)
			{
				TArray<FString> Items;
				for (const FJamSolverMissing& M : Result.Missing)
				{
					Items.Add(FString::Printf(TEXT("%d %s"), M.Count, *M.Type.ToString()));
				}
				Lines.Add(FString::Printf(TEXT("Materiales faltantes: %s"), *FString::Join(Items, TEXT(", "))));
			}
			if (Result.Unsupported.Num() > 0)
			{
				Lines.Add(FString::Printf(TEXT("Aún no existe en el catálogo: %s"), *NamesToText(Result.Unsupported)));
			}
			if (Result.EnergyCostPerSecond > 0.f)
			{
				Lines.Add(FString::Printf(TEXT("Energía: gasta %.1f/s, hay %.0f -> dura %.0f s%s | masa %.0f kg"), Result.EnergyCostPerSecond, Result.EnergyAvailable,
					Result.DurationSeconds, Result.bLowEnergy ? TEXT(" (POCA ENERGÍA)") : TEXT(""), Result.MassKg));
			}
			else
			{
				Lines.Add(FString::Printf(TEXT("Energía: no gasta | masa %.0f kg"), Result.MassKg));
			}
			Lines.Add(FString::Printf(TEXT("Asistente: %s"), *Result.Message));
			if (Result.bDegraded || Result.Missing.Num() > 0 || Result.Trimmed.Num() > 0 || Result.Substitutions.Num() > 0 || Result.Reductions.Num() > 0)
			{
				Color = FColor::Yellow;
			}
		}
		else
		{
			// Sin BuildKnowledge.json: camino anterior (tabla de arquetipos), sin inventario.
			FString Note;
			if (!UJamBuildGenerator::ComposeRecipe(Choice, Recipe, Note))
			{
				Lines.Add(FString::Printf(TEXT("Respaldo por tabla (sin BuildKnowledge.json): FALLÓ (%s)"), *Note));
				S.LastSummary = FString::Join(Lines, TEXT("\n"));
				Show(Lines, FColor::Red);
				return;
			}
			Lines.Add(FString::Printf(TEXT("Respaldo por tabla (sin BuildKnowledge.json): %s | %d piezas"), *Note, Recipe.Parts.Num()));
			Lines.Add(FString::Printf(TEXT("Piezas: %s"), *PartsToText(Recipe)));
			Color = FColor::Yellow;
		}

		TArray<AJamBuildPart*> Built;
		FString Error;
		if (!UJamAssembler::Assemble(World, Recipe, ComputeOrigin(World, Orientation), Built, Error))
		{
			Lines.Add(FString::Printf(TEXT("Ensamblador: FALLÓ (%s)"), *Error));
			S.LastSummary = FString::Join(Lines, TEXT("\n"));
			Show(Lines, FColor::Red);
			return;
		}

		// Descontar del inventario lo que se usó (el Seat es gratis). Jam.Clear lo devuelve.
		FBuiltRecord Record;
		Record.bFromDebugInventory = bFromDebugInventory;
		Record.PlayerInventory = PlayerInventory;
		for (AJamBuildPart* Part : Built)
		{
			Record.Parts.Add(Part);
		}
		if (!Request.bUnlimitedInventory)
		{
			for (const FBuildRecipePart& P : Recipe.Parts)
			{
				if (!UJamSolver::IsInfiniteType(P.Type))
				{
					Record.ConsumedTypes.FindOrAdd(P.Type) += 1;
				}
			}
			for (const TPair<FName, int32>& KV : Record.ConsumedTypes)
			{
				if (bFromDebugInventory)
				{
					// La entrada se queda aunque llegue a 0: así el inventario de depuración sigue mandando.
					int32& Count = S.DebugInventory.FindOrAdd(KV.Key);
					Count = FMath::Max(0, Count - KV.Value);
				}
				else if (PlayerInventory)
				{
					PlayerInventory->RemoveItem(FName(*FString::Printf(TEXT("Part.%s"), *KV.Key.ToString())), KV.Value);
				}
			}
		}
		S.Builds.Add(Record);

		// Encender motores/armas tras un momento, cuando ya asentó la física.
		const TArray<TWeakObjectPtr<AJamBuildPart>> WeakParts = Record.Parts;
		const TArray<FName> ActivateIds = Recipe.Activate;
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakParts, ActivateIds]()
		{
			TArray<AJamBuildPart*> Alive;
			for (const TWeakObjectPtr<AJamBuildPart>& Weak : WeakParts)
			{
				if (AJamBuildPart* Part = Weak.Get())
				{
					Alive.Add(Part);
				}
			}
			UJamAssembler::SetBuildActive(Alive, ActivateIds, true);
		}), ActivateDelay, false);

		Lines.Add(FString::Printf(TEXT("Ensamblado: %d piezas a %.0f cm; se enciende en %.1f s (%d dispositivos). Jam.Clear para quitarlo."),
			Built.Num(), SpawnDistance, ActivateDelay, ActivateIds.Num()));
		S.LastSummary = FString::Join(Lines, TEXT("\n"));
		Show(Lines, Color);
	}

	UWorld* GameWorldOrWarn(UWorld* World)
	{
		if (!World || !World->IsGameWorld())
		{
			ShowLine(TEXT("[Jam] Este comando solo funciona jugando (Play / PIE): dale Play y abre la consola con ~."), FColor::Red);
			return nullptr;
		}
		return World;
	}

	void CmdBuild(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() == 0)
		{
			ShowLine(TEXT("[Jam.Build] Uso: Jam.Build <pedido libre>. Ejemplos: Jam.Prompts"), FColor::Yellow);
			return;
		}
		if (GameWorldOrWarn(World))
		{
			UJamPoc::Build(World, FString::Join(Args, TEXT(" ")));
		}
	}

	void CmdInv(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 2 || !UJamPoc::SetDebugInventory(FName(*Args[0]), FCString::Atoi(*Args[1])))
		{
			ShowLine(FString::Printf(TEXT("[Jam.Inv] Uso: Jam.Inv <Tipo> <n>. Tipos: %s"), *NamesToText(UJamPartCatalog::GetTypes())), FColor::Yellow);
			return;
		}
		ShowLine(FString::Printf(TEXT("[Jam.Inv] %s = %s"), *Args[0], *Args[1]), FColor::Cyan);
	}

	void CmdInvShow(const TArray<FString>& Args, UWorld* World)
	{
		const TMap<FName, int32>& Inv = State().DebugInventory;
		if (Inv.Num() == 0)
		{
			ShowLine(TEXT("[Jam.InvShow] Inventario de depuración vacío: Jam.Build usa el del jugador o, si no hay, ilimitado."), FColor::Cyan);
			return;
		}
		TArray<FString> Items;
		for (const TPair<FName, int32>& KV : Inv)
		{
			Items.Add(FString::Printf(TEXT("%s x%d"), *KV.Key.ToString(), KV.Value));
		}
		ShowLine(FString::Printf(TEXT("[Jam.InvShow] %s (el Seat es gratis e infinito)"), *FString::Join(Items, TEXT(", "))), FColor::Cyan);
	}

	void CmdInvAll(const TArray<FString>& Args, UWorld* World)
	{
		const int32 Count = Args.Num() > 0 ? FMath::Max(0, FCString::Atoi(*Args[0])) : 10;
		UJamPoc::FillDebugInventory(Count);
		ShowLine(FString::Printf(TEXT("[Jam.InvAll] %d de cada pieza del catálogo (%d tipos)"), Count, State().DebugInventory.Num()), FColor::Cyan);
	}

	void CmdInvClear(const TArray<FString>& Args, UWorld* World)
	{
		UJamPoc::ClearDebugInventory();
		ShowLine(TEXT("[Jam.InvClear] Inventario de depuración vacío: Jam.Build usa el del jugador o, si no hay, ilimitado."), FColor::Cyan);
	}

	void CmdClear(const TArray<FString>& Args, UWorld* World)
	{
		const int32 Num = State().Builds.Num();
		UJamPoc::ClearBuilds();
		ShowLine(FString::Printf(TEXT("[Jam.Clear] %d construcciones destruidas; piezas devueltas al inventario."), Num), FColor::Cyan);
	}

	void CmdPrompts(const TArray<FString>& Args, UWorld* World)
	{
		TArray<FString> Lines;
		Lines.Add(TEXT("[Jam.Prompts] Escribe: Jam.Build <pedido>. Ejemplos:"));
		for (const FString& P : UJamPoc::GetExamplePrompts())
		{
			Lines.Add(TEXT("  Jam.Build ") + P);
		}
		Lines.Add(TEXT("  Inventario pobre: Jam.InvClear | Jam.Inv Plank 1 | Jam.Inv Fan 2 | Jam.Build un carro  (sin ruedas: sale moto flotante y pide ruedas)"));
		Show(Lines, FColor::Cyan);
	}

	void CmdReload(const TArray<FString>& Args, UWorld* World)
	{
		const bool bOk = UJamSolver::LoadKnowledge(true);
		ShowLine(FString::Printf(TEXT("[Jam.Reload] BuildKnowledge.json %s (%s)"), bOk ? TEXT("cargado") : TEXT("NO cargó: se usa el respaldo por tabla"),
			*UJamSolver::GetKnowledgePath()), bOk ? FColor::Cyan : FColor::Red);
	}

	FAutoConsoleCommandWithWorldAndArgs BuildCommand(TEXT("Jam.Build"),
		TEXT("Jam.Build <pedido libre>: arma una construcción delante de la cámara a partir de lenguaje natural."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CmdBuild));
	FAutoConsoleCommandWithWorldAndArgs InvCommand(TEXT("Jam.Inv"),
		TEXT("Jam.Inv <Tipo> <n>: pone n piezas de ese tipo en el inventario de depuración."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CmdInv));
	FAutoConsoleCommandWithWorldAndArgs InvAllCommand(TEXT("Jam.InvAll"),
		TEXT("Jam.InvAll [n]: inventario de depuración con n (10) de cada pieza del catálogo."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CmdInvAll));
	FAutoConsoleCommandWithWorldAndArgs InvClearCommand(TEXT("Jam.InvClear"),
		TEXT("Jam.InvClear: vacía el inventario de depuración."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CmdInvClear));
	FAutoConsoleCommandWithWorldAndArgs InvShowCommand(TEXT("Jam.InvShow"),
		TEXT("Jam.InvShow: lista el inventario de depuración."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CmdInvShow));
	FAutoConsoleCommandWithWorldAndArgs ClearCommand(TEXT("Jam.Clear"),
		TEXT("Jam.Clear: destruye lo construido con Jam.Build y devuelve las piezas."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CmdClear));
	FAutoConsoleCommandWithWorldAndArgs PromptsCommand(TEXT("Jam.Prompts"),
		TEXT("Jam.Prompts: imprime pedidos de ejemplo para Jam.Build."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CmdPrompts));
	FAutoConsoleCommandWithWorldAndArgs ReloadCommand(TEXT("Jam.Reload"),
		TEXT("Jam.Reload: vuelve a leer BuildKnowledge.json."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CmdReload));
}

void UJamPoc::Build(UObject* WorldContextObject, const FString& Prompt)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return;
	}

	const bool bHasKey = !FJamDecisionClient::GetTypeSafeKey().IsEmpty() || !FJamDecisionClient::GetApiKey().IsEmpty() || !FJamDecisionClient::GetFalKey().IsEmpty();
	if (bHasKey)
	{
		JamPocImpl::ShowLine(FString::Printf(TEXT("[Jam.Build] \"%s\": consultando al modelo de decisión..."), *Prompt), FColor::Cyan);
		const TWeakObjectPtr<UWorld> WeakWorld(World);
		FJamDecisionClient::RequestChoice(Prompt, [WeakWorld, Prompt](bool bOk, const FJamBuildChoice& ModelChoice, const FString& Note)
		{
			UWorld* CallbackWorld = WeakWorld.Get();
			if (!CallbackWorld)
			{
				return; // se cerró el Play mientras respondía
			}
			FJamBuildChoice Choice = ModelChoice;
			FString Source;
			if (bOk)
			{
				UJamSolver::RefineChoiceFromText(Prompt, Choice);
				Source = FString::Printf(TEXT("modelo de decisión (%s)"), *Note);
			}
			else
			{
				UJamBuildGenerator::ChoiceFromKeywords(Prompt, Choice);
				Source = FString::Printf(TEXT("RESPALDO por palabras clave (el modelo falló: %s)"), *Note);
			}
			JamPocImpl::BuildFromChoice(CallbackWorld, Prompt, Choice, Source);
		});
		return;
	}

	FJamBuildChoice Choice;
	UJamBuildGenerator::ChoiceFromKeywords(Prompt, Choice);
	JamPocImpl::BuildFromChoice(World, Prompt, Choice, TEXT("RESPALDO por palabras clave (sin API key)"));
}

void UJamPoc::ClearBuilds()
{
	JamPocImpl::FPocState& S = JamPocImpl::State();
	for (JamPocImpl::FBuiltRecord& Record : S.Builds)
	{
		TArray<AJamBuildPart*> Alive;
		for (const TWeakObjectPtr<AJamBuildPart>& Weak : Record.Parts)
		{
			if (AJamBuildPart* Part = Weak.Get())
			{
				Alive.Add(Part);
			}
		}
		if (Alive.Num() == 0)
		{
			continue; // de una sesión de Play anterior: ya no existe, no se devuelve nada
		}
		UJamAssembler::DestroyBuild(Alive);
		for (const TPair<FName, int32>& KV : Record.ConsumedTypes)
		{
			if (Record.bFromDebugInventory)
			{
				S.DebugInventory.FindOrAdd(KV.Key) += KV.Value;
			}
			else if (UInventoryComponent* Inventory = Record.PlayerInventory.Get())
			{
				Inventory->AddItem(FName(*FString::Printf(TEXT("Part.%s"), *KV.Key.ToString())), KV.Value);
			}
		}
	}
	S.Builds.Reset();
}

bool UJamPoc::SetDebugInventory(FName Type, int32 Count)
{
	FJamPartDef Def;
	if (!UJamPartCatalog::Find(Type, Def))
	{
		return false;
	}
	JamPocImpl::State().DebugInventory.FindOrAdd(Def.Type) = FMath::Max(0, Count);
	return true;
}

void UJamPoc::FillDebugInventory(int32 Count)
{
	TMap<FName, int32>& Inventory = JamPocImpl::State().DebugInventory;
	Inventory.Reset();
	for (const FName& Type : UJamPartCatalog::GetTypes())
	{
		Inventory.Add(Type, FMath::Max(0, Count));
	}
}

void UJamPoc::ClearDebugInventory()
{
	JamPocImpl::State().DebugInventory.Reset();
}

TMap<FName, int32> UJamPoc::GetDebugInventory()
{
	return JamPocImpl::State().DebugInventory;
}

FString UJamPoc::GetLastSummary()
{
	return JamPocImpl::State().LastSummary;
}

TArray<FString> UJamPoc::GetExamplePrompts()
{
	TArray<FString> Out;
	Out.Add(TEXT("un carro"));
	Out.Add(TEXT("un carro con cohete"));
	Out.Add(TEXT("un carro con ruedas grandes y un cañón"));
	Out.Add(TEXT("un cohete"));
	Out.Add(TEXT("un avión con ventilador"));
	Out.Add(TEXT("una torreta que apunte sola"));
	Out.Add(TEXT("un tanque con lanzallamas"));
	Out.Add(TEXT("una rueda acostada con dos lanzallamas que gire y queme todo"));
	Out.Add(TEXT("una rueda infernal"));
	Out.Add(TEXT("un globo con fuego debajo"));
	Out.Add(TEXT("un hovercraft con jet"));
	Out.Add(TEXT("una fortaleza con cañón"));
	return Out;
}
