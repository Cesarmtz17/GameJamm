#include "Building/JamPlayConsole.h"

#include "Building/JamBuildPart.h"
#include "Building/JamBuildSpawner.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"

AJamTestCharacter::AJamTestCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0, 0, 64));
	Camera->bUsePawnControlRotation = true;
	bUseControllerRotationYaw = true;
	GetCharacterMovement()->MaxWalkSpeed = 700.f;
	GetCharacterMovement()->JumpZVelocity = 600.f;
}

void AJamTestCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}
	// ponytail: teclas leidas directo (sin Enhanced Input ni assets); cambiar por el personaje real del juego cuando exista.
	float DX = 0.f, DY = 0.f;
	PC->GetInputMouseDelta(DX, DY);
	FRotator R = PC->GetControlRotation();
	R.Yaw += DX * MouseSensitivity;
	R.Pitch = FMath::Clamp(FRotator::NormalizeAxis(R.Pitch) + DY * MouseSensitivity, -85.f, 85.f);
	PC->SetControlRotation(R);

	const FRotator Yaw(0, R.Yaw, 0);
	AddMovementInput(Yaw.Vector(), (PC->IsInputKeyDown(EKeys::W) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::S) ? 1.f : 0.f));
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), (PC->IsInputKeyDown(EKeys::D) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::A) ? 1.f : 0.f));
	if (PC->WasInputKeyJustPressed(EKeys::SpaceBar))
	{
		Jump();
	}
}

AJamTestGameMode::AJamTestGameMode()
{
	DefaultPawnClass = AJamTestCharacter::StaticClass();
}

namespace
{
	const FName SayTag(TEXT("JamSay"));

	void Screen(const FString& Msg, FColor Color = FColor::Yellow)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 8.f, Color, Msg);
		}
	}

	void JamSay(const TArray<FString>& Args, UWorld* World)
	{
		const FString Text = FString::Join(Args, TEXT(" ")).TrimStartAndEnd();
		if (!World || Text.IsEmpty())
		{
			Screen(TEXT("Uso: Jam.Say <que quieres construir>   (ej. Jam.Say un carro con cohete)"));
			return;
		}
		// ~6 m delante del jugador, sobre el suelo, de perfil (su frente apunta a la derecha del jugador: no le dispara ni lo atropella).
		FVector Loc = FVector::ZeroVector;
		float Yaw = 0.f;
		const APlayerController* PC = World->GetFirstPlayerController();
		if (PC)
		{
			FRotator ViewRot;
			PC->GetPlayerViewPoint(Loc, ViewRot);
			Yaw = ViewRot.Yaw;
			Loc += FRotator(0, Yaw, 0).Vector() * 600.f;
			FHitResult Hit;
			FCollisionQueryParams Q(NAME_None, false, PC->GetPawn());
			if (World->LineTraceSingleByChannel(Hit, Loc + FVector(0, 0, 300), Loc - FVector(0, 0, 3000), ECC_Visibility, Q))
			{
				Loc = Hit.ImpactPoint;
			}
		}
		const FTransform At(FRotator(0, Yaw + 90.f, 0), Loc + FVector(0, 0, 10));
		if (AJamBuildSpawner* S = World->SpawnActorDeferred<AJamBuildSpawner>(AJamBuildSpawner::StaticClass(), At, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			S->Prompt = Text;
			S->bAnnounceOnScreen = true;
			S->Tags.Add(SayTag);
			S->FinishSpawning(At); // BeginPlay -> BuildNow: Jev (o respaldo) -> receta -> ensamblador -> activar
		}
	}

	void JamClear(const TArray<FString>&, UWorld* World)
	{
		if (!World)
		{
			return;
		}
		int32 N = 0;
		for (TActorIterator<AJamBuildSpawner> It(World); It; ++It)
		{
			It->ClearBuild();
			if (It->ActorHasTag(SayTag)) { It->Destroy(); }
		}
		for (TActorIterator<AJamBuildPart> It(World); It; ++It)
		{
			It->Destroy();
			++N;
		}
		Screen(FString::Printf(TEXT("Jam.Clear: construcciones borradas (%d piezas sueltas)"), N));
	}

	void JamSeat(const TArray<FString>&, UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		ACharacter* Me = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
		if (!Me)
		{
			Screen(TEXT("Jam.Seat: no hay personaje (usa PIE normal con AJamTestGameMode)"), FColor::Red);
			return;
		}
		if (Cast<AJamBuildPart>(Me->GetAttachParentActor()))
		{
			Me->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			Me->SetActorRotation(FRotator(0, Me->GetActorRotation().Yaw, 0));
			Me->AddActorWorldOffset(FVector(0, 150, 100));
			Me->SetActorEnableCollision(true);
			Me->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			Screen(TEXT("Jam.Seat: te bajaste"));
			return;
		}
		// El asiento mas cercano; si no hay asiento, la pieza mas cercana.
		AJamBuildPart* Best = nullptr;
		double BestD = FMath::Square(3000.0);
		for (TActorIterator<AJamBuildPart> It(World); It; ++It)
		{
			const double D = FVector::DistSquared(It->GetActorLocation(), Me->GetActorLocation()) * (It->Kind == EJamPartKind::Seat ? 0.01 : 1.0);
			if (D < BestD)
			{
				BestD = D;
				Best = *It;
			}
		}
		if (!Best)
		{
			Screen(TEXT("Jam.Seat: no hay ninguna construccion a menos de 30 m"), FColor::Red);
			return;
		}
		// ponytail: solo "va montado" (pegado al asiento); dirigir con WASD queda para el Seat real (research 13 §5).
		Me->GetCharacterMovement()->SetMovementMode(MOVE_None);
		Me->SetActorEnableCollision(false);
		Me->SetActorLocation(Best->GetActorLocation() + FVector(0, 0, 110));
		Me->AttachToActor(Best, FAttachmentTransformRules::KeepWorldTransform);
		Screen(FString::Printf(TEXT("Jam.Seat: montado en %s (Jam.Seat otra vez para bajar)"), *Best->PartType.ToString()));
	}

	FAutoConsoleCommandWithWorldAndArgs CmdSay(TEXT("Jam.Say"), TEXT("Jam.Say <texto libre>: construye lo que pidas delante del jugador (modelo de decision Jev; respaldo por palabras clave)."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamSay));
	FAutoConsoleCommandWithWorldAndArgs CmdClear(TEXT("Jam.Clear"), TEXT("Borra todas las construcciones."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamClear));
	FAutoConsoleCommandWithWorldAndArgs CmdSeat(TEXT("Jam.Seat"), TEXT("Sube al asiento de la construccion mas cercana, o baja."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamSeat));
}
