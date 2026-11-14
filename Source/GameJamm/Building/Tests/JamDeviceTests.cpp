// Tests de la lógica pura de dispositivos (sin mundo). Mismos casos que Content/Game/Building/Data/devices_proto.py.
// Correr: editor -> Tools -> Session Frontend -> Automation -> "GameJamm.Building.Devices",
// o por consola:  UnrealEditor-Cmd GameJamm.uproject -ExecCmds="Automation RunTests GameJamm.Building.Devices; Quit" -unattended -nullrhi
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Building/JamBuildPart.h"
#include "Building/JamDeviceMath.h"

namespace JamDeviceTestsPrivate
{
	bool Spend(float Reserve, float Current, float Amount, float ExpectReserve, float ExpectCurrent, bool bExpectOk)
	{
		JamDevices::FEnergyState S;
		S.Reserve = Reserve;
		S.Current = Current;
		const bool bOk = JamDevices::SpendEnergy(S, Amount);
		return bOk == bExpectOk && FMath::IsNearlyEqual(S.Reserve, ExpectReserve, 1.e-4f) && FMath::IsNearlyEqual(S.Current, ExpectCurrent, 1.e-4f);
	}

	JamDevices::FTargetCandidate Cand(double X, EJamTeam Team, bool bAlive)
	{
		JamDevices::FTargetCandidate C;
		C.Position = FVector(X, 0.0, 0.0);
		C.Team = Team;
		C.bAlive = bAlive;
		return C;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJamDeviceEnergy, "GameJamm.Building.Devices.Energy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJamDeviceEnergy::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("bar only"), JamDeviceTestsPrivate::Spend(0.f, 60.f, 10.f, 0.f, 50.f, true));
	TestTrue(TEXT("reserve first"), JamDeviceTestsPrivate::Spend(30.f, 60.f, 10.f, 20.f, 60.f, true));
	TestTrue(TEXT("reserve then bar"), JamDeviceTestsPrivate::Spend(5.f, 60.f, 10.f, 0.f, 55.f, true));
	TestTrue(TEXT("not enough -> zero + false"), JamDeviceTestsPrivate::Spend(1.f, 2.f, 10.f, 0.f, 0.f, false));
	TestTrue(TEXT("zero amount is free"), JamDeviceTestsPrivate::Spend(0.f, 0.f, 0.f, 0.f, 0.f, true));

	TestEqual(TEXT("regen waits delay"), JamDevices::RegenEnergy(10.f, 100.f, 2.9f, 1.f, 2.f, 3.f), 10.f);
	TestEqual(TEXT("regen after delay"), JamDevices::RegenEnergy(10.f, 100.f, 3.f, 1.f, 2.f, 3.f), 12.f);
	TestEqual(TEXT("regen clamps to max"), JamDevices::RegenEnergy(99.5f, 100.f, 10.f, 1.f, 2.f, 3.f), 100.f);

	TestEqual(TEXT("fan 1 s"), JamDevices::EnergyCost(1.5f, 1.f, 1.f), 1.5f);
	TestEqual(TEXT("throttle 0 is free"), JamDevices::EnergyCost(1.5f, 0.f, 1.f), 0.f);
	TestEqual(TEXT("reverse costs like forward"), JamDevices::EnergyCost(0.2f, -1.f, 0.5f), 0.1f);

	TestEqual(TEXT("small crystal"), JamDevices::PickupEnergy(FName(TEXT("Energy.Small"))), 15.f);
	TestEqual(TEXT("large crystal"), JamDevices::PickupEnergy(FName(TEXT("Energy.Large"))), 40.f);
	TestEqual(TEXT("not energy"), JamDevices::PickupEnergy(FName(TEXT("Part.Fan"))), 0.f);

	// Research 13 §6: carro con 4 ruedas + 1 ventilador = 2.3/s; la barra llena dura ~43 s.
	{
		JamDevices::FEnergyState S;
		S.Current = 100.f;
		const float Dt = 1.f / 60.f;
		const float Costs[] = { 0.2f, 0.2f, 0.2f, 0.2f, 1.5f };
		float T = 0.f;
		bool bOk = true;
		while (bOk && T < 200.f)
		{
			for (float C : Costs)
			{
				bOk = JamDevices::SpendEnergy(S, JamDevices::EnergyCost(C, 1.f, Dt));
				if (!bOk)
				{
					break;
				}
			}
			T += Dt;
		}
		TestTrue(TEXT("car lasts ~43 s"), T >= 42.5f && T <= 44.5f);
	}

	// Catálogo: costos por segundo y reserva de la batería.
	FJamPartDef D;
	TestTrue(TEXT("Fan in catalog"), UJamPartCatalog::Find("Fan", D));
	TestEqual(TEXT("Fan cost"), D.EnergyPerSec, 1.5f);
	TestTrue(TEXT("Wheel in catalog"), UJamPartCatalog::Find("Wheel", D));
	TestEqual(TEXT("Wheel cost"), D.EnergyPerSec, 0.2f);
	TestTrue(TEXT("Cannon in catalog"), UJamPartCatalog::Find("Cannon", D));
	TestEqual(TEXT("Cannon cost"), D.EnergyPerSec, 3.f);
	TestTrue(TEXT("Seat in catalog"), UJamPartCatalog::Find("Seat", D));
	TestEqual(TEXT("Seat is free"), D.EnergyPerSec, 0.f);
	TestTrue(TEXT("Rocket in catalog"), UJamPartCatalog::Find("Rocket", D));
	TestEqual(TEXT("Rocket is free"), D.EnergyPerSec, 0.f);
	TestTrue(TEXT("Battery in catalog"), UJamPartCatalog::Find("Battery", D));
	TestEqual(TEXT("Battery reserve"), D.Power, JamDevices::BatteryReserve);
	TestTrue(TEXT("Head in catalog"), UJamPartCatalog::Find("CTRL_HEAD", D));
	TestTrue(TEXT("Head kind"), D.Kind == EJamPartKind::Head);
	TestTrue(TEXT("Turntable in catalog"), UJamPartCatalog::Find("Turntable", D));
	TestTrue(TEXT("Stake in catalog"), UJamPartCatalog::Find("Stake", D));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJamDeviceConeAndTeams, "GameJamm.Building.Devices.ConeAndTeams",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJamDeviceConeAndTeams::RunTest(const FString& Parameters)
{
	const FVector O = FVector::ZeroVector;
	const FVector Fwd = FVector::ForwardVector;
	TestTrue(TEXT("ahead"), JamDevices::IsInCone(O, Fwd, FVector(300, 0, 0), 400.f, 25.f));
	TestFalse(TEXT("out of range"), JamDevices::IsInCone(O, Fwd, FVector(401, 0, 0), 400.f, 25.f));
	TestFalse(TEXT("behind"), JamDevices::IsInCone(O, Fwd, FVector(-100, 0, 0), 400.f, 25.f));
	const double C24 = FMath::Cos(FMath::DegreesToRadians(24.0)), S24 = FMath::Sin(FMath::DegreesToRadians(24.0));
	const double C26 = FMath::Cos(FMath::DegreesToRadians(26.0)), S26 = FMath::Sin(FMath::DegreesToRadians(26.0));
	TestTrue(TEXT("inside edge 24 deg"), JamDevices::IsInCone(O, Fwd, FVector(300.0 * C24, 300.0 * S24, 0.0), 400.f, 25.f));
	TestFalse(TEXT("outside edge 26 deg"), JamDevices::IsInCone(O, Fwd, FVector(300.0 * C26, 300.0 * S26, 0.0), 400.f, 25.f));
	TestTrue(TEXT("at origin"), JamDevices::IsInCone(O, Fwd, FVector(0.2, 0, 0), 400.f, 25.f));
	TestTrue(TEXT("unnormalized dir"), JamDevices::IsInCone(O, FVector(10, 0, 0), FVector(300, 0, 0), 400.f, 25.f));
	// Si la base gira, el cono gira.
	TestFalse(TEXT("rotated cone misses old point"), JamDevices::IsInCone(O, FVector::RightVector, FVector(300, 0, 0), 400.f, 25.f));
	TestTrue(TEXT("rotated cone hits new point"), JamDevices::IsInCone(O, FVector::RightVector, FVector(0, 300, 0), 400.f, 25.f));

	TestTrue(TEXT("red vs blue"), JamDevices::AreHostile(EJamTeam::Red, EJamTeam::Blue));
	TestTrue(TEXT("red vs neutral"), JamDevices::AreHostile(EJamTeam::Red, EJamTeam::Neutral));
	TestTrue(TEXT("neutral vs blue"), JamDevices::AreHostile(EJamTeam::Neutral, EJamTeam::Blue));
	TestFalse(TEXT("no friendly fire"), JamDevices::AreHostile(EJamTeam::Red, EJamTeam::Red));
	TestFalse(TEXT("neutral vs neutral"), JamDevices::AreHostile(EJamTeam::Neutral, EJamTeam::Neutral));

	// Llama: 15/s en ticks de 0.25 s -> 100 de vida en 27 ticks.
	float Hp = 100.f;
	int32 Ticks = 0;
	while (Hp > 0.f)
	{
		Hp -= JamDevices::FlameDps * JamDevices::DamageInterval;
		++Ticks;
	}
	TestEqual(TEXT("flame kills 100 hp in 27 ticks"), Ticks, 27);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJamDeviceTargeting, "GameJamm.Building.Devices.Targeting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJamDeviceTargeting::RunTest(const FString& Parameters)
{
	TArray<JamDevices::FTargetCandidate> C;
	C.Add(JamDeviceTestsPrivate::Cand(1000, EJamTeam::Blue, true));
	C.Add(JamDeviceTestsPrivate::Cand(500, EJamTeam::Red, true));
	C.Add(JamDeviceTestsPrivate::Cand(300, EJamTeam::Blue, false));
	TestEqual(TEXT("skips ally and dead"), JamDevices::PickNearestTarget(FVector::ZeroVector, C, EJamTeam::Red, 2000.f), 0);
	C.Add(JamDeviceTestsPrivate::Cand(800, EJamTeam::Neutral, true));
	C.Add(JamDeviceTestsPrivate::Cand(5000, EJamTeam::Blue, true));
	TestEqual(TEXT("nearest hostile alive"), JamDevices::PickNearestTarget(FVector::ZeroVector, C, EJamTeam::Red, 2000.f), 3);
	TestEqual(TEXT("none in range"), JamDevices::PickNearestTarget(FVector::ZeroVector, C, EJamTeam::Red, 700.f), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("neutral turret hits teams"), JamDevices::PickNearestTarget(FVector::ZeroVector, C, EJamTeam::Neutral, 2000.f), 1);
	TestEqual(TEXT("empty"), JamDevices::PickNearestTarget(FVector::ZeroVector, TArray<JamDevices::FTargetCandidate>(), EJamTeam::Red, 2000.f), static_cast<int32>(INDEX_NONE));

	const FVector Fwd = FVector::ForwardVector, Up = FVector::UpVector;
	TestTrue(TEXT("left 90"), FMath::IsNearlyEqual(JamDevices::SignedAngleAroundAxis(Fwd, FVector::RightVector, Up), HALF_PI, 1.e-4f));
	TestTrue(TEXT("right 90"), FMath::IsNearlyEqual(JamDevices::SignedAngleAroundAxis(Fwd, -FVector::RightVector, Up), -HALF_PI, 1.e-4f));
	TestTrue(TEXT("ignores height"), FMath::IsNearlyEqual(JamDevices::SignedAngleAroundAxis(Fwd, FVector(1, 1, 5), Up), PI / 4.f, 1.e-4f));
	TestEqual(TEXT("parallel to axis is zero"), JamDevices::SignedAngleAroundAxis(Fwd, Up, Up), 0.f);
	TestEqual(TEXT("rate saturates +"), JamDevices::AimRate(2.f, 6.f, 3.f), 3.f);
	TestEqual(TEXT("rate saturates -"), JamDevices::AimRate(-2.f, 6.f, 3.f), -3.f);
	TestTrue(TEXT("rate proportional"), FMath::IsNearlyEqual(JamDevices::AimRate(0.1f, 6.f, 3.f), 0.6f, 1.e-4f));

	// Lazo cerrado: el signo del ángulo coincide con omega x r, así que la Head converge al objetivo.
	FVector Aim = Fwd;
	const FVector Target = FVector(-1, 1, 0).GetSafeNormal();
	const float Dt = 1.f / 60.f;
	float Omega = 0.f;
	for (int32 i = 0; i < 240; ++i)
	{
		const float Err = JamDevices::SignedAngleAroundAxis(Aim, Target, Up);
		Omega += JamDevices::SpinAccel(JamDevices::AimRate(Err, JamDevices::HeadKp, JamDevices::HeadMaxRate), Omega, JamDevices::SpinGain, JamDevices::SpinMaxAccel, Dt) * Dt;
		Aim = (Aim + FVector::CrossProduct(Up * static_cast<double>(Omega), Aim) * static_cast<double>(Dt)).GetSafeNormal();
	}
	TestTrue(TEXT("head converges in 4 s"), FMath::Abs(JamDevices::SignedAngleAroundAxis(Aim, Target, Up)) < 0.02f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJamDeviceSpinAndSeat, "GameJamm.Building.Devices.SpinAndSeat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJamDeviceSpinAndSeat::RunTest(const FString& Parameters)
{
	const float Dt = 1.f / 60.f;
	TestTrue(TEXT("proportional"), FMath::IsNearlyEqual(JamDevices::SpinAccel(10.f, 8.f, 30.f, 200.f, Dt), 60.f, 1.e-3f));
	TestEqual(TEXT("saturates"), JamDevices::SpinAccel(10.f, 0.f, 30.f, 200.f, Dt), 200.f);
	TestTrue(TEXT("gain limited at low fps"), FMath::IsNearlyEqual(JamDevices::SpinAccel(1.f, 0.f, 30.f, 200.f, 0.1f), 8.f, 1.e-3f));

	// Rueda raíz: de 0 a 10 rad/s sin pasarse.
	float W = 0.f, Peak = 0.f;
	for (int32 i = 0; i < 120; ++i)
	{
		W += JamDevices::SpinAccel(10.f, W, JamDevices::SpinGain, JamDevices::SpinMaxAccel, Dt) * Dt;
		Peak = FMath::Max(Peak, W);
	}
	TestTrue(TEXT("root wheel reaches target"), FMath::IsNearlyEqual(W, 10.f, 0.01f) && Peak <= 10.001f);
	W = 0.f;
	for (int32 i = 0; i < 40; ++i)
	{
		W += JamDevices::SpinAccel(10.f, W, JamDevices::SpinGain, JamDevices::SpinMaxAccel, 0.1f) * 0.1f;
	}
	TestTrue(TEXT("stable at 10 fps"), FMath::IsNearlyEqual(W, 10.f, 0.05f));

	TestEqual(TEXT("wheel straight L"), JamDevices::WheelScale(1.f, 0.f, -1.f), 1.f);
	TestEqual(TEXT("turn right slows right"), JamDevices::WheelScale(1.f, 1.f, 1.f), 0.5f);
	TestEqual(TEXT("turn right keeps left"), JamDevices::WheelScale(1.f, 1.f, -1.f), 1.f);
	TestEqual(TEXT("turn left slows left"), JamDevices::WheelScale(1.f, -1.f, -1.f), 0.5f);
	TestEqual(TEXT("reverse"), JamDevices::WheelScale(-1.f, 0.f, 1.f), -1.f);
	TestEqual(TEXT("root wheel (side 0)"), JamDevices::WheelScale(0.5f, 1.f, 0.f), 0.5f);

	TestEqual(TEXT("thrust fan follows throttle"), JamDevices::FanScale(false, 0.5f, 1.f), 0.5f);
	TestEqual(TEXT("thrust fan clamps"), JamDevices::FanScale(false, -2.f, 0.f), -1.f);
	TestEqual(TEXT("lift fan idle"), JamDevices::FanScale(true, 0.f, 0.f), 1.f);
	TestEqual(TEXT("lift fan up"), JamDevices::FanScale(true, 0.f, 1.f), 1.5f);
	TestEqual(TEXT("lift fan down"), JamDevices::FanScale(true, 1.f, -1.f), 0.5f);

	const FVector Up = FVector::UpVector, Fwd = FVector::ForwardVector;
	TestTrue(TEXT("up is lift"), JamDevices::IsLiftDirection(Up, Up));
	TestFalse(TEXT("forward is not lift"), JamDevices::IsLiftDirection(Fwd, Up));
	TestFalse(TEXT("down is not lift"), JamDevices::IsLiftDirection(-Up, Up));

	const FVector Tilted = JamDevices::TiltedUp(Fwd, 1.f, 15.f);
	TestTrue(TEXT("tilt forward 15 deg"), FMath::IsNearlyEqual(FMath::RadiansToDegrees(FMath::Atan2(Tilted.X, Tilted.Z)), 15.0, 1.e-3) && FMath::IsNearlyZero(Tilted.Y));
	TestTrue(TEXT("no tilt without throttle"), JamDevices::TiltedUp(Fwd, 0.f, 15.f).Equals(Up, 1.e-4));

	TestTrue(TEXT("level: nothing to do when level (yaw spin ignored)"), JamDevices::LevelAccel(Up, Up, FVector(0, 0, 3), 25.f, 8.f).IsNearlyZero(1.e-4));
	const FVector Rolled = FVector(0.0, 0.5, 1.0).GetSafeNormal();
	const FVector Restore = JamDevices::LevelAccel(Rolled, Up, FVector::ZeroVector, 25.f, 8.f);
	TestTrue(TEXT("level restores"), FVector::CrossProduct(Restore, Rolled).Y < 0.0);
	TestTrue(TEXT("level damps roll"), JamDevices::LevelAccel(Up, Up, FVector(2, 0, 0), 25.f, 8.f).Equals(FVector(-16, 0, 0), 1.e-4));

	// Lazo cerrado del nivelado: de 30° a menos de 1° en 2 s.
	FVector U = FVector(0.0, FMath::Tan(FMath::DegreesToRadians(30.0)), 1.0).GetSafeNormal();
	FVector Omega = FVector::ZeroVector;
	for (int32 i = 0; i < 120; ++i)
	{
		Omega += JamDevices::LevelAccel(U, Up, Omega, JamDevices::SeatLevelKp, JamDevices::SeatLevelKd) * static_cast<double>(Dt);
		U = (U + FVector::CrossProduct(Omega, U) * static_cast<double>(Dt)).GetSafeNormal();
	}
	TestTrue(TEXT("level converges"), FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(U, Up), -1.0, 1.0))) < 1.0);

	TestTrue(TEXT("seat turns on fan"), JamDevices::SeatActivates(EJamPartKind::Fan));
	TestTrue(TEXT("seat turns on wheel"), JamDevices::SeatActivates(EJamPartKind::Wheel));
	TestTrue(TEXT("seat turns on flame"), JamDevices::SeatActivates(EJamPartKind::Flame));
	TestTrue(TEXT("seat turns on stake"), JamDevices::SeatActivates(EJamPartKind::Stake));
	TestFalse(TEXT("not structural"), JamDevices::SeatActivates(EJamPartKind::Structural));
	TestFalse(TEXT("not seat"), JamDevices::SeatActivates(EJamPartKind::Seat));
	TestFalse(TEXT("not battery"), JamDevices::SeatActivates(EJamPartKind::Battery));
	TestFalse(TEXT("not bomb"), JamDevices::SeatActivates(EJamPartKind::Bomb));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJamDeviceCooldownAndStatus, "GameJamm.Building.Devices.CooldownAndStatus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJamDeviceCooldownAndStatus::RunTest(const FString& Parameters)
{
	float Timer = 0.f;
	int32 Shots = 0;
	for (int32 i = 0; i < 48; ++i) // 6 s en pasos de 0.125 s (exacto en binario)
	{
		if (JamDevices::CooldownReady(Timer, 0.125f, JamDevices::CannonInterval))
		{
			++Shots;
		}
	}
	TestEqual(TEXT("cannon: 4 shots in 6 s"), Shots, 4);

	Timer = 0.f;
	TestFalse(TEXT("not ready early"), JamDevices::CooldownReady(Timer, 0.1f, 1.5f));
	TestEqual(TEXT("timer accumulates"), Timer, 0.1f);

	TestEqual(TEXT("slow starts"), JamDevices::RefreshSlowEnd(0.f, 10.f, 3.f), 13.f);
	TestEqual(TEXT("slow refreshes"), JamDevices::RefreshSlowEnd(13.f, 11.f, 3.f), 14.f);
	TestEqual(TEXT("slow never shortens"), JamDevices::RefreshSlowEnd(20.f, 10.f, 3.f), 20.f);
	TestEqual(TEXT("frost = 50% for 3 s"), JamDevices::FrostSlowFactor * JamDevices::FrostSlowDuration, 1.5f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
