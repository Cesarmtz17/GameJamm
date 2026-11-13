#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FJamOnInventoryChanged, UInventoryComponent*, Inventory, FName, ItemId, int32, NewCount);

/**
 * Inventario simple por conteo. CONTRATO COMPARTIDO.
 * IDs de items (FName):  "Mat.<Nombre>" materiales de fusión (Mat.Fire, Mat.Rock, Mat.Ice)
 *                        "Part.<Tipo>"  piezas de construcción (Part.Wheel, Part.Plank, Part.Flamethrower)
 */
UCLASS(ClassGroup = (Jam), meta = (BlueprintSpawnableComponent))
class GAMEJAMM_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TMap<FName, int32> Items;

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FJamOnInventoryChanged OnInventoryChanged;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void AddItem(FName ItemId, int32 Count = 1);

	/** Quita Count unidades. Devuelve false (y no quita nada) si no hay suficientes. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool RemoveItem(FName ItemId, int32 Count = 1);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetCount(FName ItemId) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool HasItems(const TMap<FName, int32>& Required) const;

	/** Quita todos los items requeridos de golpe. false (sin cambios) si falta alguno. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool ConsumeItems(const TMap<FName, int32>& Required);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void ClearInventory();
};
