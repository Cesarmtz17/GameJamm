// Tests del solver por capacidades (sin mundo). Correr: editor -> Tools -> Session Frontend -> Automation -> "GameJamm.Building.Solver",
// o por consola del editor:  Automation RunTests GameJamm.Building.Solver
// Los casos de JamSolverCases.inl son el espejo exacto de Content/Game/Building/Data/solver_proto.py
// (regenerar con:  python3 -I Content/Game/Building/Data/solver_proto.py --emit-cpp).
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Building/BuildRecipe.h"
#include "Building/JamAssembler.h"
#include "Building/JamBuildPart.h"
#include "Building/JamLayout.h"
#include "Building/JamSolver.h"
#include "Core/InventoryComponent.h"
#include "UObject/Package.h"

// Namespace con nombre: en unity build no debe chocar con los helpers de JamRecipeTests.cpp.
namespace JamSolverTestsImpl
{
	/** Una fila de JamSolverCases.inl: entrada + resultado exacto del prototipo Python. */
	struct FMirrorCase
	{
		const TCHAR* Name;
		const TCHAR* Archetype;
		const TCHAR* Extras;      // "rocket,flame"
		const TCHAR* Wants;       // "Head,Battery,Battery"
		const TCHAR* Inventory;   // "Plank:1,Wheel:4"
		bool bUnlimited;
		float Energy;
		bool bBuildable;
		const TCHAR* Template;
		const TCHAR* Parts;       // "id:type:parent:socket|..."
		const TCHAR* Activate;    // "id,id"
		const TCHAR* Subs;        // "role:wanted>used:count|..."
		const TCHAR* Reductions;  // "role:wanted:got|..."
		const TCHAR* Missing;     // "Type:n,Type:n"
		const TCHAR* Unsupported; // "Head,Mirror"
		int32 Trimmed;
		const TCHAR* Orientation;
		float Cost;
		float Duration;
	};

	const FMirrorCase MirrorCases[] = {
#include "Building/Tests/JamSolverCases.inl"
	};

	FString NameOrEmpty(FName Name)
	{
		return Name.IsNone() ? FString() : Name.ToString();
	}

	TArray<FName> SplitNames(const TCHAR* Text)
	{
		TArray<FString> Tokens;
		FString(Text).ParseIntoArray(Tokens, TEXT(","), true);
		TArray<FName> Out;
		for (const FString& T : Tokens)
		{
			Out.Add(FName(*T));
		}
		return Out;
	}

	FJamSolverRequest MakeRequest(const TCHAR* Archetype, const TCHAR* Extras, const TCHAR* Wants, const TCHAR* Inventory, bool bUnlimited = false, float Energy = -1.f)
	{
		FJamSolverRequest Req;
		Req.Choice.Archetype = FName(Archetype);
		Req.Choice.Extras = SplitNames(Extras);
		Req.Choice.WantTypes = SplitNames(Wants);
		Req.bUnlimitedInventory = bUnlimited;
		Req.Energy = Energy;
		TArray<FString> Items;
		FString(Inventory).ParseIntoArray(Items, TEXT(","), true);
		for (const FString& Item : Items)
		{
			FString Type, Count;
			if (Item.Split(TEXT(":"), &Type, &Count))
			{
				Req.Inventory.Add(FName(*Type), FCString::Atoi(*Count));
			}
		}
		return Req;
	}

	FString PartsToString(const FBuildRecipe& R)
	{
		TArray<FString> Out;
		for (const FBuildRecipePart& P : R.Parts)
		{
			Out.Add(FString::Printf(TEXT("%s:%s:%s:%s"), *NameOrEmpty(P.Id), *NameOrEmpty(P.Type), *NameOrEmpty(P.AttachTo), *NameOrEmpty(P.Socket)));
		}
		return FString::Join(Out, TEXT("|"));
	}

	FString NamesToString(const TArray<FName>& Names)
	{
		TArray<FString> Out;
		for (const FName& N : Names)
		{
			Out.Add(N.ToString());
		}
		return FString::Join(Out, TEXT(","));
	}

	FString SubsToString(const FJamSolverResult& Res)
	{
		TArray<FString> Out;
		for (const FJamSolverSubstitution& S : Res.Substitutions)
		{
			Out.Add(FString::Printf(TEXT("%s:%s>%s:%d"), *S.Role, *S.Wanted.ToString(), *S.Used.ToString(), S.Count));
		}
		return FString::Join(Out, TEXT("|"));
	}

	FString ReductionsToString(const FJamSolverResult& Res)
	{
		TArray<FString> Out;
		for (const FJamSolverReduction& R : Res.Reductions)
		{
			Out.Add(FString::Printf(TEXT("%s:%d:%d"), *R.Role, R.Wanted, R.Got));
		}
		return FString::Join(Out, TEXT("|"));
	}

	FString MissingToString(const FJamSolverResult& Res)
	{
		TArray<FString> Out;
		for (const FJamSolverMissing& M : Res.Missing)
		{
			Out.Add(FString::Printf(TEXT("%s:%d"), *M.Type.ToString(), M.Count));
		}
		return FString::Join(Out, TEXT(","));
	}

	int32 CountOfType(const FBuildRecipe& R, const TCHAR* Type)
	{
		int32 Count = 0;
		for (const FBuildRecipePart& P : R.Parts)
		{
			Count += P.Type == FName(Type) ? 1 : 0;
		}
		return Count;
	}

	/** Compara texto sin distinguir mayúsculas (los FName no garantizan conservarlas fuera del editor). */
	void ExpectText(FAutomationTestBase& Test, const FString& Ctx, const TCHAR* Field, const FString& Actual, const FString& Expected)
	{
		if (!Actual.Equals(Expected, ESearchCase::IgnoreCase))
		{
			Test.AddError(FString::Printf(TEXT("%s: %s = '%s', expected '%s'"), *Ctx, Field, *Actual, *Expected));
		}
	}

	/** Receta válida para el ensamblador y sin piezas encimadas (salvo padre/hijo). */
	void ExpectAssemblable(FAutomationTestBase& Test, const FBuildRecipe& R, const FString& Ctx)
	{
		FString Err;
		if (!UBuildRecipeLibrary::ValidateRecipe(R, nullptr, Err))
		{
			Test.AddError(FString::Printf(TEXT("%s: invalid recipe: %s"), *Ctx, *Err));
			return;
		}
		TMap<FName, FJamPlacedPart> Placed;
		if (!JamLayout::Compute(R, Placed, Err))
		{
			Test.AddError(FString::Printf(TEXT("%s: layout failed: %s"), *Ctx, *Err));
			return;
		}
		for (const TPair<FName, FJamPlacedPart>& A : Placed)
		{
			for (const TPair<FName, FJamPlacedPart>& B : Placed)
			{
				if (A.Key.LexicalLess(B.Key) && A.Value.Parent != B.Key && B.Value.Parent != A.Key && JamLayout::Overlaps(A.Value, B.Value))
				{
					Test.AddError(FString::Printf(TEXT("%s: '%s' overlaps '%s'"), *Ctx, *A.Key.ToString(), *B.Key.ToString()));
				}
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJamSolverKnowledgeLoads, "GameJamm.Building.Solver.KnowledgeLoads",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJamSolverKnowledgeLoads::RunTest(const FString& Parameters)
{
	// El JSON real del proyecto carga y trae una plantilla por cada arquetipo del código.
	if (!TestTrue(FString::Printf(TEXT("loads %s"), *UJamSolver::GetKnowledgePath()), UJamSolver::LoadKnowledge(true)))
	{
		return true;
	}
	for (const FJamArchetypeDef& A : UJamBuildGenerator::ArchetypeTable())
	{
		TestTrue(FString::Printf(TEXT("template for archetype %s"), *A.Name.ToString()), UJamSolver::HasTemplate(A.Name));
	}
	TestTrue(TEXT("json-only template Spinner"), UJamSolver::HasTemplate(FName(TEXT("Spinner"))));
	TestFalse(TEXT("unknown template"), UJamSolver::HasTemplate(FName(TEXT("Submarine"))));
	TestTrue(TEXT("Seat is infinite"), UJamSolver::IsInfiniteType(FName(TEXT("Seat"))));
	TestFalse(TEXT("Wheel is not infinite"), UJamSolver::IsInfiniteType(FName(TEXT("Wheel"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJamSolverMirrorCases, "GameJamm.Building.Solver.MirrorPythonCases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJamSolverMirrorCases::RunTest(const FString& Parameters)
{
	using namespace JamSolverTestsImpl;
	if (!TestTrue(TEXT("knowledge loads"), UJamSolver::LoadKnowledge(true)))
	{
		return true;
	}

	for (const FMirrorCase& C : MirrorCases)
	{
		const FString Ctx = C.Name;

		// El caso espera que ciertas piezas AÚN no existan en el catálogo; si ya se agregaron, regenerar el espejo.
		bool bStale = false;
		for (const FName& U : SplitNames(C.Unsupported))
		{
			FJamPartDef Def;
			bStale = bStale || (UJamPartCatalog::Find(U, Def) && Def.Type == U);
		}
		if (bStale)
		{
			AddWarning(FString::Printf(TEXT("%s: skipped, '%s' now exists in UJamPartCatalog (regenerate JamSolverCases.inl)"), *Ctx, C.Unsupported));
			continue;
		}

		const FJamSolverRequest Req = MakeRequest(C.Archetype, C.Extras, C.Wants, C.Inventory, C.bUnlimited, C.Energy);
		FJamSolverResult Res;
		const bool bOk = UJamSolver::Solve(Req, Res);

		TestTrue(Ctx + TEXT(": return == bBuildable"), bOk == Res.bBuildable);
		TestTrue(FString::Printf(TEXT("%s: buildable = %d, expected %d"), *Ctx, Res.bBuildable ? 1 : 0, C.bBuildable ? 1 : 0), Res.bBuildable == C.bBuildable);
		ExpectText(*this, Ctx, TEXT("template"), NameOrEmpty(Res.Template), C.Template);
		ExpectText(*this, Ctx, TEXT("parts"), PartsToString(Res.Recipe), C.Parts);
		ExpectText(*this, Ctx, TEXT("activate"), NamesToString(Res.Recipe.Activate), C.Activate);
		ExpectText(*this, Ctx, TEXT("substitutions"), SubsToString(Res), C.Subs);
		ExpectText(*this, Ctx, TEXT("reductions"), ReductionsToString(Res), C.Reductions);
		ExpectText(*this, Ctx, TEXT("missing"), MissingToString(Res), C.Missing);
		ExpectText(*this, Ctx, TEXT("unsupported"), NamesToString(Res.Unsupported), C.Unsupported);
		ExpectText(*this, Ctx, TEXT("orientation"), NameOrEmpty(Res.Orientation), C.Orientation);
		TestEqual(Ctx + TEXT(": trimmed"), Res.Trimmed.Num(), C.Trimmed);
		TestTrue(FString::Printf(TEXT("%s: cost %.3f ~ %.3f"), *Ctx, Res.EnergyCostPerSecond, C.Cost), FMath::IsNearlyEqual(Res.EnergyCostPerSecond, C.Cost, 0.01f));
		TestTrue(FString::Printf(TEXT("%s: duration %.3f ~ %.3f"), *Ctx, Res.DurationSeconds, C.Duration), FMath::IsNearlyEqual(Res.DurationSeconds, C.Duration, 0.05f));

		if (Res.bBuildable)
		{
			TestTrue(Ctx + TEXT(": <= MaxParts"), Res.Recipe.Parts.Num() <= UJamAssembler::MaxParts);
			TestFalse(Ctx + TEXT(": message not empty"), Res.Message.IsEmpty());
			ExpectAssemblable(*this, Res.Recipe, Ctx);

			// Nunca usa más de lo que hay en el inventario (el Seat es gratis).
			if (!C.bUnlimited)
			{
				TMap<FName, int32> UsedByType;
				for (const FBuildRecipePart& P : Res.Recipe.Parts)
				{
					UsedByType.FindOrAdd(P.Type) += 1;
				}
				for (const TPair<FName, int32>& KV : UsedByType)
				{
					if (!UJamSolver::IsInfiniteType(KV.Key))
					{
						TestTrue(FString::Printf(TEXT("%s: uses %d %s, inventory has %d"), *Ctx, KV.Value, *KV.Key.ToString(), Req.Inventory.FindRef(KV.Key)),
							KV.Value <= Req.Inventory.FindRef(KV.Key));
					}
				}
			}
		}
		else
		{
			TestEqual(Ctx + TEXT(": not buildable -> empty recipe"), Res.Recipe.Parts.Num(), 0);
		}

		// Determinismo: misma entrada, misma receta.
		FJamSolverResult Again;
		UJamSolver::Solve(Req, Again);
		ExpectText(*this, Ctx, TEXT("deterministic parts"), PartsToString(Again.Recipe), PartsToString(Res.Recipe));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJamSolverHeadlineBuilds, "GameJamm.Building.Solver.HeadlineBuilds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJamSolverHeadlineBuilds::RunTest(const FString& Parameters)
{
	using namespace JamSolverTestsImpl;
	if (!TestTrue(TEXT("knowledge loads"), UJamSolver::LoadKnowledge(true)))
	{
		return true;
	}
	FJamSolverResult Res;

	// "una rueda acostada con dos lanzallamas que gire y queme todo": nadie programó ese objeto; sale de las capacidades.
	TestTrue(TEXT("spinner builds"), UJamSolver::Solve(MakeRequest(TEXT("Spinner"), TEXT("flame"), TEXT(""), TEXT("Wheel:1,Flamethrower:2")), Res));
	TestEqual(TEXT("spinner: 3 parts"), Res.Recipe.Parts.Num(), 3);
	TestEqual(TEXT("spinner: 2 flamethrowers"), CountOfType(Res.Recipe, TEXT("Flamethrower")), 2);
	if (Res.Recipe.Parts.Num() > 0)
	{
		TestEqual(TEXT("spinner: root is the wheel"), Res.Recipe.Parts[0].Type.ToString(), FString(TEXT("Wheel")));
		TestTrue(TEXT("spinner: root has no parent"), Res.Recipe.Parts[0].AttachTo.IsNone());
	}
	TestEqual(TEXT("spinner: lies flat"), Res.Orientation.ToString(), FString(TEXT("flat")));
	TestEqual(TEXT("spinner: everything activates"), Res.Recipe.Activate.Num(), 3);

	// Carro sin ruedas ni nada que lo mueva: no se entrega una receta inmóvil; se piden materiales.
	TestFalse(TEXT("car without wheels is not buildable"), UJamSolver::Solve(MakeRequest(TEXT("Car"), TEXT(""), TEXT(""), TEXT("Plank:1")), Res));
	TestEqual(TEXT("car without wheels: empty recipe"), Res.Recipe.Parts.Num(), 0);
	TestEqual(TEXT("car without wheels: asks for wheels"), MissingToString(Res), FString(TEXT("Wheel:2")));
	TestFalse(TEXT("car without wheels: message"), Res.Message.IsEmpty());

	// Carro sin ruedas pero con ventiladores: cambia de forma en vez de fallar.
	TestTrue(TEXT("car -> hoverbike"), UJamSolver::Solve(MakeRequest(TEXT("Car"), TEXT(""), TEXT(""), TEXT("Plank:1,Fan:2")), Res));
	TestTrue(TEXT("degraded"), Res.bDegraded);
	TestEqual(TEXT("template"), Res.Template.ToString(), FString(TEXT("Hoverbike")));
	TestEqual(TEXT("requested"), Res.RequestedTemplate.ToString(), FString(TEXT("Car")));

	// Torreta sin armas: el arquetipo exige atacar.
	TestFalse(TEXT("turret without weapons"), UJamSolver::Solve(MakeRequest(TEXT("Turret"), TEXT(""), TEXT(""), TEXT("Block:2,Plank:3")), Res));
	TestEqual(TEXT("turret asks for a cannon"), MissingToString(Res), FString(TEXT("Cannon:1")));

	// Seat gratis e infinito: no está en el inventario y aun así aparece; pedir tres no cuesta nada.
	TestTrue(TEXT("car with 3 seats"), UJamSolver::Solve(MakeRequest(TEXT("Car"), TEXT(""), TEXT("Seat,Seat,Seat"), TEXT("Plank:1,Wheel:4")), Res));
	TestEqual(TEXT("3 seats"), CountOfType(Res.Recipe, TEXT("Seat")), 3);
	TestEqual(TEXT("nothing missing"), Res.Missing.Num(), 0);

	// Más de 21 piezas pedidas: se recorta al límite del ensamblador.
	FString ManyBatteries;
	for (int32 i = 0; i < 40; ++i)
	{
		ManyBatteries += TEXT("Battery,");
	}
	TestTrue(TEXT("car with 40 batteries"), UJamSolver::Solve(MakeRequest(TEXT("Car"), TEXT(""), *ManyBatteries, TEXT("Plank:1,Wheel:4,Battery:40")), Res));
	TestEqual(TEXT("capped at MaxParts"), Res.Recipe.Parts.Num(), static_cast<int32>(UJamAssembler::MaxParts));
	TestEqual(TEXT("trimmed 25"), Res.Trimmed.Num(), 25);
	ExpectAssemblable(*this, Res.Recipe, TEXT("40 batteries"));

	// Energía: ejemplo del doc 13 §6 (4 ruedas + 1 ventilador = 2.3/s; barra llena = ~43 s).
	TestTrue(TEXT("car with fan"), UJamSolver::Solve(MakeRequest(TEXT("Car"), TEXT("fan"), TEXT(""), TEXT("Plank:1,Wheel:4,Fan:1"), false, 100.f), Res));
	TestTrue(TEXT("2.3/s"), FMath::IsNearlyEqual(Res.EnergyCostPerSecond, 2.3f, 0.01f));
	TestTrue(TEXT("~43 s"), FMath::IsNearlyEqual(Res.DurationSeconds, 100.f / 2.3f, 0.05f));
	TestFalse(TEXT("not low"), Res.bLowEnergy);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJamSolverFallbackAndIntegration, "GameJamm.Building.Solver.FallbackAndIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJamSolverFallbackAndIntegration::RunTest(const FString& Parameters)
{
	using namespace JamSolverTestsImpl;
	FBuildRecipe R;
	FString Note, Err;
	FJamSolverResult Res;

	// 1) Sin conocimiento (JSON ausente o inválido): el solver dice que no y el generador sigue por la tabla de siempre.
	UJamSolver::UnloadKnowledge();
	TestFalse(TEXT("no knowledge -> Solve false"), UJamSolver::Solve(MakeRequest(TEXT("Car"), TEXT(""), TEXT(""), TEXT(""), true), Res));
	TestFalse(TEXT("no knowledge flag"), Res.bKnowledgeLoaded);
	TestTrue(TEXT("legacy generate still works"), UJamBuildGenerator::GenerateRecipe(TEXT("un carro con cohete"), R, Note));
	TestTrue(TEXT("legacy note"), Note.StartsWith(TEXT("archetype=")));
	TestEqual(TEXT("legacy car + rocket = 7 parts"), R.Parts.Num(), 7);
	FJamBuildChoice Choice;
	UJamBuildGenerator::ChoiceFromKeywords(TEXT("una rueda acostada con dos lanzallamas que gire"), Choice);
	TestEqual(TEXT("no knowledge: keywords stay inside ArchetypeTable"), Choice.Archetype.ToString(), FString(TEXT("Car")));

	TestFalse(TEXT("garbage json rejected"), UJamSolver::LoadKnowledgeFromString(TEXT("{\"elements\": 3"), Err));
	TestFalse(TEXT("json without templates rejected"), UJamSolver::LoadKnowledgeFromString(TEXT("{\"elements\":[{\"type\":\"Plank\"}]}"), Err));
	TestTrue(TEXT("after bad json legacy still works"), UJamBuildGenerator::GenerateRecipe(TEXT("un tanque"), R, Note));
	TestTrue(TEXT("legacy note 2"), Note.StartsWith(TEXT("archetype=")));

	// 2) Con conocimiento: GenerateRecipe pasa por el solver.
	if (!TestTrue(TEXT("knowledge reloads"), UJamSolver::LoadKnowledge(true)))
	{
		return true;
	}
	TestTrue(TEXT("solver generate"), UJamBuildGenerator::GenerateRecipe(TEXT("un carro con cohete"), R, Note));
	TestTrue(FString::Printf(TEXT("solver note (%s)"), *Note), Note.StartsWith(TEXT("solver ")));
	TestEqual(TEXT("car + rocket = 7 parts"), R.Parts.Num(), 7);
	TestEqual(TEXT("1 rocket"), CountOfType(R, TEXT("Rocket")), 1);
	TestEqual(TEXT("4 wheels"), CountOfType(R, TEXT("Wheel")), 4);
	ExpectAssemblable(*this, R, TEXT("carro con cohete"));

	// Prompt estrella: palabras clave -> plantilla solo-JSON -> receta.
	UJamBuildGenerator::ChoiceFromKeywords(TEXT("una rueda acostada con dos lanzallamas que gire y queme todo"), Choice);
	TestEqual(TEXT("keywords -> Spinner"), Choice.Archetype.ToString(), FString(TEXT("Spinner")));
	TestTrue(TEXT("flame extra"), Choice.Extras.Contains(FName(TEXT("flame"))));
	TestTrue(TEXT("spinner composes"), UJamBuildGenerator::ComposeRecipe(Choice, R, Note));
	TestEqual(TEXT("spinner 3 parts"), R.Parts.Num(), 3);
	TestEqual(TEXT("spinner 2 flamethrowers"), CountOfType(R, TEXT("Flamethrower")), 2);
	ExpectAssemblable(*this, R, TEXT("rueda acostada"));

	UJamBuildGenerator::ChoiceFromKeywords(TEXT("un globo con fuego debajo"), Choice);
	TestEqual(TEXT("keywords -> Airship"), Choice.Archetype.ToString(), FString(TEXT("Airship")));
	TestTrue(TEXT("airship composes"), UJamBuildGenerator::ComposeRecipe(Choice, R, Note));
	TestEqual(TEXT("airship: 1 balloon"), CountOfType(R, TEXT("Balloon")), 1);
	TestEqual(TEXT("airship: 1 flamethrower under it"), CountOfType(R, TEXT("Flamethrower")), 1);
	ExpectAssemblable(*this, R, TEXT("globo con fuego"));

	UJamBuildGenerator::ChoiceFromKeywords(TEXT("un carro con globo"), Choice);
	TestEqual(TEXT("vehicle word wins over balloon"), Choice.Archetype.ToString(), FString(TEXT("Car")));

	UJamBuildGenerator::ChoiceFromKeywords(TEXT("una torreta que apunte sola"), Choice);
	TestEqual(TEXT("keywords -> Turret"), Choice.Archetype.ToString(), FString(TEXT("Turret")));
	TestTrue(TEXT("wants Head"), Choice.WantTypes.Contains(FName(TEXT("Head"))));
	TestTrue(TEXT("turret composes with or without Head in the catalog"), UJamBuildGenerator::ComposeRecipe(Choice, R, Note));
	TestTrue(TEXT("turret has a weapon"), CountOfType(R, TEXT("Cannon")) == 1);
	ExpectAssemblable(*this, R, TEXT("torreta que apunta sola"));

	// 3) Inventario desde el componente compartido, sin tocar su API: "Part.<Tipo>" -> tipo; lo demás se ignora.
	UInventoryComponent* Inventory = NewObject<UInventoryComponent>(GetTransientPackage());
	Inventory->AddItem(FName(TEXT("Part.Plank")), 1);
	Inventory->AddItem(FName(TEXT("Part.Wheel")), 2);
	Inventory->AddItem(FName(TEXT("Mat.Fire")), 5);
	FJamSolverRequest Req;
	Req.Choice.Archetype = FName(TEXT("Car"));
	Req.Inventory = UJamSolver::InventoryFromComponent(Inventory);
	TestEqual(TEXT("2 part types"), Req.Inventory.Num(), 2);
	TestEqual(TEXT("2 wheels"), Req.Inventory.FindRef(FName(TEXT("Wheel"))), 2);
	TestTrue(TEXT("car from component inventory"), UJamSolver::Solve(Req, Res));
	TestEqual(TEXT("reduced to 2 wheels"), CountOfType(Res.Recipe, TEXT("Wheel")), 2);
	TestEqual(TEXT("one reduction"), Res.Reductions.Num(), 1);
	TestEqual(TEXT("asks for 2 more wheels"), MissingToString(Res), FString(TEXT("Wheel:2")));
	TestEqual(TEXT("component untouched"), Inventory->GetCount(FName(TEXT("Part.Wheel"))), 2);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
