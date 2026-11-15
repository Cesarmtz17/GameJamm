#include "Combat/JamStatusComponent.h"
#include "Core/HealthComponent.h"
#include "GameJamm.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

UJamStatusComponent::UJamStatusComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

UJamStatusComponent* UJamStatusComponent::Get(AActor* Target)
{
	if (!Target)
	{
		return nullptr;
	}
	if (UJamStatusComponent* S = Target->FindComponentByClass<UJamStatusComponent>())
	{
		return S;
	}
	UJamStatusComponent* S = NewObject<UJamStatusComponent>(Target, TEXT("JamStatus"));
	S->RegisterComponent();
	return S;
}

float UJamStatusComponent::Now() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

void UJamStatusComponent::Burn(float DamagePerSecond, float Seconds, AController* By, AActor* Causer)
{
	if (!IsBurning()) { NextBurnTick = Now() + 1.f; }
	BurnDps = FMath::Max(BurnDps * (IsBurning() ? 1.f : 0.f), DamagePerSecond);
	BurnUntil = FMath::Max(BurnUntil, Now() + Seconds);
	BurnBy = By;
	BurnCauser = Causer;
	UE_LOG(LogJam, Log, TEXT("Estado %s: quemandose %.0f/s por %.1f s"), *GetOwner()->GetName(), BurnDps, Seconds);
}

void UJamStatusComponent::Slow(float SpeedFactor, float Seconds)
{
	SlowFactor = FMath::Min(SlowUntil > Now() ? SlowFactor : 1.f, SpeedFactor);
	SlowUntil = FMath::Max(SlowUntil, Now() + Seconds);
	UE_LOG(LogJam, Log, TEXT("Estado %s: lento x%.2f por %.1f s"), *GetOwner()->GetName(), SlowFactor, Seconds);
	ApplySpeed();
}

void UJamStatusComponent::Freeze(float Seconds)
{
	FrozenUntil = FMath::Max(FrozenUntil, Now() + Seconds);
	IceHits = 0;
	UE_LOG(LogJam, Log, TEXT("Estado %s: congelado %.1f s"), *GetOwner()->GetName(), Seconds);
	ApplySpeed();
}

void UJamStatusComponent::Stun(float Seconds)
{
	StunUntil = FMath::Max(StunUntil, Now() + Seconds);
	UE_LOG(LogJam, Log, TEXT("Estado %s: aturdido %.1f s"), *GetOwner()->GetName(), Seconds);
	ApplySpeed();
}

int32 UJamStatusComponent::AddIceHit()
{
	IceHits = Now() - LastIceHit < 1.5f ? IceHits + 1 : 1;
	LastIceHit = Now();
	return IceHits;
}

bool UJamStatusComponent::IsBurning() const
{
	return BurnUntil > Now();
}

bool UJamStatusComponent::IsFrozen() const
{
	return FrozenUntil > Now();
}

void UJamStatusComponent::ApplySpeed()
{
	ACharacter* Char = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* Move = Char ? Char->GetCharacterMovement() : nullptr;
	if (!Move)
	{
		return;
	}
	const float T = Now();
	const bool bStopped = FrozenUntil > T || StunUntil > T;
	const bool bSlow = SlowUntil > T;
	if (!bStopped && !bSlow)
	{
		if (BaseSpeed >= 0.f) { Move->MaxWalkSpeed = BaseSpeed; BaseSpeed = -1.f; }
		return;
	}
	if (BaseSpeed < 0.f) { BaseSpeed = Move->MaxWalkSpeed; }
	Move->MaxWalkSpeed = bStopped ? 0.f : BaseSpeed * SlowFactor;
	if (bStopped) { Move->StopMovementImmediately(); }
}

void UJamStatusComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const float T = Now();
	const UHealthComponent* H = GetOwner()->FindComponentByClass<UHealthComponent>();
	if (H && H->IsDead())
	{
		BurnUntil = SlowUntil = FrozenUntil = StunUntil = -1.f;
	}
	// Quemadura: un tick de dano por segundo mientras dure.
	while (BurnDps > 0.f && T >= NextBurnTick && NextBurnTick <= BurnUntil)
	{
		UGameplayStatics::ApplyDamage(GetOwner(), BurnDps, BurnBy.Get(), BurnCauser.Get(), nullptr);
		UE_LOG(LogJam, Log, TEXT("Estado %s: quemadura -%.0f"), *GetOwner()->GetName(), BurnDps);
		NextBurnTick += 1.f;
	}
	if (T > BurnUntil) { BurnDps = 0.f; }
	if (BaseSpeed >= 0.f) { ApplySpeed(); }
}
