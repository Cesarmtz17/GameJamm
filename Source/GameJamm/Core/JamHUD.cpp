#include "Core/JamHUD.h"
#include "Building/JamBuildPart.h"
#include "Building/JamBuildSpawner.h"
#include "Core/InventoryComponent.h"
#include "Core/JamLootChest.h"
#include "Core/JamMessages.h"
#include "Core/JamStatics.h"
#include "Core/JamGameMode.h"
#include "Core/JamGameState.h"
#include "Core/HealthComponent.h"
#include "Core/TeamStructure.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

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
namespace
{
	// T4: conectar con UJamWeaponComponent (el arma pedida a la IA). Mientras no exista, el paso del objetivo no se cumple solo.
	bool JamHudHasGeneratedWeapon(const APawn* Pawn)
	{
		(void)Pawn;
		return false;
	}

	/** El paso "pide un arma" solo se muestra si existe la variable jam.Weapon.Generated y vale distinto de 0. */
	bool JamHudWeaponStepEnabled()
	{
		const IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("jam.Weapon.Generated"));
		return Var && Var->GetInt() != 0;
	}

	/** true si el jugador ya pidio una construccion (AJamBuildSpawner con la etiqueta JamSay) y tiene piezas. */
	bool JamHudVehicleBuilt(UWorld* World)
	{
		static const FName SayTag(TEXT("JamSay"));
		for (TActorIterator<AJamBuildSpawner> It(World); It; ++It)
		{
			if (!It->ActorHasTag(SayTag))
			{
				continue;
			}
			for (const AJamBuildPart* Part : It->GetParts())
			{
				if (IsValid(Part))
				{
					return true;
				}
			}
		}
		return false;
	}
}

FString AJamHUD::UpdateObjective(UWorld* World, const APawn* Pawn, EJamTeam MyTeam)
{
	if (!World)
	{
		return FString();
	}
	const double Now = FPlatformTime::Seconds();
	if (Pawn && !bObjHaveStart)
	{
		bObjHaveStart = true;
		ObjStartLoc = Pawn->GetActorLocation();
		ObjFirstSeen = Now;
	}

	// Estructuras del otro equipo: cuantas cayeron y a que distancia esta la torre (no base) viva mas cercana.
	float NearestTower = TNumericLimits<float>::Max();
	int32 Down = 0;
	bool bTowerDown = false;
	for (TActorIterator<ATeamStructure> It(World); It; ++It)
	{
		const ATeamStructure* S = *It;
		const EJamTeam T = S->GetTeam();
		if (!S->Health || T == MyTeam || T == EJamTeam::Neutral)
		{
			continue;
		}
		if (S->Health->IsDead())
		{
			++Down;
			bTowerDown |= !S->bIsBase;
		}
		else if (!S->bIsBase && Pawn)
		{
			NearestTower = FMath::Min(NearestTower, static_cast<float>(FVector::Dist(S->GetActorLocation(), Pawn->GetActorLocation())));
		}
	}
	ObjTowersDownMax = FMath::Max(ObjTowersDownMax, Down);

	// Mochila: el personaje recibe StartingItems en BeginPlay, asi que se compara contra el mayor conteo de los primeros 2 s.
	int32 InvTotal = 0;
	if (const UInventoryComponent* Inv = Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr)
	{
		for (const TPair<FName, int32>& Item : Inv->Items)
		{
			InvTotal += FMath::Max(0, Item.Value);
		}
		if (Now - ObjFirstSeen < 2.0)
		{
			ObjInvBaseline = FMath::Max(ObjInvBaseline, InvTotal);
		}
	}

	// Cada paso se cumple por estado del juego y no retrocede.
	for (int32 Guard = 0; Guard < 6 && ObjStep < 4; ++Guard)
	{
		bool bDone = false;
		switch (ObjStep)
		{
		case 0: // cofre junto al spawn abierto, o la mochila crecio
			bDone = bObjHaveStart && Now - ObjFirstSeen > 2.0 && InvTotal > ObjInvBaseline;
			for (TActorIterator<AJamLootChest> It(World); It && !bDone; ++It)
			{
				bDone = It->IsOpen() && FVector::Dist(It->GetActorLocation(), ObjStartLoc) < 4000.f;
			}
			break;
		case 1: // arma pedida (solo si la funcion existe); si el jugador ya armo un vehiculo, el paso se da por superado
			if (!JamHudWeaponStepEnabled())
			{
				++ObjStep; // se salta sin aviso
				continue;
			}
			bDone = JamHudHasGeneratedWeapon(Pawn) || JamHudVehicleBuilt(World);
			break;
		case 2: // vehiculo pedido
			bDone = JamHudVehicleBuilt(World);
			break;
		case 3: // cerca de la torre enemiga mas cercana, o una ya cayo
			bDone = bTowerDown || NearestTower < 3000.f;
			break;
		default:
			break;
		}
		if (!bDone)
		{
			break;
		}
		++ObjStep;
		JamMsg::Big(TEXT("Hecho!"), JamMsg::Good, 2.5f, TEXT("JamObjDone"));
	}

	switch (ObjStep)
	{
	case 0: return TEXT("Pasa por el cofre junto a ti");
	case 1: return TEXT("Pulsa B y pide un arma. Prueba: un rifle que dispare fuego");
	case 2: return TEXT("Pulsa B y pide un vehiculo. Prueba: un carro que queme todo");
	case 3: return TEXT("C lo enciende, E te subes. Sigue a tus minions hasta la torre 1");
	default: return FString::Printf(TEXT("Derriba las 2 torres y la base rival (%d/3)"), FMath::Clamp(ObjTowersDownMax, 0, 3));
	}
}

float AJamHUD::DrawObjective(const FString& Text, float Y)
{
	UFont* Font = GEngine->GetMediumFont();
	const float SX = Canvas->SizeX;
	const float MaxW = FMath::Min(SX * 0.35f, 560.f * K);
	// Parte el texto en filas para que no llegue a los paneles de los lados.
	TArray<FString> Words;
	(TEXT("OBJETIVO: ") + Text).ParseIntoArray(Words, TEXT(" "), true);
	TArray<FString> Rows;
	FString Cur;
	for (const FString& Word : Words)
	{
		const FString Try = Cur.IsEmpty() ? Word : Cur + TEXT(" ") + Word;
		float TW = 0.f, TH = 0.f;
		GetTextSize(Try, TW, TH, Font, K);
		if (TW > MaxW && !Cur.IsEmpty())
		{
			Rows.Add(Cur);
			Cur = Word;
		}
		else
		{
			Cur = Try;
		}
	}
	if (!Cur.IsEmpty())
	{
		Rows.Add(Cur);
	}
	float RowW = 0.f, RowH = 0.f;
	for (const FString& Row : Rows)
	{
		float TW = 0.f, TH = 0.f;
		GetTextSize(Row, TW, TH, Font, K);
		RowW = FMath::Max(RowW, TW);
		RowH = FMath::Max(RowH, TH);
	}
	const float PanelH = Rows.Num() * RowH + 16.f * K;
	const float PX = (SX - RowW) * 0.5f - 16.f * K;
	DrawRect(JamPanel, PX, Y, RowW + 32.f * K, PanelH);
	DrawRect(JamGold, PX, Y, 4.f * K, PanelH);
	float Ty = Y + 8.f * K;
	for (const FString& Row : Rows)
	{
		float TW = 0.f, TH = 0.f;
		GetTextSize(Row, TW, TH, Font, K);
		DrawText(Row, JamText, (SX - TW) * 0.5f, Ty, Font, K);
		Ty += RowH;
	}
	return Y + PanelH;
}

void AJamHUD::DrawBigs(float Y)
{
	UFont* Font = GEngine->GetLargeFont();
	const double Now = FPlatformTime::Seconds();
	for (const JamMsg::FEntry& E : JamMsg::Bigs())
	{
		const float Alpha = FMath::Clamp(static_cast<float>(E.Expire - Now) / 0.5f, 0.f, 1.f);
		float TW = 0.f, TH = 0.f;
		GetTextSize(E.Text, TW, TH, Font, K);
		const float X = (Canvas->SizeX - TW) * 0.5f;
		FLinearColor Bg = JamPanel;
		Bg.A *= Alpha;
		FLinearColor Fg = E.Color;
		Fg.A *= Alpha;
		DrawRect(Bg, X - 18.f * K, Y - 6.f * K, TW + 36.f * K, TH + 12.f * K);
		DrawText(E.Text, Fg, X, Y, Font, K);
		Y += TH + 22.f * K;
	}
}

void AJamHUD::DrawPostLines(float BaseY)
{
	UFont* Font = GEngine->GetMediumFont();
	const double Now = FPlatformTime::Seconds();
	const TArray<JamMsg::FEntry>& Lines = JamMsg::Lines();
	float Bottom = BaseY;
	for (int32 i = Lines.Num() - 1; i >= 0; --i) // la mas nueva abajo
	{
		const JamMsg::FEntry& E = Lines[i];
		const float Alpha = FMath::Clamp(static_cast<float>(E.Expire - Now) / 0.5f, 0.f, 1.f);
		float TW = 0.f, TH = 0.f;
		GetTextSize(E.Text, TW, TH, Font, K);
		const float X = (Canvas->SizeX - TW) * 0.5f, Y = Bottom - TH - 8.f * K;
		FLinearColor Bg = JamPanel;
		Bg.A *= Alpha;
		FLinearColor Fg = E.Color;
		Fg.A *= Alpha;
		DrawRect(Bg, X - 14.f * K, Y - 4.f * K, TW + 28.f * K, TH + 8.f * K);
		DrawText(E.Text, Fg, X, Y, Font, K);
		Bottom = Y - 10.f * K;
	}
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

	// 5) Objetivo (arriba al centro) y avisos: durante la partida, no en la pantalla final.
	const bool bOver = GS && GS->IsMatchOver();
	if (!bOver)
	{
		EJamTeam ObjTeam = Pawn ? UJamStatics::GetActorTeam(Pawn) : MyTeam;
		if (ObjTeam == EJamTeam::Neutral)
		{
			ObjTeam = MyTeam;
		}
		const FString Obj = UpdateObjective(World, Pawn, ObjTeam);
		const float ObjBottom = Obj.IsEmpty() ? 14.f * K : DrawObjective(Obj, 14.f * K);
		DrawBigs(ObjBottom + 12.f * K);
	}
	DrawPostLines(MsgBaseY > 0.f ? MsgBaseY : SY - 118.f * K);

	// 6) Fin de partida.
	if (bOver)
	{
		const EJamTeam Winner = GS->GetWinningTeam();
		DrawRect(FLinearColor(0, 0, 0, 0.45f), 0, 0, SX, SY);
		const FString Title = Winner == EJamTeam::Neutral ? TEXT("EMPATE") : (Winner == MyTeam ? TEXT("VICTORIA") : TEXT("DERROTA"));
		DrawCentered(Title, Winner == EJamTeam::Neutral ? JamText : (Winner == MyTeam ? JamGold : JamRed), SY * 0.36f, 3.2f * K);
		DrawCentered(Winner == EJamTeam::Neutral ? TEXT("Las dos bases quedaron igual") :
			FString::Printf(TEXT("Gana el equipo %s"), Winner == EJamTeam::Blue ? TEXT("AZUL") : TEXT("ROJO")), TeamColor(Winner), SY * 0.5f, 1.4f * K, false);
	}
}
