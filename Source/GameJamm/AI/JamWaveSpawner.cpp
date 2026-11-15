#include "AI/JamWaveSpawner.h"
#include "AI/JamBotHero.h"
#include "AI/JamCampGuard.h"
#include "GameFramework/PlayerController.h"
#include "Core/HealthComponent.h"
#include "Core/JamGameState.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "GameJamm.h"

AJamWaveSpawner::AJamWaveSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AJamWaveSpawner::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		return;
	}
	SpawnBots(EJamTeam::Red, RedBots);
	SpawnBots(EJamTeam::Blue, BlueBots);
	GetWorldTimerManager().SetTimer(WaveTimer, this, &AJamWaveSpawner::SpawnWave, WaveInterval, true, FirstWaveDelay);
}

AJamMinion* AJamWaveSpawner::SpawnAgent(UClass* Class, EJamAgentKind Kind, EJamTeam Team, const FVector& Where)
{
	// Piso real: traza hacia abajo desde arriba (las islas tienen alturas distintas; nacer dentro del suelo los atora).
	FVector At = Where;
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(JamWaveGround), false);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Where + FVector(0, 0, 3000.f), Where - FVector(0, 0, 3000.f), ECC_Visibility, Q))
	{
		At.Z = Hit.ImpactPoint.Z + 130.f;
		UE_LOG(LogJam, Verbose, TEXT("Piso para agente en X=%.0f: Z=%.0f (%s)"), Where.X, Hit.ImpactPoint.Z, *GetNameSafe(Hit.GetActor()));
	}
	const FTransform T(FRotator(0, Team == EJamTeam::Red ? 0.f : 180.f, 0), At);
	AJamMinion* M = GetWorld()->SpawnActorDeferred<AJamMinion>(Class, T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (M)
	{
		M->InitAgent(Kind, Team);
		M->FinishSpawning(T);
	}
	return M;
}

void AJamWaveSpawner::SpawnBots(EJamTeam Team, const TArray<EJamAgentKind>& Kinds)
{
	const float X = AJamMinion::BaseX(Team);
	for (int32 i = 0; i < Kinds.Num(); ++i)
	{
		SpawnAgent(AJamBotHero::StaticClass(), Kinds[i], Team, FVector(X + (X < 0.f ? 1600.f : -1600.f), -250.f + i * 250.f, 300.f));
	}
}

void AJamWaveSpawner::SpawnWave()
{
	const AJamGameState* GS = GetWorld()->GetGameState<AJamGameState>();
	if (GS && GS->IsMatchOver())
	{
		GetWorldTimerManager().ClearTimer(WaveTimer);
		return;
	}
	const bool bHeavy = GS && GS->MatchSeconds >= HeavyFromSeconds;
	SpawnTeamWave(EJamTeam::Red, bHeavy);
	SpawnTeamWave(EJamTeam::Blue, bHeavy);
}

void AJamWaveSpawner::SpawnTeamWave(EJamTeam Team, bool bHeavy)
{
	int32 Alive = 0;
	FString Where;
	for (TActorIterator<AJamMinion> It(GetWorld()); It; ++It)
	{
		if (It->GetTeam() == Team && It->Health && !It->Health->IsDead())
		{
			const FVector L = It->GetActorLocation();
			Where += FString::Printf(TEXT("%s(%.0f,%.0f,%.0f) "), It->IsA<AJamBotHero>() ? TEXT("B") : TEXT("m"), L.X, L.Y, L.Z);
			Alive += It->IsA<AJamBotHero>() ? 0 : 1;
		}
	}
	UE_LOG(LogJam, Log, TEXT("Vivos %s: %s"), Team == EJamTeam::Red ? TEXT("Rojo") : TEXT("Azul"), *Where);
	if (Alive >= MaxAlivePerTeam)
	{
		return;
	}
	const float X = AJamMinion::BaseX(Team);
	const float Fwd = X < 0.f ? 1.f : -1.f;
	const FVector Front(X + Fwd * 2000.f, 0.f, 260.f);
	for (int32 i = 0; i < 3; ++i)
	{
		SpawnAgent(AJamMinion::StaticClass(), EJamAgentKind::Melee, Team, Front + FVector(0, -160.f + i * 160.f, 0));
	}
	SpawnAgent(AJamMinion::StaticClass(), EJamAgentKind::Ranged, Team, Front + FVector(-Fwd * 250.f, 0, 0));
	if (bHeavy)
	{
		SpawnAgent(AJamMinion::StaticClass(), EJamAgentKind::Heavy, Team, Front + FVector(Fwd * 200.f, 0, 40.f));
	}
	UE_LOG(LogJam, Log, TEXT("Oleada %s (%d vivos antes)%s"), Team == EJamTeam::Red ? TEXT("Roja") : TEXT("Azul"), Alive, bHeavy ? TEXT(" + pesado") : TEXT(""));
}

namespace
{
	void JamWave(const TArray<FString>& Args, UWorld* World)
	{
		for (TActorIterator<AJamWaveSpawner> It(World); It; ++It)
		{
			It->SpawnWave();
		}
	}
	void JamAgents(const TArray<FString>& Args, UWorld* World)
	{
		for (TActorIterator<AJamMinion> It(World); It; ++It)
		{
			if (It->Health && !It->Health->IsDead())
			{
				UE_LOG(LogJam, Log, TEXT("Agente %s %s X=%.0f Y=%.0f Z=%.0f vida %.0f/%.0f"), *It->GetName(), It->GetTeam() == EJamTeam::Red ? TEXT("Rojo") : TEXT("Azul"),
					It->GetActorLocation().X, It->GetActorLocation().Y, It->GetActorLocation().Z, It->Health->GetHealth(), It->Health->MaxHealth);
			}
		}
	}
	void JamProfile(const TArray<FString>& Args, UWorld* World)
	{
		// Perfil del carril (Y=0): altura del primer piso y si hay techo encima (para diagnosticar atorones).
		// Jam.Profile [X0 X1 Paso Y]: por defecto todo el carril cada 200.
		const float X0 = Args.Num() > 0 ? FCString::Atof(*Args[0]) : -12400.f;
		const float X1 = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 12400.f;
		const float StepX = Args.Num() > 2 ? FMath::Max(5.f, FCString::Atof(*Args[2])) : 200.f;
		const float PY = Args.Num() > 3 ? FCString::Atof(*Args[3]) : 0.f;
		FString Line;
		for (float X = X0; X <= X1; X += StepX)
		{
			FHitResult Down, Up;
			const FVector Top(X, PY, 3000.f), Bot(X, PY, -3000.f);
			const bool bD = World->LineTraceSingleByChannel(Down, Top, Bot, ECC_Pawn);
			const bool bU = bD && World->LineTraceSingleByChannel(Up, Down.ImpactPoint + FVector(0, 0, 5), Down.ImpactPoint + FVector(0, 0, 200), ECC_Pawn);
			Line += FString::Printf(TEXT("%.0f:%s%s "), X, bD ? *FString::Printf(TEXT("%.0f%s"), Down.ImpactPoint.Z, *(Down.GetComponent() && Cast<UStaticMeshComponent>(Down.GetComponent()) && Cast<UStaticMeshComponent>(Down.GetComponent())->GetStaticMesh() ? FString(TEXT("[")) + Cast<UStaticMeshComponent>(Down.GetComponent())->GetStaticMesh()->GetName().Replace(TEXT("SM_MAP_Dressed_"), TEXT("")) + TEXT("]") : FString())) : TEXT("-"), bU ? TEXT("^") : TEXT(""));
			if (bU && Up.GetComponent())
			{
				const UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Up.GetComponent());
				UE_LOG(LogJam, Log, TEXT("Techo en X=%.0f Z=%.0f: %s"), X, Up.ImpactPoint.Z, SMC && SMC->GetStaticMesh() ? *SMC->GetStaticMesh()->GetName() : *GetNameSafe(Up.GetComponent()));
			}
		}
		UE_LOG(LogJam, Log, TEXT("Perfil carril: %s"), *Line);
	}
	// Jam.Minion [red|blue] [dist] [melee|ranged|heavy|guard|thrower|bot]: pone uno quieto-ish frente al jugador (pruebas de figura/animacion).
	void JamMinionCmd(const TArray<FString>& Args, UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Pawn) { return; }
		const FRotator Yaw(0.f, PC->GetControlRotation().Yaw, 0.f);
		const float Dist = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 400.f;
		const FTransform T(Yaw + FRotator(0, 180, 0), Pawn->GetActorLocation() + Yaw.Vector() * Dist + FVector(0, 0, 60));
		const EJamTeam Team = Args.Num() > 0 && Args[0].Equals(TEXT("red"), ESearchCase::IgnoreCase) ? EJamTeam::Red : EJamTeam::Blue;
		const FString What = Args.Num() > 2 ? Args[2].ToLower() : TEXT("melee");
		if (What == TEXT("guard") || What == TEXT("thrower"))
		{
			if (AJamCampGuard* G = World->SpawnActorDeferred<AJamCampGuard>(AJamCampGuard::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn))
			{
				G->InitGuard(What == TEXT("thrower"), T.GetLocation());
				G->FinishSpawning(T);
				G->SpawnDefaultController();
			}
			return;
		}
		UClass* Class = What == TEXT("bot") ? AJamBotHero::StaticClass() : AJamMinion::StaticClass();
		const EJamAgentKind Kind = What == TEXT("ranged") ? EJamAgentKind::Ranged : What == TEXT("heavy") ? EJamAgentKind::Heavy : What == TEXT("bot") ? EJamAgentKind::Fighter : EJamAgentKind::Melee;
		if (AJamMinion* M = World->SpawnActorDeferred<AJamMinion>(Class, T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn))
		{
			M->InitAgent(Kind, Team);
			M->FinishSpawning(T);
			M->SpawnDefaultController();
		}
	}
	FAutoConsoleCommandWithWorldAndArgs CmdMinion(TEXT("Jam.Minion"), TEXT("Jam.Minion [red|blue] [dist] [melee|ranged|heavy|guard|thrower|bot]: pone un agente frente al jugador."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamMinionCmd));
	FAutoConsoleCommandWithWorldAndArgs CmdProfile(TEXT("Jam.Profile"), TEXT("Altura del piso a lo largo del carril (log)."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamProfile));
	FAutoConsoleCommandWithWorldAndArgs CmdAgents(TEXT("Jam.Agents"), TEXT("Lista minions y bots vivos (posicion y vida) en el log."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamAgents));
	FAutoConsoleCommandWithWorldAndArgs CmdWave(TEXT("Jam.Wave"), TEXT("Saca una oleada de minions ya."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamWave));
}
