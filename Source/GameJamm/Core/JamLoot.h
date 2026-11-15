#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UInventoryComponent;
class UWorld;
class AActor;

/**
 * Loot (GDD 4): lee Content/Game/Core/Data/Loot.json, tira tablas y mete piezas en la mochila respetando sus topes
 * (40 piezas, 8 comunes / 4 dispositivos / 2 raras por tipo, 10 por material). "Energy" en una tabla sube la barra.
 */
namespace JamLoot
{
	/** JSON completo (se carga una vez). */
	TSharedPtr<FJsonObject> Config();

	/** Cuantas unidades de ItemId caben todavia en la mochila. */
	int32 Room(const UInventoryComponent* Inventory, FName ItemId);

	/** Agrega hasta Count respetando topes; devuelve cuantas entraron. */
	int32 Give(UInventoryComponent* Inventory, FName ItemId, int32 Count);

	/** Tira la tabla (common, device, rare, scrap): ids -> cantidad. "Energy" viene como id aparte. */
	TMap<FName, int32> Roll(const FString& Table);

	/** Entrega un lote a un actor: piezas y materiales a su mochila, energia a la barra si es el jugador. Texto de lo que recibio. */
	FString GiveBundle(AActor* Taker, const TMap<FName, int32>& Bundle, TMap<FName, int32>* OutLeftover = nullptr);

	/** "+2 Rueda, +1 Lanzallamas, +15 energia" (nombres en espanol para el jugador). */
	FString Describe(const TMap<FName, int32>& Bundle);

	/** Nombre legible en espanol de un tipo de pieza ("Wheel" -> "Rueda", de names_es de BuildKnowledge.json); si no lo conoce, el tipo tal cual. */
	FString PartName(const FString& Type);

	/** Presentacion del loot: nombres, iconos y colores; no modifica inventario ni topes. */
	FString ItemName(FName ItemId);
	FString IconName(FName ItemId);
	FString ItemTier(FName ItemId);
	FLinearColor TierColor(const FString& Tier);
	FLinearColor ItemColor(FName ItemId);

	/** Z del suelo bajo (X, Y); false si abajo solo hay vacio. */
	bool GroundAt(UWorld* World, float X, float Y, FVector& Out, const AActor* Ignore = nullptr);

	float EnergyMin();
	float EnergyMax();
}
