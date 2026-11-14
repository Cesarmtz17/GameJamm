#pragma once

#include "CoreMinimal.h"
#include "Building/JamBuildPart.h"
#include "Core/JamTypes.h"

/**
 * Lógica pura de los dispositivos (sin mundo, testeable): energía, conos de daño, selección de objetivo,
 * cooldowns y control del asiento. La usan AJamBuildPart, UJamEnergyComponent y Tests/JamDeviceTests.cpp.
 * Espejo ejecutable en Content/Game/Building/Data/devices_proto.py (mismos casos de prueba).
 */
namespace JamDevices
{
	// ---- Valores de diseño (docs/mechanics-research/13-conocimiento-ia-construccion.md §2, §5, §6, §7) ----
	constexpr float DamageInterval = 0.25f;        // s entre aplicaciones de daño/estado de un cono o rayo
	constexpr float FlameDps = 15.f;
	constexpr float FlameRange = 400.f;            // cm
	constexpr float FlameHalfAngleDeg = 25.f;
	constexpr float FrostRange = 350.f;
	constexpr float FrostHalfAngleDeg = 30.f;
	constexpr float FrostSlowFactor = 0.5f;        // lento 50 %
	constexpr float FrostSlowDuration = 3.f;       // por 3 s
	constexpr float BeamDps = 20.f;
	constexpr float BeamRange = 1500.f;
	constexpr float CannonDamage = 25.f;
	constexpr float CannonInterval = 1.5f;
	constexpr float CannonSpeed = 2500.f;          // cm/s
	constexpr float HeadRange = 2000.f;
	constexpr float HeadKp = 6.f;                  // (rad/s) por rad de error
	constexpr float HeadMaxRate = 3.f;             // rad/s
	constexpr float TurntableRate = 2.f;           // rad/s
	constexpr float SpinGain = 30.f;               // 1/s
	constexpr float SpinMaxAccel = 200.f;          // rad/s^2
	constexpr float BalloonHeatMultiplier = 2.f;   // fuego debajo del globo = x2
	constexpr float BalloonHeatHold = 0.5f;        // s que dura el calor tras la última llama
	constexpr float SpringInterval = 1.5f;
	constexpr float SpringLaunchBoost = 2.f;
	constexpr float SeatMaxYawRate = 2.f;          // rad/s
	constexpr float SeatYawGain = 8.f;             // 1/s
	constexpr float SeatLevelKp = 25.f;            // 1/s^2
	constexpr float SeatLevelKd = 8.f;             // 1/s
	constexpr float SeatMaxTiltDeg = 15.f;         // inclinación hacia adelante con W cuando solo hay empuje vertical

	/** Barra del jugador + reserva de baterías pegadas a la construcción. */
	struct FEnergyState
	{
		float Reserve = 0.f;
		float Current = 0.f;
	};

	/** Candidato a objetivo de una Head/torreta. */
	struct FTargetCandidate
	{
		FVector Position = FVector::ZeroVector;
		EJamTeam Team = EJamTeam::Neutral;
		bool bAlive = true;
	};

	/** Gasta primero la reserva y luego la barra. Si no alcanza: deja todo en 0 y devuelve false. */
	GAMEJAMM_API bool SpendEnergy(FEnergyState& State, float Amount);

	/** Recarga en reposo: solo tras RegenDelay segundos sin gastar; nunca pasa de Max. */
	GAMEJAMM_API float RegenEnergy(float Current, float Max, float TimeSinceSpend, float DeltaSeconds, float RegenPerSec, float RegenDelay);

	/** Energía que pide una pieza en un tick; el multiplicador del asiento escala el gasto (acelerador en 0 = gratis). */
	GAMEJAMM_API float EnergyCost(float CostPerSec, float DriveScale, float DeltaSeconds);

	/** Items de loot "Energy.Small"/"Energy.Large" -> energía. 0 si no es un item de energía. */
	GAMEJAMM_API float PickupEnergy(FName ItemId);

	/** Regla de equipos (igual que UJamStatics::AreEnemies): hostiles si los equipos son distintos. */
	GAMEJAMM_API bool AreHostile(EJamTeam Source, EJamTeam Target);

	/** true si Point cae dentro del cono con vértice en Origin que mira hacia Dir. */
	GAMEJAMM_API bool IsInCone(const FVector& Origin, const FVector& Dir, const FVector& Point, float Range, float HalfAngleDeg);

	/** Índice del candidato vivo y hostil más cercano dentro de Range; INDEX_NONE si no hay. */
	GAMEJAMM_API int32 PickNearestTarget(const FVector& Origin, const TArray<FTargetCandidate>& Candidates, EJamTeam SourceTeam, float Range);

	/** Ángulo con signo (rad) de From a To girando alrededor de Axis (regla de omega x r). 0 si alguno es paralelo al eje. */
	GAMEJAMM_API float SignedAngleAroundAxis(const FVector& From, const FVector& To, const FVector& Axis);

	/** Velocidad angular objetivo (rad/s) de un apuntador proporcional, saturada. */
	GAMEJAMM_API float AimRate(float AngleError, float Kp, float MaxRate);

	/** Aceleración angular (rad/s^2) de un control P de velocidad; la ganancia se limita para no oscilar a pocos fps. */
	GAMEJAMM_API float SpinAccel(float TargetOmega, float CurrentOmega, float Gain, float MaxAccel, float DeltaSeconds);

	/** Diferencial: la rueda del lado hacia donde se gira va más lenta. Side: -1 izquierda, +1 derecha. */
	GAMEJAMM_API float WheelScale(float Throttle, float Steer, float Side);

	/** Fan de sustentación: 0.5…1.5 según Lift. Fan de empuje: Throttle (-1…+1). */
	GAMEJAMM_API float FanScale(bool bLiftFan, float Throttle, float Lift);

	/** true si el empuje apunta (casi) hacia arriba de la construcción: es un Fan en la cara Bottom. */
	GAMEJAMM_API bool IsLiftDirection(const FVector& ThrustDir, const FVector& BuildUp);

	/** "Arriba" deseado: vertical, inclinado hacia ForwardFlat según el acelerador (hover que avanza). */
	GAMEJAMM_API FVector TiltedUp(const FVector& ForwardFlat, float Throttle, float MaxTiltDeg);

	/** Aceleración angular PD que lleva Up hacia DesiredUp y amortigua el giro que no es de yaw. */
	GAMEJAMM_API FVector LevelAccel(const FVector& Up, const FVector& DesiredUp, const FVector& AngularVelocity, float Kp, float Kd);

	/** Acumula tiempo; devuelve true (y reinicia) cada Interval segundos. */
	GAMEJAMM_API bool CooldownReady(float& Timer, float DeltaSeconds, float Interval);

	/** Piezas que el asiento enciende al montarse (todo lo conectado menos estructura, asiento, batería y bomba). */
	GAMEJAMM_API bool SeatActivates(EJamPartKind Kind);

	/** Fin del estado "lento": se renueva, nunca se acorta. */
	GAMEJAMM_API float RefreshSlowEnd(float CurrentEnd, float Now, float Duration);

	/** Jam.Debug: dibujar conos, ejes de empuje, torque y objetivos (definido en JamPocDevices.cpp). */
	GAMEJAMM_API bool IsDebugDrawEnabled();
}
