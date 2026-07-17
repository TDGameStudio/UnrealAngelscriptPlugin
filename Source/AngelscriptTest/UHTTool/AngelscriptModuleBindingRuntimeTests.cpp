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
#include "Bindings/AngelscriptModuleBindingProtocol.h"

#if WITH_ANGELSCRIPT_UNITTESTS && WITH_ANGELSCRIPT_MODULE_LOCAL_BINDINGS

ANGELSCRIPTRUNTIME_API void GAngelscriptModuleBindingEnsureRegisteredForTesting();
ANGELSCRIPTRUNTIME_API int32 GAngelscriptModuleBindingBindGlobalFunctionForTesting(const ANSICHAR* Signature, FAngelscriptModuleBinding* Binding);

TEST_CLASS_WITH_FLAGS(FAngelscriptModuleBindingDirectBindRuntimeTests,
	"Angelscript.CppTests.UHTToolResolver.ModuleBindingDirectBind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static constexpr const TCHAR* TestModuleName = TEXT("Engine");
	static constexpr const TCHAR* TestClassName = TEXT("Actor");
	static constexpr const TCHAR* LateFunctionName = TEXT("ModuleBindingLateLoadedAutomationProbe");
	static constexpr const TCHAR* WorkerFunctionName = TEXT("ModuleBindingWorkerThreadAutomationProbe");
	static constexpr const TCHAR* MismatchFunctionName = TEXT("ModuleBindingLayoutMismatchAutomationProbe");
	static constexpr const TCHAR* MalformedFunctionName = TEXT("ModuleBindingMalformedAutomationProbe");
	static constexpr const TCHAR* ExistingFunctionName = TEXT("ModuleBindingExistingSlotAutomationProbe");
	static constexpr const TCHAR* MultiShardFunctionNameA = TEXT("ModuleBindingMultiShardAutomationProbeA");
	static constexpr const TCHAR* MultiShardFunctionNameB = TEXT("ModuleBindingMultiShardAutomationProbeB");

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

	static void NoOpThunk(UObject* /*Self*/, FAngelscriptModuleBindingCallFrame* /*Frame*/)
	{
	}

	static void SumThunk(UObject* Self, FAngelscriptModuleBindingCallFrame* Frame)
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
		TMap<FString, FAngelscriptFunctionBinding> SavedClassMap;

		explicit FScopedClassFuncMapRestore(UClass* InClass)
			: Class(InClass)
		{
			if (Class == nullptr)
			{
				return;
			}

			if (TMap<FString, FAngelscriptFunctionBinding>* ExistingMap = FAngelscriptBinds::GetClassFunctionBindings().Find(Class))
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

			TMap<UClass*, TMap<FString, FAngelscriptFunctionBinding>>& ClassFunctionBindings = FAngelscriptBinds::GetClassFunctionBindings();
			if (bHadClassMap)
			{
				ClassFunctionBindings.FindOrAdd(Class) = SavedClassMap;
			}
			else
			{
				ClassFunctionBindings.Remove(Class);
			}
		}
	};

	struct FTestModuleBindingFeature : public IModularFeature
	{
		const FAngelscriptModuleBinding* Table;
		int32 Count;
		const TCHAR* ModuleName;
		uint32 LayoutVersion;

		FTestModuleBindingFeature(const FAngelscriptModuleBinding* InTable, int32 InCount, const TCHAR* InModuleName, uint32 InLayoutVersion)
			: Table(InTable)
			, Count(InCount)
			, ModuleName(InModuleName)
			, LayoutVersion(InLayoutVersion)
		{
		}
	};

	static void RegisterAndUnregisterFeature(FTestModuleBindingFeature& Feature)
	{
		IModularFeatures::Get().RegisterModularFeature(FAngelscriptModuleBindingProtocol::FeatureName(), &Feature);
		IModularFeatures::Get().UnregisterModularFeature(FAngelscriptModuleBindingProtocol::FeatureName(), &Feature);
	}

	static const FAngelscriptFunctionBinding* FindActorEntry(const TCHAR* FunctionName)
	{
		const TMap<FString, FAngelscriptFunctionBinding>* ActorEntries = FAngelscriptBinds::GetClassFunctionBindings().Find(AActor::StaticClass());
		return ActorEntries != nullptr ? ActorEntries->Find(FunctionName) : nullptr;
	}

	static bool IsEntryFunctionBound(const FAngelscriptFunctionBinding& Entry)
	{
		return const_cast<FGenericFuncPtr&>(Entry.FunctionPointer).IsBound();
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

	static bool EnsureModuleBindingSubscriptionReady()
	{
		GAngelscriptModuleBindingEnsureRegisteredForTesting();
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

FAngelscriptModuleBindingDirectBindRuntimeTests::FGenericHookProbeState* FAngelscriptModuleBindingDirectBindRuntimeTests::GGenericHookProbeState = nullptr;

bool FAngelscriptModuleBindingDirectBindRuntimeTests::RunExistingSlotPriority(FAutomationTestBase& Test)
{
	if (!EnsureModuleBindingSubscriptionReady())
	{
		return true;
	}

	FScopedClassFuncMapRestore RestoreActorEntries(AActor::StaticClass());
	const FAngelscriptFunctionBinding ExistingEntry = { FGenericFuncPtr::Make(1), ASAutoCaller::FunctionCaller{}, reinterpret_cast<void*>(0x1), false, false };
	FAngelscriptBinds::RegisterFunctionBinding(AActor::StaticClass(), ExistingFunctionName, ExistingEntry);

	const FAngelscriptModuleBinding Binding = { TestClassName, ExistingFunctionName, &NoOpThunk, 0, 0, 0 };
	FTestModuleBindingFeature Feature(&Binding, 1, TestModuleName, FAngelscriptModuleBindingProtocol::LayoutVersionExpected);
	RegisterAndUnregisterFeature(Feature);

	const FAngelscriptFunctionBinding* FinalEntry = FindActorEntry(ExistingFunctionName);
	if (!Test.TestNotNull(TEXT("Existing same-module entry should remain present"), FinalEntry))
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= Test.TestEqual(TEXT("Module binding injection should not replace an already bound slot"), FinalEntry->UserData, ExistingEntry.UserData);
	bPassed &= Test.TestFalse(TEXT("Existing non-generic slot should keep its original call mode"), FinalEntry->bUsesGenericCall);
	return bPassed;
}

bool FAngelscriptModuleBindingDirectBindRuntimeTests::RunLateRegistration(FAutomationTestBase& Test)
{
	if (!EnsureModuleBindingSubscriptionReady())
	{
		return true;
	}

	FScopedClassFuncMapRestore RestoreActorEntries(AActor::StaticClass());
	const FAngelscriptModuleBinding Binding = { TestClassName, LateFunctionName, &NoOpThunk, 0, 0, 0 };
	FTestModuleBindingFeature Feature(&Binding, 1, TestModuleName, FAngelscriptModuleBindingProtocol::LayoutVersionExpected);

	RegisterAndUnregisterFeature(Feature);

	const FAngelscriptFunctionBinding* InjectedEntry = FindActorEntry(LateFunctionName);
	if (!Test.TestNotNull(TEXT("Late-registered module-binding feature should inject a ClassFunctionBindings entry"), InjectedEntry))
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= Test.TestTrue(TEXT("Late-registered module-binding entry should be bound"), IsEntryFunctionBound(*InjectedEntry));
	bPassed &= Test.TestTrue(TEXT("Late-registered module-binding entry should use the generic bridge"), InjectedEntry->bUsesGenericCall);
	bPassed &= Test.TestEqual(TEXT("Late-registered module-binding entry should carry the source entry as user data"), InjectedEntry->UserData, static_cast<void*>(const_cast<FAngelscriptModuleBinding*>(&Binding)));
	return bPassed;
}

bool FAngelscriptModuleBindingDirectBindRuntimeTests::RunWorkerThreadRegistration(FAutomationTestBase& Test)
{
	if (!EnsureModuleBindingSubscriptionReady())
	{
		return true;
	}

	FScopedClassFuncMapRestore RestoreActorEntries(AActor::StaticClass());
	const FAngelscriptModuleBinding Binding = { TestClassName, WorkerFunctionName, &NoOpThunk, 0, 0, 0 };
	FTestModuleBindingFeature Feature(&Binding, 1, TestModuleName, FAngelscriptModuleBindingProtocol::LayoutVersionExpected);
	TAtomic<bool> bEntryVisibleOnWorkerThread(false);

	TFuture<void> Worker = Async(EAsyncExecution::ThreadPool, [&Feature, &bEntryVisibleOnWorkerThread]()
	{
		IModularFeatures::Get().RegisterModularFeature(FAngelscriptModuleBindingProtocol::FeatureName(), &Feature);
		bEntryVisibleOnWorkerThread = FindActorEntry(WorkerFunctionName) != nullptr;
	});
	Worker.Wait();

	ON_SCOPE_EXIT
	{
		IModularFeatures::Get().UnregisterModularFeature(FAngelscriptModuleBindingProtocol::FeatureName(), &Feature);
	};

	bool bPassed = true;
	bPassed &= Test.TestFalse(TEXT("Worker-thread registration should not mutate ClassFunctionBindings on the worker thread"), bEntryVisibleOnWorkerThread.Load());
	bPassed &= Test.TestTrue(TEXT("Worker-thread registration should inject on the game thread"), WaitForGameThreadFeatureInjection(WorkerFunctionName));

	const FAngelscriptFunctionBinding* InjectedEntry = FindActorEntry(WorkerFunctionName);
	if (!Test.TestNotNull(TEXT("Worker-thread module-binding feature should inject a ClassFunctionBindings entry"), InjectedEntry))
	{
		return false;
	}

	bPassed &= Test.TestTrue(TEXT("Worker-thread module-binding entry should be bound"), IsEntryFunctionBound(*InjectedEntry));
	bPassed &= Test.TestTrue(TEXT("Worker-thread module-binding entry should use the generic bridge"), InjectedEntry->bUsesGenericCall);
	bPassed &= Test.TestEqual(TEXT("Worker-thread module-binding entry should carry the source entry as user data"), InjectedEntry->UserData, static_cast<void*>(const_cast<FAngelscriptModuleBinding*>(&Binding)));
	return bPassed;
}

bool FAngelscriptModuleBindingDirectBindRuntimeTests::RunGenericHookFrameThunk(FAutomationTestBase& Test)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	FAngelscriptEngineScope Scope(Engine);

	FGenericHookProbeState ProbeState;
	TGuardValue<FGenericHookProbeState*> ProbeStateGuard(GGenericHookProbeState, &ProbeState);

	FAngelscriptModuleBinding Binding = {
		nullptr,
		TEXT("ModuleBindingGenericHookProbe"),
		&SumThunk,
		2,
		sizeof(int32),
		FAngelscriptModuleBindingProtocol::FlagStatic
	};

	const int32 FunctionId = GAngelscriptModuleBindingBindGlobalFunctionForTesting(
		"int ModuleBindingGenericHookProbe(int Left, int Right)",
		&Binding);
	if (!Test.TestTrue(TEXT("Module binding generic hook bridge should register a global function"), FunctionId >= 0))
	{
		return false;
	}

	FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ModuleBindingGenericHookFrameThunk"), TEXT(R"(
int Run()
{
	return ModuleBindingGenericHookProbe(17, 25);
}
)"));
	if (!Test.TestTrue(TEXT("Module binding generic hook script should compile"), ModuleScope.IsValid()))
	{
		return false;
	}

	FAngelscriptTestExecutor Executor(Test, Engine, ModuleScope.GetModule(), TEXT("int Run()"));
	if (!Test.TestTrue(TEXT("Module binding generic hook script entry should be invokable"), Executor.IsValid()))
	{
		return false;
	}

	const int32 Result = Executor.ExecuteAndGet<int32>(INDEX_NONE);

	bool bPassed = true;
	bPassed &= Test.TestEqual(TEXT("Module binding generic hook should return the frame thunk result"), Result, 42);
	bPassed &= Test.TestEqual(TEXT("Module binding generic hook should invoke the frame thunk once"), ProbeState.HitCount, 1);
	bPassed &= Test.TestEqual(TEXT("Module binding generic hook frame should expose arg count"), ProbeState.ArgCount, 2);
	bPassed &= Test.TestEqual(TEXT("Module binding generic hook should pass arg 0 through a frame slot"), ProbeState.Left, 17);
	bPassed &= Test.TestEqual(TEXT("Module binding generic hook should pass arg 1 through a frame slot"), ProbeState.Right, 25);
	bPassed &= Test.TestTrue(TEXT("Module binding static generic hook should pass null Self"), ProbeState.bSelfWasNull);
	bPassed &= Test.TestTrue(TEXT("Module binding generic hook should provide valid frame arg slots"), ProbeState.bArgSlotsWereValid);
	bPassed &= Test.TestTrue(TEXT("Module binding generic hook should provide a return slot"), ProbeState.bReturnWasValid);
	bPassed &= Test.TestTrue(TEXT("Module binding static generic hook should leave ScriptSelf null"), ProbeState.bScriptSelfWasNull);
	bPassed &= Test.TestTrue(TEXT("Module binding generic hook should leave WorldContext null until a policy exists"), ProbeState.bWorldContextWasNull);
	bPassed &= Test.TestEqual(TEXT("Module binding generic hook should copy entry flags into the frame"), ProbeState.FrameFlags, FAngelscriptModuleBindingProtocol::FlagStatic);
	return bPassed;
}

bool FAngelscriptModuleBindingDirectBindRuntimeTests::RunSameModuleMultipleFeature(FAutomationTestBase& Test)
{
	if (!EnsureModuleBindingSubscriptionReady())
	{
		return true;
	}

	FScopedClassFuncMapRestore RestoreActorEntries(AActor::StaticClass());
	const FAngelscriptModuleBinding BindingA = { TestClassName, MultiShardFunctionNameA, &NoOpThunk, 0, 0, 0 };
	const FAngelscriptModuleBinding BindingB = { TestClassName, MultiShardFunctionNameB, &NoOpThunk, 0, 0, 0 };
	FTestModuleBindingFeature FeatureA(&BindingA, 1, TestModuleName, FAngelscriptModuleBindingProtocol::LayoutVersionExpected);
	FTestModuleBindingFeature FeatureB(&BindingB, 1, TestModuleName, FAngelscriptModuleBindingProtocol::LayoutVersionExpected);

	IModularFeatures::Get().RegisterModularFeature(FAngelscriptModuleBindingProtocol::FeatureName(), &FeatureA);
	IModularFeatures::Get().RegisterModularFeature(FAngelscriptModuleBindingProtocol::FeatureName(), &FeatureB);
	ON_SCOPE_EXIT
	{
		IModularFeatures::Get().UnregisterModularFeature(FAngelscriptModuleBindingProtocol::FeatureName(), &FeatureB);
		IModularFeatures::Get().UnregisterModularFeature(FAngelscriptModuleBindingProtocol::FeatureName(), &FeatureA);
	};

	const FAngelscriptFunctionBinding* InjectedEntryA = FindActorEntry(MultiShardFunctionNameA);
	const FAngelscriptFunctionBinding* InjectedEntryB = FindActorEntry(MultiShardFunctionNameB);
	if (!Test.TestNotNull(TEXT("First same-module module-binding feature should inject an entry"), InjectedEntryA) ||
		!Test.TestNotNull(TEXT("Second same-module module-binding feature should inject an entry"), InjectedEntryB))
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= Test.TestTrue(TEXT("First same-module feature entry should be bound"), IsEntryFunctionBound(*InjectedEntryA));
	bPassed &= Test.TestTrue(TEXT("Second same-module feature entry should be bound"), IsEntryFunctionBound(*InjectedEntryB));
	bPassed &= Test.TestEqual(TEXT("First same-module feature should keep its own user data"), InjectedEntryA->UserData, static_cast<void*>(const_cast<FAngelscriptModuleBinding*>(&BindingA)));
	bPassed &= Test.TestEqual(TEXT("Second same-module feature should keep its own user data"), InjectedEntryB->UserData, static_cast<void*>(const_cast<FAngelscriptModuleBinding*>(&BindingB)));
	return bPassed;
}

bool FAngelscriptModuleBindingDirectBindRuntimeTests::RunLayoutMismatch(FAutomationTestBase& Test)
{
	if (!EnsureModuleBindingSubscriptionReady())
	{
		return true;
	}

	FScopedClassFuncMapRestore RestoreActorEntries(AActor::StaticClass());
	const FAngelscriptModuleBinding Binding = { TestClassName, MismatchFunctionName, &NoOpThunk, 0, 0, 0 };
	FTestModuleBindingFeature Feature(&Binding, 1, TestModuleName, 0xDEADBEEFu);

	Test.AddExpectedErrorPlain(TEXT("Module binding binding feature skipped because layout version"), EAutomationExpectedErrorFlags::Contains, 1);
	RegisterAndUnregisterFeature(Feature);

	return Test.TestNull(TEXT("Layout-mismatched module-binding feature should not inject an entry"), FindActorEntry(MismatchFunctionName));
}

bool FAngelscriptModuleBindingDirectBindRuntimeTests::RunMalformedFeature(FAutomationTestBase& Test)
{
	if (!EnsureModuleBindingSubscriptionReady())
	{
		return true;
	}

	FScopedClassFuncMapRestore RestoreActorEntries(AActor::StaticClass());
	const FAngelscriptModuleBinding Binding = { TestClassName, MalformedFunctionName, &NoOpThunk, 0, 0, 0 };

	FTestModuleBindingFeature NegativeCountFeature(&Binding, -1, TestModuleName, FAngelscriptModuleBindingProtocol::LayoutVersionExpected);
	FTestModuleBindingFeature NullTableFeature(nullptr, 1, TestModuleName, FAngelscriptModuleBindingProtocol::LayoutVersionExpected);
	FTestModuleBindingFeature NullModuleFeature(&Binding, 1, nullptr, FAngelscriptModuleBindingProtocol::LayoutVersionExpected);

	Test.AddExpectedErrorPlain(TEXT("Module binding binding feature skipped because its payload is malformed."), EAutomationExpectedErrorFlags::Contains, 3);
	RegisterAndUnregisterFeature(NegativeCountFeature);
	RegisterAndUnregisterFeature(NullTableFeature);
	RegisterAndUnregisterFeature(NullModuleFeature);

	return Test.TestNull(TEXT("Malformed module-binding features should not inject entries"), FindActorEntry(MalformedFunctionName));
}

#endif
