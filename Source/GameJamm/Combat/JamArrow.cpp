#include "Combat/JamArrow.h"
#include "Combat/JamWeaponComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

AJamArrow::AJamArrow()
{
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(8.f);
	Collision->SetCollisionProfileName(TEXT("Projectile"));
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Collision->SetGenerateOverlapEvents(true);
	RootComponent = Collision;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeLocation(FVector(-30.f, 0.f, 0.f)); // la punta va adelante de la esfera
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ArrowMesh(TEXT("/Game/Game/Combat/Weapons/SM_WPN_ARROW.SM_WPN_ARROW"));
	if (ArrowMesh.Succeeded()) { Mesh->SetStaticMesh(ArrowMesh.Object); }

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->bRotationFollowsVelocity = true;
	Movement->ProjectileGravityScale = 0.35f; // cae poco: a 40 m baja ~1 m
	Movement->bAutoActivate = false;

	InitialLifeSpan = 6.f;
}

void AJamArrow::Launch(UJamWeaponComponent* InWeapon, const FVector& Velocity, float InDamage)
{
	Weapon = InWeapon;
	Damage = InDamage;
	if (AActor* Shooter = InWeapon ? InWeapon->GetOwner() : nullptr)
	{
		Collision->IgnoreActorWhenMoving(Shooter, true);
		SetOwner(Shooter);
		SetInstigator(Cast<APawn>(Shooter));
	}
	Collision->OnComponentHit.AddDynamic(this, &AJamArrow::HandleHit);
	Collision->OnComponentBeginOverlap.AddDynamic(this, &AJamArrow::HandleOverlap);
	Movement->Velocity = Velocity;
	Movement->Activate();
}

void AJamArrow::HandleHit(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, FVector, const FHitResult&)
{
	Strike(OtherActor);
}

void AJamArrow::HandleOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (OtherActor && OtherActor != GetOwner()) { Strike(OtherActor); }
}

void AJamArrow::Strike(AActor* Other)
{
	if (bDone || !Other || Other == GetOwner())
	{
		return;
	}
	UJamWeaponComponent* W = Weapon.Get();
	const bool bHurt = W && W->DealDamage(Other, Damage, Movement->Velocity.GetSafeNormal() * 150.f);
	// Un aliado no la detiene: sigue de largo. Lo demas (enemigo, suelo, pared) la clava.
	if (!bHurt && Other->IsA<APawn>() && W && W->GetOwner() && !Other->IsA<AJamArrow>())
	{
		return;
	}
	bDone = true;
	Movement->StopMovementImmediately();
	Movement->Deactivate();
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (USceneComponent* Root = Other->GetRootComponent()) { AttachToComponent(Root, FAttachmentTransformRules::KeepWorldTransform); }
	SetLifeSpan(3.f);
}
