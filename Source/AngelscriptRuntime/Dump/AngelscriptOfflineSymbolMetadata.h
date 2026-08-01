#pragma once

#include "AngelscriptOfflineContractTypes.h"

class asIScriptEngine;
class asIScriptFunction;
class asITypeInfo;

namespace AngelscriptOfflineContract
{
	struct FObservedHostMetadata
	{
		TMap<FString, asITypeInfo*> TypesByStableId;
		TMap<FString, asIScriptFunction*> FunctionsByStableId;
	};

	void SupplementWithCurrentUnrealMetadata(
		asIScriptEngine& ScriptEngine,
		const FObservedHostMetadata& ObservedMetadata,
		TArray<FSymbolRecord>& Symbols);
}
