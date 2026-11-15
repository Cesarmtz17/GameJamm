#include "Core/JamFigureAnim.h"
#include "Combat/JamWeaponComponent.h"
#include "GameJamm.h"
#include "Animation/AnimSingleNodeInstanceProxy.h"
#include "Animation/AnimNodeBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarJamFigureMoves(TEXT("jam.Figure.Moves"), 1,
		TEXT("1 = espadazo/arco/espada cargada hechos por codigo sobre la figura; 0 = solo los clips de Characters.json."));
	TAutoConsoleVariable<float> CVarJamFigureFreeze(TEXT("jam.Figure.Freeze"), -1.f,
		TEXT("Pruebas: >= 0 congela el movimiento de arma en ese avance (0..1) para verlo en captura."));

	// ---- Poses (direcciones en el actor: X frente, Y derecha, Z arriba) ----
	// U = hombro > codo, F = codo > mano, B = hoja de la espada / largo del arco. Cero = no tocar ese hueso.
	struct FKey
	{
		float T;
		FVector U, F, B;
	};

	// Espada cargada al caminar/correr: punta al frente y hacia afuera (no cruza el torso ni las piernas).
	const FVector CarryBlade(0.55f, 0.65f, 0.1f);

	// Espadazo en diagonal: arriba a la derecha > cruza al frente > termina abajo a la izquierda.
	const FKey SlashKeys[] = {
		{ 0.00f, FVector(-0.10f, 0.80f, 0.55f), FVector(-0.30f, 0.30f, 0.90f), FVector(-0.55f, 0.45f, 0.70f) }, // atras arriba
		{ 0.30f, FVector(-0.10f, 0.80f, 0.55f), FVector(-0.30f, 0.30f, 0.90f), FVector(-0.55f, 0.45f, 0.70f) }, // carga
		{ 0.48f, FVector(0.90f, 0.35f, 0.25f), FVector(1.00f, 0.05f, 0.05f), FVector(0.80f, -0.35f, 0.10f) },  // golpe al frente
		{ 0.62f, FVector(0.60f, -0.45f, -0.25f), FVector(0.40f, -0.85f, -0.30f), FVector(0.15f, -0.80f, -0.55f) }, // remate
		{ 1.00f, FVector(0.60f, -0.45f, -0.25f), FVector(0.40f, -0.85f, -0.30f), FVector(0.15f, -0.80f, -0.55f) },
	};
	const float SlashIn = 0.12f, SlashOut = 0.72f; // entra rapido, sale despues del remate

	// Arco: brazo del arco extendido al frente con el arco vertical; la otra mano tensa junto a la cara y suelta hacia atras.
	const FKey BowArmKeys[] = {
		{ 0.00f, FVector(1.00f, -0.20f, 0.05f), FVector(1.00f, -0.05f, 0.05f), FVector(0.05f, -0.15f, 1.00f) },
		{ 1.00f, FVector(1.00f, -0.20f, 0.05f), FVector(1.00f, -0.05f, 0.05f), FVector(0.05f, -0.15f, 1.00f) },
	};
	const FKey DrawArmKeys[] = {
		{ 0.00f, FVector(0.60f, 0.70f, 0.05f), FVector(0.90f, -0.30f, 0.10f), FVector::ZeroVector },   // mano en la cuerda, al frente
		{ 0.22f, FVector(-0.55f, 0.80f, 0.10f), FVector(0.75f, -0.65f, 0.10f), FVector::ZeroVector },  // tensado: codo atras, mano en la barbilla
		{ 0.40f, FVector(-0.55f, 0.80f, 0.10f), FVector(0.75f, -0.65f, 0.10f), FVector::ZeroVector },  // apunta
		{ 0.50f, FVector(-0.70f, 0.70f, 0.15f), FVector(-0.35f, 0.90f, 0.25f), FVector::ZeroVector },  // suelta: la mano sale hacia atras
		{ 1.00f, FVector(-0.70f, 0.70f, 0.15f), FVector(-0.35f, 0.90f, 0.25f), FVector::ZeroVector },
	};
	const float ShotIn = 0.15f, ShotOut = 0.75f;

	FVector Nlerp(const FVector& A, const FVector& B, float Alpha)
	{
		if (A.IsNearlyZero() || B.IsNearlyZero()) { return Alpha < 0.5f ? A : B; }
		return FMath::Lerp(A.GetSafeNormal(), B.GetSafeNormal(), Alpha).GetSafeNormal();
	}

	FKey Sample(const FKey* Keys, int32 Num, float T)
	{
		if (T <= Keys[0].T) { return Keys[0]; }
		for (int32 I = 1; I < Num; ++I)
		{
			if (T <= Keys[I].T)
			{
				const FKey& A = Keys[I - 1];
				const FKey& B = Keys[I];
				const float Alpha = FMath::SmoothStep(0.f, 1.f, (T - A.T) / FMath::Max(KINDA_SMALL_NUMBER, B.T - A.T));
				return { T, Nlerp(A.U, B.U, Alpha), Nlerp(A.F, B.F, Alpha), Nlerp(A.B, B.B, Alpha) };
			}
		}
		return Keys[Num - 1];
	}

	float Envelope(float T, float In, float Out)
	{
		return FMath::SmoothStep(0.f, In, T) * (1.f - FMath::SmoothStep(Out, 1.f, T));
	}

	// ---- Huesos ----
	FCompactPoseBoneIndex FindBone(const FCompactPose& Pose, FName Name)
	{
		const FBoneContainer& BC = Pose.GetBoneContainer();
		const int32 PoseIdx = Name.IsNone() ? INDEX_NONE : BC.GetPoseBoneIndexForBoneName(Name);
		return PoseIdx == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : BC.MakeCompactPoseIndex(FMeshPoseBoneIndex(PoseIdx));
	}

	FTransform ComponentSpace(const FCompactPose& Pose, FCompactPoseBoneIndex I)
	{
		FTransform T = FTransform::Identity;
		while (I.IsValid())
		{
			T = T * Pose[I];
			I = Pose.GetParentBoneIndex(I);
		}
		return T;
	}

	// Gira el hueso B (en espacio de componente, pivote en el propio hueso) para que Cur apunte a Want. Los hijos lo siguen.
	void Turn(FCompactPose& Pose, FCompactPoseBoneIndex B, const FVector& Cur, const FVector& Want, float W)
	{
		if (!B.IsValid() || W <= 0.f || Cur.IsNearlyZero() || Want.IsNearlyZero()) { return; }
		const FCompactPoseBoneIndex P = Pose.GetParentBoneIndex(B);
		const FQuat Qp = P.IsValid() ? ComponentSpace(Pose, P).GetRotation() : FQuat::Identity;
		const FQuat D = FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(Cur.GetSafeNormal(), Want.GetSafeNormal()), FMath::Clamp(W, 0.f, 1.f));
		FTransform& L = Pose[B];
		L.SetRotation((Qp.Inverse() * D * Qp * L.GetRotation()).GetNormalized());
	}

	// Brazo hombro > codo > mano apuntando a U, F (en el componente) y la pieza en mano (eje Axis del hueso de la mano) a Blade.
	void AimArm(FCompactPose& Pose, FName HandName, const FVector& U, const FVector& F, float WArm, const FVector& Axis, const FVector& Blade, float WBlade)
	{
		const FCompactPoseBoneIndex Hand = FindBone(Pose, HandName);
		if (!Hand.IsValid()) { return; }
		const FCompactPoseBoneIndex Fore = Pose.GetParentBoneIndex(Hand);
		const FCompactPoseBoneIndex Upper = Fore.IsValid() ? Pose.GetParentBoneIndex(Fore) : FCompactPoseBoneIndex(INDEX_NONE);
		if (WArm > 0.f && Upper.IsValid() && !U.IsNearlyZero())
		{
			Turn(Pose, Upper, ComponentSpace(Pose, Fore).GetLocation() - ComponentSpace(Pose, Upper).GetLocation(), U, WArm);
		}
		if (WArm > 0.f && Fore.IsValid() && !F.IsNearlyZero())
		{
			Turn(Pose, Fore, ComponentSpace(Pose, Hand).GetLocation() - ComponentSpace(Pose, Fore).GetLocation(), F, WArm);
		}
		if (WBlade > 0.f && !Blade.IsNearlyZero())
		{
			Turn(Pose, Hand, ComponentSpace(Pose, Hand).GetRotation().RotateVector(Axis), Blade, WBlade);
		}
	}

	void ApplyMoves(FCompactPose& Pose, const FJamFigureState& S)
	{
		if (!S.bRig || CVarJamFigureMoves.GetValueOnAnyThread() == 0) { return; }
		const float Freeze = CVarJamFigureFreeze.GetValueOnAnyThread();
		const float T = Freeze >= 0.f ? FMath::Clamp(Freeze, 0.f, 1.f) : S.T;
		const FQuat ToMesh = S.MeshQuat.Inverse();
		const FQuat Aim = FRotator(S.Pitch, 0.f, 0.f).Quaternion();
		auto Mesh = [&ToMesh](const FVector& V) { return V.IsNearlyZero() ? V : ToMesh.RotateVector(V.GetSafeNormal()); };
		auto MeshAim = [&ToMesh, &Aim](const FVector& V) { return V.IsNearlyZero() ? V : ToMesh.RotateVector(Aim.RotateVector(V.GetSafeNormal())); };

		if (!S.bBow)
		{
			const FVector SwordAxis = S.SwordInBone.RotateVector(FVector::ForwardVector);
			const bool bSlash = S.Move == EJamFigureMove::Slash || Freeze >= 0.f;
			const float W = bSlash ? Envelope(T, SlashIn, SlashOut) : 0.f;
			const FKey K = Sample(SlashKeys, UE_ARRAY_COUNT(SlashKeys), T);
			const FVector Blade = S.bCarry ? Nlerp(CarryBlade, K.B, W) : K.B;
			const float WBlade = S.bCarry ? 1.f : W;
			AimArm(Pose, S.SwordBone, Mesh(K.U), Mesh(K.F), W, SwordAxis, Mesh(Blade), WBlade);
			return;
		}

		if (S.Move != EJamFigureMove::Shot && Freeze < 0.f) { return; }
		const float W = Envelope(T, ShotIn, ShotOut);
		const FKey A = Sample(BowArmKeys, UE_ARRAY_COUNT(BowArmKeys), T);
		const FKey D = Sample(DrawArmKeys, UE_ARRAY_COUNT(DrawArmKeys), T);
		const FVector BowAxis = S.BowInBone.RotateVector(FVector::UpVector);
		AimArm(Pose, S.BowBone, MeshAim(A.U), MeshAim(A.F), W, BowAxis, Mesh(A.B), W);
		AimArm(Pose, S.DrawBone, MeshAim(D.U), MeshAim(D.F), W, FVector::ZeroVector, FVector::ZeroVector, 0.f);
	}

	FName OtherHand(FName Hand)
	{
		FString S = Hand.ToString();
		if (S.EndsWith(TEXT("_l"))) { return FName(*(S.LeftChop(2) + TEXT("_r"))); }
		if (S.EndsWith(TEXT("_r"))) { return FName(*(S.LeftChop(2) + TEXT("_l"))); }
		if (S.Contains(TEXT("Left"))) { return FName(*S.Replace(TEXT("Left"), TEXT("Right"))); }
		if (S.Contains(TEXT("Right"))) { return FName(*S.Replace(TEXT("Right"), TEXT("Left"))); }
		return NAME_None;
	}
}

/** Proxy: evalua la animacion suelta igual que siempre y luego aplica los giros de arma. */
struct FJamFigureAnimProxy : public FAnimSingleNodeInstanceProxy
{
	FJamFigureAnimProxy() = default;
	explicit FJamFigureAnimProxy(UAnimInstance* InAnimInstance) : FAnimSingleNodeInstanceProxy(InAnimInstance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override
	{
		FAnimSingleNodeInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
		if (const UJamFigureAnim* Anim = Cast<UJamFigureAnim>(InAnimInstance)) { Figure = Anim->GetFigureState(); }
	}

	virtual bool Evaluate(FPoseContext& Output) override
	{
		const bool bResult = FAnimSingleNodeInstanceProxy::Evaluate(Output);
		ApplyMoves(Output.Pose, Figure);
		return bResult;
	}

	FJamFigureState Figure;
};

FAnimInstanceProxy* UJamFigureAnim::CreateAnimInstanceProxy()
{
	return new FJamFigureAnimProxy(this);
}

void UJamFigureAnim::SetRig(FName InSwordBone, const FQuat& InSwordInBone, FName InBowBone, const FQuat& InBowInBone)
{
	SwordBone = InSwordBone;
	BowBone = InBowBone;
	DrawBone = OtherHand(InBowBone);
	SwordInBone = InSwordInBone;
	BowInBone = InBowInBone;
	bHasRig = true;
	if (const USkeletalMeshComponent* SK = GetSkelMeshComponent())
	{
		const FName Fore = SK->GetParentBone(SwordBone);
		UE_LOG(LogJam, Log, TEXT("Figura: espada en %s (codo %s, hombro %s), arco en %s, cuerda con %s"),
			*SwordBone.ToString(), *Fore.ToString(), *SK->GetParentBone(Fore).ToString(), *BowBone.ToString(), *DrawBone.ToString());
	}
}

void UJamFigureAnim::StartMove(EJamFigureMove InMove, float InDuration)
{
	Move = InMove;
	Elapsed = 0.f;
	Duration = FMath::Max(0.05f, InDuration);
}

void UJamFigureAnim::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	if (Move != EJamFigureMove::None)
	{
		Elapsed += DeltaSeconds;
		if (Elapsed >= Duration) { Move = EJamFigureMove::None; }
	}

	const USkeletalMeshComponent* SK = GetSkelMeshComponent();
	const ACharacter* Char = SK ? Cast<ACharacter>(SK->GetOwner()) : nullptr;
	const UJamWeaponComponent* Weapon = Char ? Char->FindComponentByClass<UJamWeaponComponent>() : nullptr;
	State.bRig = bHasRig && bMovesEnabled && Weapon && Weapon->IsInHand();
	State.bBow = Weapon && Weapon->Current == EJamWeapon::Bow;
	State.SwordBone = SwordBone;
	State.BowBone = BowBone;
	State.DrawBone = DrawBone;
	State.SwordInBone = SwordInBone;
	State.BowInBone = BowInBone;
	State.MeshQuat = SK ? SK->GetRelativeRotation().Quaternion() : FQuat::Identity;
	State.Move = Move;
	State.T = Move == EJamFigureMove::None ? 1.f : FMath::Clamp(Elapsed / Duration, 0.f, 1.f);
	State.Pitch = 0.f;
	if (Char && Char->GetController())
	{
		State.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Char->GetControlRotation().Pitch), -45.f, 45.f);
	}
}
