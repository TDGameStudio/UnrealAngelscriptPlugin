#include "Bind_FStringTableRegistry_Functions.h"

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
