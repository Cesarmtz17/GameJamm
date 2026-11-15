#include "Core/TeamStructure.h"
#include "Core/HealthComponent.h"
#include "Core/JamStatics.h"
#include "Core/JamGameMode.h"
#include "Core/JamAudio.h"
#include "Core/JamMessages.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ATeamStructure::ATeamStructure()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	Health->MaxHealth = 1500.f; // GDD 9: torre 1500, base 3000 (se ajusta por instancia)

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));
	Bolt = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bolt"));
	Bolt->SetupAttachment(Mesh);
	Bolt->SetUsingAbsoluteLocation(true);
	Bolt->SetUsingAbsoluteRotation(true);
	Bolt->SetUsingAbsoluteScale(true);
	Bolt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bolt->SetCastShadow(false);
	Bolt->SetHiddenInGame(true);
	if (Cyl.Succeeded()) { Bolt->SetStaticMesh(Cyl.Object); }
	if (Glow.Succeeded()) { Bolt->SetMaterial(0, Glow.Object); }
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
	HideBolt();
	// Cae: deja de verse y de estorbar (BP puede poner ruinas/explosion en OnStructureDestroyed).
	Mesh->SetHiddenInGame(true);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OnStructureDestroyed(Killer);
	// Torre caida (GDD 9 y 10): evento de toda la partida, se oye y se avisa aunque el jugador este lejos.
	JamAudio::Play2D(this, TEXT("SFX_Tower_Destroyed"), 0.7f);

	if (AJamGameMode* GameMode = GetWorld()->GetAuthGameMode<AJamGameMode>())
	{
		const bool bEnemy = GetTeam() != GameMode->PlayerTeam;
		JamMsg::Big(bIsBase ? (bEnemy ? TEXT("Cayo la base rival") : TEXT("Cayo tu base"))
			: (bEnemy ? TEXT("Cayo una torre rival") : TEXT("Cayo una de tus torres")),
			bEnemy ? JamMsg::Good : JamMsg::Bad, 4.f, TEXT("tower"));
		// Fanfarria al caer una estructura enemiga (o la base de cualquiera); si cae una torre propia solo suena el derrumbe.
		if (bIsBase || bEnemy)
		{
			JamAudio::Play2D(this, TEXT("SFX_Fanfare"), 0.8f);
		}
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

	// GDD 3: primero los minions enemigos en rango; si no hay, el enemigo mas cercano.
	AActor* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	bool bBestIsMinion = false;
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
		const bool bMinion = Candidate->ActorHasTag(TEXT("Minion"));
		if ((bMinion && !bBestIsMinion) || (bMinion == bBestIsMinion && DistSq < BestDistSq))
		{
			BestDistSq = DistSq;
			Best = Candidate;
			bBestIsMinion = bMinion;
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
		ShotsOnTarget = LastTarget.Get() == Target ? ShotsOnTarget + 1 : 0;
		LastTarget = Target;
		const bool bMinion = Target->ActorHasTag(TEXT("Minion"));
		const float Damage = bMinion ? AttackDamage : HeroDamage * FMath::Min(1.f + HeroRampPerShot * ShotsOnTarget, HeroRampMax);
		UGameplayStatics::ApplyDamage(Target, Damage, nullptr, this, nullptr);
		JamAudio::PlayAt(this, TEXT("SFX_Tower_Shot"), GetActorLocation(), 0.6f);

		// Rayo desde lo alto de la torre al objetivo.
		FVector Origin, Extent;
		Mesh->Bounds.GetBox().GetCenterAndExtents(Origin, Extent);
		const FVector From = FVector(Origin.X, Origin.Y, Origin.Z + Extent.Z * 0.8f);
		const FVector To = Target->GetActorLocation();
		const FVector Dir = To - From;
		Bolt->SetWorldLocationAndRotation(From + Dir * 0.5f, FRotationMatrix::MakeFromZ(Dir.GetSafeNormal()).Rotator());
		Bolt->SetWorldScale3D(FVector(0.12f, 0.12f, Dir.Size() / 100.f));
		Bolt->SetHiddenInGame(false);
		GetWorldTimerManager().SetTimer(BoltTimer, this, &ATeamStructure::HideBolt, 0.15f, false);

		OnAttack(Target);
	}
}

void ATeamStructure::HideBolt()
{
	if (Bolt)
	{
		Bolt->SetHiddenInGame(true);
	}
}

float ATeamStructure::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// GDD 3: un jugador o construccion que golpea sin minions aliados cerca hace la mitad (hay que empujar con la oleada).
	const AActor* Attacker = EventInstigator && EventInstigator->GetPawn() ? EventInstigator->GetPawn() : DamageCauser;
	if (DamageAmount > 0.f && Attacker && !Attacker->ActorHasTag(TEXT("Minion")))
	{
		const EJamTeam AttackerTeam = UJamStatics::GetActorTeam(Attacker);
		bool bEscorted = false;
		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(JamTowerEscort), false, this);
		GetWorld()->OverlapMultiByObjectType(Overlaps, GetActorLocation(), FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn),
			FCollisionShape::MakeSphere(EscortRadius), Params);
		for (const FOverlapResult& O : Overlaps)
		{
			const AActor* A = O.GetActor();
			const UHealthComponent* H = A ? A->FindComponentByClass<UHealthComponent>() : nullptr;
			if (A && A->ActorHasTag(TEXT("Minion")) && H && !H->IsDead() && H->Team == AttackerTeam)
			{
				bEscorted = true;
				break;
			}
		}
		if (!bEscorted)
		{
			DamageAmount *= 0.5f;
		}
	}
	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}
