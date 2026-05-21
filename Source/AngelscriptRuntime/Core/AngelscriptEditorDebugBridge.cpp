#include "AngelscriptEditorDebugBridge.h"

namespace
{
	FAngelscriptDebugListAssets& DebugListAssets()
	{
		static FAngelscriptDebugListAssets Delegate;
		return Delegate;
	}

	FAngelscriptEditorCreateBlueprint& EditorCreateBlueprint()
	{
		static FAngelscriptEditorCreateBlueprint Delegate;
		return Delegate;
	}

	FAngelscriptEditorGetCreateBlueprintDefaultAssetPath& EditorGetCreateBlueprintDefaultAssetPath()
	{
		static FAngelscriptEditorGetCreateBlueprintDefaultAssetPath Delegate;
		return Delegate;
	}
}

FAngelscriptDebugListAssets& FAngelscriptEditorDebugBridge::GetDebugListAssets()
{
	return DebugListAssets();
}

FAngelscriptEditorCreateBlueprint& FAngelscriptEditorDebugBridge::GetEditorCreateBlueprint()
{
	return EditorCreateBlueprint();
}

FAngelscriptEditorGetCreateBlueprintDefaultAssetPath& FAngelscriptEditorDebugBridge::GetEditorGetCreateBlueprintDefaultAssetPath()
{
	return EditorGetCreateBlueprintDefaultAssetPath();
}

#if WITH_DEV_AUTOMATION_TESTS
void FAngelscriptEditorDebugBridge::ResetForTesting()
{
	DebugListAssets().Clear();
	EditorCreateBlueprint().Clear();
	EditorGetCreateBlueprintDefaultAssetPath().Unbind();
}
#endif
