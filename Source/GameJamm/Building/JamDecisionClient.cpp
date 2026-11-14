#include "Building/JamDecisionClient.h"

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
		if (R.bCountable)
		{
			TSharedRef<FJsonObject> N = Obj();
			N->SetStringField(TEXT("type"), TEXT("choice"));
			N->SetStringField(TEXT("instructions"), FString::Printf(TEXT("How many pieces of this kind does the user ask for? Piece: %s"), *R.Ask));
			TSharedRef<FJsonObject> NC = Obj();
			NC->SetStringField(TEXT("default"), TEXT("The user gives no number for this piece."));
			for (const TCHAR* Num : { TEXT("1"), TEXT("2"), TEXT("3"), TEXT("4"), TEXT("6"), TEXT("8"), TEXT("10"), TEXT("12"), TEXT("16"), TEXT("20"), TEXT("25") })
			{
				NC->SetStringField(Num, FString::Printf(TEXT("The user asks for %s of them (or the closest number to %s)."), Num, Num));
			}
			N->SetObjectField(TEXT("criteria"), NC);
			Questions->SetObjectField(TEXT("n_") + R.Type.ToString(), N);
		}
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
	Flag(TEXT("mode_lift"), TEXT("It must lift off the ground with propellers or rockets pointing down: drone, helicopter, quadcopter, rocket ship, missile."),
		TEXT("Its propellers or rockets push it forward along the ground, or it has none."));
	Flag(TEXT("mode_launch"), TEXT("It must throw or launch things through the air: catapult, launcher, trampoline, trebuchet."),
		TEXT("It does not throw things; a spring would only make it jump."));
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
		for (const FJamPieceRule& R : UJamBuildGenerator::PieceTable())
		{
			if (Noul(TEXT("use_") + R.Type.ToString()) <= 0.5) { continue; }
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
				const bool bSpring = R.Type == "Spring";
				if (bSpring ? bLaunch : bLift) { I.Mode = bSpring ? FName("launch") : FName("lift"); }
				if (I.Mode == "lift" && R.Type == "Fan") { I.Count = FMath::Max(I.Count, 2); } // para sustentar hacen falta al menos dos helices
			}
			OutChoice.Pieces.Add(I);
			Txt.Add(FString::Printf(TEXT("%s x%d"), *I.Type.ToString(), I.Count));
		}
		OutChoice.Archetype = "Pieces";
		OutNote = FString::Printf(TEXT("jev pieces=%d conf=1.00 [%s]"), OutChoice.Pieces.Num(), *FString::Join(Txt, TEXT(", ")));
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
		// Respaldo: LLM de fal elige arquetipo y extras en JSON (no es Jev; misma interfaz de decision).
		TSharedRef<FJsonObject> Body = Obj();
		Body->SetStringField(TEXT("model"), TEXT("google/gemini-2.5-flash"));
		Body->SetNumberField(TEXT("temperature"), 0.0);
				FString Sys = TEXT("Eres el modelo de decision de un juego de construccion. Responde SOLO un JSON sin markdown: {\"archetype\": uno de [");
		for (const FJamArchetypeDef& A : UJamBuildGenerator::ArchetypeTable()) { Sys += A.Name.ToString() + TEXT(" = ") + A.Description + TEXT("; "); }
		Sys += TEXT("], \"extras\": lista con las claves que el jugador pida de [");
		for (const FJamExtraDef& E : UJamBuildGenerator::ExtraTable()) { Sys += E.Key.ToString() + TEXT(" = ") + E.TrueText + TEXT("; "); }
		Sys += TEXT("]}. Solo incluye extras pedidos explicitamente o implicitos.");
		Body->SetStringField(TEXT("system_prompt"), Sys);
		Body->SetStringField(TEXT("prompt"), Prompt);
		FString BodyStr;
		FJsonSerializer::Serialize(Body, TJsonWriterFactory<>::Create(&BodyStr));

		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> FReq = FHttpModule::Get().CreateRequest();
		FReq->SetURL(TEXT("https://fal.run/openrouter/router"));
		FReq->SetVerb(TEXT("POST"));
		FReq->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
		FReq->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Key %s"), *FalKey));
		FReq->SetContentAsString(BodyStr);
		FReq->SetTimeout(15.f);
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
			UE_LOG(LogJamDecision, Log, TEXT("backend=fal ok=%d %s extras=[%s]"), bParsed, *Note, *ExtrasStr(C));
			Done(bParsed, C, Note);
		});
		FReq->ProcessRequest();
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
