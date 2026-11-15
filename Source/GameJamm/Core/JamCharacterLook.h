#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/JamTypes.h"
#include "JamCharacterLook.generated.h"

class UAnimSequenceBase;
class UHealthComponent;
class UJamWeaponComponent;
class UMaterialInterface;
enum class EJamWeapon : uint8;

/**
 * Figura rigeada de un personaje (pedido de arte en docs/ARTE-TRIPO.md, seccion Personajes). Lee la entrada Key de
 * Content/Game/Art/Tripo/Characters/Characters.json: pone la SkeletalMesh en GetMesh(), oculta el cuerpo de cilindros
 * (Body/Nose/Head), pinta por equipo con material y reproduce una animacion suelta por estado (sin Anim Blueprint).
 * Sin archivo, sin entrada o si la malla no carga, no hace nada y queda el cilindro. No toca colision, dano ni velocidad.
 */
UCLASS(ClassGroup = (Jam))
class GAMEJAMM_API UJamCharacterLook : public UActorComponent
{
	GENERATED_BODY()

public:
	UJamCharacterLook();

	/** Player, Minion, BotHero, Guard o Thrower. Fijarlo antes de BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	FName Key = TEXT("Minion");

	/** Accion de una vez (Attack, Shoot, Hit): la deja terminar antes de volver a caminar/quieto. */
	UFUNCTION(BlueprintCallable, Category = "Look")
	void PlayAction(FName Action);

	/** true si la figura rigeada se aplico (si no, sigue el cilindro). */
	UFUNCTION(BlueprintPure, Category = "Look")
	bool IsApplied() const { return bApplied; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	struct FAnim
	{
		TObjectPtr<UAnimSequenceBase> Asset;
		float Speed = 0.f;
		float Rate = 1.f;
	};

	bool Apply();
	const FAnim* Find(FName State) const;
	void Play(FName State, bool bLoop, float Rate);

	UFUNCTION()
	void HandleWeaponAttack(UJamWeaponComponent* InWeapon, EJamWeapon Kind);

	UFUNCTION()
	void HandleHealthChanged(UHealthComponent* HealthComponent, float NewHealth, float Delta);

	TMap<FName, FAnim> Anims;

	UPROPERTY()
	TMap<EJamTeam, TObjectPtr<UMaterialInterface>> TeamMaterials;

	/** Las animaciones sueltas no las guarda nadie mas: se retienen aqui para el GC. */
	UPROPERTY()
	TArray<TObjectPtr<UObject>> Keep;

	bool bApplied = false;
	bool bHasTeam = false;
	EJamTeam LastTeam = EJamTeam::Neutral;
	float RunAbove = 220.f;
	FName Current;
	FName PendingAction;
	float ActionUntil = -1.f;
};
