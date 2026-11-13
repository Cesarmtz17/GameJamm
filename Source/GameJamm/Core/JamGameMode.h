#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Core/JamTypes.h"
#include "JamGameMode.generated.h"

class ATeamStructure;

/**
 * Reglas de la partida: cae la base de un equipo -> gana el otro.
 * Hacer BP hijo (BP_JamGameMode) para asignar DefaultPawnClass (personaje de 3ra persona), HUD, etc.
 */
UCLASS()
class GAMEJAMM_API AJamGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AJamGameMode();

	UFUNCTION(BlueprintCallable, Category = "Jam|Match")
	void NotifyStructureDestroyed(ATeamStructure* Structure, AActor* Killer);

	UFUNCTION(BlueprintCallable, Category = "Jam|Match")
	void EndMatch(EJamTeam Winner);

	UFUNCTION(BlueprintImplementableEvent, Category = "Jam|Match")
	void OnStructureDestroyed(ATeamStructure* Structure, AActor* Killer);

	UFUNCTION(BlueprintImplementableEvent, Category = "Jam|Match")
	void OnMatchEnded(EJamTeam Winner);
};
