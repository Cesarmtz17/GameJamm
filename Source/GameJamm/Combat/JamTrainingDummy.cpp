#include "Combat/JamTrainingDummy.h"
#include "Core/HealthComponent.h"
#include "GameJamm.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AJamTrainingDummy::AJamTrainingDummy()
{
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cyl.Succeeded()) { Mesh->SetStaticMesh(Cyl.Object); }
	Mesh->SetWorldScale3D(FVector(0.7f, 0.7f, 1.8f));
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	RootComponent = Mesh;

	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	Health->MaxHealth = 300.f;
	Health->Team = EJamTeam::Blue;
}

void AJamTrainingDummy::BeginPlay()
{
	Super::BeginPlay();
	Health->OnHealthChanged.AddDynamic(this, &AJamTrainingDummy::HandleHealthChanged);
	Health->OnDied.AddDynamic(this, &AJamTrainingDummy::HandleDied);
}

void AJamTrainingDummy::HandleHealthChanged(UHealthComponent*, float NewHealth, float Delta)
{
	if (Delta >= 0.f) { return; }
	UE_LOG(LogJam, Log, TEXT("Dummy %s: -%.0f (queda %.0f)"), *GetName(), -Delta, NewHealth);
	if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Orange, FString::Printf(TEXT("-%.0f"), -Delta)); }
}

void AJamTrainingDummy::HandleDied(UHealthComponent*, AActor*)
{
	UE_LOG(LogJam, Log, TEXT("Dummy %s derribado; revive en 2 s"), *GetName());
	GetWorldTimerManager().SetTimer(ReviveTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { Health->ResetHealth(); }), 2.f, false);
}
