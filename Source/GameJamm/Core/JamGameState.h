#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Core/JamTypes.h"
#include "JamGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FJamOnMatchEnded, EJamTeam, Winner);

/** Estado de la partida visible para todos (UI: "¡Ganó el equipo Azul!"). */
UCLASS()
class GAMEJAMM_API AJamGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Jam|Match")
	FJamOnMatchEnded OnMatchEnded;

	UFUNCTION(BlueprintPure, Category = "Jam|Match")
	bool IsMatchOver() const { return bMatchOver; }

	UFUNCTION(BlueprintPure, Category = "Jam|Match")
	EJamTeam GetWinningTeam() const { return WinningTeam; }

	/** Solo servidor (lo llama el GameMode). */
	void SetMatchResult(EJamTeam Winner);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_MatchOver();

	UPROPERTY(ReplicatedUsing = OnRep_MatchOver)
	bool bMatchOver = false;

	UPROPERTY(Replicated)
	EJamTeam WinningTeam = EJamTeam::Neutral;
};
