#pragma once

#include "Registration/AngelscriptCapabilityClassification.h"
#include "Registration/AngelscriptRegistrationPlan.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

class asIScriptEngine;

namespace AngelscriptStandalone
{
	struct FRegistrationRuntimeMap
	{
		std::unordered_map<std::string, int> TypeIdByStableId;
		std::unordered_map<std::string, int> FunctionIdByStableId;
		std::unordered_map<std::string, int> PropertyIndexByStableId;
		std::unordered_map<std::string, std::string> AvailabilityByStableId;
	};

	class FRegistrationBackingStore;

	struct FRegistrationLoadResult
	{
		bool bSuccess = false;
		std::string Error;
		FRegistrationRuntimeMap RuntimeMap;
		std::vector<FCapabilityObservation> Capabilities;
		std::shared_ptr<FRegistrationBackingStore> BackingStore;
	};

	FRegistrationLoadResult ApplyRegistrationPlan(
		asIScriptEngine* Engine,
		const FRegistrationPlan& Plan,
		const FOfflineManifest& Manifest);

	std::string NormalizeApplicationRegistrationDeclaration(
		std::string_view Declaration,
		const FOfflineManifest& Manifest);
}
