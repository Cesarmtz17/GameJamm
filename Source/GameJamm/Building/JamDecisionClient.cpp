#include "Building/JamDecisionClient.h"

#include "Building/JamBuildPart.h"

#include "Dom/JsonObject.h"
#include "HAL/PlatformMisc.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/ConfigCacheIni.h"
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
}

FString FJamDecisionClient::GetApiKey()
{
	FString Key = FPlatformMisc::GetEnvironmentVariable(TEXT("OPENROUTER_API_KEY"));
	if (!Key.IsEmpty())
	{
		return Key;
	}
	FConfigFile Secrets;
	Secrets.Read(FPaths::ProjectConfigDir() / TEXT("DefaultSecrets.ini"));
	Secrets.GetString(TEXT("Jam"), TEXT("OpenRouterApiKey"), Key);
	return Key;
}

FString FJamDecisionClient::GetTypeSafeKey()
{
	FString Key = FPlatformMisc::GetEnvironmentVariable(TEXT("TYPESAFE_API_KEY"));
	if (!Key.IsEmpty())
	{
		return Key;
	}
	FConfigFile Secrets;
	Secrets.Read(FPaths::ProjectConfigDir() / TEXT("DefaultSecrets.ini"));
	Secrets.GetString(TEXT("Jam"), TEXT("DecisionsApiKey"), Key);
	return Key;
}

FString FJamDecisionClient::GetFalKey()
{
	FString Key = FPlatformMisc::GetEnvironmentVariable(TEXT("FAL_KEY"));
	if (!Key.IsEmpty())
	{
		return Key;
	}
	FConfigFile Secrets;
	Secrets.Read(FPaths::ProjectConfigDir() / TEXT("DefaultSecrets.ini"));
	Secrets.GetString(TEXT("Jam"), TEXT("FalApiKey"), Key);
	return Key;
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
			if (ModeStr == TEXT("lift") || ModeStr == TEXT("launch") || ModeStr == TEXT("inline")) { I->Mode = FName(*ModeStr); }
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

FString FJamDecisionClient::BuildRequestBody(const FString& Prompt, const TCHAR* ModelOverride)
{
	TSharedRef<FJsonObject> Root = Obj();
	Root->SetStringField(TEXT("model"), ModelOverride ? ModelOverride : ModelId);

	TSharedRef<FJsonObject> State = Obj();
	State->SetStringField(TEXT("request"), Prompt);
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
	Root->SetObjectField(TEXT("questions"), Questions);

	FString Out;
	const TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root, W);
	return Out;
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
		// Formato por piezas: use_<Tipo> (noul), n_<Tipo> (choice), mode_lift / mode_launch (noul).
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
		OutNote = FString::Printf(TEXT("jev pieces=%d conf=1.00 pattern=%s root=%s [%s]"), OutChoice.Pieces.Num(), *OutChoice.Pattern.ToString(), *OutChoice.Root.ToString(), *FString::Join(Txt, TEXT(", ")));
		return OutChoice.Pieces.Num() > 0;
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
	FReq->SetTimeout(8.f);
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

void FJamDecisionClient::RequestChoice(const FString& Prompt, TFunction<void(bool, const FJamBuildChoice&, const FString&)> Done)
{
	// Prioridad: TypeSafe directo (Jev) > OpenRouter (Jev) > fal (LLM generico).
	const FString TsKey = GetTypeSafeKey();
	const bool bDirect = !TsKey.IsEmpty();
	const FString Key = bDirect ? TsKey : GetApiKey();
	if (Key.IsEmpty())
	{
		const FString FalKey = GetFalKey();
		if (FalKey.IsEmpty())
		{
			Done(false, FJamBuildChoice(), TEXT("no OPENROUTER_API_KEY ni FAL_KEY"));
			return;
		}
		RequestPlan(Prompt, Done); // sin Jev: el planificador LLM decide la bolsa de piezas
		return;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(bDirect ? TEXT("https://api.typesafe.ai/v1/systemone") : Endpoint);
	Req->SetVerb(TEXT("POST"));
	Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Req->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *Key));
	Req->SetContentAsString(BuildRequestBody(Prompt, bDirect ? TEXT("jev-1.13.0") : nullptr));
	Req->SetTimeout(8.f);
	Req->OnProcessRequestComplete().BindLambda([Done, bDirect](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
	{
		FJamBuildChoice C;
		FString Note;
		if (!bOk || !Resp.IsValid())
		{
			Done(false, C, TEXT("http failure/timeout"));
			return;
		}
		if (Resp->GetResponseCode() != 200)
		{
			Done(false, C, FString::Printf(TEXT("http %d %s"), Resp->GetResponseCode(), *Resp->GetContentAsString().Left(200)));
			return;
		}
		const bool bParsed = ParseResponse(Resp->GetContentAsString(), C, Note);
		UE_LOG(LogJamDecision, Log, TEXT("backend=%s ok=%d %s extras=[%s]"), bDirect ? TEXT("typesafe-jev") : TEXT("openrouter-jev"), bParsed, *Note, *ExtrasStr(C));
		if (!bParsed) { UE_LOG(LogJamDecision, Warning, TEXT("respuesta: %s"), *Resp->GetContentAsString().Left(400)); }
		Done(bParsed, C, Note);
	});
	Req->ProcessRequest();
}
