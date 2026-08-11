#pragma once

#include "CoreMinimal.h"

namespace AngelscriptCacheMemoryView_Private
{
	struct FAddressRange
	{
		UPTRINT Begin = 0;
		UPTRINT End = 0;

		bool IsEmpty() const
		{
			return Begin == End;
		}
	};

	inline bool TryMakeAddressRange(
		const void* Data,
		const uint64 Size,
		FAddressRange& OutRange)
	{
		OutRange = {};
		if (Size == 0)
		{
			return true;
		}
		if (Data == nullptr)
		{
			return false;
		}

		const UPTRINT Begin = reinterpret_cast<UPTRINT>(Data);
		const UPTRINT MaxAddress = TNumericLimits<UPTRINT>::Max();
		if (Size > static_cast<uint64>(MaxAddress - Begin))
		{
			return false;
		}

		OutRange.Begin = Begin;
		OutRange.End = Begin + static_cast<UPTRINT>(Size);
		return true;
	}

	inline bool TryGetViewRange(
		const TConstArrayView<uint8> View,
		FAddressRange& OutRange)
	{
		if (View.Num() < 0)
		{
			OutRange = {};
			return false;
		}
		return TryMakeAddressRange(
			View.GetData(), static_cast<uint64>(View.Num()), OutRange);
	}

	inline bool TryGetViewRange(
		const FStringView View,
		FAddressRange& OutRange)
	{
		if (View.Len() < 0)
		{
			OutRange = {};
			return false;
		}
		const uint64 CharacterCount = static_cast<uint64>(View.Len());
		if (CharacterCount > MAX_uint64 / sizeof(TCHAR))
		{
			OutRange = {};
			return false;
		}
		return TryMakeAddressRange(
			View.GetData(), CharacterCount * sizeof(TCHAR), OutRange);
	}

	template <typename ElementType, typename AllocatorType>
	bool TryGetAllocationRange(
		const TArray<ElementType, AllocatorType>& Values,
		FAddressRange& OutRange)
	{
		return TryMakeAddressRange(
			Values.GetData(),
			static_cast<uint64>(Values.GetAllocatedSize()),
			OutRange);
	}

	inline bool TryGetAllocationRange(
		const FString& Value,
		FAddressRange& OutRange)
	{
		return TryMakeAddressRange(
			Value.GetCharArray().GetData(),
			static_cast<uint64>(Value.GetCharArray().GetAllocatedSize()),
			OutRange);
	}

	inline bool DoRangesOverlap(
		const FAddressRange& Left,
		const FAddressRange& Right)
	{
		return !Left.IsEmpty()
			&& !Right.IsEmpty()
			&& Left.Begin < Right.End
			&& Right.Begin < Left.End;
	}

	template <typename ViewType, typename AllocationType>
	bool TryIsViewAliasedWithAllocation(
		const ViewType View,
		const AllocationType& Allocation,
		bool& bOutAliased)
	{
		bOutAliased = false;
		FAddressRange ViewRange;
		FAddressRange AllocationRange;
		if (!TryGetViewRange(View, ViewRange)
			|| !TryGetAllocationRange(Allocation, AllocationRange))
		{
			return false;
		}
		bOutAliased = DoRangesOverlap(ViewRange, AllocationRange);
		return true;
	}
}
