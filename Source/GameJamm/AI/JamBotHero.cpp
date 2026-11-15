#include "AI/JamBotHero.h"
#include "Building/JamBuildPart.h"
#include "Building/JamBuildSpawner.h"
#include "Building/JamDrivePhysics.h"
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
	BuildPrompts = { TEXT("un carro con lanzallamas"), TEXT("un tanque"), TEXT("un carro con lanzallamas y puas") };
	TrapPrompts = { TEXT("una torreta con lanzallamas"), TEXT("una torreta con lanzallamas y hielo") };
}

namespace
{
	bool IsWeapon(EJamPartKind K)
	{
		return K == EJamPartKind::Cannon || K == EJamPartKind::Flame || K == EJamPartKind::Frost || K == EJamPartKind::Beam
			|| K == EJamPartKind::Shock || K == EJamPartKind::Hydrant;
	}

	/** Enemigo vivo (no neutral) mas cercano a At dentro de Range. */
	AActor* NearestEnemy(const UWorld* World, const AActor* Me, const FVector& At, float Range)
	{
		AActor* Best = nullptr;
		float BestD = Range * Range;
		for (TActorIterator<APawn> It(World); It; ++It)
		{
			const UHealthComponent* H = It->FindComponentByClass<UHealthComponent>();
			if (!H || H->IsDead() || H->Team == EJamTeam::Neutral || !UJamStatics::AreEnemies(*It, Me)) { continue; }
			const float D = FVector::DistSquared(It->GetActorLocation(), At);
			if (D < BestD) { BestD = D; Best = *It; }
		}
		return Best;
	}

	bool GroundUnder(const UWorld* World, const FVector& At, const AActor* Ignore, const TArray<AJamBuildPart*>* IgnoreParts, float* OutZ = nullptr)
	{
		FCollisionQueryParams Q(SCENE_QUERY_STAT(JamBotGround), false, Ignore);
		if (IgnoreParts) { for (AJamBuildPart* P : *IgnoreParts) { Q.AddIgnoredActor(P); } }
		FHitResult Hit;
		const bool bHit = World->LineTraceSingleByChannel(Hit, At + FVector(0, 0, 300.f), At - FVector(0, 0, 1200.f), ECC_Visibility, Q);
		if (bHit && OutZ) { *OutZ = Hit.ImpactPoint.Z; }
		return bHit && !Cast<APawn>(Hit.GetActor());
	}
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
	NextTrapTime = GetWorld()->GetTimeSeconds() + 20.f;
}

void AJamBotHero::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Health->IsDead())
	{
		return;
	}
	if (MountAt > 0.f)
	{
		AJamBuildSpawner* B = MyBuild.Get();
		AJamBuildPart* Root = B && B->GetParts().Num() > 0 ? B->GetParts()[0].Get() : nullptr;
		if (!Root)
		{
			MountAt = -1.f;
		}
		else
		{
			for (AJamBuildPart* P : B->GetParts()) { if (P && P->Kind == EJamPartKind::Wheel) { P->SetDrive(0.f); } }
			const float Now = GetWorld()->GetTimeSeconds();
			const bool bSettled = Root->Mesh->GetPhysicsLinearVelocity().Size() < 60.f && Root->Mesh->GetPhysicsAngularVelocityInRadians().Size() < 0.5f;
			if (Now >= MountAt && (bSettled || Now >= MountAt + 6.f))
			{
				MountAt = -1.f;
				MountBuild();
			}
		}
	}
	if (bMounted)
	{
		if (Seat.IsValid() && Seat->Build.IsValid()) { Drive(DeltaSeconds); }
		else { Dismount(TEXT("la construccion desaparecio")); }
	}
	TickTrap();
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
	if (bMounted)
	{
		if (Pct >= 0.3f)
		{
			return; // manejando: lo decide Drive() cada frame
		}
		Dismount(TEXT("poca vida"));
	}
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

	// Constructor: arma algo con el asistente cada BuildEvery segundos, solo en la pradera central (fuera de ella el piso
	// es angosto y las torres tapan el carril: un carro se cae o se vuelca).
	if (Kind == EJamAgentKind::Builder && GetWorld()->GetTimeSeconds() >= NextBuildTime && FMath::Abs(GetActorLocation().X) < DriveHalfX - 200.f
		&& GetCharacterMovement()->IsMovingOnGround())
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
	// A ras de piso, como lo que pide el jugador (SayTransform): armado en el aire caia y a veces quedaba de cabeza.
	FVector At = GetActorLocation() + FVector(Dir * 500.f, 0, 0);
	float GroundZ = 0.f;
	if (!GroundUnder(GetWorld(), At, this, nullptr, &GroundZ))
	{
		return;
	}
	At.Z = GroundZ + 10.f;
	const FTransform Where(FRotator(0, Dir > 0.f ? 0.f : 180.f, 0), At);
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
	bool bHasSeat = false;
	for (AJamBuildPart* P : S->GetParts())
	{
		if (!P) { continue; }
		bHasSeat |= P->Kind == EJamPartKind::Seat;
		if (P->Kind == EJamPartKind::Wheel) { P->SetDrive(0.f); } // quieto hasta que se suba
	}
	if (bHasSeat) { MountAt = GetWorld()->GetTimeSeconds() + 1.2f; }
	else { ClearLater(S, 60.f); }
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
	const bool bWasMounted = bMounted;
	if (bMounted) { Dismount(TEXT("murio")); }
	MountAt = -1.f;
	StopMoving();
	bRetreating = false;
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->DisableMovement();
	if (Look && Look->IsApplied() && !bWasMounted)
	{
		// Con figura rigeada: que se vea la animacion Death y se esconde despues (solo si sigue muerto: Respawn reusa este actor).
		// Montado no: Dismount lo deja 1 m arriba junto al carro y sin movimiento, la caida se veria flotando.
		FTimerHandle HideTimer;
		GetWorldTimerManager().SetTimer(HideTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (Health && Health->IsDead()) { SetActorHiddenInGame(true); }
		}), 2.f, false);
	}
	else
	{
		SetActorHiddenInGame(true);
	}
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

void AJamBotHero::ClearLater(AJamBuildSpawner* S, float Seconds)
{
	if (!S)
	{
		return;
	}
	TWeakObjectPtr<AJamBuildSpawner> Weak(S);
	FTimerHandle H;
	GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(S, [Weak]()
	{
		if (Weak.IsValid()) { Weak->ClearBuild(); Weak->Destroy(); }
	}), Seconds, false);
}

void AJamBotHero::MountBuild()
{
	AJamBuildSpawner* S = MyBuild.Get();
	if (!S || Health->IsDead() || bMounted)
	{
		return;
	}
	AJamBuildPart* Best = nullptr;
	for (AJamBuildPart* P : S->GetParts()) { if (P && P->Kind == EJamPartKind::Seat) { Best = P; break; } }
	if (!Best || !Best->Build.IsValid() || FVector::Dist(Best->GetActorLocation(), GetActorLocation()) > 2000.f)
	{
		UE_LOG(LogJam, Log, TEXT("Bot %s no se sube: %s"), *GetName(), !Best ? TEXT("sin asiento") : !Best->Build.IsValid() ? TEXT("sin estado") : TEXT("quedo lejos"));
		return;
	}
	// Igual que Jam.Seat del jugador: sin movimiento ni colision, pegado al asiento.
	StopMoving();
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->SetMovementMode(MOVE_None);
	SetActorEnableCollision(false);
	SetActorLocation(Best->GetActorLocation() + FVector(0, 0, 110));
	AttachToActor(Best, FAttachmentTransformRules::KeepWorldTransform);
	Seat = Best;
	bMounted = true;
	SmoothThrottle = SmoothSteer = 0.f;
	SlowSince = UpsideSince = -1.f;
	ReverseUntil = 0.f;
	Reverses = 0;
	UE_LOG(LogJam, Log, TEXT("Bot %s se sube a su construccion (%d piezas)"), *GetName(), Best->Build->Parts.Num());
}

void AJamBotHero::Dismount(const TCHAR* Why)
{
	if (!bMounted)
	{
		return;
	}
	bMounted = false;
	AJamBuildPart* S = Seat.Get();
	if (S && S->Build.IsValid())
	{
		for (const TWeakObjectPtr<AJamBuildPart>& P : S->Build->Parts)
		{
			if (!P.IsValid()) { continue; }
			if (P->Kind == EJamPartKind::Wheel) { P->SetDrive(0.f); }
			if (IsWeapon(P->Kind) && P->bActive) { P->SetActive(false); }
		}
	}
	const FVector Side = S ? FVector(S->GetActorRightVector().X, S->GetActorRightVector().Y, 0).GetSafeNormal() : FVector::RightVector;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorRotation(FRotator(0, GetActorRotation().Yaw, 0));
	AddActorWorldOffset(Side * 150.f + FVector(0, 0, 100));
	SetActorEnableCollision(true);
	if (!Health->IsDead()) { GetCharacterMovement()->SetMovementMode(MOVE_Falling); }
	Seat.Reset();
	UE_LOG(LogJam, Log, TEXT("Bot %s se baja: %s"), *GetName(), Why);
	// La carcasa queda un rato (estorba o sirve de cobertura) y luego se limpia para poder armar otra.
	if (AJamBuildSpawner* B = MyBuild.Get())
	{
		B->SetBuildActive(false);
		ClearLater(B, 12.f);
	}
	NextBuildTime = FMath::Min(NextBuildTime, GetWorld()->GetTimeSeconds() + 20.f);
}

void AJamBotHero::Drive(float DeltaSeconds)
{
	AJamBuildPart* S = Seat.Get();
	if (!S->Build->Parts.Num() || !S->Build->Parts[0].IsValid())
	{
		Dismount(TEXT("sin piezas"));
		return;
	}
	UWorld* World = GetWorld();
	const float Now = World->GetTimeSeconds();
	AJamBuildPart* Root = S->Build->Parts[0].Get();
	const FVector Pos = Root->GetActorLocation();
	if (Pos.Z < -1500.f)
	{
		Dismount(TEXT("se cayo"));
		return;
	}
	const AJamGameState* GS = World->GetGameState<AJamGameState>();
	const bool bOver = GS && GS->IsMatchOver();

	TArray<AJamBuildPart*> Mine;
	for (const TWeakObjectPtr<AJamBuildPart>& P : S->Build->Parts) { if (P.IsValid()) { Mine.Add(P.Get()); } }

	// Estabilidad (la misma ayuda que tiene el jugador al manejar): en el suelo, un par regresa el cabeceo/alabeo; si queda
	// de lado o de cabeza casi quieto mas de 1 s, se endereza solo. Si sigue volcado a los 6 s, se baja.
	const FVector Up = Root->GetActorUpVector();
	const FVector Com = Root->Mesh->GetCenterOfMass();
	const bool bGround = GroundUnder(World, Com - FVector(0, 0, 200.f), this, &Mine);
	FVector W = Root->Mesh->GetPhysicsAngularVelocityInRadians();
	W.Z = 0;
	if (bGround && Up.Z >= 0.3f && Up.Z < 0.995f)
	{
		Root->Mesh->AddTorqueInRadians(FVector::CrossProduct(Up, FVector::UpVector) * 120.f - W * 15.f, NAME_None, true);
	}
	if (Up.Z < 0.3f)
	{
		if (UpsideSince < 0.f) { UpsideSince = Now; }
		if (Now - UpsideSince > 6.f) { Dismount(TEXT("volcado")); return; }
		if (Now - UpsideSince > 1.f && Root->Mesh->GetPhysicsLinearVelocity().Size() < 400.f)
		{
			const FVector Axis = FVector::CrossProduct(Up, FVector::UpVector);
			const FVector AxisN = Axis.IsNearlyZero() ? Root->GetActorForwardVector() : Axis.GetSafeNormal();
			Root->Mesh->AddTorqueInRadians(AxisN * 14.f - W * 3.f, NAME_None, true);
			Root->Mesh->AddForce(FVector(0, 0, FMath::Max(S->Build->MassKg, 1.f) * 700.f));
		}
	}
	else
	{
		UpsideSince = -1.f;
	}

	// Adonde: enemigo cercano; si no, la torre enemiga mas cercana; si no, adelante por el carril (Y = 0).
	// Siempre dentro de la pradera central (|X| < DriveHalfX): ahi cabe el carro.
	const float LaneDir = -FMath::Sign(BaseX(GetTeam())); // +1 rojo, -1 azul
	AActor* Enemy = bOver ? nullptr : NearestEnemy(World, this, Pos, 2200.f);
	bool bStructure = false;
	if (!Enemy && !bOver)
	{
		float BestD = FMath::Square(3000.f);
		for (TActorIterator<ATeamStructure> It(World); It; ++It)
		{
			const float D = FVector::DistSquared2D(It->GetActorLocation(), Pos);
			if (It->GetTeam() != GetTeam() && It->GetTeam() != EJamTeam::Neutral && It->Health && !It->Health->IsDead() && D < BestD)
			{
				BestD = D;
				Enemy = *It;
				bStructure = true;
			}
		}
	}
	FVector Aim = Enemy ? Enemy->GetActorLocation() : FVector(Pos.X + LaneDir * 1500.f, Now < DetourUntil ? DetourY : 0.f, Pos.Z);
	if (Now < DetourUntil && Enemy) { Aim.Y = DetourY; Aim.X = Pos.X + LaneDir * 900.f; } // rodea lo que lo atoro
	Aim.X = FMath::Clamp(Aim.X, -DriveHalfX, DriveHalfX);
	Aim.Y = FMath::Clamp(Aim.Y, -2200.f, 2200.f);
	if (!Enemy && FMath::Abs(Pos.X) > DriveHalfX - 350.f && (Pos.X * LaneDir) > 0.f)
	{
		Dismount(TEXT("llego al final de la pradera"));
		NextBuildTime = Now + BuildEvery; // sigue a pie con la oleada
		return;
	}
	const FVector Fwd = FVector(Root->GetActorForwardVector().X, Root->GetActorForwardVector().Y, 0.f).GetSafeNormal();
	const FVector Want = FVector(Aim.X - Pos.X, Aim.Y - Pos.Y, 0.f).GetSafeNormal();
	const float Err = FMath::Atan2(FVector::CrossProduct(Fwd, Want).Z, FVector::DotProduct(Fwd, Want)); // + = a la derecha
	const float Dist = FVector::Dist2D(Aim, Pos);

	// De frente acelera; muy de lado gira casi en el lugar (giro diferencial) para no voltearse.
	float Throttle = FMath::Abs(Err) < 0.6f ? 1.f : FMath::Abs(Err) < 1.2f ? 0.4f : 0.1f;
	float Steer = FMath::Clamp(Err / 0.8f, -0.75f, 0.75f);
	if (Enemy && !bStructure && Dist < 450.f) { Throttle = 0.5f; } // encima: embiste despacio y quema
	if (Enemy && bStructure && FVector::Dist2D(Enemy->GetActorLocation(), Pos) < 750.f) { Throttle = 0.f; } // estacionado frente a la torre
	if (Dist < 150.f) { Throttle = 0.f; Steer = 0.f; }
	if (bOver) { Throttle = 0.f; Steer = 0.f; }

	// Hueco o borde adelante: frena y da reversa girando hacia el centro del carril.
	const float Speed = Root->Mesh->GetPhysicsLinearVelocity().Size2D();
	const float Ahead = 250.f + Speed * 0.5f;
	if (Throttle > 0.f && !GroundUnder(World, Pos + Fwd * Ahead, this, &Mine))
	{
		Throttle = -0.6f;
		Steer = Pos.Y > 0.f ? -0.5f : 0.5f;
	}

	// Atorado (acelera y no avanza): reversa 1.2 s girando; a la cuarta se baja.
	if (Throttle > 0.5f && Speed < 60.f)
	{
		if (SlowSince < 0.f) { SlowSince = Now; }
		if (Now - SlowSince > 2.5f)
		{
			SlowSince = -1.f;
			if (Now - LastCarHop > 5.f)
			{
				// Primero un brinco (escalones bajos que la llanta no sube): la misma velocidad a todas las piezas, sin forzar uniones.
				LastCarHop = Now;
				for (AJamBuildPart* P : Mine) { P->Mesh->AddImpulse(Fwd * 250.f + FVector(0, 0, 420.f), NAME_None, true); }
				UE_LOG(LogJam, Log, TEXT("Bot %s atorado en X=%.0f Y=%.0f: brinca"), *GetName(), Pos.X, Pos.Y);
				return;
			}
			if (++Reverses > 4) { Dismount(TEXT("atorado")); return; }
			ReverseUntil = Now + 1.2f;
			// Al terminar la reversa toma un carril lateral un rato (alterna lado) para rodear el obstaculo.
			DetourY = FMath::Clamp(Pos.Y + ((Reverses % 2) ? 700.f : -700.f), -2000.f, 2000.f);
			DetourUntil = ReverseUntil + 4.f;
			UE_LOG(LogJam, Log, TEXT("Bot %s atorado en X=%.0f Y=%.0f: reversa y rodea por Y=%.0f"), *GetName(), Pos.X, Pos.Y, DetourY);
		}
	}
	else
	{
		SlowSince = -1.f;
	}
	if (Now < ReverseUntil)
	{
		Throttle = -0.8f;
		Steer = Steer >= 0.f ? -0.5f : 0.5f;
	}

	SmoothThrottle = FMath::FInterpTo(SmoothThrottle, Throttle, DeltaSeconds, 4.f);
	SmoothSteer = FMath::FInterpTo(SmoothSteer, Steer, DeltaSeconds, 6.f);

	// Armas: solo con el enemigo enfrente y cerca.
	bool bFire = false;
	if (Enemy)
	{
		const FVector To = (Enemy->GetActorLocation() - Pos).GetSafeNormal2D();
		bFire = FVector::Dist2D(Enemy->GetActorLocation(), Pos) < (bStructure ? 1100.f : 900.f) && FVector::DotProduct(To, Fwd) > 0.6f;
	}
	const FVector Right = Root->GetActorRightVector();
	// A velocidad alta se limita el giro para que la aceleracion lateral no lo vuelque (JamDrivePhysics, igual que el jugador).
	double HalfTrack = 0.0, Height = 1.0;
	const FVector BCom = Root->GetBuildCenterOfMass();
	for (AJamBuildPart* P : Mine)
	{
		if (P->Kind != EJamPartKind::Wheel) { continue; }
		HalfTrack = FMath::Max(HalfTrack, FMath::Abs(FVector::DotProduct(P->GetActorLocation() - BCom, Right)));
		Height = FMath::Max(Height, BCom.Z - P->GetActorLocation().Z + P->Mesh->Bounds.BoxExtent.Z);
	}
	const float MaxYawRate = HalfTrack > 1.0 ? FMath::Min(1.3f, JamDrivePhysics::YawRate(Speed, HalfTrack, Height)) : 1.3f;
	const float DriveSteer = SmoothSteer * MaxYawRate / 1.5f;
	bool bWheels = false;
	for (AJamBuildPart* P : Mine)
	{
		if (IsWeapon(P->Kind) && P->bActive != bFire) { P->SetActive(bFire); }
		if (P->Kind == EJamPartKind::Fan && FMath::Abs(P->GetActorQuat().RotateVector(P->LocalDirection).Z) < 0.5)
		{
			P->SetDrive(FMath::Max(SmoothThrottle, 0.f));
			if (P->bActive != (SmoothThrottle > 0.01f)) { P->SetActive(SmoothThrottle > 0.01f); }
		}
		if (P->Kind == EJamPartKind::Wheel && P->DriveConstraint)
		{
			bWheels = true;
			const float Side = FVector::DotProduct(P->GetActorLocation() - Root->GetActorLocation(), Right) >= 0.0 ? 1.f : -1.f;
			P->SetDrive(FMath::Clamp(SmoothThrottle - 1.5f * DriveSteer * Side, -1.f, 1.f));
		}
	}
	// Giro: controla la velocidad de guinada con un par limitado (misma idea que el mando del jugador).
	if (FMath::Abs(SmoothSteer) > 0.05f || !bWheels)
	{
		double Inertia = 0.0, Arm = 0.0;
		int32 N = 0;
		for (AJamBuildPart* P : Mine)
		{
			const double D2 = FVector::DistSquared2D(P->GetActorLocation(), Pos);
			Inertia += P->Mesh->CalculateMass() * D2;
			if (P->Kind == EJamPartKind::Wheel) { Arm += FMath::Sqrt(D2); ++N; }
		}
		const double Mass = FMath::Max((double)S->Build->MassKg, 1.0);
		Inertia = FMath::Max(Inertia, Mass * 6400.0);
		const double MaxTorque = Inertia * 6.0 + Mass * 980.0 * 0.8 * FMath::Max(N > 0 ? Arm / N : 0.0, 100.0);
		const double YawRate = MaxYawRate;
		const double Wz = Root->Mesh->GetPhysicsAngularVelocityInRadians().Z;
		Root->Mesh->AddTorqueInRadians(FVector(0, 0, FMath::Clamp(Inertia * 20.0 * (YawRate * SmoothSteer - Wz), -MaxTorque, MaxTorque)));
	}
	if (Now >= DriveLogAt)
	{
		DriveLogAt = Now + 3.f;
		const FString What = Enemy ? FString::Printf(TEXT("tras %s a %.0f"), *Enemy->GetName(), Dist) : FString(TEXT("por el carril"));
		UE_LOG(LogJam, Log, TEXT("Bot %s maneja en X=%.0f Y=%.0f v=%.0f upz=%.2f wz=%.2f yawmax=%.2f err=%.0f acel=%.2f giro=%.2f %s%s"), *GetName(), Pos.X, Pos.Y, Speed, Up.Z, Root->Mesh->GetPhysicsAngularVelocityInRadians().Z, MaxYawRate,
			FMath::RadiansToDegrees(Err), SmoothThrottle, SmoothSteer, *What, bFire ? TEXT(" FUEGO") : TEXT(""));
	}
}

void AJamBotHero::BuildTrap()
{
	if (TrapPrompts.Num() == 0 || MyTrap.IsValid())
	{
		return;
	}
	// Solo en su mitad del mapa, a un lado del carril, sobre piso.
	const FVector L = GetActorLocation();
	const float Own = BaseX(GetTeam());
	if (L.X * Own < 0.f || FMath::Abs(L.X) < 3000.f || !GetCharacterMovement()->IsMovingOnGround())
	{
		return;
	}
	const float LaneDir = -FMath::Sign(Own);
	const float Y = (TrapIndex % 2 == 0) ? 420.f : -420.f;
	FVector At(L.X + LaneDir * 300.f, Y, L.Z);
	float GroundZ = 0.f;
	if (!GroundUnder(GetWorld(), At, this, nullptr, &GroundZ))
	{
		At.Y = -Y;
		if (!GroundUnder(GetWorld(), At, this, nullptr, &GroundZ)) { return; }
	}
	At.Z = GroundZ + 60.f;
	const FString Phrase = TrapPrompts[TrapIndex++ % TrapPrompts.Num()];
	const FTransform Where(FRotator(0, LaneDir > 0.f ? 0.f : 180.f, 0), At);
	AJamBuildSpawner* S = GetWorld()->SpawnActorDeferred<AJamBuildSpawner>(AJamBuildSpawner::StaticClass(), Where, this);
	if (!S)
	{
		return;
	}
	S->Prompt = Phrase;
	S->Team = GetTeam();
	S->bUseDecisionModel = false;
	S->bAutoActivate = true;
	S->bBuildOnBeginPlay = true;
	S->FinishSpawning(Where);
	MyTrap = S;
	TrapUntil = GetWorld()->GetTimeSeconds() + TrapLife;
	for (AJamBuildPart* P : S->GetParts())
	{
		if (!P) { continue; }
		if (P->Build.IsValid()) { P->Build->Energy = 100000.f; P->Build->Owner = this; }
		if (IsWeapon(P->Kind)) { P->SetActive(false); } // se encienden al acercarse un enemigo (TickTrap)
	}
	UE_LOG(LogJam, Log, TEXT("Bot %s pone trampa: '%s' en X=%.0f Y=%.0f (%d piezas)"), *GetName(), *Phrase, At.X, At.Y, S->GetParts().Num());
}

void AJamBotHero::TickTrap()
{
	AJamBuildSpawner* T = MyTrap.Get();
	const float Now = GetWorld()->GetTimeSeconds();
	if (!T)
	{
		if (Kind == EJamAgentKind::Fighter && !bMounted && !bRetreating && Now >= NextTrapTime)
		{
			NextTrapTime = Now + 5.f; // reintenta pronto si no estaba en buen lugar
			BuildTrap();
			if (MyTrap.IsValid()) { NextTrapTime = Now + TrapEvery; }
		}
		return;
	}
	if (Now >= TrapUntil)
	{
		UE_LOG(LogJam, Log, TEXT("Bot %s retira su trampa"), *GetName());
		T->ClearBuild();
		T->Destroy();
		MyTrap.Reset();
		return;
	}
	const TArray<TObjectPtr<AJamBuildPart>>& Parts = T->GetParts();
	if (Parts.Num() == 0 || !Parts[0])
	{
		MyTrap.Reset();
		return;
	}
	const bool bNear = NearestEnemy(GetWorld(), this, Parts[0]->GetActorLocation(), TrapRange) != nullptr;
	for (AJamBuildPart* P : Parts)
	{
		if (P && IsWeapon(P->Kind) && P->bActive != bNear)
		{
			P->SetActive(bNear);
			if (bNear) { UE_LOG(LogJam, Log, TEXT("Trampa de %s se enciende (%s)"), *GetName(), *P->PartType.ToString()); }
		}
	}
}
