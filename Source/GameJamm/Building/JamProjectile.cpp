#include "Building/JamProjectile.h"

#include "Building/JamBuildPart.h"
#include "Building/JamDeviceMath.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HealthComponent.h"
#include "Core/JamStatics.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

AJamProjectile::AJamProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	InitialLifeSpan = 4.f;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Mesh->SetRelativeScale3D(FVector(0.2));
	Mesh->SetNotifyRigidBodyCollision(true);
}

void AJamProjectile::BeginPlay()
{
	Super::BeginPlay();
	// Mismo patrón que AJamBuildPart::InitFromDef: el mesh placeholder se carga en runtime.
	if (UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")))
	{
		Mesh->SetStaticMesh(SM);
	}
	Mesh->SetMassOverrideInKg(NAME_None, 2.f, true);
	Mesh->SetSimulatePhysics(true);
	Mesh->SetEnableGravity(false); // tiro recto: la Head solo apunta en yaw
	Mesh->OnComponentHit.AddDynamic(this, &AJamProjectile::HandleHit);
}

void AJamProjectile::Launch(const FVector& Velocity)
{
	if (Mesh)
	{
		Mesh->SetPhysicsLinearVelocity(Velocity);
	}
}

void AJamProjectile::HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bSpent || !IsValid(OtherActor) || OtherActor == this || OtherActor == GetOwner() || OtherActor == GetInstigator())
	{
		return;
	}
	// Las piezas de construcción no llevan vida: la bala rebota en ellas (incluida la propia construcción).
	if (Cast<AJamBuildPart>(OtherActor))
	{
		return;
	}
	const UHealthComponent* Health = OtherActor->FindComponentByClass<UHealthComponent>();
	if (!Health)
	{
		return;
	}
	bSpent = true;
	if (!Health->IsDead() && JamDevices::AreHostile(UJamStatics::GetActorTeam(this), Health->Team))
	{
		AController* InstigatorController = GetInstigator() ? GetInstigator()->GetController() : nullptr;
		UGameplayStatics::ApplyDamage(OtherActor, Damage, InstigatorController, this, nullptr);
	}
	Destroy();
}
