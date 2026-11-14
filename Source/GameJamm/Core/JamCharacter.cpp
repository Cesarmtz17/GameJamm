#include "Core/JamCharacter.h"
#include "Core/HealthComponent.h"
#include "Core/InventoryComponent.h"
#include "Core/JamGameMode.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"

AJamCharacter::AJamCharacter()
{
	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	Health->MaxHealth = 150.f;
	Health->Team = EJamTeam::Red;

	Inventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("Inventory"));
	StartingItems = {
		{ "Part.Plank", 8 }, { "Part.Wheel", 4 }, { "Part.Block", 4 }, { "Part.Wall", 2 }, { "Part.Beam", 2 },
		{ "Part.Rocket", 1 }, { "Part.Flamethrower", 1 }, { "Part.Cannon", 1 }, { "Part.Fan", 2 },
		{ "Part.Balloon", 2 }, { "Part.Battery", 1 } };
}

void AJamCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (Inventory && Inventory->Items.Num() == 0)
	{
		for (const auto& Item : StartingItems) { Inventory->AddItem(Item.Key, Item.Value); }
	}
	Health->OnDied.AddDynamic(this, &AJamCharacter::HandleDied);
}

void AJamCharacter::Tick(float DeltaSeconds)
{
	// Muerto: no se procesan las teclas de construccion ni el movimiento.
	if (Health && Health->IsDead())
	{
		return;
	}
	Super::Tick(DeltaSeconds);
}

void AJamCharacter::HandleDied(UHealthComponent* HealthComponent, AActor* Killer)
{
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetActorHiddenInGame(true);

	if (AJamGameMode* GameMode = GetWorld()->GetAuthGameMode<AJamGameMode>())
	{
		GameMode->NotifyPlayerDied(this, Killer);
	}
}

void AJamCharacter::FellOutOfWorld(const UDamageType& DmgType)
{
	if (Health && !Health->IsDead())
	{
		Health->Kill(nullptr);
	}
}
