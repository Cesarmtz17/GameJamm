#include "Core/JamMessages.h"
#include "GameJamm.h"
#include "HAL/PlatformTime.h"

namespace JamMsg
{
	const FLinearColor Info(0.95f, 0.95f, 0.92f, 1.f);
	const FLinearColor Good(0.45f, 0.9f, 0.5f, 1.f);
	const FLinearColor Warn(1.f, 0.7f, 0.25f, 1.f);
	const FLinearColor Bad(0.95f, 0.3f, 0.3f, 1.f);
	const FLinearColor Loot(1.f, 0.82f, 0.35f, 1.f);

	namespace
	{
		constexpr int32 MaxLines = 4;
		constexpr int32 MaxBigs = 2;
		TArray<FEntry> GLines;
		TArray<FEntry> GBigs;

		void Push(TArray<FEntry>& Queue, int32 Max, const FString& Text, const FLinearColor& Color, float Seconds, FName Key)
		{
			const FString Clean = Text.TrimStartAndEnd();
			if (Clean.IsEmpty())
			{
				return;
			}
			const double Now = FPlatformTime::Seconds();
			FEntry E;
			E.Text = Clean;
			E.Color = Color;
			E.Born = Now;
			E.Expire = Now + FMath::Max(0.5f, Seconds);
			E.Key = Key;
			// Misma Key: reemplaza en su lugar. Mismo texto aun vivo: solo se renueva (no se apila "mochila llena" x4).
			for (FEntry& Old : Queue)
			{
				if ((!Key.IsNone() && Old.Key == Key) || Old.Text == Clean)
				{
					Old = E;
					UE_LOG(LogJam, Log, TEXT("JamMsg: %s"), *Clean);
					return;
				}
			}
			Queue.Add(E);
			while (Queue.Num() > Max)
			{
				Queue.RemoveAt(0);
			}
			UE_LOG(LogJam, Log, TEXT("JamMsg: %s"), *Clean);
		}

		const TArray<FEntry>& Alive(TArray<FEntry>& Queue)
		{
			const double Now = FPlatformTime::Seconds();
			Queue.RemoveAll([Now](const FEntry& E) { return E.Expire <= Now; });
			return Queue;
		}
	}

	void Post(const FString& Text, const FLinearColor& Color, float Seconds, FName Key)
	{
		Push(GLines, MaxLines, Text, Color, Seconds, Key);
	}

	void Big(const FString& Text, const FLinearColor& Color, float Seconds, FName Key)
	{
		Push(GBigs, MaxBigs, Text, Color, Seconds, Key);
	}

	void Remove(FName Key)
	{
		if (Key.IsNone())
		{
			return;
		}
		GLines.RemoveAll([Key](const FEntry& E) { return E.Key == Key; });
		GBigs.RemoveAll([Key](const FEntry& E) { return E.Key == Key; });
	}

	const TArray<FEntry>& Lines() { return Alive(GLines); }
	const TArray<FEntry>& Bigs() { return Alive(GBigs); }

	void Clear()
	{
		GLines.Reset();
		GBigs.Reset();
	}
}
