#pragma once
#include "CoreMinimal.h"
#include "Binds/Bind_Helpers.h"
#include "Binds/Bind_FInstancedStruct_Functions.h"
#include "Binds/Bind_TSubclassOf.h"
#include "Binds/Bind_TMap.h"
#include "Binds/Bind_TSet.h"
//#include "Binds/Bind_TOptional.h"
#include "Binds/Bind_Delegates.h"
#include "Binds/Bind_Actor.h"

struct ANGELSCRIPTRUNTIME_API FStaticJITHelperFunctions
{
	FORCEINLINE static bool FName_Equals(const FName& A, const FName& B)
	{
		return A == B;
	}
};
