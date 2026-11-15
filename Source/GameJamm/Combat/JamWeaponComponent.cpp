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
		FString Name = Id.ToString();
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
	RefreshHandMesh();
}

void UJamWeaponComponent::RefreshHandMesh()
{
	if (!HandMesh)
	{
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
	return Current == EJamWeapon::Sword ? FusedSword : FusedBow;
}

int32 UJamWeaponComponent::GetFusedUses() const
{
	if (Current == EJamWeapon::Sword) { return FusedSword.IsNone() ? 0 : SwordUses; }
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
	return SwordInterval * (FusedSword.IsNone() ? 1.f : MatNumber(FusedSword, TEXT("sword_interval"), 1.f));
}

void UJamWeaponComponent::Notify(const FString& Text) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	UE_LOG(LogJam, Log, TEXT("Fusion: %s"), *Text);
	if (GEngine && Pawn && Pawn->IsPlayerControlled()) { GEngine->AddOnScreenDebugMessage(9100, 3.f, FColor(255, 200, 90), Text); }
}

bool UJamWeaponComponent::Fuse(FName MaterialId)
{
	UInventoryComponent* Inv = GetOwner() ? GetOwner()->FindComponentByClass<UInventoryComponent>() : nullptr;
	if (!Inv || !MatConfig(MaterialId).IsValid() || Inv->GetCount(InvId(MaterialId)) <= 0)
	{
		Notify(FString::Printf(TEXT("No tienes %s para fusionar"), *MatName(MaterialId)));
		return false;
	}
	if (Current == EJamWeapon::Sword)
	{
		Inv->RemoveItem(InvId(MaterialId), 1); // el anterior se pierde
		FusedSword = MaterialId;
		double Uses = 20.0;
		FusionConfig()->TryGetNumberField(TEXT("sword_uses"), Uses);
		SwordUses = FMath::RoundToInt(MatNumber(MaterialId, TEXT("sword_uses"), float(Uses)));
		Notify(FString::Printf(TEXT("Espada + %s (%d golpes)"), *MatName(MaterialId), SwordUses));
	}
	else
	{
		FusedBow = MaterialId;
		Notify(FString::Printf(TEXT("Flechas de %s (%d)"), *MatName(MaterialId), Inv->GetCount(InvId(MaterialId))));
	}
	RefreshFuseMesh();
	return true;
}

bool UJamWeaponComponent::FuseNext()
{
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
	Notify(TEXT("No tienes materiales para fusionar (salen de cofres y chatarra)"));
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
		UGameplayStatics::ApplyDamage(A, Damage, By, Owner, nullptr);
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
				if (A != Target) { UGameplayStatics::ApplyDamage(A, MatNumber(Id, TEXT("arrow_area_damage"), 10.f), By, GetOwner(), nullptr); }
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
	LastAttackTime = GetWorld()->GetTimeSeconds();
	if (Current == EJamWeapon::Sword) { SwingSword(); }
	else { ShootArrow(); }
	OnWeaponAttack.Broadcast(this, Current);
	return true;
}

void UJamWeaponComponent::NextWeapon()
{
	SetWeapon(Current == EJamWeapon::Sword ? EJamWeapon::Bow : EJamWeapon::Sword);
}

void UJamWeaponComponent::SetWeapon(EJamWeapon Weapon)
{
	Current = Weapon;
	SwingAlpha = 1.f;
	RefreshHandMesh();
}

void UJamWeaponComponent::GetAim(FVector& OutFrom, FVector& OutDir) const
{
	const AActor* Owner = GetOwner();
	const APawn* Pawn = Cast<APawn>(Owner);
	const FRotator Control = Pawn && Pawn->GetController() ? Pawn->GetController()->GetControlRotation() : Owner->GetActorRotation();
	OutFrom = Owner->GetActorLocation() + FRotator(0.f, Control.Yaw, 0.f).Vector() * 50.f + FVector(0, 0, 40.f);
	OutDir = Control.Vector();

	// Con camara: a donde mira el centro de la pantalla (hasta BowRange), para que la flecha vaya a la mira.
	if (const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr)
	{
		FVector ViewLoc;
		FRotator ViewRot;
		PC->GetPlayerViewPoint(ViewLoc, ViewRot);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(JamBowAim), false, Owner);
		FHitResult Hit;
		const FVector End = ViewLoc + ViewRot.Vector() * BowRange;
		const FVector Target = GetWorld()->LineTraceSingleByChannel(Hit, ViewLoc + ViewRot.Vector() * 200.f, End, ECC_Visibility, Params) ? Hit.ImpactPoint : End;
		OutDir = (Target - OutFrom).GetSafeNormal();
	}
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
	UE_LOG(LogJam, Log, TEXT("Arma %s pega a %s: %.0f"), Current == EJamWeapon::Sword ? TEXT("espada") : TEXT("arco"), *Target->GetName(), Damage);
	OnWeaponHit.Broadcast(this, Target, Damage);
	return true;
}

void UJamWeaponComponent::SwingSword()
{
	SwingAlpha = 0.f;
	AActor* Owner = GetOwner();
	FVector From, Aim;
	GetAim(From, Aim);
	const FVector Fwd = FVector(Aim.X, Aim.Y, 0.f).GetSafeNormal();
	const float Radius = SwordRange * 0.5f;
	const FVector Center = Owner->GetActorLocation() + Fwd * Radius;

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
		if (!A || Done.Contains(A)) { continue; }
		Done.Add(A);
		if (DealDamage(A, SwordDamage + BonusDamage + Bonus, Fwd * SwordKnockback * PushMul))
		{
			if (!Fused.IsNone() && !(bHit && Fused == TEXT("Bomb"))) { ApplyFusionHit(Fused, A, A->GetActorLocation(), false); }
			bHit = true;
		}
	}
	// El material se gasta con el uso: un golpe por tajo que pega; al acabarse se cae.
	if (bHit && !Fused.IsNone() && --SwordUses <= 0)
	{
		Notify(FString::Printf(TEXT("Se acabo el material de la espada (%s)"), *MatName(Fused)));
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
			Notify(FString::Printf(TEXT("Sin %s: flechas normales"), *MatName(Fused)));
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
