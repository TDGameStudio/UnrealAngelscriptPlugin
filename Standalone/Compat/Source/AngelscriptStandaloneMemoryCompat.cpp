#include "CoreMinimal.h"
#include "angelscript.h"

extern "C"
{
	int asSetGlobalMemoryFunctions(
		asALLOCFUNC_t Allocate,
		asFREEFUNC_t Free)
	{
		if (Allocate == nullptr || Free == nullptr)
		{
			return asINVALID_ARG;
		}

		FMemory::SetAllocationFunctions(Allocate, Free);
		return asSUCCESS;
	}

	int asResetGlobalMemoryFunctions()
	{
		FMemory::ResetAllocationFunctions();
		return asSUCCESS;
	}
}
