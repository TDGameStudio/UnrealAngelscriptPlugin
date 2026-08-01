#include "Runtime/AngelscriptStandaloneAllocator.h"

#include "angelscript.h"

#include <algorithm>
#include <cstdlib>
#include <limits>

namespace AngelscriptStandalone
{
	namespace
	{
		struct FAllocationHeader
		{
			void* RawAddress = nullptr;
			std::size_t Size = 0;
			FCountingAllocator::FState* Owner = nullptr;
		};

		std::atomic<FCountingAllocator::FState*> GActiveAllocatorState = nullptr;
		std::atomic<bool> GAreAllocatorCallbacksInstalled = false;
	}

	struct FCountingAllocator::FState
	{
		explicit FState(std::uint64_t InLimitBytes)
			: LimitBytes(InLimitBytes)
		{
		}

		const std::uint64_t LimitBytes;
		std::atomic<std::uint64_t> CurrentBytes = 0;
		std::atomic<std::uint64_t> PeakBytes = 0;
		std::atomic<std::uint64_t> RejectedAllocations = 0;
		std::atomic<std::uint64_t> OutstandingAllocations = 0;
		std::atomic<std::uint64_t> LifetimeReferences = 1;
	};

	FCountingAllocator::FCountingAllocator(std::uint64_t InLimitBytes)
		: State(new FState(InLimitBytes))
	{
	}

	FCountingAllocator::~FCountingAllocator()
	{
		Uninstall();
		FState* OwnerState = State;
		State = nullptr;
		if (OwnerState->LifetimeReferences.fetch_sub(1) == 1)
		{
			delete OwnerState;
		}
	}

	bool FCountingAllocator::Install()
	{
		FState* Expected = nullptr;
		if (!GActiveAllocatorState.compare_exchange_strong(Expected, State))
		{
			return false;
		}
		bool ExpectedCallbacksInstalled = false;
		if (GAreAllocatorCallbacksInstalled.compare_exchange_strong(ExpectedCallbacksInstalled, true)
			&& asSetGlobalMemoryFunctions(&FCountingAllocator::Allocate, &FCountingAllocator::Free) < 0)
		{
			GAreAllocatorCallbacksInstalled.store(false);
			GActiveAllocatorState.store(nullptr);
			return false;
		}
		bInstalled = true;
		return true;
	}

	void FCountingAllocator::Uninstall()
	{
		if (!bInstalled)
		{
			return;
		}
		FState* Expected = State;
		GActiveAllocatorState.compare_exchange_strong(Expected, nullptr);
		bInstalled = false;
	}

	std::uint64_t FCountingAllocator::GetCurrentBytes() const
	{
		return State->CurrentBytes.load();
	}

	std::uint64_t FCountingAllocator::GetPeakBytes() const
	{
		return State->PeakBytes.load();
	}

	std::uint64_t FCountingAllocator::GetRejectedAllocationCount() const
	{
		return State->RejectedAllocations.load();
	}

	bool FCountingAllocator::RejectedAnyAllocation() const
	{
		return GetRejectedAllocationCount() != 0;
	}

	void* FCountingAllocator::Allocate(std::size_t Size, std::size_t Alignment)
	{
		FState* State = GActiveAllocatorState.load();
		Alignment = std::max(Alignment, alignof(void*));
		if ((Alignment & (Alignment - 1)) != 0
			|| Size > std::numeric_limits<std::size_t>::max() - Alignment - sizeof(FAllocationHeader))
		{
			if (State != nullptr)
			{
				State->RejectedAllocations.fetch_add(1);
			}
			return nullptr;
		}

		std::uint64_t ReservedBytes = 0;
		if (State != nullptr)
		{
			std::uint64_t Current = State->CurrentBytes.load();
			for (;;)
			{
				if (Current > State->LimitBytes
					|| Size > State->LimitBytes - Current)
				{
					State->RejectedAllocations.fetch_add(1);
					return nullptr;
				}
				ReservedBytes = Current + static_cast<std::uint64_t>(Size);
				if (State->CurrentBytes.compare_exchange_weak(
						Current,
						ReservedBytes))
				{
					break;
				}
			}
		}

		const std::size_t TotalSize = Size + Alignment - 1 + sizeof(FAllocationHeader);
		void* RawAddress = std::malloc(TotalSize);
		if (RawAddress == nullptr)
		{
			if (State != nullptr)
			{
				State->CurrentBytes.fetch_sub(Size);
				State->RejectedAllocations.fetch_add(1);
			}
			return nullptr;
		}

		const std::uintptr_t First =
			reinterpret_cast<std::uintptr_t>(RawAddress) + sizeof(FAllocationHeader);
		const std::uintptr_t Aligned = (First + Alignment - 1) & ~(Alignment - 1);
		auto* Header = reinterpret_cast<FAllocationHeader*>(Aligned - sizeof(FAllocationHeader));
		Header->RawAddress = RawAddress;
		Header->Size = Size;
		Header->Owner = State;

		if (State != nullptr)
		{
			State->OutstandingAllocations.fetch_add(1);
			State->LifetimeReferences.fetch_add(1);
			const std::uint64_t CandidatePeak = ReservedBytes;
			std::uint64_t ObservedPeak = State->PeakBytes.load();
			while (CandidatePeak > ObservedPeak
				&& !State->PeakBytes.compare_exchange_weak(ObservedPeak, CandidatePeak))
			{
			}
		}
		return reinterpret_cast<void*>(Aligned);
	}

	void FCountingAllocator::Free(void* Address)
	{
		if (Address == nullptr)
		{
			return;
		}
		auto* Header = reinterpret_cast<FAllocationHeader*>(
			static_cast<std::byte*>(Address) - sizeof(FAllocationHeader));
		void* RawAddress = Header->RawAddress;
		const std::size_t Size = Header->Size;
		FState* OwnerState = Header->Owner;
		if (OwnerState != nullptr)
		{
			OwnerState->CurrentBytes.fetch_sub(Size);
			OwnerState->OutstandingAllocations.fetch_sub(1);
		}
		std::free(RawAddress);
		if (OwnerState != nullptr
			&& OwnerState->LifetimeReferences.fetch_sub(1) == 1)
		{
			delete OwnerState;
		}
	}
}
