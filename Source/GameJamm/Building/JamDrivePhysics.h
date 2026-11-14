#pragma once

#include "CoreMinimal.h"

namespace JamDrivePhysics
{
	// Fuerza de traccion hasta 0.35 g, limitada ademas por el momento que levanta un eje.
	// Unidades de Chaos: kg, cm, segundos; el limite del motor es kg*cm^2/s^2.
	inline float WheelTorque(float MassKg, float RadiusCm, int32 Motors, float PitchMarginCm)
	{
		if (Motors <= 0 || MassKg <= 0.f || RadiusCm <= 0.f) { return 0.f; }
		const float Traction = MassKg * 980.f * 0.35f * RadiusCm;
		const float Tip = MassKg * 980.f * FMath::Max(PitchMarginCm, 0.f) * 0.65f;
		return FMath::Min(Traction, Tip) / Motors;
	}

	inline double GroundBoostScale(double MassKg, double Power)
	{
		return FMath::Min(1.0, FMath::Max(MassKg, 0.0) * 980.0 * 0.65 / FMath::Max(Power, 1.0));
	}

	inline float YawRate(float SpeedCmS, float HalfTrackCm, float ComHeightCm)
	{
		const float LateralG = FMath::Clamp(0.65f * HalfTrackCm / FMath::Max(ComHeightCm, 1.f), 0.f, 0.45f);
		return FMath::Min(1.5f, LateralG * 980.f / FMath::Max(FMath::Abs(SpeedCmS), 1.f));
	}
}
