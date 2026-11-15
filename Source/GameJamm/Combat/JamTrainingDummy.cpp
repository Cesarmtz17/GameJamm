#include "Combat/JamTrainingDummy.h"
#include "Combat/JamStatusComponent.h"
#include "Core/HealthComponent.h"
#include "Core/JamMessages.h"
#include "GameJamm.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
	/** Avisos del muneco solo si el jugador esta a menos de 30 m (si no, "Muneco: -5" saldria en cualquier lado del mapa). */
	bool JamDummyPlayerNear(const AActor* Dummy)
	{
		const UWorld* World = Dummy ? Dummy->GetWorld() : nullptr;
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		return Pawn && FVector::DistSquared(Pawn->GetActorLocation(), Dummy->GetActorLocation()) < FMath::Square(3000.f);
	}
}

namespace
{
	// Color base del muneco y color de cada estado (indice = ShownState).
	FLinearColor JamDummyColor(int32 State)
	{
		switch (State)
		{
		case 1: return FLinearColor(1.f, 0.30f, 0.04f);   // quemado
		case 2: return FLinearColor(0.45f, 0.65f, 0.95f); // lento
		case 3: return FLinearColor(1.f, 0.88f, 0.15f);   // aturdido
		case 4: return FLinearColor(0.60f, 0.92f, 1.f);   // congelado
		case 5: return FLinearColor(0.08f, 0.08f, 0.08f); // derribado
		default: return FLinearColor(0.62f, 0.50f, 0.34f);
		}
	}
}

AJamTrainingDummy::AJamTrainingDummy()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cyl.Succeeded()) { Mesh->SetStaticMesh(Cyl.Object); }
	Mesh->SetWorldScale3D(FVector(0.7f, 0.7f, 1.8f));
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	RootComponent = Mesh;

	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	Health->MaxHealth = 300.f;
	Health->Team = EJamTeam::Blue;
}

void AJamTrainingDummy::BeginPlay()
{
	Super::BeginPlay();
	Health->OnHealthChanged.AddDynamic(this, &AJamTrainingDummy::HandleHealthChanged);
	Health->OnDied.AddDynamic(this, &AJamTrainingDummy::HandleDied);
	Tint = Mesh->CreateDynamicMaterialInstance(0);
	Paint(JamDummyColor(0));
}

void AJamTrainingDummy::Paint(const FLinearColor& Color)
{
	if (Tint) { Tint->SetVectorParameterValue(TEXT("Color"), Color); }
}

void AJamTrainingDummy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Estado visible: derribado > congelado > aturdido > lento > quemado. Slow/Stun/Freeze de UJamStatusComponent solo
	// frenan a un ACharacter, asi que aqui se ven con color y un aviso corto la primera vez de cada tanda.
	int32 State = 0;
	if (Health && Health->IsDead()) { State = 5; }
	else if (const UJamStatusComponent* S = FindComponentByClass<UJamStatusComponent>())
	{
		State = S->IsFrozen() ? 4 : S->IsStunned() ? 3 : S->IsSlowed() ? 2 : S->IsBurning() ? 1 : 0;
	}
	if (State == ShownState) { return; }
	ShownState = State;
	Paint(JamDummyColor(State));
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (State == 0 || Now - LastFxPost[State] < 1.5 || !JamDummyPlayerNear(this)) { return; }
	LastFxPost[State] = Now;
	switch (State)
	{
	case 1: JamMsg::Post(TEXT("Muneco: quemandose"), JamMsg::Warn, 2.f, TEXT("dummy_fx")); break;
	case 2: JamMsg::Post(TEXT("Muneco: ralentizado"), JamMsg::Info, 2.f, TEXT("dummy_fx")); break;
	case 3: JamMsg::Post(TEXT("Muneco: aturdido"), JamMsg::Info, 2.f, TEXT("dummy_fx")); break;
	case 4: JamMsg::Post(TEXT("Muneco: congelado"), JamMsg::Info, 2.f, TEXT("dummy_fx")); break;
	case 5: JamMsg::Post(TEXT("Muneco derribado: revive en 2 s"), JamMsg::Good, 2.f, TEXT("dummy_fx")); break;
	default: break;
	}
}

void AJamTrainingDummy::HandleHealthChanged(UHealthComponent*, float NewHealth, float Delta)
{
	if (Delta >= 0.f) { return; }
	UE_LOG(LogJam, Log, TEXT("Dummy %s: -%.0f (queda %.0f)"), *GetName(), -Delta, NewHealth);
	if (!JamDummyPlayerNear(this)) { return; } // solo avisa si el jugador esta practicando cerca
	JamMsg::Post(FString::Printf(TEXT("Muneco: -%.0f"), -Delta), JamMsg::Warn, 1.5f, TEXT("dummy_dmg"));
}

void AJamTrainingDummy::HandleDied(UHealthComponent*, AActor*)
{
	UE_LOG(LogJam, Log, TEXT("Dummy %s derribado; revive en 2 s"), *GetName());
	GetWorldTimerManager().SetTimer(ReviveTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { Health->ResetHealth(); }), 2.f, false);
}
