#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "JamFigureAnim.generated.h"

/** Movimiento de arma hecho por codigo encima de la animacion suelta (no hay clip de tajo ni de tensar el arco). */
enum class EJamFigureMove : uint8
{
	None,
	Slash, // espadazo en diagonal con la mano del arma
	Shot   // tensar y soltar el arco: brazo del arco al frente, la otra mano jala la cuerda
};

/** Lo que el hilo de juego le pasa al de animacion cada frame. */
struct FJamFigureState
{
	bool bRig = false;            // hay arma en mano (UJamCharacterLook::Apply llamo SetRig)
	bool bBow = false;            // arma actual: arco (si no, espada)
	bool bCarry = true;           // espada: punta hacia afuera al caminar/correr para no atravesar el cuerpo
	FName SwordBone, BowBone, DrawBone, SpineBone;
	FQuat SwordInBone = FQuat::Identity; // rotacion de la espada respecto a su hueso (eje X = hoja)
	FQuat BowInBone = FQuat::Identity;   // rotacion del arco respecto a su hueso (eje Z = largo del arco)
	FQuat MeshQuat = FQuat::Identity;    // rotacion relativa de la malla en el actor (Yaw de Characters.json)
	EJamFigureMove Move = EJamFigureMove::None;
	float T = 1.f;     // avance del movimiento 0..1
	float Pitch = 0.f; // mira vertical en grados (arco)

	// Arma de mano pedida (GDD 5.1): va en GunBone; GunInBone = su rotacion en ese hueso (X = canon, Z = arriba).
	bool bGun = false;
	FName GunBone;
	FQuat GunInBone = FQuat::Identity;
	float GunAim = 0.f;  // 0 = arma baja al frente; 1 = brazo extendido a la mira (acaba de disparar)
	float GunSide = 1.f; // 1 = mano derecha, -1 = izquierda (espeja la pose)

	// Cuerpo natural (todas las figuras): cruce entre clips, inclinacion al correr/girar y cabeza hacia la mira.
	bool bNatural = false;
	FName LowSpineBone, NeckBone, HeadBone;
	uint32 ClipSerial = 0;  // cambia cada vez que cambia el clip (Idle/Walk/Run/Hit...): arranca un cruce
	float FadeTime = 0.2f;
	float BodyLean = 0.f;   // grados al frente (aceleracion/velocidad)
	float BodyRoll = 0.f;   // grados hacia el lado de la curva
	float LookYaw = 0.f;    // cabeza: grados respecto al cuerpo
	float LookPitch = 0.f;
};

/**
 * Instancia de animacion de la figura rigeada: la misma animacion suelta de siempre (Idle/Walk/Run... de Characters.json)
 * y encima, en C++, gira hombro, codo y mano para el espadazo, el tiro con arco y la espada "cargada" hacia afuera.
 * No necesita Anim Blueprint ni clips nuevos; los giros se apuntan por direccion (actor: X frente, Y derecha, Z arriba),
 * asi que sirve para cualquier rig con cadena hombro > codo > mano.
 */
UCLASS(transient, NotBlueprintable)
class GAMEJAMM_API UJamFigureAnim : public UAnimSingleNodeInstance
{
	GENERATED_BODY()

public:
	/** Huesos y rotaciones del arma en mano (las mismas que usa UJamWeaponComponent::AttachToHand). */
	void SetRig(FName SwordBone, const FQuat& SwordInBone, FName BowBone, const FQuat& BowInBone);

	/** Arranca un movimiento de arma (reinicia si ya habia uno). */
	void StartMove(EJamFigureMove Move, float Duration);

	/** Sentado o muerto: sin giros por codigo (solo la animacion). */
	void SetMovesEnabled(bool bEnabled) { bMovesEnabled = bEnabled; }

	bool IsMoving() const { return Move != EJamFigureMove::None; }

	const FJamFigureState& GetFigureState() const { return State; }

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;

private:
	FJamFigureState State;
	FName SwordBone, BowBone, DrawBone, SpineBone;
	FQuat SwordInBone = FQuat::Identity;
	FQuat BowInBone = FQuat::Identity;
	bool bHasRig = false;
	bool bMovesEnabled = true;
	EJamFigureMove Move = EJamFigureMove::None;
	float Elapsed = 0.f;
	float Duration = 0.5f;

	void ResolveBodyBones();
	bool bBodyBones = false;
	FName LowSpineBone, NeckBone, HeadBone;
	TWeakObjectPtr<const UAnimationAsset> LastAsset;
	uint32 ClipSerial = 0;
	FVector PrevVel = FVector::ZeroVector;
	float PrevYaw = 0.f;
	bool bHasPrev = false;
	float BodyLean = 0.f, BodyRoll = 0.f, LookYaw = 0.f, LookPitch = 0.f;
	float GunAim = 0.f; // arma pedida: 0..1 suavizado hacia "apuntando"
};
