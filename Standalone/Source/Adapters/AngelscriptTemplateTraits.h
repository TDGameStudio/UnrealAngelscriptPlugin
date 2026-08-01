#pragma once

#include "Contract/AngelscriptOfflineRecords.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace AngelscriptStandalone
{
	struct FTemplateTraits
	{
		bool bKnown = false;
		bool bConstructible = false;
		bool bDestructible = false;
		bool bCopyable = false;
		bool bComparable = false;
		bool bHashable = false;
		bool bTemplateEligible = false;
		bool bObjectReference = false;
		bool bObjectHandleCompatible = false;
		bool bValueType = false;
		bool bRequiresGarbageCollection = false;
		std::uint64_t ValueSize = 0;
		std::uint64_t ValueAlignment = 0;
	};

	enum class ETemplateFamily
	{
		Array,
		MapKey,
		MapValue,
		Set,
		Optional,
		ObjectWrapper,
		ClassWrapper,
	};

	struct FTemplateTraitValidation
	{
		bool bSuccess = false;
		std::string MissingTrait;
	};

	FTemplateTraits DeriveTemplateTraits(
		const FOfflineTypeRecord& Type);
	FTemplateTraits DeriveAdapterInstanceTraits(
		std::string_view AdapterName,
		std::uint64_t ValueSize,
		std::uint64_t ValueAlignment);
	FTemplateTraitValidation ValidateTemplateTraits(
		ETemplateFamily Family,
		const FTemplateTraits& Traits);
	FTemplateTraitValidation ValidateAdapterTemplateTraits(
		std::string_view AdapterName,
		const std::vector<FTemplateTraits>& Subtypes);
	bool IsNestedTemplateAllowed(
		std::string_view OuterAdapterName,
		std::string_view InnerAdapterName);
	bool IsValidCompileAlignment(std::uint64_t Alignment);
	bool CheckedAlignUp(
		std::uint64_t Value,
		std::uint64_t Alignment,
		std::uint64_t& OutValue);
	bool ComputeOptionalCompileLayout(
		const FTemplateTraits& Traits,
		std::uint64_t& OutSize,
		std::uint64_t& OutAlignment);
}
