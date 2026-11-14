#pragma once
#include "Building/BuildRecipe.h"

namespace JamInventorySolver
{
	// Available usa IDs Part.<Tipo>. No consume: la confirmacion ocurre despues de ensamblar.
	bool Fit(const FBuildRecipe& Wanted, const TMap<FName, int32>& Available, FBuildRecipe& Out, FString& Note);
}
