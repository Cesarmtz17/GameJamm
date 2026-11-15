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
	FName SwordBone, BowBone, DrawBone;
	FQuat SwordInBone = FQuat::Identity; // rotacion de la espada respecto a su hueso (eje X = hoja)
	FQuat BowInBone = FQuat::Identity;   // rotacion del arco respecto a su hueso (eje Z = largo del arco)
	FQuat MeshQuat = FQuat::Identity;    // rotacion relativa de la malla en el actor (Yaw de Characters.json)
	EJamFigureMove Move = EJamFigureMove::None;
	float T = 1.f;     // avance del movimiento 0..1
	float Pitch = 0.f; // mira vertical en grados (arco)
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
	FName SwordBone, BowBone, DrawBone;
	FQuat SwordInBone = FQuat::Identity;
	FQuat BowInBone = FQuat::Identity;
	bool bHasRig = false;
	bool bMovesEnabled = true;
	EJamFigureMove Move = EJamFigureMove::None;
	float Elapsed = 0.f;
	float Duration = 0.5f;
};
