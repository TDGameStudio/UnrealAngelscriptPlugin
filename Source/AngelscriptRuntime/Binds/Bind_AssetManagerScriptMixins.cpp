#include "AngelscriptBinds.h"
#include "Core/FunctionCallers.h"

#include "FunctionLibraries/UAssetManagerMixinLibrary.h"

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_AssetManagerScriptMixins((int32)FAngelscriptBinds::EOrder::Late + 49, []
{
	// UHT cannot direct-bind a few AssetManager mixin signatures, so provide exact
	// entries before the generated function table installs reflective stubs.
	FAngelscriptBinds::RegisterFunctionBinding(
		UAssetManagerMixinLibrary::StaticClass(),
		"CallOrRegister_OnCompletedInitialScan",
		{ ERASE_FUNCTION_PTR(UAssetManagerMixinLibrary::CallOrRegister_OnCompletedInitialScan, (UAssetManager*, UObject*, const FName&), ERASE_ARGUMENT_PACK(void)) });
	FAngelscriptBinds::RegisterFunctionBinding(
		UAssetManagerMixinLibrary::StaticClass(),
		"GetPrimaryAssetData",
		{ ERASE_FUNCTION_PTR(UAssetManagerMixinLibrary::GetPrimaryAssetData, (UAssetManager*, const FPrimaryAssetId&, FAssetData&), ERASE_ARGUMENT_PACK(bool)) });
	FAngelscriptBinds::RegisterFunctionBinding(
		UAssetManagerMixinLibrary::StaticClass(),
		"GetPrimaryAssetObject",
		{ ERASE_FUNCTION_PTR(UAssetManagerMixinLibrary::GetPrimaryAssetObject, (UAssetManager*, const FPrimaryAssetId&), ERASE_ARGUMENT_PACK(UObject*)) });
});
