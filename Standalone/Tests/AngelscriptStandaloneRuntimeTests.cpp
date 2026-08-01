#include "Runtime/AngelscriptStandaloneRunner.h"
#include "Runtime/AngelscriptStandaloneAllocator.h"

#include "ClassGenerator/ASClass.h"
#include "CoreMinimal.h"
#include "angelscript.h"
#include "scriptstdstring/scriptstdstring.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
	struct FInstructionAbortObservation
	{
		int BeforeCount = 0;
		int AfterCount = 0;
	};

	constexpr asPWORD RawObjectTeardownProbeSlot =
		static_cast<asPWORD>(0x5241575445415244ull);
	bool GRawObjectRegistryWasAliveDuringTypeCleanup = false;

	bool Require(bool Condition, const char* What)
	{
		if (!Condition)
		{
			std::cerr << "FAILED: " << What << '\n';
		}
		return Condition;
	}

	std::filesystem::path MakeTemporaryDirectory()
	{
		const auto Suffix = std::chrono::steady_clock::now().time_since_epoch().count();
		const auto Directory = std::filesystem::temp_directory_path()
			/ ("angelscript-standalone-runtime-" + std::to_string(Suffix));
		std::filesystem::create_directories(Directory);
		return Directory;
	}

	void WriteSource(
		const std::filesystem::path& Root,
		const std::string& Name,
		const std::string& Source)
	{
		std::ofstream Output(Root / Name, std::ios::binary | std::ios::trunc);
		Output << Source << '\n';
	}

	void FailBeforeFirstInstruction(
		asIScriptContext* Context,
		const asSVMInstructionInfo* Info,
		void* UserData)
	{
		auto* Observation =
			static_cast<FInstructionAbortObservation*>(UserData);
		if (Info->Phase == asVM_BEFORE_INSTRUCTION)
		{
			++Observation->BeforeCount;
			Context->SetException("stop before instruction");
		}
		else
		{
			++Observation->AfterCount;
		}
	}

	void ObserveRawObjectRegistryDuringTypeCleanup(asITypeInfo* ScriptType)
	{
		void* ScriptObject = ScriptType->GetUserData(RawObjectTeardownProbeSlot);
		GRawObjectRegistryWasAliveDuringTypeCleanup =
			ScriptObject != nullptr
			&& UASClass::GetRawScriptObjectType(ScriptObject) == ScriptType;
		UASClass::UnregisterRawScriptObject(ScriptObject);
		FMemory::Free(ScriptObject);
	}

	bool TestHardMemoryLimitRejectsBeforeAllocation()
	{
		using namespace AngelscriptStandalone;

		FCountingAllocator Allocator(64);
		bool bPassed = Require(
			Allocator.Install(),
			"hard-limit allocator installation");
		void* First = asAllocMem(32);
		bPassed &= Require(First != nullptr, "below-limit allocation");
		bPassed &= Require(
			Allocator.GetCurrentBytes() == 32,
			"below-limit allocation accounting");
		void* Rejected = asAllocMem(40);
		bPassed &= Require(
			Rejected == nullptr,
			"over-limit allocation must return null");
		bPassed &= Require(
			Allocator.GetCurrentBytes() == 32,
			"rejected allocation must not change current bytes");
		bPassed &= Require(
			Allocator.GetPeakBytes() <= 64,
			"rejected allocation must not exceed peak limit");
		bPassed &= Require(
			Allocator.GetRejectedAllocationCount() == 1,
			"rejected allocation count");
		asFreeMem(Rejected);
		asFreeMem(First);
		bPassed &= Require(
			Allocator.GetCurrentBytes() == 0,
			"hard-limit allocator cleanup");
		Allocator.Uninstall();
		return bPassed;
	}

	bool TestZeroSizeAllocationCanOutliveAllocatorOwner()
	{
		using namespace AngelscriptStandalone;

		void* DelayedFree = nullptr;
		bool bPassed = true;
		{
			FCountingAllocator Allocator(64);
			bPassed &= Require(
				Allocator.Install(),
				"zero-size allocator installation");
			DelayedFree = asAllocMem(0);
			bPassed &= Require(
				DelayedFree != nullptr,
				"zero-size allocation must retain a releasable address");
			bPassed &= Require(
				Allocator.GetCurrentBytes() == 0,
				"zero-size allocation must not charge the byte budget");
		}

		asFreeMem(DelayedFree);

		FCountingAllocator NextAllocator(64);
		bPassed &= Require(
			NextAllocator.Install(),
			"allocator installation after delayed zero-size free");
		void* NextAllocation = asAllocMem(16);
		bPassed &= Require(
			NextAllocation != nullptr,
			"allocation after delayed zero-size free");
		asFreeMem(NextAllocation);
		bPassed &= Require(
			NextAllocator.GetCurrentBytes() == 0,
			"allocator accounting after delayed zero-size free");
		NextAllocator.Uninstall();
		return bPassed;
	}

	bool TestScriptStringStorageUsesCountingAllocator()
	{
		using namespace AngelscriptStandalone;

		FCountingAllocator Allocator(64);
		bool bPassed = Require(
			Allocator.Install(),
			"script-string allocator installation");
		bool bRejected = false;
		try
		{
			scriptstring_t Value;
			Value.resize(256);
		}
		catch (const std::bad_alloc&)
		{
			bRejected = true;
		}
		bPassed &= Require(
			bRejected,
			"script string storage must propagate allocation rejection");
		bPassed &= Require(
			Allocator.RejectedAnyAllocation(),
			"script string storage must use the counting allocator");
		bPassed &= Require(
			Allocator.GetCurrentBytes() == 0,
			"rejected script string allocation must not leak");
		Allocator.Uninstall();
		return bPassed;
	}

	bool TestAbortBeforeInstructionHasNoAfterEvent()
	{
		asIScriptEngine* Engine = asCreateScriptEngine();
		bool bPassed = Require(Engine != nullptr, "instruction callback engine");
		if (Engine == nullptr)
		{
			return false;
		}

		asIScriptModule* Module =
			Engine->GetModule("instruction-abort", asGM_ALWAYS_CREATE);
		const char* Script = "int main() { return 42; }";
		bPassed &= Require(
			Module != nullptr
				&& Module->AddScriptSection("instruction-abort.as", Script) >= 0
				&& Module->Build() >= 0,
			"instruction callback fixture build");
		asIScriptFunction* Function =
			Module != nullptr ? Module->GetFunctionByDecl("int main()") : nullptr;
		asIScriptContext* Context = Engine->CreateContext();
		FInstructionAbortObservation Observation;
		bPassed &= Require(
			Function != nullptr
				&& Context != nullptr
				&& Context->Prepare(Function) >= 0
				&& Context->SetInstructionCallback(
					&FailBeforeFirstInstruction,
					&Observation) >= 0,
			"instruction callback fixture prepare");
		if (Context != nullptr && Function != nullptr)
		{
			const int ExecutionResult = Context->Execute();
			if (ExecutionResult != asEXECUTION_EXCEPTION
				|| Observation.BeforeCount != 1
				|| Observation.AfterCount != 0)
			{
				std::cerr
					<< "instruction callback observation: result="
					<< ExecutionResult
					<< ", before=" << Observation.BeforeCount
					<< ", after=" << Observation.AfterCount << '\n';
			}
			bPassed &= Require(
				ExecutionResult == asEXECUTION_EXCEPTION,
				"instruction callback exception result");
			bPassed &= Require(
				Observation.BeforeCount == 1,
				"instruction callback before count");
			bPassed &= Require(
				Observation.AfterCount == 0,
				"rejected instruction must not emit an after event");
		}
		if (Context != nullptr)
		{
			Context->Release();
		}
		Engine->ShutDownAndRelease();
		asThreadCleanup();
		return bPassed;
	}

	bool TestDirectEngineReleaseDrainsRawScriptObjects()
	{
		using namespace AngelscriptStandalone;

		FCountingAllocator Allocator(64ull * 1024ull * 1024ull);
		bool bPassed = Require(
			Allocator.Install(),
			"direct-release allocator installation");
		asIScriptEngine* Engine = asCreateScriptEngine();
		bPassed &= Require(Engine != nullptr, "direct-release engine");
		if (Engine != nullptr)
		{
			GRawObjectRegistryWasAliveDuringTypeCleanup = false;
			Engine->SetTypeInfoUserDataCleanupCallback(
				&ObserveRawObjectRegistryDuringTypeCleanup,
				RawObjectTeardownProbeSlot);
			asIScriptModule* Module =
				Engine->GetModule("direct-release", asGM_ALWAYS_CREATE);
			const char* Script =
				"class FReleaseProbe { int Value; }";
			bPassed &= Require(
				Module != nullptr
					&& Module->AddScriptSection("direct-release.as", Script) >= 0
					&& Module->Build() >= 0,
				"direct-release raw-object fixture build");
			asITypeInfo* ScriptType = Module != nullptr
				? Module->GetTypeInfoByDecl("FReleaseProbe")
				: nullptr;
			void* ScriptObject = FMemory::Malloc(sizeof(void*), alignof(void*));
			bPassed &= Require(
				ScriptType != nullptr && ScriptObject != nullptr,
				"direct-release raw-object teardown probe allocation");
			if (ScriptType != nullptr && ScriptObject != nullptr)
			{
				UASClass::RegisterRawScriptObject(ScriptObject, ScriptType);
				ScriptType->SetUserData(
					ScriptObject,
					RawObjectTeardownProbeSlot);
			}
			Engine->Release();
		}
		bPassed &= Require(
			GRawObjectRegistryWasAliveDuringTypeCleanup,
			"direct Engine::Release must retain raw-object type lookup through shutdown");
		bPassed &= Require(
			asThreadCleanup() == asSUCCESS,
			"direct-release thread cleanup");
		bPassed &= Require(
			Allocator.GetCurrentBytes() == 0,
			"direct Engine::Release must drain raw script objects");
		Allocator.Uninstall();
		return bPassed;
	}
}

int main()
{
	using namespace AngelscriptStandalone;

	bool Passed = true;
	Passed &= TestHardMemoryLimitRejectsBeforeAllocation();
	Passed &= TestZeroSizeAllocationCanOutliveAllocatorOwner();
	Passed &= TestScriptStringStorageUsesCountingAllocator();
	Passed &= TestAbortBeforeInstructionHasNoAfterEvent();
	Passed &= TestDirectEngineReleaseDrainsRawScriptObjects();
	const std::filesystem::path Root = MakeTemporaryDirectory();
	WriteSource(
		Root,
		"success.as",
		"int main(const array<string> args) { print(\"running\"); return args.length() == 2 ? 31 : -1; }");
	WriteSource(
		Root,
		"exception.as",
		"void main(const array<string> args) { assert(false, \"expected failure\"); }");
	WriteSource(
		Root,
		"timeout.as",
		"void main(const array<string> args) { while (true) { int value = 1; } }");
	WriteSource(
		Root,
		"bad-entry.as",
		"int helper() { return 1; }");
	WriteSource(
		Root,
		"memory-limit.as",
		"int main(const array<string> args) { string value; value.resize(33554432); return 0; }");

	FRunRequest Request;
	Request.ScriptRoots.push_back(Root);
	Request.Entry = "success.as";
	Request.Arguments = {"first", "second"};
	Request.TimeoutMilliseconds = 5000;
	Request.MemoryLimitBytes = 256ull * 1024ull * 1024ull;
	const FRunResult Success = RunNativeScript(Request);
	if (Success.AllocatedBytesAfterShutdown != 0)
	{
		std::cerr
			<< "successful run allocator residue: current="
			<< Success.AllocatedBytesAfterShutdown
			<< ", peak=" << Success.PeakAllocatedBytes << '\n';
	}
	Passed &= Require(Success.ExitCode == 0, "successful run exit");
	Passed &= Require(Success.ScriptResult.has_value() && *Success.ScriptResult == 31, "script result");
	Passed &= Require(Success.StandardOutput == "running\n", "captured print output");
	Passed &= Require(Success.PeakAllocatedBytes > 0, "tracked peak allocation");
	Passed &= Require(Success.AllocatedBytesAfterShutdown == 0, "zero tracked allocations after shutdown");

	Request.Entry = "exception.as";
	Request.Arguments.clear();
	const FRunResult Exception = RunNativeScript(Request);
	Passed &= Require(Exception.ExitCode == 3, "script exception exit");
	Passed &= Require(Exception.ExceptionMessage == "expected failure", "script exception message");

	Request.Entry = "timeout.as";
	Request.TimeoutMilliseconds = 25;
	const FRunResult Timeout = RunNativeScript(Request);
	Passed &= Require(Timeout.ExitCode == 4, "timeout exit");
	Passed &= Require(Timeout.bTimedOut, "timeout category");

	Request.Entry = "bad-entry.as";
	Request.TimeoutMilliseconds = 5000;
	const FRunResult BadEntry = RunNativeScript(Request);
	Passed &= Require(BadEntry.ExitCode == 1, "invalid entry exit");

	Request.Entry = "success.as";
	Request.MemoryLimitBytes = 128ull * 1024ull;
	const FRunResult MemoryLimited = RunNativeScript(Request);
	Passed &= Require(MemoryLimited.ExitCode == 4, "memory limit exit");
	Passed &= Require(MemoryLimited.bMemoryLimitReached, "memory limit category");
	Passed &= Require(
		MemoryLimited.AllocatedBytesAfterShutdown == 0,
		"zero tracked allocations after memory rejection");

	Request.Entry = "memory-limit.as";
	Request.MemoryLimitBytes = 20ull * 1024ull * 1024ull;
	const FRunResult AddonMemoryLimited = RunNativeScript(Request);
	Passed &= Require(
		AddonMemoryLimited.ExitCode == 4,
		"bounded-library memory limit exit");
	Passed &= Require(
		AddonMemoryLimited.bMemoryLimitReached,
		"bounded-library memory limit category");
	Passed &= Require(
		AddonMemoryLimited.AllocatedBytesAfterShutdown == 0,
		"zero tracked allocations after bounded-library rejection");

	std::error_code CleanupError;
	std::filesystem::remove_all(Root, CleanupError);
	return Passed ? 0 : 1;
}
