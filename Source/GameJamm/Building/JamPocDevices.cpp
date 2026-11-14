// PoC de dispositivos: comandos de consola para ver cada mecánica en PIE sin assets nuevos.
// No define Jam.Build, Jam.Inv*, Jam.Clear ni Jam.Prompts (son de la rama de integración).
#include "Building/JamPocDevices.h"

#include "Building/JamBuildPart.h"
#include "Building/JamDeviceMath.h"
#include "Building/JamEnergyComponent.h"
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
