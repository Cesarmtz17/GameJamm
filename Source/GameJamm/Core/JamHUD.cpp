#include "Core/JamHUD.h"
#include "Combat/JamWeaponComponent.h"
#include "Building/JamBuildPart.h"
#include "Building/JamBuildSpawner.h"
#include "Core/InventoryComponent.h"
#include "Core/JamLootChest.h"
#include "Core/JamLoot.h"
#include "Core/JamPickup.h"
#include "Core/JamMessages.h"
#include "Core/JamStatics.h"
#include "Core/JamGameMode.h"
#include "Core/JamGameState.h"
#include "Core/HealthComponent.h"
#include "Core/TeamStructure.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectIterator.h"

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
	/** true si el jugador ya tiene un arma de mano pedida (T4, UJamWeaponComponent). */
	bool JamHudHasGeneratedWeapon(const APawn* Pawn)
	{
		const UJamWeaponComponent* Weapon = Pawn ? Pawn->FindComponentByClass<UJamWeaponComponent>() : nullptr;
		return Weapon && Weapon->HasGeneratedWeapon();
	}

	/** Arma en mano para el HUD (GDD 9): nombre, efecto y gasto por disparo del arma pedida; espada o arco si no. */
	FString JamHudWeaponLine(const APawn* Pawn, bool& bOutLowEnergy)
	{
		bOutLowEnergy = false;
		const UJamWeaponComponent* Weapon = Pawn ? Pawn->FindComponentByClass<UJamWeaponComponent>() : nullptr;
		if (!Weapon)
		{
			return FString();
		}
		const bool bCanCycle = Weapon->HasGeneratedWeapon() && UJamWeaponComponent::IsGeneratedEnabled();
		if (Weapon->Current == EJamWeapon::Generated && Weapon->HasGeneratedWeapon())
		{
			const float Cost = Weapon->GetGeneratedEnergyCost();
			bOutLowEnergy = AJamBuildPart::EnergyMode() != 0 && Cost > 0.f && AJamBuildPart::GetPlayerEnergy(Pawn->GetWorld()) < Cost;
			return TEXT("EN MANO: ") + Weapon->GetGeneratedHudText() + (bOutLowEnergy ? TEXT("   SIN ENERGIA") : TEXT("")) + TEXT("   (Q cambia)");
		}
		const FName Fused = Weapon->GetFused();
		FString Line = Weapon->Current == EJamWeapon::Bow ? TEXT("EN MANO: Arco") : TEXT("EN MANO: Espada");
		if (!Fused.IsNone())
		{
			Line += FString::Printf(TEXT(" + %s"), *Fused.ToString());
		}
		return Line + (bCanCycle ? TEXT("   (Q: tu arma pedida)") : TEXT("   (Q cambia)"));
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

namespace
{
	/** true si la caja de texto (T/B/Enter) esta abierta: ahi se escribe y la R no debe reiniciar la partida. */
	bool JamHudEndTyping(const UWorld* World)
	{
		for (TObjectIterator<UJamSayWidget> It; It; ++It)
		{
			if (!It->HasAnyFlags(RF_ClassDefaultObject) && It->GetWorld() == World && It->IsInViewport())
			{
				return true;
			}
		}
		return false;
	}
}

void AJamHUD::TrackCreations(UWorld* World, const APawn* Pawn, float DeltaSeconds)
{
	if (!World || DeltaSeconds <= 0.f)
	{
		return;
	}
	const float Dt = FMath::Min(DeltaSeconds, 0.25f);
	static const FName SayTag(TEXT("JamSay"));
	// Si el jugador va montado, su peon esta pegado a una pieza de la construccion.
	const AJamBuildPart* Mounted = Pawn ? Cast<AJamBuildPart>(Pawn->GetAttachParentActor()) : nullptr;
	for (TActorIterator<AJamBuildSpawner> It(World); It; ++It)
	{
		const AJamBuildSpawner* Spawner = *It;
		if (!Spawner->ActorHasTag(SayTag) || Spawner->Prompt.IsEmpty())
		{
			continue;
		}
		const int32 Alive = Spawner->CountAliveParts();
		if (Alive <= 0)
		{
			continue;
		}
		FJamHudCreation& C = Creations.FindOrAdd(Spawner->Prompt);
		C.MaxParts = FMath::Max(C.MaxParts, Alive);
		if (Spawner->IsBuildActive())
		{
			C.SecsActive += Dt;
		}
		if (Mounted)
		{
			bool bMine = false;
			for (const AJamBuildPart* Part : Spawner->GetParts())
			{
				if (IsValid(Part) && (Part == Mounted || (Part->Build.IsValid() && Part->Build == Mounted->Build)))
				{
					bMine = true;
					break;
				}
			}
			if (bMine)
			{
				C.SecsDriven += Dt;
			}
		}
	}
}

void AJamHUD::DrawFinalCut(EJamTeam Winner, EJamTeam MyTeam)
{
	const float SX = Canvas->SizeX, SY = Canvas->SizeY;
	UFont* Font = GEngine->GetLargeFont();
	auto HeightOf = [this, Font](const FString& Text, float Scale)
	{
		float TW = 0.f, TH = 0.f;
		GetTextSize(Text, TW, TH, Font, Scale);
		return TH;
	};
	// Recorta con "..." hasta que el texto quepa en el 86% del ancho.
	auto Fit = [this, Font, SX](FString Text, float Scale)
	{
		float TW = 0.f, TH = 0.f;
		GetTextSize(Text, TW, TH, Font, Scale);
		if (TW <= SX * 0.86f)
		{
			return Text;
		}
		while (Text.Len() > 4 && TW > SX * 0.86f)
		{
			Text = Text.Left(Text.Len() - 1);
			GetTextSize(Text + TEXT("..."), TW, TH, Font, Scale);
		}
		return Text + TEXT("...");
	};

	DrawRect(FLinearColor(0, 0, 0, 0.55f), 0, 0, SX, SY);
	float Y = SY * 0.08f;

	// Resultado.
	const FString Title = Winner == EJamTeam::Neutral ? TEXT("EMPATE") : (Winner == MyTeam ? TEXT("VICTORIA") : TEXT("DERROTA"));
	const float TitleScale = 3.2f * K;
	DrawCentered(Title, Winner == EJamTeam::Neutral ? JamText : (Winner == MyTeam ? JamGold : JamRed), Y, TitleScale);
	Y += HeightOf(Title, TitleScale) + 22.f * K;
	const FString WinLine = Winner == EJamTeam::Neutral ? FString(TEXT("Las dos bases quedaron igual")) :
		FString::Printf(TEXT("Gana el equipo %s"), Winner == EJamTeam::Blue ? TEXT("AZUL") : TEXT("ROJO"));
	DrawCentered(WinLine, TeamColor(Winner), Y, 1.4f * K, false);
	Y += HeightOf(WinLine, 1.4f * K) + 34.f * K;

	// Tu corte final: la creacion con mas puntaje (montado * 2 + encendida).
	const FString CutTitle(TEXT("TU CORTE FINAL"));
	DrawCentered(CutTitle, JamGold, Y, 1.3f * K);
	Y += HeightOf(CutTitle, 1.3f * K) + 24.f * K;
	const FString* BestPrompt = nullptr;
	const FJamHudCreation* Best = nullptr;
	for (const TPair<FString, FJamHudCreation>& Pair : Creations)
	{
		if (!Best || Pair.Value.Score() > Best->Score())
		{
			Best = &Pair.Value;
			BestPrompt = &Pair.Key;
		}
	}
	if (Best && BestPrompt)
	{
		const FString Quote = TEXT("\"") + Fit(*BestPrompt, 1.3f * K) + TEXT("\"");
		DrawCentered(Quote, JamText, Y, 1.3f * K);
		Y += HeightOf(Quote, 1.3f * K) + 22.f * K;
		const FString Stats = FString::Printf(TEXT("%d %s  -  %d s encendida, %d s manejando"), Best->MaxParts, Best->MaxParts == 1 ? TEXT("pieza") : TEXT("piezas"),
			FMath::RoundToInt(Best->SecsActive), FMath::RoundToInt(Best->SecsDriven));
		DrawCentered(Stats, JamText, Y, 1.0f * K, false);
		Y += HeightOf(Stats, 1.0f * K) + 20.f * K;
	}
	else
	{
		const FString None(TEXT("No construiste nada esta vez: pulsa B y pide algo la proxima"));
		DrawCentered(Fit(None, 1.1f * K), JamText, Y, 1.1f * K, false);
		Y += HeightOf(None, 1.1f * K) + 20.f * K;
	}

	// Revancha (late suave para que se note).
	const double Now = FPlatformTime::Seconds();
	const float Pulse = 0.8f + 0.2f * FMath::Sin(static_cast<float>(Now) * 4.f);
	const FString Again = bRestartRequested ? FString(TEXT("Reiniciando...")) : FString(TEXT("R = revancha"));
	DrawCentered(Again, FLinearColor(JamGold.R * Pulse, JamGold.G * Pulse, JamGold.B * Pulse, 1.f), FMath::Max(Y + 20.f * K, SY * 0.76f), 2.2f * K);
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

bool AJamHUD::ShowLoot(AActor* Taker, const TMap<FName, int32>& Offered, const TMap<FName, int32>& Leftover,
	const FString& SourceTier, float EnergyBefore)
{
	const APawn* Pawn = Cast<APawn>(Taker);
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	AJamHUD* HUD = PC && PC->IsLocalController() ? Cast<AJamHUD>(PC->GetHUD()) : nullptr;
	if (!HUD) { return false; }

	TArray<FName> Ids;
	Offered.GetKeys(Ids);
	Ids.Sort([](const FName& A, const FName& B) { return JamLoot::ItemName(A) < JamLoot::ItemName(B); });
	for (const FName Id : Ids)
	{
		const int32 Count = Id == TEXT("Energy")
			? FMath::Max(0, FMath::RoundToInt(AJamBuildPart::GetPlayerEnergy(Taker->GetWorld()) - EnergyBefore))
			: FMath::Max(0, Offered.FindRef(Id) - Leftover.FindRef(Id));
		if (Count <= 0) { continue; }
		FJamHudLootNotice& Notice = HUD->LootNotices.AddDefaulted_GetRef();
		Notice.ItemId = Id;
		Notice.Count = Count;
		Notice.Tier = SourceTier == TEXT("common") || SourceTier == TEXT("device") || SourceTier == TEXT("rare")
			? SourceTier : JamLoot::ItemTier(Id);
	}
	if (Leftover.Num() > 0)
	{
		const double Now = FPlatformTime::Seconds();
		if (Now >= HUD->BackpackFlashUntil) { HUD->BlockedLootItems.Reset(); }
		HUD->BackpackFlashUntil = Now + 4.0;
		for (const auto& Item : Leftover) { if (Item.Value > 0) { HUD->BlockedLootItems.Add(Item.Key); } }
	}
	return true;
}

FString AJamHUD::FitLootText(FString Text, UFont* Font, float Scale, float MaxWidth) const
{
	float W = 0.f, H = 0.f;
	GetTextSize(Text, W, H, Font, Scale);
	if (W <= MaxWidth) { return Text; }
	while (Text.Len() > 1)
	{
		Text.LeftChopInline(1);
		GetTextSize(Text + TEXT("..."), W, H, Font, Scale);
		if (W <= MaxWidth) { break; }
	}
	return Text + TEXT("...");
}

UTexture2D* AJamHUD::GetLootIcon(FName ItemId)
{
	if (const TObjectPtr<UTexture2D>* Found = LootIcons.Find(ItemId)) { return Found->Get(); }
	const FString Name = JamLoot::IconName(ItemId);
	const FString Path = FString::Printf(TEXT("/Game/Game/UI/Icons/%s.%s"), *Name, *Name);
	UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	LootIcons.Add(ItemId, Texture);
	return Texture;
}

void AJamHUD::DrawLootIcon(FName ItemId, float X, float Y, float Size, float Alpha)
{
	if (UTexture2D* Texture = GetLootIcon(ItemId))
	{
		DrawTexture(Texture, X, Y, Size, Size, 0.f, 0.f, 1.f, 1.f, FLinearColor(1.f, 1.f, 1.f, Alpha));
		return;
	}
	FLinearColor Color = JamLoot::ItemColor(ItemId);
	Color.A = Alpha;
	DrawRect(Color, X + 2.f * K, Y + 2.f * K, Size - 4.f * K, Size - 4.f * K);
	const FString Initial = JamLoot::ItemName(ItemId).Left(1).ToUpper();
	float W = 0.f, H = 0.f;
	const float Scale = K * 1.1f;
	GetTextSize(Initial, W, H, GEngine->GetMediumFont(), Scale);
	DrawText(Initial, FLinearColor(0.02f, 0.03f, 0.04f, Alpha), X + (Size - W) * 0.5f, Y + (Size - H) * 0.5f,
		GEngine->GetMediumFont(), Scale);
}

void AJamHUD::DrawLootNotices(float Bottom)
{
	const double Now = FPlatformTime::Seconds();
	LootNotices.RemoveAll([Now](const FJamHudLootNotice& N) { return N.Started >= 0.0 && Now - N.Started >= 4.0; });
	const float RowH = 50.f * K, Gap = 5.f * K;
	const float Top = FMath::Max(124.f * K, Canvas->SizeY * 0.24f);
	const int32 Slots = FMath::Max(1, FMath::FloorToInt((Bottom - Top) / (RowH + Gap)));
	const int32 Visible = FMath::Min(Slots, LootNotices.Num());
	const float Width = FMath::Min(300.f * K, Canvas->SizeX * 0.3f);
	UFont* Font = GEngine->GetSmallFont();
	for (int32 i = 0; i < Visible; ++i)
	{
		FJamHudLootNotice& N = LootNotices[i];
		if (N.Started < 0.0) { N.Started = Now; }
		const float Age = static_cast<float>(Now - N.Started);
		const float Alpha = FMath::Min(FMath::Clamp(Age / 0.12f, 0.f, 1.f), FMath::Clamp((4.f - Age) / 0.65f, 0.f, 1.f));
		const float X = 18.f * K - 10.f * K * (1.f - FMath::Clamp(Age / 0.12f, 0.f, 1.f));
		const float Y = Top + i * (RowH + Gap);
		FLinearColor Bg = JamPanel; Bg.A = 0.9f * Alpha;
		FLinearColor Accent = JamLoot::TierColor(N.Tier); Accent.A = Alpha;
		DrawRect(Bg, X, Y, Width, RowH);
		DrawRect(Accent, X, Y, 3.f * K, RowH);
		DrawLootIcon(N.ItemId, X + 7.f * K, Y + 5.f * K, 40.f * K, Alpha);
		const FString Line = FitLootText(FString::Printf(TEXT("+%d %s"), N.Count, *JamLoot::ItemName(N.ItemId)), Font, 1.1f * K, Width - 62.f * K);
		DrawText(Line, FLinearColor(JamText.R, JamText.G, JamText.B, Alpha), X + 54.f * K, Y + 15.f * K, Font, 1.1f * K);
	}
	if (LootNotices.Num() > Visible)
	{
		DrawText(FString::Printf(TEXT("+%d objetos por mostrar"), LootNotices.Num() - Visible), JamGold, 22.f * K,
			Top + Visible * (RowH + Gap), Font, K);
	}
}

float AJamHUD::DrawBackpack(const APawn* Pawn)
{
	const UInventoryComponent* Inventory = Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
	if (!Inventory) { return MsgBaseY; }
	TArray<FName> Ids;
	int32 PartCount = 0;
	for (const auto& Item : Inventory->Items)
	{
		if (Item.Value <= 0) { continue; }
		const FString Id = Item.Key.ToString();
		if (!Id.StartsWith(TEXT("Part.")) && !Id.StartsWith(TEXT("Mat."))) { continue; }
		Ids.Add(Item.Key);
		if (Id.StartsWith(TEXT("Part."))) { PartCount += Item.Value; }
	}
	Ids.Sort([](const FName& A, const FName& B)
	{
		const bool AMat = A.ToString().StartsWith(TEXT("Mat.")), BMat = B.ToString().StartsWith(TEXT("Mat."));
		return AMat != BMat ? !AMat : JamLoot::ItemName(A) < JamLoot::ItemName(B);
	});
	int32 Cap = 40;
	const TSharedPtr<FJsonObject>* Caps = nullptr;
	if (JamLoot::Config()->TryGetObjectField(TEXT("caps"), Caps)) { (*Caps)->TryGetNumberField(TEXT("total_parts"), Cap); }
	const double Now = FPlatformTime::Seconds();
	const bool bRejected = Now < BackpackFlashUntil;
	const bool bFull = PartCount >= Cap;
	const bool bBlink = FMath::Fmod(Now, 0.5) < 0.25;
	const float MaxWidth = FMath::Min(Canvas->SizeX - 28.f * K, 1060.f * K);
	const float Width = FMath::Min(MaxWidth, FMath::Max(560.f * K, (74.f + Ids.Num() * 48.f) * K));
	const float X = (Canvas->SizeX - Width) * 0.5f;
	const float SlotW = 48.f * K;
	const int32 Columns = FMath::Max(1, FMath::FloorToInt((Width - 74.f * K) / SlotW));
	const int32 Rows = FMath::Clamp(FMath::DivideAndRoundUp(Ids.Num(), Columns), 1, 2);
	const int32 PerPage = Columns * Rows;
	const int32 Pages = FMath::Max(1, FMath::DivideAndRoundUp(Ids.Num(), PerPage));
	const int32 Page = FMath::FloorToInt(GetWorld()->GetTimeSeconds() / 5.f) % Pages;
	const float Height = (24.f + Rows * 48.f) * K;
	const float Bottom = FMath::Min(Canvas->SizeY - 120.f * K, MsgBaseY - 8.f * K);
	const float Top = Bottom - Height;
	DrawRect(FLinearColor(JamPanel.R, JamPanel.G, JamPanel.B, 0.86f), X, Top, Width, Height);
	DrawRect((bRejected || bFull) && bBlink ? JamRed : JamGold, X, Top, Width, 2.f * K);
	DrawLootIcon(TEXT("Backpack"), X + 10.f * K, Top + 25.f * K, 42.f * K, (bRejected || bFull) && bBlink ? 0.3f : 1.f);
	const FString PageText = Pages > 1 ? FString::Printf(TEXT("   página %d/%d (cambia sola)"), Page + 1, Pages) : FString();
	const FString Header = FString::Printf(TEXT("MOCHILA  %d / %d piezas%s%s"), PartCount, Cap,
		bRejected ? TEXT("   SIN ESPACIO: el resto queda en el suelo") : bFull ? TEXT("   LLENA") : TEXT(""),
		*PageText);
	UFont* Font = GEngine->GetSmallFont();
	DrawText(FitLootText(Header, Font, K, Width - 24.f * K), bRejected || bFull ? JamGold : JamText, X + 10.f * K, Top + 5.f * K, Font, K);
	if (Ids.Num() == 0) { DrawText(TEXT("Recoge piezas y materiales al pasar sobre ellos"), JamText, X + 74.f * K, Top + 38.f * K, Font, K); }
	APlayerController* PC = GetOwningPlayerController();
	float MouseX = -1.f, MouseY = -1.f;
	if (PC) { PC->GetMousePosition(MouseX, MouseY); }
	for (int32 n = 0; n < PerPage && Page * PerPage + n < Ids.Num(); ++n)
	{
		const FName Id = Ids[Page * PerPage + n];
		const float IX = X + 68.f * K + (n % Columns) * SlotW, IY = Top + 23.f * K + (n / Columns) * 48.f * K;
		const bool bBlocked = JamLoot::Room(Inventory, Id) == 0 || (bRejected && BlockedLootItems.Contains(Id));
		const float Alpha = bBlocked && bBlink ? 0.35f : 1.f;
		DrawRect(FLinearColor(0.1f, 0.12f, 0.15f, 0.85f), IX, IY, 44.f * K, 45.f * K);
		DrawLootIcon(Id, IX + 5.f * K, IY, 34.f * K, Alpha);
		FLinearColor Accent = bBlocked && bBlink ? JamRed : JamLoot::TierColor(JamLoot::ItemTier(Id));
		DrawRect(Accent, IX, IY + 43.f * K, 44.f * K, 2.f * K);
		const FString Count = FString::Printf(TEXT("%d"), Inventory->GetCount(Id));
		float W = 0.f, H = 0.f;
		GetTextSize(Count, W, H, Font, K);
		DrawText(Count, JamText, IX + (44.f * K - W) * 0.5f, IY + 31.f * K, Font, K);
		if (MouseX >= IX && MouseX < IX + 44.f * K && MouseY >= IY && MouseY < IY + 45.f * K)
		{
			const FString Label = JamLoot::ItemName(Id) + (bBlocked ? TEXT(" (tope alcanzado)") : TEXT(""));
			GetTextSize(Label, W, H, Font, K);
			const float TX = FMath::Clamp(IX, 12.f * K, Canvas->SizeX - W - 12.f * K);
			DrawRect(JamPanel, TX - 4.f * K, Top - H - 10.f * K, W + 8.f * K, H + 8.f * K);
			DrawText(Label, JamText, TX, Top - H - 6.f * K, Font, K);
		}
	}
	// Se suma al HUD heredado sin cambiar su barra de energia.
	DrawLootIcon(TEXT("Energy"), (Canvas->SizeX - 360.f * K) * 0.5f - 38.f * K, Canvas->SizeY - 106.f * K, 30.f * K);
	return Top - 10.f * K;
}

void AJamHUD::DrawPickupLabels(const APawn* Pawn, float Bottom)
{
	APlayerController* PC = GetOwningPlayerController();
	if (!Pawn || !PC || !GetWorld()) { return; }
	TArray<AJamPickup*> Nearby;
	for (TActorIterator<AJamPickup> It(GetWorld()); It; ++It)
	{
		if (!It->IsHidden() && FVector::DistSquared(It->GetActorLocation(), Pawn->GetActorLocation()) < FMath::Square(1500.f)) { Nearby.Add(*It); }
	}
	Nearby.Sort([Pawn](const AJamPickup& A, const AJamPickup& B)
	{
		return FVector::DistSquared(A.GetActorLocation(), Pawn->GetActorLocation()) < FVector::DistSquared(B.GetActorLocation(), Pawn->GetActorLocation());
	});
	UFont* Font = GEngine->GetSmallFont();
	int32 Drawn = 0;
	TArray<FBox2D> Used;
	for (const AJamPickup* Pickup : Nearby)
	{
		if (Drawn >= 6 || !PC->LineOfSightTo(Pickup)) { continue; }
		const FVector P = Project(Pickup->GetActorLocation() + FVector(0.f, 0.f, 100.f));
		if (P.Z <= 0.f) { continue; }
		TArray<FName> Ids;
		for (const auto& Item : Pickup->Items) { if (Item.Value > 0) { Ids.Add(Item.Key); } }
		Ids.Sort([](const FName& A, const FName& B) { return JamLoot::ItemName(A) < JamLoot::ItemName(B); });
		const FName MainId = Ids.Num() > 0 ? Ids[0] : FName(TEXT("Energy"));
		FString Label = Ids.Num() > 0 ? FString::Printf(TEXT("%s x%d"), *JamLoot::ItemName(MainId), Pickup->Items.FindRef(MainId))
			: FString::Printf(TEXT("Energía +%.0f"), Pickup->Energy);
		const int32 More = Ids.Num() - 1 + (Ids.Num() > 0 && Pickup->Energy > 0.f ? 1 : 0);
		if (More > 0) { Label += FString::Printf(TEXT(" (+%d tipos)"), More); }
		Label += FString::Printf(TEXT("  %.0f m"), FVector::Dist(Pickup->GetActorLocation(), Pawn->GetActorLocation()) / 100.f);
		Label = FitLootText(Label, Font, K, 290.f * K);
		float W = 0.f, H = 0.f;
		GetTextSize(Label, W, H, Font, K);
		const float BW = W + 38.f * K, BH = FMath::Max(26.f * K, H + 8.f * K);
		const float X = P.X - BW * 0.5f;
		float Y = P.Y - BH;
		// Desplaza los nombres que caen uno encima de otro sin llenar el centro con etiquetas repetidas.
		for (int32 Try = 0; Try < 6; ++Try)
		{
			const FBox2D Box(FVector2D(X, Y), FVector2D(X + BW, Y + BH));
			bool bOverlap = false;
			for (const FBox2D& Other : Used) { bOverlap |= Box.Intersect(Other); }
			if (!bOverlap) { break; }
			Y -= BH + 4.f * K;
		}
		if (X < 8.f * K || X + BW > Canvas->SizeX - 8.f * K || Y < 110.f * K || Y + BH > Bottom) { continue; }
		DrawRect(FLinearColor(JamPanel.R, JamPanel.G, JamPanel.B, 0.88f), X, Y, BW, BH);
		DrawRect(JamLoot::TierColor(JamLoot::ItemTier(MainId)), X, Y, 2.f * K, BH);
		DrawLootIcon(MainId, X + 4.f * K, Y + 2.f * K, 22.f * K);
		DrawText(Label, JamText, X + 31.f * K, Y + (BH - H) * 0.5f, Font, K);
		Used.Add(FBox2D(FVector2D(X, Y), FVector2D(X + BW, Y + BH)));
		++Drawn;
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
		// Arma en mano (GDD 9): a la derecha de la vida, con nombre, efecto y gasto por disparo.
		if (!MyHealth->IsDead())
		{
			bool bLow = false;
			const FString Wpn = JamHudWeaponLine(Pawn, bLow);
			if (!Wpn.IsEmpty())
			{
				float TW = 0.f, TH = 0.f;
				GetTextSize(Wpn, TW, TH, Small, K);
				const float WX = FMath::Min(X + W + 18.f * K, SX - TW - 20.f * K), WY = Y - 2.f * K;
				DrawRect(JamPanel, WX - 8.f * K, WY - 4.f * K, TW + 16.f * K, TH + 8.f * K);
				DrawText(Wpn, bLow ? JamRed : JamGold, WX, WY, Small, K);
			}
		}
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
	float TextBaseY = MsgBaseY > 0.f ? MsgBaseY : SY - 118.f * K;
	if (!bOver)
	{
		if (Pawn && (!MyHealth || !MyHealth->IsDead()))
		{
			TextBaseY = FMath::Min(TextBaseY, DrawBackpack(Pawn));
			DrawPickupLabels(Pawn, TextBaseY);
			DrawLootNotices(TextBaseY - 24.f * K);
		}
		TrackCreations(World, Pawn, World ? World->GetDeltaSeconds() : 0.f);
		EJamTeam ObjTeam = Pawn ? UJamStatics::GetActorTeam(Pawn) : MyTeam;
		if (ObjTeam == EJamTeam::Neutral)
		{
			ObjTeam = MyTeam;
		}
		const FString Obj = UpdateObjective(World, Pawn, ObjTeam);
		const float ObjBottom = Obj.IsEmpty() ? 14.f * K : DrawObjective(Obj, 14.f * K);
		DrawBigs(ObjBottom + 12.f * K);
	}
	DrawPostLines(TextBaseY);

	// 6) Fin de partida.
	if (bOver)
	{
		const double Now = FPlatformTime::Seconds();
		if (OverSince < 0.0)
		{
			OverSince = Now;
		}
		DrawFinalCut(GS->GetWinningTeam(), MyTeam);

		// R = revancha. RestartLevel (exec de APlayerController) recarga el mismo mapa con "?restart" y solo actua en Standalone,
		// que es PIE de un jugador y el juego empaquetado; si no es Standalone se abre el nivel actual. Medio segundo de gracia y
		// nada de reiniciar mientras se escribe en la caja de texto.
		if (!bRestartRequested && PC && Now - OverSince > 0.5 && !JamHudEndTyping(World) && PC->WasInputKeyJustPressed(EKeys::R))
		{
			bRestartRequested = true;
			if (World->GetNetMode() == NM_Standalone)
			{
				PC->ConsoleCommand(TEXT("RestartLevel"));
			}
			else
			{
				UGameplayStatics::OpenLevel(World, FName(*UGameplayStatics::GetCurrentLevelName(World, true)));
			}
		}
	}
}
