#pragma once

#include "Contract/AngelscriptOfflineManifest.h"

#include <string>
#include <string_view>
#include <vector>

namespace AngelscriptStandalone
{
	struct FKnownAdapter
	{
		std::string_view StableId;
		std::string_view Name;
		std::string_view Version;
		std::string_view SurfaceHash;
		std::vector<std::string_view> RequiredTraits;
	};

	struct FAdapterHandshakeResult
	{
		bool bSuccess = false;
		std::string Error;
		std::vector<const FKnownAdapter*> ResolvedAdapters;
	};

	const FKnownAdapter* FindKnownAdapter(std::string_view StableId);

	FAdapterHandshakeResult ValidateAdapterHandshake(
		const FOfflineManifest& Manifest);
}
