#pragma once

#include "Contract/AngelscriptOfflineManifest.h"
#include "Compiler/Frontend/AngelscriptStandaloneFrontendSession.h"
#include "Registration/AngelscriptScriptBaselinePlan.h"

#include <string>
#include <vector>

namespace AngelscriptStandalone
{
	enum class ERegistrationStage
	{
		EngineSettings,
		EnumTypedefFuncdef,
		TypeSkeleton,
		TypeRelationships,
		MembersAndGlobals,
		Adapters,
		Sources,
	};

	struct FRegistrationPlanItem
	{
		ERegistrationStage Stage = ERegistrationStage::EngineSettings;
		std::string StableId;
		const FOfflineSymbolRecord* Symbol = nullptr;
		const FOfflineAdapterDescriptor* Adapter = nullptr;
		const AngelscriptStandalone::Frontend::FLanguageModule* Source = nullptr;
	};

	struct FRegistrationPlan
	{
		bool bSuccess = false;
		std::string Error;
		std::vector<FRegistrationPlanItem> Items;
	};

	FRegistrationPlan BuildRegistrationPlan(
		const FOfflineManifest& Manifest,
		const FScriptBaselinePlan& Baseline,
		const std::vector<AngelscriptStandalone::Frontend::FLanguageModule>&
			SourceClosure);
}
