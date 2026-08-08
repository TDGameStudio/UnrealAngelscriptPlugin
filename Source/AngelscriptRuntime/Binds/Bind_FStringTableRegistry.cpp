#include "AngelscriptBinds.h"

#include "Internationalization/StringTableRegistry.h"

#include "Bind_FStringTableRegistry_Functions.h"

namespace
{
	void BindFStringTableRegistryTypes(FAngelscriptBinds& Binds)
	{
		auto Policy_ = Binds.EnumForTarget("EStringTableLoadingPolicy");
		Policy_["Find"] = EStringTableLoadingPolicy::Find;
		Policy_["FindOrLoad"] = EStringTableLoadingPolicy::FindOrLoad;
		Policy_["FindOrFullyLoad"] = EStringTableLoadingPolicy::FindOrFullyLoad;
	}

	void BindFStringTableRegistryManual(FAngelscriptBinds& Binds)
	{
		Binds.BindGlobalFunctionForTarget(
			"void LOCTABLE_NEW(const FName TableId, const FString& Namespace)",
			&FAngelscriptFStringTableRegistryBinds::NewTable)
			.NoDiscard();
		Binds.BindGlobalFunctionForTarget(
			"void LOCTABLE_FROMFILE_ENGINE(const FName TableId, const FString& Namespace, const FString& FilePath)",
			&FAngelscriptFStringTableRegistryBinds::LoadEngineTable)
			.NoDiscard();
		Binds.BindGlobalFunctionForTarget(
			"void LOCTABLE_FROMFILE_GAME(const FName TableId, const FString& Namespace, const FString& FilePath)",
			&FAngelscriptFStringTableRegistryBinds::LoadGameTable)
			.NoDiscard();
		Binds.BindGlobalFunctionForTarget(
			"void LOCTABLE_SETSTRING(const FName TableId, const FString& Key, const FString& SourceString)",
			&FAngelscriptFStringTableRegistryBinds::SetString)
			.NoDiscard();
		Binds.BindGlobalFunctionForTarget(
			"void LOCTABLE_SETMETA(const FName TableId, const FString& Key, const FName MetaDataId, const FString& MetaData)",
			&FAngelscriptFStringTableRegistryBinds::SetMetaData)
			.NoDiscard();
		Binds.BindGlobalFunctionForTarget(
			"FText LOCTABLE(const FName TableId, const FString& Key)",
			&FAngelscriptFStringTableRegistryBinds::FindText)
			.NoDiscard();
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FStringTableRegistry_Types(
	TEXT("FStringTableRegistry.Types"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindFStringTableRegistryTypes);

AS_FORCE_LINK const FAngelscriptBind Bind_FStringTableRegistry(
	TEXT("FStringTableRegistry.Manual"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFStringTableRegistryManual);
