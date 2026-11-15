#pragma once

#include "CoreMinimal.h"

class UStaticMesh;

/**
 * Una pieza visual del arma pedida (o su proyectil), en el espacio del arma:
 * origen = centro de la mano, X = hacia donde dispara, Y = derecha, Z = arriba. Medidas en cm.
 */
struct FJamGunPiece
{
	FString Mesh;                          // nombre corto de /Game/Game/Building/Parts o ruta completa
	FVector Size = FVector::ZeroVector;    // largo, ancho, alto que debe medir (0 = conserva la proporcion de la malla)
	FVector Pos = FVector::ZeroVector;     // centro de la pieza
	FRotator Rot = FRotator::ZeroRotator;
	bool bGlow = false;                    // material emisivo con el color de la boca (proyectiles)
};

/** Arma de mano ya decidida (GDD 5.1): numeros finales + piezas visuales. Todo sale de Weapons.json. */
struct FJamGunSpec
{
	FString Name;        // "Rifle de hielo"
	FString BodyName;    // "Rifle"
	FString MuzzleName;  // "Hielo" (efecto legible para el HUD)
	TArray<FString> ExtraNames; // extras pedidos, sin los que el cuerpo trae de fabrica: "mira", "cargador"
	FString Sound;       // nombre del SFX de disparo (aun no se toca; ver UJamWeaponComponent::FireGenerated)
	FName Effect;        // material de Fusion.json (Fire, Ice, Shock, Rock, Bomb) o None
	bool bArea = true;   // true = efecto de flecha (en area); false = efecto de golpe (Ice: ralentiza y congela a los N impactos)
	int32 EffectPellets = 0; // cuantos proyectiles de la rafaga llevan el efecto (0 = todos)

	float Damage = 12.f;
	float Interval = 0.5f;
	float Speed = 4500.f;
	float Gravity = 0.2f;
	float Energy = 4.f;   // energia del jugador por clic
	float Range = 4500.f;
	float Push = 250.f;
	int32 Pellets = 1;
	float SpreadDeg = 0.f;
	float FanJitter = 0.25f; // variacion vertical de los perdigones, en fraccion de SpreadDeg
	FLinearColor Color = FLinearColor(1.f, 0.85f, 0.5f);

	TArray<FJamGunPiece> Pieces;                  // empunadura + cuerpo + boca + extras
	FVector MuzzlePoint = FVector(40.f, 0.f, 8.f); // de donde sale el disparo
	FJamGunPiece Projectile;

	// Mano (hand de Weapons.json)
	FName Socket = TEXT("hand_r");
	bool bAutoGrip = true;
	FVector HandLoc = FVector::ZeroVector;
	FRotator HandRot = FRotator::ZeroRotator;
	float HandScale = 1.f;
	FVector LooseLoc = FVector(45.f, 38.f, 10.f);
	float AimSeconds = 1.2f;

	// Fogonazo
	float FlashSeconds = 0.07f;
	float FlashIntensity = 6000.f;
	float FlashRadius = 350.f;

	// Retroceso visual: vuelve a la pose de reposo sin mover la camara ni la punteria.
	float RecoilDistance = 2.5f; // cm hacia atras, independientes de hand.scale
	float RecoilPitch = 2.f;    // grados de elevacion de la boca
	float RecoilSeconds = 0.16f;
};

/**
 * Decision del arma de mano pedida, sin clave: palabras clave de Content/Game/Combat/Data/Weapons.json.
 * El archivo se lee una vez y se guarda; Reload() lo vuelve a leer (lo usa Jam.Arma para afinar sin compilar).
 */
namespace JamGun
{
	/** jam.Weapon.Generated (1 por defecto). 0 = sin arma pedida: todo pedido va a construccion y Q no la ofrece. */
	GAMEJAMM_API bool IsEnabled();

	/** Minusculas, sin tildes, solo letras y numeros, con un espacio entre palabras y en los extremos (" un rifle "). */
	GAMEJAMM_API FString Normalize(const FString& Text);

	/** true si la frase pide un arma de mano (y no una construccion). false con la variable apagada. */
	GAMEJAMM_API bool IsWeaponPhrase(const FString& Phrase);

	/** Arma la especificacion a partir de la frase. false si no es frase de arma. */
	GAMEJAMM_API bool Decide(const FString& Phrase, FJamGunSpec& Out);

	/**
	 * Igual que Decide, pero el cuerpo, la boca y los extras son los que ELIGIO el modelo de decision (ids de Weapons.json).
	 * El codigo sigue decidiendo lo posible: un id vacio o que no existe cae a la palabra clave, y los numeros, limites y
	 * piezas salen de Weapons.json. ExtraIds = extras que el modelo marco con si (los de fabrica del cuerpo van siempre).
	 */
	GAMEJAMM_API bool DecideWithIds(const FString& Phrase, const FString& BodyId, const FString& MuzzleId, const TArray<FString>& ExtraIds, FJamGunSpec& Out);

	/**
	 * Cuerpo JSON para la Decisions API de OpenAI: un choice sobre los id de bodies, un choice sobre los id de muzzles (mas
	 * "Default" = no se nombro efecto) y un predicate por cada id de extras. Textos en el bloque "decision" de Weapons.json.
	 */
	GAMEJAMM_API FString BuildDecisionRequest(const FString& Phrase);

	/**
	 * Lee answers[] de esa peticion. Cuerpo o boca negados ("refusal"), con poca confianza o "Default" quedan vacios (decide la
	 * palabra clave); un extra negado se decide por su palabra clave. false si no es JSON valido o todo viene negado.
	 */
	GAMEJAMM_API bool ParseDecisionResponse(const FString& ResponseJson, const FString& Phrase, FString& OutBodyId, FString& OutMuzzleId, TArray<FString>& OutExtraIds, FString& OutNote);

	/** Transform local de la pieza para que la caja de la malla mida Size y su centro quede en Pos (mide la malla real). */
	GAMEJAMM_API FTransform FitPiece(const UStaticMesh* Mesh, const FJamGunPiece& Piece);

	/** Carga la malla de una pieza ("SM_DEV_FROST" o ruta completa). nullptr si no existe. */
	GAMEJAMM_API UStaticMesh* LoadMesh(const FString& NameOrPath);

	/** Vuelve a leer Weapons.json en el siguiente uso. */
	GAMEJAMM_API void Reload();

	/** "4" o "1.5": numero corto para textos del jugador. */
	GAMEJAMM_API FString Number(float Value);
}
