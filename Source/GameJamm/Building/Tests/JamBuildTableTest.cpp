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

	// 3c) SENTIDO: 30 bolsas fijas (sin Jev ni palabras clave: mide el compositor y el validador) y 10 recetas malas a proposito.
	{
		struct FBagItem { const TCHAR* Type; int32 Count; const TCHAR* Mode; };
		struct FCoh { const TCHAR* Name; TArray<FBagItem> Bag; int32 Min; };
		const TArray<FCoh> Cases = {
			{ TEXT("carro"), { { TEXT("Plank"), 1, nullptr }, { TEXT("Wheel"), 4, nullptr } }, 97 },
			{ TEXT("carro con 25 llantas"), { { TEXT("Plank"), 1, nullptr }, { TEXT("Wheel"), 25, nullptr } }, 97 },
			{ TEXT("carro con seis ruedas y dos lanzallamas"), { { TEXT("Plank"), 1, nullptr }, { TEXT("Wheel"), 6, nullptr }, { TEXT("Flamethrower"), 2, nullptr } }, 97 },
			{ TEXT("carro con cohete"), { { TEXT("Plank"), 1, nullptr }, { TEXT("Wheel"), 4, nullptr }, { TEXT("Rocket"), 1, nullptr } }, 97 },
			{ TEXT("carro con dos cohetes y un canon"), { { TEXT("Plank"), 1, nullptr }, { TEXT("Wheel"), 4, nullptr }, { TEXT("Rocket"), 2, nullptr }, { TEXT("Cannon"), 1, nullptr } }, 97 },
			{ TEXT("tanque"), { { TEXT("Plank"), 1, nullptr }, { TEXT("BigWheel"), 4, nullptr }, { TEXT("Cannon"), 1, nullptr } }, 97 },
			{ TEXT("tanque con 4 canones"), { { TEXT("Plank"), 1, nullptr }, { TEXT("BigWheel"), 4, nullptr }, { TEXT("Cannon"), 4, nullptr } }, 97 },
			{ TEXT("tanque con escudo y canon"), { { TEXT("Plank"), 1, nullptr }, { TEXT("BigWheel"), 4, nullptr }, { TEXT("Cannon"), 1, nullptr }, { TEXT("Panel"), 1, nullptr } }, 97 },
			{ TEXT("carro con un globo"), { { TEXT("Plank"), 1, nullptr }, { TEXT("Wheel"), 4, nullptr }, { TEXT("Balloon"), 1, nullptr } }, 94 },
			{ TEXT("dirigible"), { { TEXT("Plank"), 1, nullptr }, { TEXT("Balloon"), 2, nullptr }, { TEXT("Flamethrower"), 1, nullptr }, { TEXT("Fan"), 1, nullptr } }, 95 },
			{ TEXT("dron"), { { TEXT("Plank"), 1, nullptr }, { TEXT("Fan"), 2, TEXT("lift") }, { TEXT("Stabilizer"), 1, nullptr } }, 94 },
			{ TEXT("cuadricoptero"), { { TEXT("Plank"), 1, nullptr }, { TEXT("Fan"), 4, TEXT("lift") }, { TEXT("Stabilizer"), 1, nullptr } }, 95 },
			{ TEXT("avion"), { { TEXT("Plank"), 1, nullptr }, { TEXT("Wing"), 2, nullptr }, { TEXT("Rocket"), 1, nullptr }, { TEXT("Stabilizer"), 1, nullptr } }, 97 },
			{ TEXT("cohete"), { { TEXT("Cylinder"), 1, nullptr }, { TEXT("Rocket"), 2, TEXT("lift") }, { TEXT("Stabilizer"), 1, nullptr } }, 95 },
			{ TEXT("cohete con 3 propulsores"), { { TEXT("Cylinder"), 1, nullptr }, { TEXT("Rocket"), 3, TEXT("lift") }, { TEXT("Stabilizer"), 1, nullptr } }, 95 },
			{ TEXT("moto"), { { TEXT("Plank"), 1, nullptr }, { TEXT("BigWheel"), 2, TEXT("inline") }, { TEXT("Stabilizer"), 1, nullptr } }, 97 },
			{ TEXT("trineo con ventilador"), { { TEXT("Sled"), 1, nullptr }, { TEXT("Fan"), 1, nullptr } }, 97 },
			{ TEXT("hovercraft con vela y hielo"), { { TEXT("Plank"), 1, nullptr }, { TEXT("HoverStone"), 2, nullptr }, { TEXT("Stabilizer"), 1, nullptr }, { TEXT("Sail"), 1, nullptr }, { TEXT("Frost"), 1, nullptr } }, 95 },
			{ TEXT("torreta con laser"), { { TEXT("Block"), 1, nullptr }, { TEXT("Cannon"), 1, nullptr }, { TEXT("Battery"), 1, nullptr }, { TEXT("Beam"), 1, nullptr } }, 97 },
			{ TEXT("torreta con cabeza"), { { TEXT("Block"), 1, nullptr }, { TEXT("Head"), 1, nullptr }, { TEXT("Cannon"), 1, nullptr }, { TEXT("Battery"), 1, nullptr } }, 97 },
			{ TEXT("aspersor de fuego"), { { TEXT("Block"), 1, nullptr }, { TEXT("Turntable"), 1, nullptr }, { TEXT("Flamethrower"), 2, nullptr } }, 97 },
			{ TEXT("catapulta"), { { TEXT("Plank"), 1, nullptr }, { TEXT("Block"), 1, nullptr }, { TEXT("Spring"), 1, TEXT("launch") }, { TEXT("Wheel"), 2, nullptr }, { TEXT("Girder"), 1, nullptr }, { TEXT("Ball"), 1, nullptr } }, 89 },
			{ TEXT("puente con 10 tablas"), { { TEXT("Plank"), 10, nullptr } }, 97 },
			{ TEXT("torre de 12 bloques"), { { TEXT("Block"), 12, nullptr } }, 93 },
			{ TEXT("fortaleza con dos canones"), { { TEXT("Wall"), 3, nullptr }, { TEXT("Block"), 1, nullptr }, { TEXT("Cannon"), 2, nullptr } }, 97 },
			{ TEXT("rueda sola con fuego"), { { TEXT("BigWheel"), 1, nullptr }, { TEXT("Flamethrower"), 2, nullptr } }, 97 },
			{ TEXT("carro con 8 ruedas, alas y turbina"), { { TEXT("Plank"), 1, nullptr }, { TEXT("Wheel"), 8, nullptr }, { TEXT("Wing"), 2, nullptr }, { TEXT("Turbine"), 1, nullptr } }, 97 },
			{ TEXT("carrito loco"), { { TEXT("Plank"), 1, nullptr }, { TEXT("Wheel"), 4, nullptr }, { TEXT("Flamethrower"), 1, nullptr }, { TEXT("Balloon"), 1, nullptr }, { TEXT("Rocket"), 1, nullptr }, { TEXT("Light"), 1, nullptr }, { TEXT("Battery"), 1, nullptr } }, 97 },
			{ TEXT("carro con 3 ruedas"), { { TEXT("Plank"), 1, nullptr }, { TEXT("Wheel"), 3, nullptr } }, 97 },
			{ TEXT("camion de guerra"), { { TEXT("Plank"), 1, nullptr }, { TEXT("Wheel"), 10, nullptr }, { TEXT("Cannon"), 2, nullptr }, { TEXT("Flamethrower"), 2, nullptr }, { TEXT("Rocket"), 2, nullptr }, { TEXT("Panel"), 1, nullptr }, { TEXT("BigBattery"), 1, nullptr }, { TEXT("Bomb"), 1, nullptr } }, 97 },
		};
		int32 Sum = 0, Pass = 0;
		for (const FCoh& Cs : Cases)
		{
			TArray<FJamPieceCount> Bag;
			for (const FBagItem& B : Cs.Bag)
			{
				FJamPieceCount I;
				I.Type = B.Type; I.Count = B.Count; I.Mode = B.Mode ? FName(B.Mode) : NAME_None;
				Bag.Add(I);
			}
			FBuildRecipe R;
			FString Note;
			TestTrue(*FString::Printf(TEXT("sentido '%s': compone (%s)"), Cs.Name, *Note), UJamBuildGenerator::ComposeFromBag(Bag, R, Note));
			const FJamCoherence C = UJamAssembler::Coherence(R);
			Sum += C.Score;
			Pass += C.Score >= 85 ? 1 : 0;
			TestTrue(*FString::Printf(TEXT("sentido '%s': %s >= %d sin fallo grave [%s]"), Cs.Name, *C.ToText(), Cs.Min, *Note), C.Score >= Cs.Min && C.Grave.Num() == 0);
			TestFalse(*FString::Printf(TEXT("sentido '%s': nada sin colocar [%s]"), Cs.Name, *Note), Note.Contains(TEXT("skipped=")));
			UE_LOG(LogTemp, Display, TEXT("JamSentido: %s -> %d piezas, %s"), Cs.Name, R.Parts.Num(), *Note);
		}
		UE_LOG(LogTemp, Display, TEXT("JamSentido: media %.1f, pasan (>=85) %d de %d"), (float)Sum / Cases.Num(), Pass, Cases.Num());

		auto Pt = [](const TCHAR* Id, const TCHAR* Type, const TCHAR* Parent = nullptr, const TCHAR* Socket = nullptr)
		{
			FBuildRecipePart P;
			P.Id = Id; P.Type = Type; P.AttachTo = Parent ? FName(Parent) : NAME_None; P.Socket = Socket ? FName(Socket) : NAME_None;
			return P;
		};
		struct FNeg { const TCHAR* Name; TArray<FBuildRecipePart> Parts; };
		const TArray<FNeg> Bad = {
			{ TEXT("N1 canon mirando atras"), { Pt(TEXT("body"), TEXT("Plank")), Pt(TEXT("w0"), TEXT("Wheel"), TEXT("body"), TEXT("Left_Front")), Pt(TEXT("w1"), TEXT("Wheel"), TEXT("body"), TEXT("Right_Front")), Pt(TEXT("w2"), TEXT("Wheel"), TEXT("body"), TEXT("Left_Back")), Pt(TEXT("w3"), TEXT("Wheel"), TEXT("body"), TEXT("Right_Back")), Pt(TEXT("seat"), TEXT("Seat"), TEXT("body"), TEXT("Top")), Pt(TEXT("gun"), TEXT("Cannon"), TEXT("body"), TEXT("Top_Front_Back")) } },
			{ TEXT("N2 canon detras del asiento"), { Pt(TEXT("body"), TEXT("Plank")), Pt(TEXT("w0"), TEXT("Wheel"), TEXT("body"), TEXT("Left_Front")), Pt(TEXT("w1"), TEXT("Wheel"), TEXT("body"), TEXT("Right_Front")), Pt(TEXT("w2"), TEXT("Wheel"), TEXT("body"), TEXT("Left_Back")), Pt(TEXT("w3"), TEXT("Wheel"), TEXT("body"), TEXT("Right_Back")), Pt(TEXT("seat"), TEXT("Seat"), TEXT("body"), TEXT("Top")), Pt(TEXT("gun"), TEXT("Cannon"), TEXT("body"), TEXT("Top_Back_Front")) } },
			{ TEXT("N3 cohete con la tobera al frente"), { Pt(TEXT("body"), TEXT("Plank")), Pt(TEXT("w0"), TEXT("Wheel"), TEXT("body"), TEXT("Left_Front")), Pt(TEXT("w1"), TEXT("Wheel"), TEXT("body"), TEXT("Right_Front")), Pt(TEXT("w2"), TEXT("Wheel"), TEXT("body"), TEXT("Left_Back")), Pt(TEXT("w3"), TEXT("Wheel"), TEXT("body"), TEXT("Right_Back")), Pt(TEXT("seat"), TEXT("Seat"), TEXT("body"), TEXT("Top")), Pt(TEXT("r"), TEXT("Rocket"), TEXT("body"), TEXT("Front")) } },
			{ TEXT("N4 ruedas arriba"), { Pt(TEXT("body"), TEXT("Plank")), Pt(TEXT("w0"), TEXT("Wheel"), TEXT("body"), TEXT("Top_Front")), Pt(TEXT("w1"), TEXT("Wheel"), TEXT("body"), TEXT("Top_Back")), Pt(TEXT("seat"), TEXT("Seat"), TEXT("body"), TEXT("Top")) } },
			{ TEXT("N5 ruedas de un solo lado"), { Pt(TEXT("body"), TEXT("Plank")), Pt(TEXT("w0"), TEXT("Wheel"), TEXT("body"), TEXT("Left_Front")), Pt(TEXT("w1"), TEXT("Wheel"), TEXT("body"), TEXT("Left_Back")), Pt(TEXT("seat"), TEXT("Seat"), TEXT("body"), TEXT("Top")) } },
			{ TEXT("N6 globo debajo y helice hacia el suelo"), { Pt(TEXT("body"), TEXT("Plank")), Pt(TEXT("b"), TEXT("Balloon"), TEXT("body"), TEXT("Bottom")), Pt(TEXT("f"), TEXT("Fan"), TEXT("body"), TEXT("Top_Center_Bottom")), Pt(TEXT("seat"), TEXT("Seat"), TEXT("body"), TEXT("Top_Back")) } },
			{ TEXT("N7 todo el peso atras"), { Pt(TEXT("body"), TEXT("Plank")), Pt(TEXT("w0"), TEXT("Wheel"), TEXT("body"), TEXT("Left_Front")), Pt(TEXT("w1"), TEXT("Wheel"), TEXT("body"), TEXT("Right_Front")), Pt(TEXT("w2"), TEXT("Wheel"), TEXT("body"), TEXT("Left_Back")), Pt(TEXT("w3"), TEXT("Wheel"), TEXT("body"), TEXT("Right_Back")), Pt(TEXT("seat"), TEXT("Seat"), TEXT("body"), TEXT("Top")), Pt(TEXT("p"), TEXT("Plank"), TEXT("body"), TEXT("Back")), Pt(TEXT("p2"), TEXT("Plank"), TEXT("p"), TEXT("Back")), Pt(TEXT("k0"), TEXT("Block"), TEXT("p2"), TEXT("Top")), Pt(TEXT("k1"), TEXT("Block"), TEXT("k0"), TEXT("Top")), Pt(TEXT("k2"), TEXT("Block"), TEXT("p2"), TEXT("Top_Back")) } },
			{ TEXT("N8 dos piezas encimadas"), { Pt(TEXT("body"), TEXT("Plank")), Pt(TEXT("w0"), TEXT("Wheel"), TEXT("body"), TEXT("Left_Front")), Pt(TEXT("w1"), TEXT("Wheel"), TEXT("body"), TEXT("Right_Front")), Pt(TEXT("w2"), TEXT("Wheel"), TEXT("body"), TEXT("Left_Back")), Pt(TEXT("w3"), TEXT("Wheel"), TEXT("body"), TEXT("Right_Back")), Pt(TEXT("seat"), TEXT("Seat"), TEXT("body"), TEXT("Top")), Pt(TEXT("bat"), TEXT("BigBattery"), TEXT("body"), TEXT("Top")) } },
			{ TEXT("N9 dron con una sola helice en la punta"), { Pt(TEXT("body"), TEXT("Plank")), Pt(TEXT("f"), TEXT("Fan"), TEXT("body"), TEXT("Bottom_Front")), Pt(TEXT("seat"), TEXT("Seat"), TEXT("body"), TEXT("Top")) } },
			{ TEXT("N10 lanzallamas contra la bomba y turbina arrastrando"), { Pt(TEXT("body"), TEXT("Plank")), Pt(TEXT("w0"), TEXT("Wheel"), TEXT("body"), TEXT("Left_Front")), Pt(TEXT("w1"), TEXT("Wheel"), TEXT("body"), TEXT("Right_Front")), Pt(TEXT("w2"), TEXT("Wheel"), TEXT("body"), TEXT("Left_Back")), Pt(TEXT("w3"), TEXT("Wheel"), TEXT("body"), TEXT("Right_Back")), Pt(TEXT("seat"), TEXT("Seat"), TEXT("body"), TEXT("Top_Back")), Pt(TEXT("fl"), TEXT("Flamethrower"), TEXT("body"), TEXT("Top_Center_Front")), Pt(TEXT("bomb"), TEXT("Bomb"), TEXT("body"), TEXT("Top_Front")), Pt(TEXT("t"), TEXT("Turbine"), TEXT("body"), TEXT("Back")) } },
		};
		for (const FNeg& Ng : Bad)
		{
			FBuildRecipe R;
			R.Parts = Ng.Parts;
			const FJamCoherence Before = UJamAssembler::Coherence(R);
			TestTrue(*FString::Printf(TEXT("mala '%s': el validador la marca (%s)"), Ng.Name, *Before.ToText()), Before.Score < 70);
			FString What;
			const FJamCoherence After = UJamBuildGenerator::Repair(R, What);
			TestTrue(*FString::Printf(TEXT("mala '%s': reparada (%s) [%s]"), Ng.Name, *After.ToText(), *What), After.Score >= 85);
			UE_LOG(LogTemp, Display, TEXT("JamSentido: mala %s: %d -> %d (%s)"), Ng.Name, Before.Score, After.Score, *What);
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
		// Colocacion con sentido: el bloque "place" del JSON es el mismo que la regla de PieceTable.
		const TSharedPtr<FJsonObject>* Place = nullptr;
		const FJamPieceRule* Rule = UJamBuildGenerator::PieceTable().FindByPredicate([Type](const FJamPieceRule& R) { return R.Type == Type; });
		if (Rule && Rule->Faces.Num() > 0)
		{
			if (!(*O)->TryGetObjectField(TEXT("place"), Place))
			{
				AddError(FString::Printf(TEXT("%s: falta el bloque place en BuildKnowledge.json"), *Type.ToString()));
				continue;
			}
			TArray<FString> Faces;
			(*Place)->TryGetStringArrayField(TEXT("faces"), Faces);
			TestEqual(*FString::Printf(TEXT("%s: place.faces"), *Type.ToString()), FString::Join(Faces, TEXT("|")), FString::Join(Rule->Faces, TEXT("|")));
			TestEqual(*FString::Printf(TEXT("%s: place.sym"), *Type.ToString()), (*Place)->GetStringField(TEXT("sym")), Rule->Sym);
			TestEqual(*FString::Printf(TEXT("%s: place.order"), *Type.ToString()), (*Place)->GetStringField(TEXT("order")), Rule->Order);
			double Clear = 0, UpCm = 0;
			(*Place)->TryGetNumberField(TEXT("clear"), Clear);
			(*Place)->TryGetNumberField(TEXT("up"), UpCm);
			TestEqual(*FString::Printf(TEXT("%s: place.clear"), *Type.ToString()), (float)Clear, Rule->ClearCm);
			TestEqual(*FString::Printf(TEXT("%s: place.up"), *Type.ToString()), (float)UpCm, Rule->UpCm);
		}
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
