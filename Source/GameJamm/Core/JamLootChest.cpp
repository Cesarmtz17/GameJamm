#include "Core/JamLootChest.h"
#include "Core/JamLoot.h"
#include "Core/JamPickup.h"
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
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AJamLootChest::AJamLootChest()
{
	PrimaryActorTick.bCanEverTick = true;
	Base = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Base"));
	Base->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	RootComponent = Base;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BaseMesh(TEXT("/Game/Game/Core/Loot/SM_PROP_CHEST_BASE.SM_PROP_CHEST_BASE"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> LidMesh(TEXT("/Game/Game/Core/Loot/SM_PROP_CHEST_LID.SM_PROP_CHEST_LID"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> RareBase(TEXT("/Game/Game/Core/Loot/SM_PROP_CHEST_RARE_BASE.SM_PROP_CHEST_RARE_BASE"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> RareLid(TEXT("/Game/Game/Core/Loot/SM_PROP_CHEST_RARE_LID.SM_PROP_CHEST_RARE_LID"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Scrap(TEXT("/Game/Game/Core/Loot/SM_PROP_SCRAP_PILE.SM_PROP_SCRAP_PILE"));
	RareBaseMesh = RareBase.Object;
	RareLidMesh = RareLid.Object;
	ScrapMesh = Scrap.Object;
	if (BaseMesh.Succeeded()) { Base->SetStaticMesh(BaseMesh.Object); }

	Lid = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Lid"));
	Lid->SetupAttachment(Base);
	Lid->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Lid->SetRelativeLocation(FVector(0.f, 36.f, 50.f)); // bisagra en el borde trasero de la base
	if (LidMesh.Succeeded()) { Lid->SetStaticMesh(LidMesh.Object); }

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(Base);
	Trigger->InitSphereRadius(220.f);
	Trigger->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Trigger->SetGenerateOverlapEvents(true);
}

void AJamLootChest::SetLook(bool bRare, bool bScrap)
{
	bScrapLook = bScrap;
	if (bScrap)
	{
		Base->SetStaticMesh(ScrapMesh);
		Lid->SetHiddenInGame(true);
		Lid->SetVisibility(false);
	}
	else if (bRare)
	{
		Base->SetStaticMesh(RareBaseMesh);
		Lid->SetStaticMesh(RareLidMesh);
		SetActorScale3D(FVector(1.3f));
	}
}

void AJamLootChest::BeginPlay()
{
	Super::BeginPlay();
	// El frente de la tapa (Y-) tiene que subir: se elige el signo del giro que lo levanta.
	OpenRoll = FRotator(0.f, 0.f, 100.f).RotateVector(FVector(0.f, -72.f, 0.f)).Z > 0.f ? 100.f : -100.f;
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AJamLootChest::HandleOverlap);
	// Si el jugador ya esta adentro al aparecer (cofre del spawn), se abre en el siguiente overlap o con OpenFor.
}

void AJamLootChest::HandleOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (Pawn && Pawn->FindComponentByClass<UInventoryComponent>()) { OpenFor(OtherActor); }
}

bool AJamLootChest::OpenFor(AActor* Taker)
{
	const UHealthComponent* H = Taker ? Taker->FindComponentByClass<UHealthComponent>() : nullptr;
	if (bOpen || !Taker || (H && H->IsDead()))
	{
		return false;
	}
	bOpen = true;
	const TMap<FName, int32> Bundle = FixedItems.Num() > 0 ? FixedItems : JamLoot::Roll(Table);
	TMap<FName, int32> Leftover;
	const FString Got = JamLoot::GiveBundle(Taker, Bundle, &Leftover);
	if (Leftover.Num() > 0)
	{
		AJamPickup::SpawnBundle(GetWorld(), GetActorLocation() + GetActorForwardVector() * -120.f + FVector(0, 0, 40.f), Leftover, 0.f, 0.f);
	}
	const FString What = bScrapLook ? TEXT("Chatarra") : FixedItems.Num() > 0 ? TEXT("Cofre inicial") : Table == TEXT("rare") ? TEXT("Cofre raro") : TEXT("Cofre");
	UE_LOG(LogJam, Log, TEXT("Loot: %s abre %s (%s): %s"), *Taker->GetName(), *What, *Table, *Got);
	const APawn* Pawn = Cast<APawn>(Taker);
	// Sonido: el cofre se abre (la chatarra suena a recoger); si es el jugador y se llevo algo, el pitido de loot; si no cupo, mochila llena.
	JamAudio::PlayAt(this, bScrapLook ? TEXT("SFX_Loot_Pickup") : TEXT("SFX_Chest_Open"), GetActorLocation(), 0.8f);
	if (Pawn && Pawn->IsPlayerControlled())
	{
		if (!Got.IsEmpty() && !bScrapLook) { JamAudio::Play2D(this, TEXT("SFX_Loot_Pickup"), 0.6f); }
		if (Leftover.Num() > 0) { JamAudio::Play2D(this, TEXT("SFX_Backpack_Full"), 0.7f); }
	}
	if (Pawn && Pawn->IsPlayerControlled())
	{
		// Aviso al jugador: lo que recibio ("Cofre: +2 Rueda, +1 Lanzallamas"); el cofre raro va como aviso grande.
		const bool bRareChest = !bScrapLook && FixedItems.Num() == 0 && Table == TEXT("rare");
		if (!Got.IsEmpty() || Leftover.Num() == 0)
		{
			const FString Line = FString::Printf(TEXT("%s: %s"), *What, Got.IsEmpty() ? TEXT("vacio") : *Got);
			if (bRareChest) { JamMsg::Big(Line, JamMsg::Loot, 5.f, TEXT("loot")); }
			else { JamMsg::Post(Line, JamMsg::Loot, 5.f, TEXT("loot")); }
		}
		if (Leftover.Num() > 0) { JamMsg::Post(TEXT("Mochila llena: se quedo en el suelo"), JamMsg::Warn, 5.f, TEXT("bag")); }
		if (!Message.IsEmpty()) { JamMsg::Post(Message, JamMsg::Info, 10.f, TEXT("tip")); }
	}
	if (bScrapLook) { SetActorHiddenInGame(true); SetActorEnableCollision(false); }
	if (RespawnSeconds > 0.f)
	{
		GetWorldTimerManager().SetTimer(RefillTimer, this, &AJamLootChest::Refill, RespawnSeconds, false);
	}
	else if (bScrapLook)
	{
		SetLifeSpan(1.f);
	}
	return true;
}

void AJamLootChest::Refill()
{
	bOpen = false;
	if (bScrapLook) { SetActorHiddenInGame(false); SetActorEnableCollision(true); }
	// Quien se quedo parado encima lo abre otra vez al reaparecer.
	TArray<AActor*> Inside;
	Trigger->GetOverlappingActors(Inside, APawn::StaticClass());
	for (AActor* A : Inside) { if (A->FindComponentByClass<UInventoryComponent>()) { OpenFor(A); break; } }
}

void AJamLootChest::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bScrapLook) { return; }
	const float Target = bOpen ? 1.f : 0.f;
	if (!FMath::IsNearlyEqual(LidAlpha, Target))
	{
		LidAlpha = FMath::FInterpConstantTo(LidAlpha, Target, DeltaSeconds, 3.f);
		Lid->SetRelativeRotation(FRotator(0.f, 0.f, OpenRoll * FMath::InterpEaseOut(0.f, 1.f, LidAlpha, 2.f)));
	}
}
