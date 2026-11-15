#include "Combat/JamWeaponComponent.h"
#include "Combat/JamArrow.h"
#include "Combat/JamStatusComponent.h"
#include "Core/InventoryComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Building/JamPlayConsole.h"
#include "Building/JamDecisionClient.h"
#include "Core/HealthComponent.h"
#include "Core/JamStatics.h"
#include "GameJamm.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Containers/Ticker.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Building/JamBuildPart.h"
#include "Combat/JamGunSpec.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/JamMessages.h"
#include "Core/JamAudio.h"
#include "Core/JamHUD.h"
#include "Engine/SkeletalMesh.h"
#include "ReferenceSkeleton.h"

namespace
{
	TAutoConsoleVariable<float> CVarJamHitStop(TEXT("jam.Sword.HitStop"), 0.06f,
		TEXT("Segundos (reales) que se congela el juego cuando la espada del jugador pega; 0 = sin pausa."));
	TAutoConsoleVariable<int32> CVarJamSwordDebug(TEXT("jam.Sword.Debug"), 0,
		TEXT("1 = dibuja el alcance del tajo (arco) y lo que pega, y escribe en el log lo que descarta."));

	void JamApplyDamageWithHud(AActor* Target, float Damage, AController* By, AActor* Source, FName Effect)
	{
		const UHealthComponent* H = Target ? Target->FindComponentByClass<UHealthComponent>() : nullptr;
		const float Before = H ? H->GetHealth() : 0.f;
		UGameplayStatics::ApplyDamage(Target, Damage, By, Source, nullptr);
		if (H) { AJamHUD::ShowDamage(Source, Target, Before - H->GetHealth(), Effect); }
	}
}

namespace
{
	/** Content/Game/Combat/Data/Fusion.json, leido una vez. */
	TSharedPtr<FJsonObject> FusionConfig()
	{
		static TSharedPtr<FJsonObject> Cached;
		if (!Cached.IsValid())
		{
			FString Text;
			const FString Path = FPaths::ProjectContentDir() / TEXT("Game/Combat/Data/Fusion.json");
			TSharedPtr<FJsonObject> Parsed;
			if (FFileHelper::LoadFileToString(Text, *Path) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Parsed) && Parsed.IsValid())
			{
				Cached = Parsed;
			}
			else
			{
				UE_LOG(LogJam, Warning, TEXT("Fusion: no pude leer %s; uso valores por defecto"), *Path);
				Cached = MakeShared<FJsonObject>();
			}
		}
		return Cached;
	}

	TSharedPtr<FJsonObject> MatConfig(FName Id)
	{
		const TSharedPtr<FJsonObject>* Mats = nullptr;
		const TSharedPtr<FJsonObject>* M = nullptr;
		if (FusionConfig()->TryGetObjectField(TEXT("materials"), Mats) && (*Mats)->TryGetObjectField(Id.ToString(), M)) { return *M; }
		return nullptr;
	}

	FString MatName(FName Id)
	{
		FString Name = Id == TEXT("Shock") ? FString(TEXT("Lightning")) : Id.ToString();
		if (const TSharedPtr<FJsonObject> M = MatConfig(Id)) { M->TryGetStringField(TEXT("name"), Name); }
		return Name;
	}

	FName InvId(FName Id) { return FName(*(TEXT("Mat.") + Id.ToString())); }
}

UJamWeaponComponent::UJamWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sword(TEXT("/Game/Game/Combat/Weapons/SM_WPN_SWORD.SM_WPN_SWORD"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Bow(TEXT("/Game/Game/Combat/Weapons/SM_WPN_BOW.SM_WPN_BOW"));
	if (Sword.Succeeded()) { SwordMesh = Sword.Object; }
	if (Bow.Succeeded()) { BowMesh = Bow.Object; }
	static ConstructorHelpers::FObjectFinder<UStaticMesh> FFire(TEXT("/Game/Game/Combat/Fusion/SM_FUSE_FIRE.SM_FUSE_FIRE"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> FIce(TEXT("/Game/Game/Combat/Fusion/SM_FUSE_ICE.SM_FUSE_ICE"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> FShock(TEXT("/Game/Game/Combat/Fusion/SM_FUSE_SHOCK.SM_FUSE_SHOCK"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> FRock(TEXT("/Game/Game/Combat/Fusion/SM_FUSE_ROCK.SM_FUSE_ROCK"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> FBomb(TEXT("/Game/Game/Combat/Fusion/SM_FUSE_BOMB.SM_FUSE_BOMB"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> FSpike(TEXT("/Game/Game/Combat/Fusion/SM_FUSE_SPIKE.SM_FUSE_SPIKE"));
	FuseMeshes.Add(TEXT("Fire"), FFire.Object);
	FuseMeshes.Add(TEXT("Ice"), FIce.Object);
	FuseMeshes.Add(TEXT("Shock"), FShock.Object);
	FuseMeshes.Add(TEXT("Rock"), FRock.Object);
	FuseMeshes.Add(TEXT("Bomb"), FBomb.Object);
	FuseMeshes.Add(TEXT("Spike"), FSpike.Object);
}

void UJamWeaponComponent::BeginPlay()
{
	Super::BeginPlay();
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->GetRootComponent())
	{
		return;
	}
	HandMesh = NewObject<UStaticMeshComponent>(Owner, TEXT("WeaponInHand"));
	HandMesh->SetupAttachment(Owner->GetRootComponent());
	HandMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HandMesh->SetCastShadow(true);
	HandMesh->RegisterComponent();
	FuseMesh = NewObject<UStaticMeshComponent>(Owner, TEXT("FuseOnWeapon"));
	FuseMesh->SetupAttachment(HandMesh);
	FuseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FuseMesh->RegisterComponent();
	RefreshHandMesh();
}

void UJamWeaponComponent::AttachToHand(USceneComponent* Skeleton, FName InSwordSocket, const FTransform& Sword, FName InBowSocket, const FTransform& Bow)
{
	if (!Skeleton)
	{
		return;
	}
	HandParent = Skeleton;
	SwordSocket = InSwordSocket;
	BowSocket = InBowSocket;
	SwordInHand = Sword;
	BowInHand = Bow;
	SwingAlpha = 1.f;
	PlaceGun(); // si ya habia arma pedida, pasa al hueso de la mano
	RefreshHandMesh();
}

void UJamWeaponComponent::RefreshHandMesh()
{
	if (!HandMesh)
	{
		return;
	}
	// Arma pedida en la mano: se ven sus piezas y se esconden la espada / el arco (y al reves).
	const bool bGun = Current == EJamWeapon::Generated && bHasGun;
	HandMesh->SetVisibility(!bGun);
	if (GunRoot)
	{
		GunRoot->SetVisibility(bGun, true);
		if (GunFlash) { GunFlash->SetVisibility(false); }
	}
	if (bGun)
	{
		if (FuseMesh) { FuseMesh->SetVisibility(false); }
		return;
	}
	if (USceneComponent* Parent = HandParent.Get())
	{
		// En la mano de la figura: sigue al hueso al caminar y al atacar.
		const bool bSword = Current == EJamWeapon::Sword;
		HandMesh->SetStaticMesh(bSword ? SwordMesh : BowMesh);
		HandMesh->AttachToComponent(Parent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, bSword ? SwordSocket : BowSocket);
		HandMesh->SetRelativeTransform(bSword ? SwordInHand : BowInHand);
		if (const UStaticMesh* M = HandMesh->GetStaticMesh())
		{
			UE_LOG(LogJam, Verbose, TEXT("Arma en mano %s: caja %s"), *M->GetName(), *M->GetBoundingBox().ToString());
		}
		RefreshFuseMesh();
		return;
	}
	if (Current == EJamWeapon::Sword)
	{
		HandMesh->SetStaticMesh(SwordMesh);
		HandMesh->SetRelativeLocation(FVector(45.f, 38.f, 0.f)); // a la derecha, apuntando al frente
		HandMesh->SetRelativeRotation(FRotator(0.f, 0.f, 0.f));
	}
	else
	{
		HandMesh->SetStaticMesh(BowMesh);
		HandMesh->SetRelativeLocation(FVector(35.f, 30.f, 10.f));
		HandMesh->SetRelativeRotation(FRotator(0.f, 0.f, 0.f));
	}
	RefreshFuseMesh();
}

void UJamWeaponComponent::RefreshFuseMesh()
{
	if (!FuseMesh || !HandMesh)
	{
		return;
	}
	const FName Id = GetFused();
	const TObjectPtr<UStaticMesh>* M = Id.IsNone() ? nullptr : FuseMeshes.Find(Id);
	UStaticMesh* Weapon = HandMesh->GetStaticMesh();
	if (!M || !*M || !Weapon)
	{
		FuseMesh->SetVisibility(false);
		return;
	}
	FuseMesh->SetStaticMesh(*M);
	FuseMesh->SetVisibility(true);
	// En la punta: el extremo del eje mas largo del arma (espada) o el centro del arco; ~22 cm de grande.
	const FBox B = Weapon->GetBoundingBox();
	const FVector Size = B.GetSize();
	FVector Tip = B.GetCenter();
	if (Current == EJamWeapon::Sword)
	{
		const int32 Axis = Size.X >= Size.Y && Size.X >= Size.Z ? 0 : (Size.Y >= Size.Z ? 1 : 2);
		Tip[Axis] = FMath::Abs(B.Max[Axis]) >= FMath::Abs(B.Min[Axis]) ? B.Max[Axis] : B.Min[Axis];
	}
	FuseMesh->SetRelativeLocation(Tip);
	const float FuseSize = FMath::Max(1.f, (*M)->GetBoundingBox().GetSize().GetMax());
	FuseMesh->SetRelativeScale3D(FVector(22.f / FuseSize));
}

FName UJamWeaponComponent::GetFused() const
{
	if (Current == EJamWeapon::Generated) { return NAME_None; } // el arma pedida no lleva material: su boca trae el efecto
	return Current == EJamWeapon::Sword ? FusedSword : FusedBow;
}

int32 UJamWeaponComponent::GetFusedUses() const
{
	if (Current == EJamWeapon::Sword) { return FusedSword.IsNone() ? 0 : SwordUses; }
	if (Current == EJamWeapon::Generated) { return 0; }
	const UInventoryComponent* Inv = GetOwner() ? GetOwner()->FindComponentByClass<UInventoryComponent>() : nullptr;
	return FusedBow.IsNone() || !Inv ? 0 : Inv->GetCount(InvId(FusedBow));
}

float UJamWeaponComponent::MatNumber(FName MaterialId, const TCHAR* Field, float Default) const
{
	double V = Default;
	if (const TSharedPtr<FJsonObject> M = MatConfig(MaterialId)) { M->TryGetNumberField(Field, V); }
	return float(V);
}

float UJamWeaponComponent::GetInterval() const
{
	if (Current == EJamWeapon::Bow) { return BowInterval; }
	if (Current == EJamWeapon::Generated) { return bHasGun ? Gun.Interval : BowInterval; }
	return SwordInterval * (FusedSword.IsNone() ? 1.f : MatNumber(FusedSword, TEXT("sword_interval"), 1.f));
}

void UJamWeaponComponent::Notify(const FString& Text) const
{
	Notify(Text, JamMsg::Warn, NAME_None);
}

void UJamWeaponComponent::Notify(const FString& Text, const FLinearColor& Color, FName Key) const
{
	// Avisos del jugador por JamMsg (los dibuja el HUD y se ven en Shipping; JamMsg ya los escribe en el log).
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn && Pawn->IsPlayerControlled()) { JamMsg::Post(Text, Color, 4.f, Key); }
	else { UE_LOG(LogJam, Log, TEXT("Arma: %s"), *Text); }
}

bool UJamWeaponComponent::Fuse(FName MaterialId)
{
	if (Current == EJamWeapon::Generated)
	{
		Notify(TEXT("Your requested weapon already has an effect. Switch to sword or bow (Q) to fuse materials"));
		return false;
	}
	UInventoryComponent* Inv = GetOwner() ? GetOwner()->FindComponentByClass<UInventoryComponent>() : nullptr;
	if (!Inv || !MatConfig(MaterialId).IsValid() || Inv->GetCount(InvId(MaterialId)) <= 0)
	{
		Notify(FString::Printf(TEXT("No %s available to fuse"), *MatName(MaterialId)));
		return false;
	}
	if (Current == EJamWeapon::Sword)
	{
		Inv->RemoveItem(InvId(MaterialId), 1); // el anterior se pierde
		FusedSword = MaterialId;
		double Uses = 20.0;
		FusionConfig()->TryGetNumberField(TEXT("sword_uses"), Uses);
		SwordUses = FMath::RoundToInt(MatNumber(MaterialId, TEXT("sword_uses"), float(Uses)));
		Notify(FString::Printf(TEXT("Sword + %s (%d hits)"), *MatName(MaterialId), SwordUses), JamMsg::Good, NAME_None);
	}
	else
	{
		FusedBow = MaterialId;
		Notify(FString::Printf(TEXT("%s arrows (%d)"), *MatName(MaterialId), Inv->GetCount(InvId(MaterialId))), JamMsg::Good, NAME_None);
	}
	RefreshFuseMesh();
	return true;
}

bool UJamWeaponComponent::FuseNext()
{
	if (Current == EJamWeapon::Generated)
	{
		return Fuse(NAME_None); // avisa que el arma pedida no se fusiona
	}
	const UInventoryComponent* Inv = GetOwner() ? GetOwner()->FindComponentByClass<UInventoryComponent>() : nullptr;
	TArray<FString> Order;
	FusionConfig()->TryGetStringArrayField(TEXT("order"), Order);
	if (Order.Num() == 0) { Order = { TEXT("Fire"), TEXT("Ice"), TEXT("Shock"), TEXT("Rock"), TEXT("Bomb") }; }
	const int32 Start = Order.IndexOfByKey(GetFused().ToString());
	for (int32 i = 1; i <= Order.Num() && Inv; ++i)
	{
		const FName Id(*Order[(Start + i + Order.Num()) % Order.Num()]);
		if (Inv->GetCount(InvId(Id)) > 0 && (Id != GetFused() || Current == EJamWeapon::Sword))
		{
			return Fuse(Id);
		}
	}
	Notify(TEXT("No fusion materials: find them in chests and scrap"));
	return false;
}

TArray<AActor*> UJamWeaponComponent::EnemiesNear(const FVector& Where, float Radius) const
{
	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	GetWorld()->OverlapMultiByObjectType(Overlaps, Where, FQuat::Identity, Objects, FCollisionShape::MakeSphere(Radius));
	TArray<AActor*> Out;
	for (const FOverlapResult& O : Overlaps)
	{
		AActor* A = O.GetActor();
		const UHealthComponent* H = A ? A->FindComponentByClass<UHealthComponent>() : nullptr;
		if (H && !H->IsDead() && !Out.Contains(A) && UJamStatics::AreEnemies(GetOwner(), A)) { Out.Add(A); }
	}
	return Out;
}

void UJamWeaponComponent::Explode(const FVector& Where, float Damage, float Radius)
{
	AActor* Owner = GetOwner();
	const APawn* Pawn = Cast<APawn>(Owner);
	AController* By = Pawn ? Pawn->GetController() : nullptr;
	int32 Hits = 0;
	for (AActor* A : EnemiesNear(Where, Radius))
	{
		JamApplyDamageWithHud(A, Damage, By, Owner, TEXT("Bomb"));
		if (ACharacter* C = Cast<ACharacter>(A)) { C->LaunchCharacter((A->GetActorLocation() - Where).GetSafeNormal2D() * 700.f + FVector(0, 0, 350.f), true, true); }
		++Hits;
	}
	// Tambien a ti si estas pegado (GDD 8.2).
	if (Owner && FVector::Dist(Owner->GetActorLocation(), Where) < Radius * 0.5f)
	{
		UGameplayStatics::ApplyDamage(Owner, Damage * 0.5f, By, Owner, nullptr);
	}
	AJamTestCharacter::AddTrauma(GetWorld(), Where, 0.6f, 400.f, 2500.f);
	UE_LOG(LogJam, Log, TEXT("Fusion: bomba explota en %s (%d enemigos, %.0f de dano)"), *Where.ToCompactString(), Hits, Damage);
}

void UJamWeaponComponent::ApplyFusionHit(FName Id, AActor* Target, const FVector& Where, bool bArrow)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	AController* By = Pawn ? Pawn->GetController() : nullptr;
	const FString Key = Id.ToString();
	if (Key == TEXT("Bomb"))
	{
		Explode(Where, MatNumber(Id, TEXT("blast_damage"), 35.f), MatNumber(Id, TEXT("blast_radius"), 250.f));
	}
	else if (Key == TEXT("Fire"))
	{
		TArray<AActor*> Burn = bArrow ? EnemiesNear(Where, MatNumber(Id, TEXT("arrow_radius"), 150.f)) : TArray<AActor*>();
		if (Target) { Burn.AddUnique(Target); }
		for (AActor* A : Burn) { UJamStatusComponent::Get(A)->Burn(MatNumber(Id, TEXT("burn_dps"), 5.f), MatNumber(Id, TEXT("burn_seconds"), 3.f), By, GetOwner()); }
	}
	else if (Key == TEXT("Ice") && Target)
	{
		UJamStatusComponent* S = UJamStatusComponent::Get(Target);
		if (bArrow) { S->Freeze(MatNumber(Id, TEXT("arrow_freeze_seconds"), 1.5f)); }
		else
		{
			S->Slow(MatNumber(Id, TEXT("slow"), 0.5f), MatNumber(Id, TEXT("slow_seconds"), 2.f));
			if (S->AddIceHit() >= FMath::RoundToInt(MatNumber(Id, TEXT("freeze_hits"), 3.f))) { S->Freeze(MatNumber(Id, TEXT("freeze_seconds"), 1.f)); }
		}
	}
	else if (Key == TEXT("Shock"))
	{
		const float Stun = MatNumber(Id, TEXT("stun_seconds"), 0.5f);
		if (bArrow)
		{
			// Descarga en area de 2 m.
			for (AActor* A : EnemiesNear(Where, MatNumber(Id, TEXT("arrow_radius"), 200.f)))
			{
				if (A != Target) { JamApplyDamageWithHud(A, MatNumber(Id, TEXT("arrow_area_damage"), 10.f), By, GetOwner(), Id); }
				UJamStatusComponent::Get(A)->Stun(Stun);
			}
		}
		else if (Target) { UJamStatusComponent::Get(Target)->Stun(Stun); }
	}
}

bool UJamWeaponComponent::IsReady() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() - LastAttackTime >= GetInterval();
}

bool UJamWeaponComponent::Attack()
{
	const UHealthComponent* Health = GetOwner() ? GetOwner()->FindComponentByClass<UHealthComponent>() : nullptr;
	if (!IsReady() || (Health && Health->IsDead()))
	{
		return false;
	}
	if (Current == EJamWeapon::Generated)
	{
		return FireGenerated(); // dispara al instante con la energia del jugador (GDD 5.1)
	}
	const float Now = GetWorld()->GetTimeSeconds();
	LastAttackTime = Now;
	// Voltea a la mira (la camara puede ver a otro lado que el cuerpo) y espera a que la animacion llegue al golpe.
	FVector From, Aim;
	GetAim(From, Aim);
	// Espada: hacia donde apunta el control (el centro de la camara puede no estar en linea con el cuerpo); arco: hacia el blanco.
	const APawn* AimPawn = Cast<APawn>(GetOwner());
	FaceYaw = Current == EJamWeapon::Bow || !AimPawn || !AimPawn->GetController() ? Aim.Rotation().Yaw : AimPawn->GetControlRotation().Yaw;
	const float Delay = Current == EJamWeapon::Sword ? SwordHitDelay : BowReleaseDelay;
	FaceUntil = Now + Delay + 0.2f;
	if (!GetOwner()->GetAttachParentActor()) { GetOwner()->SetActorRotation(FRotator(0.f, FaceYaw, 0.f)); }
	PendingKind = Current;
	PendingAt = Now + Delay;
	if (Delay <= 0.f) { PendingAt = -1.f; if (PendingKind == EJamWeapon::Sword) { SwingSword(); } else { ShootArrow(); } }
	OnWeaponAttack.Broadcast(this, Current);
	return true;
}

void UJamWeaponComponent::NextWeapon()
{
	// Espada > arco > arma pedida (si ya pediste una y jam.Weapon.Generated esta encendida) > espada.
	if (Current == EJamWeapon::Sword) { SetWeapon(EJamWeapon::Bow); }
	else if (Current == EJamWeapon::Bow) { SetWeapon(HasGeneratedWeapon() && IsGeneratedEnabled() ? EJamWeapon::Generated : EJamWeapon::Sword); }
	else { SetWeapon(EJamWeapon::Sword); }
}

void UJamWeaponComponent::SetWeapon(EJamWeapon Weapon)
{
	if (Weapon == EJamWeapon::Generated && !(bHasGun && IsGeneratedEnabled())) { Weapon = EJamWeapon::Sword; } // aun no hay arma pedida
	Current = Weapon;
	SwingAlpha = 1.f;
	RefreshHandMesh();
}

void UJamWeaponComponent::GetAim(FVector& OutFrom, FVector& OutDir, float Range, FVector* OutTarget) const
{
	const float Reach = Range > 0.f ? Range : BowRange;
	const AActor* Owner = GetOwner();
	const APawn* Pawn = Cast<APawn>(Owner);
	const FRotator Control = Pawn && Pawn->GetController() ? Pawn->GetController()->GetControlRotation() : Owner->GetActorRotation();
	OutFrom = Owner->GetActorLocation() + FRotator(0.f, Control.Yaw, 0.f).Vector() * 50.f + FVector(0, 0, 40.f);
	OutDir = Control.Vector();
	FVector Target = OutFrom + OutDir * Reach;

	// Con camara: a donde mira el centro de la pantalla (hasta el alcance), para que la flecha vaya a la mira.
	if (const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr)
	{
		FVector ViewLoc;
		FRotator ViewRot;
		PC->GetPlayerViewPoint(ViewLoc, ViewRot);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(JamBowAim), false, Owner);
		FHitResult Hit;
		const FVector End = ViewLoc + ViewRot.Vector() * Reach;
		Target = GetWorld()->LineTraceSingleByChannel(Hit, ViewLoc + ViewRot.Vector() * 200.f, End, ECC_Visibility, Params) ? Hit.ImpactPoint : End;
		OutDir = (Target - OutFrom).GetSafeNormal();
	}
	if (OutTarget) { *OutTarget = Target; }
}

bool UJamWeaponComponent::DealDamage(AActor* Target, float Damage, const FVector& Push)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Target || Target == Owner || !UJamStatics::AreEnemies(Owner, Target))
	{
		return false;
	}
	const UHealthComponent* TargetHealth = Target->FindComponentByClass<UHealthComponent>();
	if (!TargetHealth || TargetHealth->IsDead())
	{
		return false;
	}
	const APawn* Pawn = Cast<APawn>(Owner);
	UGameplayStatics::ApplyDamage(Target, Damage, Pawn ? Pawn->GetController() : nullptr, Owner, nullptr);
	if (ACharacter* Char = Cast<ACharacter>(Target))
	{
		Char->LaunchCharacter(Push + FVector(0, 0, Push.IsNearlyZero() ? 0.f : 180.f), true, false);
	}
	AJamTestCharacter::AddTrauma(GetWorld(), Target->GetActorLocation(), 0.25f, 300.f, 1500.f);
	JamAudio::PlayAt(this, TEXT("SFX_Hit"), Target->GetActorLocation(), 0.7f);
	UE_LOG(LogJam, Log, TEXT("Arma %s pega a %s: %.0f"), Current == EJamWeapon::Sword ? TEXT("espada") : (Current == EJamWeapon::Bow ? TEXT("arco") : TEXT("arma pedida")), *Target->GetName(), Damage);
	OnWeaponHit.Broadcast(this, Target, Damage);
	return true;
}

void UJamWeaponComponent::SwingSword()
{
	SwingAlpha = 0.f;
	AActor* Owner = GetOwner();
	if (Owner)
	{
		JamAudio::PlayAt(this, TEXT("SFX_Sword_Swing"), Owner->GetActorLocation(), 0.6f);
	}
	const FVector Fwd = FRotator(0.f, FaceYaw, 0.f).Vector();
	const FVector Origin = Owner->GetActorLocation();
	const float Radius = SwordRange;
	const FVector Center = Origin;
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(SwordArc));
	const bool bDebug = CVarJamSwordDebug.GetValueOnGameThread() != 0;
	if (bDebug)
	{
		const FVector Z(0, 0, 1);
		DrawDebugLine(GetWorld(), Origin, Origin + Fwd.RotateAngleAxis(SwordArc, Z) * Radius, FColor::Yellow, false, 1.f, 0, 2.f);
		DrawDebugLine(GetWorld(), Origin, Origin + Fwd.RotateAngleAxis(-SwordArc, Z) * Radius, FColor::Yellow, false, 1.f, 0, 2.f);
		DrawDebugCircleArc(GetWorld(), Origin, Radius, Fwd, FMath::DegreesToRadians(SwordArc), 16, FColor::Yellow, false, 1.f, 0, 2.f);
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(JamSword), false, Owner);
	GetWorld()->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, Objects, FCollisionShape::MakeSphere(Radius), Params);

	const FName Fused = FusedSword;
	const float Bonus = Fused.IsNone() ? 0.f : MatNumber(Fused, TEXT("sword_damage"), 0.f);
	const float PushMul = Fused.IsNone() ? 1.f : MatNumber(Fused, TEXT("sword_push"), 1.f);
	TSet<AActor*> Done;
	bool bHit = false;
	for (const FOverlapResult& O : Overlaps)
	{
		AActor* A = O.GetActor();
		if (!A || A == Owner || Done.Contains(A)) { continue; }
		// Punto mas cercano del objetivo (torres grandes: su borde, no su centro).
		FVector Point = A->GetActorLocation();
		if (UPrimitiveComponent* C = O.GetComponent())
		{
			FVector Closest;
			if (C->GetClosestPointOnCollision(Origin, Closest) >= 0.f) { Point = Closest; }
		}
		FVector To = Point - Origin;
		To.Z = 0.f;
		const float Dist = To.Size();
		// Solo lo que esta frente al cuerpo (o pegado a el) y a la vista: no pega por la espalda ni a traves de paredes.
		if (Dist > 60.f && FVector::DotProduct(To / Dist, Fwd) < MinDot)
		{
			if (bDebug) { UE_LOG(LogJam, Log, TEXT("Espada: %s fuera del arco"), *A->GetName()); }
			continue;
		}
		FHitResult Block;
		FCollisionQueryParams Sight(SCENE_QUERY_STAT(JamSwordSight), false, Owner);
		Sight.AddIgnoredActor(A);
		if (GetWorld()->LineTraceSingleByChannel(Block, Origin + FVector(0, 0, 40.f), Point, ECC_Visibility, Sight) && !Cast<APawn>(Block.GetActor()))
		{
			if (bDebug) { UE_LOG(LogJam, Log, TEXT("Espada: %s tapado por %s"), *A->GetName(), *GetNameSafe(Block.GetActor())); }
			continue;
		}
		Done.Add(A);
		if (bDebug) { DrawDebugSphere(GetWorld(), Point, 20.f, 8, FColor::Red, false, 1.f); }
		const UHealthComponent* TargetHealth = A->FindComponentByClass<UHealthComponent>();
		const float Before = TargetHealth ? TargetHealth->GetHealth() : 0.f;
		if (DealDamage(A, SwordDamage + BonusDamage + Bonus, Fwd * SwordKnockback * PushMul))
		{
			if (TargetHealth) { AJamHUD::ShowDamage(Owner, A, Before - TargetHealth->GetHealth(), Fused); }
			if (!Fused.IsNone() && !(bHit && Fused == TEXT("Bomb"))) { ApplyFusionHit(Fused, A, A->GetActorLocation(), false); }
			bHit = true;
		}
	}
	// Pausa de impacto (solo el jugador): un instante congelado hace que el golpe se sienta.
	const APawn* OwnerPawn = Cast<APawn>(Owner);
	const float Stop = CVarJamHitStop.GetValueOnGameThread();
	if (bHit && Stop > 0.f && OwnerPawn && OwnerPawn->IsPlayerControlled() && FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(GetWorld()), 1.f))
	{
		UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 0.05f);
		TWeakObjectPtr<UWorld> WeakWorld(GetWorld());
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakWorld](float)
		{
			if (WeakWorld.IsValid()) { UGameplayStatics::SetGlobalTimeDilation(WeakWorld.Get(), 1.f); }
			return false;
		}), Stop);
	}
	// El material se gasta con el uso: un golpe por tajo que pega; al acabarse se cae.
	if (bHit && !Fused.IsNone() && --SwordUses <= 0)
	{
		Notify(FString::Printf(TEXT("Sword fusion depleted (%s)"), *MatName(Fused)));
		FusedSword = NAME_None;
		SwordUses = 0;
		RefreshFuseMesh();
	}
}

void UJamWeaponComponent::ShootArrow()
{
	FVector From, Dir;
	GetAim(From, Dir);
	FActorSpawnParameters P;
	P.Owner = GetOwner();
	P.Instigator = Cast<APawn>(GetOwner());
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AJamArrow* Arrow = GetWorld()->SpawnActor<AJamArrow>(AJamArrow::StaticClass(), From, Dir.Rotation(), P))
	{
		// Cada flecha fusionada gasta 1 material de la mochila; sin material vuelve a ser flecha normal.
		FName Fused = FusedBow;
		UInventoryComponent* Inv = GetOwner()->FindComponentByClass<UInventoryComponent>();
		if (!Fused.IsNone() && !(Inv && Inv->RemoveItem(InvId(Fused), 1)))
		{
			Notify(FString::Printf(TEXT("No %s left: normal arrows"), *MatName(Fused)));
			Fused = FusedBow = NAME_None;
			RefreshFuseMesh();
		}
		const float Bonus = Fused.IsNone() ? 0.f : MatNumber(Fused, TEXT("arrow_damage"), 0.f);
		Arrow->Fused = Fused;
		Arrow->Push = Fused.IsNone() ? 150.f : MatNumber(Fused, TEXT("arrow_push"), 150.f);
		if (const TObjectPtr<UStaticMesh>* M = Fused.IsNone() ? nullptr : FuseMeshes.Find(Fused)) { Arrow->ShowFuse(*M); }
		Arrow->Launch(this, Dir * ArrowSpeed, BowDamage + BonusDamage + Bonus);
		if (!Fused.IsNone() && Inv && Inv->GetCount(InvId(Fused)) <= 0) { FusedBow = NAME_None; RefreshFuseMesh(); }
	}
}

void UJamWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const float Now = GetWorld()->GetTimeSeconds();
	AActor* Owner = GetOwner();
	if (Now < FaceUntil && Owner && !Owner->GetAttachParentActor()) { Owner->SetActorRotation(FRotator(0.f, FaceYaw, 0.f)); }
	if (PendingAt >= 0.f && Now >= PendingAt)
	{
		PendingAt = -1.f;
		const UHealthComponent* Health = Owner ? Owner->FindComponentByClass<UHealthComponent>() : nullptr;
		if (!(Health && Health->IsDead()))
		{
			if (PendingKind == EJamWeapon::Sword) { SwingSword(); }
			else if (PendingKind == EJamWeapon::Bow) { ShootArrow(); } // el arma pedida dispara al instante, nunca queda pendiente
		}
	}
	// Arma pedida: apaga el fogonazo; si la apagan por consola (jam.Weapon.Generated 0) vuelve a la espada; y sus piezas
	// no se quedan a la vista con otra arma en la mano (por si alguien vuelve a prender todo lo que cuelga de la figura).
	if (GunFlash && FlashUntil >= 0.f && Now >= FlashUntil)
	{
		FlashUntil = -1.f;
		GunFlash->SetVisibility(false);
	}
	if (Current == EJamWeapon::Generated && !(bHasGun && IsGeneratedEnabled())) { SetWeapon(EJamWeapon::Sword); }
	if (GunRoot && Current != EJamWeapon::Generated && GunRoot->IsVisible()) { GunRoot->SetVisibility(false, true); }
	if (GunRoot && bHasGun)
	{
		// Retroceso del arma, no de la camara: cada cuadro parte de GunInHand, asi nunca acumula deriva.
		// La bala ya salio por la mira; el pulso visual vuelve suave a la mano y no afecta al asiento.
		FTransform Pose = GunInHand;
		const float SinceShot = Now - LastAttackTime;
		if (Current == EJamWeapon::Generated && Owner && !Owner->GetAttachParentActor()
			&& Gun.RecoilSeconds > 0.f && SinceShot >= 0.f && SinceShot < Gun.RecoilSeconds)
		{
			const float T = 1.f - SinceShot / Gun.RecoilSeconds;
			const float Kick = T * T * (3.f - 2.f * T);
			Pose.AddToTranslation(GunInHand.GetRotation().RotateVector(FVector(-Gun.RecoilDistance * Kick, 0.f, 0.f)));
			Pose.SetRotation((GunInHand.GetRotation() * FRotator(Gun.RecoilPitch * Kick, 0.f, 0.f).Quaternion()).GetNormalized());
		}
		GunRoot->SetRelativeTransform(Pose);
	}
	if (!HandMesh || Current != EJamWeapon::Sword || SwingAlpha >= 1.f || HandParent.IsValid())
	{
		return; // con figura rigeada el tajo viene en la animacion Attack
	}
	// Tajo de derecha a izquierda en 0.2 s y regreso.
	SwingAlpha = FMath::Min(1.f, SwingAlpha + DeltaTime / 0.2f);
	const float Yaw = SwingAlpha < 1.f ? FMath::Lerp(60.f, -70.f, SwingAlpha) : 0.f;
	HandMesh->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
	HandMesh->SetRelativeLocation(FVector(45.f, SwingAlpha < 1.f ? FMath::Lerp(45.f, -20.f, SwingAlpha) : 38.f, 0.f));
}

// ---------------------------------------------------------------------------------------------------------------------
// Arma de mano pedida (GDD 5.1). La decision y todos los numeros salen de Weapons.json (Combat/JamGunSpec).
// ---------------------------------------------------------------------------------------------------------------------

bool UJamWeaponComponent::IsGeneratedEnabled()
{
	return JamGun::IsEnabled();
}

bool UJamWeaponComponent::IsWeaponPhrase(const FString& Phrase)
{
	return JamGun::IsWeaponPhrase(Phrase);
}

bool UJamWeaponComponent::HasGeneratedWeapon() const
{
	return bHasGun;
}

FString UJamWeaponComponent::GetGeneratedName() const
{
	return bHasGun ? Gun.Name : FString();
}

float UJamWeaponComponent::GetGeneratedEnergyCost() const
{
	return bHasGun ? Gun.Energy : 0.f;
}

FString UJamWeaponComponent::GetGeneratedHudText() const
{
	if (!bHasGun)
	{
		return FString();
	}
	const FString Cost = FString::Printf(TEXT("%s energy per shot"), *JamGun::Number(Gun.Energy));
	return Gun.MuzzleName.IsEmpty() ? FString::Printf(TEXT("%s - %s"), *Gun.Name, *Cost) : FString::Printf(TEXT("%s - %s, %s"), *Gun.Name, *Gun.MuzzleName, *Cost);
}

bool UJamWeaponComponent::RequestGeneratedWeapon(const FString& Phrase)
{
	// 1) Respaldo por palabras clave, sincrono: dice si la frase es de arma (si no, el pedido va a construccion) y deja lista el arma de respaldo.
	FJamGunSpec Spec;
	if (!IsGeneratedEnabled() || !JamGun::Decide(Phrase, Spec))
	{
		return false;
	}
	const int32 Request = ++GunRequestId; // un pedido nuevo deja sin efecto la respuesta pendiente del anterior
	// 2) Sin clave de OpenAI (o no es el jugador): se equipa ya, como siempre.
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsPlayerControlled() || FJamDecisionClient::GetOpenAIKey().IsEmpty())
	{
		EquipGenerated(Spec, FString());
		return true;
	}
	// 3) Con clave: el modelo solo ELIGE cuerpo, boca y extras entre los ids de Weapons.json; el codigo pone numeros, limites y piezas
	// (JamGun::DecideWithIds). Con error, corte (3 s) o todo negado se equipa la de palabras. El arma anterior sigue en la mano mientras.
	Notify(TEXT("Thinking..."), JamMsg::Info, TEXT("JamGunMade"));
	TWeakObjectPtr<UJamWeaponComponent> Weak(this);
	FJamDecisionClient::RequestGunChoice(Phrase, [Weak, Request, Phrase, Spec](bool bOk, const FString& BodyId, const FString& MuzzleId, const TArray<FString>& ExtraIds)
	{
		UJamWeaponComponent* Self = Weak.Get();
		if (!Self || Self->GunRequestId != Request)
		{
			return; // el jugador ya no existe o pidio otra arma: esta respuesta ya no vale
		}
		// Si el modelo no eligio nada con confianza (ni cuerpo, ni boca, ni extras), el arma es la de palabras y no se le atribuye.
		const bool bChoseSomething = !BodyId.IsEmpty() || !MuzzleId.IsEmpty() || ExtraIds.Num() > 0;
		FJamGunSpec Chosen;
		if (bOk && bChoseSomething && JamGun::DecideWithIds(Phrase, BodyId, MuzzleId, ExtraIds, Chosen)) { Self->EquipGenerated(Chosen, TEXT("OpenAI")); }
		else { Self->EquipGenerated(Spec, FString()); }
	});
	return true;
}

void UJamWeaponComponent::EquipGenerated(const FJamGunSpec& Spec, const FString& ChosenBy)
{
	Gun = Spec;
	bHasGun = true;
	BuildGunMesh();
	SetWeapon(EJamWeapon::Generated);
	Notify(FString::Printf(TEXT("Built: %s"), *GetGeneratedHudText()), JamMsg::Good, TEXT("JamGunMade"));
	// Que se vea la IA: una linea aparte con lo que eligio el modelo ("OpenAI eligio: Rifle, boca de Hielo, Mira").
	const FName AiKey(TEXT("JamGunAI"));
	if (ChosenBy.IsEmpty())
	{
		const APawn* Pawn = Cast<APawn>(GetOwner());
		if (Pawn && Pawn->IsPlayerControlled()) { JamMsg::Remove(AiKey); } // la del arma anterior ya no aplica
		return;
	}
	FString Line = FString::Printf(TEXT("%s picked: %s"), *ChosenBy, *Gun.BodyName);
	if (!Gun.MuzzleName.IsEmpty()) { Line += TEXT(", muzzle: ") + Gun.MuzzleName; }
	for (FString Extra : Gun.ExtraNames)
	{
		if (Extra.IsEmpty()) { continue; }
		Extra[0] = FChar::ToUpper(Extra[0]);
		Line += TEXT(", ") + Extra;
	}
	Notify(Line, JamMsg::Info, AiKey);
}

void UJamWeaponComponent::BuildGunMesh()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->GetRootComponent())
	{
		return;
	}
	// Otro pedido: las piezas del arma anterior se van.
	for (UStaticMeshComponent* Old : GunParts)
	{
		if (Old) { Old->DestroyComponent(); }
	}
	GunParts.Reset();
	if (!GunRoot)
	{
		GunRoot = NewObject<USceneComponent>(Owner);
		GunRoot->SetupAttachment(Owner->GetRootComponent());
		GunRoot->RegisterComponent();
	}
	if (!GunFlash)
	{
		GunFlash = NewObject<UPointLightComponent>(Owner);
		GunFlash->SetMobility(EComponentMobility::Movable);
		GunFlash->SetupAttachment(GunRoot);
		GunFlash->SetCastShadows(false);
		GunFlash->SetVisibility(false);
		GunFlash->RegisterComponent();
	}
	// Empunadura + cuerpo + boca + extras: mallas sin fisica ni colision, hijas de GunRoot (que va en la mano).
	for (const FJamGunPiece& Piece : Gun.Pieces)
	{
		UStaticMesh* PieceMesh = JamGun::LoadMesh(Piece.Mesh);
		if (!PieceMesh)
		{
			continue;
		}
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(Owner);
		Part->SetupAttachment(GunRoot);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetGenerateOverlapEvents(false);
		Part->SetCanEverAffectNavigation(false);
		Part->SetCastShadow(true);
		Part->SetStaticMesh(PieceMesh);
		Part->SetRelativeTransform(JamGun::FitPiece(PieceMesh, Piece));
		Part->RegisterComponent();
		GunParts.Add(Part);
	}
	GunShotMesh = JamGun::LoadMesh(Gun.Projectile.Mesh);
	GunFlash->SetRelativeLocation(Gun.MuzzlePoint + FVector(6.f, 0.f, 0.f));
	GunFlash->SetLightColor(Gun.Color);
	GunFlash->SetIntensity(Gun.FlashIntensity);
	GunFlash->SetAttenuationRadius(Gun.FlashRadius);
	FlashUntil = -1.f;
	PlaceGun();
	UE_LOG(LogJam, Log, TEXT("Arma pedida: %s armada con %d de %d piezas (hueso %s)"), *Gun.Name, GunParts.Num(), Gun.Pieces.Num(), *GunSocket.ToString());
}

FTransform UJamWeaponComponent::MakeGunGrip(USceneComponent* Parent, FName Socket) const
{
	if (!Gun.bAutoGrip)
	{
		// hand.auto false: rot / loc directos en el hueso.
		return FTransform(Gun.HandRot, Gun.HandLoc, FVector(Gun.HandScale));
	}
	// Respaldo: como la espada (el canon donde iba la hoja).
	FQuat Base = SwordInHand.GetRotation();
	const USkeletalMeshComponent* Skel = Cast<USkeletalMeshComponent>(Parent);
	const USkeletalMesh* Asset = Skel ? Skel->GetSkeletalMeshAsset() : nullptr;
	if (Asset)
	{
		const FReferenceSkeleton& Ref = Asset->GetRefSkeleton();
		const int32 Bone = Ref.FindBoneIndex(Socket);
		if (Bone != INDEX_NONE && Ref.GetParentIndex(Bone) != INDEX_NONE && Ref.GetRefBonePose().IsValidIndex(Bone))
		{
			// Canon = direccion antebrazo > mano vista desde el hueso de la mano (sirve para cualquier rig).
			const FTransform& Local = Ref.GetRefBonePose()[Bone];
			const FVector Fwd = Local.GetRotation().UnrotateVector(Local.GetTranslation()).GetSafeNormal();
			if (!Fwd.IsNearlyZero())
			{
				// Arriba del arma = hacia donde sale la hoja de la espada del puno; asi la empunadura queda dentro de la mano.
				FVector Up = SwordInHand.GetRotation().RotateVector(FVector::ForwardVector);
				Up -= Fwd * FVector::DotProduct(Up, Fwd);
				if (Up.SizeSquared() < 0.04)
				{
					FVector Other;
					Fwd.FindBestAxisVectors(Up, Other);
				}
				Base = FRotationMatrix::MakeFromXZ(Fwd, Up.GetSafeNormal()).ToQuat();
			}
		}
	}
	// hand.rot / hand.loc son un ajuste en los ejes del arma (X = canon, Y = derecha, Z = arriba).
	return FTransform(Base * Gun.HandRot.Quaternion(), Base.RotateVector(Gun.HandLoc), FVector(Gun.HandScale));
}

void UJamWeaponComponent::PlaceGun()
{
	AActor* Owner = GetOwner();
	if (!GunRoot || !bHasGun || !Owner)
	{
		return;
	}
	if (USceneComponent* Parent = HandParent.Get())
	{
		// Figura rigeada: al hueso de la mano (hand.socket; si ese hueso no existe, el de la espada).
		FName Socket = Gun.Socket;
		if (Socket.IsNone() || !Parent->DoesSocketExist(Socket)) { Socket = SwordSocket; }
		GunSocket = Socket;
		GunInHand = MakeGunGrip(Parent, Socket);
		GunRoot->AttachToComponent(Parent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
		GunRoot->SetRelativeTransform(GunInHand);
	}
	else if (USceneComponent* Root = Owner->GetRootComponent())
	{
		// Sin esqueleto: a un lado del cuerpo, apuntando al frente (como la espada suelta).
		GunSocket = NAME_None;
		GunInHand = FTransform(Gun.HandRot, Gun.LooseLoc, FVector(Gun.HandScale));
		GunRoot->AttachToComponent(Root, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		GunRoot->SetRelativeTransform(GunInHand);
	}
}

void UJamWeaponComponent::SetGeneratedGrip(const FRotator& Rot, const FVector& Loc, float Scale)
{
	if (!bHasGun)
	{
		return;
	}
	Gun.HandRot = Rot;
	Gun.HandLoc = Loc;
	Gun.HandScale = FMath::Clamp(Scale, 0.05f, 20.f);
	PlaceGun();
}

bool UJamWeaponComponent::GetGeneratedPose(FName& OutBone, FQuat& OutInBone, float& OutAim) const
{
	if (Current != EJamWeapon::Generated || !bHasGun || !HandParent.IsValid() || GunSocket.IsNone())
	{
		return false;
	}
	OutBone = GunSocket;
	OutInBone = GunInHand.GetRotation();
	const UWorld* World = GetWorld();
	OutAim = (World && World->GetTimeSeconds() - LastAttackTime < Gun.AimSeconds) ? 1.f : 0.f;
	return true;
}

bool UJamWeaponComponent::FireGenerated()
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!bHasGun || !World || !Owner)
	{
		return false;
	}
	const float Now = World->GetTimeSeconds();

	// Municion = energia del jugador (GDD 5.1): un cobro por clic (no por perdigon). jam.Build.Energy 0 = gratis.
	// Se revisa antes de tocar LastAttackTime: sin energia no dispara ni entra en espera.
	const APawn* Pawn = Cast<APawn>(Owner);
	if (Pawn && Pawn->IsPlayerControlled() && AJamBuildPart::EnergyMode() != 0 && Gun.Energy > 0.f)
	{
		const float Have = AJamBuildPart::GetPlayerEnergy(World);
		if (Have < Gun.Energy)
		{
			// Attack corre cada frame con el clic sostenido: el aviso sale como mucho cada 2 s.
			if (Now - LastNoEnergyNote > 2.f)
			{
				LastNoEnergyNote = Now;
				Notify(FString::Printf(TEXT("No energy: %s uses %s per shot. Find energy or wait to recharge"), *Gun.Name, *JamGun::Number(Gun.Energy)), JamMsg::Warn, TEXT("JamGunEnergy"));
			}
			return false;
		}
		AJamBuildPart::SetPlayerEnergy(World, Have - Gun.Energy);
	}
	LastAttackTime = Now;

	// Voltea a la mira y dispara al centro de la pantalla.
	FVector From, Dir, Target;
	GetAim(From, Dir, Gun.Range, &Target);
	FaceYaw = Dir.Rotation().Yaw;
	FaceUntil = Now + 0.3f;
	if (!Owner->GetAttachParentActor()) { Owner->SetActorRotation(FRotator(0.f, FaceYaw, 0.f)); }

	// Sale de la boca del arma si esta a la vista y el blanco no esta pegado; si no, del pecho como la flecha.
	if (GunRoot && GunRoot->IsVisible())
	{
		const FVector Muzzle = GunRoot->GetComponentTransform().TransformPosition(Gun.MuzzlePoint);
		const FVector ToTarget = Target - Muzzle;
		if (FVector::DistSquared(Muzzle, Owner->GetActorLocation()) < FMath::Square(250.0) && ToTarget.SizeSquared() > FMath::Square(150.0)
			&& FVector::DotProduct(ToTarget.GetSafeNormal(), Dir) > 0.5)
		{
			From = Muzzle;
			Dir = ToTarget.GetSafeNormal();
		}
	}

	FActorSpawnParameters P;
	P.Owner = Owner;
	P.Instigator = Cast<APawn>(Owner);
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	UStaticMesh* ShotMesh = GunShotMesh;
	const FTransform ShotFit = JamGun::FitPiece(ShotMesh, Gun.Projectile);
	const float Life = FMath::Clamp(Gun.Range / FMath::Max(1.f, Gun.Speed), 0.15f, 8.f);
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Dir).GetSafeNormal();
	const int32 Half = FMath::Max(1, Gun.Pellets / 2);
	TArray<AJamArrow*> Volley;
	for (int32 I = 0; I < Gun.Pellets; ++I)
	{
		// Un proyectil: recto (con su dispersion). Varios: abanico; el primero va al centro y los demas se abren a los lados.
		FVector ShotDir = Dir;
		if (Gun.SpreadDeg > 0.f && Gun.Pellets == 1)
		{
			ShotDir = FMath::VRandCone(Dir, FMath::DegreesToRadians(Gun.SpreadDeg));
		}
		else if (Gun.SpreadDeg > 0.f && I > 0)
		{
			const float Yaw = float((I + 1) / 2) * (Gun.SpreadDeg / float(Half)) * ((I % 2) != 0 ? 1.f : -1.f);
			const float Jitter = Gun.SpreadDeg * Gun.FanJitter;
			ShotDir = Dir.RotateAngleAxis(Yaw, FVector::UpVector);
			if (!Right.IsNearlyZero()) { ShotDir = ShotDir.RotateAngleAxis(FMath::FRandRange(-Jitter, Jitter), Right); }
		}
		AJamArrow* Shot = World->SpawnActor<AJamArrow>(AJamArrow::StaticClass(), From, ShotDir.Rotation(), P);
		if (!Shot)
		{
			continue;
		}
		// El efecto de la boca va por la misma via que la flecha fusionada (ApplyFusionHit, con credito al jugador),
		// pero sin gastar material del inventario.
		Shot->Fused = (Gun.EffectPellets <= 0 || I < Gun.EffectPellets) ? Gun.Effect : FName();
		Shot->Push = Gun.Push;
		Shot->SetupShot(ShotMesh, ShotFit, Gun.Color, Gun.Projectile.bGlow, Gun.Gravity, Life, Gun.bArea);
		for (AJamArrow* Other : Volley)
		{
			Shot->IgnoreShot(Other);
			Other->IgnoreShot(Shot);
		}
		Shot->Launch(this, ShotDir * Gun.Speed, Gun.Damage);
		Volley.Add(Shot);
	}

	// Fogonazo: la luz de la boca un instante.
	if (GunFlash && Gun.FlashSeconds > 0.f && GunRoot && GunRoot->IsVisible())
	{
		GunFlash->SetVisibility(true);
		FlashUntil = Now + Gun.FlashSeconds;
	}
	// Sonido del disparo segun la boca (Weapons.json, "sound"). Un *_Loop como disparo suelto no pararia nunca: se omite.
	if (!Gun.Sound.IsEmpty() && !Gun.Sound.EndsWith(TEXT("_Loop")))
	{
		JamAudio::PlayAt(this, *Gun.Sound, Owner->GetActorLocation(), 0.7f);
	}
	OnWeaponAttack.Broadcast(this, Current);
	return Volley.Num() > 0;
}
