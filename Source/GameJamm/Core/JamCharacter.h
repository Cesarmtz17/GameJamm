#pragma once

#include "CoreMinimal.h"
#include "Building/JamPlayConsole.h"
#include "JamCharacter.generated.h"

class UHealthComponent;
class UInventoryComponent;

/**
 * Personaje de la partida (Core). Hereda la tercera persona y todas las teclas de construccion de AJamTestCharacter
 * (Building) y le agrega vida, equipo, inventario y muerte -> respawn en su base (lo programa AJamGameMode).
 * GDD 9: 150 HP; reaparece en 5 s + 1 s por minuto (tope 15 s).
 */
UCLASS()
class GAMEJAMM_API AJamCharacter : public AJamTestCharacter
{
	GENERATED_BODY()

public:
	AJamCharacter();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Jam")
	TObjectPtr<UHealthComponent> Health;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Jam")
	TObjectPtr<UInventoryComponent> Inventory;

	/** Piezas con las que se empieza (IDs Part.<Tipo>) hasta que exista loot; construir las consume y desarmar las devuelve. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jam")
	TMap<FName, int32> StartingItems;

	virtual void Tick(float DeltaSeconds) override;

	/** Caer de la isla (KillZ) cuenta como muerte: reaparece en la base en vez de destruirse. */
	virtual void FellOutOfWorld(const class UDamageType& DmgType) override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleDied(UHealthComponent* HealthComponent, AActor* Killer);
};
