#include "AI/JamBotHero.h"
#include "Building/JamBuildPart.h"
#include "Building/JamBuildSpawner.h"
#include "Components/CapsuleComponent.h"
#include "Core/JamCharacterLook.h"
#include "Core/HealthComponent.h"
#include "Core/JamGameMode.h"
#include "Core/JamGameState.h"
#include "Core/JamStatics.h"
#include "Core/TeamStructure.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "GameJamm.h"

AJamBotHero::AJamBotHero()
{
	Tags.Remove(TEXT("Minion")); // las torres priorizan minions; un heroe no lo es
	Tags.Add(TEXT("Bot"));
	GetCapsuleComponent()->InitCapsuleSize(38.f, 92.f);
	GetCharacterMovement()->MaxWalkSpeed = 520.f;
	Health->MaxHealth = 150.f;
	EnergyReward = 0.f;
	Look->Key = TEXT("BotHero");
	AggroRange = 1600.f;
	BuildPrompts = { TEXT("un carro con lanzallamas"), TEXT("un tanque"), TEXT("una torreta con laser") };
}

void AJamBotHero::InitAgent(EJamAgentKind InKind, EJamTeam InTeam)
{
	Kind = InKind;
	Health->Team = InTeam;
	Health->MaxHealth = 150.f;
	switch (Kind)
	{
	case EJamAgentKind::Shooter: AttackDamage = 15.f; AttackRange = 1800.f; AttackInterval = 0.8f; AggroRange = 2200.f; break;
	case EJamAgentKind::Builder: AttackDamage = 12.f; AttackRange = 200.f; AttackInterval = 0.8f; break;
	default:                     AttackDamage = 20.f; AttackRange = 200.f; AttackInterval = 0.6f; break; // Fighter
	}
	SetActorScale3D(FVector(1.1f));
	// Color de equipo pleno (los minions van apagados) y cabeza clara por rol.
	SetColor(InTeam == EJamTeam::Blue ? FLinearColor(0.15f, 0.4f, 1.f) : FLinearColor(1.f, 0.18f, 0.2f));
	if (UMaterialInstanceDynamic* MID = Head->CreateDynamicMaterialInstance(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), Kind == EJamAgentKind::Builder ? FLinearColor(1.f, 0.75f, 0.2f)
			: Kind == EJamAgentKind::Shooter ? FLinearColor(0.4f, 1.f, 0.5f) : FLinearColor(0.95f, 0.95f, 0.95f));
	}
}

void AJamBotHero::BeginPlay()
{
	Super::BeginPlay();
	NextBuildTime = GetWorld()->GetTimeSeconds() + 8.f;
}

FVector AJamBotHero::HomeLocation() const
{
	const float X = BaseX(GetTeam());
	return FVector(X + (X < 0.f ? 1600.f : -1600.f), 0.f, 260.f);
}

void AJamBotHero::Think()
{
	if (Health->IsDead())
	{
		return;
	}
	if (GetActorLocation().Z < -2500.f)
	{
		Health->Kill(nullptr);
		return;
	}
	const AJamGameState* GS = GetWorld()->GetGameState<AJamGameState>();
	if (GS && GS->IsMatchOver())
	{
		StopMoving();
		return;
	}

	// Retirarse: con poca vida vuelve a su base; ahi se cura hasta 90 %.
	const float Pct = Health->GetHealthPercent();
	if (Pct < 0.3f)
	{
		bRetreating = true;
	}
	if (bRetreating)
	{
		const FVector Home = HomeLocation();
		if (FVector::Dist2D(GetActorLocation(), Home) < 1500.f)
		{
			Health->Heal(15.f * 0.2f);
			StopMoving();
			if (Health->GetHealthPercent() >= 0.9f)
			{
				bRetreating = false;
			}
		}
		else
		{
			MoveTo(Home);
		}
		return;
	}

	// Constructor: arma algo con el asistente cada BuildEvery segundos.
	if (Kind == EJamAgentKind::Builder && GetWorld()->GetTimeSeconds() >= NextBuildTime)
	{
		BuildSomething();
		NextBuildTime = GetWorld()->GetTimeSeconds() + BuildEvery;
	}

	// Defender: si un enemigo esta cerca de una torre propia, va por el.
	AActor* Target = FindTarget(AggroRange);
	if (!Target)
	{
		for (TActorIterator<ATeamStructure> It(GetWorld()); It && !Target; ++It)
		{
			if (It->GetTeam() == GetTeam() && It->Health && !It->Health->IsDead())
			{
				const FVector Saved = GetActorLocation();
				// enemigo dentro del rango de la torre propia
				for (TActorIterator<APawn> P(GetWorld()); P; ++P)
				{
					const UHealthComponent* H = P->FindComponentByClass<UHealthComponent>();
					if (H && !H->IsDead() && UJamStatics::AreEnemies(*P, this) && H->Team != EJamTeam::Neutral &&
						FVector::Dist2D(P->GetActorLocation(), It->GetActorLocation()) < It->AttackRange &&
						FVector::Dist2D(Saved, It->GetActorLocation()) < 6000.f)
					{
						Target = *P;
						break;
					}
				}
			}
		}
	}
	if (Target)
	{
		if (!TryAttack(Target))
		{
			MoveTo(Target->GetActorLocation());
		}
		return;
	}

	// Empujar con la oleada: avanza por el carril, pero sin adelantarse mucho a su minion mas avanzado.
	float FrontX = BaseX(GetTeam()) * 0.7f;
	const float Dir = -FMath::Sign(BaseX(GetTeam())); // +1 rojo, -1 azul
	for (TActorIterator<AJamMinion> It(GetWorld()); It; ++It)
	{
		if (*It != this && !It->IsA<AJamBotHero>() && It->GetTeam() == GetTeam() && It->Health && !It->Health->IsDead())
		{
			FrontX = Dir > 0.f ? FMath::Max(FrontX, It->GetActorLocation().X) : FMath::Min(FrontX, It->GetActorLocation().X);
		}
	}
	const float MyX = GetActorLocation().X;
	if ((MyX - FrontX) * Dir > -300.f)
	{
		StopMoving(); // espera a la oleada
		return;
	}
	MoveTo(LaneGoal());
}

void AJamBotHero::BuildSomething()
{
	if (MyBuild.IsValid() || BuildPrompts.Num() == 0)
	{
		return; // una construccion a la vez
	}
	const FString Phrase = BuildPrompts[PromptIndex++ % BuildPrompts.Num()];
	const float Dir = -FMath::Sign(BaseX(GetTeam()));
	const FTransform Where(FRotator(0, Dir > 0.f ? 0.f : 180.f, 0), GetActorLocation() + FVector(Dir * 500.f, 0, 60.f));
	AJamBuildSpawner* S = GetWorld()->SpawnActorDeferred<AJamBuildSpawner>(AJamBuildSpawner::StaticClass(), Where, this);
	if (!S)
	{
		return;
	}
	S->Prompt = Phrase;
	S->Team = GetTeam();
	S->bUseDecisionModel = false; // frases fijas: el respaldo por palabras clave es instantaneo y no gasta la API
	S->bAutoActivate = true;
	S->bBuildOnBeginPlay = true;
	S->FinishSpawning(Where);
	MyBuild = S;
	// Reserva propia de energia: la construccion del bot no gasta la barra del jugador.
	for (AJamBuildPart* P : S->GetParts())
	{
		if (P && P->Build.IsValid())
		{
			P->Build->Energy = 100000.f;
			P->Build->Owner = this;
			break;
		}
	}
	UE_LOG(LogJam, Log, TEXT("Bot %s construye: \"%s\" (%d piezas)"), *GetName(), *Phrase, S->GetParts().Num());
}

void AJamBotHero::HandleDied(UHealthComponent* HealthComponent, AActor* Killer)
{
	StopMoving();
	bRetreating = false;
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->DisableMovement();
	SetActorHiddenInGame(true);
	const AJamGameMode* GM = GetWorld()->GetAuthGameMode<AJamGameMode>();
	GetWorldTimerManager().SetTimer(RespawnTimer, this, &AJamBotHero::Respawn, GM ? GM->GetRespawnDelay() : 5.f, false);
}

void AJamBotHero::Respawn()
{
	const AJamGameState* GS = GetWorld()->GetGameState<AJamGameState>();
	if (GS && GS->IsMatchOver())
	{
		return;
	}
	FVector At = HomeLocation() + FVector(0, FMath::FRandRange(-300.f, 300.f), 0);
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(JamBotGround), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0, 0, 3000.f), At - FVector(0, 0, 3000.f), ECC_Visibility, Q))
	{
		At.Z = Hit.ImpactPoint.Z + 130.f;
	}
	TeleportTo(At, GetActorRotation());
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SetActorHiddenInGame(false);
	Health->ResetHealth();
}
