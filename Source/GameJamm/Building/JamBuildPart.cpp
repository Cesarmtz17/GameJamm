#include "Building/JamBuildPart.h"

#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
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

	FJamPartDef Make(FName Type, EJamPartKind Kind, const TCHAR* Mesh, FVector Size, float Mass, float Power, int32 Axis, FLinearColor Color)
	{
		FJamPartDef D;
		D.Type = Type;
		D.Kind = Kind;
		D.MeshPath = Mesh;
		D.Size = Size;
		D.MassKg = Mass;
		D.Power = Power;
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
			Make("Battery", EJamPartKind::Battery, CubeMesh, FVector(20, 20, 20), 3.f, 0.f, 0, FLinearColor(0.9f, 0.9f, 0.1f)),
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

void AJamBuildPart::SetActive(bool bNewActive)
{
	bActive = bNewActive;
	if (Kind == EJamPartKind::Rocket && bNewActive)
	{
		BurnLeft = 3.f;
	}
	if (Kind == EJamPartKind::Bomb && bNewActive)
	{
		BurnLeft = 4.f; // mecha de 4 s
	}
	if (DriveConstraint)
	{
		DriveConstraint->SetAngularVelocityTarget(FVector(bNewActive ? Power : 0.f, 0.f, 0.f) / (2.0 * PI));
	}
}

void AJamBuildPart::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bActive || !Mesh)
	{
		return;
	}

	const FVector WorldDir = GetActorQuat().RotateVector(LocalDirection);

	switch (Kind)
	{
	case EJamPartKind::Fan:
		Mesh->AddForce(WorldDir * Power);
		break;
	case EJamPartKind::Rocket:
		Mesh->AddForce(WorldDir * Power);
		BurnLeft -= DeltaSeconds;
		if (BurnLeft <= 0.f)
		{
			bActive = false;
		}
		break;
	case EJamPartKind::Balloon:
		Mesh->AddForce(FVector(0, 0, Power));
		break;
	case EJamPartKind::Flame:
		DrawDebugSphere(GetWorld(), GetActorLocation() + WorldDir * (Length * 0.5f + 25.f), 22.f, 8, FColor::Orange, false, 0.f);
		break;
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
		FireTimer += DeltaSeconds;
		if (FireTimer >= 1.5f)
		{
			FireTimer = 0.f;
			Mesh->AddImpulse(FVector(0, 0, Power), NAME_None, true); // salto periodico
		}
		break;
	case EJamPartKind::Bomb:
		BurnLeft -= DeltaSeconds;
		if (BurnLeft <= 0.f)
		{
			bActive = false;
			const FVector At = GetActorLocation();
			DrawDebugSphere(GetWorld(), At, 300.f, 16, FColor::Red, false, 0.6f);
			Mesh->AddRadialImpulse(At, 500.f, Power, ERadialImpulseFalloff::RIF_Linear, true);
			Destroy();
		}
		break;
	case EJamPartKind::Frost:
	case EJamPartKind::Beam:
	{
		const FVector Start = GetActorLocation() + WorldDir * (Length * 0.5f + 10.f);
		if (Kind == EJamPartKind::Frost)
		{
			// Cono de hielo: frena (amortigua) cuerpos simulados que no son de esta construccion.
			DrawDebugSphere(GetWorld(), Start + WorldDir * 150.f, 150.f, 10, FColor::Cyan, false, 0.f);
			TArray<FOverlapResult> Hits;
			GetWorld()->OverlapMultiByChannel(Hits, Start + WorldDir * 150.f, FQuat::Identity, ECC_PhysicsBody, FCollisionShape::MakeSphere(150.f));
			for (const FOverlapResult& H : Hits)
			{
				UPrimitiveComponent* C = H.GetComponent();
				if (C && C->IsSimulatingPhysics() && !Cast<AJamBuildPart>(C->GetOwner()))
				{
					C->SetLinearDamping(8.f);
					C->SetAngularDamping(8.f);
				}
			}
		}
		else
		{
			// Rayo: linea hacia delante que empuja lo que toca.
			FHitResult Hit;
			const FVector End = Start + WorldDir * 1500.f;
			FCollisionQueryParams Q(NAME_None, false, this);
			const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_PhysicsBody, Q);
			DrawDebugLine(GetWorld(), Start, bHit ? Hit.ImpactPoint : End, FColor::Magenta, false, 0.f, 0, 3.f);
			if (bHit && Hit.GetComponent() && Hit.GetComponent()->IsSimulatingPhysics() && !Cast<AJamBuildPart>(Hit.GetActor()))
			{
				Hit.GetComponent()->AddForceAtLocation(WorldDir * Power, Hit.ImpactPoint);
			}
		}
		break;
	}
	case EJamPartKind::Cannon:
	{
		FireTimer += DeltaSeconds;
		if (FireTimer >= 1.5f)
		{
			FireTimer = 0.f;
			FActorSpawnParameters P;
			P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AStaticMeshActor* Ball = GetWorld()->SpawnActor<AStaticMeshActor>(GetActorLocation() + WorldDir * (Length * 0.5f + 25.f), FRotator::ZeroRotator, P))
			{
				UStaticMeshComponent* C = Ball->GetStaticMeshComponent();
				C->SetMobility(EComponentMobility::Movable);
				C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, SphereMesh));
				C->SetWorldScale3D(FVector(0.2));
				C->SetSimulatePhysics(true);
				C->SetMassOverrideInKg(NAME_None, 2.f, true);
				C->AddImpulse(WorldDir * 2500.f, NAME_None, true);
				Ball->SetLifeSpan(5.f);
			}
		}
		break;
	}
	default:
		break;
	}
}
