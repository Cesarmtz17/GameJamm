#include "Core/TeamStructure.h"
#include "Core/HealthComponent.h"
#include "Core/JamStatics.h"
#include "Core/JamGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

ATeamStructure::ATeamStructure()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	Health->MaxHealth = 1000.f;
}

void ATeamStructure::BeginPlay()
{
	Super::BeginPlay();

	Health->OnDied.AddDynamic(this, &ATeamStructure::HandleDied);

	for (ATeamStructure* Prereq : Prerequisites)
	{
		if (IsValid(Prereq) && Prereq->Health)
		{
			Prereq->Health->OnDied.AddDynamic(this, &ATeamStructure::HandlePrerequisiteDied);
		}
	}
	RefreshVulnerability();

	if (HasAuthority() && bCanAttack)
	{
		GetWorldTimerManager().SetTimer(AttackTimer, this, &ATeamStructure::TryAttack, AttackInterval, true, AttackInterval);
	}
}

EJamTeam ATeamStructure::GetTeam() const
{
	return Health ? Health->Team : EJamTeam::Neutral;
}

bool ATeamStructure::IsVulnerable() const
{
	for (const ATeamStructure* Prereq : Prerequisites)
	{
		if (IsValid(Prereq) && Prereq->Health && !Prereq->Health->IsDead())
		{
			return false;
		}
	}
	return true;
}

void ATeamStructure::RefreshVulnerability()
{
	const bool bWasInvulnerable = Health->bInvulnerable;
	Health->bInvulnerable = !IsVulnerable();
	if (bWasInvulnerable && !Health->bInvulnerable)
	{
		OnBecameVulnerable();
	}
}

void ATeamStructure::HandlePrerequisiteDied(UHealthComponent* HealthComponent, AActor* Killer)
{
	RefreshVulnerability();
}

void ATeamStructure::HandleDied(UHealthComponent* HealthComponent, AActor* Killer)
{
	GetWorldTimerManager().ClearTimer(AttackTimer);
	OnStructureDestroyed(Killer);

	if (AJamGameMode* GameMode = GetWorld()->GetAuthGameMode<AJamGameMode>())
	{
		GameMode->NotifyStructureDestroyed(this, Killer);
	}
}

AActor* ATeamStructure::FindTarget() const
{
	const EJamTeam EnemyTeam = UJamStatics::GetOpposingTeam(GetTeam());
	if (EnemyTeam == EJamTeam::Neutral)
	{
		return nullptr;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(JamTowerTarget), false, this);
	GetWorld()->OverlapMultiByObjectType(
		Overlaps,
		GetActorLocation(),
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(AttackRange),
		Params);

	AActor* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();
		if (!Candidate || UJamStatics::GetActorTeam(Candidate) != EnemyTeam)
		{
			continue;
		}
		const UHealthComponent* CandidateHealth = Candidate->FindComponentByClass<UHealthComponent>();
		if (!CandidateHealth || CandidateHealth->IsDead())
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(Candidate->GetActorLocation(), GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Candidate;
		}
	}
	return Best;
}

void ATeamStructure::TryAttack()
{
	if (!bCanAttack || Health->IsDead())
	{
		return;
	}
	if (AActor* Target = FindTarget())
	{
		UGameplayStatics::ApplyDamage(Target, AttackDamage, nullptr, this, nullptr);
		OnAttack(Target);
	}
}
