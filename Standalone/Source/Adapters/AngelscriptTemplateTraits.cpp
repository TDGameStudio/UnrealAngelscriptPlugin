#include "Adapters/AngelscriptTemplateTraits.h"

#include <limits>

namespace AngelscriptStandalone
{
	namespace
	{
		bool IsContainerAdapter(const std::string_view Name)
		{
			return Name == "TArray"
				|| Name == "TMap"
				|| Name == "TSet"
				|| Name == "TOptional";
		}

		bool IsObjectWrapperAdapter(const std::string_view Name)
		{
			return Name == "TObjectPtr"
				|| Name == "TWeakObjectPtr"
				|| Name == "TSoftObjectPtr"
				|| Name == "TSubclassOf"
				|| Name == "TSoftClassPtr";
		}

		bool IsKnownAdapter(const std::string_view Name)
		{
			return IsContainerAdapter(Name)
				|| IsObjectWrapperAdapter(Name);
		}
	}

	FTemplateTraits DeriveTemplateTraits(
		const FOfflineTypeRecord& Type)
	{
		FTemplateTraits Result;
		Result.bKnown = Type.Kind == "primitive"
			|| Type.Kind == "enum"
			|| Type.Kind == "value"
			|| Type.Kind == "reference";
		Result.bConstructible = Type.Traits.bConstructible;
		Result.bDestructible = Type.Traits.bDestructible;
		Result.bCopyable = Type.Traits.bCopyConstructible
			&& Type.Traits.bCopyAssignable;
		Result.bComparable = Type.Traits.bComparable;
		Result.bHashable = Type.Traits.bHashable;
		Result.bTemplateEligible = Type.Traits.bTemplateEligible;
		Result.bObjectReference = Type.Kind == "reference";
		Result.bObjectHandleCompatible =
			Result.bObjectReference && Type.bHandle;
		Result.bValueType = Type.Kind == "primitive"
			|| Type.Kind == "enum"
			|| Type.Kind == "value";
		Result.bRequiresGarbageCollection =
			Type.Traits.bGarbageCollected;
		Result.ValueSize = Type.CompileSize > 0
			? static_cast<std::uint64_t>(Type.CompileSize)
			: 0;
		Result.ValueAlignment = Type.CompileAlignment > 0
			? static_cast<std::uint64_t>(Type.CompileAlignment)
			: 0;
		if (Type.Kind == "primitive" || Type.Kind == "enum")
		{
			Result.bKnown = true;
			Result.bConstructible = true;
			Result.bDestructible = true;
			Result.bCopyable = true;
			Result.bComparable = true;
			Result.bHashable = true;
			Result.bTemplateEligible = true;
		}
		return Result;
	}

	FTemplateTraits DeriveAdapterInstanceTraits(
		const std::string_view AdapterName,
		const std::uint64_t ValueSize,
		const std::uint64_t ValueAlignment)
	{
		FTemplateTraits Result;
		if (!IsKnownAdapter(AdapterName))
		{
			return Result;
		}

		Result.bKnown = true;
		Result.bConstructible = true;
		Result.bDestructible = true;
		Result.bCopyable = true;
		Result.bTemplateEligible = true;
		Result.bValueType = true;
		Result.ValueSize = ValueSize;
		Result.ValueAlignment = ValueAlignment;

		// The compile-only object/class wrappers model pointer identity. Their
		// validated subtype is a UObject/UClass reference, so equality and
		// hashing are deterministic compile traits even though invocation is
		// still trapped. Container instances remain copyable values but do not
		// become hashable keys merely because their own subtype callback passed.
		if (IsObjectWrapperAdapter(AdapterName))
		{
			Result.bComparable = true;
			Result.bHashable = true;
		}
		return Result;
	}

	FTemplateTraitValidation ValidateTemplateTraits(
		const ETemplateFamily Family,
		const FTemplateTraits& Traits)
	{
		const auto Missing = [](const char* Trait)
		{
			return FTemplateTraitValidation{false, Trait};
		};
		if (!Traits.bKnown)
			return Missing("known");
		if (Family == ETemplateFamily::ObjectWrapper
			|| Family == ETemplateFamily::ClassWrapper)
		{
			if (!Traits.bObjectReference)
				return Missing("subtype.uobject");
			if (!Traits.bObjectHandleCompatible)
				return Missing("subtype.handle");
			return {true, {}};
		}
		if (Traits.bObjectReference)
		{
			if (!Traits.bObjectHandleCompatible)
				return Missing("subtype.handle");
			// Containers store an AngelScript object handle, not an inline
			// instance of the referenced UObject. Handle construction, copy and
			// destruction are therefore valid even when the pointee type itself
			// is abstract or disallows instantiation. Set/map keys additionally
			// retain the exported pointer equality and hash requirements.
			if (Family == ETemplateFamily::MapKey
				|| Family == ETemplateFamily::Set)
			{
				if (!Traits.bComparable)
					return Missing("compare");
				if (!Traits.bHashable)
					return Missing("hash");
			}
			return {true, {}};
		}
		if (!Traits.bTemplateEligible)
			return Missing("template-eligible");
		if (!Traits.bConstructible)
			return Missing("construct");
		if (!Traits.bDestructible)
			return Missing("destruct");
		if (!Traits.bCopyable)
			return Missing("copy");
		if (Traits.bValueType && Traits.ValueSize == 0)
			return Missing("non-zero-size");
		if (Family == ETemplateFamily::MapKey
			|| Family == ETemplateFamily::Set)
		{
			if (!Traits.bComparable)
				return Missing("compare");
			if (!Traits.bHashable)
				return Missing("hash");
		}
		return {true, {}};
	}

	FTemplateTraitValidation ValidateAdapterTemplateTraits(
		const std::string_view AdapterName,
		const std::vector<FTemplateTraits>& Subtypes)
	{
		auto InvalidArity = []()
		{
			return FTemplateTraitValidation{
				false,
				"template-arity",
			};
		};
		if (AdapterName == "TMap")
		{
			if (Subtypes.size() != 2)
				return InvalidArity();
			FTemplateTraitValidation Result =
				ValidateTemplateTraits(
					ETemplateFamily::MapKey,
					Subtypes[0]);
			if (!Result.bSuccess)
				return Result;
			return ValidateTemplateTraits(
				ETemplateFamily::MapValue,
				Subtypes[1]);
		}
		if (Subtypes.size() != 1)
			return InvalidArity();
		ETemplateFamily Family;
		if (AdapterName == "TArray")
			Family = ETemplateFamily::Array;
		else if (AdapterName == "TSet")
			Family = ETemplateFamily::Set;
		else if (AdapterName == "TOptional")
			Family = ETemplateFamily::Optional;
		else if (AdapterName == "TObjectPtr"
			|| AdapterName == "TWeakObjectPtr"
			|| AdapterName == "TSoftObjectPtr")
			Family = ETemplateFamily::ObjectWrapper;
		else if (AdapterName == "TSubclassOf"
			|| AdapterName == "TSoftClassPtr")
			Family = ETemplateFamily::ClassWrapper;
		else
			return {false, "adapter"};
		return ValidateTemplateTraits(Family, Subtypes[0]);
	}

	bool IsNestedTemplateAllowed(
		const std::string_view OuterAdapterName,
		const std::string_view InnerAdapterName)
	{
		// Only container/optional adapters compose other validated adapter
		// instances. The inner template's own callback has already checked its
		// subtype contract; the outer callback still applies the compound
		// construct/copy/compare/hash requirements for its position.
		return IsContainerAdapter(OuterAdapterName)
			&& IsKnownAdapter(InnerAdapterName);
	}

	bool IsValidCompileAlignment(const std::uint64_t Alignment)
	{
		return Alignment != 0
			&& (Alignment & (Alignment - 1)) == 0
			&& Alignment <= 4096;
	}

	bool CheckedAlignUp(
		const std::uint64_t Value,
		const std::uint64_t Alignment,
		std::uint64_t& OutValue)
	{
		if (!IsValidCompileAlignment(Alignment))
			return false;
		const std::uint64_t Mask = Alignment - 1;
		if (Value > std::numeric_limits<std::uint64_t>::max() - Mask)
			return false;
		OutValue = (Value + Mask) & ~Mask;
		return true;
	}

	bool ComputeOptionalCompileLayout(
		const FTemplateTraits& Traits,
		std::uint64_t& OutSize,
		std::uint64_t& OutAlignment)
	{
		if (!ValidateTemplateTraits(
				ETemplateFamily::Optional,
				Traits).bSuccess
			|| !IsValidCompileAlignment(Traits.ValueAlignment)
			|| Traits.ValueSize
				== std::numeric_limits<std::uint64_t>::max())
		{
			return false;
		}
		if (!CheckedAlignUp(
				Traits.ValueSize + 1,
				Traits.ValueAlignment,
				OutSize))
		{
			return false;
		}
		OutAlignment = Traits.ValueAlignment;
		return true;
	}
}
