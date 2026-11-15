#include "AI/JamMinion.h"
#include "GameJamm.h"
#include "AIController.h"
#include "Building/JamBuildPart.h"
#include "Components/CapsuleComponent.h"
#include "Core/JamCharacterLook.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HealthComponent.h"
#include "Core/JamGameState.h"
#include "Core/JamStatics.h"
#include "Core/TeamStructure.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AJamMinion::AJamMinion()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AAIController::StaticClass();
	Tags.Add(TEXT("Minion"));

	GetCapsuleComponent()->InitCapsuleSize(34.f, 80.f);
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0, 540, 0);
	GetCharacterMovement()->MaxWalkSpeed = 380.f;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(GetCapsuleComponent());
	Body->SetRelativeLocation(FVector(0, 0, -25));
	Body->SetRelativeScale3D(FVector(0.55f, 0.55f, 1.1f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Cyl.Succeeded()) { Body->SetStaticMesh(Cyl.Object); }

	Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	Head->SetupAttachment(GetCapsuleComponent());
	Head->SetRelativeLocation(FVector(0, 0, 55));
	Head->SetRelativeScale3D(FVector(0.4f));
	Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Sphere.Succeeded()) { Head->SetStaticMesh(Sphere.Object); }

	Bolt = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bolt"));
	Bolt->SetupAttachment(GetCapsuleComponent());
	Bolt->SetUsingAbsoluteLocation(true);
	Bolt->SetUsingAbsoluteRotation(true);
	Bolt->SetUsingAbsoluteScale(true);
	Bolt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bolt->SetCastShadow(false);
	Bolt->SetHiddenInGame(true);
	if (Cyl.Succeeded()) { Bolt->SetStaticMesh(Cyl.Object); }
	if (Glow.Succeeded()) { Bolt->SetMaterial(0, Glow.Object); }

	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	Health->MaxHealth = 60.f;
	Look = CreateDefaultSubobject<UJamCharacterLook>(TEXT("Look"));
	Look->Key = TEXT("Minion");
}

void AJamMinion::InitAgent(EJamAgentKind InKind, EJamTeam InTeam)
{
	Kind = InKind;
	Health->Team = InTeam;
	switch (Kind)
	{
	case EJamAgentKind::Melee:  Health->MaxHealth = 60.f;  AttackDamage = 8.f;  AttackRange = 180.f; AttackInterval = 1.f; break;
	case EJamAgentKind::Ranged: Health->MaxHealth = 40.f;  AttackDamage = 6.f;  AttackRange = 800.f; AttackInterval = 1.2f; break;
	case EJamAgentKind::Heavy:
		Health->MaxHealth = 200.f; AttackDamage = 15.f; AttackRange = 220.f; AttackInterval = 1.5f;
		GetCharacterMovement()->MaxWalkSpeed = 300.f;
		SetActorScale3D(FVector(1.35f));
		break;
	default: break;
	}
	// Color de equipo apagado (ART.md 6): los minions no compiten con los heroes.
	const FLinearColor TeamCol = InTeam == EJamTeam::Blue ? FLinearColor(0.12f, 0.22f, 0.45f) : FLinearColor(0.45f, 0.12f, 0.12f);
	SetColor(TeamCol);
}

void AJamMinion::SetColor(const FLinearColor& Color)
{
	for (UStaticMeshComponent* C : { Body.Get(), Head.Get() })
	{
		if (UMaterialInstanceDynamic* MID = C->CreateDynamicMaterialInstance(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
		}
	}
}

EJamTeam AJamMinion::GetTeam() const
{
	return Health ? Health->Team : EJamTeam::Neutral;
}

void AJamMinion::BeginPlay()
{
	Super::BeginPlay();
	Health->OnDied.AddDynamic(this, &AJamMinion::HandleDied);
	GetWorldTimerManager().SetTimer(ThinkTimer, this, &AJamMinion::Think, 0.2f, true, FMath::FRandRange(0.f, 0.2f));
}

void AJamMinion::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bHasGoal || Health->IsDead())
	{
		return;
	}
	FVector Dir = Goal - GetActorLocation();
	Dir.Z = 0.f;
	if (Dir.SizeSquared() < 60.f * 60.f)
	{
		bHasGoal = false;
		return;
	}
	AddMovementInput(Dir.GetSafeNormal());

	// Huecos entre isla y puente (hasta ~1.5 m, Jam.Ground los muestra): si no hay piso justo adelante, salta para cruzarlo.
	UCharacterMovementComponent* Move = GetCharacterMovement();
	const float Now = GetWorld()->GetTimeSeconds();
	if (Move->IsMovingOnGround() && Now - LastHop > 0.7f)
	{
		const FVector Ahead = GetActorLocation() + Dir.GetSafeNormal() * 90.f;
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(JamMinionGap), false, this);
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Ahead, Ahead - FVector(0, 0, 400.f), ECC_Visibility, Params))
		{
			LastHop = Now;
			LaunchCharacter(Dir.GetSafeNormal() * 520.f + FVector(0, 0, 360.f), true, true);
		}
	}
}

FVector AJamMinion::LaneGoal() const
{
	// El carril es la linea Y = 0 entre las bases (puentes incluidos): se vuelve a ella y se avanza hacia la base enemiga.
	const FVector Me = GetActorLocation();
	const float EnemyX = BaseX(UJamStatics::GetOpposingTeam(GetTeam()));
	const float Dir = FMath::Sign(EnemyX - Me.X);
	const float NextX = FMath::Abs(EnemyX - Me.X) < 800.f ? EnemyX : Me.X + Dir * 800.f;
	// Las torres propias estan sobre el carril y no hay navmesh: si una queda justo adelante, rodearla por un lado.
	for (TActorIterator<ATeamStructure> It(GetWorld()); It; ++It)
	{
		if (It->GetTeam() != GetTeam())
		{
			continue; // las enemigas son blanco: se detiene antes a atacarlas
		}
		FVector O, E;
		It->GetActorBounds(true, O, E);
		const float Ahead = (O.X - Me.X) * Dir;
		if (Ahead > -E.X - 40.f && Ahead < E.X + 450.f && FMath::Abs(O.Y - Me.Y) < E.Y + 110.f)
		{
			const float Side = (Me.Y >= O.Y) ? 1.f : -1.f;
			return FVector(Me.X + Dir * (Ahead > E.X + 120.f ? 120.f : 400.f), O.Y + Side * (E.Y + 140.f), Me.Z);
		}
	}
	return FVector(NextX, FMath::Abs(Me.Y) > 150.f ? 0.f : Me.Y * 0.5f, Me.Z);
}

void AJamMinion::MoveTo(const FVector& InGoal)
{
	Goal = InGoal;
	// Fuera de la pradera central (|X| > 5000) el piso es angosto: no se aleja del carril para no caer.
	const float MaxY = FMath::Abs(GetActorLocation().X) < 5000.f ? 3000.f : 350.f;
	Goal.Y = FMath::Clamp(Goal.Y, -MaxY, MaxY);
	bHasGoal = true;
}

AActor* AJamMinion::FindTarget(float Range) const
{
	const EJamTeam Mine = GetTeam();
	const FVector Me = GetActorLocation();
	AActor* Best = nullptr;
	float BestD = Range * Range;

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(JamMinionTarget), false, this);
	GetWorld()->OverlapMultiByObjectType(Overlaps, Me, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Range), Params);
	for (const FOverlapResult& O : Overlaps)
	{
		AActor* A = O.GetActor();
		const UHealthComponent* H = A ? A->FindComponentByClass<UHealthComponent>() : nullptr;
		if (!H || H->IsDead() || H->Team == Mine || H->Team == EJamTeam::Neutral)
		{
			continue;
		}
		const float D = FVector::DistSquared(A->GetActorLocation(), Me);
		if (D < BestD)
		{
			BestD = D;
			Best = A;
		}
	}
	if (Best)
	{
		return Best;
	}
	// Sin enemigos vivos cerca: la torre o base enemiga en rango (solo si ya se le puede hacer dano).
	for (TActorIterator<ATeamStructure> It(GetWorld()); It; ++It)
	{
		if (It->GetTeam() == Mine || !It->Health || It->Health->IsDead() || It->Health->bInvulnerable)
		{
			continue;
		}
		const float D = FVector::DistSquared2D(It->GetActorLocation(), Me);
		if (D < BestD)
		{
			BestD = D;
			Best = *It;
		}
	}
	return Best;
}

bool AJamMinion::TryAttack(AActor* Target)
{
	const float Now = GetWorld()->GetTimeSeconds();
	// Distancia al borde: torres y bases son anchas.
	float Reach = AttackRange + 40.f;
	if (const ATeamStructure* S = Cast<ATeamStructure>(Target))
	{
		FVector O, E;
		S->GetActorBounds(true, O, E);
		Reach += FMath::Max(E.X, E.Y);
	}
	if (FVector::Dist2D(Target->GetActorLocation(), GetActorLocation()) > Reach)
	{
		return false;
	}
	StopMoving();
	FRotator Face = (Target->GetActorLocation() - GetActorLocation()).Rotation();
	SetActorRotation(FRotator(0, Face.Yaw, 0));
	if (Now - LastAttack < AttackInterval)
	{
		return true;
	}
	LastAttack = Now;
	UGameplayStatics::ApplyDamage(Target, AttackDamage, GetController(), this, nullptr);
	Look->PlayAction(AttackRange > 400.f ? TEXT("Shoot") : TEXT("Attack"));

	if (AttackRange > 400.f)
	{
		const FVector From = GetActorLocation() + FVector(0, 0, 40);
		const FVector Dir = Target->GetActorLocation() - From;
		Bolt->SetWorldLocationAndRotation(From + Dir * 0.5f, FRotationMatrix::MakeFromZ(Dir.GetSafeNormal()).Rotator());
		Bolt->SetWorldScale3D(FVector(0.06f, 0.06f, Dir.Size() / 100.f));
		Bolt->SetHiddenInGame(false);
		GetWorldTimerManager().SetTimer(BoltTimer, this, &AJamMinion::HideBolt, 0.12f, false);
	}
	else
	{
		// golpe: pequeno salto hacia el objetivo
		LaunchCharacter(Face.Vector() * 250.f + FVector(0, 0, 120.f), false, false);
	}
	return true;
}

void AJamMinion::HideBolt()
{
	Bolt->SetHiddenInGame(true);
}

void AJamMinion::Think()
{
	if (Health->IsDead())
	{
		return;
	}
	if (GetActorLocation().Z < -2500.f)
	{
		Health->Kill(nullptr); // se cayo de la isla
		return;
	}
	const AJamGameState* GS = GetWorld()->GetGameState<AJamGameState>();
	if (GS && GS->IsMatchOver())
	{
		StopMoving();
		return;
	}
	if (AActor* Target = FindTarget(AggroRange))
	{
		if (!TryAttack(Target))
		{
			MoveTo(Target->GetActorLocation());
		}
		return;
	}
	MoveTo(LaneGoal());
}

void AJamMinion::HandleDied(UHealthComponent* HealthComponent, AActor* Killer)
{
	GetWorldTimerManager().ClearTimer(ThinkTimer);
	StopMoving();
	UE_LOG(LogJam, Log, TEXT("Muere %s %s en X=%.0f Y=%.0f Z=%.0f por %s"), *GetName(), GetTeam() == EJamTeam::Red ? TEXT("Rojo") : TEXT("Azul"),
		GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z, *GetNameSafe(Killer));
	// +3 de energia al jugador que lo mato (GDD 3).
	const APawn* KillerPawn = Cast<APawn>(Killer);
	if (EnergyReward > 0.f && KillerPawn && KillerPawn->IsPlayerControlled())
	{
		AJamBuildPart::SetPlayerEnergy(GetWorld(), FMath::Min(150.f, AJamBuildPart::GetPlayerEnergy(GetWorld()) + EnergyReward));
	}
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Look->IsApplied())
	{
		SetLifeSpan(1.6f); // que se vea la animacion Death
	}
	else
	{
		SetActorHiddenInGame(true);
		SetLifeSpan(0.2f);
	}
}
