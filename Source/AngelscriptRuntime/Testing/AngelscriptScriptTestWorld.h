#pragma once

#include "CoreMinimal.h"

class UClass;

/**
 * Private local-World policy for reflected script tests. The actual state is
 * owned directly by one execution context; no public World fixture UObject is
 * introduced.
 */
class FAngelscriptScriptTestWorld
{
public:
	static UClass* ResolveCurrentClass(UClass* Candidate);
	static bool AreTickArgumentsValid(
		float DeltaSeconds,
		int32 NumTicks);
};
