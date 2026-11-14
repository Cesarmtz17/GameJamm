#include "Building/JamPlayConsole.h"

#include "Building/JamBuildPart.h"
#include "Building/JamBuildSpawner.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/Ticker.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
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

	/** ~6 m delante del jugador, sobre el suelo, de perfil (su frente apunta a la derecha del jugador: no le dispara ni lo atropella). */
	FTransform SayTransform(UWorld* World)
	{
		FVector Loc = FVector::ZeroVector;
		float Yaw = 0.f;
		if (const APlayerController* PC = World->GetFirstPlayerController())
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
		return FTransform(FRotator(0, Yaw + 90.f, 0), Loc + FVector(0, 0, 10));
	}

	AJamBuildSpawner* SpawnSay(UWorld* World, const FString& Text, const FTransform& At)
	{
		AJamBuildSpawner* S = World->SpawnActorDeferred<AJamBuildSpawner>(AJamBuildSpawner::StaticClass(), At, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (S)
		{
			S->Prompt = Text;
			S->bAnnounceOnScreen = true;
			S->Tags.Add(SayTag);
			S->FinishSpawning(At); // BeginPlay -> BuildNow: Jev (o respaldo) -> receta -> ensamblador -> activar
		}
		return S;
	}

	void JamSay(const TArray<FString>& Args, UWorld* World)
	{
		const FString Text = FString::Join(Args, TEXT(" ")).TrimStartAndEnd();
		if (!World || Text.IsEmpty())
		{
			Screen(TEXT("Uso: Jam.Say <que quieres construir>   (ej. Jam.Say un carro con cohete)"));
			return;
		}
		SpawnSay(World, Text, SayTransform(World));
	}

	void ClearAll(UWorld* World)
	{
		for (TActorIterator<AJamBuildSpawner> It(World); It; ++It)
		{
			It->ClearBuild();
			if (It->ActorHasTag(SayTag)) { It->Destroy(); }
		}
		for (TActorIterator<AJamBuildPart> It(World); It; ++It)
		{
			It->Destroy();
		}
	}

	void JamClear(const TArray<FString>&, UWorld* World)
	{
		if (World)
		{
			ClearAll(World);
			Screen(TEXT("Jam.Clear: construcciones borradas"));
		}
	}

	void JamEnergy(const TArray<FString>& Args, UWorld* World)
	{
		if (!World) { return; }
		if (Args.Num() > 0) { AJamBuildPart::SetPlayerEnergy(World, FCString::Atof(*Args[0])); }
		Screen(FString::Printf(TEXT("Energia del jugador: %.0f / 100  (modo jam.Build.Energy=%d: 2 = barra del jugador, 1 = por construccion, 0 = infinita)"),
			AJamBuildPart::GetPlayerEnergy(World), AJamBuildPart::EnergyMode()));
	}

	// ---- Jam.Test: bateria de frases -> docs/building/REPORTE-POC.md ---------------------------------------------
	const TCHAR* TestPhrases[] = {
		TEXT("un carro con cohete"), TEXT("un tanque"), TEXT("un cohete"), TEXT("un avion con alas y ventilador"),
		TEXT("una torreta con laser"), TEXT("un globo aerostatico con fuego debajo"), TEXT("un carro con lanzallamas"),
		TEXT("un hovercraft con vela y emisor de hielo"), TEXT("un carro con una bomba"), TEXT("un puente largo de tablas"),
		TEXT("un trampolin con resorte que lance cosas"), TEXT("un velero con ruedas"), TEXT("una fortaleza con cañones"),
		TEXT("una moto"), TEXT("un trineo con ventilador"), TEXT("un dron con un faro"), TEXT("una catapulta"),
		TEXT("un aspersor de fuego que gire"), TEXT("una rueda sola que ruede quemando todo"), TEXT("un planeador"),
		TEXT("una torreta con cabeza que apunte sola"), TEXT("un carro con manguera de agua"), TEXT("un tanque electrico que de toques"),
		TEXT("un carro con turbina y bateria"), TEXT("un carro levitante con piedras antigravedad"), TEXT("un dirigible con cañon"),
		TEXT("una torreta anclada al suelo con una bisagra"), TEXT("algo que ruede y queme todo"),
		TEXT("hazme un carrito bien loco con lanzallamas, un globo, cohete, luces y una bateria extra"),
	};

	struct FJamTestRun
	{
		TWeakObjectPtr<UWorld> World;
		FTransform At;
		int32 Index = -1;
		float Timer = 0.f;
		float Duration = 10.f;
		TWeakObjectPtr<AJamBuildSpawner> Cur;
		TWeakObjectPtr<AStaticMeshActor> Dummy;
		FVector DummyStart = FVector::ZeroVector;
		// metricas de la frase actual
		bool bStarted = false;
		FVector Start = FVector::ZeroVector;
		int32 Parts = 0, MaxOn = 0, MaxBalls = 0, Hits = 0;
		double MaxSpeed = 0, MaxUp = 0, Disp = 0, MaxSpin = 0, DummyMoved = 0;
		bool bHadBomb = false, bExploded = false, bFrozen = false;
		int32 Pass = 0;
		FString Rows;
		FTSTicker::FDelegateHandle Handle;
	};
	FJamTestRun GTest;

	void TestFinishPhrase()
	{
		FJamTestRun& T = GTest;
		const FString Status = T.Cur.IsValid() ? T.Cur->LastStatus : FString(TEXT("(sin construir)"));
		const TCHAR* Backend = Status.Contains(TEXT("fallback")) ? TEXT("respaldo (fallo Jev)") : Status.Contains(TEXT("(jev")) ? TEXT("Jev") : Status.Contains(TEXT("keywords")) ? TEXT("palabras clave") : TEXT("?");
		TArray<FString> Seen;
		if (T.Disp > 60) { Seen.Add(FString::Printf(TEXT("se movio %.0f cm (max %.0f cm/s)"), T.Disp, T.MaxSpeed)); }
		if (T.MaxUp > 80) { Seen.Add(FString::Printf(TEXT("subio %.0f cm"), T.MaxUp)); }
		if (T.MaxSpin > 2) { Seen.Add(FString::Printf(TEXT("algo gira a %.1f rad/s"), T.MaxSpin)); }
		if (T.MaxBalls > 0) { Seen.Add(FString::Printf(TEXT("disparo/lanzo %d bolas"), T.MaxBalls)); }
		if (T.Hits > 0) { Seen.Add(FString::Printf(TEXT("su efecto toco al objetivo (%d)"), T.Hits)); }
		if (T.bFrozen) { Seen.Add(TEXT("objetivo congelado")); }
		if (T.DummyMoved > 30) { Seen.Add(FString::Printf(TEXT("objetivo empujado %.0f cm"), T.DummyMoved)); }
		if (T.bExploded) { Seen.Add(TEXT("la bomba exploto")); }
		const bool bBuilt = Status.StartsWith(TEXT("OK"));
		const bool bPass = bBuilt && (Seen.Num() > 0 || T.MaxOn == 0);
		if (bBuilt && Seen.Num() == 0) { Seen.Add(T.MaxOn == 0 ? TEXT("estructura estatica, se mantuvo en pie") : TEXT("nada medible")); }
		T.Pass += bPass ? 1 : 0;
		T.Rows += FString::Printf(TEXT("| %d | %s | %s | `%s` | %d | %d | %s | %s |\n"), T.Index + 1, TestPhrases[T.Index], Backend,
			*Status.Replace(TEXT("|"), TEXT("/")), T.Parts, T.MaxOn, *FString::Join(Seen, TEXT("; ")), bPass ? TEXT("PASA") : TEXT("FALLA"));
		UE_LOG(LogTemp, Display, TEXT("JamTest %d/%d %s | %s | %s | %s"), T.Index + 1, (int32)UE_ARRAY_COUNT(TestPhrases), bPass ? TEXT("PASA") : TEXT("FALLA"), TestPhrases[T.Index], *Status, *FString::Join(Seen, TEXT("; ")));
	}

	bool TestTick(float Dt)
	{
		FJamTestRun& T = GTest;
		UWorld* World = T.World.Get();
		if (!World)
		{
			return false;
		}
		T.Timer += Dt;
		if (T.Index >= 0 && T.Timer < T.Duration)
		{
			// Medir la construccion actual.
			if (AJamBuildSpawner* S = T.Cur.Get())
			{
				const TArray<TObjectPtr<AJamBuildPart>>& Parts = S->GetParts();
				int32 On = 0, Hits = 0, Alive = 0;
				bool bBomb = false;
				for (const AJamBuildPart* P : Parts)
				{
					if (!IsValid(P)) { continue; }
					++Alive;
					On += P->bActive ? 1 : 0;
					Hits += P->EffectHits;
					bBomb |= P->Kind == EJamPartKind::Bomb;
					T.MaxSpin = FMath::Max(T.MaxSpin, P->Mesh->GetPhysicsAngularVelocityInRadians().Size());
				}
				if (Parts.Num() > 0)
				{
					T.Parts = FMath::Max(T.Parts, Parts.Num());
					T.MaxOn = FMath::Max(T.MaxOn, On);
					T.Hits = FMath::Max(T.Hits, Hits);
					T.bExploded |= T.bHadBomb && !bBomb;
					T.bHadBomb |= bBomb;
					if (IsValid(Parts[0]))
					{
						const FVector L = Parts[0]->GetActorLocation();
						if (!T.bStarted) { T.bStarted = true; T.Start = L; }
						T.Disp = FMath::Max(T.Disp, FVector::Dist2D(L, T.Start));
						T.MaxUp = FMath::Max(T.MaxUp, L.Z - T.Start.Z);
						T.MaxSpeed = FMath::Max(T.MaxSpeed, Parts[0]->Mesh->GetPhysicsLinearVelocity().Size());
					}
				}
			}
			int32 Balls = 0;
			for (TActorIterator<AStaticMeshActor> It(World); It; ++It) { Balls += It->ActorHasTag(TEXT("JamProjectile")) ? 1 : 0; }
			T.MaxBalls = FMath::Max(T.MaxBalls, Balls);
			if (AStaticMeshActor* D = T.Dummy.Get())
			{
				T.DummyMoved = FMath::Max(T.DummyMoved, FVector::Dist(D->GetActorLocation(), T.DummyStart));
				T.bFrozen |= D->GetStaticMeshComponent()->GetLinearDamping() > 1.f;
			}
			return true;
		}

		// Cerrar la frase actual y pasar a la siguiente.
		if (T.Index >= 0)
		{
			TestFinishPhrase();
		}
		ClearAll(World);
		if (T.Dummy.IsValid()) { T.Dummy->Destroy(); }
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It) { if (It->ActorHasTag(TEXT("JamProjectile"))) { It->Destroy(); } }
		++T.Index;
		if (T.Index >= (int32)UE_ARRAY_COUNT(TestPhrases))
		{
			const int32 N = UE_ARRAY_COUNT(TestPhrases);
			FString Md = TEXT("---\ntitle: Reporte del POC de construccion por lenguaje natural\ntags:\n  - building\n  - pruebas\nstatus: generado\n---\n\n# Reporte del POC: frase -> modelo de decision -> construccion\n\n");
			Md += TEXT("> [!info]\n> Archivo GENERADO por el comando de consola `Jam.Test` (en PIE, `Source/GameJamm/Building/JamPlayConsole.cpp`). No editar a mano: volver a correr el comando.\n");
			Md += FString::Printf(TEXT("> Fecha: %s. Cada frase se construye delante del jugador con un cubo-objetivo de 20 kg a 3.5 m al frente y se mide durante %.0f s.\n\n"), *FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M")), T.Duration);
			Md += FString::Printf(TEXT("**Resultado: %d de %d frases pasan.** \"PASA\" = se construyo y se midio al menos una mecanica (o es una estructura estatica sin dispositivos). No juzga si el arquetipo elegido es el que una persona esperaria: eso se lee en la columna de decision.\n\n"), T.Pass, N);
			Md += TEXT("| # | Frase | Backend | Decision y receta (estado del spawner) | Piezas | Encendidas | Mecanica observada | Veredicto |\n|---|---|---|---|---|---|---|---|\n");
			Md += T.Rows;
			Md += TEXT("\n## Como leerlo\n\n- **Backend**: `Jev` = decidio el modelo de decision; `respaldo` = fallo la llamada y se uso el generador por palabras clave.\n- **Decision**: arquetipo (plantilla de piezas), `skipped=[...]` = extras pedidos que no cupieron o no aplican, `extras=[...]` = extras que el modelo marco.\n- **Mecanica observada**: medido en el juego (posicion, velocidad, giro, proyectiles, efecto sobre el cubo-objetivo).\n");
			const FString Path = FPaths::ProjectDir() / TEXT("docs/building/REPORTE-POC.md");
			const bool bSaved = FFileHelper::SaveStringToFile(Md, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
			Screen(FString::Printf(TEXT("Jam.Test terminado: %d/%d pasan. Reporte %s: %s"), T.Pass, N, bSaved ? TEXT("guardado") : TEXT("NO guardado"), *Path), FColor::Green);
			UE_LOG(LogTemp, Display, TEXT("JamTest DONE %d/%d saved=%d %s"), T.Pass, N, bSaved, *Path);
			T.Index = -1;
			return false;
		}
		// Siguiente frase: energia llena, objetivo nuevo, metricas a cero.
		AJamBuildPart::SetPlayerEnergy(World, 100.f);
		T.Timer = 0.f;
		T.bStarted = false;
		T.Parts = T.MaxOn = T.MaxBalls = T.Hits = 0;
		T.MaxSpeed = T.MaxUp = T.Disp = T.MaxSpin = T.DummyMoved = 0;
		T.bHadBomb = T.bExploded = T.bFrozen = false;
		T.DummyStart = T.At.TransformPosition(FVector(350, 0, 40));
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AStaticMeshActor* D = World->SpawnActor<AStaticMeshActor>(T.DummyStart, FRotator::ZeroRotator, P))
		{
			UStaticMeshComponent* C = D->GetStaticMeshComponent();
			C->SetMobility(EComponentMobility::Movable);
			C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			C->SetWorldScale3D(FVector(0.6));
			C->SetSimulatePhysics(true);
			C->SetMassOverrideInKg(NAME_None, 20.f, true);
			T.Dummy = D;
		}
		Screen(FString::Printf(TEXT("Jam.Test %d/%d: %s"), T.Index + 1, (int32)UE_ARRAY_COUNT(TestPhrases), TestPhrases[T.Index]), FColor::Cyan);
		T.Cur = SpawnSay(World, TestPhrases[T.Index], T.At);
		return true;
	}

	void JamTest(const TArray<FString>& Args, UWorld* World)
	{
		if (!World || !World->IsGameWorld())
		{
			Screen(TEXT("Jam.Test: correr dentro del juego (PIE)"), FColor::Red);
			return;
		}
		FTSTicker::GetCoreTicker().RemoveTicker(GTest.Handle);
		GTest = FJamTestRun();
		GTest.World = World;
		GTest.At = SayTransform(World);
		if (Args.Num() > 0) { GTest.Duration = FMath::Clamp(FCString::Atof(*Args[0]), 3.f, 60.f); }
		GTest.Timer = GTest.Duration; // arranca ya con la primera frase
		GTest.Handle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TestTick));
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
	FAutoConsoleCommandWithWorldAndArgs CmdEnergy(TEXT("Jam.Energy"), TEXT("Jam.Energy [valor]: muestra (o fija) la barra de energia del jugador."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamEnergy));
	FAutoConsoleCommandWithWorldAndArgs CmdTest(TEXT("Jam.Test"), TEXT("Jam.Test [segundos por frase]: corre la bateria de frases y escribe docs/building/REPORTE-POC.md."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamTest));
	FAutoConsoleCommandWithWorldAndArgs CmdSeat(TEXT("Jam.Seat"), TEXT("Sube al asiento de la construccion mas cercana, o baja."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamSeat));
}
