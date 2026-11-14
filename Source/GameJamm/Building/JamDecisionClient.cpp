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

namespace
{
	const TCHAR* Endpoint = TEXT("https://openrouter.ai/api/alpha/decisions");
	const TCHAR* ModelId = TEXT("typesafe/jev-1.13"); // fijado: los umbrales cambian entre versiones.

	// Orden fijo de las opciones: Jev altera las probabilidades si cambia el orden.
	struct FExtra { const TCHAR* Key; const TCHAR* TrueText; const TCHAR* FalseText; };
	const FExtra Extras[] = {
		{ TEXT("fan"), TEXT("The user wants a fan, propeller, turbo or extra air thrust."), TEXT("No fan or propeller is asked for.") },
		{ TEXT("rocket"), TEXT("The user wants a rocket, nitro or a rocket booster."), TEXT("No rocket or nitro is asked for.") },
		{ TEXT("flame"), TEXT("The user wants fire, flames or a flamethrower."), TEXT("No fire is asked for.") },
		{ TEXT("cannon"), TEXT("The user wants a cannon, gun or something that shoots."), TEXT("No cannon or shooting is asked for.") },
		{ TEXT("balloon"), TEXT("The user wants a balloon or floating lift."), TEXT("No balloon is asked for.") },
	};

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
	D->TryGetBoolField(TEXT("fan"), OutChoice.bFan);
	D->TryGetBoolField(TEXT("rocket"), OutChoice.bRocket);
	D->TryGetBoolField(TEXT("flame"), OutChoice.bFlame);
	D->TryGetBoolField(TEXT("cannon"), OutChoice.bCannon);
	D->TryGetBoolField(TEXT("balloon"), OutChoice.bBalloon);
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

	TSharedRef<FJsonObject> Arch = Obj();
	Arch->SetStringField(TEXT("type"), TEXT("choice"));
	Arch->SetStringField(TEXT("instructions"), TEXT("Pick the kind of machine the user wants to build from their request."));
	TSharedRef<FJsonObject> ArchCriteria = Obj();
	ArchCriteria->SetStringField(TEXT("Car"), TEXT("A ground vehicle: car, cart, truck, buggy, anything that drives on wheels."));
	ArchCriteria->SetStringField(TEXT("Flyer"), TEXT("Something that flies or hovers: plane, drone, helicopter, glider."));
	ArchCriteria->SetStringField(TEXT("Turret"), TEXT("A fixed defensive or shooting emplacement: turret, cannon tower."));
	ArchCriteria->SetStringField(TEXT("Rocket"), TEXT("A rocket or missile that launches upward."));
	Arch->SetObjectField(TEXT("criteria"), ArchCriteria);
	Questions->SetObjectField(TEXT("archetype"), Arch);

	for (const FExtra& E : Extras)
	{
		TSharedRef<FJsonObject> Q = Obj();
		Q->SetStringField(TEXT("type"), TEXT("noul"));
		Q->SetStringField(TEXT("instructions"), TEXT("Decide whether the user's request asks for this extra part."));
		TSharedRef<FJsonObject> Crit = Obj();
		Crit->SetStringField(TEXT("true"), E.TrueText);
		Crit->SetStringField(TEXT("false"), E.FalseText);
		Q->SetObjectField(TEXT("criteria"), Crit);
		Questions->SetObjectField(E.Key, Q);
	}
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
	const TSharedPtr<FJsonObject>* Arch = nullptr;
	FString Picked;
	if (!(*Answers)->TryGetObjectField(TEXT("archetype"), Arch) || !Arch || !(*Arch)->TryGetStringField(TEXT("choice"), Picked))
	{
		OutNote = TEXT("answers.archetype.choice missing");
		return false;
	}
	OutChoice.Archetype = FName(*Picked);

	auto Yes = [&](const TCHAR* Key) -> bool
	{
		const TSharedPtr<FJsonObject>* A = nullptr;
		double P = 0.0;
		return (*Answers)->TryGetObjectField(Key, A) && A && (*A)->TryGetNumberField(TEXT("noul"), P) && P > 0.5;
	};
	OutChoice.bFan = Yes(TEXT("fan"));
	OutChoice.bRocket = Yes(TEXT("rocket"));
	OutChoice.bFlame = Yes(TEXT("flame"));
	OutChoice.bCannon = Yes(TEXT("cannon"));
	OutChoice.bBalloon = Yes(TEXT("balloon"));

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
		Body->SetStringField(TEXT("system_prompt"), TEXT("Eres el modelo de decision de un juego de construccion. Responde SOLO un JSON sin markdown con: archetype (Car|Flyer|Turret|Rocket) y booleanos fan, rocket, flame, cannon, balloon segun lo que pida el jugador."));
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
	Req->OnProcessRequestComplete().BindLambda([Done](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
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
		Done(bParsed, C, Note);
	});
	Req->ProcessRequest();
}
