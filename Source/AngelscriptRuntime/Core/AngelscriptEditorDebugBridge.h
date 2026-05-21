#pragma once

#include "CoreMinimal.h"

class UASClass;

DECLARE_MULTICAST_DELEGATE_TwoParams(FAngelscriptDebugListAssets, TArray<FString>, UASClass*);
DECLARE_MULTICAST_DELEGATE_OneParam(FAngelscriptEditorCreateBlueprint, UASClass*);
DECLARE_DELEGATE_RetVal_OneParam(FString, FAngelscriptEditorGetCreateBlueprintDefaultAssetPath, UASClass*);

class ANGELSCRIPTRUNTIME_API FAngelscriptEditorDebugBridge
{
public:
	static FAngelscriptDebugListAssets& GetDebugListAssets();
	static FAngelscriptEditorCreateBlueprint& GetEditorCreateBlueprint();
	static FAngelscriptEditorGetCreateBlueprintDefaultAssetPath& GetEditorGetCreateBlueprintDefaultAssetPath();

#if WITH_DEV_AUTOMATION_TESTS
	static void ResetForTesting();
#endif
};
