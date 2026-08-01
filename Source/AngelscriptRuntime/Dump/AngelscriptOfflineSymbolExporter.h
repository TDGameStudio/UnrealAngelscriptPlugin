#pragma once

#include "AngelscriptOfflineContractTypes.h"

class asIScriptEngine;
struct FAngelscriptEngine;

namespace AngelscriptOfflineContract
{
	struct FSymbolExportResult
	{
		bool bSuccess = false;
		FString Error;
		FScopeRecord SymbolScope;
		TArray<FSymbolRecord> Symbols;
		TArray<FAdapterRecord> Adapters;
	};

	class ANGELSCRIPTRUNTIME_API FAngelscriptOfflineSymbolExporter
	{
	public:
		static FSymbolExportResult ExportHostSurface(asIScriptEngine& ScriptEngine);
		static FSymbolExportResult ExportScriptBaseline(
			FAngelscriptEngine& AngelscriptEngine);
	};
}
