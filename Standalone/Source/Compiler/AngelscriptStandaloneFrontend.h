#pragma once

#include "Host/AngelscriptStandaloneDiagnosticSink.h"
#include "Host/AngelscriptStandaloneFileSystem.h"

#include <string>
#include <vector>

namespace AngelscriptStandalone
{
	struct FFrontendResult
	{
		bool bSuccess = false;
		std::string LogicalPath;
		std::string ModuleName;
		std::string ModuleId;
		std::string ProcessedSource;
		std::vector<FDiagnostic> Diagnostics;
	};

	FFrontendResult ProcessNativeFrontend(const FResolvedSource& Source);
}
