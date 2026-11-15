#include "Core/JamCharacter.h"
#include "Core/HealthComponent.h"
#include "Core/InventoryComponent.h"
#include "Core/JamGameMode.h"
#include "Core/JamLoot.h"
#include "GameJamm.h"
#include "Core/JamPickup.h"
#include "Building/JamBuildPart.h"
#include "Combat/JamWeaponComponent.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UObjectIterator.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"

AJamCharacter::AJamCharacter()
{
	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	Health->MaxHealth = 150.f;
	Health->Team = EJamTeam::Red;

	Inventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("Inventory"));
	Weapon = CreateDefaultSubobject<UJamWeaponComponent>(TEXT("Weapon"));
	// GDD 4: apareces casi sin nada; lo primero sale del cofre garantizado junto al spawn (Loot.json "starter").
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

	// Armas a pie: montado, el clic es de la construccion; con la caja de texto abierta no se leen teclas.
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!SayBox.IsValid() && SayWidget)
	{
		// UJamSayWidget vive en el .cpp de Building: se ubica por nombre de clase una sola vez.
		for (TObjectIterator<UUserWidget> It; It; ++It)
		{
			if (It->GetWorld() == GetWorld() && It->GetClass()->GetFName() == TEXT("JamSayWidget")) { SayBox = *It; break; }
		}
	}
	const bool bTyping = SayBox.IsValid() && SayBox->IsInViewport();
	if (!PC || !Weapon || bTyping || Cast<AJamBuildPart>(GetAttachParentActor()))
	{
		return;
	}
	if (PC->WasInputKeyJustPressed(EKeys::MouseScrollUp) || PC->WasInputKeyJustPressed(EKeys::MouseScrollDown) ||
		PC->WasInputKeyJustPressed(EKeys::Q) || PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Top))
	{
		Weapon->NextWeapon();
	}
	if (PC->IsInputKeyDown(EKeys::LeftMouseButton) || PC->GetInputAnalogKeyState(EKeys::Gamepad_RightTriggerAxis) > 0.5f)
	{
		Weapon->Attack();
	}
}

void AJamCharacter::HandleDied(UHealthComponent* HealthComponent, AActor* Killer)
{
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// GDD 4 (ya sin colision, para no recogerla uno mismo): bolsa con el 50 % de tus piezas y materiales (redondeado abajo) y orbe con el 50 % de la energia sobre el minimo.
	TMap<FName, int32> Bag;
	if (Inventory)
	{
		for (const auto& It : Inventory->Items) { if (It.Value / 2 > 0) { Bag.Add(It.Key, It.Value / 2); } }
		for (const auto& It : Bag) { Inventory->RemoveItem(It.Key, It.Value); }
	}
	float OrbEnergy = 0.f;
	if (IsPlayerControlled())
	{
		const float Have = AJamBuildPart::GetPlayerEnergy(GetWorld());
		const float Min = JamLoot::EnergyMin();
		if (Have > Min)
		{
			OrbEnergy = FMath::FloorToFloat((Have - Min) * 0.5f);
			AJamBuildPart::SetPlayerEnergy(GetWorld(), Min);
		}
	}
	double BagLife = 30.0;
	JamLoot::Config()->TryGetNumberField(TEXT("death_bag_seconds"), BagLife);
	AJamPickup::SpawnBundle(GetWorld(), GetActorLocation(), Bag, OrbEnergy, float(BagLife));
	UE_LOG(LogJam, Log, TEXT("%s suelta bolsa: %s (+%.0f energia)"), *GetName(), *JamLoot::Describe(Bag), OrbEnergy);

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
