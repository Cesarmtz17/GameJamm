#include "Building/JamBuildPart.h"

#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/PackageName.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogJamPart, Log, All);

static TAutoConsoleVariable<int32> CVarJamEnergy(TEXT("jam.Build.Energy"), 2,
	TEXT("2 = las piezas gastan primero las baterias de su construccion y luego la barra del jugador (por defecto); 1 = reserva propia por construccion; 0 = energia infinita."));

static TAutoConsoleVariable<int32> CVarJamRealMeshes(TEXT("jam.Build.RealMeshes"), 1,
	TEXT("1 = usar las mallas de Content/Game/Building/Parts cuando existan; 0 = siempre formas basicas."));

static TAutoConsoleVariable<int32> CVarJamFx(TEXT("jam.Build.Fx"), 1, TEXT("1 = efectos visuales simples (fuego, hielo, agua, humo); 0 = sin efectos."));

static TAutoConsoleVariable<int32> CVarJamDebug(TEXT("jam.Build.Debug"), 0,
	TEXT("1 = la pieza raiz de cada construccion escribe en el log su posicion, velocidad y energia cada segundo."));

namespace
{
	const TCHAR* CubeMesh = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* CylMesh = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	const TCHAR* SphereMesh = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	const TCHAR* ConeMesh = TEXT("/Engine/BasicShapes/Cone.Cone");
	const FName ProjectileTag(TEXT("JamProjectile"));

	typedef EJamPartKind K;

	FJamPartDef Make(FName Type, EJamPartKind Kind, const TCHAR* Mesh, FVector Size, float Mass, float Power, int32 Axis, FLinearColor Color, float Energy = 0.f, bool bFace = false)
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
		D.EnergyPerSecond = Energy;
		D.bFaceAligned = bFace;
		return D;
	}

	// ponytail: tabla en codigo; mover a DataAssets / leer Content/Game/Building/Data/BuildKnowledge.json cuando lleguen los assets.
	// Nombres, masas y energia = BuildKnowledge.json v2. Power = fuerza (kg*cm/s^2; 980 levantan 1 kg), rad/s o capacidad segun el Kind.
	const TArray<FJamPartDef>& Table()
	{
		static const TArray<FJamPartDef> T = {
			// --- estructura ---
			Make("Plank", K::Structural, CubeMesh, FVector(200, 60, 10), 5.f, 0.f, 0, FLinearColor(0.55f, 0.38f, 0.2f)),
			Make("Block", K::Structural, CubeMesh, FVector(60, 60, 60), 15.f, 0.f, 0, FLinearColor(0.5f, 0.5f, 0.55f)),
			Make("Cylinder", K::Structural, CylMesh, FVector(150, 50, 50), 10.f, 0.f, 1, FLinearColor(0.6f, 0.6f, 0.65f)),
			Make("Wall", K::Structural, CubeMesh, FVector(30, 200, 150), 30.f, 0.f, 0, FLinearColor(0.45f, 0.45f, 0.5f), 0.f, true),
			Make("Panel", K::Structural, CubeMesh, FVector(10, 100, 100), 8.f, 0.f, 0, FLinearColor(0.7f, 0.75f, 0.8f), 0.f, true),
			Make("Girder", K::Structural, CubeMesh, FVector(300, 20, 20), 6.f, 0.f, 0, FLinearColor(0.4f, 0.3f, 0.25f)),
			Make("Ball", K::Structural, SphereMesh, FVector(60, 60, 60), 10.f, 0.f, 0, FLinearColor(0.35f, 0.35f, 0.35f)),
			Make("Sled", K::Slide, CubeMesh, FVector(160, 70, 15), 4.f, 0.f, 0, FLinearColor(0.3f, 0.5f, 0.8f)),
			// --- uniones y anclas ---
			Make("Hinge", K::Hinge, CylMesh, FVector(20, 60, 20), 2.f, 60.f, 2, FLinearColor(0.8f, 0.6f, 0.2f)),
			Make("Turntable", K::Turntable, CylMesh, FVector(40, 40, 30), 4.f, 4.f, 0, FLinearColor(0.8f, 0.3f, 0.6f), 0.3f),
			Make("Stake", K::Stake, ConeMesh, FVector(15, 15, 20), 2.f, 0.f, 0, FLinearColor(0.3f, 0.2f, 0.1f)),
			// --- movilidad ---
			Make("Wheel", K::Wheel, CylMesh, FVector(50, 20, 50), 4.f, 10.f, 2, FLinearColor(0.1f, 0.1f, 0.1f), 0.2f),
			Make("BigWheel", K::Wheel, CylMesh, FVector(80, 30, 80), 8.f, 8.f, 2, FLinearColor(0.05f, 0.05f, 0.05f), 0.2f),
			Make("Spring", K::Spring, CylMesh, FVector(30, 30, 18), 3.f, 450.f, 0, FLinearColor(0.2f, 0.9f, 0.2f)),
			// --- propulsion y vuelo ---
			Make("Fan", K::Fan, CylMesh, FVector(15, 50, 50), 3.f, 25000.f, 1, FLinearColor(0.2f, 0.6f, 0.9f), 1.5f),
			Make("Jet", K::Fan, CylMesh, FVector(60, 40, 40), 6.f, 90000.f, 1, FLinearColor(0.9f, 0.5f, 0.15f), 2.5f),
			Make("Turbine", K::Fan, CylMesh, FVector(25, 90, 90), 6.f, 60000.f, 1, FLinearColor(0.3f, 0.8f, 0.7f), 2.5f),
			Make("Rocket", K::Rocket, ConeMesh, FVector(60, 20, 20), 4.f, 80000.f, 1, FLinearColor(0.9f, 0.2f, 0.1f)),
			Make("Balloon", K::Balloon, SphereMesh, FVector(90, 90, 90), 1.f, 14000.f, 0, FLinearColor(0.9f, 0.8f, 0.2f)),
			Make("HoverStone", K::Hover, SphereMesh, FVector(45, 45, 45), 4.f, 60000.f, 0, FLinearColor(0.6f, 0.4f, 0.9f), 0.5f),
			Make("Wing", K::Wing, CubeMesh, FVector(80, 160, 6), 3.f, 45.f, 0, FLinearColor(0.85f, 0.85f, 0.9f)),
			Make("Sail", K::Sail, CubeMesh, FVector(10, 120, 100), 2.f, 6000.f, 0, FLinearColor(0.95f, 0.95f, 0.85f)),
			Make("Stabilizer", K::Stabilizer, CubeMesh, FVector(30, 30, 20), 2.f, 120.f, 0, FLinearColor(0.1f, 0.8f, 0.8f), 0.3f),
			// --- control y energia (Battery: Power = capacidad) ---
			Make("Seat", K::Seat, CubeMesh, FVector(40, 40, 30), 5.f, 0.f, 0, FLinearColor(0.2f, 0.7f, 0.3f)),
			Make("Head", K::Head, CubeMesh, FVector(40, 40, 40), 3.f, 150.f, 0, FLinearColor(0.9f, 0.9f, 0.9f), 0.3f),
			Make("Battery", K::Battery, CubeMesh, FVector(20, 20, 20), 3.f, 30.f, 0, FLinearColor(0.9f, 0.9f, 0.1f)),
			Make("BigBattery", K::Battery, CubeMesh, FVector(35, 35, 35), 6.f, 80.f, 0, FLinearColor(1.f, 0.8f, 0.f)),
			// --- armas y emisores ---
			Make("Cannon", K::Cannon, CylMesh, FVector(70, 20, 20), 8.f, 2500.f, 1, FLinearColor(0.25f, 0.25f, 0.3f), 3.f),
			Make("Flamethrower", K::Flame, ConeMesh, FVector(40, 20, 20), 3.f, 10.f, 1, FLinearColor(1.f, 0.45f, 0.f), 2.f),
			Make("Frost", K::Frost, ConeMesh, FVector(40, 25, 25), 3.f, 0.f, 1, FLinearColor(0.5f, 0.9f, 1.f), 2.f),
			Make("Beam", K::Beam, CylMesh, FVector(50, 12, 12), 3.f, 400000.f, 1, FLinearColor(1.f, 0.1f, 0.4f), 2.f),
			Make("Shock", K::Shock, ConeMesh, FVector(40, 25, 25), 3.f, 8.f, 1, FLinearColor(1.f, 1.f, 0.3f), 2.f),
			Make("Bomb", K::Bomb, SphereMesh, FVector(30, 30, 30), 2.f, 1500.f, 0, FLinearColor(0.1f, 0.1f, 0.1f)),
			Make("Hydrant", K::Hydrant, CylMesh, FVector(40, 30, 30), 3.f, 15000.f, 1, FLinearColor(0.1f, 0.3f, 1.f), 0.3f),
			Make("Light", K::Light, SphereMesh, FVector(25, 25, 25), 2.f, 50000.f, 0, FLinearColor(1.f, 1.f, 0.8f), 0.2f),
		};
		return T;
	}
}

namespace
{
	// Barra de energia del jugador (BuildKnowledge.json: energy). ponytail: un solo jugador local y vive aqui;
	// cuando exista la barra real (HUD/Core), sustituir estas 3 funciones por ese componente.
	struct FJamPlayerEnergy
	{
		TWeakObjectPtr<const UWorld> World;
		float Energy = 60.f;      // energy.start
		float LastDrain = -100.f;
		float LastUpdate = 0.f;
	};

	FJamPlayerEnergy& PlayerPool(const UWorld* World)
	{
		static FJamPlayerEnergy P;
		if (P.World.Get() != World)
		{
			P = FJamPlayerEnergy();
			P.World = World;
		}
		const float Now = World ? World->GetTimeSeconds() : 0.f;
		const float From = FMath::Max(P.LastUpdate, P.LastDrain + 3.f); // energy.regen_delay_s
		if (Now > From)
		{
			P.Energy = FMath::Min(100.f, P.Energy + 2.f * (Now - From)); // energy.regen_per_s, energy.max
		}
		P.LastUpdate = Now;
		return P;
	}

	// Tipo de receta -> malla de arte (SourceArt/Building/README.md).
	const TMap<FName, FString>& RealMeshNames()
	{
		static const TMap<FName, FString> M = {
			{"Plank", "SM_PART_PLANK"}, {"Block", "SM_PART_BLOCK"}, {"Cylinder", "SM_PART_CYLINDER"}, {"Wall", "SM_PART_WALL"}, {"Panel", "SM_PART_PANEL"},
			{"Girder", "SM_PART_GIRDER"}, {"Ball", "SM_PART_BALL"}, {"Sled", "SM_DEV_SLED"}, {"Hinge", "SM_JOINT_HINGE"}, {"Turntable", "SM_JOINT_TURNTABLE"},
			{"Stake", "SM_ANCHOR_STAKE"}, {"Wheel", "SM_DEV_WHEEL_S"}, {"BigWheel", "SM_DEV_WHEEL_L"}, {"Spring", "SM_DEV_SPRING"}, {"Fan", "SM_DEV_FAN"},
			{"Jet", "SM_DEV_JET"}, {"Turbine", "SM_DEV_TURBINE"}, {"Rocket", "SM_DEV_ROCKET"}, {"Balloon", "SM_DEV_BALLOON"}, {"HoverStone", "SM_DEV_HOVER"},
			{"Wing", "SM_DEV_WING"}, {"Sail", "SM_DEV_SAIL"}, {"Stabilizer", "SM_DEV_STABILIZER"}, {"Seat", "SM_CTRL_SEAT"}, {"Head", "SM_CTRL_HEAD"},
			{"Battery", "SM_PWR_BATTERY_S"}, {"BigBattery", "SM_PWR_BATTERY_L"}, {"Cannon", "SM_DEV_CANNON"}, {"Flamethrower", "SM_DEV_FLAME"}, {"Frost", "SM_DEV_FROST"},
			{"Beam", "SM_DEV_BEAM"}, {"Shock", "SM_DEV_SHOCK"}, {"Bomb", "SM_CTRL_TIMEBOMB"}, {"Hydrant", "SM_DEV_HYDRANT"}, {"Light", "SM_DEV_LIGHT"},
		};
		return M;
	}
}

UStaticMesh* AJamBuildPart::FindRealMesh(FName Type)
{
	if (CVarJamRealMeshes.GetValueOnGameThread() == 0)
	{
		return nullptr;
	}
	static TMap<FName, TWeakObjectPtr<UStaticMesh>> Cache;
	if (const TWeakObjectPtr<UStaticMesh>* Hit = Cache.Find(Type))
	{
		if (Hit->IsValid()) { return Hit->Get(); }
	}
	const FString* Name = RealMeshNames().Find(Type);
	if (!Name)
	{
		return nullptr;
	}
	const FString Pkg = TEXT("/Game/Game/Building/Parts/") + *Name;
	UStaticMesh* SM = FPackageName::DoesPackageExist(Pkg) ? LoadObject<UStaticMesh>(nullptr, *(Pkg + TEXT(".") + *Name)) : nullptr;
	if (SM)
	{
		Cache.Add(Type, SM);
	}
	return SM;
}

int32 AJamBuildPart::EnergyMode()
{
	return CVarJamEnergy.GetValueOnGameThread();
}

float AJamBuildPart::GetPlayerEnergy(const UWorld* World)
{
	return PlayerPool(World).Energy;
}

void AJamBuildPart::SetPlayerEnergy(const UWorld* World, float Value)
{
	PlayerPool(World).Energy = FMath::Clamp(Value, 0.f, 100.f);
}

bool UJamPartCatalog::Find(FName Type, FJamPartDef& OutDef)
{
	for (const FJamPartDef& D : Table())
	{
		if (D.Type == Type)
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

void AJamBuildPart::InitFromDef(const FJamPartDef& Def, const FVector& BoxSize, const FQuat& VisualWorldRot)
{
	PartType = Def.Type;
	Kind = Def.Kind;
	Power = Def.Power;
	EnergyPerSecond = Def.EnergyPerSecond;
	Length = FMath::Max(Def.Size.X, 1.0);

	if (UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, *Def.MeshPath))
	{
		Mesh->SetStaticMesh(SM);
	}
	UStaticMesh* Real = FindRealMesh(Def.Type);

	// Escala nativa (mesh basico 100 cm). Axis 1/2: el eje Z del mesh ya viene rotado por el ensamblador.
	FVector Scale;
	switch (Def.Axis)
	{
	case 1: Scale = FVector(Def.Size.Y, Def.Size.Y, Def.Size.X) / 100.0; break; // eje largo = hacia donde mira
	case 2: Scale = FVector(Def.Size.X, Def.Size.X, Def.Size.Y) / 100.0; break;  // eje = Y (ruedas)
	default: Scale = BoxSize / 100.0; break;
	}
	Mesh->SetRelativeScale3D(Scale);

	// BasicShapeMaterial no tiene parametro "Color"; DebugMeshMaterial si.
	UMaterialInterface* ColorMat = Real ? nullptr : LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/DebugMeshMaterial.DebugMeshMaterial"));
	if (UMaterialInstanceDynamic* MID = ColorMat ? UMaterialInstanceDynamic::Create(ColorMat, this) : nullptr)
	{
		Mesh->SetMaterial(0, MID);
		MID->SetVectorParameterValue(TEXT("Color"), Def.Color);
	}

	if (Real)
	{
		// Malla de arte (caja exacta, +X = hacia donde mira, eje de rueda = Y): solo visual; la fisica sigue en la forma basica oculta.
		UStaticMeshComponent* Visual = NewObject<UStaticMeshComponent>(this);
		Visual->SetupAttachment(Mesh);
		Visual->SetUsingAbsoluteScale(true);
		Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Visual->SetStaticMesh(Real);
		Visual->RegisterComponent();
		Visual->SetWorldRotation(VisualWorldRot);
		Mesh->SetVisibility(false, false);
	}

	if (Kind == EJamPartKind::Slide)
	{
		// ponytail: clase por nombre para no depender del modulo PhysicsCore en GameJamm.Build.cs (archivo de otro rol);
		// si se agrega "PhysicsCore" a las dependencias, usar NewObject<UPhysicalMaterial>(this).
		if (UClass* PMClass = FindObject<UClass>(nullptr, TEXT("/Script/PhysicsCore.PhysicalMaterial")))
		{
			UPhysicalMaterial* PM = static_cast<UPhysicalMaterial*>(NewObject<UObject>(this, PMClass));
			PM->Friction = 0.05f;
			PM->bOverrideFrictionCombineMode = true;
			PM->FrictionCombineMode = EFrictionCombineMode::Min;
			Mesh->SetPhysMaterialOverride(PM);
		}
	}

	Mesh->SetMassOverrideInKg(NAME_None, Def.MassKg, true);
	Mesh->SetSimulatePhysics(true);
	Mesh->SetEnableGravity(true);
}

bool AJamBuildPart::IsMine(const AActor* Other) const
{
	const AJamBuildPart* P = Cast<AJamBuildPart>(Other);
	return P && (P == this || (Build.IsValid() && P->Build == Build));
}

void AJamBuildPart::Nearby(const FVector& Center, float Radius, TArray<UPrimitiveComponent*>& Out) const
{
	FCollisionObjectQueryParams Obj;
	Obj.AddObjectTypesToQuery(ECC_PhysicsBody);
	Obj.AddObjectTypesToQuery(ECC_WorldDynamic);
	Obj.AddObjectTypesToQuery(ECC_Pawn);
	TArray<FOverlapResult> Hits;
	GetWorld()->OverlapMultiByObjectType(Hits, Center, FQuat::Identity, Obj, FCollisionShape::MakeSphere(Radius), FCollisionQueryParams(NAME_None, false, this));
	for (const FOverlapResult& H : Hits)
	{
		if (UPrimitiveComponent* C = H.GetComponent())
		{
			Out.AddUnique(C);
		}
	}
}

void AJamBuildPart::SpawnBall(const FVector& At, const FVector& Velocity)
{
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AStaticMeshActor* Ball = GetWorld()->SpawnActor<AStaticMeshActor>(At, FRotator::ZeroRotator, P))
	{
		Ball->Tags.Add(ProjectileTag);
		UStaticMeshComponent* C = Ball->GetStaticMeshComponent();
		C->SetMobility(EComponentMobility::Movable);
		C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, SphereMesh));
		C->SetWorldScale3D(FVector(0.2));
		C->SetCollisionProfileName(TEXT("PhysicsActor"));
		C->SetSimulatePhysics(true);
		C->SetMassOverrideInKg(NAME_None, 2.f, true);
		C->AddImpulse(Velocity, NAME_None, true);
		Ball->SetLifeSpan(5.f);
	}
}

void AJamBuildPart::Puff(const FVector& At, float SizeCm, const FLinearColor& Color, float Life)
{
	if (CVarJamFx.GetValueOnGameThread() == 0)
	{
		return;
	}
	// ponytail: un actor-esfera por bocanada; sustituir por Niagara (NS_flame, NS_frost...) cuando arte los entregue.
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AStaticMeshActor* A = GetWorld()->SpawnActor<AStaticMeshActor>(At, FRotator::ZeroRotator, P))
	{
		UStaticMeshComponent* C = A->GetStaticMeshComponent();
		C->SetMobility(EComponentMobility::Movable);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, SphereMesh));
		C->SetWorldScale3D(FVector(SizeCm / 100.f));
		UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/DebugMeshMaterial.DebugMeshMaterial"));
		if (UMaterialInstanceDynamic* MID = Mat ? UMaterialInstanceDynamic::Create(Mat, A) : nullptr)
		{
			C->SetMaterial(0, MID);
			MID->SetVectorParameterValue(TEXT("Color"), Color);
		}
		A->SetLifeSpan(Life);
	}
}

void AJamBuildPart::SetActive(bool bNewActive)
{
	const int32 Mode = EnergyMode();
	if (bNewActive && Build.IsValid() && EnergyPerSecond > 0.f && Mode != 0 && Build->Energy <= 0.f && !(Mode == 2 && GetPlayerEnergy(GetWorld()) > 0.f))
	{
		bNewActive = false; // sin energia no enciende
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
	if (Kind == EJamPartKind::Hover && bNewActive)
	{
		HoldZ = GetActorLocation().Z;
	}
	if (DriveConstraint)
	{
		// Ruedas: signo negativo = rodar hacia el frente (+X) de la construccion (verificado en el editor).
		const float Dir = Kind == EJamPartKind::Wheel ? -1.f : 1.f;
		DriveConstraint->SetAngularVelocityTarget(FVector(bNewActive ? Dir * Power : 0.f, 0.f, 0.f) / (2.0 * PI));
	}
	if (Kind == EJamPartKind::Stake)
	{
		// Ancla: constraint fijo entre la pieza y el mundo.
		if (bNewActive && !WorldLock)
		{
			WorldLock = NewObject<UPhysicsConstraintComponent>(this);
			WorldLock->RegisterComponent();
			WorldLock->SetWorldLocation(GetActorLocation());
			WorldLock->SetLinearXLimit(LCM_Locked, 0);
			WorldLock->SetLinearYLimit(LCM_Locked, 0);
			WorldLock->SetLinearZLimit(LCM_Locked, 0);
			WorldLock->SetAngularSwing1Limit(ACM_Locked, 0);
			WorldLock->SetAngularSwing2Limit(ACM_Locked, 0);
			WorldLock->SetAngularTwistLimit(ACM_Locked, 0);
			WorldLock->SetConstrainedComponents(Mesh, NAME_None, nullptr, NAME_None);
		}
		else if (!bNewActive && WorldLock)
		{
			WorldLock->BreakConstraint();
			WorldLock->DestroyComponent();
			WorldLock = nullptr;
		}
	}
	if (Kind == EJamPartKind::Light)
	{
		if (bNewActive && !LightComp)
		{
			USpotLightComponent* L = NewObject<USpotLightComponent>(this);
			L->SetupAttachment(Mesh);
			L->RegisterComponent();
			L->SetWorldRotation(GetActorQuat().RotateVector(LocalDirection).Rotation());
			L->SetIntensityUnits(ELightUnits::Candelas);
			L->SetIntensity(Power);
			L->SetOuterConeAngle(35.f);
			L->SetAttenuationRadius(3000.f);
			LightComp = L;
		}
		if (LightComp)
		{
			LightComp->SetVisibility(bNewActive);
		}
	}
}

void AJamBuildPart::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Mesh)
	{
		return;
	}
	UWorld* World = GetWorld();
	const float Now = World->GetTimeSeconds();

	// Hielo: descongelar lo que ya cumplio su tiempo (aunque el emisor este apagado).
	for (auto It = Frozen.CreateIterator(); It; ++It)
	{
		if (Now >= It.Value())
		{
			if (UPrimitiveComponent* C = It.Key().Get())
			{
				// ponytail: se restaura al damping por defecto de UE, no al valor previo del cuerpo.
				C->SetLinearDamping(0.01f);
				C->SetAngularDamping(0.f);
			}
			It.RemoveCurrent();
		}
	}

	if (CVarJamDebug.GetValueOnGameThread() != 0 && Build.IsValid() && Build->Parts.Num() > 0 && Build->Parts[0] == this && (DebugTimer += DeltaSeconds) >= 1.f)
	{
		DebugTimer = 0.f;
		int32 On = 0;
		double MaxSpin = 0.0;
		for (const TWeakObjectPtr<AJamBuildPart>& P : Build->Parts)
		{
			if (P.IsValid())
			{
				On += P->bActive ? 1 : 0;
				MaxSpin = FMath::Max(MaxSpin, P->Mesh->GetPhysicsAngularVelocityInRadians().Size());
			}
		}
		UE_LOG(LogJamPart, Log, TEXT("state t=%.1f root=%s pos=(%s) vel=(%s) up.z=%.2f spin=%.1f energy=%.1f on=%d/%d"), Now, *PartType.ToString(),
			*GetActorLocation().ToCompactString(), *Mesh->GetPhysicsLinearVelocity().ToCompactString(), GetActorUpVector().Z, MaxSpin, Build->Energy + (EnergyMode() == 2 ? GetPlayerEnergy(World) : 0.f), On, Build->Parts.Num());
	}

	if (!bActive)
	{
		return;
	}

	// Energia: cada pieza encendida gasta de la reserva de su construccion; a cero se apaga todo.
	FxTimer += DeltaSeconds;
	const bool bFx = FxTimer >= 0.06f; // bocanadas visuales: ~16 por segundo por pieza
	if (bFx) { FxTimer = 0.f; }
	if (Build.IsValid() && EnergyPerSecond > 0.f && EnergyMode() != 0)
	{
		// Primero las baterias (o la reserva) de la construccion; despues la barra del jugador.
		const float Need = EnergyPerSecond * DeltaSeconds;
		bool bPaid = true;
		if (Build->Energy >= Need)
		{
			Build->Energy -= Need;
		}
		else if (EnergyMode() == 2 && PlayerPool(World).Energy >= Need)
		{
			FJamPlayerEnergy& Pool = PlayerPool(World);
			Pool.Energy -= Need;
			Pool.LastDrain = Now;
		}
		else
		{
			bPaid = false;
		}
		if (!bPaid)
		{
			UE_LOG(LogJamPart, Log, TEXT("build out of energy: devices off"));
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(7001, 6.f, FColor::Orange, TEXT("Sin energia: los dispositivos se apagaron. La barra del jugador se recarga sola tras 3 s sin gastar (Jam.Energy 100 la llena)."));
			}
			for (const TWeakObjectPtr<AJamBuildPart>& P : Build->Parts)
			{
				if (P.IsValid() && P->EnergyPerSecond > 0.f)
				{
					P->SetActive(false);
				}
			}
			return;
		}
	}

	// Una construccion quieta se duerme y las fuerzas no la despiertan: mientras este encendida, se mantiene despierta.
	Mesh->WakeAllRigidBodies();

	const FVector WorldDir = GetActorQuat().RotateVector(LocalDirection);
	const FVector Nozzle = GetActorLocation() + WorldDir * (Length * 0.5f + 10.f);
	const float BuildMass = Build.IsValid() ? FMath::Max(Build->MassKg, 1.f) : Mesh->GetMass();
	UStaticMeshComponent* RootMesh = (Build.IsValid() && Build->Parts.Num() > 0 && Build->Parts[0].IsValid()) ? Build->Parts[0]->Mesh.Get() : Mesh.Get();

	switch (Kind)
	{
	case EJamPartKind::Wheel:
		// Rueda con padre: la mueve el motor del constraint. Rueda raiz (sin padre): torque directo -> rueda sola.
		if (!DriveConstraint)
		{
			const FVector Axle = GetActorUpVector(); // eje nativo del cilindro
			const FVector W = Mesh->GetPhysicsAngularVelocityInRadians();
			const double Spin = FVector::DotProduct(W, Axle);
			// Gira hasta su velocidad y se mantiene de pie (eje horizontal), como una rueda que ya va rodando.
			FVector Torque = FVector::CrossProduct(Axle, FVector(Axle.X, Axle.Y, 0).GetSafeNormal()) * 60.f - (W - Axle * Spin) * 8.f;
			if (Spin < Power)
			{
				Torque += Axle * 30.f;
			}
			Mesh->AddTorqueInRadians(Torque, NAME_None, true);
		}
		break;
	case EJamPartKind::Fan:
	{
		// Una helice no empuja mas rapido que el aire que mueve: el empuje cae a cero al llegar a su velocidad tope.
		const double TopSpeed = Power > 50000.f ? 2000.0 : 800.0;
		const double Along = FVector::DotProduct(Mesh->GetPhysicsLinearVelocity(), WorldDir);
		Mesh->AddForce(WorldDir * Power * FMath::Clamp(1.0 - Along / TopSpeed, 0.0, 1.0));
		if (bFx && Power > 50000.f) { Puff(GetActorLocation() - WorldDir * (Length * 0.5f + FMath::FRandRange(10.f, 50.f)), 18.f, FLinearColor(0.3f, 0.6f, 1.f)); } // jet/turbina
		break;
	}
	case EJamPartKind::Rocket:
		Mesh->AddForce(WorldDir * Power);
		if (bFx) { Puff(GetActorLocation() - WorldDir * (Length * 0.5f + FMath::FRandRange(10.f, 80.f)) + FMath::VRand() * 8.f, FMath::FRandRange(15.f, 30.f), FLinearColor(1.f, 0.6f, 0.1f), 0.35f); }
		BurnLeft -= DeltaSeconds;
		if (BurnLeft <= 0.f)
		{
			bActive = false;
		}
		break;
	case EJamPartKind::Balloon:
		// Fuego cerca (cualquier lanzallamas) = sube el doble. El globo frena con el aire: sube y avanza despacio.
		Mesh->AddForce(FVector(0, 0, Power * (Now < HeatedUntil ? 2.f : 1.f)));
		RootMesh->AddForce(RootMesh->GetPhysicsLinearVelocity() * -50.f); // el freno va en la raiz: sobre el globo (1 kg) seria inestable
		break;
	case EJamPartKind::Hover:
	{
		// Mantiene ~80 cm sobre el suelo (o la altura de encendido si no hay suelo): PD repartido entre las piedras.
		int32 Stones = 0;
		FCollisionQueryParams Q(NAME_None, false, this);
		if (Build.IsValid())
		{
			for (const TWeakObjectPtr<AJamBuildPart>& P : Build->Parts)
			{
				if (P.IsValid())
				{
					Q.AddIgnoredActor(P.Get());
					Stones += (P->Kind == EJamPartKind::Hover && P->bActive) ? 1 : 0;
				}
			}
		}
		const FVector At = GetActorLocation();
		FHitResult Hit;
		const double TargetZ = World->LineTraceSingleByChannel(Hit, At, At - FVector(0, 0, 1000), ECC_Visibility, Q) ? Hit.ImpactPoint.Z + 80.0 : HoldZ;
		const double Vz = Mesh->GetPhysicsLinearVelocity().Z;
		const double F = BuildMass * (980.0 + (TargetZ - At.Z) * 20.0 - Vz * 8.0) / FMath::Max(Stones, 1);
		RootMesh->AddForce(FVector(0, 0, FMath::Clamp(F, 0.0, (double)Power))); // en la raiz: empujar desde abajo del centro de masa la voltea
		break;
	}
	case EJamPartKind::Stabilizer:
	{
		// Endereza: torque hacia "arriba = +Z" y amortigua cabeceo/alabeo (no el giro en Z).
		FVector W = RootMesh->GetPhysicsAngularVelocityInRadians();
		W.Z = 0;
		// Como aceleracion (no depende de la inercia): estable aunque la construccion sea muy ligera o el frame rate bajo.
		RootMesh->AddTorqueInRadians(FVector::CrossProduct(GetActorUpVector(), FVector::UpVector) * Power - W * Power * 0.2f, NAME_None, true);
		break;
	}
	case EJamPartKind::Flame:
	{
		// Fuego: daña lo ajeno y calienta globos (de cualquier construccion) dentro de la llama.
		const FVector C = Nozzle + WorldDir * 100.f;
		if (bFx)
		{
			const float D = FMath::FRandRange(0.f, 220.f);
			Puff(Nozzle + WorldDir * D + FMath::VRand() * D * 0.12f, 12.f + D * 0.16f, FLinearColor(1.f, FMath::FRandRange(0.15f, 0.6f), 0.02f), 0.22f);
		}
		TArray<UPrimitiveComponent*> Hits;
		Nearby(C, 150.f, Hits);
		for (UPrimitiveComponent* H : Hits)
		{
			AJamBuildPart* P = Cast<AJamBuildPart>(H->GetOwner());
			if (P && P->Kind == EJamPartKind::Balloon)
			{
				P->HeatedUntil = Now + 0.5f;
			}
			if (!IsMine(H->GetOwner()))
			{
				EffectHits += bFx ? 1 : 0;
				UGameplayStatics::ApplyDamage(H->GetOwner(), Power * DeltaSeconds, nullptr, this, nullptr);
			}
		}
		break;
	}
	case EJamPartKind::Wing:
	{
		// Sustentacion: crece con la velocidad hacia adelante (el ala no se rota: su X = frente del vehiculo).
		const double Fwd = FVector::DotProduct(Mesh->GetPhysicsLinearVelocity(), GetActorForwardVector());
		// Con tope y con freno de aire, para que planee en vez de salir disparada.
		Mesh->AddForce(FVector(0, 0, FMath::Clamp(Fwd, 0.0, 900.0) * Power) - Mesh->GetPhysicsLinearVelocity() * 12.f);
		break;
	}
	case EJamPartKind::Sail:
	{
		// ponytail: el viento sopla siempre de popa (hacia el frente de la vela) a 600 cm/s; con un viento global del mapa,
		// cambiar Wind por ese vector y multiplicar por cuanto lo encara la vela.
		const FVector Wind = GetActorForwardVector();
		const double Rel = FMath::Clamp(1.0 - FVector::DotProduct(Mesh->GetPhysicsLinearVelocity(), Wind) / 600.0, 0.0, 1.0);
		Mesh->AddForce(Wind * Power * Rel);
		break;
	}
	case EJamPartKind::Spring:
		FireTimer += DeltaSeconds;
		if (FireTimer >= 1.5f)
		{
			FireTimer = 0.f;
			if (WorldDir.Z < -0.5)
			{
				// Mira al suelo: si lo toca, hace saltar a toda la construccion.
				FCollisionQueryParams Q(NAME_None, false, this);
				FHitResult Hit;
				if (World->LineTraceSingleByChannel(Hit, GetActorLocation(), GetActorLocation() + WorldDir * 70.f, ECC_Visibility, Q) && !IsMine(Hit.GetActor()))
				{
					RootMesh->AddImpulse(FVector(0, 0, Power * BuildMass));
				}
			}
			else
			{
				// Mira hacia otro lado: lanza lo que tenga encima; si mira arriba y no hay nada, se carga una bola.
				const FVector Throw = (WorldDir + RootMesh->GetForwardVector() * 0.7f).GetSafeNormal() * 1100.f;
				TArray<UPrimitiveComponent*> Hits;
				Nearby(Nozzle + WorldDir * 30.f, 45.f, Hits);
				bool bThrew = false;
				for (UPrimitiveComponent* H : Hits)
				{
					if (H->IsSimulatingPhysics() && !IsMine(H->GetOwner()))
					{
						H->AddImpulse(Throw, NAME_None, true);
						bThrew = true;
						++EffectHits;
					}
				}
				if (!bThrew && WorldDir.Z > 0.5)
				{
					SpawnBall(Nozzle + WorldDir * 30.f, Throw);
				}
			}
		}
		break;
	case EJamPartKind::Bomb:
		BurnLeft -= DeltaSeconds;
		if (BurnLeft <= 0.f)
		{
			// Explosion: empuja y daña todo lo cercano, y su onda enciende los dispositivos que alcanza.
			bActive = false;
			const FVector At = GetActorLocation();
			Puff(At, 500.f, FLinearColor(1.f, 0.25f, 0.f), 0.25f);
			Puff(At, 250.f, FLinearColor(1.f, 0.9f, 0.3f), 0.4f);
			TArray<UPrimitiveComponent*> Hits;
			Nearby(At, 500.f, Hits);
			for (UPrimitiveComponent* H : Hits)
			{
				if (H->IsSimulatingPhysics())
				{
					H->AddRadialImpulse(At, 500.f, Power, ERadialImpulseFalloff::RIF_Linear, true);
				}
				if (AJamBuildPart* P = Cast<AJamBuildPart>(H->GetOwner()))
				{
					if (!P->bActive && P->Kind != EJamPartKind::Bomb)
					{
						P->SetActive(true);
					}
				}
				UGameplayStatics::ApplyDamage(H->GetOwner(), 30.f, nullptr, this, nullptr);
			}
			Destroy();
		}
		break;
	case EJamPartKind::Frost:
	{
		// Cono de hielo: frena 3 s (amortigua) cuerpos simulados que no son de esta construccion.
		const FVector C = Nozzle + WorldDir * 150.f;
		if (bFx)
		{
			const float D = FMath::FRandRange(0.f, 280.f);
			Puff(Nozzle + WorldDir * D + FMath::VRand() * D * 0.3f, 14.f + D * 0.12f, FLinearColor(0.6f, 0.95f, 1.f), 0.3f);
		}
		TArray<UPrimitiveComponent*> Hits;
		Nearby(C, 150.f, Hits);
		for (UPrimitiveComponent* H : Hits)
		{
			if (H->IsSimulatingPhysics() && !IsMine(H->GetOwner()))
			{
				EffectHits += bFx ? 1 : 0;
				H->SetLinearDamping(8.f);
				H->SetAngularDamping(8.f);
				Frozen.Add(H, Now + 3.f);
			}
		}
		break;
	}
	case EJamPartKind::Beam:
	{
		// Rayo: linea hacia delante que empuja y daña lo que toca (ignora la propia construccion).
		FHitResult Hit;
		const FVector End = Nozzle + WorldDir * 1500.f;
		FCollisionQueryParams Q(NAME_None, false, this);
		if (Build.IsValid())
		{
			for (const TWeakObjectPtr<AJamBuildPart>& P : Build->Parts)
			{
				if (P.IsValid()) { Q.AddIgnoredActor(P.Get()); }
			}
		}
		const bool bHit = World->LineTraceSingleByChannel(Hit, Nozzle, End, ECC_PhysicsBody, Q);
		DrawDebugLine(World, Nozzle, bHit ? Hit.ImpactPoint : End, FColor::Magenta, false, 0.f, 0, 3.f);
		if (bFx)
		{
			const FVector To = bHit ? Hit.ImpactPoint : End;
			for (int32 i = 1; i <= 6; ++i) { Puff(FMath::Lerp(Nozzle, To, i / 6.f), 9.f, FLinearColor(1.f, 0.05f, 0.4f), 0.08f); }
		}
		if (bHit && Hit.GetComponent())
		{
			EffectHits += bFx ? 1 : 0;
			if (Hit.GetComponent()->IsSimulatingPhysics())
			{
				Hit.GetComponent()->AddForceAtLocation(WorldDir * Power, Hit.ImpactPoint);
			}
			UGameplayStatics::ApplyDamage(Hit.GetActor(), 12.f * DeltaSeconds, nullptr, this, nullptr);
		}
		break;
	}
	case EJamPartKind::Shock:
		// Descarga cada 1 s al cuerpo ajeno mas cercano: daño, lo detiene y, si es una construccion, la apaga entera.
		FireTimer += DeltaSeconds;
		DrawDebugSphere(World, Nozzle, 12.f, 6, FColor::Yellow, false, 0.f);
		if (FireTimer >= 1.f)
		{
			FireTimer = 0.f;
			TArray<UPrimitiveComponent*> Hits;
			Nearby(Nozzle, 300.f, Hits);
			UPrimitiveComponent* Best = nullptr;
			for (UPrimitiveComponent* H : Hits)
			{
				if (!IsMine(H->GetOwner()) && (!Best || FVector::DistSquared(H->GetComponentLocation(), Nozzle) < FVector::DistSquared(Best->GetComponentLocation(), Nozzle)))
				{
					Best = H;
				}
			}
			if (Best)
			{
				DrawDebugLine(World, Nozzle, Best->GetComponentLocation(), FColor::Yellow, false, 0.3f, 0, 5.f);
				for (int32 i = 0; i <= 5; ++i) { Puff(FMath::Lerp(Nozzle, Best->GetComponentLocation(), i / 5.f) + FMath::VRand() * 12.f, 14.f, FLinearColor(1.f, 1.f, 0.2f), 0.3f); }
				++EffectHits;
				UGameplayStatics::ApplyDamage(Best->GetOwner(), Power, nullptr, this, nullptr);
				if (Best->IsSimulatingPhysics())
				{
					Best->SetPhysicsLinearVelocity(FVector::ZeroVector);
				}
				const AJamBuildPart* P = Cast<AJamBuildPart>(Best->GetOwner());
				if (P && P->Build.IsValid())
				{
					for (const TWeakObjectPtr<AJamBuildPart>& O : P->Build->Parts)
					{
						if (O.IsValid()) { O->SetActive(false); }
					}
				}
			}
		}
		break;
	case EJamPartKind::Hydrant:
	{
		// Chorro: empuja cuerpos ajenos y apaga cualquier lanzallamas que moje.
		const FVector C = Nozzle + WorldDir * 150.f;
		if (bFx)
		{
			const float D = FMath::FRandRange(0.f, 250.f);
			Puff(Nozzle + WorldDir * D - FVector(0, 0, D * D * 0.0006f), 16.f, FLinearColor(0.1f, 0.35f, 1.f), 0.25f);
		}
		TArray<UPrimitiveComponent*> Hits;
		Nearby(C, 100.f, Hits);
		for (UPrimitiveComponent* H : Hits)
		{
			AJamBuildPart* P = Cast<AJamBuildPart>(H->GetOwner());
			if (P && P->Kind == EJamPartKind::Flame && P->bActive)
			{
				P->SetActive(false);
			}
			if (H->IsSimulatingPhysics() && !IsMine(H->GetOwner()))
			{
				H->AddForce(WorldDir * Power);
				EffectHits += bFx ? 1 : 0;
			}
		}
		break;
	}
	case EJamPartKind::Head:
	{
		// Cabeza: busca el objetivo ajeno mas cercano (peon, cuerpo fisico u otra construccion) y gira hacia el.
		FireTimer += DeltaSeconds;
		if (FireTimer >= 0.25f || !Target.IsValid())
		{
			FireTimer = 0.f;
			Target = nullptr;
			TArray<UPrimitiveComponent*> Hits;
			Nearby(GetActorLocation(), 2500.f, Hits);
			double BestD = TNumericLimits<double>::Max();
			for (UPrimitiveComponent* H : Hits)
			{
				AActor* A = H->GetOwner();
				const double D = FVector::DistSquared(H->GetComponentLocation(), GetActorLocation());
				if (A && D < BestD && !IsMine(A) && !A->ActorHasTag(ProjectileTag) && (A->IsA<APawn>() || H->IsSimulatingPhysics()))
				{
					BestD = D;
					Target = A;
				}
			}
		}
		if (Target.IsValid())
		{
			const FVector Axis = GetActorUpVector();
			const FVector To = FVector::VectorPlaneProject(Target->GetActorLocation() - GetActorLocation(), Axis).GetSafeNormal();
			const FVector Fwd = FVector::VectorPlaneProject(WorldDir, Axis).GetSafeNormal();
			const double Err = FMath::Atan2(FVector::DotProduct(FVector::CrossProduct(Fwd, To), Axis), FVector::DotProduct(Fwd, To));
			const double Spin = FVector::DotProduct(Mesh->GetPhysicsAngularVelocityInRadians(), Axis);
			Mesh->AddTorqueInRadians(Axis * (Err * Power - Spin * Power * 0.17), NAME_None, true);
			DrawDebugLine(World, GetActorLocation(), Target->GetActorLocation(), FColor::White, false, 0.f, 0, 1.f);
		}
		break;
	}
	case EJamPartKind::Cannon:
		FireTimer += DeltaSeconds;
		if (FireTimer >= 1.5f)
		{
			FireTimer = 0.f;
			SpawnBall(Nozzle + WorldDir * 15.f, WorldDir * Power);
		}
		break;
	default:
		break;
	}
}
