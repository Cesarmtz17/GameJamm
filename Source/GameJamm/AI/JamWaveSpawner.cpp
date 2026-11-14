#include "AI/JamWaveSpawner.h"
#include "AI/JamBotHero.h"
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
	for (TActorIterator<AJamMinion> It(GetWorld()); It; ++It)
	{
		Alive += (!It->IsA<AJamBotHero>() && It->GetTeam() == Team && It->Health && !It->Health->IsDead()) ? 1 : 0;
	}
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
		FString Line;
		for (float X = -12400.f; X <= 12400.f; X += 200.f)
		{
			FHitResult Down, Up;
			const FVector Top(X, 0, 3000.f), Bot(X, 0, -3000.f);
			const bool bD = World->LineTraceSingleByChannel(Down, Top, Bot, ECC_Pawn);
			const bool bU = bD && World->LineTraceSingleByChannel(Up, Down.ImpactPoint + FVector(0, 0, 5), Down.ImpactPoint + FVector(0, 0, 200), ECC_Pawn);
			Line += FString::Printf(TEXT("%.0f:%s%s "), X, bD ? *FString::Printf(TEXT("%.0f"), Down.ImpactPoint.Z) : TEXT("-"), bU ? TEXT("^") : TEXT(""));
			if (bU && Up.GetComponent())
			{
				const UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Up.GetComponent());
				UE_LOG(LogJam, Log, TEXT("Techo en X=%.0f Z=%.0f: %s"), X, Up.ImpactPoint.Z, SMC && SMC->GetStaticMesh() ? *SMC->GetStaticMesh()->GetName() : *GetNameSafe(Up.GetComponent()));
			}
		}
		UE_LOG(LogJam, Log, TEXT("Perfil carril: %s"), *Line);
	}
	FAutoConsoleCommandWithWorldAndArgs CmdProfile(TEXT("Jam.Profile"), TEXT("Altura del piso a lo largo del carril (log)."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamProfile));
	FAutoConsoleCommandWithWorldAndArgs CmdAgents(TEXT("Jam.Agents"), TEXT("Lista minions y bots vivos (posicion y vida) en el log."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamAgents));
	FAutoConsoleCommandWithWorldAndArgs CmdWave(TEXT("Jam.Wave"), TEXT("Saca una oleada de minions ya."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamWave));
}
