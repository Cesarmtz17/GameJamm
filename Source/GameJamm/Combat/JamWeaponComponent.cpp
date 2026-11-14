#include "Combat/JamWeaponComponent.h"
#include "Combat/JamArrow.h"
#include "Building/JamPlayConsole.h"
#include "Core/HealthComponent.h"
#include "Core/JamStatics.h"
#include "GameJamm.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

UJamWeaponComponent::UJamWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sword(TEXT("/Game/Game/Combat/Weapons/SM_WPN_SWORD.SM_WPN_SWORD"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Bow(TEXT("/Game/Game/Combat/Weapons/SM_WPN_BOW.SM_WPN_BOW"));
	if (Sword.Succeeded()) { SwordMesh = Sword.Object; }
	if (Bow.Succeeded()) { BowMesh = Bow.Object; }
}

void UJamWeaponComponent::BeginPlay()
{
	Super::BeginPlay();
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->GetRootComponent())
	{
		return;
	}
	HandMesh = NewObject<UStaticMeshComponent>(Owner, TEXT("WeaponInHand"));
	HandMesh->SetupAttachment(Owner->GetRootComponent());
	HandMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HandMesh->SetCastShadow(true);
	HandMesh->RegisterComponent();
	RefreshHandMesh();
}

void UJamWeaponComponent::RefreshHandMesh()
{
	if (!HandMesh)
	{
		return;
	}
	if (Current == EJamWeapon::Sword)
	{
		HandMesh->SetStaticMesh(SwordMesh);
		HandMesh->SetRelativeLocation(FVector(45.f, 38.f, 0.f)); // a la derecha, apuntando al frente
		HandMesh->SetRelativeRotation(FRotator(0.f, 0.f, 0.f));
	}
	else
	{
		HandMesh->SetStaticMesh(BowMesh);
		HandMesh->SetRelativeLocation(FVector(35.f, 30.f, 10.f));
		HandMesh->SetRelativeRotation(FRotator(0.f, 0.f, 0.f));
	}
}

bool UJamWeaponComponent::IsReady() const
{
	const UWorld* World = GetWorld();
	const float Interval = Current == EJamWeapon::Sword ? SwordInterval : BowInterval;
	return World && World->GetTimeSeconds() - LastAttackTime >= Interval;
}

bool UJamWeaponComponent::Attack()
{
	const UHealthComponent* Health = GetOwner() ? GetOwner()->FindComponentByClass<UHealthComponent>() : nullptr;
	if (!IsReady() || (Health && Health->IsDead()))
	{
		return false;
	}
	LastAttackTime = GetWorld()->GetTimeSeconds();
	if (Current == EJamWeapon::Sword) { SwingSword(); }
	else { ShootArrow(); }
	OnWeaponAttack.Broadcast(this, Current);
	return true;
}

void UJamWeaponComponent::NextWeapon()
{
	SetWeapon(Current == EJamWeapon::Sword ? EJamWeapon::Bow : EJamWeapon::Sword);
}

void UJamWeaponComponent::SetWeapon(EJamWeapon Weapon)
{
	Current = Weapon;
	SwingAlpha = 1.f;
	RefreshHandMesh();
}

void UJamWeaponComponent::GetAim(FVector& OutFrom, FVector& OutDir) const
{
	const AActor* Owner = GetOwner();
	const APawn* Pawn = Cast<APawn>(Owner);
	const FRotator Control = Pawn && Pawn->GetController() ? Pawn->GetController()->GetControlRotation() : Owner->GetActorRotation();
	OutFrom = Owner->GetActorLocation() + FRotator(0.f, Control.Yaw, 0.f).Vector() * 50.f + FVector(0, 0, 40.f);
	OutDir = Control.Vector();

	// Con camara: a donde mira el centro de la pantalla (hasta BowRange), para que la flecha vaya a la mira.
	if (const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr)
	{
		FVector ViewLoc;
		FRotator ViewRot;
		PC->GetPlayerViewPoint(ViewLoc, ViewRot);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(JamBowAim), false, Owner);
		FHitResult Hit;
		const FVector End = ViewLoc + ViewRot.Vector() * BowRange;
		const FVector Target = GetWorld()->LineTraceSingleByChannel(Hit, ViewLoc + ViewRot.Vector() * 200.f, End, ECC_Visibility, Params) ? Hit.ImpactPoint : End;
		OutDir = (Target - OutFrom).GetSafeNormal();
	}
}

bool UJamWeaponComponent::DealDamage(AActor* Target, float Damage, const FVector& Push)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Target || Target == Owner || !UJamStatics::AreEnemies(Owner, Target))
	{
		return false;
	}
	const UHealthComponent* TargetHealth = Target->FindComponentByClass<UHealthComponent>();
	if (!TargetHealth || TargetHealth->IsDead())
	{
		return false;
	}
	const APawn* Pawn = Cast<APawn>(Owner);
	UGameplayStatics::ApplyDamage(Target, Damage, Pawn ? Pawn->GetController() : nullptr, Owner, nullptr);
	if (ACharacter* Char = Cast<ACharacter>(Target))
	{
		Char->LaunchCharacter(Push + FVector(0, 0, Push.IsNearlyZero() ? 0.f : 180.f), true, false);
	}
	AJamTestCharacter::AddTrauma(GetWorld(), Target->GetActorLocation(), 0.25f, 300.f, 1500.f);
	UE_LOG(LogJam, Log, TEXT("Arma %s pega a %s: %.0f"), Current == EJamWeapon::Sword ? TEXT("espada") : TEXT("arco"), *Target->GetName(), Damage);
	OnWeaponHit.Broadcast(this, Target, Damage);
	return true;
}

void UJamWeaponComponent::SwingSword()
{
	SwingAlpha = 0.f;
	AActor* Owner = GetOwner();
	FVector From, Aim;
	GetAim(From, Aim);
	const FVector Fwd = FVector(Aim.X, Aim.Y, 0.f).GetSafeNormal();
	const float Radius = SwordRange * 0.5f;
	const FVector Center = Owner->GetActorLocation() + Fwd * Radius;

	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(JamSword), false, Owner);
	GetWorld()->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, Objects, FCollisionShape::MakeSphere(Radius), Params);

	TSet<AActor*> Done;
	for (const FOverlapResult& O : Overlaps)
	{
		AActor* A = O.GetActor();
		if (!A || Done.Contains(A)) { continue; }
		Done.Add(A);
		DealDamage(A, SwordDamage + BonusDamage, Fwd * SwordKnockback);
	}
}

void UJamWeaponComponent::ShootArrow()
{
	FVector From, Dir;
	GetAim(From, Dir);
	FActorSpawnParameters P;
	P.Owner = GetOwner();
	P.Instigator = Cast<APawn>(GetOwner());
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AJamArrow* Arrow = GetWorld()->SpawnActor<AJamArrow>(AJamArrow::StaticClass(), From, Dir.Rotation(), P))
	{
		Arrow->Launch(this, Dir * ArrowSpeed, BowDamage + BonusDamage);
	}
}

void UJamWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!HandMesh || Current != EJamWeapon::Sword || SwingAlpha >= 1.f)
	{
		return;
	}
	// Tajo de derecha a izquierda en 0.2 s y regreso.
	SwingAlpha = FMath::Min(1.f, SwingAlpha + DeltaTime / 0.2f);
	const float Yaw = SwingAlpha < 1.f ? FMath::Lerp(60.f, -70.f, SwingAlpha) : 0.f;
	HandMesh->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
	HandMesh->SetRelativeLocation(FVector(45.f, SwingAlpha < 1.f ? FMath::Lerp(45.f, -20.f, SwingAlpha) : 38.f, 0.f));
}
