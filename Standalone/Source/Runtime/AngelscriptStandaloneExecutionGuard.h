#pragma once

#include "angelscript.h"

#include <atomic>
#include <chrono>

namespace AngelscriptStandalone
{
	class FCountingAllocator;

	class FExecutionGuard
	{
	public:
		explicit FExecutionGuard(
			int TimeoutMilliseconds,
			const FCountingAllocator* CountingAllocator = nullptr);

		void Cancel();
		bool IsTimedOut() const;
		bool IsCancelled() const;
		bool IsMemoryLimitReached() const;
		static void InstructionCallback(
			asIScriptContext* Context,
			const asSVMInstructionInfo* Instruction,
			void* UserData);

	private:
		const std::chrono::steady_clock::time_point Deadline;
		std::atomic<bool> bCancelRequested = false;
		std::atomic<bool> bDeadlineReached = false;
		const FCountingAllocator* CountingAllocator = nullptr;
	};
}
