#include "Core/JamAudio.h"
#include "GameJamm.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Sound/SoundBase.h"

namespace JamAudio
{
	namespace
	{
		TMap<FString, TWeakObjectPtr<USoundBase>> GJamAudioCache;
		TSet<FString> GJamAudioLogged;
		TWeakObjectPtr<UAudioComponent> GJamMusic;
		FString GJamMusicName;

		bool JamAudioEnabled()
		{
			const IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("jam.Build.Sfx"));
			return !V || V->GetInt() != 0;
		}

		void JamAudioLogOnce(const FString& Name, bool bFound)
		{
			const FString Key = Name + (bFound ? TEXT("+") : TEXT("-"));
			if (!GJamAudioLogged.Contains(Key))
			{
				GJamAudioLogged.Add(Key);
				if (bFound) { UE_LOG(LogJam, Log, TEXT("JamSfx: suena %s"), *Name); }
				else { UE_LOG(LogJam, Warning, TEXT("JamSfx: falta %s (importar a /Game/Game/Building/Audio)"), *Name); }
			}
		}
	}

	USoundBase* Find(const TCHAR* Name)
	{
		if (!Name || !*Name || !JamAudioEnabled())
		{
			return nullptr;
		}
		if (const TWeakObjectPtr<USoundBase>* Hit = GJamAudioCache.Find(Name))
		{
			if (Hit->IsValid()) { return Hit->Get(); }
		}
		const FString Pkg = FString(TEXT("/Game/Game/Building/Audio/")) + Name;
		USoundBase* S = FPackageName::DoesPackageExist(Pkg) ? LoadObject<USoundBase>(nullptr, *(Pkg + TEXT(".") + Name)) : nullptr;
		if (S)
		{
			GJamAudioCache.Add(Name, S);
		}
		else
		{
			JamAudioLogOnce(Name, false);
		}
		return S;
	}

	void Play2D(const UObject* WorldContext, const TCHAR* Name, float Volume)
	{
		if (USoundBase* S = Find(Name))
		{
			UGameplayStatics::PlaySound2D(WorldContext, S, Volume);
			JamAudioLogOnce(Name, true);
		}
	}

	void PlayAt(const UObject* WorldContext, const TCHAR* Name, const FVector& Where, float Volume)
	{
		if (USoundBase* S = Find(Name))
		{
			UGameplayStatics::PlaySoundAtLocation(WorldContext, S, Where, Volume);
			JamAudioLogOnce(Name, true);
		}
	}

	void Music(const UObject* WorldContext, const TCHAR* Name, float Volume)
	{
		const FString Wanted = Name ? FString(Name) : FString();
		UAudioComponent* Cur = GJamMusic.Get();
		// La musica vive en un mundo: si cambio el mundo (otra partida en PIE) se empieza de nuevo.
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		if (Cur && (Cur->GetWorld() != World || !IsValid(Cur)))
		{
			Cur = nullptr;
			GJamMusicName.Reset();
		}
		if (Cur && GJamMusicName == Wanted)
		{
			return;
		}
		if (Cur)
		{
			Cur->FadeOut(1.5f, 0.f);
		}
		GJamMusic.Reset();
		GJamMusicName = Wanted;
		if (Wanted.IsEmpty())
		{
			return;
		}
		if (USoundBase* S = Find(*Wanted))
		{
			UAudioComponent* C = UGameplayStatics::SpawnSound2D(WorldContext, S, Volume, 1.f, 0.f, nullptr, false, true);
			if (C)
			{
				C->FadeIn(1.5f, Volume);
				GJamMusic = C;
				JamAudioLogOnce(Wanted, true);
			}
		}
	}
}
