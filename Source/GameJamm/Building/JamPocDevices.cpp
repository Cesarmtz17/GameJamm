// PoC de dispositivos: comandos de consola para ver cada mecánica en PIE sin assets nuevos.
// No define Jam.Build, Jam.Inv*, Jam.Clear ni Jam.Prompts (son de la rama de integración).
#include "Building/JamPocDevices.h"

#include "Building/BuildRecipe.h"
#include "Building/JamAssembler.h"
#include "Building/JamBuildPart.h"
#include "Building/JamDeviceMath.h"
#include "Building/JamEnergyComponent.h"
#include "Building/JamProjectile.h"
#include "Building/JamSlowComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HealthComponent.h"
#include "Core/JamStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"

// Con nombre (no anónimo) para no chocar con helpers de otros .cpp en builds unity.
namespace JamPocDevicesPrivate
{
	TAutoConsoleVariable<int32> CVarJamDebug(
		TEXT("Jam.Debug"), 1,
		TEXT("1 = dibuja conos de llama/hielo, ejes de empuje, torque de rueda y objetivo de Head. 0 = nada."),
		ECVF_Default);

	void Say(const FString& Text, const FColor& Color = FColor::Green)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 6.f, Color, Text);
		}
		UE_LOG(LogTemp, Log, TEXT("[JamPoc] %s"), *Text);
	}

	APlayerController* FirstPlayer(const UWorld* World)
	{
		return World ? World->GetFirstPlayerController() : nullptr;
	}

	/** Pieza más cercana al jugador (cualquier tipo). */
	AJamBuildPart* NearestPart(UWorld* World)
	{
		APlayerController* PC = FirstPlayer(World);
		if (!PC)
		{
			return nullptr;
		}
		FVector ViewLocation;
		FRotator ViewRotation;
		PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
		return AJamBuildPart::FindNearestSeat(World, ViewLocation, 1000000.f, true);
	}

	void CmdDummy(const TArray<FString>& Args, UWorld* World)
	{
		APlayerController* PC = FirstPlayer(World);
		if (!PC)
		{
			Say(TEXT("Jam.Dummy: no hay jugador (usar en PIE)"), FColor::Red);
			return;
		}
		const int32 Count = FMath::Clamp(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 1, 1, 20);

		FVector ViewLocation;
		FRotator ViewRotation;
		PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
		const FRotator Yaw(0.0, ViewRotation.Yaw, 0.0);
		const FVector Forward = Yaw.Vector();
		const FVector Right = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y);

		// Equipo contrario al del jugador; si el jugador es Neutral (sin UHealthComponent), los blancos son Red.
		const EJamTeam PlayerTeam = UJamStatics::GetActorTeam(PC);
		const EJamTeam DummyTeam = PlayerTeam == EJamTeam::Neutral ? EJamTeam::Red : UJamStatics::GetOpposingTeam(PlayerTeam);

		int32 Spawned = 0;
		for (int32 i = 0; i < Count; ++i)
		{
			FVector At = ViewLocation + Forward * 800.0 + Right * ((i - (Count - 1) * 0.5) * 250.0);
			FHitResult Ground;
			if (World->LineTraceSingleByChannel(Ground, At + FVector(0, 0, 500), At - FVector(0, 0, 5000), ECC_Visibility))
			{
				At = Ground.ImpactPoint + FVector(0, 0, 95);
			}
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AJamPocDummy* Dummy = World->SpawnActor<AJamPocDummy>(At, Yaw, Params))
			{
				Dummy->Health->Team = DummyTeam;
				++Spawned;
			}
		}
		Say(FString::Printf(TEXT("Jam.Dummy: %d blancos (equipo %s) a 8 m. Vida encima de cada uno."), Spawned,
			DummyTeam == EJamTeam::Red ? TEXT("Red") : TEXT("Blue")));
	}

	void CmdEnergy(const TArray<FString>& Args, UWorld* World)
	{
		APlayerController* PC = FirstPlayer(World);
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Pawn)
		{
			Say(TEXT("Jam.Energy: el jugador no tiene Pawn (usar en PIE)"), FColor::Red);
			return;
		}
		UJamEnergyComponent* Energy = Pawn->FindComponentByClass<UJamEnergyComponent>();
		if (!Energy)
		{
			Energy = NewObject<UJamEnergyComponent>(Pawn);
			Energy->RegisterComponent();
		}
		Energy->bDebugOnScreen = true;

		if (Args.Num() > 0)
		{
			if (Args[0].Equals(TEXT("full"), ESearchCase::IgnoreCase))
			{
				Energy->SetEnergy(Energy->MaxEnergy);
			}
			else if (Args[0].Equals(TEXT("empty"), ESearchCase::IgnoreCase))
			{
				Energy->RemoveReserve(Energy->GetReserve());
				Energy->SetEnergy(0.f);
			}
			else
			{
				Energy->SetEnergy(FCString::Atof(*Args[0]));
			}
		}

		// Las construcciones sin dueño pasan a ser del jugador: así gastan de su barra y usan su equipo.
		int32 Adopted = 0;
		for (TActorIterator<AJamBuildPart> It(World); It; ++It)
		{
			if (!It->GetOwner())
			{
				It->SetOwner(Pawn);
				++Adopted;
			}
		}
		Say(FString::Printf(TEXT("Jam.Energy: barra %.1f/%.0f, reserva %.1f. %d piezas sin dueno ahora gastan de tu barra."),
			Energy->GetEnergy(), Energy->MaxEnergy, Energy->GetReserve(), Adopted));
	}

	void CmdSeat(const TArray<FString>& Args, UWorld* World)
	{
		APlayerController* PC = FirstPlayer(World);
		if (!PC)
		{
			Say(TEXT("Jam.Seat: no hay jugador (usar en PIE)"), FColor::Red);
			return;
		}
		APawn* Pawn = PC->GetPawn();
		AActor* Rider = Pawn ? static_cast<AActor*>(Pawn) : static_cast<AActor*>(PC);

		// Si ya va montado: bajarse.
		for (TActorIterator<AJamBuildPart> It(World); It; ++It)
		{
			if (It->IsMounted() && It->GetRider() == Rider)
			{
				It->Dismount(false);
				It->bPocKeyboardInput = false;
				PC->ResetIgnoreMoveInput();
				Say(TEXT("Jam.Seat: te bajaste; la construccion se apago."));
				return;
			}
		}

		AJamBuildPart* Near = NearestPart(World);
		if (!Near)
		{
			Say(TEXT("Jam.Seat: no hay ninguna construccion (AJamBuildPart) en el mundo"), FColor::Red);
			return;
		}
		// El Seat de esa construcción; si no trae, se monta sobre la raíz (el asiento es infinito).
		AJamBuildPart* Seat = Near->GetBuildRoot();
		const TArray<AJamBuildPart*> Parts = Near->GetBuildParts();
		for (AJamBuildPart* Part : Parts)
		{
			if (Part->Kind == EJamPartKind::Seat)
			{
				Seat = Part;
				break;
			}
		}
		Seat->bPocKeyboardInput = true;
		if (!Seat->Mount(Rider, Pawn != nullptr))
		{
			Seat->bPocKeyboardInput = false;
			Say(TEXT("Jam.Seat: no se pudo montar (ya hay conductor?)"), FColor::Red);
			return;
		}
		PC->SetIgnoreMoveInput(true);
		Say(FString::Printf(TEXT("Jam.Seat: montado en '%s' (%d piezas, todo encendido). W/S acelera, A/D gira, Space/Ctrl sube/baja. Jam.Seat otra vez para bajar."),
			*Seat->PartType.ToString(), Parts.Num()));
	}

	void CmdPower(const TArray<FString>& Args, UWorld* World)
	{
		AJamBuildPart* Near = NearestPart(World);
		if (!Near)
		{
			Say(TEXT("Jam.Power: no hay ninguna construccion en el mundo"), FColor::Red);
			return;
		}
		const bool bOn = Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : true;
		Near->SetBuildPower(bOn);
		Say(FString::Printf(TEXT("Jam.Power: construccion mas cercana (%d piezas) %s"), Near->GetBuildParts().Num(), bOn ? TEXT("ENCENDIDA") : TEXT("apagada")));
	}

	/** Construcciones de ejemplo, una por mecánica (recetas en el formato JSON compartido). */
	struct FPreset
	{
		const TCHAR* Name;
		const TCHAR* Help;
		const TCHAR* Json;
		bool bFlat;        // acostada: toda la construcción rotada 90° sobre X
		bool bActivate;    // encender al aparecer (las de asiento esperan a Jam.Seat)
	};

	const FPreset Presets[] = {
		{ TEXT("wheel"), TEXT("rueda raiz parada con 2 lanzallamas: rueda sola y las llamas giran con ella (Roller)"),
			TEXT(R"({"name":"Rueda Infernal","parts":[{"id":"w","type":"BigWheel"},{"id":"f1","type":"Flamethrower","attach_to":"w","socket":"Front"},{"id":"f2","type":"Flamethrower","attach_to":"w","socket":"Back"}],"activate":["w","f1","f2"]})"),
			false, true },
		{ TEXT("spinner"), TEXT("la misma rueda acostada: gira como trompo y barre un anillo de fuego"),
			TEXT(R"({"name":"Spinner","parts":[{"id":"w","type":"BigWheel"},{"id":"f1","type":"Flamethrower","attach_to":"w","socket":"Front"},{"id":"f2","type":"Flamethrower","attach_to":"w","socket":"Back"}],"activate":["w","f1","f2"]})"),
			true, true },
		{ TEXT("flame"), TEXT("bloque clavado con lanzallamas al frente: cono con dano en el tiempo"),
			TEXT(R"({"name":"Lanzallamas","parts":[{"id":"b","type":"Block"},{"id":"s","type":"Stake","attach_to":"b","socket":"Bottom"},{"id":"f","type":"Flamethrower","attach_to":"b","socket":"Front"}],"activate":["s","f"]})"),
			false, true },
		{ TEXT("frost"), TEXT("bloque clavado con emisor de hielo: lento 50% por 3 s"),
			TEXT(R"({"name":"Hielo","parts":[{"id":"b","type":"Block"},{"id":"s","type":"Stake","attach_to":"b","socket":"Bottom"},{"id":"f","type":"Frost","attach_to":"b","socket":"Front"}],"activate":["s","f"]})"),
			false, true },
		{ TEXT("beam"), TEXT("bloque clavado con laser: dano continuo en linea"),
			TEXT(R"({"name":"Laser","parts":[{"id":"b","type":"Block"},{"id":"s","type":"Stake","attach_to":"b","socket":"Bottom"},{"id":"f","type":"Beam","attach_to":"b","socket":"Front"}],"activate":["s","f"]})"),
			false, true },
		{ TEXT("cannon"), TEXT("bloque clavado con canon al frente: una bala con dano cada 1.5 s"),
			TEXT(R"({"name":"Canon","parts":[{"id":"b","type":"Block"},{"id":"s","type":"Stake","attach_to":"b","socket":"Bottom"},{"id":"c","type":"Cannon","attach_to":"b","socket":"Front"}],"activate":["s","c"]})"),
			false, true },
		{ TEXT("turret"), TEXT("torreta autonoma: Head sobre un bloque clavado y canon encima; apunta sola al blanco mas cercano"),
			TEXT(R"({"name":"Torreta","parts":[{"id":"b","type":"Block"},{"id":"s","type":"Stake","attach_to":"b","socket":"Bottom"},{"id":"h","type":"Head","attach_to":"b","socket":"Top"},{"id":"c","type":"Cannon","attach_to":"h","socket":"Top"}],"activate":["s","h","c"]})"),
			false, true },
		{ TEXT("turntable"), TEXT("aspersor: plataforma giratoria con lanzallamas encima"),
			TEXT(R"({"name":"Aspersor","parts":[{"id":"b","type":"Block"},{"id":"s","type":"Stake","attach_to":"b","socket":"Bottom"},{"id":"t","type":"Turntable","attach_to":"b","socket":"Top"},{"id":"f","type":"Flamethrower","attach_to":"t","socket":"Top"}],"activate":["s","t","f"]})"),
			false, true },
		{ TEXT("catapult"), TEXT("resorte sobre una tabla con un bloque encima: al encenderse lanza el bloque"),
			TEXT(R"({"name":"Catapulta","parts":[{"id":"p","type":"Plank"},{"id":"s","type":"Spring","attach_to":"p","socket":"Top"},{"id":"b","type":"Block","attach_to":"s","socket":"Top"}],"activate":["s"]})"),
			false, true },
		{ TEXT("balloon"), TEXT("globo con lastre que solo sube cuando el lanzallamas lo calienta (x2)"),
			TEXT(R"({"name":"Globo","parts":[{"id":"p","type":"Plank"},{"id":"k","type":"Cylinder","attach_to":"p","socket":"Bottom"},{"id":"g","type":"Balloon","attach_to":"p","socket":"Top_Front"},{"id":"f","type":"Flamethrower","attach_to":"p","socket":"Top_Back"}],"activate":["g","f"]})"),
			false, true },
		{ TEXT("car"), TEXT("carro con asiento, 4 ruedas, ventilador y bateria: usar Jam.Seat para manejarlo"),
			TEXT(R"({"name":"Carro","parts":[{"id":"p","type":"Plank"},{"id":"w0","type":"Wheel","attach_to":"p","socket":"Left_Front"},{"id":"w1","type":"Wheel","attach_to":"p","socket":"Right_Front"},{"id":"w2","type":"Wheel","attach_to":"p","socket":"Left_Back"},{"id":"w3","type":"Wheel","attach_to":"p","socket":"Right_Back"},{"id":"seat","type":"Seat","attach_to":"p","socket":"Top"},{"id":"fan","type":"Fan","attach_to":"p","socket":"Back"},{"id":"bat","type":"Battery","attach_to":"p","socket":"Top_Back"}],"activate":[]})"),
			false, false },
		{ TEXT("hover"), TEXT("tabla con 2 ventiladores abajo y asiento: usar Jam.Seat; flota, W inclina y avanza, A/D gira, Space/Ctrl sube/baja"),
			TEXT(R"({"name":"Hover","parts":[{"id":"p","type":"Plank"},{"id":"f0","type":"Fan","attach_to":"p","socket":"Bottom_Front"},{"id":"f1","type":"Fan","attach_to":"p","socket":"Bottom_Back"},{"id":"seat","type":"Seat","attach_to":"p","socket":"Top"}],"activate":[]})"),
			false, false },
	};

	FString PresetNames()
	{
		FString Names;
		for (const FPreset& P : Presets)
		{
			Names += (Names.IsEmpty() ? TEXT("") : TEXT(", ")) + FString(P.Name);
		}
		return Names;
	}

	void CmdDevice(const TArray<FString>& Args, UWorld* World)
	{
		APlayerController* PC = FirstPlayer(World);
		if (!PC)
		{
			Say(TEXT("Jam.Device: no hay jugador (usar en PIE)"), FColor::Red);
			return;
		}
		const FPreset* Preset = nullptr;
		for (const FPreset& P : Presets)
		{
			if (Args.Num() > 0 && Args[0].Equals(P.Name, ESearchCase::IgnoreCase))
			{
				Preset = &P;
				break;
			}
		}
		if (!Preset)
		{
			Say(TEXT("Jam.Device <") + PresetNames() + TEXT(">"), FColor::Yellow);
			return;
		}

		FBuildRecipe Recipe;
		FString Error;
		if (!UBuildRecipeLibrary::ParseRecipeJson(Preset->Json, Recipe, Error))
		{
			Say(TEXT("Jam.Device: receta invalida: ") + Error, FColor::Red);
			return;
		}

		// 4.5 m delante de la cámara, mirando hacia donde mira el jugador (los blancos de Jam.Dummy quedan a 8 m).
		FVector ViewLocation;
		FRotator ViewRotation;
		PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
		const FRotator Yaw(0.0, ViewRotation.Yaw, 0.0);
		FVector At = ViewLocation + Yaw.Vector() * 450.0;
		FHitResult Ground;
		if (World->LineTraceSingleByChannel(Ground, At + FVector(0, 0, 500), At - FVector(0, 0, 5000), ECC_Visibility))
		{
			At = Ground.ImpactPoint + FVector(0, 0, 10);
		}
		FQuat Rotation = Yaw.Quaternion();
		if (Preset->bFlat)
		{
			Rotation = Rotation * FQuat(FVector::ForwardVector, HALF_PI);
			At += FVector(0, 0, 60);
		}

		TArray<AJamBuildPart*> Parts;
		if (!UJamAssembler::Assemble(World, Recipe, FTransform(Rotation, At), Parts, Error) || Parts.Num() == 0)
		{
			Say(TEXT("Jam.Device: no se pudo armar: ") + Error, FColor::Red);
			return;
		}
		// Del jugador: usa su equipo y, si tiene UJamEnergyComponent (Jam.Energy), gasta de su barra.
		if (APawn* Pawn = PC->GetPawn())
		{
			Parts[0]->SetBuildOwner(Pawn);
		}
		if (Preset->bActivate)
		{
			UJamAssembler::SetBuildActive(Parts, Recipe.Activate, true);
		}
		Say(FString::Printf(TEXT("Jam.Device %s: %s (%d piezas)%s"), Preset->Name, Preset->Help, Parts.Num(),
			Preset->bActivate ? TEXT("") : TEXT(" -> ahora Jam.Seat")));
	}

	void CmdDeviceClear(const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}
		if (APlayerController* PC = FirstPlayer(World))
		{
			PC->ResetIgnoreMoveInput();
		}
		int32 Count = 0;
		TArray<AActor*> Doomed;
		for (TActorIterator<AJamBuildPart> It(World); It; ++It) { Doomed.Add(*It); }
		for (TActorIterator<AJamPocDummy> It(World); It; ++It) { Doomed.Add(*It); }
		for (TActorIterator<AJamProjectile> It(World); It; ++It) { Doomed.Add(*It); }
		for (AActor* Actor : Doomed)
		{
			if (IsValid(Actor))
			{
				Actor->Destroy();
				++Count;
			}
		}
		Say(FString::Printf(TEXT("Jam.DeviceClear: %d actores del PoC destruidos"), Count));
	}

	FAutoConsoleCommandWithWorldAndArgs CmdDeviceRegistration(
		TEXT("Jam.Device"), TEXT("Jam.Device <preset>: arma delante de la camara una construccion de ejemplo (wheel, spinner, flame, frost, beam, cannon, turret, turntable, catapult, balloon, car, hover)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CmdDevice));

	FAutoConsoleCommandWithWorldAndArgs CmdDeviceClearRegistration(
		TEXT("Jam.DeviceClear"), TEXT("Jam.DeviceClear: destruye todas las piezas, blancos y balas del PoC."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CmdDeviceClear));

	FAutoConsoleCommandWithWorldAndArgs CmdDummyRegistration(
		TEXT("Jam.Dummy"), TEXT("Jam.Dummy [n]: spawnea n blancos de prueba con vida (equipo enemigo) 8 m delante de la camara."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CmdDummy));

	FAutoConsoleCommandWithWorldAndArgs CmdEnergyRegistration(
		TEXT("Jam.Energy"), TEXT("Jam.Energy [valor|full|empty]: fija la energia del jugador y muestra la barra y el gasto por segundo."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CmdEnergy));

	FAutoConsoleCommandWithWorldAndArgs CmdSeatRegistration(
		TEXT("Jam.Seat"), TEXT("Jam.Seat: monta (o desmonta) la construccion mas cercana y la dirige con WASD/flechas, Space/Ctrl."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CmdSeat));

	FAutoConsoleCommandWithWorldAndArgs CmdPowerRegistration(
		TEXT("Jam.Power"), TEXT("Jam.Power [0|1]: prende o apaga todos los dispositivos de la construccion mas cercana."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CmdPower));
}

bool JamDevices::IsDebugDrawEnabled()
{
	return JamPocDevicesPrivate::CVarJamDebug.GetValueOnGameThread() != 0;
}

void JamPoc::ReadDriveKeys(const UWorld* World, float& OutThrottle, float& OutSteer, float& OutLift)
{
	const APlayerController* PC = JamPocDevicesPrivate::FirstPlayer(World);
	if (!PC)
	{
		return;
	}
	auto Axis = [PC](const FKey& Plus, const FKey& PlusAlt, const FKey& Minus, const FKey& MinusAlt)
	{
		float Value = 0.f;
		if (PC->IsInputKeyDown(Plus) || PC->IsInputKeyDown(PlusAlt)) { Value += 1.f; }
		if (PC->IsInputKeyDown(Minus) || PC->IsInputKeyDown(MinusAlt)) { Value -= 1.f; }
		return Value;
	};
	OutThrottle = Axis(EKeys::W, EKeys::Up, EKeys::S, EKeys::Down);
	OutSteer = Axis(EKeys::D, EKeys::Right, EKeys::A, EKeys::Left);
	OutLift = Axis(EKeys::SpaceBar, EKeys::SpaceBar, EKeys::LeftControl, EKeys::C);
}

// ---------------------------------------------------------------------------------------------
// Blanco de prueba
// ---------------------------------------------------------------------------------------------

AJamPocDummy::AJamPocDummy()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Mesh->SetRelativeScale3D(FVector(0.6, 0.6, 1.8));

	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	Health->MaxHealth = 100.f;
	Health->Team = EJamTeam::Red;
	Health->bDestroyOwnerOnDeath = true;
}

void AJamPocDummy::BeginPlay()
{
	Super::BeginPlay();
	if (UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
	{
		Mesh->SetStaticMesh(SM);
	}
	Home = GetActorLocation();
	PatrolAxis = GetActorRightVector();
	Phase = FMath::FRandRange(0.f, 6.28f);
	Health->OnHealthChanged.AddDynamic(this, &AJamPocDummy::HandleHealthChanged);
}

void AJamPocDummy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// DeltaSeconds ya viene multiplicado por CustomTimeDilation: con Frost el paseo va a la mitad.
	if (bPatrol)
	{
		Phase += DeltaSeconds * 1.5f;
		SetActorLocation(Home + PatrolAxis * (FMath::Sin(Phase) * 150.f));
	}
	const bool bSlowed = UJamSlowComponent::IsSlowed(this);
	const FString Text = FString::Printf(TEXT("%s %.0f/%.0f%s"),
		Health->Team == EJamTeam::Red ? TEXT("RED") : (Health->Team == EJamTeam::Blue ? TEXT("BLUE") : TEXT("NEUTRAL")),
		Health->GetHealth(), Health->MaxHealth, bSlowed ? TEXT("  LENTO") : TEXT(""));
	DrawDebugString(GetWorld(), GetActorLocation() + FVector(0, 0, 130), Text, nullptr, bSlowed ? FColor::Cyan : FColor::White, 0.f, true);
}

void AJamPocDummy::HandleHealthChanged(UHealthComponent* HealthComponent, float NewHealth, float Delta)
{
	if (GEngine)
	{
		const FString Text = NewHealth <= 0.f
			? FString::Printf(TEXT("Blanco %s DESTRUIDO"), *GetName())
			: FString::Printf(TEXT("Blanco %s: %+.1f -> %.1f/%.0f"), *GetName(), Delta, NewHealth, HealthComponent ? HealthComponent->MaxHealth : 0.f);
		GEngine->AddOnScreenDebugMessage(static_cast<int32>(GetUniqueID()), 2.f, NewHealth <= 0.f ? FColor::Red : FColor::Orange, Text);
	}
}
