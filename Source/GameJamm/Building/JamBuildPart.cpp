#include "Building/JamBuildPart.h"

#include "Building/JamDeviceMath.h"
#include "Building/JamEnergyComponent.h"
#include "Building/JamPocDevices.h"
#include "Building/JamProjectile.h"
#include "Building/JamSlowComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HealthComponent.h"
#include "Core/JamStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"

namespace
{
	const TCHAR* CubeMesh = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* CylMesh = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	const TCHAR* SphereMesh = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	const TCHAR* ConeMesh = TEXT("/Engine/BasicShapes/Cone.Cone");

	/** Energía por segundo mientras la pieza está encendida. Espejo de energy_cost_per_s en BuildKnowledge.json
	 *  (devices_proto.py comprueba que esta tabla y el JSON digan lo mismo). Lo que no aparece cuesta 0. */
	float EnergyCostFor(FName Type)
	{
		struct FCost { const TCHAR* Type; float PerSec; };
		static const FCost Costs[] = {
			{ TEXT("Wheel"), 0.2f }, { TEXT("BigWheel"), 0.2f },
			{ TEXT("Fan"), 1.5f }, { TEXT("Jet"), 2.5f }, { TEXT("Turbine"), 2.5f },
			{ TEXT("HoverStone"), 0.5f },
			{ TEXT("Flamethrower"), 2.0f }, { TEXT("Frost"), 2.0f }, { TEXT("Beam"), 2.0f },
			{ TEXT("Cannon"), 3.0f },
			{ TEXT("Turntable"), 0.3f }, { TEXT("Head"), 0.3f },
		};
		for (const FCost& C : Costs)
		{
			if (Type == FName(C.Type))
			{
				return C.PerSec;
			}
		}
		return 0.f;
	}

	FJamPartDef Make(FName Type, EJamPartKind Kind, const TCHAR* Mesh, FVector Size, float Mass, float Power, int32 Axis, FLinearColor Color)
	{
		FJamPartDef D;
		D.Type = Type;
		D.Kind = Kind;
		D.MeshPath = Mesh;
		D.Size = Size;
		D.MassKg = Mass;
		D.Power = Power;
		D.EnergyPerSec = EnergyCostFor(Type);
		D.Axis = Axis;
		D.Color = Color;
		return D;
	}

	// ponytail: tabla en código para el POC; mover a DataAssets (editables por diseño) cuando lleguen los assets.
	const TArray<FJamPartDef>& Table()
	{
		static const TArray<FJamPartDef> T = {
			Make("Plank", EJamPartKind::Structural, CubeMesh, FVector(200, 60, 10), 5.f, 0.f, 0, FLinearColor(0.55f, 0.38f, 0.2f)),
			Make("Block", EJamPartKind::Structural, CubeMesh, FVector(60, 60, 60), 15.f, 0.f, 0, FLinearColor(0.5f, 0.5f, 0.55f)),
			Make("Cylinder", EJamPartKind::Structural, CylMesh, FVector(150, 50, 50), 10.f, 0.f, 1, FLinearColor(0.6f, 0.6f, 0.65f)),
			Make("Wheel", EJamPartKind::Wheel, CylMesh, FVector(50, 20, 50), 4.f, 10.f, 2, FLinearColor(0.1f, 0.1f, 0.1f)),
			Make("BigWheel", EJamPartKind::Wheel, CylMesh, FVector(80, 30, 80), 8.f, 8.f, 2, FLinearColor(0.05f, 0.05f, 0.05f)),
			Make("Fan", EJamPartKind::Fan, CylMesh, FVector(15, 50, 50), 3.f, 25000.f, 1, FLinearColor(0.2f, 0.6f, 0.9f)),
			Make("Rocket", EJamPartKind::Rocket, ConeMesh, FVector(60, 20, 20), 4.f, 80000.f, 1, FLinearColor(0.9f, 0.2f, 0.1f)),
			Make("Balloon", EJamPartKind::Balloon, SphereMesh, FVector(90, 90, 90), 1.f, 12000.f, 0, FLinearColor(0.9f, 0.8f, 0.2f)),
			Make("Cannon", EJamPartKind::Cannon, CylMesh, FVector(70, 20, 20), 8.f, 0.f, 1, FLinearColor(0.25f, 0.25f, 0.3f)),
			Make("Seat", EJamPartKind::Seat, CubeMesh, FVector(40, 40, 30), 5.f, 0.f, 0, FLinearColor(0.2f, 0.7f, 0.3f)),
			Make("Battery", EJamPartKind::Battery, CubeMesh, FVector(20, 20, 20), 3.f, 30.f, 0, FLinearColor(0.9f, 0.9f, 0.1f)),
			Make("Flamethrower", EJamPartKind::Flame, ConeMesh, FVector(40, 20, 20), 3.f, 0.f, 1, FLinearColor(1.f, 0.45f, 0.f)),
			// --- ampliacion: elementos de construccion por piezas (ver docs/assets/CATALOGO-ELEMENTOS.md) ---
			Make("Wall", EJamPartKind::Structural, CubeMesh, FVector(200, 30, 150), 30.f, 0.f, 0, FLinearColor(0.45f, 0.45f, 0.5f)),
			Make("Panel", EJamPartKind::Structural, CubeMesh, FVector(100, 10, 100), 6.f, 0.f, 0, FLinearColor(0.7f, 0.75f, 0.8f)),
			Make("Jet", EJamPartKind::Fan, CylMesh, FVector(60, 40, 40), 6.f, 90000.f, 1, FLinearColor(0.9f, 0.5f, 0.15f)),
			Make("Turbine", EJamPartKind::Fan, CylMesh, FVector(25, 90, 90), 6.f, 60000.f, 1, FLinearColor(0.3f, 0.8f, 0.7f)),
			Make("HoverStone", EJamPartKind::Balloon, SphereMesh, FVector(45, 45, 45), 2.f, 18000.f, 0, FLinearColor(0.6f, 0.4f, 0.9f)),
			Make("Wing", EJamPartKind::Wing, CubeMesh, FVector(80, 160, 6), 3.f, 45.f, 0, FLinearColor(0.85f, 0.85f, 0.9f)),
			Make("Frost", EJamPartKind::Frost, ConeMesh, FVector(40, 25, 25), 3.f, 0.f, 1, FLinearColor(0.5f, 0.9f, 1.f)),
			Make("Beam", EJamPartKind::Beam, CylMesh, FVector(50, 12, 12), 3.f, 400000.f, 1, FLinearColor(1.f, 0.1f, 0.4f)),
			Make("Bomb", EJamPartKind::Bomb, SphereMesh, FVector(30, 30, 30), 2.f, 2500.f, 0, FLinearColor(0.1f, 0.1f, 0.1f)),
			Make("Spring", EJamPartKind::Spring, CylMesh, FVector(30, 30, 40), 2.f, 700.f, 0, FLinearColor(0.2f, 0.9f, 0.2f)),
			Make("Sail", EJamPartKind::Sail, CubeMesh, FVector(10, 120, 100), 2.f, 6000.f, 0, FLinearColor(0.95f, 0.95f, 0.85f)),
			// --- comportamiento por cara (research 13 §2): Power de Battery = reserva de energía; de Turntable = rad/s ---
			Make("Turntable", EJamPartKind::Turntable, CylMesh, FVector(60, 60, 15), 4.f, 2.f, 0, FLinearColor(0.8f, 0.7f, 0.2f)),
			Make("Head", EJamPartKind::Head, SphereMesh, FVector(35, 35, 35), 3.f, 0.f, 0, FLinearColor(0.9f, 0.3f, 0.9f)),
			Make("Stake", EJamPartKind::Stake, ConeMesh, FVector(20, 20, 50), 2.f, 0.f, 0, FLinearColor(0.4f, 0.3f, 0.2f)),
		};
		return T;
	}
}

namespace
{
	/** IDs del catálogo de assets y sinónimos comunes -> tipo de receta (docs/assets/CATALOGO-ELEMENTOS.md §0.1). */
	struct FTypeAlias { const TCHAR* From; const TCHAR* To; };
	const FTypeAlias TypeAliases[] = {
		{ TEXT("PART_PLANK"), TEXT("Plank") }, { TEXT("Board"), TEXT("Plank") }, { TEXT("Tabla"), TEXT("Plank") },
		{ TEXT("PART_BLOCK"), TEXT("Block") }, { TEXT("Cube"), TEXT("Block") }, { TEXT("Bloque"), TEXT("Block") },
		{ TEXT("PART_CYLINDER"), TEXT("Cylinder") }, { TEXT("Pole"), TEXT("Cylinder") },
		{ TEXT("DEV_WHEEL_S"), TEXT("Wheel") }, { TEXT("SmallWheel"), TEXT("Wheel") }, { TEXT("Llanta"), TEXT("Wheel") }, { TEXT("Rueda"), TEXT("Wheel") },
		{ TEXT("DEV_WHEEL_L"), TEXT("BigWheel") }, { TEXT("LargeWheel"), TEXT("BigWheel") },
		{ TEXT("DEV_FAN"), TEXT("Fan") }, { TEXT("Propeller"), TEXT("Fan") }, { TEXT("Ventilador"), TEXT("Fan") },
		{ TEXT("DEV_ROCKET"), TEXT("Rocket") }, { TEXT("Thruster"), TEXT("Rocket") }, { TEXT("Cohete"), TEXT("Rocket") },
		{ TEXT("DEV_BALLOON"), TEXT("Balloon") }, { TEXT("Globo"), TEXT("Balloon") },
		{ TEXT("DEV_CANNON"), TEXT("Cannon") }, { TEXT("Gun"), TEXT("Cannon") }, { TEXT("Canon"), TEXT("Cannon") },
		{ TEXT("CTRL_SEAT"), TEXT("Seat") }, { TEXT("Chair"), TEXT("Seat") }, { TEXT("Asiento"), TEXT("Seat") },
		{ TEXT("PWR_BATTERY_S"), TEXT("Battery") }, { TEXT("Bateria"), TEXT("Battery") },
		{ TEXT("DEV_FLAME"), TEXT("Flamethrower") }, { TEXT("Flame"), TEXT("Flamethrower") }, { TEXT("Lanzallamas"), TEXT("Flamethrower") },
		{ TEXT("PART_WALL"), TEXT("Wall") }, { TEXT("Muro"), TEXT("Wall") }, { TEXT("PART_PANEL"), TEXT("Panel") },
		{ TEXT("DEV_WING"), TEXT("Wing") }, { TEXT("Ala"), TEXT("Wing") }, { TEXT("DEV_FROST"), TEXT("Frost") }, { TEXT("Ice"), TEXT("Frost") }, { TEXT("Hielo"), TEXT("Frost") },
		{ TEXT("DEV_BEAM"), TEXT("Beam") }, { TEXT("Laser"), TEXT("Beam") }, { TEXT("DEV_HOVER"), TEXT("HoverStone") }, { TEXT("Hover"), TEXT("HoverStone") },
		{ TEXT("CTRL_TIMEBOMB"), TEXT("Bomb") }, { TEXT("TimeBomb"), TEXT("Bomb") }, { TEXT("Bomba"), TEXT("Bomb") },
		{ TEXT("DEV_SPRING"), TEXT("Spring") }, { TEXT("Resorte"), TEXT("Spring") }, { TEXT("Vela"), TEXT("Sail") }, { TEXT("Turbina"), TEXT("Turbine") },
		{ TEXT("JOINT_TURNTABLE"), TEXT("Turntable") }, { TEXT("Tornamesa"), TEXT("Turntable") },
		{ TEXT("CTRL_HEAD"), TEXT("Head") }, { TEXT("Cabeza"), TEXT("Head") },
		{ TEXT("ANCHOR_STAKE"), TEXT("Stake") }, { TEXT("Estaca"), TEXT("Stake") },
	};
}

bool UJamPartCatalog::Find(FName Type, FJamPartDef& OutDef)
{
	// Tolerante: "Part.Wheel" (id de inventario), "DEV_WHEEL_S" (id de asset), " wheel " -> Wheel.
	FString S = Type.ToString().TrimStartAndEnd();
	S.RemoveFromStart(TEXT("Part."));
	for (const FTypeAlias& A : TypeAliases)
	{
		if (S.Equals(A.From, ESearchCase::IgnoreCase))
		{
			S = A.To;
			break;
		}
	}
	const FName Key(*S);

	for (const FJamPartDef& D : Table())
	{
		if (D.Type == Key)
		{
			OutDef = D;
			return true;
		}
	}
	return false;
}

TArray<FName> UJamPartCatalog::GetTypes()
{
	TArray<FName> Out;
	for (const FJamPartDef& D : Table())
	{
		Out.Add(D.Type);
	}
	return Out;
}

namespace
{
	/** Rotación que UJamAssembler aplica al mesh según Def.Axis (el mesh nativo mira a +Z). */
	FQuat PartAxisQuat(int32 Axis)
	{
		if (Axis == 1) { return FQuat::FindBetweenNormals(FVector::UpVector, FVector::ForwardVector); }
		if (Axis == 2) { return FQuat::FindBetweenNormals(FVector::UpVector, FVector::RightVector); }
		return FQuat::Identity;
	}

	bool IsWeaponPartKind(EJamPartKind Kind)
	{
		return Kind == EJamPartKind::Cannon || Kind == EJamPartKind::Flame || Kind == EJamPartKind::Frost || Kind == EJamPartKind::Beam;
	}

	AController* PartControllerOf(const AActor* Owner)
	{
		const APawn* Pawn = Cast<APawn>(Owner);
		return Pawn ? Pawn->GetController() : nullptr;
	}
}

AJamBuildPart::AJamBuildPart()
{
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
}

void AJamBuildPart::InitFromDef(const FJamPartDef& Def)
{
	PartType = Def.Type;
	Kind = Def.Kind;
	Power = Def.Power;
	EnergyPerSec = Def.EnergyPerSec;
	DefAxis = Def.Axis;
	Length = FMath::Max(Def.Size.X, 1.0);

	UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, *Def.MeshPath);
	if (SM)
	{
		Mesh->SetStaticMesh(SM);
	}

	// Medida deseada en los ejes locales del mesh, según el eje del cilindro/cono.
	FVector Desired;
	switch (Def.Axis)
	{
	case 1: Desired = FVector(Def.Size.Y, Def.Size.Y, Def.Size.X); break; // eje largo = X del vehículo
	case 2: Desired = FVector(Def.Size.X, Def.Size.X, Def.Size.Y); break;  // eje = Y (ruedas)
	default: Desired = Def.Size; break;
	}
	// Se escala contra el bounding box real del mesh: los BasicShapes miden 100 cm (igual que antes) y los
	// meshes de arte encajan en su Size aunque no midan exacto. El layout usa Size, así que siempre cuadra.
	FVector Native(100.0);
	if (SM)
	{
		const FVector B = SM->GetBoundingBox().GetSize();
		Native = FVector(FMath::Max(B.X, 1.0), FMath::Max(B.Y, 1.0), FMath::Max(B.Z, 1.0));
	}
	Mesh->SetRelativeScale3D(Desired / Native);

	// BasicShapeMaterial no tiene parametro "Color"; DebugMeshMaterial si.
	UMaterialInterface* ColorMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/DebugMeshMaterial.DebugMeshMaterial"));
	if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(ColorMat ? ColorMat : Mesh->GetMaterial(0), this))
	{
		Mesh->SetMaterial(0, MID);
		MID->SetVectorParameterValue(TEXT("Color"), Def.Color);
	}

	Mesh->SetMassOverrideInKg(NAME_None, Def.MassKg, true);
	Mesh->SetSimulatePhysics(true);
	Mesh->SetEnableGravity(true);
}

// ---------------------------------------------------------------------------------------------
// Construcción conectada: se deduce de las constraints que crea UJamAssembler (hijo -> padre).
// ---------------------------------------------------------------------------------------------

UPhysicsConstraintComponent* AJamBuildPart::FindParentJoint(AJamBuildPart*& OutParent) const
{
	OutParent = nullptr;
	TInlineComponentArray<UPhysicsConstraintComponent*> Joints(this);
	for (UPhysicsConstraintComponent* Joint : Joints)
	{
		if (!Joint || Joint == StakeJoint)
		{
			continue;
		}
		// UJamAssembler llama SetConstrainedComponents(Padre->Mesh, ..., Hijo->Mesh, ...): el componente 1 es el padre.
		const UPrimitiveComponent* Other = Joint->OverrideComponent1.Get();
		AJamBuildPart* Parent = Other ? Cast<AJamBuildPart>(Other->GetOwner()) : nullptr;
		if (Parent && Parent != this)
		{
			OutParent = Parent;
			return Joint;
		}
	}
	return nullptr;
}

void AJamBuildPart::ResolveBuild()
{
	if (bBuildResolved)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	auto RootOf = [](AJamBuildPart* Part)
	{
		for (int32 i = 0; i < 64 && Part; ++i)
		{
			AJamBuildPart* Up = nullptr;
			Part->FindParentJoint(Up);
			if (!Up)
			{
				break;
			}
			Part = Up;
		}
		return Part;
	};

	AJamBuildPart* Root = RootOf(this);
	TArray<AJamBuildPart*> Members;
	for (TActorIterator<AJamBuildPart> It(World); It; ++It)
	{
		AJamBuildPart* Part = *It;
		if (IsValid(Part) && RootOf(Part) == Root)
		{
			Members.Add(Part);
		}
	}
	for (AJamBuildPart* Part : Members)
	{
		AJamBuildPart* Up = nullptr;
		Part->FindParentJoint(Up);
		Part->ParentPart = Up;
		Part->BuildRoot = Root;
		Part->BuildMembers.Reset();
		for (AJamBuildPart* Member : Members)
		{
			Part->BuildMembers.Add(Member);
		}
		Part->bBuildResolved = true;
	}
	for (AJamBuildPart* Part : Members)
	{
		Part->FixWeaponDirection();
	}
}

void AJamBuildPart::InvalidateBuild()
{
	TArray<TWeakObjectPtr<AJamBuildPart>> Old = BuildMembers;
	bBuildResolved = false;
	for (const TWeakObjectPtr<AJamBuildPart>& Member : Old)
	{
		if (AJamBuildPart* Part = Member.Get())
		{
			Part->bBuildResolved = false;
		}
	}
}

AJamBuildPart* AJamBuildPart::GetParentPart()
{
	ResolveBuild();
	return ParentPart.Get();
}

AJamBuildPart* AJamBuildPart::GetBuildRoot()
{
	ResolveBuild();
	AJamBuildPart* Root = BuildRoot.Get();
	return Root ? Root : this;
}

TArray<AJamBuildPart*> AJamBuildPart::GetBuildParts()
{
	ResolveBuild();
	TArray<AJamBuildPart*> Out;
	for (const TWeakObjectPtr<AJamBuildPart>& Member : BuildMembers)
	{
		if (AJamBuildPart* Part = Member.Get())
		{
			Out.Add(Part);
		}
	}
	if (Out.Num() == 0)
	{
		Out.Add(this);
	}
	return Out;
}

void AJamBuildPart::SetBuildOwner(AActor* NewOwner)
{
	for (AJamBuildPart* Part : GetBuildParts())
	{
		Part->SetOwner(NewOwner);
	}
}

void AJamBuildPart::SetBuildPower(bool bOn)
{
	for (AJamBuildPart* Part : GetBuildParts())
	{
		if (IsValid(Part) && JamDevices::SeatActivates(Part->Kind))
		{
			Part->SetActive(bOn);
		}
	}
}

FQuat AJamBuildPart::GetBuildQuat() const
{
	return GetActorQuat() * PartAxisQuat(DefAxis).Inverse();
}

FVector AJamBuildPart::GetFaceAxisWorld() const
{
	if (ParentPart.IsValid())
	{
		return GetActorQuat().RotateVector(LocalDirection).GetSafeNormal();
	}
	// Raíz: el ensamblador le pone LocalDirection = adelante; "lo de encima" está en el arriba de la construcción.
	return GetBuildQuat().GetUpVector();
}

void AJamBuildPart::GetChildParts(TArray<AJamBuildPart*>& OutChildren) const
{
	for (const TWeakObjectPtr<AJamBuildPart>& Member : BuildMembers)
	{
		AJamBuildPart* Part = Member.Get();
		if (Part && Part != this && Part->ParentPart.Get() == this)
		{
			OutChildren.Add(Part);
		}
	}
}

void AJamBuildPart::CollectSubtree(TArray<AJamBuildPart*>& OutParts)
{
	if (OutParts.Contains(this))
	{
		return;
	}
	OutParts.Add(this);
	TArray<AJamBuildPart*> Children;
	GetChildParts(Children);
	for (AJamBuildPart* Child : Children)
	{
		Child->CollectSubtree(OutParts);
	}
}

void AJamBuildPart::FixWeaponDirection()
{
	AJamBuildPart* Root = BuildRoot.Get();
	if (bDirectionFixed || !IsWeaponPartKind(Kind) || !ParentPart.IsValid() || !Root)
	{
		return;
	}
	bDirectionFixed = true;
	// Un arma en la cara Top no dispara al cielo: apunta hacia el frente de la construcción (su cañón ya
	// se dibuja así). Es lo que permite el tanque y "Head + arma encima = torreta". En Bottom sí dispara hacia abajo.
	const FQuat BuildQuat = Root->GetBuildQuat();
	const FVector WorldDir = GetActorQuat().RotateVector(LocalDirection);
	if (FVector::DotProduct(WorldDir, BuildQuat.GetUpVector()) > 0.9)
	{
		LocalDirection = GetActorQuat().UnrotateVector(BuildQuat.GetForwardVector());
	}
}

// ---------------------------------------------------------------------------------------------
// Encendido y energía
// ---------------------------------------------------------------------------------------------

void AJamBuildPart::UpdateDriveTarget()
{
	if (DriveConstraint)
	{
		DriveConstraint->SetAngularVelocityTarget(FVector(bActive ? Power * DriveScale : 0.f, 0.f, 0.f) / (2.0 * PI));
	}
}

void AJamBuildPart::SetDriveScale(float NewScale)
{
	NewScale = FMath::Clamp(NewScale, -1.5f, 1.5f);
	if (FMath::IsNearlyEqual(NewScale, DriveScale))
	{
		return;
	}
	DriveScale = NewScale;
	UpdateDriveTarget();
}

void AJamBuildPart::SetActive(bool bNewActive)
{
	if (bNewActive)
	{
		ResolveBuild();
		if (Kind == EJamPartKind::Rocket && bRocketSpent)
		{
			// De un solo uso: un cohete quemado no se vuelve a encender.
			bActive = false;
			return;
		}
	}
	bActive = bNewActive;
	if (Kind == EJamPartKind::Rocket && bNewActive)
	{
		BurnLeft = 3.f;
	}
	if (Kind == EJamPartKind::Bomb && bNewActive)
	{
		BurnLeft = 4.f; // mecha de 4 s
	}
	UpdateDriveTarget();

	switch (Kind)
	{
	case EJamPartKind::Stake:
		SetStaked(bNewActive);
		break;
	case EJamPartKind::Turntable:
	case EJamPartKind::Head:
		if (bNewActive)
		{
			SetupRotorJoints();
		}
		break;
	case EJamPartKind::Spring:
		if (bNewActive)
		{
			FireTimer = 0.f;
			FireSpring();
		}
		break;
	default:
		break;
	}
}

UJamEnergyComponent* AJamBuildPart::GetEnergy()
{
	AActor* CurrentOwner = GetOwner();
	if (!CurrentOwner)
	{
		EnergyOwner = nullptr;
		EnergyComp = nullptr;
		return nullptr;
	}
	if (!EnergyComp.IsValid() || EnergyOwner.Get() != CurrentOwner)
	{
		EnergyOwner = CurrentOwner;
		UJamEnergyComponent* Found = CurrentOwner->FindComponentByClass<UJamEnergyComponent>();
		if (!Found)
		{
			if (AController* Controller = PartControllerOf(CurrentOwner))
			{
				Found = Controller->FindComponentByClass<UJamEnergyComponent>();
			}
		}
		EnergyComp = Found;
	}
	return EnergyComp.Get();
}

bool AJamBuildPart::ConsumeEnergy(float DeltaSeconds)
{
	if (EnergyPerSec <= 0.f)
	{
		return true;
	}
	UJamEnergyComponent* Energy = GetEnergy();
	if (!Energy)
	{
		return true; // sin componente de energía en el dueño: funciona sin gastar
	}
	const float Scale = (Kind == EJamPartKind::Wheel || Kind == EJamPartKind::Fan) ? DriveScale : 1.f;
	return Energy->Spend(JamDevices::EnergyCost(EnergyPerSec, Scale, DeltaSeconds));
}

void AJamBuildPart::TryBindBattery()
{
	if (Power <= 0.f)
	{
		bBatteryGiven = true;
		return;
	}
	if (UJamEnergyComponent* Energy = GetEnergy())
	{
		Energy->AddReserve(Power);
		BatteryComp = Energy;
		bBatteryGiven = true;
	}
}

// ---------------------------------------------------------------------------------------------
// Dispositivos
// ---------------------------------------------------------------------------------------------

void AJamBuildPart::ApplyCone(const FVector& Start, const FVector& Dir, float Range, float HalfAngleDeg, float Damage, bool bFrost)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	TArray<FOverlapResult> Hits;
	FCollisionQueryParams Query(NAME_None, false, this);
	World->OverlapMultiByObjectType(Hits, Start, FQuat::Identity, FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects),
		FCollisionShape::MakeSphere(Range), Query);

	const EJamTeam MyTeam = UJamStatics::GetActorTeam(this);
	AController* InstigatorController = PartControllerOf(GetOwner());
	const FVector Middle = Start + Dir * (Range * 0.5f);
	static const FName BalloonType(TEXT("Balloon"));

	TArray<AActor*> Seen;
	for (const FOverlapResult& Hit : Hits)
	{
		AActor* Target = Hit.GetActor();
		const UPrimitiveComponent* Comp = Hit.GetComponent();
		if (!IsValid(Target) || !Comp || Target == this || Seen.Contains(Target))
		{
			continue;
		}
		// Punto de la caja del componente más cercano al centro del cono: así una torre grande cuenta aunque su origen quede fuera.
		const FVector Point = Comp->Bounds.GetBox().GetClosestPointTo(Middle);
		if (!JamDevices::IsInCone(Start, Dir, Point, Range, HalfAngleDeg))
		{
			continue;
		}
		Seen.Add(Target);

		if (AJamBuildPart* Part = Cast<AJamBuildPart>(Target))
		{
			// Tabla de interacciones: fuego sobre un globo = sube el doble mientras dure la llama.
			if (!bFrost && Part->PartType == BalloonType)
			{
				Part->HeatLeft = JamDevices::BalloonHeatHold;
			}
			continue;
		}
		if (Target == GetOwner())
		{
			continue;
		}
		const UHealthComponent* Health = Target->FindComponentByClass<UHealthComponent>();
		if (!Health || Health->IsDead() || !JamDevices::AreHostile(MyTeam, Health->Team))
		{
			continue;
		}
		if (bFrost)
		{
			UJamSlowComponent::ApplySlow(Target, JamDevices::FrostSlowFactor, JamDevices::FrostSlowDuration);
		}
		else
		{
			UGameplayStatics::ApplyDamage(Target, Damage, InstigatorController, this, nullptr);
		}
	}
}

void AJamBuildPart::FireSpring()
{
	ResolveBuild();
	if (!Mesh)
	{
		return;
	}
	const FVector Out = GetFaceAxisWorld();
	TArray<AJamBuildPart*> Children;
	GetChildParts(Children);

	if (Children.Num() == 0)
	{
		// Nada encima: el resorte empuja contra lo que tiene enfrente y toda la construcción salta al lado contrario.
		for (AJamBuildPart* Part : GetBuildParts())
		{
			if (Part->Mesh)
			{
				Part->Mesh->AddImpulse(-Out * Power, NAME_None, true);
			}
		}
		return;
	}

	// Lanza lo de encima: suelta la unión y le da velocidad a todo el subárbol (catapulta). Es de un disparo.
	TArray<AJamBuildPart*> Launched;
	for (AJamBuildPart* Child : Children)
	{
		Child->CollectSubtree(Launched);
	}
	for (AJamBuildPart* Child : Children)
	{
		AJamBuildPart* Unused = nullptr;
		if (UPhysicsConstraintComponent* Joint = Child->FindParentJoint(Unused))
		{
			Joint->BreakConstraint();
			Joint->DestroyComponent();
		}
	}
	InvalidateBuild();
	for (AJamBuildPart* Part : Launched)
	{
		if (Part->Mesh)
		{
			Part->Mesh->AddImpulse(Out * (Power * JamDevices::SpringLaunchBoost), NAME_None, true);
		}
	}
	bActive = false;
}

void AJamBuildPart::SetupRotorJoints()
{
	if (bRotorReady || !Mesh)
	{
		return;
	}
	ResolveBuild();
	bRotorReady = true;

	const FVector Axis = GetFaceAxisWorld();
	TArray<AJamBuildPart*> Children;
	GetChildParts(Children);
	for (AJamBuildPart* Child : Children)
	{
		if (Child->DriveConstraint || !Child->Mesh)
		{
			continue; // las ruedas conservan su propio eje con motor
		}
		AJamBuildPart* Unused = nullptr;
		UPhysicsConstraintComponent* Joint = Child->FindParentJoint(Unused);
		if (!Joint)
		{
			continue;
		}
		// La soldadura se vuelve un eje libre (twist = X de la constraint) que pasa por el centro de esta pieza.
		Joint->SetWorldLocation(GetActorLocation());
		Joint->SetWorldRotation(FRotationMatrix::MakeFromX(Axis).ToQuat());
		Joint->SetLinearXLimit(LCM_Locked, 0);
		Joint->SetLinearYLimit(LCM_Locked, 0);
		Joint->SetLinearZLimit(LCM_Locked, 0);
		Joint->SetAngularSwing1Limit(ACM_Locked, 0);
		Joint->SetAngularSwing2Limit(ACM_Locked, 0);
		Joint->SetAngularTwistLimit(ACM_Free, 0);
		Joint->SetConstrainedComponents(Mesh, NAME_None, Child->Mesh, NAME_None);
		RotorChildren.Add(Child);
	}

	if (Kind == EJamPartKind::Head)
	{
		// Referencia de puntería: la primera arma que haya encima; si no hay, el frente de lo que lleve encima.
		TArray<AJamBuildPart*> Above;
		for (const TWeakObjectPtr<AJamBuildPart>& Child : RotorChildren)
		{
			if (AJamBuildPart* Part = Child.Get())
			{
				Part->CollectSubtree(Above);
			}
		}
		AJamBuildPart* Ref = nullptr;
		for (AJamBuildPart* Part : Above)
		{
			if (IsWeaponPartKind(Part->Kind))
			{
				Ref = Part;
				break;
			}
		}
		if (Ref)
		{
			AimRefLocalDir = Ref->LocalDirection;
		}
		else if (Above.Num() > 0)
		{
			Ref = Above[0];
			AimRefLocalDir = Ref->GetActorQuat().UnrotateVector(GetBuildRoot()->GetBuildQuat().GetForwardVector());
		}
		AimRefPart = Ref;
	}
}

void AJamBuildPart::DriveRotor(float TargetRate, float DeltaSeconds)
{
	if (!Mesh)
	{
		return;
	}
	const FVector Axis = GetFaceAxisWorld();
	const FVector MyOmega = Mesh->GetPhysicsAngularVelocityInRadians();
	for (const TWeakObjectPtr<AJamBuildPart>& Child : RotorChildren)
	{
		AJamBuildPart* Part = Child.Get();
		if (!Part || !Part->Mesh)
		{
			continue;
		}
		// Velocidad de giro relativa a esta pieza sobre el eje; el torque va directo al cuerpo de encima.
		const float Relative = static_cast<float>(FVector::DotProduct(Part->Mesh->GetPhysicsAngularVelocityInRadians() - MyOmega, Axis));
		const float Accel = JamDevices::SpinAccel(TargetRate, Relative, JamDevices::SpinGain, JamDevices::SpinMaxAccel, DeltaSeconds);
		Part->Mesh->AddTorqueInRadians(Axis * Accel, NAME_None, true);
	}
}

AActor* AJamBuildPart::FindAimTarget() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	TArray<FOverlapResult> Hits;
	FCollisionQueryParams Query(NAME_None, false, this);
	World->OverlapMultiByObjectType(Hits, GetActorLocation(), FQuat::Identity, FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects),
		FCollisionShape::MakeSphere(JamDevices::HeadRange), Query);

	TArray<AActor*> Actors;
	TArray<JamDevices::FTargetCandidate> Candidates;
	for (const FOverlapResult& Hit : Hits)
	{
		AActor* Target = Hit.GetActor();
		if (!IsValid(Target) || Target == GetOwner() || Actors.Contains(Target))
		{
			continue;
		}
		const UHealthComponent* Health = Target->FindComponentByClass<UHealthComponent>();
		if (!Health)
		{
			continue;
		}
		JamDevices::FTargetCandidate Candidate;
		Candidate.Position = Target->GetActorLocation();
		Candidate.Team = Health->Team;
		Candidate.bAlive = !Health->IsDead();
		Actors.Add(Target);
		Candidates.Add(Candidate);
	}
	const int32 Best = JamDevices::PickNearestTarget(GetActorLocation(), Candidates, UJamStatics::GetActorTeam(this), JamDevices::HeadRange);
	return Actors.IsValidIndex(Best) ? Actors[Best] : nullptr;
}

void AJamBuildPart::SetStaked(bool bStaked)
{
	if (!bStaked)
	{
		if (StakeJoint)
		{
			StakeJoint->BreakConstraint();
			StakeJoint->DestroyComponent();
			StakeJoint = nullptr;
		}
		return;
	}
	if (StakeJoint || !Mesh)
	{
		return;
	}
	// Constraint contra el mundo (segundo componente nulo), todo bloqueado: la construcción queda clavada donde está.
	UPhysicsConstraintComponent* Joint = NewObject<UPhysicsConstraintComponent>(this);
	Joint->RegisterComponent();
	Joint->AttachToComponent(Mesh, FAttachmentTransformRules::KeepRelativeTransform);
	Joint->SetWorldLocation(GetActorLocation());
	Joint->SetLinearXLimit(LCM_Locked, 0);
	Joint->SetLinearYLimit(LCM_Locked, 0);
	Joint->SetLinearZLimit(LCM_Locked, 0);
	Joint->SetAngularSwing1Limit(ACM_Locked, 0);
	Joint->SetAngularSwing2Limit(ACM_Locked, 0);
	Joint->SetAngularTwistLimit(ACM_Locked, 0);
	Joint->SetConstrainedComponents(Mesh, NAME_None, nullptr, NAME_None);
	StakeJoint = Joint;
}

// ---------------------------------------------------------------------------------------------
// Asiento: montarse y dirigir (research 13 §5)
// ---------------------------------------------------------------------------------------------

bool AJamBuildPart::Mount(AActor* Rider, bool bAttachRider)
{
	if (!IsValid(Rider) || bMounted || !Mesh)
	{
		return false;
	}
	ResolveBuild();
	const TArray<AJamBuildPart*> Parts = GetBuildParts();
	for (const AJamBuildPart* Part : Parts)
	{
		if (Part->bMounted)
		{
			return false; // un conductor por construcción
		}
	}

	bMounted = true;
	SeatRider = Rider;
	bRiderAttached = bAttachRider;
	SeatThrottle = 0.f;
	SeatSteer = 0.f;
	SeatLift = 0.f;
	SetBuildOwner(Rider);

	// Masa, inercia aproximada y si la construcción vuela (para el nivelado automático).
	const FQuat BuildQuat = GetBuildRoot()->GetBuildQuat();
	const FVector BuildUp = BuildQuat.GetUpVector();
	double TotalMass = 0.0;
	FVector Com = FVector::ZeroVector;
	bool bAirborne = false;
	bSeatHasLiftFans = false;
	for (const AJamBuildPart* Part : Parts)
	{
		if (!Part->Mesh)
		{
			continue;
		}
		const double Mass = Part->Mesh->GetMass();
		TotalMass += Mass;
		Com += Part->GetActorLocation() * Mass;
		if (Part->Kind == EJamPartKind::Fan && JamDevices::IsLiftDirection(Part->GetActorQuat().RotateVector(Part->LocalDirection), BuildUp))
		{
			bSeatHasLiftFans = true;
		}
		if (Part->Kind == EJamPartKind::Balloon || Part->Kind == EJamPartKind::Wing)
		{
			bAirborne = true;
		}
	}
	Com /= FMath::Max(TotalMass, 1.0);
	double Inertia = 0.0;
	for (const AJamBuildPart* Part : Parts)
	{
		if (Part->Mesh)
		{
			Inertia += static_cast<double>(Part->Mesh->GetMass()) * (FVector::DistSquared(Part->GetActorLocation(), Com) + 400.0);
		}
	}
	BuildInertia = static_cast<float>(Inertia);
	bSeatLevel = (bAirborne || bSeatHasLiftFans) && FVector::DotProduct(BuildUp, FVector::UpVector) > 0.7;

	if (bAttachRider)
	{
		if (ACharacter* Character = Cast<ACharacter>(Rider))
		{
			if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
			{
				Movement->DisableMovement();
			}
		}
		Rider->SetActorEnableCollision(false);
		Rider->SetActorLocationAndRotation(GetActorLocation() + FVector::UpVector * 110.0, FRotator(0.0, BuildQuat.Rotator().Yaw, 0.0));
		Rider->AttachToComponent(Mesh, FAttachmentTransformRules::KeepWorldTransform);
	}

	SetBuildPower(true);
	return true;
}

void AJamBuildPart::ReleaseRider()
{
	AActor* Rider = SeatRider.Get();
	if (Rider && bRiderAttached)
	{
		Rider->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		Rider->SetActorRotation(FRotator(0.0, Rider->GetActorRotation().Yaw, 0.0));
		Rider->SetActorEnableCollision(true);
		if (ACharacter* Character = Cast<ACharacter>(Rider))
		{
			if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
			{
				Movement->SetMovementMode(MOVE_Falling);
			}
		}
	}
	SeatRider = nullptr;
	bMounted = false;
	bRiderAttached = false;
	SeatThrottle = 0.f;
	SeatSteer = 0.f;
	SeatLift = 0.f;
}

void AJamBuildPart::Dismount(bool bKeepRunning)
{
	if (!bMounted)
	{
		return;
	}
	ReleaseRider();
	for (AJamBuildPart* Part : GetBuildParts())
	{
		Part->SetDriveScale(1.f); // sin asiento la construcción va derecho a potencia completa
	}
	if (!bKeepRunning)
	{
		SetBuildPower(false);
	}
}

void AJamBuildPart::SetThrottle(float Value)
{
	SeatThrottle = FMath::Clamp(Value, -1.f, 1.f);
}

void AJamBuildPart::SetSteer(float Value)
{
	SeatSteer = FMath::Clamp(Value, -1.f, 1.f);
}

void AJamBuildPart::SetLift(float Value)
{
	SeatLift = FMath::Clamp(Value, -1.f, 1.f);
}

void AJamBuildPart::TickSeat(float DeltaSeconds)
{
	if (!SeatRider.IsValid())
	{
		Dismount(false);
		return;
	}
	if (bPocKeyboardInput)
	{
		JamPoc::ReadDriveKeys(GetWorld(), SeatThrottle, SeatSteer, SeatLift);
	}

	AJamBuildPart* Root = GetBuildRoot();
	if (!Root || !Root->Mesh)
	{
		return;
	}
	const FQuat BuildQuat = Root->GetBuildQuat();
	const FVector Up = BuildQuat.GetUpVector();
	const FVector Forward = BuildQuat.GetForwardVector();
	const FVector Right = BuildQuat.GetRightVector();
	const FVector RootLocation = Root->GetActorLocation();

	// 1) Acelerador, diferencial y sustentación: multiplicador por pieza.
	for (const TWeakObjectPtr<AJamBuildPart>& Member : BuildMembers)
	{
		AJamBuildPart* Part = Member.Get();
		if (!Part)
		{
			continue;
		}
		if (Part->Kind == EJamPartKind::Wheel)
		{
			const double Lateral = FVector::DotProduct(Part->GetActorLocation() - RootLocation, Right);
			const float Side = Lateral > 1.0 ? 1.f : (Lateral < -1.0 ? -1.f : 0.f);
			Part->SetDriveScale(JamDevices::WheelScale(SeatThrottle, SeatSteer, Side));
		}
		else if (Part->Kind == EJamPartKind::Fan)
		{
			const bool bLiftFan = JamDevices::IsLiftDirection(Part->GetActorQuat().RotateVector(Part->LocalDirection), Up);
			Part->SetDriveScale(JamDevices::FanScale(bLiftFan, SeatThrottle, SeatLift));
		}
	}

	// 2) Torque directo sobre la raíz: yaw (A/D) y nivelado PD de pitch/roll si la construcción vuela.
	const FVector Omega = Root->Mesh->GetPhysicsAngularVelocityInRadians();
	FVector Accel = FVector::ZeroVector;
	if (bSeatLevel)
	{
		const FVector ForwardFlat = FVector::VectorPlaneProject(Forward, FVector::UpVector);
		const FVector DesiredUp = bSeatHasLiftFans ? JamDevices::TiltedUp(ForwardFlat, SeatThrottle, JamDevices::SeatMaxTiltDeg) : FVector::UpVector;
		Accel += JamDevices::LevelAccel(Up, DesiredUp, Omega, JamDevices::SeatLevelKp, JamDevices::SeatLevelKd);
	}
	if (bSeatLevel || FMath::Abs(SeatSteer) > 0.05f)
	{
		const double YawRate = FVector::DotProduct(Omega, Up);
		Accel += Up * ((static_cast<double>(SeatSteer * JamDevices::SeatMaxYawRate) - YawRate) * static_cast<double>(JamDevices::SeatYawGain));
	}
	if (!Accel.IsNearlyZero())
	{
		Root->Mesh->AddTorqueInRadians(Accel * static_cast<double>(BuildInertia), NAME_None, false);
	}

	if (JamDevices::IsDebugDrawEnabled())
	{
		DrawDebugDirectionalArrow(GetWorld(), RootLocation + Up * 60.0, RootLocation + Up * 60.0 + Forward * (60.0 + 100.0 * SeatThrottle) + Right * (100.0 * SeatSteer),
			25.f, FColor::White, false, 0.f, 0, 3.f);
	}
}

AJamBuildPart* AJamBuildPart::FindNearestSeat(const UObject* WorldContextObject, FVector Location, float MaxDistance, bool bAnyPart)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}
	AJamBuildPart* Best = nullptr;
	double BestDistSq = static_cast<double>(MaxDistance) * static_cast<double>(MaxDistance);
	for (TActorIterator<AJamBuildPart> It(World); It; ++It)
	{
		AJamBuildPart* Part = *It;
		if (!IsValid(Part) || (!bAnyPart && Part->Kind != EJamPartKind::Seat))
		{
			continue;
		}
		const double DistSq = FVector::DistSquared(Part->GetActorLocation(), Location);
		if (DistSq <= BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Part;
		}
	}
	return Best;
}

void AJamBuildPart::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bMounted && EndPlayReason == EEndPlayReason::Destroyed)
	{
		ReleaseRider();
	}
	if (bBatteryGiven && BatteryComp.IsValid())
	{
		BatteryComp->RemoveReserve(Power); // la reserva que quedaba se va con la batería
	}
	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------------------------
// Tick: lo que hace cada pieza encendida
// ---------------------------------------------------------------------------------------------

void AJamBuildPart::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Mesh)
	{
		return;
	}
	if (Kind == EJamPartKind::Battery && !bBatteryGiven)
	{
		TryBindBattery();
	}
	if (bMounted)
	{
		TickSeat(DeltaSeconds);
	}
	if (!bActive)
	{
		if (bRotorReady)
		{
			DriveRotor(0.f, DeltaSeconds); // apagada: frena lo de encima en vez de dejarlo suelto
		}
		return;
	}
	// Energía tipo maná: se pide antes de empujar o disparar. Si no alcanza, la pieza se apaga.
	if (!ConsumeEnergy(DeltaSeconds))
	{
		SetActive(false);
		return;
	}

	UWorld* World = GetWorld();
	const bool bDebug = JamDevices::IsDebugDrawEnabled();
	const FVector Location = GetActorLocation();
	const FVector WorldDir = GetActorQuat().RotateVector(LocalDirection);

	switch (Kind)
	{
	case EJamPartKind::Wheel:
	{
		const FVector Axis = GetActorUpVector(); // el eje del cilindro es el Z local del mesh
		if (!DriveConstraint)
		{
			// Rueda raíz (o suelta): no hay padre contra el cual girar, así que el torque va directo al cuerpo
			// y lo que lleve pegado gira con ella (Rueda Infernal, Spinner, Roller).
			const float Omega = static_cast<float>(FVector::DotProduct(Mesh->GetPhysicsAngularVelocityInRadians(), Axis));
			const float Accel = JamDevices::SpinAccel(Power * DriveScale, Omega, JamDevices::SpinGain, JamDevices::SpinMaxAccel, DeltaSeconds);
			Mesh->AddTorqueInRadians(Axis * Accel, NAME_None, true);
		}
		if (bDebug)
		{
			DrawDebugDirectionalArrow(World, Location, Location + Axis * (DriveScale < 0.f ? -90.0 : 90.0), 20.f,
				DriveConstraint ? FColor::Green : FColor::Yellow, false, 0.f, 0, 2.f);
		}
		break;
	}
	case EJamPartKind::Fan:
		Mesh->AddForce(WorldDir * (Power * DriveScale));
		if (bDebug)
		{
			DrawDebugDirectionalArrow(World, Location, Location + WorldDir * (120.0 * DriveScale), 25.f, FColor::Cyan, false, 0.f, 0, 3.f);
		}
		break;
	case EJamPartKind::Rocket:
		Mesh->AddForce(WorldDir * Power);
		if (bDebug)
		{
			DrawDebugDirectionalArrow(World, Location, Location + WorldDir * 200.0, 35.f, FColor::Red, false, 0.f, 0, 4.f);
		}
		BurnLeft -= DeltaSeconds;
		if (BurnLeft <= 0.f)
		{
			bActive = false;
			bRocketSpent = true;
		}
		break;
	case EJamPartKind::Balloon:
	{
		const float Heat = HeatLeft > 0.f ? JamDevices::BalloonHeatMultiplier : 1.f;
		HeatLeft = FMath::Max(0.f, HeatLeft - DeltaSeconds);
		Mesh->AddForce(FVector(0, 0, Power * Heat));
		if (bDebug && Heat > 1.f)
		{
			DrawDebugDirectionalArrow(World, Location, Location + FVector(0, 0, 150), 30.f, FColor::Orange, false, 0.f, 0, 3.f);
		}
		break;
	}
	case EJamPartKind::Flame:
	{
		// Cono hacia afuera de su cara: daño en el tiempo a lo que tenga vida y sea de otro equipo. Si la base gira, el cono gira.
		const FVector Start = Location + WorldDir * (Length * 0.5f);
		if (bDebug)
		{
			const float Half = FMath::DegreesToRadians(JamDevices::FlameHalfAngleDeg);
			DrawDebugCone(World, Start, WorldDir, JamDevices::FlameRange, Half, Half, 12, FColor::Orange, false, 0.f, 0, 1.5f);
		}
		if (JamDevices::CooldownReady(DamageTimer, DeltaSeconds, JamDevices::DamageInterval))
		{
			ApplyCone(Start, WorldDir, JamDevices::FlameRange, JamDevices::FlameHalfAngleDeg, JamDevices::FlameDps * JamDevices::DamageInterval, false);
		}
		break;
	}
	case EJamPartKind::Frost:
	{
		// Cono de hielo: lento 50 % por 3 s (se renueva mientras siga dentro).
		const FVector Start = Location + WorldDir * (Length * 0.5f);
		if (bDebug)
		{
			const float Half = FMath::DegreesToRadians(JamDevices::FrostHalfAngleDeg);
			DrawDebugCone(World, Start, WorldDir, JamDevices::FrostRange, Half, Half, 12, FColor::Cyan, false, 0.f, 0, 1.5f);
		}
		if (JamDevices::CooldownReady(DamageTimer, DeltaSeconds, JamDevices::DamageInterval))
		{
			ApplyCone(Start, WorldDir, JamDevices::FrostRange, JamDevices::FrostHalfAngleDeg, 0.f, true);
		}
		break;
	}
	case EJamPartKind::Beam:
	{
		// Rayo: línea hacia delante que empuja lo físico y daña lo que tenga vida.
		const bool bDamageTick = JamDevices::CooldownReady(DamageTimer, DeltaSeconds, JamDevices::DamageInterval);
		const FVector Start = Location + WorldDir * (Length * 0.5f + 10.f);
		const FVector End = Start + WorldDir * JamDevices::BeamRange;
		FHitResult Hit;
		FCollisionQueryParams Query(NAME_None, false, this);
		for (const TWeakObjectPtr<AJamBuildPart>& Member : BuildMembers)
		{
			if (const AJamBuildPart* Part = Member.Get())
			{
				Query.AddIgnoredActor(Part);
			}
		}
		if (GetOwner())
		{
			Query.AddIgnoredActor(GetOwner());
		}
		const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_PhysicsBody, Query);
		DrawDebugLine(World, Start, bHit ? Hit.ImpactPoint : End, FColor::Magenta, false, 0.f, 0, 3.f);
		AActor* Target = bHit ? Hit.GetActor() : nullptr;
		if (Target && Hit.GetComponent() && Hit.GetComponent()->IsSimulatingPhysics() && !Cast<AJamBuildPart>(Target))
		{
			Hit.GetComponent()->AddForceAtLocation(WorldDir * Power, Hit.ImpactPoint);
		}
		if (Target && bDamageTick)
		{
			const UHealthComponent* Health = Target->FindComponentByClass<UHealthComponent>();
			if (Health && !Health->IsDead() && JamDevices::AreHostile(UJamStatics::GetActorTeam(this), Health->Team))
			{
				UGameplayStatics::ApplyDamage(Target, JamDevices::BeamDps * JamDevices::DamageInterval, PartControllerOf(GetOwner()), this, nullptr);
			}
		}
		break;
	}
	case EJamPartKind::Cannon:
	{
		if (bDebug)
		{
			DrawDebugLine(World, Location, Location + WorldDir * JamDevices::HeadRange, FColor(120, 120, 120), false, 0.f, 0, 1.f);
		}
		if (JamDevices::CooldownReady(FireTimer, DeltaSeconds, JamDevices::CannonInterval))
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Params.Owner = this;
			Params.Instigator = Cast<APawn>(GetOwner());
			if (AJamProjectile* Ball = World->SpawnActor<AJamProjectile>(Location + WorldDir * (Length * 0.5f + 25.f), FRotator::ZeroRotator, Params))
			{
				Ball->Damage = JamDevices::CannonDamage;
				Ball->Launch(WorldDir * JamDevices::CannonSpeed + Mesh->GetPhysicsLinearVelocity());
			}
		}
		break;
	}
	case EJamPartKind::Wing:
	{
		// Sustentacion: crece con la velocidad hacia adelante (el ala no se rota: su X = frente del vehiculo).
		const double Fwd = FVector::DotProduct(Mesh->GetPhysicsLinearVelocity(), GetActorForwardVector());
		Mesh->AddForce(FVector(0, 0, FMath::Clamp(Fwd, 0.0, 2500.0) * Power));
		break;
	}
	case EJamPartKind::Sail:
		Mesh->AddForce(FVector(Power, 0, 0)); // viento constante hacia +X del mundo
		break;
	case EJamPartKind::Spring:
		// Sin nada encima sigue saltando cada 1.5 s; con algo encima ya lo lanzó en SetActive y se apagó.
		if (JamDevices::CooldownReady(FireTimer, DeltaSeconds, JamDevices::SpringInterval))
		{
			FireSpring();
		}
		break;
	case EJamPartKind::Bomb:
		BurnLeft -= DeltaSeconds;
		if (BurnLeft <= 0.f)
		{
			bActive = false;
			const FVector At = GetActorLocation();
			DrawDebugSphere(World, At, 300.f, 16, FColor::Red, false, 0.6f);
			Mesh->AddRadialImpulse(At, 500.f, Power, ERadialImpulseFalloff::RIF_Linear, true);
			Destroy();
		}
		break;
	case EJamPartKind::Turntable:
		// Gira lo de encima a velocidad constante (torreta giratoria, aspersor).
		DriveRotor(Power > 0.f ? Power : JamDevices::TurntableRate, DeltaSeconds);
		if (bDebug)
		{
			const FVector Axis = GetFaceAxisWorld();
			DrawDebugDirectionalArrow(World, Location, Location + Axis * 80.0, 20.f, FColor::Yellow, false, 0.f, 0, 2.f);
		}
		break;
	case EJamPartKind::Head:
	{
		// Apunta (en yaw sobre su eje) al enemigo más cercano; lo de encima apunta con ella.
		if (JamDevices::CooldownReady(DamageTimer, DeltaSeconds, JamDevices::DamageInterval))
		{
			AimTarget = FindAimTarget();
		}
		float Rate = 0.f;
		const AActor* Target = AimTarget.Get();
		const AJamBuildPart* Ref = AimRefPart.Get();
		if (Target && Ref)
		{
			const FVector AimDir = Ref->GetActorQuat().RotateVector(AimRefLocalDir);
			const float Error = JamDevices::SignedAngleAroundAxis(AimDir, Target->GetActorLocation() - Ref->GetActorLocation(), GetFaceAxisWorld());
			Rate = JamDevices::AimRate(Error, JamDevices::HeadKp, JamDevices::HeadMaxRate);
			if (bDebug)
			{
				DrawDebugLine(World, Location, Target->GetActorLocation(), FColor::Red, false, 0.f, 0, 1.5f);
				DrawDebugDirectionalArrow(World, Ref->GetActorLocation(), Ref->GetActorLocation() + AimDir * 150.0, 20.f, FColor::Green, false, 0.f, 0, 2.f);
			}
		}
		DriveRotor(Rate, DeltaSeconds);
		break;
	}
	default:
		break;
	}
}
