#include "Misc/AutomationTest.h"

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

#if WITH_DEV_AUTOMATION_TESTS

ANGELSCRIPTRUNTIME_API void GAngelscriptCrossModuleEnsureRegisteredForTesting();
ANGELSCRIPTRUNTIME_API int32 GAngelscriptCrossModuleBindGlobalFunctionForTesting(const ANSICHAR* Signature, FAngelscriptCrossModuleEntry* Entry);

namespace AngelscriptTest_UHTTool_CrossModuleDirect_Private
{
	constexpr const TCHAR* TestModuleName = TEXT("Engine");
	constexpr const TCHAR* TestClassName = TEXT("Actor");
	constexpr const TCHAR* LateFunctionName = TEXT("CrossModuleLateLoadedAutomationProbe");
	constexpr const TCHAR* WorkerFunctionName = TEXT("CrossModuleWorkerThreadAutomationProbe");
	constexpr const TCHAR* MismatchFunctionName = TEXT("CrossModuleLayoutMismatchAutomationProbe");
	constexpr const TCHAR* MalformedFunctionName = TEXT("CrossModuleMalformedAutomationProbe");
	constexpr const TCHAR* ExistingFunctionName = TEXT("CrossModuleExistingSlotAutomationProbe");
	constexpr const TCHAR* MultiShardFunctionNameA = TEXT("CrossModuleMultiShardAutomationProbeA");
	constexpr const TCHAR* MultiShardFunctionNameB = TEXT("CrossModuleMultiShardAutomationProbeB");

	struct FGenericHookProbeState
	{
		int32 HitCount = 0;
		int32 Left = 0;
		int32 Right = 0;
		bool bSelfWasNull = false;
		bool bArgsWereValid = false;
		bool bReturnWasValid = false;
	};

	FGenericHookProbeState* GGenericHookProbeState = nullptr;

	void NoOpThunk(UObject* /*Self*/, void** /*Args*/, void* /*Ret*/)
	{
	}

	void SumThunk(UObject* Self, void** Args, void* Ret)
	{
		if (GGenericHookProbeState == nullptr)
		{
			return;
		}

		++GGenericHookProbeState->HitCount;
		GGenericHookProbeState->bSelfWasNull = Self == nullptr;
		GGenericHookProbeState->bArgsWereValid = Args != nullptr && Args[0] != nullptr && Args[1] != nullptr;
		GGenericHookProbeState->bReturnWasValid = Ret != nullptr;
		if (!GGenericHookProbeState->bArgsWereValid || !GGenericHookProbeState->bReturnWasValid)
		{
			return;
		}

		GGenericHookProbeState->Left = *static_cast<int32*>(Args[0]);
		GGenericHookProbeState->Right = *static_cast<int32*>(Args[1]);
		*static_cast<int32*>(Ret) = GGenericHookProbeState->Left + GGenericHookProbeState->Right;
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

	void RegisterAndUnregisterFeature(FTestCrossModuleFeature& Feature)
	{
		IModularFeatures::Get().RegisterModularFeature(FAngelscriptCrossModuleBindings::FeatureName(), &Feature);
		IModularFeatures::Get().UnregisterModularFeature(FAngelscriptCrossModuleBindings::FeatureName(), &Feature);
	}

	const FFuncEntry* FindActorEntry(const TCHAR* FunctionName)
	{
		const TMap<FString, FFuncEntry>* ActorEntries = FAngelscriptBinds::GetClassFuncMaps().Find(AActor::StaticClass());
		return ActorEntries != nullptr ? ActorEntries->Find(FunctionName) : nullptr;
	}

	bool IsEntryFunctionBound(const FFuncEntry& Entry)
	{
		return const_cast<FGenericFuncPtr&>(Entry.FuncPtr).IsBound();
	}

	bool WaitForGameThreadFeatureInjection(const TCHAR* FunctionName, double TimeoutSeconds = 5.0)
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

	bool EnsureCrossModuleSubscriptionReady()
	{
		GAngelscriptCrossModuleEnsureRegisteredForTesting();
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCrossModuleExistingSlotPriorityTest,
	"Angelscript.CppTests.UHTToolResolver.CrossModuleDirectBind.SameModuleShardWins_When_BothExist",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCrossModuleExistingSlotPriorityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace AngelscriptTest_UHTTool_CrossModuleDirect_Private;

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
	if (!TestNotNull(TEXT("Existing same-module entry should remain present"), FinalEntry))
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= TestEqual(TEXT("Cross-module injection should not replace an already bound slot"), FinalEntry->UserData, ExistingEntry.UserData);
	bPassed &= TestFalse(TEXT("Existing non-generic slot should keep its original call mode"), FinalEntry->bGenericCall);
	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCrossModuleLateRegistrationTest,
	"Angelscript.CppTests.UHTToolResolver.CrossModuleDirectBind.OnModularFeatureRegistered_LateLoadedModule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCrossModuleLateRegistrationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace AngelscriptTest_UHTTool_CrossModuleDirect_Private;

	if (!EnsureCrossModuleSubscriptionReady())
	{
		return true;
	}

	FScopedClassFuncMapRestore RestoreActorEntries(AActor::StaticClass());
	const FAngelscriptCrossModuleEntry Entry = { TestClassName, LateFunctionName, &NoOpThunk, 0, 0, 0 };
	FTestCrossModuleFeature Feature(&Entry, 1, TestModuleName, FAngelscriptCrossModuleBindings::LayoutVersionExpected);

	RegisterAndUnregisterFeature(Feature);

	const FFuncEntry* InjectedEntry = FindActorEntry(LateFunctionName);
	if (!TestNotNull(TEXT("Late-registered cross-module feature should inject a ClassFuncMaps entry"), InjectedEntry))
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= TestTrue(TEXT("Late-registered cross-module entry should be bound"), IsEntryFunctionBound(*InjectedEntry));
	bPassed &= TestTrue(TEXT("Late-registered cross-module entry should use the generic bridge"), InjectedEntry->bGenericCall);
	bPassed &= TestEqual(TEXT("Late-registered cross-module entry should carry the source entry as user data"), InjectedEntry->UserData, static_cast<void*>(const_cast<FAngelscriptCrossModuleEntry*>(&Entry)));
	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCrossModuleWorkerThreadRegistrationTest,
	"Angelscript.CppTests.UHTToolResolver.CrossModuleDirectBind.OnModularFeatureRegistered_WorkerThreadInvocation_MarshalsToGameThread",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCrossModuleWorkerThreadRegistrationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace AngelscriptTest_UHTTool_CrossModuleDirect_Private;

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
	bPassed &= TestFalse(TEXT("Worker-thread registration should not mutate ClassFuncMaps on the worker thread"), bEntryVisibleOnWorkerThread.Load());
	bPassed &= TestTrue(TEXT("Worker-thread registration should inject on the game thread"), WaitForGameThreadFeatureInjection(WorkerFunctionName));

	const FFuncEntry* InjectedEntry = FindActorEntry(WorkerFunctionName);
	if (!TestNotNull(TEXT("Worker-thread cross-module feature should inject a ClassFuncMaps entry"), InjectedEntry))
	{
		return false;
	}

	bPassed &= TestTrue(TEXT("Worker-thread cross-module entry should be bound"), IsEntryFunctionBound(*InjectedEntry));
	bPassed &= TestTrue(TEXT("Worker-thread cross-module entry should use the generic bridge"), InjectedEntry->bGenericCall);
	bPassed &= TestEqual(TEXT("Worker-thread cross-module entry should carry the source entry as user data"), InjectedEntry->UserData, static_cast<void*>(const_cast<FAngelscriptCrossModuleEntry*>(&Entry)));
	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCrossModuleGenericHookRawThunkTest,
	"Angelscript.CppTests.UHTToolResolver.CrossModuleDirectBind.GenericHook_RawThunkReceivesArgsAndReturn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCrossModuleGenericHookRawThunkTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace AngelscriptTest_UHTTool_CrossModuleDirect_Private;

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
	if (!TestTrue(TEXT("Cross-module generic hook bridge should register a global function"), FunctionId >= 0))
	{
		return false;
	}

	FScopedAngelscriptModule ModuleScope(*this, Engine, TEXT("CrossModuleGenericHookRawThunk"), TEXT(R"(
int Run()
{
	return CrossModuleGenericHookProbe(17, 25);
}
)"));
	if (!TestTrue(TEXT("Cross-module generic hook script should compile"), ModuleScope.IsValid()))
	{
		return false;
	}

	FAngelscriptTestExecutor Executor(*this, Engine, ModuleScope.GetModule(), TEXT("int Run()"));
	if (!TestTrue(TEXT("Cross-module generic hook script entry should be invokable"), Executor.IsValid()))
	{
		return false;
	}

	const int32 Result = Executor.ExecuteAndGet<int32>(INDEX_NONE);

	bool bPassed = true;
	bPassed &= TestEqual(TEXT("Cross-module generic hook should return the raw thunk result"), Result, 42);
	bPassed &= TestEqual(TEXT("Cross-module generic hook should invoke the raw thunk once"), ProbeState.HitCount, 1);
	bPassed &= TestEqual(TEXT("Cross-module generic hook should pass arg 0 through a raw Args slot"), ProbeState.Left, 17);
	bPassed &= TestEqual(TEXT("Cross-module generic hook should pass arg 1 through a raw Args slot"), ProbeState.Right, 25);
	bPassed &= TestTrue(TEXT("Cross-module static generic hook should pass null Self"), ProbeState.bSelfWasNull);
	bPassed &= TestTrue(TEXT("Cross-module generic hook should provide valid Args slots"), ProbeState.bArgsWereValid);
	bPassed &= TestTrue(TEXT("Cross-module generic hook should provide a return slot"), ProbeState.bReturnWasValid);
	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCrossModuleSameModuleMultipleFeatureTest,
	"Angelscript.CppTests.UHTToolResolver.CrossModuleDirectBind.MultipleFeaturesSameModule_AllInjected_NoModuleNameDedup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCrossModuleSameModuleMultipleFeatureTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace AngelscriptTest_UHTTool_CrossModuleDirect_Private;

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
	if (!TestNotNull(TEXT("First same-module cross-module feature should inject an entry"), InjectedEntryA) ||
		!TestNotNull(TEXT("Second same-module cross-module feature should inject an entry"), InjectedEntryB))
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= TestTrue(TEXT("First same-module feature entry should be bound"), IsEntryFunctionBound(*InjectedEntryA));
	bPassed &= TestTrue(TEXT("Second same-module feature entry should be bound"), IsEntryFunctionBound(*InjectedEntryB));
	bPassed &= TestEqual(TEXT("First same-module feature should keep its own user data"), InjectedEntryA->UserData, static_cast<void*>(const_cast<FAngelscriptCrossModuleEntry*>(&EntryA)));
	bPassed &= TestEqual(TEXT("Second same-module feature should keep its own user data"), InjectedEntryB->UserData, static_cast<void*>(const_cast<FAngelscriptCrossModuleEntry*>(&EntryB)));
	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCrossModuleLayoutMismatchTest,
	"Angelscript.CppTests.UHTToolResolver.CrossModuleDirectBind.LayoutVersionMismatch_FeatureSkipped_NoCrash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCrossModuleLayoutMismatchTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace AngelscriptTest_UHTTool_CrossModuleDirect_Private;

	if (!EnsureCrossModuleSubscriptionReady())
	{
		return true;
	}

	FScopedClassFuncMapRestore RestoreActorEntries(AActor::StaticClass());
	const FAngelscriptCrossModuleEntry Entry = { TestClassName, MismatchFunctionName, &NoOpThunk, 0, 0, 0 };
	FTestCrossModuleFeature Feature(&Entry, 1, TestModuleName, 0xDEADBEEFu);

	AddExpectedErrorPlain(TEXT("Cross-module binding feature skipped because layout version"), EAutomationExpectedErrorFlags::Contains, 1);
	RegisterAndUnregisterFeature(Feature);

	return TestNull(TEXT("Layout-mismatched cross-module feature should not inject an entry"), FindActorEntry(MismatchFunctionName));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCrossModuleMalformedFeatureTest,
	"Angelscript.CppTests.UHTToolResolver.CrossModuleDirectBind.RuntimeNullRangeValidation_RejectsMalformedFeature",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCrossModuleMalformedFeatureTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace AngelscriptTest_UHTTool_CrossModuleDirect_Private;

	if (!EnsureCrossModuleSubscriptionReady())
	{
		return true;
	}

	FScopedClassFuncMapRestore RestoreActorEntries(AActor::StaticClass());
	const FAngelscriptCrossModuleEntry Entry = { TestClassName, MalformedFunctionName, &NoOpThunk, 0, 0, 0 };

	FTestCrossModuleFeature NegativeCountFeature(&Entry, -1, TestModuleName, FAngelscriptCrossModuleBindings::LayoutVersionExpected);
	FTestCrossModuleFeature NullTableFeature(nullptr, 1, TestModuleName, FAngelscriptCrossModuleBindings::LayoutVersionExpected);
	FTestCrossModuleFeature NullModuleFeature(&Entry, 1, nullptr, FAngelscriptCrossModuleBindings::LayoutVersionExpected);

	AddExpectedErrorPlain(TEXT("Cross-module binding feature skipped because its payload is malformed."), EAutomationExpectedErrorFlags::Contains, 3);
	RegisterAndUnregisterFeature(NegativeCountFeature);
	RegisterAndUnregisterFeature(NullTableFeature);
	RegisterAndUnregisterFeature(NullModuleFeature);

	return TestNull(TEXT("Malformed cross-module features should not inject entries"), FindActorEntry(MalformedFunctionName));
}

#endif
