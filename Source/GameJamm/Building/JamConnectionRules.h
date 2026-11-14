#pragma once
#include "Building/JamBuildPart.h"

// Compartido por el ensamblador y el catalogo enviado al modelo.
namespace JamConnections
{
	inline bool Inert(EJamPartKind K) { return K == EJamPartKind::Structural || K == EJamPartKind::Seat || K == EJamPartKind::Battery || K == EJamPartKind::Slide; }
	inline bool AxleY(EJamPartKind K) { return K == EJamPartKind::Wheel || K == EJamPartKind::Hinge; }
	inline bool AxleZ(EJamPartKind K) { return K == EJamPartKind::Turntable || K == EJamPartKind::Head; }
	inline bool Motor(const FJamPartDef& D) { return (D.Kind == EJamPartKind::Wheel && D.Power > 0.f) || D.Kind == EJamPartKind::Turntable; }
	inline FString Describe(const FJamPartDef& D)
	{
		if (D.Kind == EJamPartKind::Wheel) { return D.Power > 0 ? TEXT("Y axle, motor; keep ground contact and clearance, spread wheels around center of mass") : TEXT("Y axle, free rolling, no motor; needs another source of propulsion"); }
		if (D.Kind == EJamPartKind::Hinge) { return FString::Printf(TEXT("Y hinge, limited to %.0f degrees; no motor"), D.Power); }
		if (D.Kind == EJamPartKind::Turntable) { return TEXT("Z axle, motor rotates attached children"); }
		if (D.Kind == EJamPartKind::Head) { return TEXT("Z axle, aims attached weapons at targets"); }
		return Inert(D.Kind) ? TEXT("welded to inert parent, otherwise fixed joint") : TEXT("fixed joint to parent; its capability supplies force or effects");
	}
}
