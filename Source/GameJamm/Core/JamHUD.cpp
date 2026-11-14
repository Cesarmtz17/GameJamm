#include "Core/JamHUD.h"
#include "Core/JamGameMode.h"
#include "Core/JamGameState.h"
#include "Core/HealthComponent.h"
#include "Core/TeamStructure.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

namespace
{
	const FLinearColor JamRed(0.85f, 0.22f, 0.24f, 1.f);
	const FLinearColor JamBlue(0.25f, 0.5f, 0.95f, 1.f);
	const FLinearColor JamPanel(0.04f, 0.05f, 0.07f, 0.72f);
	const FLinearColor JamText(0.95f, 0.95f, 0.92f, 1.f);
	const FLinearColor JamGold(1.f, 0.82f, 0.35f, 1.f);

	FLinearColor TeamColor(EJamTeam Team) { return Team == EJamTeam::Blue ? JamBlue : (Team == EJamTeam::Red ? JamRed : JamText); }
}

void AJamHUD::DrawCentered(const FString& Text, const FLinearColor& Color, float Y, float Scale, bool bBackground)
{
	UFont* Font = GEngine->GetLargeFont();
	float TW = 0.f, TH = 0.f;
	GetTextSize(Text, TW, TH, Font, Scale);
	const float X = (Canvas->SizeX - TW) * 0.5f;
	if (bBackground)
	{
		DrawRect(JamPanel, X - 18.f * K, Y - 8.f * K, TW + 36.f * K, TH + 16.f * K);
	}
	DrawText(Text, Color, X, Y, Font, Scale);
}

void AJamHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !GEngine)
	{
		return;
	}
	UWorld* World = GetWorld();
	const AJamGameState* GS = World ? World->GetGameState<AJamGameState>() : nullptr;
	APlayerController* PC = GetOwningPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const UHealthComponent* MyHealth = Pawn ? Pawn->FindComponentByClass<UHealthComponent>() : nullptr;
	const EJamTeam MyTeam = MyHealth ? MyHealth->Team : EJamTeam::Red;
	const float SX = Canvas->SizeX, SY = Canvas->SizeY;
	UFont* Small = GEngine->GetSmallFont();
	UFont* Medium = GEngine->GetMediumFont();

	// 1) Barras sobre las torres y bases cercanas (60 m).
	float TeamHP[3] = { 0.f, 0.f, 0.f }, TeamMax[3] = { 0.f, 0.f, 0.f };
	for (TActorIterator<ATeamStructure> It(World); It; ++It)
	{
		const ATeamStructure* S = *It;
		if (!S->Health)
		{
			continue;
		}
		const int32 T = static_cast<int32>(S->GetTeam());
		TeamHP[T] += S->Health->GetHealth();
		TeamMax[T] += S->Health->MaxHealth;
		if (S->Health->IsDead() || !Pawn || FVector::Dist(S->GetActorLocation(), Pawn->GetActorLocation()) > 6000.f)
		{
			continue;
		}
		FVector Origin, Extent;
		S->GetActorBounds(true, Origin, Extent);
		const FVector Top = Origin + FVector(0, 0, Extent.Z + 80.f);
		const FVector P = Project(Top);
		if (P.Z <= 0.f)
		{
			continue;
		}
		const float W = (S->bIsBase ? 200.f : 150.f) * K, H = 10.f * K;
		DrawRect(JamPanel, P.X - W * 0.5f - 2 * K, P.Y - 2 * K, W + 4 * K, H + 4 * K);
		DrawRect(TeamColor(S->GetTeam()), P.X - W * 0.5f, P.Y, W * S->Health->GetHealthPercent(), H);
		const FString Label = FString::Printf(TEXT("%s %.0f%s"), S->bIsBase ? TEXT("BASE") : TEXT("TORRE"), S->Health->GetHealth(),
			S->Health->bInvulnerable ? TEXT("  (protegida)") : TEXT(""));
		DrawText(Label, JamText, P.X - W * 0.5f, P.Y - 16.f * K, Small, K);
	}

	// 2) Reloj y vida de cada equipo (arriba a la izquierda).
	if (GS && GS->bMatchRunning)
	{
		const int32 Secs = FMath::FloorToInt(GS->MatchSeconds);
		const float X = 14.f * K, Y = 14.f * K, W = 260.f * K;
		DrawRect(JamPanel, X, Y, W, 84.f * K);
		DrawText(FString::Printf(TEXT("%02d:%02d%s"), Secs / 60, Secs % 60, GS->bSuddenDeath ? TEXT("   MUERTE SUBITA") : TEXT("")),
			GS->bSuddenDeath ? JamGold : JamText, X + 10 * K, Y + 6 * K, Medium, K);
		const EJamTeam Teams[2] = { EJamTeam::Red, EJamTeam::Blue };
		for (int32 i = 0; i < 2; ++i)
		{
			const int32 T = static_cast<int32>(Teams[i]);
			const float BY = Y + 34.f * K + i * 22.f * K, BW = W - 90.f * K;
			DrawText(Teams[i] == EJamTeam::Red ? TEXT("ROJO") : TEXT("AZUL"), TeamColor(Teams[i]), X + 10 * K, BY, Small, K);
			DrawRect(FLinearColor(0.15f, 0.17f, 0.2f, 1.f), X + 70 * K, BY + 2 * K, BW, 12.f * K);
			DrawRect(TeamColor(Teams[i]), X + 70 * K, BY + 2 * K, TeamMax[T] > 0.f ? BW * TeamHP[T] / TeamMax[T] : 0.f, 12.f * K);
		}
	}

	// 3) Vida del jugador (abajo al centro) y destello rojo al recibir dano.
	if (MyHealth)
	{
		const float HP = MyHealth->GetHealth();
		if (LastHealth >= 0.f && HP < LastHealth)
		{
			HurtFlash = 1.f;
		}
		LastHealth = HP;
		const float W = 360.f * K, H = 18.f * K, X = (SX - W) * 0.5f, Y = SY - 52.f * K;
		DrawRect(JamPanel, X - 6 * K, Y - 6 * K, W + 12 * K, H + 30 * K);
		DrawRect(FLinearColor(0.15f, 0.17f, 0.2f, 1.f), X, Y, W, H);
		DrawRect(MyHealth->GetHealthPercent() > 0.3f ? FLinearColor(0.35f, 0.85f, 0.4f, 1.f) : JamRed, X, Y, W * MyHealth->GetHealthPercent(), H);
		DrawText(FString::Printf(TEXT("VIDA %.0f / %.0f   equipo %s"), HP, MyHealth->MaxHealth, MyTeam == EJamTeam::Blue ? TEXT("AZUL") : TEXT("ROJO")),
			JamText, X, Y + H + 2 * K, Small, K);
	}
	if (HurtFlash > 0.f)
	{
		DrawRect(FLinearColor(0.8f, 0.05f, 0.05f, 0.25f * HurtFlash), 0, 0, SX, SY);
		HurtFlash = FMath::Max(0.f, HurtFlash - (World ? World->GetDeltaSeconds() : 0.016f) * 2.5f);
	}

	// 4) Muerto: cuenta para reaparecer.
	if (MyHealth && MyHealth->IsDead() && !(GS && GS->IsMatchOver()))
	{
		const AJamGameMode* GM = World->GetAuthGameMode<AJamGameMode>();
		const float Left = GM ? GM->GetRespawnRemaining(PC) : 0.f;
		DrawCentered(FString::Printf(TEXT("CAISTE  -  reapareces en tu base en %d s"), FMath::CeilToInt(Left)), JamRed, SY * 0.4f, 1.6f * K);
	}

	// 5) Fin de partida.
	if (GS && GS->IsMatchOver())
	{
		const EJamTeam Winner = GS->GetWinningTeam();
		DrawRect(FLinearColor(0, 0, 0, 0.45f), 0, 0, SX, SY);
		const FString Title = Winner == EJamTeam::Neutral ? TEXT("EMPATE") : (Winner == MyTeam ? TEXT("VICTORIA") : TEXT("DERROTA"));
		DrawCentered(Title, Winner == EJamTeam::Neutral ? JamText : (Winner == MyTeam ? JamGold : JamRed), SY * 0.36f, 3.2f * K);
		DrawCentered(Winner == EJamTeam::Neutral ? TEXT("Las dos bases quedaron igual") :
			FString::Printf(TEXT("Gana el equipo %s"), Winner == EJamTeam::Blue ? TEXT("AZUL") : TEXT("ROJO")), TeamColor(Winner), SY * 0.5f, 1.4f * K, false);
	}
}
