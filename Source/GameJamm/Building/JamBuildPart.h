#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "JamBuildPart.generated.h"

class UStaticMeshComponent;
class UPhysicsConstraintComponent;

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
	Flame
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

	virtual void Tick(float DeltaSeconds) override;

private:
	float Power = 0.f;
	float BurnLeft = 3.f;
	float FireTimer = 0.f;
	float Length = 50.f;
};
