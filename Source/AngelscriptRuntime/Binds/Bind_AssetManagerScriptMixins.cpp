#include "AngelscriptBinds.h"
#include "Core/FunctionCallers.h"

#include "FunctionLibraries/UAssetManagerMixinLibrary.h"

/**
 * UAssetManager generated-binding overrides.
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                              | Purpose / parameter notes                                                                                          |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | void AssetManager.CallOrRegister_OnCompletedInitialScan(UObject Object, FName Function); | Invokes Object.Function after the initial scan, or immediately if scanning has already completed.                  |
 * |                                                                                          | @param Object Callback receiver. @param Function Reflected no-argument callback name.                              |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | bool AssetManager.GetPrimaryAssetData(const FPrimaryAssetId& Id, FAssetData&out Data);   | Looks up already-scanned metadata. @param Id Primary asset identifier. @param Data Receives metadata.              |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | UObject AssetManager.GetPrimaryAssetObject(const FPrimaryAssetId& Id);                   | Returns the loaded primary asset object, or null when it is not resident.                                          |
 * |                                                                                          | @param Id Primary asset identifier.                                                                                |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_AssetManagerScriptMixins(
	TEXT("AssetManagerScriptMixins.GeneratedOverrides"),
	EAngelscriptBindPhase::ManualBindings,
	[](FAngelscriptBinds& Binds)
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
	});
