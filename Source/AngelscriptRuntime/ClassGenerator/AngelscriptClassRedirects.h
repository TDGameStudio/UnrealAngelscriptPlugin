#pragma once

#include "CoreMinimal.h"

struct ANGELSCRIPTRUNTIME_API FAngelscriptClassRedirects
{
	static constexpr const TCHAR* GeneratedRedirectSource = TEXT("Angelscript generated class rename redirect");

	static FString MakeScriptClassPath(const FString& ClassName);
	static bool TryAddGeneratedCoreRedirect(const FString& OldClassName, const FString& NewClassName);

#if WITH_AUTOMATION_TESTS
	static void SetCoreRedirectTargetIniOverrideForTesting(const FString& InIniPath);
	static void ResetCoreRedirectTargetIniOverrideForTesting();
#endif
};
