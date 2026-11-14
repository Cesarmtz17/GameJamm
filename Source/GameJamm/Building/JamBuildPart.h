#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "JamBuildPart.generated.h"

class UStaticMeshComponent;
class UPhysicsConstraintComponent;
class UJamEnergyComponent;

UENUM(BlueprintType)
enum class EJamPartKind : uint8
{
	Structural,
	Wheel,
	Fan,
	Rocket,
	Balloon,
	Cannon,
	Seat,
	Battery,
	Flame,
	Wing,
	Frost,
	Beam,
	Bomb,
	Spring,
	Sail,
	Turntable,
	Head,
	Stake
};

/** Definición de un tipo de pieza (POC: tabla en C++, después DataAsset). */
USTRUCT(BlueprintType)
struct GAMEJAMM_API FJamPartDef
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Build")
	FName Type;

	UPROPERTY(BlueprintReadOnly, Category = "Build")
	EJamPartKind Kind = EJamPartKind::Structural;

	/** Mesh placeholder (BasicShapes) hasta que lleguen los assets de arte. */
	UPROPERTY(BlueprintReadOnly, Category = "Build")
	FString MeshPath;

	/** Tamaño en el marco del vehículo (cm): X adelante, Y lado, Z arriba. */
	UPROPERTY(BlueprintReadOnly, Category = "Build")
	FVector Size = FVector(100.0);

	UPROPERTY(BlueprintReadOnly, Category = "Build")
	float MassKg = 10.f;

	/** Fuerza (kg*cm/s^2) para Fan/Rocket/Balloon, velocidad angular (rad/s) para Wheel, reserva de energía para Battery. */
	UPROPERTY(BlueprintReadOnly, Category = "Build")
	float Power = 0.f;

	/** Energía por segundo mientras está encendida (espejo de energy_cost_per_s en BuildKnowledge.json). */
	UPROPERTY(BlueprintReadOnly, Category = "Build")
	float EnergyPerSec = 0.f;

	/** Eje del mesh nativo (que mira a +Z) que se alinea: 0 = Z, 1 = eje X del vehículo, 2 = eje Y. */
	UPROPERTY(BlueprintReadOnly, Category = "Build")
	int32 Axis = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Build")
	FLinearColor Color = FLinearColor::White;
};

UCLASS()
class GAMEJAMM_API UJamPartCatalog : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Jam|Building")
	static bool Find(FName Type, FJamPartDef& OutDef);

	/** Tipos que el asistente IA puede usar en una receta. */
	UFUNCTION(BlueprintPure, Category = "Jam|Building")
	static TArray<FName> GetTypes();
};

/** Una pieza física de una construcción. */
UCLASS()
class GAMEJAMM_API AJamBuildPart : public AActor
{
	GENERATED_BODY()

public:
	AJamBuildPart();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Build")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Id de la pieza dentro de la receta ("p0"...). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Build")
	FName RecipeId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Build")
	FName PartType;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Build")
	EJamPartKind Kind = EJamPartKind::Structural;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Build")
	bool bActive = false;

	/** Motor del eje (solo ruedas), lo asigna el ensamblador. */
	UPROPERTY()
	TObjectPtr<UPhysicsConstraintComponent> DriveConstraint;

	/** Dirección de empuje/disparo en espacio local de la pieza. */
	UPROPERTY()
	FVector LocalDirection = FVector::ForwardVector;

	void InitFromDef(const FJamPartDef& Def);

	UFUNCTION(BlueprintCallable, Category = "Jam|Building")
	void SetActive(bool bNewActive);

	/** Multiplicador de potencia que fija el asiento (acelerador/diferencial/sustentación). 1 = sin asiento. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Build")
	float DriveScale = 1.f;

	/** PoC (Jam.Seat): mientras está montado, lee WASD/flechas/Space/Ctrl del primer PlayerController. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	bool bPocKeyboardInput = false;

	UFUNCTION(BlueprintCallable, Category = "Jam|Building")
	void SetDriveScale(float NewScale);

	// ---- Construcción conectada (se deduce de las constraints que crea UJamAssembler) ----

	/** Pieza a la que está pegada; nullptr si es la raíz. */
	UFUNCTION(BlueprintCallable, Category = "Jam|Building")
	AJamBuildPart* GetParentPart();

	UFUNCTION(BlueprintCallable, Category = "Jam|Building")
	AJamBuildPart* GetBuildRoot();

	/** Todas las piezas conectadas a esta (incluida ella). */
	UFUNCTION(BlueprintCallable, Category = "Jam|Building")
	TArray<AJamBuildPart*> GetBuildParts();

	/** Dueño de toda la construcción: define el equipo (UJamStatics::GetActorTeam) y de quién es la energía. */
	UFUNCTION(BlueprintCallable, Category = "Jam|Building")
	void SetBuildOwner(AActor* NewOwner);

	/** Una tecla prende o apaga toda la construcción conectada. */
	UFUNCTION(BlueprintCallable, Category = "Jam|Building")
	void SetBuildPower(bool bOn);

	/** Olvida la construcción cacheada (llamar si se agregan o quitan piezas/constraints a mano). */
	UFUNCTION(BlueprintCallable, Category = "Jam|Building")
	void InvalidateBuild();

	// ---- Asiento (research 13 §5). Pensado para la pieza Seat; funciona en cualquier pieza de la construcción. ----

	/** Sube a Rider: lo vuelve dueño de la construcción, enciende todo lo conectado y deja el control en 0. */
	UFUNCTION(BlueprintCallable, Category = "Jam|Building|Seat")
	bool Mount(AActor* Rider, bool bAttachRider = true);

	UFUNCTION(BlueprintCallable, Category = "Jam|Building|Seat")
	void Dismount(bool bKeepRunning = false);

	/** W/S: -1…+1. Multiplica Fan de empuje y ruedas. */
	UFUNCTION(BlueprintCallable, Category = "Jam|Building|Seat")
	void SetThrottle(float Value);

	/** A/D: -1 (izquierda)…+1 (derecha). Torque de yaw en la raíz + diferencial en ruedas. */
	UFUNCTION(BlueprintCallable, Category = "Jam|Building|Seat")
	void SetSteer(float Value);

	/** Space/Ctrl: -1…+1. Multiplica solo los Fan de sustentación (cara Bottom). */
	UFUNCTION(BlueprintCallable, Category = "Jam|Building|Seat")
	void SetLift(float Value);

	UFUNCTION(BlueprintPure, Category = "Jam|Building|Seat")
	bool IsMounted() const { return bMounted; }

	UFUNCTION(BlueprintPure, Category = "Jam|Building|Seat")
	AActor* GetRider() const { return SeatRider.Get(); }

	/** Asiento más cercano a Location (o, si bAnyPart, cualquier pieza) dentro de MaxDistance. */
	UFUNCTION(BlueprintCallable, Category = "Jam|Building|Seat", meta = (WorldContext = "WorldContextObject"))
	static AJamBuildPart* FindNearestSeat(const UObject* WorldContextObject, FVector Location, float MaxDistance = 400.f, bool bAnyPart = false);

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ResolveBuild();
	UPhysicsConstraintComponent* FindParentJoint(AJamBuildPart*& OutParent) const;
	/** Orientación del marco del vehículo (X adelante, Z arriba) a partir de la pose actual de esta pieza. */
	FQuat GetBuildQuat() const;
	/** Eje de la cara donde va pegada (hacia afuera), en mundo; para la raíz, el "arriba" de la construcción. */
	FVector GetFaceAxisWorld() const;
	void GetChildParts(TArray<AJamBuildPart*>& OutChildren) const;
	void CollectSubtree(TArray<AJamBuildPart*>& OutParts);
	void FixWeaponDirection();
	void UpdateDriveTarget();
	UJamEnergyComponent* GetEnergy();
	bool ConsumeEnergy(float DeltaSeconds);
	void TryBindBattery();
	void ApplyCone(const FVector& Start, const FVector& Dir, float Range, float HalfAngleDeg, float Damage, bool bFrost);
	void FireSpring();
	void SetupRotorJoints();
	void DriveRotor(float TargetRate, float DeltaSeconds);
	AActor* FindAimTarget() const;
	void SetStaked(bool bStaked);
	void TickSeat(float DeltaSeconds);
	void ReleaseRider();

	float Power = 0.f;
	float BurnLeft = 3.f;
	float FireTimer = 0.f;
	float Length = 50.f;

	float EnergyPerSec = 0.f;
	int32 DefAxis = 0;
	float DamageTimer = 0.f;
	float HeatLeft = 0.f;
	bool bRocketSpent = false;

	// Construcción conectada (cache).
	bool bBuildResolved = false;
	bool bDirectionFixed = false;
	TWeakObjectPtr<AJamBuildPart> ParentPart;
	TWeakObjectPtr<AJamBuildPart> BuildRoot;
	TArray<TWeakObjectPtr<AJamBuildPart>> BuildMembers;

	// Energía.
	TWeakObjectPtr<AActor> EnergyOwner;
	TWeakObjectPtr<UJamEnergyComponent> EnergyComp;
	TWeakObjectPtr<UJamEnergyComponent> BatteryComp;
	bool bBatteryGiven = false;

	// Turntable / Head.
	bool bRotorReady = false;
	TArray<TWeakObjectPtr<AJamBuildPart>> RotorChildren;
	TWeakObjectPtr<AJamBuildPart> AimRefPart;
	FVector AimRefLocalDir = FVector::ForwardVector;
	TWeakObjectPtr<AActor> AimTarget;

	// Stake.
	UPROPERTY()
	TObjectPtr<UPhysicsConstraintComponent> StakeJoint;

	// Asiento.
	bool bMounted = false;
	bool bRiderAttached = false;
	bool bSeatLevel = false;
	bool bSeatHasLiftFans = false;
	float SeatThrottle = 0.f;
	float SeatSteer = 0.f;
	float SeatLift = 0.f;
	float BuildInertia = 0.f;
	float BuildMass = 0.f;
	TWeakObjectPtr<AActor> SeatRider;
};
