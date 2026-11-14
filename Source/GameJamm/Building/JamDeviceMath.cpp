#include "Building/JamDeviceMath.h"

bool JamDevices::SpendEnergy(FEnergyState& State, float Amount)
{
	if (Amount <= 0.f)
	{
		return true;
	}
	if (State.Reserve + State.Current < Amount)
	{
		State.Reserve = 0.f;
		State.Current = 0.f;
		return false;
	}
	const float FromReserve = FMath::Min(State.Reserve, Amount);
	State.Reserve -= FromReserve;
	State.Current = FMath::Max(0.f, State.Current - (Amount - FromReserve));
	return true;
}

float JamDevices::RegenEnergy(float Current, float Max, float TimeSinceSpend, float DeltaSeconds, float RegenPerSec, float RegenDelay)
{
	if (TimeSinceSpend < RegenDelay || DeltaSeconds <= 0.f || Current >= Max)
	{
		return Current;
	}
	return FMath::Min(Max, Current + RegenPerSec * DeltaSeconds);
}

float JamDevices::EnergyCost(float CostPerSec, float DriveScale, float DeltaSeconds)
{
	if (CostPerSec <= 0.f || DeltaSeconds <= 0.f)
	{
		return 0.f;
	}
	return CostPerSec * FMath::Min(FMath::Abs(DriveScale), 1.5f) * DeltaSeconds;
}

float JamDevices::PickupEnergy(FName ItemId)
{
	static const FName Small(TEXT("Energy.Small"));
	static const FName Large(TEXT("Energy.Large"));
	if (ItemId == Small)
	{
		return 15.f;
	}
	if (ItemId == Large)
	{
		return 40.f;
	}
	return 0.f;
}

bool JamDevices::AreHostile(EJamTeam Source, EJamTeam Target)
{
	return Source != Target;
}

bool JamDevices::IsInCone(const FVector& Origin, const FVector& Dir, const FVector& Point, float Range, float HalfAngleDeg)
{
	const FVector To = Point - Origin;
	const double Dist = To.Size();
	if (Dist > static_cast<double>(Range))
	{
		return false;
	}
	if (Dist < 1.0)
	{
		return true;
	}
	const FVector N = Dir.GetSafeNormal();
	if (N.IsNearlyZero())
	{
		return false;
	}
	const double Cos = FVector::DotProduct(To, N) / Dist;
	return Cos >= FMath::Cos(FMath::DegreesToRadians(static_cast<double>(HalfAngleDeg)));
}

int32 JamDevices::PickNearestTarget(const FVector& Origin, const TArray<FTargetCandidate>& Candidates, EJamTeam SourceTeam, float Range)
{
	int32 Best = INDEX_NONE;
	double BestDistSq = static_cast<double>(Range) * static_cast<double>(Range);
	for (int32 i = 0; i < Candidates.Num(); ++i)
	{
		const FTargetCandidate& C = Candidates[i];
		if (!C.bAlive || !AreHostile(SourceTeam, C.Team))
		{
			continue;
		}
		const double DistSq = FVector::DistSquared(Origin, C.Position);
		if (DistSq <= BestDistSq)
		{
			BestDistSq = DistSq;
			Best = i;
		}
	}
	return Best;
}

float JamDevices::SignedAngleAroundAxis(const FVector& From, const FVector& To, const FVector& Axis)
{
	const FVector N = Axis.GetSafeNormal();
	const FVector F = From - N * FVector::DotProduct(From, N);
	const FVector T = To - N * FVector::DotProduct(To, N);
	if (N.IsNearlyZero() || F.SizeSquared() < 1.e-8 || T.SizeSquared() < 1.e-8)
	{
		return 0.f;
	}
	const double Sin = FVector::DotProduct(N, FVector::CrossProduct(F, T));
	const double Cos = FVector::DotProduct(F, T);
	return static_cast<float>(FMath::Atan2(Sin, Cos));
}

float JamDevices::AimRate(float AngleError, float Kp, float MaxRate)
{
	return FMath::Clamp(AngleError * Kp, -MaxRate, MaxRate);
}

float JamDevices::SpinAccel(float TargetOmega, float CurrentOmega, float Gain, float MaxAccel, float DeltaSeconds)
{
	float K = Gain;
	if (DeltaSeconds > 0.f)
	{
		K = FMath::Min(K, 0.8f / DeltaSeconds);
	}
	return FMath::Clamp(K * (TargetOmega - CurrentOmega), -MaxAccel, MaxAccel);
}

float JamDevices::WheelScale(float Throttle, float Steer, float Side)
{
	const float T = FMath::Clamp(Throttle, -1.f, 1.f);
	const float S = FMath::Clamp(Steer, -1.f, 1.f);
	const bool bInner = Side * S > 0.f;
	return bInner ? T * (1.f - 0.5f * FMath::Abs(S)) : T;
}

float JamDevices::FanScale(bool bLiftFan, float Throttle, float Lift)
{
	if (bLiftFan)
	{
		return FMath::Clamp(1.f + 0.5f * Lift, 0.5f, 1.5f);
	}
	return FMath::Clamp(Throttle, -1.f, 1.f);
}

bool JamDevices::IsLiftDirection(const FVector& ThrustDir, const FVector& BuildUp)
{
	return FVector::DotProduct(ThrustDir.GetSafeNormal(), BuildUp.GetSafeNormal()) > 0.7;
}

FVector JamDevices::TiltedUp(const FVector& ForwardFlat, float Throttle, float MaxTiltDeg)
{
	const double Tilt = FMath::DegreesToRadians(static_cast<double>(MaxTiltDeg * FMath::Clamp(Throttle, -1.f, 1.f)));
	return (FVector::UpVector + ForwardFlat.GetSafeNormal() * FMath::Tan(Tilt)).GetSafeNormal();
}

FVector JamDevices::LevelAccel(const FVector& Up, const FVector& DesiredUp, const FVector& AngularVelocity, float Kp, float Kd)
{
	const FVector U = Up.GetSafeNormal();
	const FVector NonYaw = AngularVelocity - U * FVector::DotProduct(AngularVelocity, U);
	return FVector::CrossProduct(U, DesiredUp.GetSafeNormal()) * static_cast<double>(Kp) - NonYaw * static_cast<double>(Kd);
}

bool JamDevices::CooldownReady(float& Timer, float DeltaSeconds, float Interval)
{
	Timer += DeltaSeconds;
	if (Timer >= Interval)
	{
		Timer = 0.f;
		return true;
	}
	return false;
}

bool JamDevices::SeatActivates(EJamPartKind Kind)
{
	switch (Kind)
	{
	case EJamPartKind::Structural:
	case EJamPartKind::Seat:
	case EJamPartKind::Battery:
	case EJamPartKind::Bomb:
		return false;
	default:
		return true;
	}
}

float JamDevices::RefreshSlowEnd(float CurrentEnd, float Now, float Duration)
{
	return FMath::Max(CurrentEnd, Now + Duration);
}
