#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace AngelscriptStandalone
{
	class FCountingAllocator
	{
	public:
		struct FState;

		explicit FCountingAllocator(std::uint64_t LimitBytes);
		~FCountingAllocator();

		FCountingAllocator(const FCountingAllocator&) = delete;
		FCountingAllocator& operator=(const FCountingAllocator&) = delete;

		bool Install();
		void Uninstall();
		std::uint64_t GetCurrentBytes() const;
		std::uint64_t GetPeakBytes() const;
		std::uint64_t GetRejectedAllocationCount() const;
		bool RejectedAnyAllocation() const;

	private:
		static void* Allocate(std::size_t Size, std::size_t Alignment);
		static void Free(void* Address);

		FState* State = nullptr;
		bool bInstalled = false;
	};
}
