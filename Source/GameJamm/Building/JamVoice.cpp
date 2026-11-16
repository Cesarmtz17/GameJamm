#include "Building/JamVoice.h"

#include "AudioCaptureCore.h"
#include "Async/Async.h"
#include "Building/JamAssembler.h"
#include "Building/JamDecisionClient.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformTime.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogJamVoice, Log, All);

/** Envoltorio para no meter el header de captura de audio en JamVoice.h. */
class FJamVoiceCapture
{
public:
	Audio::FAudioCapture Capture;
};

namespace
{
	void Put(TArray<uint8>& B, const void* Data, int32 Num) { B.Append((const uint8*)Data, Num); }
	void Put32(TArray<uint8>& B, uint32 V) { Put(B, &V, 4); }
	void Put16(TArray<uint8>& B, uint16 V) { Put(B, &V, 2); }

	/** WAV PCM de 16 bits mono, bajado a ~16 kHz (promediando muestras) para que la subida sea chica. */
	TArray<uint8> MakeWav(const TArray<float>& InMono, int32 SampleRate)
	{
		if (InMono.IsEmpty() || SampleRate <= 0) { return TArray<uint8>(); }
		// Recorta el silencio del inicio y del final (deja 0.15 s de margen): menos audio = respuesta mas rapida.
		const float Gate = 0.012f;
		int32 First = 0, Last = InMono.Num() - 1;
		while (First < Last && FMath::Abs(InMono[First]) < Gate) { ++First; }
		while (Last > First && FMath::Abs(InMono[Last]) < Gate) { --Last; }
		const int32 Margin = SampleRate * 15 / 100;
		First = FMath::Max(0, First - Margin);
		Last = FMath::Min(InMono.Num() - 1, Last + Margin);
		const TArrayView<const float> Mono(InMono.GetData() + First, FMath::Max(0, Last - First + 1));

		const int32 Step = FMath::Max(1, SampleRate / 16000);
		const int32 OutRate = SampleRate / Step;
		const int32 Num = Mono.Num() / Step;
		TArray<uint8> B;
		B.Reserve(44 + Num * 2);
		Put(B, "RIFF", 4); Put32(B, 36 + Num * 2); Put(B, "WAVE", 4);
		Put(B, "fmt ", 4); Put32(B, 16); Put16(B, 1); Put16(B, 1); Put32(B, OutRate); Put32(B, OutRate * 2); Put16(B, 2); Put16(B, 16);
		Put(B, "data", 4); Put32(B, Num * 2);
		for (int32 i = 0; i < Num; ++i)
		{
			float Sum = 0.f;
			for (int32 k = 0; k < Step; ++k) { Sum += Mono[i * Step + k]; }
			Put16(B, (uint16)(int16)FMath::Clamp(FMath::RoundToInt(Sum / Step * 32767.f), -32768, 32767));
		}
		return B;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req(const TCHAR* Verb, const FString& Url, const FString& Key, const TCHAR* ContentType)
	{
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> R = FHttpModule::Get().CreateRequest();
		R->SetVerb(Verb);
		R->SetURL(Url);
		R->SetHeader(TEXT("Content-Type"), ContentType);
		if (!Key.IsEmpty()) { R->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Key %s"), *Key)); }
		R->SetTimeout(30.f);
		R->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnGameThread);
		return R;
	}

	bool ValidateVoiceWav(const TArray<uint8>& Wav, FString& Why)
	{
		if (Wav.Num() > 25 * 1024 * 1024) { Why = TEXT("Voice file is too large (maximum 25 MiB). Press B to type your request."); return false; }
		if (Wav.Num() < 44 || FMemory::Memcmp(Wav.GetData(), "RIFF", 4) != 0 || FMemory::Memcmp(Wav.GetData() + 8, "WAVE", 4) != 0)
		{
			Why = TEXT("Voice file is not a valid WAV recording. Press B to type your request."); return false;
		}
		auto Read32 = [&Wav](int32 P) { return uint32(Wav[P]) | (uint32(Wav[P + 1]) << 8) | (uint32(Wav[P + 2]) << 16) | (uint32(Wav[P + 3]) << 24); };
		const uint64 End = uint64(Read32(4)) + 8;
		bool bFormat = false, bAudio = false;
		if (End <= uint64(Wav.Num()) && End >= 44)
		{
			for (uint64 At = 12; At + 8 <= End;)
			{
				const uint32 Size = Read32(int32(At + 4));
				const uint64 Next = At + 8 + Size;
				if (Next > End) { break; }
				if (FMemory::Memcmp(Wav.GetData() + At, "fmt ", 4) == 0 && Size >= 16)
				{
					const int32 F = int32(At + 8);
					bFormat = (Wav[F + 2] || Wav[F + 3]) && Read32(F + 4) > 0 && Read32(F + 8) > 0 && (Wav[F + 12] || Wav[F + 13]);
				}
				if (FMemory::Memcmp(Wav.GetData() + At, "data", 4) == 0 && Size > 0) { bAudio = true; }
				At = Next + (Size & 1);
			}
		}
		if (!bFormat || !bAudio) { Why = TEXT("Voice WAV is empty, truncated or has no audio format. Press B to type your request."); return false; }
		return true;
	}

	bool JsonOf(FHttpResponsePtr Resp, bool bOk, TSharedPtr<FJsonObject>& Out)
	{
		return bOk && Resp.IsValid() && Resp->GetResponseCode() / 100 == 2
			&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Resp->GetContentAsString()), Out) && Out.IsValid();
	}

	FString HttpWhy(const TCHAR* Step, FHttpResponsePtr Resp)
	{
		return Resp.IsValid() ? FString::Printf(TEXT("%s: voice service returned %d"), Step, Resp->GetResponseCode())
			: FString::Printf(TEXT("%s: could not connect to the voice service"), Step);
	}
}

namespace
{
	const TCHAR* LocalUrl = TEXT("http://127.0.0.1:8177");
	FProcHandle GServer;
	bool GLocalReady = false;   // el servidor local respondio al calentamiento
	bool GLocalTried = false;

	FString FirstExisting(const TArray<FString>& Paths)
	{
		for (const FString& P : Paths)
		{
			if (!P.IsEmpty() && FPaths::FileExists(P)) { return P; }
		}
		return FString();
	}

	/** Palabras del juego (claves de extras y plantillas) como pista para el reconocedor: sube mucho el acierto. */
	FString Vocabulary()
	{
		return TEXT("Prompt and Conquer. Build a car, tank, hovercraft, balloon, plane, drone, catapult or wheel. "
			"Plank, block, sail, cannon, laser, flamethrower, ice emitter, rocket, fan, wings. Sword, bow, rifle, shotgun.");
	}

	/** multipart/form-data para whisper-server: /inference con el WAV y los parametros. */
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> LocalRequest(const TArray<uint8>& Wav, float Timeout)
	{
		const FString Boundary = TEXT("----JamVoiceBoundary7d81b");
		TArray<uint8> Body;
		auto AddText = [&Body](const FString& S) { FTCHARToUTF8 U(*S); Body.Append((const uint8*)U.Get(), U.Length()); };
		auto Field = [&](const TCHAR* Name, const FString& Value)
		{
			AddText(FString::Printf(TEXT("--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\n\r\n%s\r\n"), *Boundary, Name, *Value));
		};
		Field(TEXT("response_format"), TEXT("json"));
		Field(TEXT("language"), TEXT("auto"));
		Field(TEXT("temperature"), TEXT("0.0"));
		Field(TEXT("prompt"), Vocabulary());
		AddText(FString::Printf(TEXT("--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"voz.wav\"\r\nContent-Type: audio/wav\r\n\r\n"), *Boundary));
		Body.Append(Wav);
		AddText(FString::Printf(TEXT("\r\n--%s--\r\n"), *Boundary));
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> R = FHttpModule::Get().CreateRequest();
		R->SetVerb(TEXT("POST"));
		R->SetURL(FString(LocalUrl) + TEXT("/inference"));
		R->SetHeader(TEXT("Content-Type"), FString::Printf(TEXT("multipart/form-data; boundary=%s"), *Boundary));
		R->SetContent(Body);
		R->SetTimeout(Timeout);
		R->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnGameThread);
		return R;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> OpenAIRequest(const TArray<uint8>& Wav, const FString& Key)
	{
		const FString Boundary = TEXT("----JamWhisperBoundary") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
		TArray<uint8> Body;
		auto AddText = [&Body](const FString& S) { FTCHARToUTF8 U(*S); Body.Append((const uint8*)U.Get(), U.Length()); };
		auto Field = [&](const TCHAR* Name, const FString& Value)
		{
			AddText(FString::Printf(TEXT("--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\n\r\n%s\r\n"), *Boundary, Name, *Value));
		};
		Field(TEXT("model"), TEXT("whisper-1"));
		Field(TEXT("response_format"), TEXT("json"));
		Field(TEXT("prompt"), Vocabulary()); // language omitted: preserve English or Spanish as spoken
		AddText(FString::Printf(TEXT("--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"voice.wav\"\r\nContent-Type: audio/wav\r\n\r\n"), *Boundary));
		Body.Append(Wav);
		AddText(FString::Printf(TEXT("\r\n--%s--\r\n"), *Boundary));
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> R = FHttpModule::Get().CreateRequest();
		R->SetVerb(TEXT("POST"));
		R->SetURL(FJamDecisionClient::GetOpenAIEndpoint(TEXT("/v1/audio/transcriptions")));
		R->SetHeader(TEXT("User-Agent"), TEXT("PromptAndConquer/1.0"));
		R->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *Key));
		R->SetHeader(TEXT("Content-Type"), FString::Printf(TEXT("multipart/form-data; boundary=%s"), *Boundary));
		R->SetContent(Body);
		R->SetTimeout(20.f);
		R->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnGameThread);
		return R;
	}

	FString CleanText(FString Text)
	{
		Text = Text.Replace(TEXT("\n"), TEXT(" ")).TrimStartAndEnd();
		Text.RemoveFromEnd(TEXT("."));
		// "hovercraft" es la palabra que mas se le escapa al modelo chico ("unavercraft", "1 vercraft", "overcraft").
		TArray<FString> Words;
		Text.ParseIntoArray(Words, TEXT(" "));
		for (FString& W : Words)
		{
			if (W.ToLower().Contains(TEXT("vercraft")) && !W.ToLower().Contains(TEXT("hovercraft"))) { W = TEXT("hovercraft"); }
		}
		Text = FString::Join(Words, TEXT(" "));
		// Ruido tipico del reconocedor con silencio: corchetes o parentesis.
		if (Text.StartsWith(TEXT("[")) || Text.StartsWith(TEXT("("))) { Text.Reset(); }
		return Text;
	}
}

void FJamVoice::WarmUp()
{
	if (!FJamDecisionClient::GetOpenAIAuthToken().IsEmpty()) { return; }
	if (GLocalTried)
	{
		return;
	}
	GLocalTried = true;
	// Medio segundo de silencio: si el servidor ya esta arriba responde y queda caliente; si no, se arranca y se reintenta.
	TArray<float> Silence;
	Silence.SetNumZeroed(8000);
	const TArray<uint8> Wav = MakeWav(Silence, 16000);
	TSharedPtr<int32> Tries = MakeShared<int32>(0);
	TSharedPtr<TFunction<void()>> Ping = MakeShared<TFunction<void()>>();
	*Ping = [Wav, Tries, Ping]()
	{
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> R = LocalRequest(Wav, 8.f);
		R->OnProcessRequestComplete().BindLambda([Tries, Ping](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
		{
			if (bOk && Resp.IsValid() && Resp->GetResponseCode() == 200)
			{
				GLocalReady = true;
				UE_LOG(LogJamVoice, Log, TEXT("voz local lista (whisper-server en %s)"), LocalUrl);
				return;
			}
			if (*Tries == 0)
			{
				// No hay servidor: arrancarlo si whisper.cpp y un modelo estan instalados.
				const FString Home = FPlatformMisc::GetEnvironmentVariable(TEXT("HOME"));
				const FString Exe = FirstExisting({ FPlatformMisc::GetEnvironmentVariable(TEXT("JAM_WHISPER_SERVER")), TEXT("/opt/homebrew/bin/whisper-server"), TEXT("/usr/local/bin/whisper-server") });
				const FString Model = FirstExisting({ FPlatformMisc::GetEnvironmentVariable(TEXT("JAM_WHISPER_MODEL")),
					Home / TEXT("Library/Application Support/GameJamm/whisper/ggml-base.bin"),
					Home / TEXT("Library/Application Support/geboou/whisper-models/ggml-base.bin"),
					TEXT("/opt/homebrew/share/whisper-cpp/ggml-base.bin") });
				if (Exe.IsEmpty() || Model.IsEmpty())
				{
					UE_LOG(LogJamVoice, Warning, TEXT("voz local no disponible (whisper-server: %s, modelo: %s): se usara la nube"), Exe.IsEmpty() ? TEXT("falta") : TEXT("ok"), Model.IsEmpty() ? TEXT("falta") : TEXT("ok"));
					return;
				}
				const FString Args = FString::Printf(TEXT("-m \"%s\" -l auto -bs 1 -nt --host 127.0.0.1 --port 8177"), *Model);
				GServer = FPlatformProcess::CreateProc(*Exe, *Args, true, true, true, nullptr, 0, nullptr, nullptr);
				UE_LOG(LogJamVoice, Log, TEXT("arrancando voz local: %s (ok=%d)"), *Exe, GServer.IsValid());
				FCoreDelegates::OnPreExit.AddLambda([]() { if (GServer.IsValid()) { FPlatformProcess::TerminateProc(GServer); } });
			}
			if (++(*Tries) <= 12)
			{
				FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Ping](float) { (*Ping)(); return false; }), 1.0f);
			}
		});
		R->ProcessRequest();
	};
	(*Ping)();
}

FJamVoice& FJamVoice::Get()
{
	static FJamVoice V;
	return V;
}

bool FJamVoice::Start(FString& OutError)
{
	if (bTranscribing)
	{
		OutError = TEXT("Voice transcription is still running. Wait for it to finish, or press B to type your request.");
		return false;
	}
	if (bRecording)
	{
		return true;
	}
	if (!GLocalReady && FJamDecisionClient::GetOpenAIAuthToken().IsEmpty() && FJamDecisionClient::GetFalKey().IsEmpty())
	{
		OutError = TEXT("Voice is unavailable: no configured recognizer. Press B to type your request.");
		return false;
	}
	if (!Capture.IsValid())
	{
		Capture = MakeShared<FJamVoiceCapture>();
	}
	{
		FScopeLock L(&Lock);
		Samples.Reset();
	}
	Audio::FAudioCaptureDeviceParams Params; // dispositivo de entrada por defecto
	Audio::FOnAudioCaptureFunction OnCapture = [this](const void* InAudio, int32 NumFrames, int32 NumChannels, int32 InSampleRate, double, bool)
	{
		if (!InAudio || NumFrames <= 0 || NumChannels <= 0 || InSampleRate <= 0) { return; }
		// Hilo de audio: mezcla a mono y acumula (maximo 20 s).
		const float* In = (const float*)InAudio;
		FScopeLock L(&Lock);
		SampleRate = InSampleRate;
		if (Samples.Num() > InSampleRate * 20) { return; }
		for (int32 f = 0; f < NumFrames; ++f)
		{
			float Sum = 0.f;
			for (int32 c = 0; c < NumChannels; ++c) { Sum += In[f * NumChannels + c]; }
			Samples.Add(Sum / FMath::Max(NumChannels, 1));
		}
	};
	if (!Capture->Capture.OpenAudioCaptureStream(Params, MoveTemp(OnCapture), 1024) || !Capture->Capture.StartStream())
	{
		Capture->Capture.AbortStream();
		OutError = TEXT("Could not open the microphone. Check microphone permissions or press B to type your request.");
		return false;
	}
	bRecording = true;
	Status = TEXT("escuchando");
	return true;
}

void FJamVoice::StopAndTranscribe(FDone Done)
{
	if (!bRecording)
	{
		return;
	}
	bRecording = false;
	Capture->Capture.StopStream();
	Capture->Capture.CloseStream();
	TArray<float> Mono;
	int32 Rate;
	{
		FScopeLock L(&Lock);
		Mono = MoveTemp(Samples);
		Samples.Reset();
		Rate = SampleRate;
	}
	float Peak = 0.f;
	for (float S : Mono) { Peak = FMath::Max(Peak, FMath::Abs(S)); }
	UE_LOG(LogJamVoice, Log, TEXT("grabado %.1f s a %d Hz, pico %.3f"), Mono.Num() / (float)FMath::Max(Rate, 1), Rate, Peak);
	if (Mono.Num() < Rate / 3)
	{
		Status.Reset();
		Done(false, TEXT("Voice recording too short. Hold G while speaking."));
		return;
	}
	if (Peak < 0.003f)
	{
		Status.Reset();
		Done(false, TEXT("No microphone audio detected. Check your input device or press B to type your request."));
		return;
	}
	TranscribeWav(MakeWav(Mono, Rate), MoveTemp(Done));
}

void FJamVoice::TranscribeWav(const TArray<uint8>& Wav, FDone InDone)
{
	if (!IsInGameThread())
	{
		AsyncTask(ENamedThreads::GameThread, [Wav, InDone = MoveTemp(InDone)]() mutable { TranscribeWav(Wav, MoveTemp(InDone)); });
		return;
	}
	FJamVoice& V = Get();
	if (V.bTranscribing || V.bRecording)
	{
		InDone(false, TEXT("Voice is busy recording or transcribing. Wait, or press B to type your request."));
		return;
	}
	V.bTranscribing = true;
	const double T0 = FPlatformTime::Seconds();
	// Todo termina aqui: guarda el resultado para el HUD y avisa.
	TSharedRef<FString> Backend = MakeShared<FString>(TEXT("none"));
	TSharedRef<bool> Finished = MakeShared<bool>(false);
	FDone Done = [InDone, T0, Backend, Finished](bool bOk, const FString& Text)
	{
		// Requests explicitly complete on the game thread. ProcessRequest failure may also finish immediately.
		if (*Finished) { return; }
		*Finished = true;
		FJamVoice& Me = Get();
		Me.bTranscribing = false;
		Me.Status.Reset();
		Me.bLastOk = bOk;
		Me.LastText = Text;
		Me.LastTime = FPlatformTime::Seconds();
		Me.LastSeconds = (float)(FPlatformTime::Seconds() - T0);
		Me.LastBackend = *Backend;
		UE_LOG(LogJamVoice, Log, TEXT("Voice transcription ok=%d backend=%s seconds=%.2f chars=%d"), bOk, **Backend, Me.LastSeconds, Text.Len());
		InDone(bOk, Text);
	};
	V.Status = TEXT("transcribiendo");
	FString WavWhy;
	if (!ValidateVoiceWav(Wav, WavWhy)) { Done(false, WavWhy); return; }

	// Respaldo en la nube (fal): subir el audio y transcribir. Tiempos limite cortos; el arranque en frio puede tardar.
	TFunction<void(const FString&)> Cloud = [Wav, Done, Backend, Finished](const FString& LocalWhy)
	{
		if (*Finished) { return; }
		const FString Key = FJamDecisionClient::GetFalKey();
		if (Key.IsEmpty())
		{
			Done(false, FString::Printf(TEXT("Voice: %s. No backup recognizer is available. Press B to type your request."), *LocalWhy));
			return;
		}
		*Backend = TEXT("fal");
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Init = Req(TEXT("POST"), TEXT("https://rest.alpha.fal.ai/storage/upload/initiate"), Key, TEXT("application/json"));
		Init->SetTimeout(5.f);
		Init->SetContentAsString(TEXT("{\"file_name\":\"voz.wav\",\"content_type\":\"audio/wav\"}"));
		Init->OnProcessRequestComplete().BindLambda([Done, Key, Wav, Finished](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
		{
			if (*Finished) { return; }
			TSharedPtr<FJsonObject> J;
			FString UploadUrl, FileUrl;
			if (!JsonOf(Resp, bOk, J) || !J->TryGetStringField(TEXT("upload_url"), UploadUrl) || !J->TryGetStringField(TEXT("file_url"), FileUrl))
			{
				Done(false, HttpWhy(TEXT("Voice upload"), Resp) + TEXT(". Press B to type your request."));
				return;
			}
			TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Up = Req(TEXT("PUT"), UploadUrl, FString(), TEXT("audio/wav"));
			Up->SetTimeout(5.f);
			Up->SetContent(Wav);
			Up->OnProcessRequestComplete().BindLambda([Done, Key, FileUrl, Finished](FHttpRequestPtr, FHttpResponsePtr UpResp, bool bUpOk)
			{
				if (*Finished) { return; }
				if (!bUpOk || !UpResp.IsValid() || UpResp->GetResponseCode() / 100 != 2)
				{
					Done(false, HttpWhy(TEXT("Audio upload"), UpResp) + TEXT(". Press B to type your request."));
					return;
				}
				TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
				Body->SetStringField(TEXT("audio_url"), FileUrl);
				Body->SetStringField(TEXT("task"), TEXT("transcribe"));
				FString BodyStr;
				FJsonSerializer::Serialize(Body, TJsonWriterFactory<>::Create(&BodyStr));
				TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Tr = Req(TEXT("POST"), TEXT("https://fal.run/fal-ai/wizper"), Key, TEXT("application/json"));
				Tr->SetTimeout(8.f);
				Tr->SetContentAsString(BodyStr);
				Tr->OnProcessRequestComplete().BindLambda([Done, Finished](FHttpRequestPtr, FHttpResponsePtr TrResp, bool bTrOk)
				{
					if (*Finished) { return; }
					TSharedPtr<FJsonObject> R;
					FString Text;
					if (!JsonOf(TrResp, bTrOk, R) || !R->TryGetStringField(TEXT("text"), Text))
					{
						Done(false, TEXT("Voice service timed out. Try again or press B to type your request."));
						return;
					}
					Text = CleanText(Text);
					Done(!Text.IsEmpty(), Text.IsEmpty() ? FString(TEXT("No words recognized. Try again or press B to type your request.")) : Text);
				});
				if (!Tr->ProcessRequest()) { Done(false, TEXT("Voice transcription could not start. Press B to type your request.")); }
			});
			if (!Up->ProcessRequest()) { Done(false, TEXT("Voice upload could not start. Press B to type your request.")); }
		});
		if (!Init->ProcessRequest()) { Done(false, TEXT("Voice upload could not start. Press B to type your request.")); }
	};

	TFunction<void(const FString&)> Fallback = [Wav, Done, Cloud, Backend, Finished](const FString& Why)
	{
		if (*Finished) { return; }
		if (!GLocalReady) { Cloud(Why); return; }
		*Backend = TEXT("local");
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Local = LocalRequest(Wav, 4.f);
		Local->OnProcessRequestComplete().BindLambda([Done, Cloud, Finished](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
		{
			if (*Finished) { return; }
			TSharedPtr<FJsonObject> R;
			FString Text;
			if (!JsonOf(Resp, bOk, R) || !R->TryGetStringField(TEXT("text"), Text))
			{
				Cloud(TEXT("local recognizer did not respond"));
				return;
			}
			Text = CleanText(Text);
			Done(!Text.IsEmpty(), Text.IsEmpty() ? FString(TEXT("No words recognized. Try again or press B to type your request.")) : Text);
		});
		if (!Local->ProcessRequest()) { Done(false, TEXT("Local voice could not start. Press B to type your request.")); }
	};

	// Voice shares the decision API proxy and Bearer token; never use fal's Key authorization helper.
	const FString OpenAIToken = FJamDecisionClient::GetOpenAIAuthToken();
	if (OpenAIToken.IsEmpty()) { Fallback(TEXT("OpenAI voice is not configured and the local recognizer is unavailable")); return; }
	*Backend = TEXT("openai-whisper");
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> OpenAI = OpenAIRequest(Wav, OpenAIToken);
	OpenAI->OnProcessRequestComplete().BindLambda([Done, Fallback, Finished](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
	{
		if (*Finished) { return; }
		TSharedPtr<FJsonObject> R;
		FString Text;
		if (!JsonOf(Resp, bOk, R) || !R->TryGetStringField(TEXT("text"), Text))
		{
			// Provider error bodies can contain request data. Only a numeric code or generic connection error leaves here.
			const FString Why = Resp.IsValid()
				? FString::Printf(TEXT("Whisper transcription failed (HTTP %d)"), Resp->GetResponseCode())
				: FString(TEXT("Whisper could not connect or timed out"));
			Fallback(Why);
			return;
		}
		Text = CleanText(Text);
		Done(!Text.IsEmpty(), Text.IsEmpty() ? FString(TEXT("No words recognized. Try again or press B to type your request.")) : Text);
	});
	if (!OpenAI->ProcessRequest()) { Done(false, TEXT("Whisper voice request could not start. Press B to type your request.")); }
}
