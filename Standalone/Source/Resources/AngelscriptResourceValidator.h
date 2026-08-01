#pragma once

#include "Contract/AngelscriptOfflineIndices.h"
#include "Resources/AngelscriptAssetIndex.h"
#include "Resources/AngelscriptResourceContext.h"

#include <string>
#include <vector>

namespace AngelscriptStandalone
{
	enum class EResourceState
	{
		Found,
		Redirected,
		Missing,
		Incompatible,
		Unknown,
	};

	struct FResourceValidation
	{
		std::string DiagnosticId;
		EResourceState State = EResourceState::Unknown;
		FResourceContext Context;
		std::string NormalizedPath;
		std::string FinalPath;
		std::string ResolvedAssetStableId;
		std::string ResolvedTypePath;
		std::string Reason;
	};

	FResourceValidation ValidateResourceContext(
		const FResourceContext& Context,
		const FAssetIndex& Assets,
		const FOfflineBundleIndices& Symbols);

	const char* ToString(EResourceState State);
}
