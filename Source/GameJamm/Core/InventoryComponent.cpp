#include "Core/InventoryComponent.h"

UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UInventoryComponent::AddItem(FName ItemId, int32 Count)
{
	if (ItemId.IsNone() || Count <= 0)
	{
		return;
	}
	int32& Current = Items.FindOrAdd(ItemId);
	Current += Count;
	OnInventoryChanged.Broadcast(this, ItemId, Current);
}

bool UInventoryComponent::RemoveItem(FName ItemId, int32 Count)
{
	if (Count <= 0)
	{
		return true;
	}
	int32* Current = Items.Find(ItemId);
	if (!Current || *Current < Count)
	{
		return false;
	}
	*Current -= Count;
	const int32 NewCount = *Current;
	if (NewCount == 0)
	{
		Items.Remove(ItemId);
	}
	OnInventoryChanged.Broadcast(this, ItemId, NewCount);
	return true;
}

int32 UInventoryComponent::GetCount(FName ItemId) const
{
	const int32* Current = Items.Find(ItemId);
	return Current ? *Current : 0;
}

bool UInventoryComponent::HasItems(const TMap<FName, int32>& Required) const
{
	for (const TPair<FName, int32>& Pair : Required)
	{
		if (GetCount(Pair.Key) < Pair.Value)
		{
			return false;
		}
	}
	return true;
}

bool UInventoryComponent::ConsumeItems(const TMap<FName, int32>& Required)
{
	if (!HasItems(Required))
	{
		return false;
	}
	for (const TPair<FName, int32>& Pair : Required)
	{
		RemoveItem(Pair.Key, Pair.Value);
	}
	return true;
}

void UInventoryComponent::ClearInventory()
{
	Items.Reset();
	OnInventoryChanged.Broadcast(this, NAME_None, 0);
}
