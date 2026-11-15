#include "Core/JamLootChest.h"
#include "Core/JamLoot.h"
#include "Core/JamPickup.h"
#include "Core/JamAudio.h"
#include "Core/JamMessages.h"
#include "Core/JamHUD.h"
#include "Building/JamBuildPart.h"
#include "Core/InventoryComponent.h"
#include "Core/HealthComponent.h"
#include "GameJamm.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
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

	BeaconLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("LootBeaconLight"));
	BeaconLight->SetupAttachment(Base);
	BeaconLight->SetRelativeLocation(FVector(0.f, 0.f, 140.f));
	BeaconLight->SetCastShadows(false);
	BeaconLight->SetVisibility(false);
	BeaconColumn = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LootBeaconColumn"));
	BeaconColumn->SetupAttachment(Base);
	BeaconColumn->SetUsingAbsoluteScale(true);
	BeaconColumn->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BeaconColumn->SetCastShadow(false);
	BeaconColumn->SetCanEverAffectNavigation(false);
	BeaconColumn->SetVisibility(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Column(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));
	if (Column.Succeeded()) { BeaconColumn->SetStaticMesh(Column.Object); }
	if (Glow.Succeeded()) { BeaconColumn->SetMaterial(0, Glow.Object); }
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
	RefreshBeacon();
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
	RefreshBeacon();
	const TMap<FName, int32> Bundle = FixedItems.Num() > 0 ? FixedItems : JamLoot::Roll(Table);
	TMap<FName, int32> Leftover;
	const float EnergyBefore = AJamBuildPart::GetPlayerEnergy(GetWorld());
	const FString Got = JamLoot::GiveBundle(Taker, Bundle, &Leftover);
	if (Leftover.Num() > 0)
	{
		AJamPickup::SpawnBundle(GetWorld(), GetActorLocation() + GetActorForwardVector() * -120.f + FVector(0, 0, 40.f), Leftover, 0.f, 0.f);
	}
	const FString What = bScrapLook ? TEXT("Scrap") : FixedItems.Num() > 0 ? TEXT("Starter chest") : Table == TEXT("rare") ? TEXT("Rare chest") : TEXT("Chest");
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
		// Filas con iconos y cantidades reales. Otro HUD conserva el aviso de texto como respaldo.
		const bool bIconNotice = AJamHUD::ShowLoot(Taker, Bundle, Leftover, Table, EnergyBefore);
		const bool bRareChest = !bScrapLook && FixedItems.Num() == 0 && Table == TEXT("rare");
		if (!bIconNotice && (!Got.IsEmpty() || Leftover.Num() == 0))
		{
			const FString Line = FString::Printf(TEXT("%s: %s"), *What, Got.IsEmpty() ? TEXT("empty") : *Got);
			if (bRareChest) { JamMsg::Big(Line, JamMsg::Loot, 5.f, TEXT("loot")); }
			else { JamMsg::Post(Line, JamMsg::Loot, 5.f, TEXT("loot")); }
		}
		if (bIconNotice && bRareChest) { JamMsg::Big(TEXT("Rare chest opened"), JamLoot::TierColor(TEXT("rare")), 3.f, TEXT("loot")); }
		if (bIconNotice && Got.IsEmpty() && Leftover.Num() == 0) { JamMsg::Post(What + TEXT(": empty"), JamMsg::Loot, 3.f, TEXT("loot")); }
		if (Leftover.Num() > 0) { JamMsg::Post(TEXT("Backpack full: the rest stayed on the ground"), JamMsg::Warn, 5.f, TEXT("bag")); }
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

void AJamLootChest::RefreshBeacon()
{
	if (!BeaconLight || !BeaconColumn) { return; }
	const bool bVisible = !bOpen && !bScrapLook;
	BeaconLight->SetVisibility(bVisible);
	BeaconColumn->SetVisibility(bVisible);
	if (!bVisible) { return; }

	double Height = 1800.0, Radius = 14.0, Intensity = 12000.0, LightRadius = 900.0, Glow = 1.8;
	FLinearColor Color = JamLoot::TierColor(Table);
	const TSharedPtr<FJsonObject>* Visuals = nullptr;
	const TSharedPtr<FJsonObject>* Beacons = nullptr;
	if (JamLoot::Config()->TryGetObjectField(TEXT("visuals"), Visuals) && (*Visuals)->TryGetObjectField(TEXT("beacons"), Beacons))
	{
		(*Beacons)->TryGetNumberField(TEXT("height_cm"), Height);
		(*Beacons)->TryGetNumberField(TEXT("radius_cm"), Radius);
		(*Beacons)->TryGetNumberField(TEXT("light_intensity"), Intensity);
		(*Beacons)->TryGetNumberField(TEXT("light_radius_cm"), LightRadius);
		(*Beacons)->TryGetNumberField(TEXT("emissive"), Glow);
		const TSharedPtr<FJsonObject>* Colors = nullptr;
		FString Hex;
		if ((*Beacons)->TryGetObjectField(TEXT("colors"), Colors) && (*Colors)->TryGetStringField(Table, Hex)) { Color = FLinearColor(FColor::FromHex(Hex)); }
	}
	BeaconLight->SetIntensity(static_cast<float>(FMath::Max(0.0, Intensity)));
	BeaconLight->SetAttenuationRadius(static_cast<float>(FMath::Max(1.0, LightRadius)));
	BeaconLight->SetLightColor(Color);
	BeaconColumn->SetWorldLocation(GetActorLocation() + FVector(0.f, 0.f, Height * 0.5 + 80.0));
	BeaconColumn->SetRelativeScale3D(FVector(Radius / 50.0, Radius / 50.0, Height / 100.0));
	if (!BeaconMaterial) { BeaconMaterial = BeaconColumn->CreateDynamicMaterialInstance(0); }
	if (BeaconMaterial) { BeaconMaterial->SetVectorParameterValue(TEXT("Color"), Color * static_cast<float>(Glow)); }
}

void AJamLootChest::Refill()
{
	bOpen = false;
	RefreshBeacon();
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
