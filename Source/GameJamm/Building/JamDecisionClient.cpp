#include "Building/JamDecisionClient.h"
#include "Building/JamConnectionRules.h"

#include "Building/JamBuildPart.h"
#include "Combat/JamGunSpec.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogJamDecision, Log, All);

namespace
{
	const TCHAR* Endpoint = TEXT("https://openrouter.ai/api/alpha/decisions");
	const TCHAR* ModelId = TEXT("typesafe/jev-1.13"); // fijado: los umbrales cambian entre versiones.

	FString ExtrasStr(const FJamBuildChoice& C)
	{
		TArray<FString> S;
		for (const FName& N : C.Extras) { S.Add(N.ToString()); }
		return FString::Join(S, TEXT(","));
	}

	TSharedRef<FJsonObject> Obj() { return MakeShared<FJsonObject>(); }

	// Decisions API de OpenAI (contrato verificado con llamadas reales el 15 nov): input [developer, user] + questions[]
	// (predicate / choice) -> answers[] con predicate.probability, choice.choice/.confidence o refusal POR PREGUNTA.
	const TCHAR* OpenAIEndpoint = TEXT("https://api.openai.com/v1/decisions");
	const TCHAR* OpenAIModel = TEXT("gpt-6-luna");
	const TCHAR* OpenAIBackend = TEXT("openai-decisions");
	// El mensaje developer deja claro que es un juego de piezas de juguete con efectos de juego: sin esto el modelo se niega
	// (refusal) a preguntas como la del lanzallamas. Cada pregunta repite "In this toy game" / "toy part".
	const TCHAR* OpenAIDeveloper = TEXT("You are the decision layer of a family-friendly toy construction video game (a physics sandbox). The player types, in Spanish, ")
		TEXT("what to build out of toy parts: boards, wheels, fans, balloons and toy gadgets. Everything is make-believe game content: fire, ice, ")
		TEXT("lightning, bombs and cannons are harmless cartoon game effects of toy parts; nothing is real and nobody can be hurt. Answer every ")
		TEXT("question about which toy parts the toy build should use. The game code assembles and validates the build; you only choose.");
	const TCHAR* OpenAIUser = TEXT("Player request (toy build): ");

	/** Clave de un proveedor: variable de entorno o [Jam] <IniKey> de Config/DefaultSecrets.ini (ignorado por git; en el paquete, <exe>/GameJamm/Config). */
	FString JamSecret(const TCHAR* EnvName, const TCHAR* IniKey)
	{
		FString Key = FPlatformMisc::GetEnvironmentVariable(EnvName);
		if (!Key.IsEmpty())
		{
			return Key;
		}
		FConfigFile Secrets;
		Secrets.Read(FPaths::ProjectConfigDir() / TEXT("DefaultSecrets.ini"));
		Secrets.GetString(TEXT("Jam"), IniKey, Key);
		return Key;
	}

	/**
	 * POST a la Decisions API de OpenAI con el corte del proveedor. Done(b200, codigo HTTP (0 = sin respuesta / corte), contenido, ms)
	 * en el hilo del juego. Nunca escribe en el log la clave ni el cuerpo de un error.
	 */
	void JamPostOpenAI(const FString& Key, const FString& Body, TFunction<void(bool, int32, const FString&, int32)> Done)
	{
		const double T0 = FPlatformTime::Seconds();
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
		Req->SetURL(OpenAIEndpoint);
		Req->SetVerb(TEXT("POST"));
		Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
		Req->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *Key));
		Req->SetContentAsString(Body);
		Req->SetTimeout(FJamDecisionClient::GetTimeout(TEXT("openai"), 3.f));
		Req->OnProcessRequestComplete().BindLambda([Done, T0](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
		{
			const int32 Ms = int32((FPlatformTime::Seconds() - T0) * 1000.0 + 0.5);
			const int32 Code = Resp.IsValid() ? Resp->GetResponseCode() : 0;
			const bool b200 = bOk && Code == 200;
			Done(b200, Code, b200 ? Resp->GetContentAsString() : FString(), Ms);
		});
		Req->ProcessRequest();
	}
}

FString FJamDecisionClient::GetApiKey()
{
	return JamSecret(TEXT("OPENROUTER_API_KEY"), TEXT("OpenRouterApiKey"));
}

FString FJamDecisionClient::GetTypeSafeKey()
{
	return JamSecret(TEXT("TYPESAFE_API_KEY"), TEXT("DecisionsApiKey"));
}

FString FJamDecisionClient::GetFalKey()
{
	return JamSecret(TEXT("FAL_KEY"), TEXT("FalApiKey"));
}

FString FJamDecisionClient::GetOpenAIKey()
{
	return JamSecret(TEXT("OPENAI_API_KEY"), TEXT("OpenAIApiKey")).TrimStartAndEnd();
}

float FJamDecisionClient::GetTimeout(const TCHAR* Provider, float Default)
{
	// Cortes por proveedor como dato: BuildKnowledge.json, bloque "decision_timeouts" (segundos). Se lee una vez; si falta, el valor del codigo.
	static TSharedPtr<FJsonObject> Timeouts;
	static bool bLoaded = false;
	if (!bLoaded)
	{
		bLoaded = true;
		FString Json;
		TSharedPtr<FJsonObject> Root;
		const TSharedPtr<FJsonObject>* Block = nullptr;
		if (FFileHelper::LoadFileToString(Json, *(FPaths::ProjectContentDir() / TEXT("Game/Building/Data/BuildKnowledge.json")))
			&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) && Root.IsValid()
			&& Root->TryGetObjectField(TEXT("decision_timeouts"), Block) && Block && Block->IsValid())
		{
			Timeouts = *Block;
		}
	}
	double Seconds = Default;
	if (Timeouts.IsValid()) { Timeouts->TryGetNumberField(FString(Provider), Seconds); }
	return float(FMath::Clamp(Seconds, 0.5, 60.0));
}

bool FJamDecisionClient::ParseFalResponse(const FString& ResponseJson, FJamBuildChoice& OutChoice, FString& OutNote)
{
	TSharedPtr<FJsonObject> Root;
	FString Text;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ResponseJson), Root) || !Root.IsValid() || !Root->TryGetStringField(TEXT("output"), Text))
	{
		OutNote = TEXT("fal: no 'output'");
		return false;
	}
	// Quita ```json ... ``` y toma de la primera { a la ultima }.
	int32 A = INDEX_NONE, B = INDEX_NONE;
	Text.FindChar(TEXT('{'), A);
	Text.FindLastChar(TEXT('}'), B);
	TSharedPtr<FJsonObject> D;
	if (A == INDEX_NONE || B <= A || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text.Mid(A, B - A + 1)), D) || !D.IsValid())
	{
		OutNote = TEXT("fal: output is not JSON");
		return false;
	}
	// Formato del planificador: {"root": "Block", "pieces": [{"type": "Wheel", "count": 4, "mode": "lift"}, ...]}.
	// Validacion: solo tipos del catalogo (sin distinguir mayusculas), cantidades 1..40, modos conocidos. Lo demas se descarta.
	const TArray<TSharedPtr<FJsonValue>>* Pieces = nullptr;
	if (D->TryGetArrayField(TEXT("pieces"), Pieces) && Pieces)
	{
		OutChoice = FJamBuildChoice();
		OutChoice.Archetype = "Pieces";
		auto Canon = [](const FString& Name) -> FName
		{
			for (const FName& T : UJamPartCatalog::GetTypes()) { if (T.ToString().Equals(Name, ESearchCase::IgnoreCase)) { return T; } }
			return NAME_None;
		};
		FString RootStr;
		D->TryGetStringField(TEXT("root"), RootStr);
		OutChoice.Root = Canon(RootStr);
		TArray<FString> Txt;
		int32 Dropped = 0;
		for (const TSharedPtr<FJsonValue>& V : *Pieces)
		{
			const TSharedPtr<FJsonObject>* O = nullptr;
			FString TypeStr, ModeStr;
			if (!V.IsValid() || !V->TryGetObject(O) || !O || !(*O)->TryGetStringField(TEXT("type"), TypeStr)) { ++Dropped; continue; }
			const FName Type = Canon(TypeStr);
			if (Type.IsNone()) { ++Dropped; continue; }
			double Count = 1;
			(*O)->TryGetNumberField(TEXT("count"), Count);
			(*O)->TryGetStringField(TEXT("mode"), ModeStr);
			FJamPieceCount* I = OutChoice.Pieces.FindByPredicate([Type](const FJamPieceCount& X) { return X.Type == Type; });
			if (!I) { I = &OutChoice.Pieces.AddDefaulted_GetRef(); I->Type = Type; I->Count = 0; }
			I->Count = FMath::Clamp(I->Count + (int32)Count, 1, 40);
			const FJamPieceRule* Rule = UJamBuildGenerator::PieceTable().FindByPredicate([Type](const FJamPieceRule& R) { return R.Type == Type; });
			if (Rule && Rule->Modes.Contains(FName(*ModeStr))) { I->Mode = FName(*ModeStr); }
		}
		for (const FJamPieceCount& I : OutChoice.Pieces) { Txt.Add(FString::Printf(TEXT("%s x%d"), *I.Type.ToString(), I.Count)); }
		OutNote = FString::Printf(TEXT("fal-plan pieces=%d dropped=%d root=%s [%s]"), OutChoice.Pieces.Num(), Dropped, *OutChoice.Root.ToString(), *FString::Join(Txt, TEXT(", ")));
		return OutChoice.Pieces.Num() > 0;
	}
	FString Arch;
	if (!D->TryGetStringField(TEXT("archetype"), Arch))
	{
		OutNote = TEXT("fal: archetype missing");
		return false;
	}
	OutChoice = FJamBuildChoice();
	OutChoice.Archetype = FName(*Arch);
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (D->TryGetArrayField(TEXT("extras"), Arr) && Arr)
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const FName Key(*V->AsString());
			for (const FJamExtraDef& E : UJamBuildGenerator::ExtraTable())
			{
				if (E.Key == Key) { OutChoice.Extras.AddUnique(Key); }
			}
		}
	}
	OutNote = FString::Printf(TEXT("fal-llm archetype=%s"), *Arch);
	return true;
}

FString FJamDecisionClient::BuildConnectionGuide()
{
	FString Guide = TEXT("Axes: X forward, Y right, Z up. Code places and validates all connections. Choose parts, counts and supported modes; do not invent coordinates or forces. Fans/rockets in default mode propel forward; lift mode provides upward thrust. Lift needs at least 1.2 times total weight, balanced around center of mass. Stabilizer permits controlled tilt in flight. Power units: fan/rocket/balloon force in kg*cm/s^2, wheel/turntable angular speed in rad/s, hinge limit in degrees; other device power is capability-specific. A mode applies to all pieces of that type. Propeller defaults to lift and Launcher to launch.\n");
	for (const FJamPieceRule& R : UJamBuildGenerator::PieceTable())
	{
		FJamPartDef D;
		if (!UJamPartCatalog::Find(R.Type, D)) { continue; }
		TArray<FString> Modes = { TEXT("default") };
		for (const auto& M : R.Modes) { Modes.Add(M.Key.ToString()); }
		Modes.Sort();
		Guide += FString::Printf(TEXT("%s: %s; mass=%.1f kg; power=%.1f; modes=%s; candidate faces=%s; clearance=%.0f cm; symmetry=%s\n"),
			*R.Type.ToString(), *JamConnections::Describe(D), D.MassKg, D.Power, *FString::Join(Modes, TEXT(",")), *FString::Join(R.Faces, TEXT(",")), R.ClearCm, *R.Sym);
	}
	return Guide;
}

FString FJamDecisionClient::BuildRequestBody(const FString& Prompt, const TCHAR* ModelOverride)
{
	TSharedRef<FJsonObject> Root = Obj();
	Root->SetStringField(TEXT("model"), ModelOverride ? ModelOverride : ModelId);

	TSharedRef<FJsonObject> State = Obj();
	State->SetStringField(TEXT("request"), Prompt);
	State->SetStringField(TEXT("connection_rules"), BuildConnectionGuide());
	Root->SetObjectField(TEXT("state"), State);

	TSharedRef<FJsonObject> Questions = Obj();

	// El modelo NO elige una plantilla: decide pieza por pieza si hace falta, cuantas y en que modo.
	// (Orden fijo = orden de PieceTable: las probabilidades de Jev dependen del orden.)
	for (const FJamPieceRule& R : UJamBuildGenerator::PieceTable())
	{
		TSharedRef<FJsonObject> Q = Obj();
		Q->SetStringField(TEXT("type"), TEXT("noul"));
		Q->SetStringField(TEXT("instructions"), TEXT("Decide whether this building piece is needed to build what the user asks for."));
		TSharedRef<FJsonObject> Crit = Obj();
		Crit->SetStringField(TEXT("true"), R.Ask);
		Crit->SetStringField(TEXT("false"), R.NotAsk);
		Q->SetObjectField(TEXT("criteria"), Crit);
		Questions->SetObjectField(TEXT("use_") + R.Type.ToString(), Q);
		// Sin preguntas n_<Tipo>: medido en 300 frases, 334 de 370 respuestas eran "1". La cantidad sale del texto (numerales por codigo).
	}
	auto Flag = [&Questions](const TCHAR* Key, const TCHAR* True, const TCHAR* False)
	{
		TSharedRef<FJsonObject> Q = Obj();
		Q->SetStringField(TEXT("type"), TEXT("noul"));
		Q->SetStringField(TEXT("instructions"), TEXT("Decide how the thing the user asks for must behave."));
		TSharedRef<FJsonObject> Crit = Obj();
		Crit->SetStringField(TEXT("true"), True);
		Crit->SetStringField(TEXT("false"), False);
		Q->SetObjectField(TEXT("criteria"), Crit);
		Questions->SetObjectField(Key, Q);
	};
	Flag(TEXT("mode_lift"), TEXT("It must rise into the air pushed from below: drone, helicopter, quadcopter, flying insect, UFO, jetpack, rocket ship, missile, elevator, anything described only as 'something that flies' or 'reach higher'."),
		TEXT("It stays on the ground or water, or it flies with wings or balloons, or its propellers and rockets push it forward."));
	Flag(TEXT("mode_launch"), TEXT("It must throw or launch things or people through the air: catapult, launcher, trampoline, trebuchet, slingshot, a trap that sends whoever steps on it flying."),
		TEXT("It does not throw things; a spring would only make the thing itself jump (frog, kangaroo, pogo)."));
	// Globales (van al final para no mover las probabilidades de las anteriores): forma general y pieza raiz.
	auto Choice = [&Questions](const TCHAR* Key, const TCHAR* Instr, std::initializer_list<TPair<const TCHAR*, const TCHAR*>> Opts)
	{
		TSharedRef<FJsonObject> Q = Obj();
		Q->SetStringField(TEXT("type"), TEXT("choice"));
		Q->SetStringField(TEXT("instructions"), Instr);
		TSharedRef<FJsonObject> Crit = Obj();
		for (const TPair<const TCHAR*, const TCHAR*>& O : Opts) { Crit->SetStringField(O.Key, O.Value); }
		Q->SetObjectField(TEXT("criteria"), Crit);
		Questions->SetObjectField(Key, Q);
	};
	Choice(TEXT("pattern"), TEXT("Overall shape of the thing the user describes."), {
		{ TEXT("vehicle"), TEXT("A body that moves on the ground on wheels or runners.") },
		{ TEXT("flyer"), TEXT("A body that flies or floats in the air.") },
		{ TEXT("boat"), TEXT("A hull that sails or floats on water.") },
		{ TEXT("row"), TEXT("Parts laid one after another in a long horizontal line: bridge, road, fence, train.") },
		{ TEXT("stack"), TEXT("Parts piled one on top of another going up: tower, column, pile.") },
		{ TEXT("arm"), TEXT("A base with a long arm that swings, throws, lifts or hits: catapult, crane, hammer.") },
		{ TEXT("emplacement"), TEXT("A fixed base with weapons or devices on it: turret, trap, sprinkler, fort.") },
		{ TEXT("single"), TEXT("One main part with a few things stuck on it: a lone wheel, a chair, a ball.") } });
	Choice(TEXT("root"), TEXT("Which part is the main body that everything else is attached to?"), {
		{ TEXT("Plank"), TEXT("A long flat board: the floor or chassis of a vehicle, aircraft or bridge.") },
		{ TEXT("Block"), TEXT("A solid cube: the base of a turret, tower or machine.") },
		{ TEXT("Cylinder"), TEXT("A tube or post: a rocket body or a column.") },
		{ TEXT("Sled"), TEXT("A sliding hull: a boat or sled.") },
		{ TEXT("Wall"), TEXT("A thick wall: a fort, bunker or barricade.") },
		{ TEXT("Wheel"), TEXT("A single wheel that rolls or spins by itself with things stuck on it.") } });
	// Modos por tipo permiten mezclar sustentacion y avance. Al final preserva el orden anterior.
	for (const FJamPieceRule& R : UJamBuildGenerator::PieceTable())
	{
		if (R.Modes.IsEmpty()) { continue; }
		TSharedRef<FJsonObject> Q = Obj(), Criteria = Obj();
		Q->SetStringField(TEXT("type"), TEXT("choice"));
		Q->SetStringField(TEXT("instructions"), TEXT("Choose the role of ") + R.Type.ToString() + TEXT(" in this build using connection_rules. Choose default unless a special mode is needed."));
		Criteria->SetStringField(TEXT("default"), TEXT("Normal placement; propulsion forward for fans/rockets; ordinary wheel support."));
		TArray<FName> Modes;
		R.Modes.GetKeys(Modes);
		Modes.Sort(FNameLexicalLess());
		for (const FName Mode : Modes)
		{
			Criteria->SetStringField(Mode.ToString(), Mode == "lift" ? TEXT("Provide upward thrust to rise or hover") : Mode == "launch" ? TEXT("Launch a payload from this spring") : TEXT("Wheels in a front/back line, such as a motorcycle"));
		}
		Q->SetObjectField(TEXT("criteria"), Criteria);
		Questions->SetObjectField(TEXT("mode_") + R.Type.ToString(), Q);
	}
	Root->SetObjectField(TEXT("questions"), Questions);

	FString Out;
	const TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root, W);
	return Out;
}

FString FJamDecisionClient::BuildOpenAIRequestBody(const FString& Prompt)
{
	// Las MISMAS preguntas que se le hacen a Jev (se leen de BuildRequestBody para que los dos backends nunca se separen),
	// traducidas al esquema de OpenAI: noul -> predicate (criterios true/false dentro de instructions), choice -> choice
	// (cada criterio pasa a ser una opcion value + description). Hoy son 59: 45 use_<Tipo>, mode_lift, mode_launch, pattern,
	// root y 10 mode_<Tipo>. No se manda la guia de conexiones: el modelo solo elige piezas y modos; el codigo las coloca.
	TArray<TSharedPtr<FJsonValue>> Out;
	TSharedPtr<FJsonObject> Jev;
	const TSharedPtr<FJsonObject>* Questions = nullptr;
	if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(BuildRequestBody(Prompt)), Jev) && Jev.IsValid()
		&& Jev->TryGetObjectField(TEXT("questions"), Questions) && Questions && Questions->IsValid())
	{
		for (const auto& KV : (*Questions)->Values)
		{
			const FString Name(KV.Key);
			const TSharedPtr<FJsonObject>* Q = nullptr;
			const TSharedPtr<FJsonObject>* Crit = nullptr;
			FString Type, Instr;
			if (!KV.Value.IsValid() || !KV.Value->TryGetObject(Q) || !Q || !Q->IsValid() || !(*Q)->TryGetStringField(TEXT("type"), Type)
				|| !(*Q)->TryGetObjectField(TEXT("criteria"), Crit) || !Crit || !Crit->IsValid())
			{
				continue;
			}
			(*Q)->TryGetStringField(TEXT("instructions"), Instr);
			Instr.ReplaceInline(TEXT(" using connection_rules"), TEXT(""));
			TSharedRef<FJsonObject> O = Obj();
			if (Type == TEXT("noul"))
			{
				FString True, False;
				(*Crit)->TryGetStringField(TEXT("true"), True);
				(*Crit)->TryGetStringField(TEXT("false"), False);
				const FString Head = Name.StartsWith(TEXT("use_"))
					? FString::Printf(TEXT("In this toy game, should the toy build include the toy part \"%s\"?"), *Name.RightChop(4))
					: FString(TEXT("In this toy game: ")) + Instr;
				O->SetStringField(TEXT("type"), TEXT("predicate"));
				O->SetStringField(TEXT("name"), Name);
				O->SetStringField(TEXT("instructions"), FString::Printf(TEXT("%s Yes if: %s No if: %s"), *Head, *True, *False));
			}
			else
			{
				TArray<TSharedPtr<FJsonValue>> Choices;
				for (const auto& C : (*Crit)->Values)
				{
					FString Desc;
					TSharedRef<FJsonObject> Opt = Obj();
					Opt->SetStringField(TEXT("value"), FString(C.Key));
					if (C.Value.IsValid() && C.Value->TryGetString(Desc)) { Opt->SetStringField(TEXT("description"), Desc); }
					Choices.Add(MakeShared<FJsonValueObject>(Opt));
				}
				O->SetStringField(TEXT("type"), TEXT("choice"));
				O->SetStringField(TEXT("name"), Name);
				O->SetStringField(TEXT("instructions"), FString(TEXT("In this toy game: ")) + Instr);
				O->SetArrayField(TEXT("choices"), Choices);
			}
			Out.Add(MakeShared<FJsonValueObject>(O));
		}
	}

	auto Message = [](const TCHAR* Role, const FString& Content) -> TSharedPtr<FJsonValue>
	{
		TSharedRef<FJsonObject> M = Obj();
		M->SetStringField(TEXT("role"), Role);
		M->SetStringField(TEXT("content"), Content);
		return MakeShared<FJsonValueObject>(M);
	};
	TArray<TSharedPtr<FJsonValue>> Input;
	Input.Add(Message(TEXT("developer"), OpenAIDeveloper));
	Input.Add(Message(TEXT("user"), FString(OpenAIUser) + Prompt));

	TSharedRef<FJsonObject> Root = Obj();
	Root->SetStringField(TEXT("model"), OpenAIModel);
	Root->SetArrayField(TEXT("input"), Input);
	Root->SetArrayField(TEXT("questions"), Out);
	FString Body;
	FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Body));
	return Body;
}

namespace
{
	/**
	 * Lectura comun de las respuestas "por piezas" (formato de Jev: Answers.<pregunta>.noul / .choice / .confidence):
	 * use_<Tipo>, n_<Tipo>, mode_lift / mode_launch, mode_<Tipo>, pattern y root. La usan Jev y OpenAI (que primero se pasa a este formato).
	 */
	bool JamParsePieceAnswers(const TSharedPtr<FJsonObject>* Answers, const TCHAR* Backend, FJamBuildChoice& OutChoice, FString& OutNote)
	{
		auto Noul = [&Answers](const FString& Key)
		{
			const TSharedPtr<FJsonObject>* A = nullptr;
			double P = 0.0;
			return (*Answers)->TryGetObjectField(Key, A) && A && (*A)->TryGetNumberField(TEXT("noul"), P) ? P : 0.0;
		};
		const bool bLift = Noul(TEXT("mode_lift")) > 0.5, bLaunch = Noul(TEXT("mode_launch")) > 0.5;
		TArray<FString> Txt;
		// Umbral relativo: si ninguna pieza pasa de 0.5, se toman las mejores (>= 0.6 x la mas alta, si la mas alta llega a 0.15; maximo 5).
		double Thr = 0.5, Best = 0.0;
		bool bAny = false;
		for (const FJamPieceRule& R : UJamBuildGenerator::PieceTable())
		{
			const double P = Noul(TEXT("use_") + R.Type.ToString());
			Best = FMath::Max(Best, P);
			bAny |= P > 0.5;
		}
		const bool bRelative = !bAny && Best >= 0.15;
		if (bRelative) { Thr = 0.6 * Best - 1e-6; }
		for (const FJamPieceRule& R : UJamBuildGenerator::PieceTable())
		{
			if (Noul(TEXT("use_") + R.Type.ToString()) <= Thr || (bRelative && OutChoice.Pieces.Num() >= 5)) { continue; }
			FJamPieceCount I;
			I.Type = R.Type;
			I.Count = R.DefaultCount;
			const TSharedPtr<FJsonObject>* N = nullptr;
			FString NumStr;
			double NumConf = 0.0;
			if ((*Answers)->TryGetObjectField(TEXT("n_") + R.Type.ToString(), N) && N && (*N)->TryGetStringField(TEXT("choice"), NumStr) && NumStr.IsNumeric()
				&& (*N)->TryGetNumberField(TEXT("confidence"), NumConf) && NumConf >= 0.8 && FCString::Atoi(*NumStr) >= 2)
			{
				I.Count = FCString::Atoi(*NumStr); // solo si el modelo esta seguro y dice 2 o mas ("1" lo contesta de mas); los numerales del texto mandan
			}
			if (R.LiftSlots.Num() > 0)
			{
				const bool bSpring = R.Type == "Spring" || R.Type == "Launcher";
				if (bSpring ? bLaunch : bLift) { I.Mode = bSpring ? FName("launch") : FName("lift"); }
				if (I.Mode == "lift" && R.Type == "Fan") { I.Count = FMath::Max(I.Count, 2); } // para sustentar hacen falta al menos dos helices
			}
			const TSharedPtr<FJsonObject>* ModeAnswer = nullptr;
			FString Mode;
			double Confidence = 0;
			if ((*Answers)->TryGetObjectField(TEXT("mode_") + R.Type.ToString(), ModeAnswer) && ModeAnswer
				&& (*ModeAnswer)->TryGetStringField(TEXT("choice"), Mode) && (*ModeAnswer)->TryGetNumberField(TEXT("confidence"), Confidence) && Confidence >= 0.6)
			{
				if (Mode == TEXT("default")) { I.Mode = NAME_None; }
				else if (R.Modes.Contains(FName(*Mode))) { I.Mode = FName(*Mode); }
			}
			OutChoice.Pieces.Add(I);
			Txt.Add(FString::Printf(TEXT("%s x%d"), *I.Type.ToString(), I.Count));
		}
		OutChoice.Archetype = "Pieces";
		// Raiz y forma: solo cuentan con confianza alta. La raiz "Wheel" la decide el codigo (una sola rueda sin chasis).
		auto Pick = [&Answers](const TCHAR* Key, double MinConf) -> FName
		{
			const TSharedPtr<FJsonObject>* A = nullptr;
			FString Opt;
			double Conf = 0.0;
			return ((*Answers)->TryGetObjectField(Key, A) && A && (*A)->TryGetStringField(TEXT("choice"), Opt) && (*A)->TryGetNumberField(TEXT("confidence"), Conf) && Conf >= MinConf) ? FName(*Opt) : NAME_None;
		};
		OutChoice.Pattern = Pick(TEXT("pattern"), 0.5);
		OutChoice.Root = Pick(TEXT("root"), 0.6);
		if (OutChoice.Root.IsNone() && (OutChoice.Pattern == "stack" || OutChoice.Pattern == "emplacement")) { OutChoice.Root = "Block"; }
		if (OutChoice.Root == "Wheel") { OutChoice.Root = NAME_None; }
		OutNote = FString::Printf(TEXT("%s pieces=%d conf=1.00 pattern=%s root=%s [%s]"), Backend, OutChoice.Pieces.Num(), *OutChoice.Pattern.ToString(), *OutChoice.Root.ToString(), *FString::Join(Txt, TEXT(", ")));
		return OutChoice.Pieces.Num() > 0;
	}
}

bool FJamDecisionClient::ParseOpenAIResponse(const FString& ResponseJson, const FString& Prompt, FJamBuildChoice& OutChoice, FString& OutNote)
{
	OutChoice = FJamBuildChoice();
	TSharedPtr<FJsonObject> Root;
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ResponseJson), Root) || !Root.IsValid())
	{
		OutNote = FString(OpenAIBackend) + TEXT(" response is not JSON");
		return false;
	}
	if (!Root->TryGetArrayField(TEXT("answers"), Arr) || !Arr)
	{
		OutNote = FString(OpenAIBackend) + TEXT(" response has no 'answers'");
		return false;
	}
	// answers[] de OpenAI -> formato de Jev (por nombre de pregunta), para leerlo EXACTAMENTE igual: predicate.probability -> noul,
	// choice.choice / .confidence igual. Una negativa ("refusal", o un tipo que no conocemos) deja esa pregunta sin respuesta.
	TSharedRef<FJsonObject> Answers = Obj();
	TArray<FString> Refused;
	int32 Answered = 0;
	for (const TSharedPtr<FJsonValue>& V : *Arr)
	{
		const TSharedPtr<FJsonObject>* A = nullptr;
		FString Kind, Name, Picked;
		if (!V.IsValid() || !V->TryGetObject(A) || !A || !A->IsValid() || !(*A)->TryGetStringField(TEXT("type"), Kind) || !(*A)->TryGetStringField(TEXT("name"), Name))
		{
			continue;
		}
		TSharedRef<FJsonObject> J = Obj();
		double P = 0.0;
		if (Kind == TEXT("predicate") && (*A)->TryGetNumberField(TEXT("probability"), P))
		{
			J->SetNumberField(TEXT("noul"), P);
		}
		else if (Kind == TEXT("choice") && (*A)->TryGetStringField(TEXT("choice"), Picked))
		{
			J->SetStringField(TEXT("choice"), Picked);
			if ((*A)->TryGetNumberField(TEXT("confidence"), P)) { J->SetNumberField(TEXT("confidence"), P); }
		}
		else
		{
			Refused.Add(Name);
			continue;
		}
		++Answered;
		Answers->SetObjectField(Name, J);
	}
	if (Answered == 0)
	{
		OutNote = FString::Printf(TEXT("%s all refused (%d)"), OpenAIBackend, Refused.Num());
		return false; // todo negado: entra el respaldo
	}
	// Negativa en un use_<Tipo>: nunca se descarta en silencio; para ESE tipo decide el respaldo por palabras clave.
	TArray<FString> ByKeyword;
	if (Refused.ContainsByPredicate([](const FString& N) { return N.StartsWith(TEXT("use_")); }))
	{
		TArray<FName> Negated;
		TArray<FJamPieceCount> Seed;
		UJamBuildGenerator::BagFromKeywords(UJamBuildGenerator::StripNegations(Prompt, Negated), Seed, false);
		for (const FString& N : Refused)
		{
			if (!N.StartsWith(TEXT("use_"))) { continue; }
			const FName PieceType(*N.RightChop(4));
			if (Seed.ContainsByPredicate([PieceType](const FJamPieceCount& I) { return I.Type == PieceType; }))
			{
				TSharedRef<FJsonObject> J = Obj();
				J->SetNumberField(TEXT("noul"), 1.0);
				Answers->SetObjectField(N, J);
				ByKeyword.Add(PieceType.ToString());
			}
		}
	}
	const TSharedPtr<FJsonObject> AnswersPtr = Answers;
	const bool bOk = JamParsePieceAnswers(&AnswersPtr, OpenAIBackend, OutChoice, OutNote);
	// refused = preguntas negadas; kw = tipos negados que puso el respaldo por palabras (el HUD no los muestra como elegidos por el modelo).
	if (Refused.Num() > 0) { OutNote += FString::Printf(TEXT(" refused=%d"), Refused.Num()); }
	if (ByKeyword.Num() > 0) { OutNote += TEXT(" kw=[") + FString::Join(ByKeyword, TEXT(",")) + TEXT("]"); }
	return bOk;
}

bool FJamDecisionClient::ParseResponse(const FString& ResponseJson, FJamBuildChoice& OutChoice, FString& OutNote)
{
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ResponseJson), Root) || !Root.IsValid())
	{
		OutNote = TEXT("response is not JSON");
		return false;
	}
	const TSharedPtr<FJsonObject>* Answers = nullptr;
	if (!Root->TryGetObjectField(TEXT("answers"), Answers) || !Answers)
	{
		FString Err;
		const TSharedPtr<FJsonObject>* ErrObj = nullptr;
		if (Root->TryGetObjectField(TEXT("error"), ErrObj) && ErrObj) { (*ErrObj)->TryGetStringField(TEXT("message"), Err); }
		OutNote = Err.IsEmpty() ? TEXT("response has no 'answers'") : Err;
		return false;
	}

	OutChoice = FJamBuildChoice();
	if ((*Answers)->HasField(TEXT("mode_lift")) || !(*Answers)->HasField(TEXT("archetype")))
	{
		// Formato por piezas: use_<Tipo> (noul), n_<Tipo> (choice), mode_lift / mode_launch (noul). Misma lectura que usa OpenAI.
		return JamParsePieceAnswers(Answers, TEXT("jev"), OutChoice, OutNote);
	}
	const TSharedPtr<FJsonObject>* Arch = nullptr;
	FString Picked;
	if (!(*Answers)->TryGetObjectField(TEXT("archetype"), Arch) || !Arch || !(*Arch)->TryGetStringField(TEXT("choice"), Picked))
	{
		OutNote = TEXT("answers.archetype.choice missing");
		return false;
	}
	OutChoice.Archetype = FName(*Picked);

	for (const FJamExtraDef& E : UJamBuildGenerator::ExtraTable())
	{
		const TSharedPtr<FJsonObject>* A = nullptr;
		double P = 0.0;
		if ((*Answers)->TryGetObjectField(E.Key.ToString(), A) && A && (*A)->TryGetNumberField(TEXT("noul"), P) && P > 0.5)
		{
			OutChoice.Extras.Add(E.Key);
		}
	}

	double Conf = 0.0;
	(*Arch)->TryGetNumberField(TEXT("confidence"), Conf);
	OutNote = FString::Printf(TEXT("jev archetype=%s conf=%.2f"), *Picked, Conf);
	return true;
}

void FJamDecisionClient::RequestPlan(const FString& Prompt, TFunction<void(bool, const FJamBuildChoice&, const FString&)> Done)
{
	const FString FalKey = GetFalKey();
	if (FalKey.IsEmpty())
	{
		Done(false, FJamBuildChoice(), TEXT("no FAL_KEY"));
		return;
	}
	// Prompt: el catalogo sale de las tablas (tipo, tamaño, masa y para que sirve). El LLM solo dice QUE piezas y CUANTAS.
	FString Sys = TEXT("You design machines for a physics sandbox game. The player says in Spanish what to build; you answer ONLY with the primitive parts that compose it. ")
		TEXT("Function emerges from the parts: wheels roll, fans and rockets push, balloons lift, a turntable spins what is on it, a head aims what is on it, a spring launches, weapons fire.\n")
		TEXT("PARTS (type | size cm, mass kg | when to use it). Use ONLY these exact type names:\n");
	for (const FJamPieceRule& R : UJamBuildGenerator::PieceTable())
	{
		FJamPartDef D;
		if (UJamPartCatalog::Find(R.Type, D)) { Sys += FString::Printf(TEXT("%s | %.0fx%.0fx%.0f, %.0f kg | %s\n"), *R.Type.ToString(), D.Size.X, D.Size.Y, D.Size.Z, D.MassKg, *R.Ask); }
	}
	Sys += BuildConnectionGuide();
	Sys += TEXT("OUTPUT: one JSON object, no markdown: {\"name\": short Spanish name, \"root\": the type of the main body (Plank, Block, Cylinder, Sled, Wall or Deck), \"pieces\": [{\"type\": type, \"count\": integer 1..40, \"mode\": optional}]}\n")
		TEXT("- mode \"lift\" on Fan, Jet, Turbine, Propeller or Rocket = pointing down so it lifts (drones, helicopters, rocket ships). mode \"launch\" on Spring = it throws things. mode \"inline\" on wheels = one behind the other (motorcycle).\n")
		TEXT("- To fly it needs lift: each Fan lifts about 25 kg, Turbine 60 kg, Propeller 70 kg, Balloon 14 kg. Keep total mass below the lift.\n")
		TEXT("- Ground vehicles need wheels (usually 4) on a Plank. Anything the player rides or drives gets a Seat automatically: do not add it.\n")
		TEXT("- If the player says a number, use exactly that count. At most 40 parts in total.\n")
		TEXT("- Never refuse: for anything absurd, build the closest fun machine out of these parts (a pizza = Deck body, an animal = body + Head + wheels as legs, a spider = Block + 8 Girder legs).\n")
		TEXT("EXAMPLES\nRequest: \"un carro con seis llantas\"\n{\"name\": \"carro de seis llantas\", \"root\": \"Plank\", \"pieces\": [{\"type\": \"Plank\", \"count\": 1}, {\"type\": \"Wheel\", \"count\": 6}]}\n")
		TEXT("Request: \"una lavadora voladora\"\n{\"name\": \"lavadora voladora\", \"root\": \"Block\", \"pieces\": [{\"type\": \"Block\", \"count\": 1}, {\"type\": \"Turntable\", \"count\": 1}, {\"type\": \"Fan\", \"count\": 4, \"mode\": \"lift\"}, {\"type\": \"Stabilizer\", \"count\": 1}]}\n")
		TEXT("Request: \"una catapulta\"\n{\"name\": \"catapulta\", \"root\": \"Plank\", \"pieces\": [{\"type\": \"Plank\", \"count\": 1}, {\"type\": \"Wheel\", \"count\": 2}, {\"type\": \"Block\", \"count\": 1}, {\"type\": \"Spring\", \"count\": 1, \"mode\": \"launch\"}, {\"type\": \"Ball\", \"count\": 1}]}");
	TSharedRef<FJsonObject> Body = Obj();
	Body->SetStringField(TEXT("model"), TEXT("google/gemini-2.5-flash"));
	Body->SetNumberField(TEXT("temperature"), 0.2);
	Body->SetStringField(TEXT("system_prompt"), Sys);
	Body->SetStringField(TEXT("prompt"), FString::Printf(TEXT("Request: \"%s\""), *Prompt));
	FString BodyStr;
	FJsonSerializer::Serialize(Body, TJsonWriterFactory<>::Create(&BodyStr));

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> FReq = FHttpModule::Get().CreateRequest();
	FReq->SetURL(TEXT("https://fal.run/openrouter/router"));
	FReq->SetVerb(TEXT("POST"));
	FReq->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	FReq->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Key %s"), *FalKey));
	FReq->SetContentAsString(BodyStr);
	FReq->SetTimeout(GetTimeout(TEXT("fal"), 8.f));
	FReq->OnProcessRequestComplete().BindLambda([Done](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
	{
		FJamBuildChoice C;
		FString Note;
		if (!bOk || !Resp.IsValid() || Resp->GetResponseCode() != 200)
		{
			Done(false, C, Resp.IsValid() ? FString::Printf(TEXT("fal http %d"), Resp->GetResponseCode()) : FString(TEXT("fal http failure/timeout")));
			return;
		}
		const bool bParsed = ParseFalResponse(Resp->GetContentAsString(), C, Note);
		UE_LOG(LogJamDecision, Log, TEXT("backend=fal-plan ok=%d %s"), bParsed, *Note);
		Done(bParsed, C, Note);
	});
	FReq->ProcessRequest();
}

void FJamDecisionClient::RequestOpenAIChoice(const FString& Prompt, TFunction<void(bool, const FJamBuildChoice&, const FString&)> Done)
{
	const FString Key = GetOpenAIKey();
	if (Key.IsEmpty())
	{
		Done(false, FJamBuildChoice(), FString(OpenAIBackend) + TEXT(" no OPENAI_API_KEY"));
		return;
	}
	JamPostOpenAI(Key, BuildOpenAIRequestBody(Prompt), [Done, Prompt](bool bHttpOk, int32 Code, const FString& Content, int32 Ms)
	{
		FJamBuildChoice C;
		FString Note;
		bool bParsed = false;
		if (bHttpOk) { bParsed = ParseOpenAIResponse(Content, Prompt, C, Note); }
		else if (Code > 0) { Note = FString::Printf(TEXT("%s http %d"), OpenAIBackend, Code); } // solo el codigo: el cuerpo de un error no va al log
		else { Note = FString(OpenAIBackend) + TEXT(" http failure/timeout"); }
		// La nota empieza con "openai-decisions " (Finish separa el backend por el primer espacio); en el log no se repite.
		FString LogNote = Note;
		LogNote.RemoveFromStart(FString(OpenAIBackend) + TEXT(" "));
		UE_LOG(LogJamDecision, Log, TEXT("backend=openai-decisions ok=%d ms=%d %s extras=[%s]"), bParsed ? 1 : 0, Ms, *LogNote, *ExtrasStr(C));
		Done(bParsed, C, Note);
	});
}

void FJamDecisionClient::RequestGunChoice(const FString& Phrase, TFunction<void(bool, const FString&, const FString&, const TArray<FString>&)> Done)
{
	const FString Key = GetOpenAIKey();
	if (Key.IsEmpty())
	{
		Done(false, FString(), FString(), TArray<FString>());
		return;
	}
	JamPostOpenAI(Key, JamGun::BuildDecisionRequest(Phrase), [Done, Phrase](bool bHttpOk, int32 Code, const FString& Content, int32 Ms)
	{
		FString BodyId, MuzzleId, Note;
		TArray<FString> ExtraIds;
		bool bParsed = false;
		if (bHttpOk) { bParsed = JamGun::ParseDecisionResponse(Content, Phrase, BodyId, MuzzleId, ExtraIds, Note); }
		else if (Code > 0) { Note = FString::Printf(TEXT("http %d"), Code); }
		else { Note = TEXT("http failure/timeout"); }
		UE_LOG(LogJamDecision, Log, TEXT("backend=openai-decisions gun ok=%d ms=%d body=%s muzzle=%s extras=[%s] %s"), bParsed ? 1 : 0, Ms, *BodyId, *MuzzleId, *FString::Join(ExtraIds, TEXT(",")), *Note);
		Done(bParsed, BodyId, MuzzleId, ExtraIds);
	});
}

void FJamDecisionClient::RequestChoice(const FString& Prompt, TFunction<void(bool, const FJamBuildChoice&, const FString&)> Done)
{
	// Prioridad: TypeSafe directo (Jev) > OpenRouter (Jev) > OpenAI Decisions > fal (LLM generico).
	const FString TsKey = GetTypeSafeKey();
	const bool bDirect = !TsKey.IsEmpty();
	const FString Key = bDirect ? TsKey : GetApiKey();
	const bool bOpenAI = !GetOpenAIKey().IsEmpty();
	if (Key.IsEmpty())
	{
		if (bOpenAI)
		{
			RequestOpenAIChoice(Prompt, Done); // sin Jev: la Decisions API de OpenAI contesta las mismas preguntas
			return;
		}
		const FString FalKey = GetFalKey();
		if (FalKey.IsEmpty())
		{
			Done(false, FJamBuildChoice(), TEXT("no OPENROUTER_API_KEY, OPENAI_API_KEY ni FAL_KEY"));
			return;
		}
		RequestPlan(Prompt, Done); // sin Jev ni OpenAI: el planificador LLM decide la bolsa de piezas
		return;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(bDirect ? TEXT("https://api.typesafe.ai/v1/systemone") : Endpoint);
	Req->SetVerb(TEXT("POST"));
	Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Req->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *Key));
	Req->SetContentAsString(BuildRequestBody(Prompt, bDirect ? TEXT("jev-1.13.0") : nullptr));
	Req->SetTimeout(bDirect ? GetTimeout(TEXT("typesafe"), 3.f) : GetTimeout(TEXT("openrouter"), 4.f));
	Req->OnProcessRequestComplete().BindLambda([Done, bDirect, bOpenAI, Prompt](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
	{
		// Si Jev falla (red, corte, error o sin piezas) y hay clave de OpenAI, se le pregunta a OpenAI antes de rendirse.
		auto Fail = [&Done, bDirect, bOpenAI, &Prompt](const FJamBuildChoice& FailChoice, const FString& FailNote)
		{
			if (bOpenAI)
			{
				UE_LOG(LogJamDecision, Log, TEXT("backend=%s ok=0 %s -> openai-decisions"), bDirect ? TEXT("typesafe-jev") : TEXT("openrouter-jev"), *FailNote.Left(200));
				RequestOpenAIChoice(Prompt, Done);
				return;
			}
			Done(false, FailChoice, FailNote);
		};
		FJamBuildChoice C;
		FString Note;
		if (!bOk || !Resp.IsValid())
		{
			Fail(C, TEXT("http failure/timeout"));
			return;
		}
		if (Resp->GetResponseCode() != 200)
		{
			Fail(C, FString::Printf(TEXT("http %d %s"), Resp->GetResponseCode(), *Resp->GetContentAsString().Left(200)));
			return;
		}
		const bool bParsed = ParseResponse(Resp->GetContentAsString(), C, Note);
		UE_LOG(LogJamDecision, Log, TEXT("backend=%s ok=%d %s extras=[%s]"), bDirect ? TEXT("typesafe-jev") : TEXT("openrouter-jev"), bParsed, *Note, *ExtrasStr(C));
		if (!bParsed)
		{
			UE_LOG(LogJamDecision, Warning, TEXT("respuesta: %s"), *Resp->GetContentAsString().Left(400));
			Fail(C, Note);
			return;
		}
		Done(true, C, Note);
	});
	Req->ProcessRequest();
}
