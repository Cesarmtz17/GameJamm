#include "Core/HealthComponent.h"
#include "Core/JamStatics.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "Net/UnrealNetwork.h"

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	Health = MaxHealth;
	bDead = false;

	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.AddDynamic(this, &UHealthComponent::HandleTakeAnyDamage);
	}
}

void UHealthComponent::HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	if (bDead || bInvulnerable || Damage <= 0.f)
	{
		return;
	}

	if (!bAllowFriendlyFire)
	{
		EJamTeam SourceTeam = EJamTeam::Neutral;
		if (InstigatedBy)
		{
			SourceTeam = UJamStatics::GetActorTeam(InstigatedBy);
		}
		if (SourceTeam == EJamTeam::Neutral && DamageCauser)
		{
			SourceTeam = UJamStatics::GetActorTeam(DamageCauser);
		}
		if (SourceTeam != EJamTeam::Neutral && SourceTeam == Team)
		{
			return;
		}
	}

	const float OldHealth = Health;
	Health = FMath::Clamp(Health - Damage, 0.f, MaxHealth);
	OnHealthChanged.Broadcast(this, Health, Health - OldHealth);

	if (Health <= 0.f)
	{
		AActor* Killer = DamageCauser;
		if (InstigatedBy && InstigatedBy->GetPawn())
		{
			Killer = InstigatedBy->GetPawn();
		}
		Kill(Killer);
	}
}

void UHealthComponent::Heal(float Amount)
{
	if (bDead || Amount <= 0.f)
	{
		return;
	}
	const float OldHealth = Health;
	Health = FMath::Clamp(Health + Amount, 0.f, MaxHealth);
	OnHealthChanged.Broadcast(this, Health, Health - OldHealth);
}

void UHealthComponent::ResetHealth()
{
	const float OldHealth = Health;
	bDead = false;
	Health = MaxHealth;
	OnHealthChanged.Broadcast(this, Health, Health - OldHealth);
}

void UHealthComponent::Kill(AActor* Killer)
{
	if (bDead)
	{
		return;
	}

	const float OldHealth = Health;
	Health = 0.f;
	bDead = true;
	if (OldHealth > 0.f)
	{
		OnHealthChanged.Broadcast(this, Health, -OldHealth);
	}
	OnDied.Broadcast(this, Killer);

	if (bDestroyOwnerOnDeath && GetOwner())
	{
		GetOwner()->SetLifeSpan(0.05f);
	}
}

void UHealthComponent::OnRep_Health(float OldHealth)
{
	OnHealthChanged.Broadcast(this, Health, Health - OldHealth);
}

void UHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UHealthComponent, Health);
	DOREPLIFETIME(UHealthComponent, bDead);
}
