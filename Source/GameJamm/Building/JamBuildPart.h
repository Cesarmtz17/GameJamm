#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Core/JamTypes.h"
#include "JamBuildPart.generated.h"

class UStaticMeshComponent;
class UPhysicsConstraintComponent;
class UAudioComponent;
class USoundBase;

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
	// --- capacidades nuevas (docs/building/README-DECISION.md) ---
	Hover,      // mantiene altura sobre el suelo
	Stabilizer, // endereza la construccion
	Shock,      // descarga: aturde y apaga construcciones ajenas
	Hydrant,    // chorro: empuja y apaga fuego
	Light,      // foco
	Head,       // gira sola hacia el objetivo mas cercano; lo montado apunta con ella
	Turntable,  // gira lo montado a velocidad constante
	Hinge,      // union con giro libre limitado
	Stake,      // ancla la construccion al mundo
	Slide,      // base de friccion baja (trineo)
	Spike       // puas: daña por contacto (embestida)
};

class AJamBuildPart;

/** Estado compartido por todas las piezas de una construccion (energia, masa, miembros). */
struct FJamBuildState
{
	TArray<TWeakObjectPtr<AJamBuildPart>> Parts;
	float Energy = 0.f;
	float MassKg = 0.f;
	EJamTeam Team = EJamTeam::Neutral;                      // equipo del dueño; la cabeza no apunta a los suyos
	float StunUntil = 0.f;                                  // descarga electrica: apagada hasta esta hora
	TArray<TWeakObjectPtr<AJamBuildPart>> Stunned;          // piezas que estaban encendidas y hay que reencender
	FVector PilotUp = FVector::UpVector;                    // objetivo de inclinacion del estabilizador al pilotar un volador
	float PilotTiltUntil = 0.f;                             // al soltar el mando vuelve a enderezar
	float LastPowerSound = -100.f;
	TWeakObjectPtr<AActor> Owner;                           // quien la pidio: la cabeza nunca le apunta
	TWeakObjectPtr<AActor> AimTarget;                       // a quien apunta la cabeza: el cañon eleva el tiro hacia el
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

	/** Fuerza (kg*cm/s^2) para Fan/Rocket/Balloon, velocidad angular (rad/s) para Wheel. */
	UPROPERTY(BlueprintReadOnly, Category = "Build")
	float Power = 0.f;

	/** Eje del mesh nativo (que mira a +Z) que se alinea: 0 = Z, 1 = eje X del vehículo, 2 = eje Y. */
	UPROPERTY(BlueprintReadOnly, Category = "Build")
	int32 Axis = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Build")
	FLinearColor Color = FLinearColor::White;

	/** Energia que gasta por segundo encendida (BuildKnowledge.json: energy_cost_per_s). */
	UPROPERTY(BlueprintReadOnly, Category = "Build")
	float EnergyPerSecond = 0.f;

	/** Placa: Size = (grosor, ancho, alto) y el grosor se alinea con la cara donde se pega. */
	UPROPERTY(BlueprintReadOnly, Category = "Build")
	bool bFaceAligned = false;
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

	/** Soldada a su padre: comparte cuerpo fisico con el (ver UJamAssembler, jam.Build.Weld). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Build")
	bool bWelded = false;

	/** Pieza a la que va pegada (la asigna el ensamblador): el resorte lanza a sus hijas. */
	TWeakObjectPtr<AJamBuildPart> ParentPart;

	/** Motor del eje (solo ruedas), lo asigna el ensamblador. */
	UPROPERTY()
	TObjectPtr<UPhysicsConstraintComponent> DriveConstraint;

	/** Dirección de empuje/disparo en espacio local de la pieza. */
	UPROPERTY()
	FVector LocalDirection = FVector::ForwardVector;

	/** Cuerpos ajenos que esta pieza ha afectado (quemado, congelado, empujado, electrocutado...). Para pruebas y UI. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Build")
	int32 EffectHits = 0;

	/** Malla real de la pieza (Content/Game/Building/Parts), o null si aun no esta importada: se usa la forma basica. */
	static UStaticMesh* FindRealMesh(FName Type);

	/** jam.Build.Energy: 0 = sin energia, 1 = reserva por construccion, 2 = barra del jugador (por defecto). */
	static int32 EnergyMode();

	/** Barra de energia del jugador (tipo mana, GDD 5): 0..150; por debajo de 50 se recarga sola +3/s sin gastar. */
	static float GetPlayerEnergy(const UWorld* World);
	static void SetPlayerEnergy(const UWorld* World, float Value);

	/** Texto corto del estado de la pieza para el HUD ("encendido", "explota en 3 s", "apagado"...). */
	FString GetStatusText() const;

	/** Energia por segundo que esta gastando ahora (0 si esta apagada). */
	float GetEnergyDrain() const { return bActive ? EnergyPerSecond : 0.f; }

	/** Estado compartido de la construccion; lo asigna el ensamblador. */
	TSharedPtr<FJamBuildState> Build;

	/**
	 * BoxSize = caja ya orientada segun la cara (ver UJamAssembler). El cuerpo fisico es siempre la forma basica;
	 * si existe la malla de arte se muestra encima (solo visual) con la rotacion VisualWorldRot.
	 */
	void InitFromDef(const FJamPartDef& Def, const FVector& BoxSize, const FQuat& VisualWorldRot);

	UFUNCTION(BlueprintCallable, Category = "Jam|Building")
	void SetActive(bool bNewActive);

	/** Conduccion: escala -1..1 de velocidad de ruedas, o 0..1 de empuje de helices. */
	UFUNCTION(BlueprintCallable, Category = "Jam|Building")
	void SetDrive(float Scale);

	/** Centro de masa fisico del conjunto, contando una sola vez cada cuerpo soldado. */
	FVector GetBuildCenterOfMass() const;

	virtual void Tick(float DeltaSeconds) override;

private:
	/** Daño por contacto: las puas siempre; el resto de la estructura solo al embestir rapido. */
	UFUNCTION()
	void OnPartHit(UPrimitiveComponent* HitComp, AActor* Other, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	float NextHitTime = 0.f;

	bool IsMine(const AActor* Other) const;
	/** Componentes fisicos/peones dentro de una esfera (sin esta pieza). */
	void Nearby(const FVector& Center, float Radius, TArray<UPrimitiveComponent*>& Out, const FVector* SightFrom = nullptr) const;
	AActor* SpawnBall(const FVector& At, const FVector& Velocity);
	/** Explosion: empuja, daña y enciende dispositivos apagados dentro del radio. */
	void Explode(const FVector& At, float Radius, float Speed, float Damage);
	void PlaySfx(const TCHAR* Name, float Volume = 1.f) const;
	/** Efecto visual barato: esfera de color que desaparece sola (hasta que haya Niagara). */
	void Puff(const FVector& At, float SizeCm, const FLinearColor& Color, float Life = 0.25f);

	float Power = 0.f;
	float DriveScale = 1.f; // acelerador de helices; sin piloto conserva empuje completo
	float EnergyPerSecond = 0.f;
	float BurnLeft = 3.f;
	float ReadyAt = 0.f;       // cohete: hora a la que se puede volver a encender (recarga)
	TArray<FVector> BallVel;   // cañon: velocidad de cada bola en el cuadro anterior (para detectar el impacto)
	float FireTimer = 0.f;
	float Length = 50.f;
	float HeatedUntil = 0.f;   // globo calentado por fuego cercano
	float DebugTimer = 0.f;
	float FxTimer = 0.f;
	float HoldZ = 0.f;         // altura objetivo de la piedra flotante sin suelo debajo
	TWeakObjectPtr<AActor> Target;                                  // cabeza
	TArray<TWeakObjectPtr<AActor>> Balls;                           // cañon: bolas en vuelo (explotan al impactar)
	TMap<TWeakObjectPtr<UPrimitiveComponent>, float> Frozen;        // hielo: componente -> hora de descongelar

	UPROPERTY()
	TObjectPtr<UPhysicsConstraintComponent> WorldLock;              // estaca
	UPROPERTY()
	TObjectPtr<USceneComponent> LightComp;                          // foco
	UPROPERTY()
	TObjectPtr<UAudioComponent> LoopSound;                          // motor / fuego mientras esta encendida
};
