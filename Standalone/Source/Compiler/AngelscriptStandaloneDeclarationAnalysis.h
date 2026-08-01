#pragma once

#include "Compiler/Frontend/AngelscriptStandaloneFrontendSession.h"
#include "Registration/AngelscriptCapabilityClassification.h"

#include <string>
#include <vector>

namespace AngelscriptStandalone
{
	struct FPreClassBinding
	{
		std::string DeclarationStableId;
		std::string ClassName;
		std::string BaseStableTypeId;
	};

	struct FDeclarationAnalysisResult
	{
		bool bSuccess = false;
		std::string Error;
		std::vector<AngelscriptStandalone::Frontend::FDeclaration> Declarations;
		std::vector<AngelscriptStandalone::Frontend::FDiagnostic> Diagnostics;
		std::vector<FCapabilityObservation> Capabilities;
		std::vector<FPreClassBinding> PreClassBindings;
	};

	FDeclarationAnalysisResult AnalyzeStandaloneDeclarations(
		std::vector<AngelscriptStandalone::Frontend::FLanguageModule>& Modules,
		const AngelscriptStandalone::Frontend::ITypeOracle& TypeOracle);
}
