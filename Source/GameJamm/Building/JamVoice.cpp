#include "Building/JamVoice.h"

#include "AudioCaptureCore.h"
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
		return R;
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
		FString V = TEXT("Construye: un carro con cohete");
		for (const FJamArchetypeDef& A : UJamBuildGenerator::ArchetypeTable())
		{
			if (A.Keywords.Num() > 0) { V += TEXT(", ") + A.Keywords[0].TrimStartAndEnd(); }
		}
		for (const FJamExtraDef& E : UJamBuildGenerator::ExtraTable())
		{
			if (E.Keywords.Num() > 0) { V += TEXT(", ") + E.Keywords[0].TrimStartAndEnd(); }
		}
		return V + TEXT(", láser, lanzallamas, hovercraft, globo aerostático, cañón, avión, dron.");
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
		Field(TEXT("language"), TEXT("es"));
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
			if (W.ToLower().Contains(TEXT("vercraft")) && !W.ToLower().Contains(TEXT("hovercraft"))) { W = TEXT("un hovercraft"); }
		}
		Text = FString::Join(Words, TEXT(" "));
		// Ruido tipico del reconocedor con silencio: corchetes o parentesis.
		if (Text.StartsWith(TEXT("[")) || Text.StartsWith(TEXT("("))) { Text.Reset(); }
		return Text;
	}
}

void FJamVoice::WarmUp()
{
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
				const FString Args = FString::Printf(TEXT("-m \"%s\" -l es -bs 1 -nt --host 127.0.0.1 --port 8177"), *Model);
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
	if (bRecording)
	{
		return true;
	}
	if (!GLocalReady && FJamDecisionClient::GetFalKey().IsEmpty())
	{
		OutError = TEXT("Voice is unavailable: no local recognizer or fal key. Press B to type your request.");
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
	FJamVoice& V = Get();
	const double T0 = FPlatformTime::Seconds();
	// Todo termina aqui: guarda el resultado para el HUD y avisa.
	TSharedRef<FString> Backend = MakeShared<FString>(TEXT("local"));
	FDone Done = [InDone, T0, Backend](bool bOk, const FString& Text)
	{
		FJamVoice& Me = Get();
		Me.Status.Reset();
		Me.bLastOk = bOk;
		Me.LastText = Text;
		Me.LastTime = FPlatformTime::Seconds();
		Me.LastSeconds = (float)(FPlatformTime::Seconds() - T0);
		Me.LastBackend = *Backend;
		UE_LOG(LogJamVoice, Log, TEXT("transcripcion ok=%d backend=%s en %.2f s: %s"), bOk, **Backend, Me.LastSeconds, *Text);
		InDone(bOk, Text);
	};
	V.Status = TEXT("transcribiendo");

	// Respaldo en la nube (fal): subir el audio y transcribir. Tiempos limite cortos; el arranque en frio puede tardar.
	TFunction<void(const FString&)> Cloud = [Wav, Done, Backend](const FString& LocalWhy)
	{
		*Backend = TEXT("nube");
		const FString Key = FJamDecisionClient::GetFalKey();
		if (Key.IsEmpty())
		{
			Done(false, FString::Printf(TEXT("Voice: %s and no fal key is configured. Press B to type your request."), *LocalWhy));
			return;
		}
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Init = Req(TEXT("POST"), TEXT("https://rest.alpha.fal.ai/storage/upload/initiate"), Key, TEXT("application/json"));
		Init->SetTimeout(5.f);
		Init->SetContentAsString(TEXT("{\"file_name\":\"voz.wav\",\"content_type\":\"audio/wav\"}"));
		Init->OnProcessRequestComplete().BindLambda([Done, Key, Wav](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
		{
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
			Up->OnProcessRequestComplete().BindLambda([Done, Key, FileUrl](FHttpRequestPtr, FHttpResponsePtr UpResp, bool bUpOk)
			{
				if (!bUpOk || !UpResp.IsValid() || UpResp->GetResponseCode() / 100 != 2)
				{
					Done(false, HttpWhy(TEXT("Audio upload"), UpResp) + TEXT(". Press B to type your request."));
					return;
				}
				TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
				Body->SetStringField(TEXT("audio_url"), FileUrl);
				Body->SetStringField(TEXT("task"), TEXT("transcribe"));
				Body->SetStringField(TEXT("language"), TEXT("es"));
				FString BodyStr;
				FJsonSerializer::Serialize(Body, TJsonWriterFactory<>::Create(&BodyStr));
				TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Tr = Req(TEXT("POST"), TEXT("https://fal.run/fal-ai/wizper"), Key, TEXT("application/json"));
				Tr->SetTimeout(8.f);
				Tr->SetContentAsString(BodyStr);
				Tr->OnProcessRequestComplete().BindLambda([Done](FHttpRequestPtr, FHttpResponsePtr TrResp, bool bTrOk)
				{
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
				Tr->ProcessRequest();
			});
			Up->ProcessRequest();
		});
		Init->ProcessRequest();
	};

	// 1) Local: whisper-server. Si no esta o falla, a la nube.
	if (!GLocalReady)
	{
		Cloud(TEXT("local recognizer unavailable"));
		return;
	}
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Local = LocalRequest(Wav, 4.f);
	Local->OnProcessRequestComplete().BindLambda([Done, Cloud](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
	{
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
	Local->ProcessRequest();
}
