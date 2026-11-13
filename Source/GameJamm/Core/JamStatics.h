#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Core/JamTypes.h"
#include "JamStatics.generated.h"

UCLASS()
class GAMEJAMM_API UJamStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Equipo de un actor. Busca UHealthComponent en el actor; si es un Controller usa su Pawn;
	 * si no tiene, prueba con su Instigator y luego su Owner (útil para proyectiles y piezas construidas).
	 */
	UFUNCTION(BlueprintPure, Category = "Jam|Team")
	static EJamTeam GetActorTeam(const AActor* Actor);

	/** true si los equipos son distintos (Neutral es enemigo de Red y Blue; dos Neutral no). */
	UFUNCTION(BlueprintPure, Category = "Jam|Team")
	static bool AreEnemies(const AActor* A, const AActor* B);

	UFUNCTION(BlueprintPure, Category = "Jam|Team")
	static EJamTeam GetOpposingTeam(EJamTeam Team);
};
