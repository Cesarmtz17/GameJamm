#include "Core/JamGameMode.h"
#include "Core/JamGameState.h"
#include "Core/JamStatics.h"
#include "Core/TeamStructure.h"
#include "GameJamm.h"

AJamGameMode::AJamGameMode()
{
	GameStateClass = AJamGameState::StaticClass();
}

void AJamGameMode::NotifyStructureDestroyed(ATeamStructure* Structure, AActor* Killer)
{
	if (!Structure)
	{
		return;
	}

	UE_LOG(LogJam, Log, TEXT("Structure destroyed: %s (base=%d)"), *Structure->GetName(), Structure->bIsBase ? 1 : 0);
	OnStructureDestroyed(Structure, Killer);

	if (Structure->bIsBase)
	{
		EndMatch(UJamStatics::GetOpposingTeam(Structure->GetTeam()));
	}
}

void AJamGameMode::EndMatch(EJamTeam Winner)
{
	AJamGameState* JamGameState = GetGameState<AJamGameState>();
	if (!JamGameState || JamGameState->IsMatchOver())
	{
		return;
	}

	UE_LOG(LogJam, Log, TEXT("Match over. Winner: %s"), *UEnum::GetValueAsString(Winner));
	JamGameState->SetMatchResult(Winner);
	OnMatchEnded(Winner);
}
