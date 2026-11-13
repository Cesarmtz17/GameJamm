#include "Core/JamStatics.h"
#include "Core/HealthComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

namespace
{
	EJamTeam GetActorTeamRecursive(const AActor* Actor, int32 Depth)
	{
		if (!Actor || Depth > 4)
		{
			return EJamTeam::Neutral;
		}

		if (const AController* Controller = Cast<AController>(Actor))
		{
			return GetActorTeamRecursive(Controller->GetPawn(), Depth + 1);
		}

		if (const UHealthComponent* Health = Actor->FindComponentByClass<UHealthComponent>())
		{
			return Health->Team;
		}

		const AActor* Instigator = Actor->GetInstigator();
		if (Instigator && Instigator != Actor)
		{
			return GetActorTeamRecursive(Instigator, Depth + 1);
		}

		const AActor* Owner = Actor->GetOwner();
		if (Owner && Owner != Actor)
		{
			return GetActorTeamRecursive(Owner, Depth + 1);
		}

		return EJamTeam::Neutral;
	}
}

EJamTeam UJamStatics::GetActorTeam(const AActor* Actor)
{
	return GetActorTeamRecursive(Actor, 0);
}

bool UJamStatics::AreEnemies(const AActor* A, const AActor* B)
{
	if (!A || !B || A == B)
	{
		return false;
	}
	return GetActorTeam(A) != GetActorTeam(B);
}

EJamTeam UJamStatics::GetOpposingTeam(EJamTeam Team)
{
	switch (Team)
	{
	case EJamTeam::Red:  return EJamTeam::Blue;
	case EJamTeam::Blue: return EJamTeam::Red;
	default:             return EJamTeam::Neutral;
	}
}
