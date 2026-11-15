#pragma once

#include "CoreMinimal.h"
#include "AI/JamMinion.h"
#include "JamCampGuard.generated.h"

/**
 * Enemigo de campamento neutral (GDD 11): cuida el cofre de su isla y ataca a cualquier equipo que entre.
 *   Guardia: 80 HP, cuerpo a cuerpo; persigue hasta 20 m de su campamento y vuelve.
 *   Lanzador: 50 HP, tira piedras desde 15 m; al morir suelta tambien un material de fusion.
 * Ambos dan +10 de energia a quien los mata. Los pone AJamLootDirector (Loot.json, "camps") y reaparecen.
 */
UCLASS()
class GAMEJAMM_API AJamCampGuard : public AJamMinion
{
	GENERATED_BODY()

public:
	AJamCampGuard();

	/** Llamar antes de FinishSpawning. */
	void InitGuard(bool bInThrower, const FVector& InHome);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jam|Camp")
	float LeashRange = 2000.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Jam|Camp")
	bool bThrower = false;

protected:
	virtual void Think() override;
	virtual void HandleDied(UHealthComponent* HealthComponent, AActor* Killer) override;

private:
	AActor* FindIntruder() const;
	bool GroundAhead(const FVector& Dir) const;
	void GoTo(const FVector& Where);

	FVector Home = FVector::ZeroVector;
	TWeakObjectPtr<AActor> LastIntruder;
};
