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
}

void AJamCharacter::BeginPlay()
{
	Super::BeginPlay();
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
