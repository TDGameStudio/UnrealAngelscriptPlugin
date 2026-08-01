#pragma once

#include "Compiler/AngelscriptStandaloneSemanticObserver.h"
#include "Contract/AngelscriptOfflineIndices.h"
#include "Compiler/Frontend/AngelscriptStandaloneFrontendSession.h"

#include <string>
#include <vector>

namespace AngelscriptStandalone
{
	enum class EResourceContextKind
	{
		SoftObject,
		SoftClass,
		LoadObject,
		LoadClass,
	};

	struct FResourceContext
	{
		std::string ContextId;
		std::string ContextStableSymbolId;
		std::string LogicalPath;
		AngelscriptStandalone::Frontend::FSourceSpan Span;
		EResourceContextKind Kind =
			EResourceContextKind::SoftObject;
		std::string OriginalExpression;
		std::string ConstantPath;
		std::string RequestedStableTypeId;
		bool bSoft = true;
		bool bClass = false;
	};

	struct FResourceContextResult
	{
		bool bSuccess = false;
		std::string Error;
		std::vector<FResourceContext> Contexts;
	};

	FResourceContextResult DiscoverResourceContexts(
		const std::vector<AngelscriptStandalone::Frontend::FLanguageModule>&
			Modules,
		const std::vector<AngelscriptStandalone::Frontend::FDeclaration>&
			Declarations,
		const FOfflineBundleIndices& Indices,
		const std::vector<FSemanticObservation>*
			SemanticObservations = nullptr);
}
