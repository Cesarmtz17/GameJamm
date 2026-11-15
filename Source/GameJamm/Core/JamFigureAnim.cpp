#include "Core/JamFigureAnim.h"
#include "Combat/JamWeaponComponent.h"
#include "GameJamm.h"
#include "Animation/AnimSingleNodeInstanceProxy.h"
#include "Animation/AnimNodeBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Controller.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarJamFigureMoves(TEXT("jam.Figure.Moves"), 1,
		TEXT("1 = espadazo/arco/espada cargada hechos por codigo sobre la figura; 0 = solo los clips de Characters.json."));
	TAutoConsoleVariable<int32> CVarJamFigureNatural(TEXT("jam.Figure.Natural"), 1,
		TEXT("1 = cuerpo natural en las figuras: cruce suave entre clips, inclinacion al correr y girar, cabeza hacia la mira."));
	TAutoConsoleVariable<float> CVarJamFigureFreeze(TEXT("jam.Figure.Freeze"), -1.f,
		TEXT("Pruebas: >= 0 congela el movimiento de arma en ese avance (0..1) para verlo en captura."));

	// ---- Poses (direcciones en el actor: X frente, Y derecha, Z arriba) ----
	// U = hombro > codo, F = codo > mano, B = hoja de la espada / largo del arco. Cero = no tocar ese hueso.
	struct FKey
	{
		float T;
		FVector U, F, B;
		float Twist = 0.f; // grados de giro del torso (+ = hombro derecho atras, izquierdo al frente)
		float Lean = 0.f;  // grados de inclinacion del torso (+ = al frente)
	};

	// Espada cargada al caminar/correr: punta al frente y hacia afuera (no cruza el torso ni las piernas).
	const FVector CarryBlade(0.55f, 0.65f, 0.1f);

	// Espadazo en diagonal (0.45 s): carga arriba a la derecha con el torso girado > cruza al frente > remata abajo a la
	// izquierda con el torso de vuelta. El dano entra en SwordHitDelay (0.19 s = T 0.42), cuando la hoja pasa al frente.
	const FKey SlashKeys[] = {
		{ 0.00f, FVector(-0.10f, 0.80f, 0.55f), FVector(-0.30f, 0.30f, 0.90f), FVector(-0.55f, 0.45f, 0.70f), 30.f, -6.f },  // atras arriba
		{ 0.28f, FVector(-0.25f, 0.80f, 0.55f), FVector(-0.40f, 0.30f, 0.85f), FVector(-0.70f, 0.35f, 0.60f), 38.f, -8.f },  // carga
		{ 0.45f, FVector(0.90f, 0.35f, 0.25f), FVector(1.00f, 0.05f, 0.05f), FVector(0.80f, -0.35f, 0.10f), -5.f, 10.f },    // golpe al frente
		{ 0.60f, FVector(0.60f, -0.45f, -0.25f), FVector(0.40f, -0.85f, -0.30f), FVector(0.15f, -0.80f, -0.55f), -35.f, 16.f }, // remate
		{ 1.00f, FVector(0.60f, -0.45f, -0.25f), FVector(0.40f, -0.85f, -0.30f), FVector(0.15f, -0.80f, -0.55f), -30.f, 12.f },
	};
	const float SlashIn = 0.15f, SlashOut = 0.72f; // entra rapido, sale despues del remate

	// Arco (0.6 s): de lado con el hombro izquierdo al frente, brazo del arco extendido y el arco vertical; la otra mano
	// tensa junto a la cara y suelta hacia atras. La flecha sale en BowReleaseDelay (0.17 s = T 0.28).
	const FKey BowArmKeys[] = {
		{ 0.00f, FVector(1.00f, -0.20f, 0.05f), FVector(1.00f, -0.05f, 0.05f), FVector(0.05f, -0.15f, 1.00f), -35.f, 0.f },
		{ 1.00f, FVector(1.00f, -0.20f, 0.05f), FVector(1.00f, -0.05f, 0.05f), FVector(0.05f, -0.15f, 1.00f), -35.f, 0.f },
	};
	const FKey DrawArmKeys[] = {
		{ 0.00f, FVector(0.60f, 0.70f, 0.05f), FVector(0.90f, -0.30f, 0.10f), FVector::ZeroVector },   // mano en la cuerda, al frente
		{ 0.18f, FVector(-0.55f, 0.80f, 0.10f), FVector(0.75f, -0.65f, 0.10f), FVector::ZeroVector },  // tensado: codo atras, mano en la barbilla
		{ 0.27f, FVector(-0.55f, 0.80f, 0.10f), FVector(0.75f, -0.65f, 0.10f), FVector::ZeroVector },  // apunta
		{ 0.36f, FVector(-0.70f, 0.70f, 0.15f), FVector(-0.35f, 0.90f, 0.25f), FVector::ZeroVector },  // suelta: la mano sale hacia atras
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
				return { T, Nlerp(A.U, B.U, Alpha), Nlerp(A.F, B.F, Alpha), Nlerp(A.B, B.B, Alpha), FMath::Lerp(A.Twist, B.Twist, Alpha), FMath::Lerp(A.Lean, B.Lean, Alpha) };
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

	// Torso: giro sobre el eje vertical y luego inclinacion al frente (direcciones del actor, se pasan al componente).
	void Torso(FCompactPose& Pose, FName SpineName, const FQuat& ToMesh, float Twist, float Lean, float W, float Roll = 0.f)
	{
		const FCompactPoseBoneIndex Spine = FindBone(Pose, SpineName);
		if (!Spine.IsValid() || W <= 0.f || (FMath::IsNearlyZero(Twist) && FMath::IsNearlyZero(Lean) && FMath::IsNearlyZero(Roll))) { return; }
		const float Yaw = FMath::DegreesToRadians(Twist * W), Tilt = FMath::DegreesToRadians(Lean * W), Side = FMath::DegreesToRadians(Roll * W);
		// Sin ambiguedad de signo: el giro lleva el frente (X) hacia la derecha (Y); la inclinacion lleva arriba (Z) al frente;
		// el ladeo lleva arriba (Z) hacia la derecha (Y).
		const FQuat TwistQ = FQuat::FindBetweenNormals(FVector::ForwardVector, FVector(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.f));
		const FQuat LeanQ = FQuat::FindBetweenNormals(FVector::UpVector, FVector(FMath::Sin(Tilt), 0.f, FMath::Cos(Tilt)));
		const FQuat RollQ = FQuat::FindBetweenNormals(FVector::UpVector, FVector(0.f, FMath::Sin(Side), FMath::Cos(Side)));
		const FQuat D = ToMesh * (TwistQ * LeanQ * RollQ) * ToMesh.Inverse();
		const FCompactPoseBoneIndex P = Pose.GetParentBoneIndex(Spine);
		const FQuat Qp = P.IsValid() ? ComponentSpace(Pose, P).GetRotation() : FQuat::Identity;
		FTransform& L = Pose[Spine];
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

	// Cuerpo natural: inclinacion del tronco (aceleracion y curvas) y cabeza hacia la mira (cuello 40 %, cabeza 60 %).
	void ApplyNatural(FCompactPose& Pose, const FJamFigureState& S)
	{
		if (!S.bNatural || CVarJamFigureNatural.GetValueOnAnyThread() == 0) { return; }
		const FQuat ToMesh = S.MeshQuat.Inverse();
		Torso(Pose, S.LowSpineBone, ToMesh, 0.f, S.BodyLean, 1.f, S.BodyRoll);
		const bool bNeck = !S.NeckBone.IsNone();
		Torso(Pose, S.NeckBone, ToMesh, S.LookYaw * 0.4f, -S.LookPitch * 0.4f, 1.f);
		Torso(Pose, S.HeadBone, ToMesh, S.LookYaw * (bNeck ? 0.6f : 1.f), -S.LookPitch * (bNeck ? 0.6f : 1.f), 1.f);
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
			Torso(Pose, S.SpineBone, ToMesh, K.Twist, K.Lean, W);
			AimArm(Pose, S.SwordBone, Mesh(K.U), Mesh(K.F), W, SwordAxis, Mesh(Blade), WBlade);
			return;
		}

		if (S.Move != EJamFigureMove::Shot && Freeze < 0.f) { return; }
		const float W = Envelope(T, ShotIn, ShotOut);
		const FKey A = Sample(BowArmKeys, UE_ARRAY_COUNT(BowArmKeys), T);
		const FKey D = Sample(DrawArmKeys, UE_ARRAY_COUNT(DrawArmKeys), T);
		const FVector BowAxis = S.BowInBone.RotateVector(FVector::UpVector);
		Torso(Pose, S.SpineBone, ToMesh, A.Twist, A.Lean, W);
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
		FadeLeft -= DeltaSeconds;
	}

	virtual bool Evaluate(FPoseContext& Output) override
	{
		const bool bResult = FAnimSingleNodeInstanceProxy::Evaluate(Output);
		FCompactPose& Pose = Output.Pose;
		// Cruce: al cambiar de clip, mezcla desde la ultima pose vista hacia el clip nuevo (sin saltos Idle > Run > Hit).
		const bool bNatural = Figure.bNatural && CVarJamFigureNatural.GetValueOnAnyThread() != 0;
		if (Figure.ClipSerial != LastSerial)
		{
			LastSerial = Figure.ClipSerial;
			if (bNatural && bHavePrev && Prev.GetNumBones() == Pose.GetNumBones())
			{
				Snapshot.CopyBonesFrom(Prev);
				FadeTotal = FMath::Max(0.01f, Figure.FadeTime);
				FadeLeft = FadeTotal;
			}
		}
		if (bNatural && FadeLeft > 0.f && Snapshot.GetNumBones() == Pose.GetNumBones())
		{
			const float Alpha = FMath::SmoothStep(0.f, 1.f, 1.f - FadeLeft / FadeTotal);
			for (const FCompactPoseBoneIndex I : Pose.ForEachBoneIndex())
			{
				FTransform B;
				B.Blend(Snapshot[I], Pose[I], Alpha);
				Pose[I] = B;
			}
		}
		Prev.CopyBonesFrom(Pose);
		bHavePrev = true;
		ApplyNatural(Pose, Figure);
		ApplyMoves(Pose, Figure);
		return bResult;
	}

	FJamFigureState Figure;
	FCompactHeapPose Prev, Snapshot;
	bool bHavePrev = false;
	uint32 LastSerial = 0;
	float FadeLeft = 0.f, FadeTotal = 0.2f;
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
		SpineBone = NAME_None;
		FName B = SK->GetParentBone(SK->GetParentBone(SwordBone));
		for (int32 I = 0; I < 6 && !B.IsNone(); ++I, B = SK->GetParentBone(B))
		{
			if (B.ToString().Contains(TEXT("spine"))) { SpineBone = B; break; }
		}
		const FName Fore = SK->GetParentBone(SwordBone);
		UE_LOG(LogJam, Log, TEXT("Figura: espada en %s (codo %s, hombro %s), arco en %s, cuerda con %s, torso %s"),
			*SwordBone.ToString(), *Fore.ToString(), *SK->GetParentBone(Fore).ToString(), *BowBone.ToString(), *DrawBone.ToString(), *SpineBone.ToString());
	}
}

void UJamFigureAnim::ResolveBodyBones()
{
	bBodyBones = true;
	const USkeletalMeshComponent* SK = GetSkelMeshComponent();
	const USkeletalMesh* Mesh = SK ? SK->GetSkeletalMeshAsset() : nullptr;
	if (!Mesh) { return; }
	const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
	for (int32 I = 0; I < Ref.GetNum(); ++I)
	{
		const FName N = Ref.GetBoneName(I);
		const FString S = N.ToString().ToLower();
		if (LowSpineBone.IsNone() && S.Contains(TEXT("spine"))) { LowSpineBone = N; }
		if (NeckBone.IsNone() && S.Contains(TEXT("neck"))) { NeckBone = N; }
		if (HeadBone.IsNone() && S.StartsWith(TEXT("head")) && !S.Contains(TEXT("end"))) { HeadBone = N; }
	}
	UE_LOG(LogJam, Verbose, TEXT("Figura %s: tronco %s, cuello %s, cabeza %s"), *GetNameSafe(SK->GetOwner()), *LowSpineBone.ToString(), *NeckBone.ToString(), *HeadBone.ToString());
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
	State.SpineBone = SpineBone;
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

	// ---- Cuerpo natural ----
	if (!bBodyBones) { ResolveBodyBones(); }
	const UAnimationAsset* Asset = const_cast<UJamFigureAnim*>(this)->GetCurrentAsset();
	if (Asset != LastAsset.Get())
	{
		const bool bFirst = !LastAsset.IsValid();
		LastAsset = Asset;
		if (!bFirst) { ++ClipSerial; }
	}
	State.ClipSerial = ClipSerial;
	State.FadeTime = 0.2f;
	State.LowSpineBone = LowSpineBone;
	State.NeckBone = NeckBone;
	State.HeadBone = HeadBone;
	State.bNatural = Char != nullptr && bMovesEnabled;
	if (Char && DeltaSeconds > KINDA_SMALL_NUMBER)
	{
		const FVector Vel = Char->GetVelocity();
		const FRotator Rot = Char->GetActorRotation();
		if (!bHasPrev) { PrevVel = Vel; PrevYaw = Rot.Yaw; bHasPrev = true; }
		const FVector Acc = Rot.UnrotateVector((Vel - PrevVel) / DeltaSeconds);
		const float YawRate = FRotator::NormalizeAxis(Rot.Yaw - PrevYaw) / DeltaSeconds;
		PrevVel = Vel;
		PrevYaw = Rot.Yaw;
		const float Speed = Vel.Size2D() / FMath::Max(0.1f, Char->GetActorScale3D().X);
		const UCharacterMovementComponent* Move2 = Char->GetCharacterMovement();
		const bool bAir = Move2 && Move2->IsFalling();
		// Al frente: un poco con la velocidad y mas al acelerar (atras al frenar); al lado: hacia la curva, mas si va rapido.
		const float WantLean = bAir ? 0.f : FMath::Clamp(Speed / 600.f * 5.f + Acc.X / 2000.f * 6.f, -6.f, 10.f);
		const float WantRoll = bAir ? 0.f : FMath::Clamp(YawRate * FMath::Min(Speed, 700.f) / 600.f * 0.04f, -9.f, 9.f);
		BodyLean = FMath::FInterpTo(BodyLean, WantLean, DeltaSeconds, 6.f);
		BodyRoll = FMath::FInterpTo(BodyRoll, WantRoll, DeltaSeconds, 6.f);

		// Cabeza: hacia donde mira el control (jugador: la camara; bots: su objetivo). Si mira hacia atras, la cabeza se queda.
		float WantYaw = 0.f, WantPitch = 0.f;
		if (Char->GetController())
		{
			const FRotator Ctrl = Char->GetControlRotation();
			const float Diff = FRotator::NormalizeAxis(Ctrl.Yaw - Rot.Yaw);
			const float Keep = 1.f - FMath::SmoothStep(90.f, 150.f, FMath::Abs(Diff));
			WantYaw = FMath::Clamp(Diff, -60.f, 60.f) * Keep;
			WantPitch = FMath::Clamp(FRotator::NormalizeAxis(Ctrl.Pitch) * 0.4f, -15.f, 20.f) * Keep;
		}
		LookYaw = FMath::FInterpTo(LookYaw, WantYaw, DeltaSeconds, 5.f);
		LookPitch = FMath::FInterpTo(LookPitch, WantPitch, DeltaSeconds, 5.f);
	}
	State.BodyLean = BodyLean;
	State.BodyRoll = BodyRoll;
	State.LookYaw = LookYaw;
	State.LookPitch = LookPitch;
}
