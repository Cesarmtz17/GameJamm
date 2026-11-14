#include "Building/JamSlowComponent.h"

#include "Building/JamDeviceMath.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UJamSlowComponent::UJamSlowComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

UJamSlowComponent* UJamSlowComponent::ApplySlow(AActor* Target, float Factor, float Duration)
{
	if (!IsValid(Target) || !Target->GetWorld() || Duration <= 0.f)
	{
		return nullptr;
	}
	UJamSlowComponent* Slow = Target->FindComponentByClass<UJamSlowComponent>();
	if (!Slow)
	{
		Slow = NewObject<UJamSlowComponent>(Target);
		if (!Slow)
		{
			return nullptr;
		}
		Slow->RegisterComponent();
	}
	if (!Slow->bApplied)
	{
		Slow->PreviousDilation = Target->CustomTimeDilation;
		Slow->bApplied = true;
	}
	Target->CustomTimeDilation = Slow->PreviousDilation * FMath::Clamp(Factor, 0.05f, 1.f);
	// Tiempo de mundo: el Tick del componente también va dilatado, así que no se cuenta con DeltaTime.
	Slow->EndTime = JamDevices::RefreshSlowEnd(Slow->EndTime, static_cast<float>(Target->GetWorld()->GetTimeSeconds()), Duration);
	return Slow;
}

bool UJamSlowComponent::IsSlowed(const AActor* Target)
{
	const UJamSlowComponent* Slow = Target ? Target->FindComponentByClass<UJamSlowComponent>() : nullptr;
	return Slow && Slow->bApplied;
}

void UJamSlowComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const UWorld* World = GetWorld();
	if (bApplied && World && static_cast<float>(World->GetTimeSeconds()) >= EndTime)
	{
		Restore();
		DestroyComponent();
	}
}

void UJamSlowComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Restore();
	Super::EndPlay(EndPlayReason);
}

void UJamSlowComponent::Restore()
{
	if (bApplied)
	{
		bApplied = false;
		if (AActor* Owner = GetOwner())
		{
			Owner->CustomTimeDilation = PreviousDilation;
		}
	}
}
