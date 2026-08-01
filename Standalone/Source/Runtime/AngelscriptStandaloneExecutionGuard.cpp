#include "Runtime/AngelscriptStandaloneExecutionGuard.h"

#include "Runtime/AngelscriptStandaloneAllocator.h"

namespace AngelscriptStandalone
{
	FExecutionGuard::FExecutionGuard(
		int TimeoutMilliseconds,
		const FCountingAllocator* InCountingAllocator)
		: Deadline(
			std::chrono::steady_clock::now()
			+ std::chrono::milliseconds(TimeoutMilliseconds)),
		CountingAllocator(InCountingAllocator)
	{
	}

	void FExecutionGuard::Cancel()
	{
		bCancelRequested.store(true);
	}

	bool FExecutionGuard::IsTimedOut() const
	{
		return bDeadlineReached.load();
	}

	bool FExecutionGuard::IsCancelled() const
	{
		return bCancelRequested.load();
	}

	bool FExecutionGuard::IsMemoryLimitReached() const
	{
		return CountingAllocator != nullptr
			&& CountingAllocator->RejectedAnyAllocation();
	}

	void FExecutionGuard::InstructionCallback(
		asIScriptContext* Context,
		const asSVMInstructionInfo*,
		void* UserData)
	{
		if (Context == nullptr || UserData == nullptr)
		{
			return;
		}
		auto* Guard = static_cast<FExecutionGuard*>(UserData);
		if (Guard->bCancelRequested.load())
		{
			Context->SetException("standalone execution cancelled");
			return;
		}
		if (Guard->IsMemoryLimitReached())
		{
			Context->SetException("standalone execution memory limit exceeded");
			return;
		}
		if (std::chrono::steady_clock::now() >= Guard->Deadline)
		{
			Guard->bDeadlineReached.store(true);
			Context->SetException("standalone execution deadline exceeded");
		}
	}
}
