#include "Runtime/AngelscriptStandaloneAllocator.h"

#include "angelscript.h"

#include <cstddef>
#include <iostream>
#include <sanitizer/asan_interface.h>

namespace
{
	struct FAllocationHeaderProbe
	{
		void* RawAddress = nullptr;
		std::size_t Size = 0;
		void* Owner = nullptr;
	};
}

int main()
{
	using namespace AngelscriptStandalone;

	void* DelayedFree = nullptr;
	{
		FCountingAllocator Allocator(64);
		if (!Allocator.Install())
		{
			std::cerr << "FAILED: allocator installation\n";
			return 1;
		}
		DelayedFree = asAllocMem(0);
		if (DelayedFree == nullptr || Allocator.GetCurrentBytes() != 0)
		{
			std::cerr << "FAILED: zero-size allocation fixture\n";
			return 1;
		}
	}

	const auto* Header = reinterpret_cast<const FAllocationHeaderProbe*>(
		static_cast<const std::byte*>(DelayedFree) - sizeof(FAllocationHeaderProbe));
	if (__asan_address_is_poisoned(Header->Owner) != 0)
	{
		std::cerr << "FAILED: zero-size allocation owner was destroyed before delayed free\n";
		return 1;
	}

	asFreeMem(DelayedFree);
	return 0;
}
