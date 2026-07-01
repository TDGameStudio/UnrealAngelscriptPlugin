#include "CQTest.h"

#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#include "Core/AngelscriptBinds.h"
#include "Core/AngelscriptEngine.h"
#include "Core/FunctionCallers.h"
#include "Features/IModularFeatures.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeExit.h"
#include "Shared/AngelscriptTestExecute.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestModuleScope.h"
#include "Templates/Atomic.h"
#include "UHT/AngelscriptCrossModuleBindings.h"

#if WITH_ANGELSCRIPT_UNITTESTS

ANGELSCRIPTRUNTIME_API void GAngelscriptCrossModuleEnsureRegisteredForTesting();
ANGELSCRIPTRUNTIME_API int32 GAngelscriptCrossModuleBindGlobalFunctionForTesting(const ANSICHAR* Signature, FAngelscriptCrossModuleEntry* Entry);

TEST_CLASS_WITH_FLAGS(FAngelscriptCrossModuleDirectBindRuntimeTests,
	"Angelscript.CppTests.UHTToolResolver.CrossModuleDirectBind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static constexpr const TCHAR* TestModuleName = TEXT("Engine");
	static constexpr const TCHAR* TestClassName = TEXT("Actor");
	static constexpr const TCHAR* LateFunctionName = TEXT("CrossModuleLateLoadedAutomationProbe");
	static constexpr const TCHAR* WorkerFunctionName = TEXT("CrossModuleWorkerThreadAutomationProbe");
	static constexpr const TCHAR* MismatchFunctionName = TEXT("CrossModuleLayoutMismatchAutomationProbe");
	static constexpr const TCHAR* MalformedFunctionName = TEXT("CrossModuleMalformedAutomationProbe");
	static constexpr const TCHAR* ExistingFunctionName = TEXT("CrossModuleExistingSlotAutomationProbe");
	static constexpr const TCHAR* MultiShardFunctionNameA = TEXT("CrossModuleMultiShardAutomationProbeA");
	static constexpr const TCHAR* MultiShardFunctionNameB = TEXT("CrossModuleMultiShardAutomationProbeB");

	struct FGenericHookProbeState
	{
		int32 HitCount = 0;
		int32 Left = 0;
		int32 Right = 0;
		uint16 ArgCount = 0;
		bool bSelfWasNull = false;
		bool bArgSlotsWereValid = false;
		bool bReturnWasValid = false;
		bool bScriptSelfWasNull = false;
		bool bWorldContextWasNull = false;
		uint32 FrameFlags = 0;
	};

	static FGenericHookProbeState* GGenericHookProbeState;

	static void NoOpThunk(UObject* /*Self*/, FAngelscriptCrossModuleCallFrame* /*Frame*/)
	{
	}

	static void SumThunk(UObject* Self, FAngelscriptCrossModuleCallFrame* Frame)
	{
		if (GGenericHookProbeState == nullptr)
		{
			return;
		}

		++GGenericHookProbeState->HitCount;
		GGenericHookProbeState->bSelfWasNull = Self == nullptr;
		GGenericHookProbeState->ArgCount = Frame != nullptr ? Frame->ArgCount : 0;
		GGenericHookProbeState->bArgSlotsWereValid = Frame != nullptr && Frame->ArgSlots != nullptr && Frame->ArgSlots[0] != nullptr && Frame->ArgSlots[1] != nullptr;
		GGenericHookProbeState->bReturnWasValid = Frame != nullptr && Frame->ReturnSlot != nullptr;
		GGenericHookProbeState->bScriptSelfWasNull = Frame != nullptr && Frame->ScriptSelf == nullptr;
		GGenericHookProbeState->bWorldContextWasNull = Frame != nullptr && Frame->WorldContext == nullptr;
		GGenericHookProbeState->FrameFlags = Frame != nullptr ? Frame->Flags : 0;
		if (!GGenericHookProbeState->bArgSlotsWereValid || !GGenericHookProbeState->bReturnWasValid)
		{
			return;
		}

		GGenericHookProbeState->Left = *static_cast<int32*>(Frame->ArgSlots[0]);
		GGenericHookProbeState->Right = *static_cast<int32*>(Frame->ArgSlots[1]);
		*static_cast<int32*>(Frame->ReturnSlot) = GGenericHookProbeState->Left + GGenericHookProbeState->Right;
	}

	struct FScopedClassFuncMapRestore
	{
		UClass* Class = nullptr;
		bool bHadClassMap = false;
		TMap<FString, FFuncEntry> SavedClassMap;

		explicit FScopedClassFuncMapRestore(UClass* InClass)
			: Class(InClass)
		{
			if (Class == nullptr)
			{
				return;
			}

			if (TMap<FString, FFuncEntry>* ExistingMap = FAngelscriptBinds::GetClassFuncMaps().Find(Class))
			{
				bHadClassMap = true;
				SavedClassMap = *ExistingMap;
			}
		}

		~FScopedClassFuncMapRestore()
		{
			if (Class == nullptr)
			{
				return;
			}

			TMap<UClass*, TMap<FString, FFuncEntry>>& ClassFuncMaps = FAngelscriptBinds::GetClassFuncMaps();
			if (bHadClassMap)
			{
				ClassFuncMaps.FindOrAdd(Class) = SavedClassMap;
			}
			else
			{
				ClassFuncMaps.Remove(Class);
			}
		}
	};

	struct FTestCrossModuleFeature : public IModularFeature
	{
		const FAngelscriptCrossModuleEntry* Table;
		int32 Count;
		const TCHAR* ModuleName;
		uint32 LayoutVersion;

		FTestCrossModuleFeature(const FAngelscriptCrossModuleEntry* InTable, int32 InCount, const TCHAR* InModuleName, uint32 InLayoutVersion)
			: Table(InTable)
			, Count(InCount)
			, ModuleName(InModuleName)
			, LayoutVersion(InLayoutVersion)
		{
		}
	};

	static void RegisterAndUnregisterFeature(FTestCrossModuleFeature& Feature)
	{
		IModularFeatures::Get().RegisterModularFeature(FAngelscriptCrossModuleBindings::FeatureName(), &Feature);
		IModularFeatures::Get().UnregisterModularFeature(FAngelscriptCrossModuleBindings::FeatureName(), &Feature);
	}

	static const FFuncEntry* FindActorEntry(const TCHAR* FunctionName)
	{
		const TMap<FString, FFuncEntry>* ActorEntries = FAngelscriptBinds::GetClassFuncMaps().Find(AActor::StaticClass());
		return ActorEntries != nullptr ? ActorEntries->Find(FunctionName) : nullptr;
	}

	static bool IsEntryFunctionBound(const FFuncEntry& Entry)
	{
		return const_cast<FGenericFuncPtr&>(Entry.FuncPtr).IsBound();
	}

	static bool WaitForGameThreadFeatureInjection(const TCHAR* FunctionName, double TimeoutSeconds = 5.0)
	{
		const double DeadlineSeconds = FPlatformTime::Seconds() + TimeoutSeconds;
		while (FPlatformTime::Seconds() < DeadlineSeconds)
		{
			FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
			if (FindActorEntry(FunctionName) != nullptr)
			{
				return true;
			}
			FPlatformProcess::Sleep(0.01f);
		}

		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
		return FindActorEntry(FunctionName) != nullptr;
	}

	static bool EnsureCrossModuleSubscriptionReady()
	{
		GAngelscriptCrossModuleEnsureRegisteredForTesting();
		return true;
	}

	static bool RunExistingSlotPriority(FAutomationTestBase& Test);
	static bool RunLateRegistration(FAutomationTestBase& Test);
	static bool RunWorkerThreadRegistration(FAutomationTestBase& Test);
	static bool RunGenericHookFrameThunk(FAutomationTestBase& Test);
	static bool RunSameModuleMultipleFeature(FAutomationTestBase& Test);
	static bool RunLayoutMismatch(FAutomationTestBase& Test);
	static bool RunMalformedFeature(FAutomationTestBase& Test);

public:
	TEST_METHOD(SameModuleShardWins_When_BothExist)
	{
		ASSERT_THAT(IsTrue(RunExistingSlotPriority(*TestRunner)));
	}

	TEST_METHOD(OnModularFeatureRegistered_LateLoadedModule)
	{
		ASSERT_THAT(IsTrue(RunLateRegistration(*TestRunner)));
	}

	TEST_METHOD(OnModularFeatureRegistered_WorkerThreadInvocation_MarshalsToGameThread)
	{
		ASSERT_THAT(IsTrue(RunWorkerThreadRegistration(*TestRunner)));
	}

	TEST_METHOD(GenericHook_FrameThunkReceivesSlotsAndReturn)
	{
		ASSERT_THAT(IsTrue(RunGenericHookFrameThunk(*TestRunner)));
	}

	TEST_METHOD(MultipleFeaturesSameModule_AllInjected_NoModuleNameDedup)
	{
		ASSERT_THAT(IsTrue(RunSameModuleMultipleFeature(*TestRunner)));
	}

	TEST_METHOD(LayoutVersionMismatch_FeatureSkipped_NoCrash)
	{
		ASSERT_THAT(IsTrue(RunLayoutMismatch(*TestRunner)));
	}

	TEST_METHOD(RuntimeNullRangeValidation_RejectsMalformedFeature)
	{
		ASSERT_THAT(IsTrue(RunMalformedFeature(*TestRunner)));
	}
};

FAngelscriptCrossModuleDirectBindRuntimeTests::FGenericHookProbeState* FAngelscriptCrossModuleDirectBindRuntimeTests::GGenericHookProbeState = nullptr;

bool FAngelscriptCrossModuleDirectBindRuntimeTests::RunExistingSlotPriority(FAutomationTestBase& Test)
{
	if (!EnsureCrossModuleSubscriptionReady())
	{
		return true;
	}

	FScopedClassFuncMapRestore RestoreActorEntries(AActor::StaticClass());
	const FFuncEntry ExistingEntry = { FGenericFuncPtr::Make(1), ASAutoCaller::FunctionCaller{}, reinterpret_cast<void*>(0x1), false, false };
	FAngelscriptBinds::AddFunctionEntry(AActor::StaticClass(), ExistingFunctionName, ExistingEntry);

	const FAngelscriptCrossModuleEntry Entry = { TestClassName, ExistingFunctionName, &NoOpThunk, 0, 0, 0 };
	FTestCrossModuleFeature Feature(&Entry, 1, TestModuleName, FAngelscriptCrossModuleBindings::LayoutVersionExpected);
	RegisterAndUnregisterFeature(Feature);

	const FFuncEntry* FinalEntry = FindActorEntry(ExistingFunctionName);
	if (!Test.TestNotNull(TEXT("Existing same-module entry should remain present"), FinalEntry))
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= Test.TestEqual(TEXT("Cross-module injection should not replace an already bound slot"), FinalEntry->UserData, ExistingEntry.UserData);
	bPassed &= Test.TestFalse(TEXT("Existing non-generic slot should keep its original call mode"), FinalEntry->bGenericCall);
	return bPassed;
}

bool FAngelscriptCrossModuleDirectBindRuntimeTests::RunLateRegistration(FAutomationTestBase& Test)
{
	if (!EnsureCrossModuleSubscriptionReady())
	{
		return true;
	}

	FScopedClassFuncMapRestore RestoreActorEntries(AActor::StaticClass());
	const FAngelscriptCrossModuleEntry Entry = { TestClassName, LateFunctionName, &NoOpThunk, 0, 0, 0 };
	FTestCrossModuleFeature Feature(&Entry, 1, TestModuleName, FAngelscriptCrossModuleBindings::LayoutVersionExpected);

	RegisterAndUnregisterFeature(Feature);

	const FFuncEntry* InjectedEntry = FindActorEntry(LateFunctionName);
	if (!Test.TestNotNull(TEXT("Late-registered cross-module feature should inject a ClassFuncMaps entry"), InjectedEntry))
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= Test.TestTrue(TEXT("Late-registered cross-module entry should be bound"), IsEntryFunctionBound(*InjectedEntry));
	bPassed &= Test.TestTrue(TEXT("Late-registered cross-module entry should use the generic bridge"), InjectedEntry->bGenericCall);
	bPassed &= Test.TestEqual(TEXT("Late-registered cross-module entry should carry the source entry as user data"), InjectedEntry->UserData, static_cast<void*>(const_cast<FAngelscriptCrossModuleEntry*>(&Entry)));
	return bPassed;
}

bool FAngelscriptCrossModuleDirectBindRuntimeTests::RunWorkerThreadRegistration(FAutomationTestBase& Test)
{
	if (!EnsureCrossModuleSubscriptionReady())
	{
		return true;
	}

	FScopedClassFuncMapRestore RestoreActorEntries(AActor::StaticClass());
	const FAngelscriptCrossModuleEntry Entry = { TestClassName, WorkerFunctionName, &NoOpThunk, 0, 0, 0 };
	FTestCrossModuleFeature Feature(&Entry, 1, TestModuleName, FAngelscriptCrossModuleBindings::LayoutVersionExpected);
	TAtomic<bool> bEntryVisibleOnWorkerThread(false);

	TFuture<void> Worker = Async(EAsyncExecution::ThreadPool, [&Feature, &bEntryVisibleOnWorkerThread]()
	{
		IModularFeatures::Get().RegisterModularFeature(FAngelscriptCrossModuleBindings::FeatureName(), &Feature);
		bEntryVisibleOnWorkerThread = FindActorEntry(WorkerFunctionName) != nullptr;
	});
	Worker.Wait();

	ON_SCOPE_EXIT
	{
		IModularFeatures::Get().UnregisterModularFeature(FAngelscriptCrossModuleBindings::FeatureName(), &Feature);
	};

	bool bPassed = true;
	bPassed &= Test.TestFalse(TEXT("Worker-thread registration should not mutate ClassFuncMaps on the worker thread"), bEntryVisibleOnWorkerThread.Load());
	bPassed &= Test.TestTrue(TEXT("Worker-thread registration should inject on the game thread"), WaitForGameThreadFeatureInjection(WorkerFunctionName));

	const FFuncEntry* InjectedEntry = FindActorEntry(WorkerFunctionName);
	if (!Test.TestNotNull(TEXT("Worker-thread cross-module feature should inject a ClassFuncMaps entry"), InjectedEntry))
	{
		return false;
	}

	bPassed &= Test.TestTrue(TEXT("Worker-thread cross-module entry should be bound"), IsEntryFunctionBound(*InjectedEntry));
	bPassed &= Test.TestTrue(TEXT("Worker-thread cross-module entry should use the generic bridge"), InjectedEntry->bGenericCall);
	bPassed &= Test.TestEqual(TEXT("Worker-thread cross-module entry should carry the source entry as user data"), InjectedEntry->UserData, static_cast<void*>(const_cast<FAngelscriptCrossModuleEntry*>(&Entry)));
	return bPassed;
}

bool FAngelscriptCrossModuleDirectBindRuntimeTests::RunGenericHookFrameThunk(FAutomationTestBase& Test)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	FAngelscriptEngineScope Scope(Engine);

	FGenericHookProbeState ProbeState;
	TGuardValue<FGenericHookProbeState*> ProbeStateGuard(GGenericHookProbeState, &ProbeState);

	FAngelscriptCrossModuleEntry Entry = {
		nullptr,
		TEXT("CrossModuleGenericHookProbe"),
		&SumThunk,
		2,
		sizeof(int32),
		FAngelscriptCrossModuleBindings::FlagStatic
	};

	const int32 FunctionId = GAngelscriptCrossModuleBindGlobalFunctionForTesting(
		"int CrossModuleGenericHookProbe(int Left, int Right)",
		&Entry);
	if (!Test.TestTrue(TEXT("Cross-module generic hook bridge should register a global function"), FunctionId >= 0))
	{
		return false;
	}

	FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("CrossModuleGenericHookFrameThunk"), TEXT(R"(
int Run()
{
	return CrossModuleGenericHookProbe(17, 25);
}
)"));
	if (!Test.TestTrue(TEXT("Cross-module generic hook script should compile"), ModuleScope.IsValid()))
	{
		return false;
	}

	FAngelscriptTestExecutor Executor(Test, Engine, ModuleScope.GetModule(), TEXT("int Run()"));
	if (!Test.TestTrue(TEXT("Cross-module generic hook script entry should be invokable"), Executor.IsValid()))
	{
		return false;
	}

	const int32 Result = Executor.ExecuteAndGet<int32>(INDEX_NONE);

	bool bPassed = true;
	bPassed &= Test.TestEqual(TEXT("Cross-module generic hook should return the frame thunk result"), Result, 42);
	bPassed &= Test.TestEqual(TEXT("Cross-module generic hook should invoke the frame thunk once"), ProbeState.HitCount, 1);
	bPassed &= Test.TestEqual(TEXT("Cross-module generic hook frame should expose arg count"), ProbeState.ArgCount, 2);
	bPassed &= Test.TestEqual(TEXT("Cross-module generic hook should pass arg 0 through a frame slot"), ProbeState.Left, 17);
	bPassed &= Test.TestEqual(TEXT("Cross-module generic hook should pass arg 1 through a frame slot"), ProbeState.Right, 25);
	bPassed &= Test.TestTrue(TEXT("Cross-module static generic hook should pass null Self"), ProbeState.bSelfWasNull);
	bPassed &= Test.TestTrue(TEXT("Cross-module generic hook should provide valid frame arg slots"), ProbeState.bArgSlotsWereValid);
	bPassed &= Test.TestTrue(TEXT("Cross-module generic hook should provide a return slot"), ProbeState.bReturnWasValid);
	bPassed &= Test.TestTrue(TEXT("Cross-module static generic hook should leave ScriptSelf null"), ProbeState.bScriptSelfWasNull);
	bPassed &= Test.TestTrue(TEXT("Cross-module generic hook should leave WorldContext null until a policy exists"), ProbeState.bWorldContextWasNull);
	bPassed &= Test.TestEqual(TEXT("Cross-module generic hook should copy entry flags into the frame"), ProbeState.FrameFlags, FAngelscriptCrossModuleBindings::FlagStatic);
	return bPassed;
}

bool FAngelscriptCrossModuleDirectBindRuntimeTests::RunSameModuleMultipleFeature(FAutomationTestBase& Test)
{
	if (!EnsureCrossModuleSubscriptionReady())
	{
		return true;
	}

	FScopedClassFuncMapRestore RestoreActorEntries(AActor::StaticClass());
	const FAngelscriptCrossModuleEntry EntryA = { TestClassName, MultiShardFunctionNameA, &NoOpThunk, 0, 0, 0 };
	const FAngelscriptCrossModuleEntry EntryB = { TestClassName, MultiShardFunctionNameB, &NoOpThunk, 0, 0, 0 };
	FTestCrossModuleFeature FeatureA(&EntryA, 1, TestModuleName, FAngelscriptCrossModuleBindings::LayoutVersionExpected);
	FTestCrossModuleFeature FeatureB(&EntryB, 1, TestModuleName, FAngelscriptCrossModuleBindings::LayoutVersionExpected);

	IModularFeatures::Get().RegisterModularFeature(FAngelscriptCrossModuleBindings::FeatureName(), &FeatureA);
	IModularFeatures::Get().RegisterModularFeature(FAngelscriptCrossModuleBindings::FeatureName(), &FeatureB);
	ON_SCOPE_EXIT
	{
		IModularFeatures::Get().UnregisterModularFeature(FAngelscriptCrossModuleBindings::FeatureName(), &FeatureB);
		IModularFeatures::Get().UnregisterModularFeature(FAngelscriptCrossModuleBindings::FeatureName(), &FeatureA);
	};

	const FFuncEntry* InjectedEntryA = FindActorEntry(MultiShardFunctionNameA);
	const FFuncEntry* InjectedEntryB = FindActorEntry(MultiShardFunctionNameB);
	if (!Test.TestNotNull(TEXT("First same-module cross-module feature should inject an entry"), InjectedEntryA) ||
		!Test.TestNotNull(TEXT("Second same-module cross-module feature should inject an entry"), InjectedEntryB))
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= Test.TestTrue(TEXT("First same-module feature entry should be bound"), IsEntryFunctionBound(*InjectedEntryA));
	bPassed &= Test.TestTrue(TEXT("Second same-module feature entry should be bound"), IsEntryFunctionBound(*InjectedEntryB));
	bPassed &= Test.TestEqual(TEXT("First same-module feature should keep its own user data"), InjectedEntryA->UserData, static_cast<void*>(const_cast<FAngelscriptCrossModuleEntry*>(&EntryA)));
	bPassed &= Test.TestEqual(TEXT("Second same-module feature should keep its own user data"), InjectedEntryB->UserData, static_cast<void*>(const_cast<FAngelscriptCrossModuleEntry*>(&EntryB)));
	return bPassed;
}

bool FAngelscriptCrossModuleDirectBindRuntimeTests::RunLayoutMismatch(FAutomationTestBase& Test)
{
	if (!EnsureCrossModuleSubscriptionReady())
	{
		return true;
	}

	FScopedClassFuncMapRestore RestoreActorEntries(AActor::StaticClass());
	const FAngelscriptCrossModuleEntry Entry = { TestClassName, MismatchFunctionName, &NoOpThunk, 0, 0, 0 };
	FTestCrossModuleFeature Feature(&Entry, 1, TestModuleName, 0xDEADBEEFu);

	Test.AddExpectedErrorPlain(TEXT("Cross-module binding feature skipped because layout version"), EAutomationExpectedErrorFlags::Contains, 1);
	RegisterAndUnregisterFeature(Feature);

	return Test.TestNull(TEXT("Layout-mismatched cross-module feature should not inject an entry"), FindActorEntry(MismatchFunctionName));
}

bool FAngelscriptCrossModuleDirectBindRuntimeTests::RunMalformedFeature(FAutomationTestBase& Test)
{
	if (!EnsureCrossModuleSubscriptionReady())
	{
		return true;
	}

	FScopedClassFuncMapRestore RestoreActorEntries(AActor::StaticClass());
	const FAngelscriptCrossModuleEntry Entry = { TestClassName, MalformedFunctionName, &NoOpThunk, 0, 0, 0 };

	FTestCrossModuleFeature NegativeCountFeature(&Entry, -1, TestModuleName, FAngelscriptCrossModuleBindings::LayoutVersionExpected);
	FTestCrossModuleFeature NullTableFeature(nullptr, 1, TestModuleName, FAngelscriptCrossModuleBindings::LayoutVersionExpected);
	FTestCrossModuleFeature NullModuleFeature(&Entry, 1, nullptr, FAngelscriptCrossModuleBindings::LayoutVersionExpected);

	Test.AddExpectedErrorPlain(TEXT("Cross-module binding feature skipped because its payload is malformed."), EAutomationExpectedErrorFlags::Contains, 3);
	RegisterAndUnregisterFeature(NegativeCountFeature);
	RegisterAndUnregisterFeature(NullTableFeature);
	RegisterAndUnregisterFeature(NullModuleFeature);

	return Test.TestNull(TEXT("Malformed cross-module features should not inject entries"), FindActorEntry(MalformedFunctionName));
}

#endif
