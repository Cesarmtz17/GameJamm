#pragma once

#include "CoreMinimal.h"
#include "AI/JamMinion.h"
#include "JamBotHero.generated.h"

class AJamBuildSpawner;

/**
 * Bot heroe (GDD 11): completa el 3v3. Misma maquina de estados que el minion mas:
 *   Retirarse (vida < 30 %: vuelve a su base y se cura) -> Defender (si atacan su torre) -> Empujar con la oleada.
 * Constructor: pide al asistente de construccion frases fijas que rotan (AJamBuildSpawner::SetPromptAndBuild) y la enciende.
 * Al morir reaparece en su base con el mismo tiempo que el jugador. No da energia al morir (no es minion).
 * Pendiente: lootear (llega con la tarea 4) y la espada/arco de Combate (tarea 2); hoy pega con el ataque base de su rol.
 */
UCLASS()
class GAMEJAMM_API AJamBotHero : public AJamMinion
{
	GENERATED_BODY()

public:
	AJamBotHero();

	virtual void InitAgent(EJamAgentKind InKind, EJamTeam InTeam) override;

	/** Frases que rota el Constructor (GDD 11). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jam|Bot")
	TArray<FString> BuildPrompts;

	/** Segundos entre construcciones del Constructor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jam|Bot")
	float BuildEvery = 60.f;

protected:
	virtual void BeginPlay() override;
	virtual void Think() override;
	virtual void HandleDied(UHealthComponent* HealthComponent, AActor* Killer) override;

	void Respawn();
	void BuildSomething();
	FVector HomeLocation() const;

	bool bRetreating = false;
	int32 PromptIndex = 0;
	float NextBuildTime = 0.f;
	TWeakObjectPtr<AJamBuildSpawner> MyBuild;
	FTimerHandle RespawnTimer;
};
