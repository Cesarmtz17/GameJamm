#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/JamGunSpec.h"
#include "JamWeaponComponent.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UPointLightComponent;
class USceneComponent;

UENUM(BlueprintType)
enum class EJamWeapon : uint8
{
	Sword,
	Bow,
	/** Arma de mano pedida a la IA (GDD 5.1). Solo el jugador; va al final para no mover los valores anteriores. */
	Generated
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FJamOnWeaponHit, UJamWeaponComponent*, Weapon, AActor*, Target, float, Damage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FJamOnWeaponAttack, UJamWeaponComponent*, Weapon, EJamWeapon, Kind);

/**
 * Armas del jugador (GDD 8.1): espada (20 cada 0.6 s, 2 m) y arco (15 por flecha cada 0.8 s, 40 m, flecha fisica).
 * El dano va por ApplyDamage a UHealthComponent y nunca a aliados. Valores ajustables en BP sin recompilar.
 * Fusion (GDD 8.2): Fuse / FuseNext (tecla F) pegan un material; sus efectos viven en UJamStatusComponent.
 * Arma pedida (GDD 5.1): RequestGeneratedWeapon("un rifle que dispare hielo") arma en la mano un arma de piezas
 * (Weapons.json) que dispara con la energia del jugador; Q / rueda pasan por espada > arco > arma pedida.
 */
UCLASS(ClassGroup = (Jam), meta = (BlueprintSpawnableComponent))
class GAMEJAMM_API UJamWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJamWeaponComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	EJamWeapon Current = EJamWeapon::Sword;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Sword")
	float SwordDamage = 20.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Sword")
	float SwordInterval = 0.6f;

	/** Alcance del golpe (cm) medido desde el centro del personaje. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Sword")
	float SwordRange = 200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Sword")
	float SwordKnockback = 450.f;

	/** Medio angulo del tajo (grados a cada lado del frente del personaje). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Sword")
	float SwordArc = 75.f;

	/** Segundos desde que se aprieta hasta que pega: cuando la hoja cruza al frente en el espadazo de la figura. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Sword")
	float SwordHitDelay = 0.19f;

	/** Segundos desde que se aprieta hasta que sale la flecha: cuando la mano suelta la cuerda. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Bow")
	float BowReleaseDelay = 0.17f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Bow")
	float BowDamage = 15.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Bow")
	float BowInterval = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Bow")
	float ArrowSpeed = 4500.f;

	/** Distancia de apuntado (cm): a donde mira la camara, hasta 40 m. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Bow")
	float BowRange = 4000.f;

	/** Dano extra de un material fusionado (lo llena Fusion). */
	UPROPERTY(BlueprintReadWrite, Category = "Weapon")
	float BonusDamage = 0.f;

	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FJamOnWeaponHit OnWeaponHit;

	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FJamOnWeaponAttack OnWeaponAttack;

	/** Ataca con el arma en mano si ya paso su intervalo. true si ataco. */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	bool Attack();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void NextWeapon();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void SetWeapon(EJamWeapon Weapon);

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsReady() const;

	/** Lo llaman la espada y la flecha al pegar: aplica dano (sin aliados) y avisa. true si hubo dano. */
	bool DealDamage(AActor* Target, float Damage, const FVector& Push);

	/**
	 * Fusion (GDD 8.2, valores en Content/Game/Combat/Data/Fusion.json): pega al arma en mano el material Mat.<Id> del
	 * inventario. Espada: gasta 1 y dura 20 golpes (Bomba, 1); reemplaza al anterior. Arco: elige la punta y cada flecha gasta 1.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Fusion")
	bool Fuse(FName MaterialId);

	/** Tecla F: fusiona el siguiente material que tengas (Fuego, Hielo, Rayo, Roca, Bomba). */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Fusion")
	bool FuseNext();

	/** Material del arma en mano (None si va sola). */
	UFUNCTION(BlueprintPure, Category = "Weapon|Fusion")
	FName GetFused() const;

	/** Golpes que le quedan a la espada (en el arco: materiales en la mochila). */
	UFUNCTION(BlueprintPure, Category = "Weapon|Fusion")
	int32 GetFusedUses() const;

	/** Intervalo real del arma en mano (la Roca hace lenta la espada). */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	float GetInterval() const;

	/** Efecto del material al pegar (Target puede ser nulo si la flecha dio en el suelo). Lo llaman la espada y AJamArrow. */
	void ApplyFusionHit(FName MaterialId, AActor* Target, const FVector& Where, bool bArrow);

	/**
	 * Pega el arma en mano a un hueso de la figura rigeada (UJamCharacterLook, campo Weapon de Characters.json).
	 * Con mano, el tajo lo hace la animacion Attack y no el balanceo por codigo. Sword/Bow = posicion local en el hueso.
	 */
	void AttachToHand(USceneComponent* Skeleton, FName SwordSocket, const FTransform& Sword, FName BowSocket, const FTransform& Bow);

	/** true si el arma va pegada a un hueso (figura rigeada). */
	bool IsInHand() const { return HandParent.IsValid(); }

	// ---- Arma de mano pedida (GDD 5.1; datos en Content/Game/Combat/Data/Weapons.json) ----

	/** jam.Weapon.Generated (1 por defecto). 0 = apagada: la caja de pedir manda todo a construccion y Q no la ofrece. */
	static bool IsGeneratedEnabled();

	/** true si la frase pide un arma de mano ("un rifle que dispare hielo") y no una construccion. false si esta apagada. */
	static bool IsWeaponPhrase(const FString& Phrase);

	/**
	 * Decide el arma, la arma de piezas en la mano, cambia a ella y avisa ("Armaste: Rifle de hielo...").
	 * Sin clave de OpenAI decide por palabras clave y la equipa al momento. Con clave devuelve true, avisa "Pensando..." y el
	 * modelo ELIGE cuerpo, boca y extras (el codigo pone los numeros); si no contesta en el corte (3 s), queda la de palabras.
	 * No gasta piezas ni materiales del inventario. false si no es frase de arma o la variable esta apagada.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Generated")
	bool RequestGeneratedWeapon(const FString& Phrase);

	/** true si ya pediste un arma (sigue disponible con Q aunque tengas otra en la mano). */
	UFUNCTION(BlueprintPure, Category = "Weapon|Generated")
	bool HasGeneratedWeapon() const;

	/** Texto para el HUD: "Rifle de hielo - Hielo, 4 de energia por disparo". Vacio si no hay arma pedida. */
	UFUNCTION(BlueprintPure, Category = "Weapon|Generated")
	FString GetGeneratedHudText() const;

	/** Nombre del arma pedida ("Rifle de hielo"), vacio si no hay. */
	UFUNCTION(BlueprintPure, Category = "Weapon|Generated")
	FString GetGeneratedName() const;

	/** Energia del jugador que gasta cada clic del arma pedida (0 si no hay). */
	UFUNCTION(BlueprintPure, Category = "Weapon|Generated")
	float GetGeneratedEnergyCost() const;

	/** Especificacion completa del arma pedida (efecto, dano, sonido...). Valida solo si HasGeneratedWeapon(). */
	const FJamGunSpec& GetGeneratedSpec() const { return Gun; }

	/** Afinar en vivo como va el arma pedida en la mano (Jam.Arma.Mano): mismos campos que hand.rot / loc / scale. */
	void SetGeneratedGrip(const FRotator& Rot, const FVector& Loc, float Scale);

	/**
	 * Para UJamFigureAnim: true si el arma pedida esta en la mano de una figura rigeada. OutBone = hueso que la lleva,
	 * OutInBone = rotacion del arma en ese hueso (X = canon, Z = arriba), OutAim = 1 si acaba de disparar (brazo a la mira).
	 */
	bool GetGeneratedPose(FName& OutBone, FQuat& OutInBone, float& OutAim) const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	void SwingSword();
	void ShootArrow();
	bool FireGenerated();
	/** Pone en la mano el arma ya decidida y avisa. ChosenBy = quien eligio ("OpenAI"); vacio = respaldo por palabras clave. */
	void EquipGenerated(const FJamGunSpec& Spec, const FString& ChosenBy);
	void BuildGunMesh();
	void PlaceGun();
	FTransform MakeGunGrip(USceneComponent* Parent, FName Socket) const;
	/** Range <= 0 usa BowRange. OutTarget (opcional) = punto al que mira el centro de la pantalla. */
	void GetAim(FVector& OutFrom, FVector& OutDir, float Range = -1.f, FVector* OutTarget = nullptr) const;
	void RefreshHandMesh();
	void RefreshFuseMesh();
	void Explode(const FVector& Where, float Damage, float Radius);
	/** Aviso al jugador (JamMsg, se ve en Shipping). El de un argumento sale en ambar (falto algo). */
	void Notify(const FString& Text) const;
	void Notify(const FString& Text, const FLinearColor& Color, FName Key) const;
	float MatNumber(FName MaterialId, const TCHAR* Field, float Default) const;
	TArray<AActor*> EnemiesNear(const FVector& Where, float Radius) const;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> HandMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SwordMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> BowMesh;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> FuseMesh;

	UPROPERTY()
	TMap<FName, TObjectPtr<UStaticMesh>> FuseMeshes;

	FName FusedSword;
	FName FusedBow;
	int32 SwordUses = 0;

	float LastAttackTime = -100.f;
	float PendingAt = -1.f;       // golpe/flecha en espera (ver SwordHitDelay, BowReleaseDelay)
	EJamWeapon PendingKind = EJamWeapon::Sword;
	float FaceYaw = 0.f;          // el personaje voltea a la mira mientras ataca
	float FaceUntil = -1.f;
	float SwingAlpha = 1.f; // 0..1 durante el tajo

	TWeakObjectPtr<USceneComponent> HandParent;
	FName SwordSocket, BowSocket;
	FTransform SwordInHand, BowInHand;

	// ---- Arma pedida ----
	/** Raiz del arma pedida: va pegada al hueso de la mano; sus hijos son las piezas (sin fisica ni colision). */
	UPROPERTY()
	TObjectPtr<USceneComponent> GunRoot;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> GunParts;

	/** Fogonazo: luz en la boca que se prende un instante al disparar. */
	UPROPERTY()
	TObjectPtr<UPointLightComponent> GunFlash;

	/** Malla del proyectil del arma pedida: se guarda para que no se descargue entre disparos. */
	UPROPERTY()
	TObjectPtr<UStaticMesh> GunShotMesh;

	FJamGunSpec Gun;
	bool bHasGun = false;
	/** Contador de pedidos de arma: si llega la respuesta del modelo de un pedido viejo (ya se pidio otra), se ignora. */
	int32 GunRequestId = 0;
	FName GunSocket;                 // hueso que la lleva (None si la figura no esta rigeada)
	FTransform GunInHand;            // arma respecto a ese hueso
	float FlashUntil = -1.f;
	float LastNoEnergyNote = -100.f; // el aviso de "sin energia" no se repite cada frame
};
