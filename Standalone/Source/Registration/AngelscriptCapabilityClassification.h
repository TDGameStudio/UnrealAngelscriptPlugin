#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace AngelscriptStandalone
{
	struct FSemanticObservation;

	enum class ECapabilityClassification
	{
		Exact,
		CompileShim,
		UERequired,
		Unsupported,
	};

	enum class EValidationCompleteness
	{
		Complete,
		Partial,
		Failed,
	};

	struct FCapabilityObservation
	{
		std::string StableId;
		std::string Rule;
		ECapabilityClassification Classification =
			ECapabilityClassification::Exact;
		std::string Reason;
	};

	struct FCapabilitySummary
	{
		EValidationCompleteness Completeness =
			EValidationCompleteness::Complete;
		bool bCanCompile = true;
		std::size_t ExactCount = 0;
		std::size_t CompileShimCount = 0;
		std::size_t UERequiredCount = 0;
		std::size_t UnsupportedCount = 0;
	};

	const char* ToString(ECapabilityClassification Classification);
	const char* ToString(EValidationCompleteness Completeness);

	FCapabilitySummary SummarizeCapabilities(
		const std::vector<FCapabilityObservation>& Observations,
		bool bAllowUERequired);

	void AppendUsedAvailabilityCapabilities(
		const std::vector<FSemanticObservation>& Observations,
		const std::unordered_map<std::string, std::string>&
			AvailabilityByStableId,
		std::vector<FCapabilityObservation>& OutCapabilities);
}
