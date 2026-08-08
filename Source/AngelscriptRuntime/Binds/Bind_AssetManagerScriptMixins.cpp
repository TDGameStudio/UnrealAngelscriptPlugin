#include "AngelscriptBinds.h"
#include "Core/FunctionCallers.h"

#include "FunctionLibraries/UAssetManagerMixinLibrary.h"

namespace
{
	void BindAssetManagerScriptMixins(FAngelscriptBinds& Binds)
	{
		// UHT cannot direct-bind these signatures. Install their exact entries in the
		// selected engine before GeneratedBindings installs reflective stubs.
		Binds.RegisterFunctionBindingForTarget(
			UAssetManagerMixinLibrary::StaticClass(),
			"CallOrRegister_OnCompletedInitialScan",
			{ERASE_FUNCTION_PTR(UAssetManagerMixinLibrary::CallOrRegister_OnCompletedInitialScan, (UAssetManager*, UObject*, const FName&), ERASE_ARGUMENT_PACK(void))});
		Binds.RegisterFunctionBindingForTarget(
			UAssetManagerMixinLibrary::StaticClass(),
			"GetPrimaryAssetData",
			{ERASE_FUNCTION_PTR(UAssetManagerMixinLibrary::GetPrimaryAssetData, (UAssetManager*, const FPrimaryAssetId&, FAssetData&), ERASE_ARGUMENT_PACK(bool))});
		Binds.RegisterFunctionBindingForTarget(
			UAssetManagerMixinLibrary::StaticClass(),
			"GetPrimaryAssetObject",
			{ERASE_FUNCTION_PTR(UAssetManagerMixinLibrary::GetPrimaryAssetObject, (UAssetManager*, const FPrimaryAssetId&), ERASE_ARGUMENT_PACK(UObject*))});
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_AssetManagerScriptMixins(
	TEXT("AssetManagerScriptMixins.GeneratedOverrides"),
	EAngelscriptBindPhase::ManualBindings,
	&BindAssetManagerScriptMixins);
