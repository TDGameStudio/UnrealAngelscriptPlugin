#include "CoreMinimal.h"

struct FAngelscriptFStringTableRegistryBinds
{
	static void NewTable(FName TableId, const FString& Namespace);
	static void LoadEngineTable(FName TableId, const FString& Namespace, const FString& FilePath);
	static void LoadGameTable(FName TableId, const FString& Namespace, const FString& FilePath);
	static void SetString(FName TableId, const FString& Key, const FString& SourceString);
	static void SetMetaData(FName TableId, const FString& Key, FName MetaDataId, const FString& MetaData);
	static FText FindText(FName TableId, const FString& Key);
};

#include "AngelscriptBinds.h"

#include "Internationalization/StringTableRegistry.h"

/**
 * String-table loading policy and global binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | enum EStringTableLoadingPolicy;                                                            | Declares the string-table lookup loading policy.                                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EStringTableLoadingPolicy::Find;                                                           | Looks only for an already loaded string table.                                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EStringTableLoadingPolicy::FindOrLoad;                                                     | Loads the table when it is not already available.                                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EStringTableLoadingPolicy::FindOrFullyLoad;                                                | Loads the table and fully resolves its asset references.                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void LOCTABLE_NEW(const FName TableId, const FString& Namespace);                          | Creates an empty runtime string table.                                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void LOCTABLE_FROMFILE_ENGINE(const FName TableId, const FString& Namespace,               | Loads a string table from a path relative to the engine content directory.                                           |
 * |     const FString& FilePath);                                                              | @param FilePath Engine-content-relative table file path.                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void LOCTABLE_FROMFILE_GAME(const FName TableId, const FString& Namespace,                 | Loads a string table from a path relative to the game content directory.                                             |
 * |     const FString& FilePath);                                                              | @param FilePath Game-content-relative table file path.                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void LOCTABLE_SETSTRING(const FName TableId, const FString& Key,                           | Adds or replaces source text for a table key.                                                                        |
 * |     const FString& SourceString);                                                          |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void LOCTABLE_SETMETA(const FName TableId, const FString& Key, const FName MetaDataId,     | Adds or replaces one metadata field for a table entry.                                                               |
 * |     const FString& MetaData);                                                              |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FText LOCTABLE(const FName TableId, const FString& Key);                                   | Returns localized text for the table key.                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_FStringTableRegistry_Types(
	TEXT("FStringTableRegistry.Types"),
	EAngelscriptBindPhase::TypeDeclarations,
	[](FAngelscriptBinds& Binds)
	{
		auto Policy_ = Binds.EnumForTarget("EStringTableLoadingPolicy");
		Policy_["Find"] = EStringTableLoadingPolicy::Find;
		Policy_["FindOrLoad"] = EStringTableLoadingPolicy::FindOrLoad;
		Policy_["FindOrFullyLoad"] = EStringTableLoadingPolicy::FindOrFullyLoad;
	});

AS_FORCE_LINK const FAngelscriptBind Bind_FStringTableRegistry(
	TEXT("FStringTableRegistry.Manual"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
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
	});

#include "Internationalization/StringTableRegistry.h"
#include "Misc/Paths.h"

void FAngelscriptFStringTableRegistryBinds::NewTable(FName TableId, const FString& Namespace)
{
	FStringTableRegistry::Get().Internal_NewLocTable(TableId, Namespace);
}

void FAngelscriptFStringTableRegistryBinds::LoadEngineTable(FName TableId, const FString& Namespace, const FString& FilePath)
{
	FStringTableRegistry::Get().Internal_LocTableFromFile(TableId, Namespace, FilePath, FPaths::EngineContentDir());
}

void FAngelscriptFStringTableRegistryBinds::LoadGameTable(FName TableId, const FString& Namespace, const FString& FilePath)
{
	FStringTableRegistry::Get().Internal_LocTableFromFile(TableId, Namespace, FilePath, FPaths::ProjectContentDir());
}

void FAngelscriptFStringTableRegistryBinds::SetString(FName TableId, const FString& Key, const FString& SourceString)
{
	FStringTableRegistry::Get().Internal_SetLocTableEntry(TableId, Key, SourceString);
}

void FAngelscriptFStringTableRegistryBinds::SetMetaData(
	FName TableId,
	const FString& Key,
	FName MetaDataId,
	const FString& MetaData)
{
	FStringTableRegistry::Get().Internal_SetLocTableEntryMetaData(TableId, Key, MetaDataId, MetaData);
}

FText FAngelscriptFStringTableRegistryBinds::FindText(FName TableId, const FString& Key)
{
	return FStringTableRegistry::Get().Internal_FindLocTableEntry(TableId, Key, EStringTableLoadingPolicy::FindOrLoad);
}
