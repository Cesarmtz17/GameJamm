#include "AI/JamCampGuard.h"
#include "Core/HealthComponent.h"
#include "Core/JamPickup.h"
#include "GameJamm.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

AJamCampGuard::AJamCampGuard()
{
	EnergyReward = 10.f;
}

void AJamCampGuard::InitGuard(bool bInThrower, const FVector& InHome)
{
	bThrower = bInThrower;
	Home = InHome;
	Health->Team = EJamTeam::Neutral;
	Kind = bThrower ? EJamAgentKind::Ranged : EJamAgentKind::Melee;
	if (bThrower)
	{
		Health->MaxHealth = 50.f; AttackDamage = 5.f; AttackRange = 1500.f; AttackInterval = 2.f; AggroRange = 1500.f;
	}
	else
	{
		Health->MaxHealth = 80.f; AttackDamage = 7.f; AttackRange = 180.f; AttackInterval = 1.2f; AggroRange = 1200.f;
		SetActorScale3D(FVector(1.15f));
	}
	GetCharacterMovement()->MaxWalkSpeed = bThrower ? 320.f : 420.f;
	// Neutral: verde musgo, distinto de rojo y azul.
	SetColor(bThrower ? FLinearColor(0.32f, 0.36f, 0.12f) : FLinearColor(0.18f, 0.32f, 0.14f));
}

AActor* AJamCampGuard::FindIntruder() const
{
	const FVector Me = GetActorLocation();
	AActor* Best = nullptr;
	float BestD = AggroRange * AggroRange;
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(JamCampTarget), false, this);
	GetWorld()->OverlapMultiByObjectType(Overlaps, Me, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(AggroRange), Params);
	for (const FOverlapResult& O : Overlaps)
	{
		AActor* A = O.GetActor();
		const UHealthComponent* H = A ? A->FindComponentByClass<UHealthComponent>() : nullptr;
		// Solo heroes y minions de un equipo, y que esten dentro del campamento (no lo persigue lejos).
		if (!H || H->IsDead() || H->Team == EJamTeam::Neutral || FVector::Dist2D(A->GetActorLocation(), Home) > LeashRange)
		{
			continue;
		}
		const float D = FVector::DistSquared(A->GetActorLocation(), Me);
		if (D < BestD) { BestD = D; Best = A; }
	}
	return Best;
}

bool AJamCampGuard::GroundAhead(const FVector& Dir) const
{
	const FVector From = GetActorLocation() + Dir.GetSafeNormal2D() * 160.f;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(JamCampGround), false, this);
	return GetWorld()->LineTraceSingleByChannel(Hit, From, From - FVector(0, 0, 600.f), ECC_Visibility, Params);
}

void AJamCampGuard::GoTo(const FVector& Where)
{
	const FVector Dir = Where - GetActorLocation();
	if (!GroundAhead(Dir))
	{
		StopMoving(); // orilla de la isla: no se tira
		return;
	}
	Goal = Where; // sin el limite de carril de AJamMinion::MoveTo (los campamentos estan lejos de Y = 0)
	bHasGoal = true;
}

void AJamCampGuard::Think()
{
	if (Health->IsDead())
	{
		return;
	}
	if (GetActorLocation().Z < -2500.f)
	{
		Health->Kill(nullptr);
		return;
	}
	if (AActor* Target = FindIntruder())
	{
		if (Target != LastIntruder.Get())
		{
			UE_LOG(LogJam, Log, TEXT("Campamento: %s va por %s"), *GetName(), *Target->GetName());
			LastIntruder = Target;
		}
		if (!TryAttack(Target)) { GoTo(Target->GetActorLocation()); }
		return;
	}
	// Nadie en el campamento: vuelve y se cura.
	if (FVector::Dist2D(GetActorLocation(), Home) > 150.f) { GoTo(Home); }
	else
	{
		StopMoving();
		Health->Heal(Health->MaxHealth);
	}
}

void AJamCampGuard::HandleDied(UHealthComponent* HealthComponent, AActor* Killer)
{
	// El Lanzador suelta un material de fusion (GDD 11).
	if (bThrower)
	{
		static const TCHAR* Mats[] = { TEXT("Mat.Rock"), TEXT("Mat.Fire"), TEXT("Mat.Ice"), TEXT("Mat.Shock") };
		TMap<FName, int32> Drop;
		Drop.Add(Mats[FMath::RandRange(0, 3)], 1);
		AJamPickup::SpawnBundle(GetWorld(), GetActorLocation(), Drop, 0.f, 30.f);
	}
	UE_LOG(LogJam, Log, TEXT("Campamento: muere %s %s por %s"), bThrower ? TEXT("Lanzador") : TEXT("Guardia"), *GetName(), *GetNameSafe(Killer));
	Super::HandleDied(HealthComponent, Killer);
}
