#include "Core/JamGameState.h"
#include "Net/UnrealNetwork.h"

void AJamGameState::SetMatchResult(EJamTeam Winner)
{
	WinningTeam = Winner;
	bMatchOver = true;
	OnMatchEnded.Broadcast(WinningTeam);
}

void AJamGameState::OnRep_MatchOver()
{
	if (bMatchOver)
	{
		OnMatchEnded.Broadcast(WinningTeam);
	}
}

void AJamGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AJamGameState, bMatchOver);
	DOREPLIFETIME(AJamGameState, WinningTeam);
	DOREPLIFETIME(AJamGameState, MatchSeconds);
	DOREPLIFETIME(AJamGameState, bSuddenDeath);
	DOREPLIFETIME(AJamGameState, bMatchRunning);
}
