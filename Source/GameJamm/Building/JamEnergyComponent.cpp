#include "Building/JamEnergyComponent.h"

#include "Building/JamDeviceMath.h"
#include "Engine/Engine.h"

UJamEnergyComponent::UJamEnergyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UJamEnergyComponent::BeginPlay()
{
	Super::BeginPlay();
	Current = FMath::Clamp(StartEnergy, 0.f, MaxEnergy);
	LastBroadcastTotal = Current + Reserve;
}

bool UJamEnergyComponent::Spend(float Amount)
{
	if (Amount <= 0.f)
	{
		return true;
	}
	JamDevices::FEnergyState State;
	State.Reserve = Reserve;
	State.Current = Current;
	const float Before = Reserve + Current;
	const bool bOk = JamDevices::SpendEnergy(State, Amount);
	Reserve = State.Reserve;
	Current = State.Current;
	const float Used = Before - (Reserve + Current);
	if (Used > 0.f)
	{
		SpentThisFrame += Used;
		TimeSinceSpend = 0.f;
	}
	// El aviso al HUD sale en TickComponent (una vez por frame aunque gasten 20 piezas).
	return bOk;
}

void UJamEnergyComponent::Add(float Amount)
{
	if (Amount <= 0.f)
	{
		return;
	}
	Current = FMath::Clamp(Current + Amount, 0.f, MaxEnergy);
	BroadcastIfChanged();
}

void UJamEnergyComponent::SetEnergy(float NewEnergy)
{
	Current = FMath::Clamp(NewEnergy, 0.f, MaxEnergy);
	BroadcastIfChanged();
}

bool UJamEnergyComponent::AddFromItem(FName ItemId)
{
	const float Amount = JamDevices::PickupEnergy(ItemId);
	if (Amount <= 0.f)
	{
		return false;
	}
	Add(Amount);
	return true;
}

void UJamEnergyComponent::AddReserve(float Amount)
{
	if (Amount > 0.f)
	{
		Reserve += Amount;
		BroadcastIfChanged();
	}
}

void UJamEnergyComponent::RemoveReserve(float Amount)
{
	if (Amount > 0.f)
	{
		Reserve = FMath::Max(0.f, Reserve - Amount);
		BroadcastIfChanged();
	}
}

void UJamEnergyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	DrainPerSecond = DeltaTime > 0.f ? SpentThisFrame / DeltaTime : 0.f;
	SpentThisFrame = 0.f;
	TimeSinceSpend += DeltaTime;
	Current = JamDevices::RegenEnergy(Current, MaxEnergy, TimeSinceSpend, DeltaTime, RegenPerSecond, RegenDelay);
	BroadcastIfChanged();

	if (bDebugOnScreen && GEngine)
	{
		const int32 Segments = FMath::Clamp(FMath::RoundToInt(GetEnergyPercent() * 10.f), 0, 10);
		FString Bar;
		for (int32 i = 0; i < 10; ++i)
		{
			Bar += i < Segments ? TEXT("#") : TEXT("-");
		}
		const FString Text = FString::Printf(TEXT("ENERGIA [%s] %.1f/%.0f  reserva %.1f  gasto -%.1f/s%s"),
			*Bar, Current, MaxEnergy, Reserve, DrainPerSecond, IsEmpty() ? TEXT("  VACIA") : TEXT(""));
		// La clave se deriva del componente para que dos jugadores no se pisen la línea.
		GEngine->AddOnScreenDebugMessage(static_cast<int32>(GetUniqueID()), 0.f, IsEmpty() ? FColor::Red : FColor::Yellow, Text);
	}
}

void UJamEnergyComponent::BroadcastIfChanged()
{
	const float Total = Current + Reserve;
	if (!FMath::IsNearlyEqual(Total, LastBroadcastTotal, 1.e-4f))
	{
		const float Delta = Total - LastBroadcastTotal;
		LastBroadcastTotal = Total;
		OnEnergyChanged.Broadcast(this, Current, Delta);
	}
}
