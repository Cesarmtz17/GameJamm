#include "Core/JamCharacterLook.h"
#include "Core/HealthComponent.h"
#include "Combat/JamWeaponComponent.h"
#include "Core/JamFigureAnim.h"
#include "GameJamm.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "HAL/IConsoleManager.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	TSharedPtr<FJsonObject> CharactersConfig()
	{
		static TSharedPtr<FJsonObject> Cached;
		static bool bTried = false;
		if (!bTried)
		{
			bTried = true;
			FString Text;
			const FString Path = FPaths::ProjectContentDir() / TEXT("Game/Art/Tripo/Characters/Characters.json");
			if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Cached))
			{
				Cached.Reset();
				UE_LOG(LogJam, Log, TEXT("Look: sin %s, personajes con cilindro"), *Path);
			}
		}
		return Cached;
	}

	FVector Vec3(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (Obj.IsValid() && Obj->TryGetArrayField(Field, A) && A->Num() == 3)
		{
			return FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber());
		}
		return FVector::ZeroVector;
	}

	bool IsHidden(const UStaticMeshComponent* C)
	{
		const FName N = C->GetFName();
		return N == TEXT("Body") || N == TEXT("Nose") || N == TEXT("Head");
	}
}

UJamCharacterLook::UJamCharacterLook()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UJamCharacterLook::BeginPlay()
{
	Super::BeginPlay();
	bApplied = Apply();
	if (!bApplied)
	{
		SetComponentTickEnabled(false);
		return;
	}
	if (UJamWeaponComponent* W = GetOwner()->FindComponentByClass<UJamWeaponComponent>()) { W->OnWeaponAttack.AddDynamic(this, &UJamCharacterLook::HandleWeaponAttack); }
	if (UHealthComponent* H = GetOwner()->FindComponentByClass<UHealthComponent>()) { H->OnHealthChanged.AddDynamic(this, &UJamCharacterLook::HandleHealthChanged); }
}

bool UJamCharacterLook::Apply()
{
	ACharacter* Char = Cast<ACharacter>(GetOwner());
	const TSharedPtr<FJsonObject> Cfg = CharactersConfig();
	const TSharedPtr<FJsonObject>* Entry = nullptr;
	if (!Char || !Char->GetMesh() || !Cfg.IsValid() || !Cfg->TryGetObjectField(Key.ToString(), Entry))
	{
		return false;
	}
	FString MeshPath;
	(*Entry)->TryGetStringField(TEXT("Mesh"), MeshPath);
	USkeletalMesh* Mesh = MeshPath.IsEmpty() ? nullptr : LoadObject<USkeletalMesh>(nullptr, *MeshPath);
	if (!Mesh)
	{
		UE_LOG(LogJam, Warning, TEXT("Look %s: no carga la malla '%s', queda el cilindro"), *Key.ToString(), *MeshPath);
		return false;
	}

	USkeletalMeshComponent* SK = Char->GetMesh();
	SK->SetSkeletalMesh(Mesh);
	FVector Offset = FVector::ZeroVector;
	const TArray<TSharedPtr<FJsonValue>>* Off = nullptr;
	if ((*Entry)->TryGetArrayField(TEXT("Offset"), Off) && Off->Num() == 3)
	{
		Offset = FVector((*Off)[0]->AsNumber(), (*Off)[1]->AsNumber(), (*Off)[2]->AsNumber());
	}
	BaseOffset = Offset;
	SitOffset = Vec3(*Entry, TEXT("SitOffset"));
	double Yaw = 0.0, Scale = 1.0, Run = 220.0;
	(*Entry)->TryGetNumberField(TEXT("Yaw"), Yaw);
	(*Entry)->TryGetNumberField(TEXT("Scale"), Scale);
	(*Entry)->TryGetNumberField(TEXT("RunAbove"), Run);
	RunAbove = float(Run);
	SK->SetRelativeLocationAndRotation(Offset, FRotator(0.f, float(Yaw), 0.f));
	SK->SetRelativeScale3D(FVector(float(Scale)));
	SK->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Animacion suelta + espadazo/arco por codigo (UJamFigureAnim); sin Anim Blueprint.
	SK->SetAnimInstanceClass(UJamFigureAnim::StaticClass());
	SK->SetVisibility(true);
	SK->SetHiddenInGame(false);
	if (!Char->IsPlayerControlled()) { SK->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered; }

	const TSharedPtr<FJsonObject>* Mats = nullptr;
	if ((*Entry)->TryGetObjectField(TEXT("Materials"), Mats))
	{
		const TPair<const TCHAR*, EJamTeam> Teams[] = { { TEXT("Red"), EJamTeam::Red }, { TEXT("Blue"), EJamTeam::Blue }, { TEXT("Neutral"), EJamTeam::Neutral } };
		for (const auto& T : Teams)
		{
			FString P;
			if ((*Mats)->TryGetStringField(T.Key, P))
			{
				if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, *P)) { TeamMaterials.Add(T.Value, M); }
			}
		}
	}

	const TSharedPtr<FJsonObject>* AnimObj = nullptr;
	if ((*Entry)->TryGetObjectField(TEXT("Anims"), AnimObj))
	{
		for (const auto& It : (*AnimObj)->Values)
		{
			const TSharedPtr<FJsonObject> A = It.Value->AsObject();
			FString P;
			if (!A.IsValid() || !A->TryGetStringField(TEXT("Asset"), P)) { continue; }
			UAnimSequenceBase* Seq = LoadObject<UAnimSequenceBase>(nullptr, *P);
			if (!Seq) { UE_LOG(LogJam, Warning, TEXT("Look %s: no carga la animacion %s"), *Key.ToString(), *P); continue; }
			FAnim& Out = Anims.Add(FName(*It.Key));
			Out.Asset = Seq;
			double V = 0.0;
			if (A->TryGetNumberField(TEXT("Speed"), V)) { Out.Speed = float(V); }
			if (A->TryGetNumberField(TEXT("Rate"), V)) { Out.Rate = float(V); }
			Keep.Add(Seq);
		}
	}

	// El cuerpo de cilindros se oculta; arma y demas piezas siguen visibles.
	TArray<UStaticMeshComponent*> Statics;
	Char->GetComponents<UStaticMeshComponent>(Statics);
	for (UStaticMeshComponent* C : Statics) { if (IsHidden(C)) { C->SetVisibility(false); } }

	// Arma en la mano (campo Weapon): Socket/Offset/Rot de la espada; BowSocket/BowOffset/BowRot del arco (si faltan, los de la espada).
	const TSharedPtr<FJsonObject>* WeaponCfg = nullptr;
	UJamWeaponComponent* Weapon = Char->FindComponentByClass<UJamWeaponComponent>();
	if (Weapon && (*Entry)->TryGetObjectField(TEXT("Weapon"), WeaponCfg))
	{
		FString Socket = TEXT("hand_r"), BowSocket;
		(*WeaponCfg)->TryGetStringField(TEXT("Socket"), Socket);
		if (!(*WeaponCfg)->TryGetStringField(TEXT("BowSocket"), BowSocket)) { BowSocket = Socket; }
		// Rot = [pitch, yaw, roll] en el hueso; Grip = punto del arma (espacio local de la malla) que queda en la mano;
		// Offset = corrimiento extra en el espacio del hueso. Bow* igual para el arco (si faltan, los de la espada).
		auto Hold = [&WeaponCfg](const TCHAR* RotF, const TCHAR* GripF, const TCHAR* OffF, const FTransform* Fallback) -> FTransform
		{
			if (Fallback && !(*WeaponCfg)->HasField(RotF) && !(*WeaponCfg)->HasField(GripF) && !(*WeaponCfg)->HasField(OffF)) { return *Fallback; }
			const FVector R = Vec3(*WeaponCfg, RotF);
			const FRotator Rot(R.X, R.Y, R.Z);
			return FTransform(Rot, Vec3(*WeaponCfg, OffF) - Rot.RotateVector(Vec3(*WeaponCfg, GripF)));
		};
		const FTransform SwordT = Hold(TEXT("Rot"), TEXT("Grip"), TEXT("Offset"), nullptr);
		const FTransform BowT = Hold(TEXT("BowRot"), TEXT("BowGrip"), TEXT("BowOffset"), &SwordT);
		if (SK->DoesSocketExist(FName(*Socket)) && SK->DoesSocketExist(FName(*BowSocket)))
		{
			Weapon->AttachToHand(SK, FName(*Socket), SwordT, FName(*BowSocket), BowT);
			if (UJamFigureAnim* Fig = Cast<UJamFigureAnim>(SK->GetAnimInstance())) { Fig->SetRig(FName(*Socket), SwordT.GetRotation(), FName(*BowSocket), BowT.GetRotation()); }
			UE_LOG(LogJam, Log, TEXT("Look %s: arma en %s (arco en %s)"), *Key.ToString(), *Socket, *BowSocket);
		}
		else
		{
			UE_LOG(LogJam, Warning, TEXT("Look %s: no existe el hueso %s/%s, el arma queda como antes"), *Key.ToString(), *Socket, *BowSocket);
		}
	}

	UE_LOG(LogJam, Log, TEXT("Look %s: %s con %d animaciones"), *Key.ToString(), *GetOwner()->GetName(), Anims.Num());
	Play(TEXT("Idle"), true, 1.f);
	return true;
}

const UJamCharacterLook::FAnim* UJamCharacterLook::Find(FName State) const
{
	const FAnim* A = Anims.Find(State);
	return A ? A : Anims.Find(TEXT("Idle"));
}

void UJamCharacterLook::Play(FName State, bool bLoop, float Rate)
{
	ACharacter* Char = Cast<ACharacter>(GetOwner());
	const FAnim* A = Find(State);
	if (!Char || !A || !A->Asset)
	{
		return;
	}
	if (State != Current)
	{
		// Directo a la instancia: PlayAnimation cambiaria el modo y quitaria UJamFigureAnim.
		if (UAnimSingleNodeInstance* Single = Char->GetMesh()->GetSingleNodeInstance())
		{
			Single->SetAnimationAsset(A->Asset, bLoop);
			Single->SetPlaying(true);
		}
		else
		{
			Char->GetMesh()->PlayAnimation(A->Asset, bLoop);
		}
		Current = State;
	}
	Char->GetMesh()->SetPlayRate(Rate);
}

void UJamCharacterLook::PlayAction(FName Action)
{
	if (!bApplied)
	{
		return;
	}
	const FAnim* A = Anims.Find(Action);
	if (!A || !A->Asset)
	{
		return;
	}
	Current = NAME_None; // la reinicia aunque sea la misma
	Play(Action, false, A->Rate);
	ActionUntil = GetWorld()->GetTimeSeconds() + A->Asset->GetPlayLength() / FMath::Max(0.1f, A->Rate);
}

void UJamCharacterLook::HandleWeaponAttack(UJamWeaponComponent* InWeapon, EJamWeapon Kind)
{
	// Con arma en mano: espadazo / tensar y soltar por codigo, sin cortar el caminar. Si no, los clips Attack/Shoot.
	ACharacter* Char = Cast<ACharacter>(GetOwner());
	UJamFigureAnim* Fig = Char ? Cast<UJamFigureAnim>(Char->GetMesh()->GetAnimInstance()) : nullptr;
	static const IConsoleVariable* MovesVar = IConsoleManager::Get().FindConsoleVariable(TEXT("jam.Figure.Moves"));
	if (Fig && InWeapon && InWeapon->IsInHand() && (!MovesVar || MovesVar->GetInt() != 0))
	{
		const bool bBow = Kind == EJamWeapon::Bow;
		const float Dur = bBow ? 0.6f : 0.42f;
		Fig->StartMove(bBow ? EJamFigureMove::Shot : EJamFigureMove::Slash, Dur);
		if (!Current.IsNone() && (Current == TEXT("Attack") || Current == TEXT("Shoot"))) { Current = NAME_None; ActionUntil = -1.f; }
		return;
	}
	PlayAction(Kind == EJamWeapon::Bow ? TEXT("Shoot") : TEXT("Attack"));
}

void UJamCharacterLook::HandleHealthChanged(UHealthComponent* HealthComponent, float NewHealth, float Delta)
{
	if (Delta < 0.f && NewHealth > 0.f && GetWorld()->GetTimeSeconds() >= ActionUntil) { PlayAction(TEXT("Hit")); }
}

void UJamCharacterLook::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ACharacter* Char = Cast<ACharacter>(GetOwner());
	const UHealthComponent* H = Char ? Char->FindComponentByClass<UHealthComponent>() : nullptr;
	if (!Char)
	{
		return;
	}
	USkeletalMeshComponent* SK = Char->GetMesh();

	// Equipo: el jugador lo recibe despues de BeginPlay (SetPlayerDefaults), por eso se compara aqui.
	if (H && (!bHasTeam || H->Team != LastTeam))
	{
		bHasTeam = true;
		LastTeam = H->Team;
		if (const TObjectPtr<UMaterialInterface>* M = TeamMaterials.Find(LastTeam)) { SK->SetMaterial(0, *M); }
	}

	// Primera persona (V): el toggle solo cambia Body/Nose; se copia a la figura.
	TArray<UStaticMeshComponent*> Statics;
	Char->GetComponents<UStaticMeshComponent>(Statics);
	for (UStaticMeshComponent* C : Statics)
	{
		if (C->GetFName() == TEXT("Body")) { SK->SetOwnerNoSee(C->bOwnerNoSee); }
		if (IsHidden(C) && C->IsVisible()) { C->SetVisibility(false); } // por si alguien lo vuelve a prender
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (UJamFigureAnim* Fig = Cast<UJamFigureAnim>(SK->GetAnimInstance()))
	{
		Fig->SetMovesEnabled(!(H && H->IsDead()) && Char->GetAttachParentActor() == nullptr);
	}
	if (H && H->IsDead())
	{
		if (Current != TEXT("Death")) { const FAnim* A = Anims.Find(TEXT("Death")); Play(TEXT("Death"), false, A ? A->Rate : 1.f); }
		return;
	}
	if (Current == TEXT("Death")) { Current = NAME_None; } // reaparecio
	// Sentado (montado): la animacion Sit es de silla; SitOffset baja la cadera al asiento. Al bajarse vuelve a Offset.
	const bool bSit = Char->GetAttachParentActor() != nullptr;
	if (bSit != bSitting)
	{
		bSitting = bSit;
		SK->SetRelativeLocation(bSit ? BaseOffset + SitOffset : BaseOffset);
	}
	if (bSit)
	{
		Play(TEXT("Sit"), true, 1.f);
		return;
	}
	if (Now < ActionUntil)
	{
		return;
	}
	const float Speed = Char->GetVelocity().Size2D();
	if (Speed > 20.f)
	{
		const FName State = Speed > RunAbove * Char->GetActorScale3D().X ? FName(TEXT("Run")) : FName(TEXT("Walk"));
		const FAnim* A = Find(State);
		const float Base = A && A->Speed > 0.f ? A->Speed * Char->GetActorScale3D().X : Speed;
		Play(State, true, FMath::Clamp(Speed / Base, 0.5f, 2.f));
	}
	else
	{
		Play(TEXT("Idle"), true, 1.f);
	}
}
