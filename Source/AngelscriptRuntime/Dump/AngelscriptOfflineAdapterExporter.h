#pragma once

#include "AngelscriptOfflineContractTypes.h"

namespace AngelscriptOfflineContract
{
	class ANGELSCRIPTRUNTIME_API FAngelscriptOfflineAdapterExporter
	{
	public:
		static TArray<FAdapterRecord> ExportAndAssign(
			TArray<FSymbolRecord>& Symbols);
	};
}
