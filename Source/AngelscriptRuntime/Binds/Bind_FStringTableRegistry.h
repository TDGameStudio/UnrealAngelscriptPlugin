#pragma once

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
