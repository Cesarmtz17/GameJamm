#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JamLootChest.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;

/**
 * Cofre o monton de chatarra (GDD 4). Se abre al acercarse (la E ya es del asiento): tira su tabla de Loot.json y lo mete
 * en la mochila con topes; lo que no cabe queda tirado como pickup. Reaparece a los RespawnSeconds (<0: una sola vez).
 */
UCLASS()
class GAMEJAMM_API AJamLootChest : public AActor
{
	GENERATED_BODY()

public:
	AJamLootChest();

	/** Tabla de Loot.json: common, device, rare o scrap. Se ignora si FixedItems no esta vacio. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	FString Table = TEXT("common");

	/** Contenido fijo (cofre garantizado del spawn). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	TMap<FName, int32> FixedItems;

	/** Texto extra al abrirlo (pista para el jugador). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	FString Message;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	float RespawnSeconds = 60.f;

	UPROPERTY(VisibleAnywhere, Category = "Loot")
	TObjectPtr<UStaticMeshComponent> Base;

	UPROPERTY(VisibleAnywhere, Category = "Loot")
	TObjectPtr<UStaticMeshComponent> Lid;

	UPROPERTY(VisibleAnywhere, Category = "Loot")
	TObjectPtr<USphereComponent> Trigger;

	/** Faro de loot cerrado: luz sin sombras y columna fina; valores en Loot.json -> visuals.beacons. */
	UPROPERTY(VisibleAnywhere, Category = "Loot|Visual")
	TObjectPtr<UPointLightComponent> BeaconLight;

	UPROPERTY(VisibleAnywhere, Category = "Loot|Visual")
	TObjectPtr<UStaticMeshComponent> BeaconColumn;

	/** Cambia a los modelos de cofre raro o de chatarra (antes de BeginPlay). */
	void SetLook(bool bRare, bool bScrap);

	/** Abre el cofre para Taker. false si ya estaba abierto. */
	UFUNCTION(BlueprintCallable, Category = "Loot")
	bool OpenFor(AActor* Taker);

	UFUNCTION(BlueprintPure, Category = "Loot")
	bool IsOpen() const { return bOpen; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);

	void Refill();
	void RefreshBeacon();

	UPROPERTY(Transient)
	TObjectPtr<class UMaterialInstanceDynamic> BeaconMaterial;

	UPROPERTY()
	TObjectPtr<class UStaticMesh> RareBaseMesh;

	UPROPERTY()
	TObjectPtr<class UStaticMesh> RareLidMesh;

	UPROPERTY()
	TObjectPtr<class UStaticMesh> ScrapMesh;

	bool bOpen = false;
	bool bScrapLook = false;
	float LidAlpha = 0.f;
	float OpenRoll = 100.f;
	FTimerHandle RefillTimer;
};
