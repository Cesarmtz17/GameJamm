#pragma once

#include "CoreMinimal.h"
#include "JamTypes.generated.h"

/** Equipo de cualquier actor. Neutral = campamentos/monstruos/objetos. CONTRATO COMPARTIDO: no cambiar sin avisar. */
UENUM(BlueprintType)
enum class EJamTeam : uint8
{
	Neutral,
	Red,
	Blue
};
