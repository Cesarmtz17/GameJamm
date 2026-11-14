// Chequeo de datos de construccion: tablas C++ (piezas, arquetipos, extras) coherentes entre si y con BuildKnowledge.json.
// Correr: UnrealEditor-Cmd GameJamm.uproject -ExecCmds="Automation RunTests Jam.Building;Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Building/JamAssembler.h"
#include "Building/JamBuildPart.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJamBuildTableTest, "Jam.Building.Tables", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FJamBuildTableTest::RunTest(const FString& Parameters)
{
	const TArray<FJamArchetypeDef>& Archs = UJamBuildGenerator::ArchetypeTable();
	const TArray<FJamExtraDef>& Extras = UJamBuildGenerator::ExtraTable();
	FJamPartDef D;
	FString Info;

	auto ArchExists = [&](FName Name) { return Archs.ContainsByPredicate([&](const FJamArchetypeDef& A) { return A.Name == Name; }); };

	// 1) Arquetipos: plantillas validas, con piezas del catalogo y sin piezas encimadas.
	for (const FJamArchetypeDef& A : Archs)
	{
		FBuildRecipe R;
		R.Parts = A.Parts;
		TestTrue(*FString::Printf(TEXT("%s: receta base valida (%s)"), *A.Name.ToString(), *Info), UBuildRecipeLibrary::ValidateRecipe(R, nullptr, Info));
		TestTrue(*FString::Printf(TEXT("%s: la raiz se llama body"), *A.Name.ToString()), A.Parts.Num() > 0 && A.Parts[0].Id == "body" && A.Parts[0].AttachTo.IsNone());
		for (const FBuildRecipePart& P : A.Parts)
		{
			TestTrue(*FString::Printf(TEXT("%s: tipo '%s' existe"), *A.Name.ToString(), *P.Type.ToString()), UJamPartCatalog::Find(P.Type, D));
		}
		Info.Reset();
		TestFalse(*FString::Printf(TEXT("%s: base sin solapes (%s)"), *A.Name.ToString(), *Info), UJamAssembler::HasOverlap(R, NAME_None, Info));
	}

	// 2) Extras: pieza existente, arquetipos existentes, caras validas, claves unicas, limite de preguntas.
	TSet<FName> Keys;
	for (const FJamExtraDef& E : Extras)
	{
		const FString K = E.Key.ToString();
		TestFalse(*FString::Printf(TEXT("extra %s: clave repetida"), *K), Keys.Contains(E.Key));
		Keys.Add(E.Key);
		TestTrue(*FString::Printf(TEXT("extra %s: textos true/false"), *K), !E.TrueText.IsEmpty() && !E.FalseText.IsEmpty() && E.TrueText != E.FalseText);
		TestTrue(*FString::Printf(TEXT("extra %s: palabras clave"), *K), E.Keywords.Num() > 0);
		if (!E.PartType.IsNone())
		{
			TestTrue(*FString::Printf(TEXT("extra %s: tipo '%s' existe"), *K, *E.PartType.ToString()), UJamPartCatalog::Find(E.PartType, D));
			TestTrue(*FString::Printf(TEXT("extra %s: tiene caras"), *K), E.Sockets.Num() > 0);
		}
		if (!E.ChildType.IsNone())
		{
			TestTrue(*FString::Printf(TEXT("extra %s: hijo '%s' existe"), *K, *E.ChildType.ToString()), UJamPartCatalog::Find(E.ChildType, D));
		}
		for (const FName& A : E.Allowed)
		{
			TestTrue(*FString::Printf(TEXT("extra %s: arquetipo '%s' existe"), *K, *A.ToString()), ArchExists(A));
		}
		TArray<FString> AllSockets = E.Sockets;
		AllSockets.Append(E.FlyerSockets);
		if (!E.MountSockets.IsEmpty()) { AllSockets.Add(E.MountSockets); }
		if (!E.ChildSocket.IsNone()) { AllSockets.Add(E.ChildSocket.ToString()); }
		for (const FString& Entry : AllSockets)
		{
			TArray<FString> Opts;
			Entry.ParseIntoArray(Opts, TEXT("|"));
			for (const FString& Opt : Opts)
			{
				FString Parent, Socket = Opt;
				Opt.Split(TEXT(":"), &Parent, &Socket);
				FBuildRecipe R;
				FBuildRecipePart Body, P;
				Body.Id = "body"; Body.Type = "Plank";
				P.Id = "p"; P.Type = E.PartType.IsNone() ? FName("Block") : E.PartType; P.AttachTo = "body"; P.Socket = FName(*Socket);
				R.Parts = { Body, P };
				Info.Reset();
				TestFalse(*FString::Printf(TEXT("extra %s: cara '%s' valida (%s)"), *K, *Opt, *Info), UJamAssembler::HasOverlap(R, NAME_None, Info));
			}
		}
	}
	TestTrue(TEXT("maximo 30 preguntas noul por llamada a Jev"), Extras.Num() <= 30);

	// 3) Cada arquetipo con TODOS los extras a la vez: receta valida, <= MaxParts y sin piezas encimadas.
	for (const FJamArchetypeDef& A : Archs)
	{
		FJamBuildChoice C;
		C.Archetype = A.Name;
		for (const FJamExtraDef& E : Extras) { C.Extras.Add(E.Key); }
		FBuildRecipe R;
		FString Note;
		TestTrue(*FString::Printf(TEXT("%s + todos los extras: compone (%s)"), *A.Name.ToString(), *Note), UJamBuildGenerator::ComposeRecipe(C, R, Note));
		TestTrue(*FString::Printf(TEXT("%s + todos: %d piezas <= MaxParts"), *A.Name.ToString(), R.Parts.Num()), R.Parts.Num() <= UJamAssembler::GetMaxParts());
		Info.Reset();
		const bool bOverlap = UJamAssembler::HasOverlap(R, NAME_None, Info);
		TestFalse(*FString::Printf(TEXT("%s + todos: sin solapes (%s)"), *A.Name.ToString(), *Info), bOverlap);
		UE_LOG(LogTemp, Display, TEXT("JamTables: %s + todos -> %d piezas, %s"), *A.Name.ToString(), R.Parts.Num(), *Note);
		AddInfo(FString::Printf(TEXT("%s + todos -> %d piezas, %s"), *A.Name.ToString(), R.Parts.Num(), *Note));
	}

	// 3b) Composer por piezas (sin red): cantidades exactas, sin solapes, dentro del limite.
	{
		struct FCase { const TCHAR* Prompt; const TCHAR* Type; int32 Count; };
		const FCase Cases[] = {
			{ TEXT("arma un carro con 25 llantas"), TEXT("Wheel"), 25 }, { TEXT("un puente con 10 tablas"), TEXT("Plank"), 10 },
			{ TEXT("un tanque con 4 cañones"), TEXT("Cannon"), 4 }, { TEXT("un cohete con 3 propulsores"), TEXT("Rocket"), 3 },
			{ TEXT("una torre de 12 bloques"), TEXT("Block"), 12 }, { TEXT("una catapulta"), TEXT("Spring"), 1 },
			{ TEXT("un carro con seis ruedas y dos lanzallamas"), TEXT("Flamethrower"), 2 },
		};
		for (const FCase& Cs : Cases)
		{
			FBuildRecipe R;
			FString Note;
			TestTrue(*FString::Printf(TEXT("composer '%s': compone (%s)"), Cs.Prompt, *Note), UJamBuildGenerator::GenerateRecipe(Cs.Prompt, R, Note));
			int32 N = 0;
			for (const FBuildRecipePart& P : R.Parts) { N += P.Type == FName(Cs.Type) ? 1 : 0; }
			TestEqual(*FString::Printf(TEXT("composer '%s': %s (%s)"), Cs.Prompt, Cs.Type, *Note), N, Cs.Count);
			TestTrue(*FString::Printf(TEXT("composer '%s': %d piezas <= limite"), Cs.Prompt, R.Parts.Num()), R.Parts.Num() <= UJamAssembler::GetMaxParts());
			Info.Reset();
			const bool bOver = UJamAssembler::HasOverlap(R, NAME_None, Info);
			TestFalse(*FString::Printf(TEXT("composer '%s': sin solapes (%s)"), Cs.Prompt, *Info), bOver);
			UE_LOG(LogTemp, Display, TEXT("JamTables: composer '%s' -> %d piezas, %s"), Cs.Prompt, R.Parts.Num(), *Note);
		}
	}

	// 4) Respaldo por palabras clave: frases de referencia.
	{
		FJamBuildChoice C;
		UJamBuildGenerator::ChoiceFromKeywords(TEXT("un tanque con turbina y laser"), C);
		TestEqual(TEXT("keywords: tanque"), C.Archetype, FName("Tank"));
		TestTrue(TEXT("keywords: turbina -> turbine"), C.Extras.Contains("turbine"));
		TestFalse(TEXT("keywords: turbina no activa fan"), C.Extras.Contains("fan"));
		TestTrue(TEXT("keywords: laser -> beam"), C.Extras.Contains("beam"));
		UJamBuildGenerator::ChoiceFromKeywords(TEXT("coche con helice y globo"), C);
		TestEqual(TEXT("keywords: coche"), C.Archetype, FName("Car"));
		TestTrue(TEXT("keywords: helice -> fan"), C.Extras.Contains("fan"));
		TestFalse(TEXT("keywords: helice no activa frost"), C.Extras.Contains("frost"));
		TestFalse(TEXT("keywords: globo no activa ball"), C.Extras.Contains("ball"));
	}

	// 5) Las tablas C++ no contradicen a BuildKnowledge.json (tipos, masa, energia, capacidad, arquetipos).
	FString Json;
	TSharedPtr<FJsonObject> Root;
	const FString Path = FPaths::ProjectContentDir() / TEXT("Game/Building/Data/BuildKnowledge.json");
	if (!FFileHelper::LoadFileToString(Json, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
	{
		AddError(FString::Printf(TEXT("no se pudo leer %s"), *Path));
		return false;
	}
	TMap<FName, TSharedPtr<FJsonObject>> ByType;
	for (const TSharedPtr<FJsonValue>& V : Root->GetArrayField(TEXT("elements")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		const FName Type(*O->GetStringField(TEXT("type")));
		ByType.Add(Type, O);
		bool bInCode = false;
		if (O->TryGetBoolField(TEXT("in_code"), bInCode) && bInCode)
		{
			TestTrue(*FString::Printf(TEXT("JSON in_code '%s' existe en el catalogo C++"), *Type.ToString()), UJamPartCatalog::Find(Type, D));
		}
	}
	for (const FName& Type : UJamPartCatalog::GetTypes())
	{
		UJamPartCatalog::Find(Type, D);
		const TSharedPtr<FJsonObject>* O = ByType.Find(Type);
		if (!O)
		{
			AddError(FString::Printf(TEXT("tipo C++ '%s' no esta en BuildKnowledge.json"), *Type.ToString()));
			continue;
		}
		double Num = 0;
		if ((*O)->TryGetNumberField(TEXT("mass_kg"), Num)) { TestEqual(*FString::Printf(TEXT("%s: mass_kg"), *Type.ToString()), D.MassKg, (float)Num); }
		if ((*O)->TryGetNumberField(TEXT("energy_cost_per_s"), Num)) { TestEqual(*FString::Printf(TEXT("%s: energy_cost_per_s"), *Type.ToString()), D.EnergyPerSecond, (float)Num); }
		if ((*O)->TryGetNumberField(TEXT("energy_capacity"), Num)) { TestEqual(*FString::Printf(TEXT("%s: energy_capacity"), *Type.ToString()), D.Power, (float)Num); }
	}
	double MaxParts = 0;
	if (Root->TryGetNumberField(TEXT("max_parts"), MaxParts)) { TestEqual(TEXT("max_parts"), UJamAssembler::GetMaxParts(), (int32)MaxParts); }
	for (const TSharedPtr<FJsonValue>& V : Root->GetArrayField(TEXT("templates")))
	{
		FString Code;
		if (V->AsObject()->TryGetStringField(TEXT("code_archetype"), Code) && !Code.IsEmpty())
		{
			TestTrue(*FString::Printf(TEXT("JSON code_archetype '%s' existe en ArchetypeTable"), *Code), ArchExists(FName(*Code)));
		}
	}
	return true;
}

#endif
