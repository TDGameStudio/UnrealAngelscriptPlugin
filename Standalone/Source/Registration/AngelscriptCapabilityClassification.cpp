#include "Registration/AngelscriptCapabilityClassification.h"

#include "Compiler/AngelscriptStandaloneSemanticObserver.h"

#include <set>

namespace AngelscriptStandalone
{
	const char* ToString(
		const ECapabilityClassification Classification)
	{
		switch (Classification)
		{
		case ECapabilityClassification::Exact:
			return "exact";
		case ECapabilityClassification::CompileShim:
			return "compile-shim";
		case ECapabilityClassification::UERequired:
			return "ue-required";
		case ECapabilityClassification::Unsupported:
			return "unsupported";
		default:
			return "unsupported";
		}
	}

	const char* ToString(const EValidationCompleteness Completeness)
	{
		switch (Completeness)
		{
		case EValidationCompleteness::Complete:
			return "complete";
		case EValidationCompleteness::Partial:
			return "partial";
		case EValidationCompleteness::Failed:
			return "failed";
		default:
			return "failed";
		}
	}

	FCapabilitySummary SummarizeCapabilities(
		const std::vector<FCapabilityObservation>& Observations,
		const bool bAllowUERequired)
	{
		FCapabilitySummary Result;
		for (const FCapabilityObservation& Observation : Observations)
		{
			switch (Observation.Classification)
			{
			case ECapabilityClassification::Exact:
				++Result.ExactCount;
				break;
			case ECapabilityClassification::CompileShim:
				++Result.CompileShimCount;
				break;
			case ECapabilityClassification::UERequired:
				++Result.UERequiredCount;
				break;
			case ECapabilityClassification::Unsupported:
				++Result.UnsupportedCount;
				break;
			}
		}
		if (Result.UnsupportedCount != 0
			|| (Result.UERequiredCount != 0 && !bAllowUERequired))
		{
			Result.Completeness = EValidationCompleteness::Failed;
			Result.bCanCompile = false;
		}
		else if (Result.UERequiredCount != 0)
		{
			Result.Completeness = EValidationCompleteness::Partial;
		}
		return Result;
	}

	void AppendUsedAvailabilityCapabilities(
		const std::vector<FSemanticObservation>& Observations,
		const std::unordered_map<std::string, std::string>&
			AvailabilityByStableId,
		std::vector<FCapabilityObservation>& OutCapabilities)
	{
		std::set<std::string> UnavailableStableIds;
		const auto ObserveStableId =
			[&AvailabilityByStableId, &UnavailableStableIds](
				const std::string& StableId)
			{
				const auto Availability =
					AvailabilityByStableId.find(StableId);
				if (Availability == AvailabilityByStableId.end())
				{
					return;
				}
				if (Availability->second == "editor-only"
					|| Availability->second == "unavailable")
				{
					UnavailableStableIds.insert(StableId);
				}
			};

		for (const FSemanticObservation& Observation : Observations)
		{
			ObserveStableId(Observation.StableFunctionId);
			ObserveStableId(Observation.SourceStableTypeId);
			ObserveStableId(Observation.TargetStableTypeId);
			for (const FSemanticArgumentObservation& Argument
				: Observation.Arguments)
			{
				ObserveStableId(Argument.ActualStableTypeId);
				ObserveStableId(Argument.ParameterStableTypeId);
			}
		}

		for (const std::string& StableId : UnavailableStableIds)
		{
			const std::string& Availability =
				AvailabilityByStableId.at(StableId);
			OutCapabilities.push_back({
				StableId,
				"availability:" + Availability,
				ECapabilityClassification::Unsupported,
				"the resolved symbol is not available in the standalone "
					"non-editor validation profile",
			});
		}
	}
}
