#include "Core/JamPickup.h"
#include "Core/JamLoot.h"
#include "Core/JamAudio.h"
#include "Core/JamMessages.h"
#include "Core/InventoryComponent.h"
#include "Core/HealthComponent.h"
#include "GameJamm.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"

AJamPickup::AJamPickup()
{
	PrimaryActorTick.bCanEverTick = true;
	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->InitSphereRadius(130.f);
	Trigger->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Trigger->SetGenerateOverlapEvents(true);
	RootComponent = Trigger;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Trigger);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Part(TEXT("/Game/Game/Core/Loot/SM_PICKUP_PART.SM_PICKUP_PART"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Mat(TEXT("/Game/Game/Core/Loot/SM_PICKUP_MATERIAL.SM_PICKUP_MATERIAL"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Orb(TEXT("/Game/Game/Core/Loot/SM_PICKUP_ENERGY_ORB.SM_PICKUP_ENERGY_ORB"));
	PartMesh = Part.Object;
	MaterialMesh = Mat.Object;
	OrbMesh = Orb.Object;
}

AJamPickup* AJamPickup::SpawnBundle(UWorld* World, const FVector& At, const TMap<FName, int32>& InItems, float InEnergy, float LifeSeconds)
{
	if (!World || (InItems.Num() == 0 && InEnergy <= 0.f))
	{
		return nullptr;
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AJamPickup* Pick = World->SpawnActorDeferred<AJamPickup>(AJamPickup::StaticClass(), FTransform(At), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Pick)
	{
		Pick->Items = InItems;
		Pick->Energy = InEnergy;
		Pick->FinishSpawning(FTransform(At));
		if (LifeSeconds > 0.f) { Pick->SetLifeSpan(LifeSeconds); }
	}
	return Pick;
}

void AJamPickup::BeginPlay()
{
	Super::BeginPlay();
	Home = GetActorLocation();
	RefreshLook();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AJamPickup::HandleOverlap);
}

void AJamPickup::RefreshLook()
{
	bool bParts = false;
	for (const auto& It : Items) { bParts |= It.Key.ToString().StartsWith(TEXT("Part.")); }
	if (bParts || Items.Num() > 1) { Mesh->SetStaticMesh(PartMesh); Mesh->SetRelativeScale3D(FVector(Items.Num() > 1 ? 1.6f : 1.f)); }
	else if (Items.Num() == 1) { Mesh->SetStaticMesh(MaterialMesh); }
	else
	{
		// Orbe de energia: chico (+15) o grande (+40), el mismo modelo escalado (GDD 4).
		Mesh->SetStaticMesh(OrbMesh);
		Mesh->SetRelativeScale3D(FVector(Energy >= 40.f ? 1.8f : 1.1f));
	}
}

void AJamPickup::HandleOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	const UHealthComponent* H = Pawn ? Pawn->FindComponentByClass<UHealthComponent>() : nullptr;
	if (!Pawn || !Pawn->FindComponentByClass<UInventoryComponent>() || (H && H->IsDead()))
	{
		return;
	}
	TMap<FName, int32> Bundle = Items;
	if (Energy > 0.f)
	{
		if (!Pawn->IsPlayerControlled() && Items.Num() == 0) { return; } // la energia es del jugador (barra unica por ahora)
		Bundle.Add(TEXT("Energy"), FMath::RoundToInt(Energy));
	}
	TMap<FName, int32> Leftover;
	const FString Got = JamLoot::GiveBundle(OtherActor, Bundle, &Leftover);
	UE_LOG(LogJam, Log, TEXT("Loot: %s recoge %s"), *OtherActor->GetName(), *Got);
	if (Pawn->IsPlayerControlled())
	{
		if (!Got.IsEmpty())
		{
			JamMsg::Post(TEXT("Recogiste: ") + Got, JamMsg::Loot, 3.f, TEXT("loot"));
			JamAudio::Play2D(this, TEXT("SFX_Loot_Pickup"), 0.7f);
		}
		if (Leftover.Num() > 0)
		{
			JamMsg::Post(TEXT("Mochila llena: se quedo en el suelo"), JamMsg::Warn, 5.f, TEXT("bag"));
			// Con la mochila llena el pickup sigue en el suelo y el overlap no se repite, asi que no hay spam.
			JamAudio::Play2D(this, TEXT("SFX_Backpack_Full"), 0.7f);
		}
	}
	if (Pawn->IsPlayerControlled()) { Energy = 0.f; }
	Items = Leftover;
	if (Items.Num() == 0 && Energy <= 0.f) { Destroy(); }
	else { RefreshLook(); }
}

void AJamPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float T = GetWorld()->GetTimeSeconds();
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, 10.f * FMath::Sin(T * 2.5f)));
	Mesh->AddRelativeRotation(FRotator(0.f, 90.f * DeltaSeconds, 0.f));
}
