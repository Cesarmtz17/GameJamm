#pragma once

#include "CoreMinimal.h"
#include "Building/JamPlayConsole.h"
#include "JamHUD.generated.h"

class UTexture2D;

/** Una fila de loot: empieza sus cuatro segundos al entrar en la columna visible. */
struct FJamHudLootNotice
{
	FName ItemId;
	int32 Count = 0;
	FString Tier;
	double Started = -1.0;
};

/** Dano realmente aplicado por el jugador; una rafaga cercana se agrupa por blanco y efecto. */
struct FJamHudDamageNotice
{
	TWeakObjectPtr<AActor> Target;
	FName Effect;
	FVector Position = FVector::ZeroVector;
	float Damage = 0.f;
	double Started = 0.0;
};

/** Seguimiento de una creacion del jugador (agrupada por pedido) para la pantalla final "tu corte final". */
struct FJamHudCreation
{
	int32 MaxParts = 0;       // maximo de piezas vivas que tuvo
	float SecsActive = 0.f;   // segundos encendida
	float SecsDriven = 0.f;   // segundos con el jugador montado en una de sus piezas

	/** Puntaje de "la mas usada": segundos montado * 2 + segundos encendida. */
	float Score() const { return SecsDriven * 2.f + SecsActive; }
};

/**
 * HUD de la partida (Core): dibuja encima del HUD de construccion (AJamTestHUD) el reloj, la vida de torres y base
 * por equipo, la vida del jugador, la cuenta de respawn, barras sobre las torres cercanas y la pantalla de victoria o derrota.
 * GDD 8 y 9: linea de objetivo por pasos (arriba al centro), avisos grandes de JamMsg::Big debajo y la cola de JamMsg::Post
 * (la linea del asistente) centrada abajo, encima de la energia.
 */
UCLASS()
class GAMEJAMM_API AJamHUD : public AJamTestHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
	virtual void NotifyHitBoxClick(FName BoxName) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Presenta solo lo recibido; false permite conservar el aviso de texto si se usa otro HUD. */
	static bool ShowLoot(AActor* Taker, const TMap<FName, int32>& Offered, const TMap<FName, int32>& Leftover,
		const FString& SourceTier, float EnergyBefore);

	/** ActualDamage = vida antes - vida despues; no presenta golpes bloqueados ni dano de otros jugadores/bots. */
	static void ShowDamage(AActor* Source, AActor* Target, float ActualDamage, FName Effect);

private:
	void UpdateBackpackInput(bool bEnabled);
	void RestoreBackpackCursor(bool bReturnToGame);
	bool bBackpackExpanded = true;
	bool bBackpackCursorOwned = false;
	bool bBackpackPreviousCursor = false;
	bool bBackpackPreviousClickEvents = false;
	TWeakObjectPtr<APlayerController> BackpackCursorController;

	void DrawDamageNotices();
	TArray<FJamHudDamageNotice> DamageNotices;

	UTexture2D* GetLootIcon(FName ItemId);
	void DrawLootIcon(FName ItemId, float X, float Y, float Size, float Alpha = 1.f);
	void DrawLootNotices(float Bottom);
	float DrawBackpack(const APawn* Pawn);
	void DrawPickupLabels(const APawn* Pawn, float Bottom);
	FString FitLootText(FString Text, UFont* Font, float Scale, float MaxWidth) const;

	/** Incluye los intentos fallidos (nullptr): nunca vuelve a cargar por frame. */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTexture2D>> LootIcons;

	TArray<FJamHudLootNotice> LootNotices;
	TSet<FName> BlockedLootItems;
	double BackpackFlashUntil = 0.0;

	void DrawCentered(const FString& Text, const FLinearColor& Color, float Y, float Scale, bool bBackground = true);

	/** Avanza el paso del objetivo segun el estado del juego (nunca retrocede) y devuelve su texto ("" si no hay). */
	FString UpdateObjective(UWorld* World, const APawn* Pawn, EJamTeam MyTeam);

	/** Dibuja el objetivo arriba al centro y devuelve la Y donde termina su panel. */
	float DrawObjective(const FString& Text, float Y);

	/** Avisos grandes (JamMsg::Big) desde la Y dada hacia abajo. */
	void DrawBigs(float Y);

	/** Cola de avisos cortos (JamMsg::Post): la mas nueva abajo, terminando en BaseY. */
	void DrawPostLines(float BaseY);

	/** Suma, por pedido, los segundos encendida y montado de cada construccion del jugador (JamSay). Solo durante la partida. */
	void TrackCreations(UWorld* World, const APawn* Pawn, float DeltaSeconds);

	/** Pantalla final: VICTORIA/DERROTA, "TU CORTE FINAL" con la creacion mas usada y "R = revancha". */
	void DrawFinalCut(EJamTeam Winner, EJamTeam MyTeam);

	/** Creaciones vistas en esta partida, por pedido (Prompt). */
	TMap<FString, FJamHudCreation> Creations;

	bool bRestartRequested = false;   // R ya pulsada: no se reinicia dos veces
	double OverSince = -1.0;          // hora (FPlatformTime) en que se vio el fin de la partida

	float LastHealth = -1.f;
	float HurtFlash = 0.f;

	// Objetivo (GDD 8): paso actual 0..4; los datos de arranque se toman la primera vez que se ve al jugador.
	int32 ObjStep = 0;
	bool bObjHaveStart = false;
	FVector ObjStartLoc = FVector::ZeroVector;
	double ObjFirstSeen = 0.0;
	int32 ObjInvBaseline = 0;
	int32 ObjTowersDownMax = 0;
};
