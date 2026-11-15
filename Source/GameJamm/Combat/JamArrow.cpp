#include "Combat/JamArrow.h"
#include "Combat/JamWeaponComponent.h"
#include "Core/HealthComponent.h"
#include "Core/JamHUD.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
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
	const UHealthComponent* TargetHealth = Other->FindComponentByClass<UHealthComponent>();
	const float Before = TargetHealth ? TargetHealth->GetHealth() : 0.f;
	const bool bHurt = W && W->DealDamage(Other, Damage, Movement->Velocity.GetSafeNormal() * Push);
	if (bHurt && TargetHealth) { AJamHUD::ShowDamage(W->GetOwner(), Other, Before - TargetHealth->GetHealth(), Fused); }
	// Un aliado no la detiene: sigue de largo. Lo demas (enemigo, suelo, pared) la clava.
	if (!bHurt && Other->IsA<APawn>() && W && W->GetOwner() && !Other->IsA<AJamArrow>())
	{
		return;
	}
	bDone = true;
	if (W && !Fused.IsNone()) { W->ApplyFusionHit(Fused, bHurt ? Other : nullptr, GetActorLocation(), bFuseArea); }
	Movement->StopMovementImmediately();
	Movement->Deactivate();
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (bShot)
	{
		// Disparo del arma pedida: no se clava, desaparece al pegar.
		SetActorHiddenInGame(true);
		SetLifeSpan(0.05f);
		return;
	}
	if (USceneComponent* Root = Other->GetRootComponent()) { AttachToComponent(Root, FAttachmentTransformRules::KeepWorldTransform); }
	SetLifeSpan(3.f);
}

void AJamArrow::ShowFuse(UStaticMesh* FuseMesh)
{
	if (!FuseMesh) { return; }
	UStaticMeshComponent* Tip = NewObject<UStaticMeshComponent>(this, TEXT("FuseTip"));
	Tip->SetupAttachment(RootComponent);
	Tip->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Tip->SetStaticMesh(FuseMesh);
	Tip->SetRelativeLocation(FVector(20.f, 0.f, 0.f));
	Tip->SetRelativeScale3D(FVector(12.f / FMath::Max(1.f, FuseMesh->GetBoundingBox().GetSize().GetMax())));
	Tip->RegisterComponent();
}

void AJamArrow::SetupShot(UStaticMesh* ShotMesh, const FTransform& MeshInShot, const FLinearColor& Color, bool bGlow, float GravityScale, float LifeSeconds, bool bAreaEffect)
{
	bShot = true;
	bFuseArea = bAreaEffect;
	Movement->ProjectileGravityScale = FMath::Max(0.f, GravityScale);
	SetLifeSpan(FMath::Max(0.1f, LifeSeconds));
	if (!ShotMesh)
	{
		return; // sin malla se queda la flecha: al menos se ve
	}
	Mesh->SetStaticMesh(ShotMesh);
	Mesh->SetRelativeTransform(MeshInShot);
	Mesh->SetCastShadow(false);
	if (bGlow)
	{
		// Mismo material emisivo que las bocanadas de AJamBuildPart::Puff: brilla con el color de la boca.
		UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));
		if (UMaterialInstanceDynamic* MID = Mat ? UMaterialInstanceDynamic::Create(Mat, this) : nullptr)
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color * 4.f);
			for (int32 I = 0; I < Mesh->GetNumMaterials(); ++I) { Mesh->SetMaterial(I, MID); }
		}
	}
}

void AJamArrow::IgnoreShot(AJamArrow* Other)
{
	if (Other && Other != this) { Collision->IgnoreActorWhenMoving(Other, true); }
}
