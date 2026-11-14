// Tests de recetas y layout (sin mundo). Correr: editor -> Tools -> Session Frontend -> Automation -> "GameJamm.Building",
// o por consola:  UnrealEditor-Cmd.exe GameJamm.uproject -ExecCmds="Automation RunTests GameJamm.Building; Quit" -unattended -nullrhi
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Building/BuildRecipe.h"
#include "Building/JamAssembler.h"
#include "Building/JamBuildPart.h"
#include "Building/JamLayout.h"

namespace
{
	const FBuildRecipePart* FindPart(const FBuildRecipe& R, FName Id)
	{
		return R.Parts.FindByPredicate([Id](const FBuildRecipePart& P) { return P.Id == Id; });
	}

	/** Ninguna pareja de piezas (que no sean padre/hijo) se encima. */
	bool NoOverlaps(FAutomationTestBase& Test, const FBuildRecipe& R, const FString& Ctx)
	{
		TMap<FName, FJamPlacedPart> Placed;
		FString Err;
		if (!JamLayout::Compute(R, Placed, Err))
		{
			Test.AddError(FString::Printf(TEXT("%s: layout failed: %s"), *Ctx, *Err));
			return false;
		}
		bool bOk = true;
		for (const auto& A : Placed)
		{
			for (const auto& B : Placed)
			{
				if (A.Key.LexicalLess(B.Key) && A.Value.Parent != B.Key && B.Value.Parent != A.Key && JamLayout::Overlaps(A.Value, B.Value))
				{
					Test.AddError(FString::Printf(TEXT("%s: '%s' overlaps '%s'"), *Ctx, *A.Key.ToString(), *B.Key.ToString()));
					bOk = false;
				}
			}
		}
		return bOk;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJamRecipeParseCanonical, "GameJamm.Building.Recipe.ParseCanonical",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJamRecipeParseCanonical::RunTest(const FString& Parameters)
{
	// Ejemplo del GDD.
	const FString Json = TEXT(R"({"name":"Rueda Infernal","parts":[{"id":"p0","type":"Wheel"},{"id":"p1","type":"Flamethrower","attach_to":"p0","socket":"Right"}],"activate":["p0","p1"]})");
	FBuildRecipe R;
	FString Err;
	TestTrue(TEXT("parses"), UBuildRecipeLibrary::ParseRecipeJson(Json, R, Err));
	TestEqual(TEXT("2 parts"), R.Parts.Num(), 2);
	TestEqual(TEXT("activate 2"), R.Activate.Num(), 2);
	TestTrue(TEXT("roundtrip"), UBuildRecipeLibrary::ParseRecipeJson(UBuildRecipeLibrary::RecipeToJson(R), R, Err));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJamRecipeParseSloppyLlm, "GameJamm.Building.Recipe.ParseSloppyLlm",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJamRecipeParseSloppyLlm::RunTest(const FString& Parameters)
{
	// Lo que un LLM escribe de verdad: texto + ```json, ids de asset, Attach_, attachTo, ids numéricos, sin activate.
	const FString Json = TEXT("Claro, aquí está tu toma:\n```json\n{\"recipe\": {\"name\": \"Carrito\", \"parts\": ["
		"{\"id\": 0, \"type\": \"PART_PLANK\"},"
		"{\"id\": \"w1\", \"type\": \"DEV_WHEEL_S\", \"attachTo\": 0, \"socket\": \"Attach_Left_Front\"},"
		"{\"id\": \"w2\", \"type\": \"part.wheel\", \"parent\": 0, \"socket\": \"right front\"},"
		"{\"id\": \"fl\", \"type\": \"Flame\", \"attach_to\": 0, \"socket\": \"Nose\"},"
		"{\"id\": \"fan\", \"type\": \"DEV_FAN\", \"attach_to\": 0, \"socket\": \"Rear\"}"
		"]}}\n```\n¡Acción!");
	FBuildRecipe R;
	FString Err;
	TestTrue(FString::Printf(TEXT("parses (%s)"), *Err), UBuildRecipeLibrary::ParseRecipeJson(Json, R, Err));
	TestEqual(TEXT("5 parts"), R.Parts.Num(), 5);
	const FBuildRecipePart* W = FindPart(R, FName(TEXT("w1")));
	if (TestNotNull(TEXT("w1 exists"), W))
	{
		TestEqual(TEXT("type alias"), W->Type.ToString(), FString(TEXT("Wheel")));
		TestEqual(TEXT("socket canonical"), W->Socket.ToString(), FString(TEXT("Left_Front")));
		TestEqual(TEXT("numeric attachTo matches numeric root id"), W->AttachTo.ToString(), R.Parts[0].Id.ToString());
	}
	const FBuildRecipePart* F = FindPart(R, FName(TEXT("fl")));
	if (TestNotNull(TEXT("fl exists"), F))
	{
		TestEqual(TEXT("Flame -> Flamethrower"), F->Type.ToString(), FString(TEXT("Flamethrower")));
		TestEqual(TEXT("Nose -> Front"), F->Socket.ToString(), FString(TEXT("Front")));
	}
	TestEqual(TEXT("default activate = wheels+flame+fan"), R.Activate.Num(), 4);
	TestTrue(TEXT("inventory key canonical"), UBuildRecipeLibrary::GetRequiredItems(R).Contains(FName(TEXT("Part.Wheel"))));
	NoOverlaps(*this, R, TEXT("sloppy"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJamRecipeRepairStructure, "GameJamm.Building.Recipe.RepairStructure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJamRecipeRepairStructure::RunTest(const FString& Parameters)
{
	FBuildRecipe R;
	FString Err;

	// Padre inexistente, dos raíces, tipo desconocido con hijo, id duplicado, ciclo, activate con basura.
	const FString Json = TEXT(R"({"parts":[
		{"id":"body","type":"Plank"},
		{"id":"w0","type":"Wheel","attach_to":"chasis","socket":"Left"},
		{"id":"extra","type":"Block"},
		{"id":"laser","type":"DeathRay","attach_to":"body","socket":"Top"},
		{"id":"gun","type":"Cannon","attach_to":"laser","socket":"Front"},
		{"id":"w0","type":"Wheel","attach_to":"body","socket":"Right"},
		{"id":"a","type":"Battery","attach_to":"b"},
		{"id":"b","type":"Battery","attach_to":"a"}
	],"activate":["gun","nope"]})");
	TestTrue(FString::Printf(TEXT("repairs (%s)"), *Err), UBuildRecipeLibrary::ParseRecipeJson(Json, R, Err));
	TestTrue(TEXT("unknown type dropped"), FindPart(R, FName(TEXT("laser"))) == nullptr);
	const FBuildRecipePart* G = FindPart(R, FName(TEXT("gun")));
	if (TestNotNull(TEXT("gun exists"), G))
	{
		TestEqual(TEXT("child of dropped -> grandparent"), G->AttachTo.ToString(), FString(TEXT("body")));
	}
	const FBuildRecipePart* E = FindPart(R, FName(TEXT("extra")));
	if (TestNotNull(TEXT("extra exists"), E))
	{
		TestEqual(TEXT("second root -> root"), E->AttachTo.ToString(), FString(TEXT("body")));
	}
	TestEqual(TEXT("dup id renamed, 7 parts"), R.Parts.Num(), 7);
	TestEqual(TEXT("activate filtered"), R.Activate.Num(), 1);
	TestTrue(TEXT("valid"), UBuildRecipeLibrary::ValidateRecipe(R, nullptr, Err));
	NoOverlaps(*this, R, TEXT("repair"));

	// Más de MaxParts.
	FString Big = TEXT(R"({"parts":[{"id":"root","type":"Block"})");
	for (int32 i = 0; i < 30; ++i)
	{
		Big += FString::Printf(TEXT(R"(,{"id":"b%d","type":"Battery","attach_to":"%s","socket":"Top"})"), i, i == 0 ? TEXT("root") : *FString::Printf(TEXT("b%d"), i - 1));
	}
	Big += TEXT("]}");
	TestTrue(TEXT("big parses"), UBuildRecipeLibrary::ParseRecipeJson(Big, R, Err));
	TestEqual(TEXT("truncated"), R.Parts.Num(), static_cast<int32>(UJamAssembler::MaxParts));

	// Basura de verdad: debe fallar con error (el spawner cae al respaldo por palabras clave).
	TestFalse(TEXT("garbage fails"), UBuildRecipeLibrary::ParseRecipeJson(TEXT("lo siento, no puedo"), R, Err));
	TestFalse(TEXT("no parts fails"), UBuildRecipeLibrary::ParseRecipeJson(TEXT(R"({"parts":[{"type":"DeathRay"}]})"), R, Err));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJamRecipeComposeAll, "GameJamm.Building.Recipe.ComposeAllCombos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJamRecipeComposeAll::RunTest(const FString& Parameters)
{
	// Los 4 arquetipos × los 32 combos de extras: receta válida y sin piezas encimadas.
	const FName Archetypes[] = { TEXT("Car"), TEXT("Flyer"), TEXT("Turret"), TEXT("Rocket") };
	for (const FName& A : Archetypes)
	{
		for (int32 Mask = 0; Mask < 32; ++Mask)
		{
			FJamBuildChoice C;
			C.Archetype = A;
			C.bFan = (Mask & 1) != 0;
			C.bRocket = (Mask & 2) != 0;
			C.bFlame = (Mask & 4) != 0;
			C.bCannon = (Mask & 8) != 0;
			C.bBalloon = (Mask & 16) != 0;
			FBuildRecipe R;
			FString Note;
			const FString Ctx = FString::Printf(TEXT("%s mask=%d"), *A.ToString(), Mask);
			if (!TestTrue(Ctx + TEXT(" composes ") + Note, UJamBuildGenerator::ComposeRecipe(C, R, Note)))
			{
				continue;
			}
			NoOverlaps(*this, R, Ctx);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJamSocketParse, "GameJamm.Building.Recipe.Sockets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJamSocketParse::RunTest(const FString& Parameters)
{
	struct FCase { const TCHAR* In; const TCHAR* Out; };
	const FCase Cases[] = {
		{ TEXT("Top"), TEXT("Top") }, { TEXT("Attach_Left"), TEXT("Left") }, { TEXT("Side"), TEXT("Right") },
		{ TEXT("Left_Front"), TEXT("Left_Front") }, { TEXT("left side"), TEXT("Left") }, { TEXT("under"), TEXT("Bottom") },
		{ TEXT("Attach_Back_Right"), TEXT("Back_Right") }, { TEXT("rear-left"), TEXT("Back_Left") },
	};
	for (const FCase& C : Cases)
	{
		FVector Dir, Shift;
		bool bShift = false;
		if (TestTrue(FString::Printf(TEXT("parse %s"), C.In), JamLayout::ParseSocket(C.In, Dir, Shift, bShift)))
		{
			TestEqual(FString::Printf(TEXT("canonical %s"), C.In), JamLayout::SocketToString(Dir, Shift, bShift), FString(C.Out));
		}
	}
	FVector D, S;
	bool bS = false;
	TestFalse(TEXT("nonsense"), JamLayout::ParseSocket(TEXT("Attach_Hat"), D, S, bS));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
