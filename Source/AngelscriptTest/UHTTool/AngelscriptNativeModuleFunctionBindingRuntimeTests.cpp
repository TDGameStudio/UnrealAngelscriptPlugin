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
#if WITH_ANGELSCRIPT_UNITTESTS && WITH_ANGELSCRIPT_NATIVE_MODULE_FUNCTION_ADDRESS

#include "FunctionBinding/NativeModuleFunctionBindingBridge.h"

ANGELSCRIPTRUNTIME_API void GAngelscriptNativeModuleFunctionBindingEnsureRegisteredForTesting();
ANGELSCRIPTRUNTIME_API int32 GAngelscriptNativeModuleFunctionBindingBindGlobalFunctionForTesting(
	FAngelscriptEngine& Engine,
	const ANSICHAR* Signature,
	FAngelscriptNativeModuleFunctionBinding* Binding);

TEST_CLASS_WITH_FLAGS(FAngelscriptNativeModuleFunctionBindingRuntimeTests,
	"Angelscript.CppTests.UHTToolResolver.NativeModuleFunctionBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static constexpr const TCHAR* TestModuleName = TEXT("Engine");
	static constexpr const TCHAR* TestClassName = TEXT("Actor");
	static constexpr const TCHAR* LateFunctionName = TEXT("NativeModuleFunctionBindingLateLoadedAutomationProbe");
	static constexpr const TCHAR* WorkerFunctionName = TEXT("NativeModuleFunctionBindingWorkerThreadAutomationProbe");
	static constexpr const TCHAR* MismatchFunctionName = TEXT("NativeModuleFunctionBindingLayoutMismatchAutomationProbe");
	static constexpr const TCHAR* MalformedFunctionName = TEXT("NativeModuleFunctionBindingMalformedAutomationProbe");
	static constexpr const TCHAR* ExistingFunctionName = TEXT("NativeModuleFunctionBindingExistingSlotAutomationProbe");
	static constexpr const TCHAR* MultiShardFunctionNameA = TEXT("NativeModuleFunctionBindingMultiShardAutomationProbeA");
	static constexpr const TCHAR* MultiShardFunctionNameB = TEXT("NativeModuleFunctionBindingMultiShardAutomationProbeB");
	static constexpr const TCHAR* DeferredUnregisterFunctionName = TEXT("NativeModuleFunctionBindingDeferredUnregisterAutomationProbe");
	static constexpr const TCHAR* ReloadFunctionName = TEXT("NativeModuleFunctionBindingReloadAutomationProbe");
	static constexpr const TCHAR* ExplicitTargetFunctionName = TEXT("NativeModuleFunctionBindingExplicitTargetAutomationProbe");

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

	static void NoOpThunk(UObject* /*Self*/, FAngelscriptNativeModuleFunctionBindingCallFrame* /*Frame*/)
	{
	}

	static void SumThunk(UObject* Self, FAngelscriptNativeModuleFunctionBindingCallFrame* Frame)
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

	struct FTestNativeModuleFunctionBindingFeature : public IModularFeature
	{
		const FAngelscriptNativeModuleFunctionBinding* Bindings;
		int32 Count;
		const TCHAR* ModuleName;
		uint32 LayoutVersion;

		FTestNativeModuleFunctionBindingFeature(const FAngelscriptNativeModuleFunctionBinding* InBindings, int32 InCount, const TCHAR* InModuleName, uint32 InLayoutVersion)
			: Bindings(InBindings)
			, Count(InCount)
			, ModuleName(InModuleName)
			, LayoutVersion(InLayoutVersion)
		{
		}
	};

	static void RegisterAndUnregisterFeature(FTestNativeModuleFunctionBindingFeature& Feature)
	{
		IModularFeatures::Get().RegisterModularFeature(FAngelscriptNativeModuleFunctionBindingBridge::FeatureName(), &Feature);
		IModularFeatures::Get().UnregisterModularFeature(FAngelscriptNativeModuleFunctionBindingBridge::FeatureName(), &Feature);
	}

	static const FAngelscriptFunctionBinding* FindActorFunctionBinding(const TCHAR* FunctionName)
	{
		const TMap<FString, FAngelscriptFunctionBinding>* ActorBindings = FAngelscriptBinds::GetClassFunctionBindings().Find(AActor::StaticClass());
		return ActorBindings != nullptr ? ActorBindings->Find(FunctionName) : nullptr;
	}

	static const FAngelscriptFunctionBinding* FindActorFunctionBinding(
		const FAngelscriptEngine& Engine,
		const TCHAR* FunctionName)
	{
		const FAngelscriptBindState* BindState = Engine.GetBindState();
		if (BindState == nullptr)
		{
			return nullptr;
		}

		const TMap<FString, FAngelscriptFunctionBinding>* ActorBindings =
			BindState->ClassFunctionBindings.Find(AActor::StaticClass());
		return ActorBindings != nullptr ? ActorBindings->Find(FunctionName) : nullptr;
	}

	static bool IsFunctionBindingBound(const FAngelscriptFunctionBinding& FunctionBinding)
	{
		return const_cast<FGenericFuncPtr&>(FunctionBinding.FunctionPointer).IsBound();
	}

	static bool WaitForGameThreadFeatureInjection(const TCHAR* FunctionName, double TimeoutSeconds = 5.0)
	{
		const double DeadlineSeconds = FPlatformTime::Seconds() + TimeoutSeconds;
		while (FPlatformTime::Seconds() < DeadlineSeconds)
		{
			FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
			if (FindActorFunctionBinding(FunctionName) != nullptr)
			{
				return true;
			}
			FPlatformProcess::Sleep(0.01f);
		}

		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
		return FindActorFunctionBinding(FunctionName) != nullptr;
	}

	static bool EnsureNativeModuleFunctionBindingSubscriptionReady()
	{
		GAngelscriptNativeModuleFunctionBindingEnsureRegisteredForTesting();
		return true;
	}

	static bool RunExistingSlotPriority(FAutomationTestBase& Test);
	static bool RunLateRegistration(FAutomationTestBase& Test);
	static bool RunWorkerThreadRegistration(FAutomationTestBase& Test);
	static bool RunDeferredUnregister(FAutomationTestBase& Test);
	static bool RunModuleReload(FAutomationTestBase& Test);
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

	TEST_METHOD(OnModularFeatureUnregistered_BeforeQueuedInjection_DoesNotReadPayload)
	{
		ASSERT_THAT(IsTrue(RunDeferredUnregister(*TestRunner)));
	}

	TEST_METHOD(OnModularFeatureReload_ReplacesOldDescriptor)
	{
		ASSERT_THAT(IsTrue(RunModuleReload(*TestRunner)));
	}

	TEST_METHOD(OnModularFeatureUnregistered_RemovesInjectedBinding)
	{
		if (!EnsureNativeModuleFunctionBindingSubscriptionReady())
		{
			return;
		}

		FScopedClassFuncMapRestore RestoreActorEntries(AActor::StaticClass());
		const FAngelscriptNativeModuleFunctionBinding Binding = { TestClassName, LateFunctionName, &NoOpThunk, 0, 0, 0 };
		FTestNativeModuleFunctionBindingFeature Feature(&Binding, 1, TestModuleName, FAngelscriptNativeModuleFunctionBindingBridge::LayoutVersionExpected);

		RegisterAndUnregisterFeature(Feature);

		ASSERT_THAT(IsTrue(
			FindActorFunctionBinding(LateFunctionName) == nullptr,
			TEXT("Unregistering a native module function binding feature should remove its injected binding")));
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

	TEST_METHOD(ExplicitEngineTargets_InjectDeduplicateAndRemoveWithoutCrossTalk)
	{
		if (!EnsureNativeModuleFunctionBindingSubscriptionReady())
		{
			return;
		}

		const FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> EngineA = CreateScriptScanFreeFullEngineForTesting(Config, Dependencies);
		TUniquePtr<FAngelscriptEngine> EngineB = CreateScriptScanFreeFullEngineForTesting(Config, Dependencies);
		ASSERT_THAT(IsNotNull(EngineA.Get(), TEXT("Explicit-target regression should create engine A")));
		ASSERT_THAT(IsNotNull(EngineB.Get(), TEXT("Explicit-target regression should create engine B")));

		const FAngelscriptFunctionBinding ExistingFunctionBinding = {
			FGenericFuncPtr::Make(1),
			ASAutoCaller::FunctionCaller{},
			reinterpret_cast<void*>(0x2),
			false,
			false
		};
		FAngelscriptBinds BindsA(*EngineA);
		BindsA.RegisterFunctionBindingForTarget(
			AActor::StaticClass(),
			ExplicitTargetFunctionName,
			ExistingFunctionBinding);

		const FAngelscriptNativeModuleFunctionBinding Binding = {
			TestClassName,
			ExplicitTargetFunctionName,
			&NoOpThunk,
			0,
			0,
			0
		};
		FTestNativeModuleFunctionBindingFeature Feature(
			&Binding,
			1,
			TestModuleName,
			FAngelscriptNativeModuleFunctionBindingBridge::LayoutVersionExpected);
		bool bFeatureRegistered = true;
		IModularFeatures::Get().RegisterModularFeature(
			FAngelscriptNativeModuleFunctionBindingBridge::FeatureName(),
			&Feature);
		ON_SCOPE_EXIT
		{
			if (bFeatureRegistered)
			{
				IModularFeatures::Get().UnregisterModularFeature(
					FAngelscriptNativeModuleFunctionBindingBridge::FeatureName(),
					&Feature);
			}
		};

		const FAngelscriptFunctionBinding* EngineABinding =
			FindActorFunctionBinding(*EngineA, ExplicitTargetFunctionName);
		const FAngelscriptFunctionBinding* EngineBBinding =
			FindActorFunctionBinding(*EngineB, ExplicitTargetFunctionName);
		ASSERT_THAT(IsNotNull(
			EngineABinding,
			TEXT("The feature should preserve engine A's pre-existing explicit slot")));
		ASSERT_THAT(IsNotNull(
			EngineBBinding,
			TEXT("The feature should inject independently into engine B's explicit slot")));
		ASSERT_THAT(AreEqual(
			ExistingFunctionBinding.UserData,
			EngineABinding->UserData,
			TEXT("Duplicate suppression should inspect only engine A's owned function table")));
		ASSERT_THAT(AreEqual(
			static_cast<void*>(const_cast<FAngelscriptNativeModuleFunctionBinding*>(&Binding)),
			EngineBBinding->UserData,
			TEXT("Engine B should receive the descriptor even when engine A already owns the same name")));

		EngineA.Reset();
		IModularFeatures::Get().UnregisterModularFeature(
			FAngelscriptNativeModuleFunctionBindingBridge::FeatureName(),
			&Feature);
		bFeatureRegistered = false;

		ASSERT_THAT(IsNull(
			FindActorFunctionBinding(*EngineB, ExplicitTargetFunctionName),
			TEXT("Feature unload after engine A shutdown should remove only engine B's injected slot")));
	}
};

FAngelscriptNativeModuleFunctionBindingRuntimeTests::FGenericHookProbeState* FAngelscriptNativeModuleFunctionBindingRuntimeTests::GGenericHookProbeState = nullptr;

bool FAngelscriptNativeModuleFunctionBindingRuntimeTests::RunExistingSlotPriority(FAutomationTestBase& Test)
{
	if (!EnsureNativeModuleFunctionBindingSubscriptionReady())
	{
		return true;
	}

	FScopedClassFuncMapRestore RestoreActorEntries(AActor::StaticClass());
	const FAngelscriptFunctionBinding ExistingFunctionBinding = { FGenericFuncPtr::Make(1), ASAutoCaller::FunctionCaller{}, reinterpret_cast<void*>(0x1), false, false };
	FAngelscriptBinds Binds(FAngelscriptEngine::Get());
	Binds.RegisterFunctionBindingForTarget(AActor::StaticClass(), ExistingFunctionName, ExistingFunctionBinding);

	const FAngelscriptNativeModuleFunctionBinding Binding = { TestClassName, ExistingFunctionName, &NoOpThunk, 0, 0, 0 };
	FTestNativeModuleFunctionBindingFeature Feature(&Binding, 1, TestModuleName, FAngelscriptNativeModuleFunctionBindingBridge::LayoutVersionExpected);
	RegisterAndUnregisterFeature(Feature);

	const FAngelscriptFunctionBinding* FinalFunctionBinding = FindActorFunctionBinding(ExistingFunctionName);
	if (!Test.TestNotNull(TEXT("Existing same-module function binding should remain present"), FinalFunctionBinding))
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= Test.TestEqual(TEXT("Native module function binding injection should not replace an already bound slot"), FinalFunctionBinding->UserData, ExistingFunctionBinding.UserData);
	bPassed &= Test.TestFalse(TEXT("Existing non-generic slot should keep its original call mode"), FinalFunctionBinding->bUsesGenericCall);
	return bPassed;
}

bool FAngelscriptNativeModuleFunctionBindingRuntimeTests::RunLateRegistration(FAutomationTestBase& Test)
{
	if (!EnsureNativeModuleFunctionBindingSubscriptionReady())
	{
		return true;
	}

	FScopedClassFuncMapRestore RestoreActorEntries(AActor::StaticClass());
	const FAngelscriptNativeModuleFunctionBinding Binding = { TestClassName, LateFunctionName, &NoOpThunk, 0, 0, 0 };
	FTestNativeModuleFunctionBindingFeature Feature(&Binding, 1, TestModuleName, FAngelscriptNativeModuleFunctionBindingBridge::LayoutVersionExpected);

	IModularFeatures::Get().RegisterModularFeature(FAngelscriptNativeModuleFunctionBindingBridge::FeatureName(), &Feature);
	ON_SCOPE_EXIT
	{
		IModularFeatures::Get().UnregisterModularFeature(FAngelscriptNativeModuleFunctionBindingBridge::FeatureName(), &Feature);
	};

	const FAngelscriptFunctionBinding* InjectedFunctionBinding = FindActorFunctionBinding(LateFunctionName);
	if (!Test.TestNotNull(TEXT("Late-registered native module function binding feature should inject a ClassFunctionBindings binding"), InjectedFunctionBinding))
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= Test.TestTrue(TEXT("Late-registered native module function binding should be bound"), IsFunctionBindingBound(*InjectedFunctionBinding));
	bPassed &= Test.TestTrue(TEXT("Late-registered native module function binding should use the generic bridge"), InjectedFunctionBinding->bUsesGenericCall);
	bPassed &= Test.TestEqual(TEXT("Late-registered native module function binding should carry the source binding as user data"), InjectedFunctionBinding->UserData, static_cast<void*>(const_cast<FAngelscriptNativeModuleFunctionBinding*>(&Binding)));
	return bPassed;
}

bool FAngelscriptNativeModuleFunctionBindingRuntimeTests::RunWorkerThreadRegistration(FAutomationTestBase& Test)
{
	if (!EnsureNativeModuleFunctionBindingSubscriptionReady())
	{
		return true;
	}

	FScopedClassFuncMapRestore RestoreActorEntries(AActor::StaticClass());
	const FAngelscriptNativeModuleFunctionBinding Binding = { TestClassName, WorkerFunctionName, &NoOpThunk, 0, 0, 0 };
	FTestNativeModuleFunctionBindingFeature Feature(&Binding, 1, TestModuleName, FAngelscriptNativeModuleFunctionBindingBridge::LayoutVersionExpected);
	TAtomic<bool> bFunctionBindingVisibleOnWorkerThread(false);

	TFuture<void> Worker = Async(EAsyncExecution::ThreadPool, [&Feature, &bFunctionBindingVisibleOnWorkerThread]()
	{
		IModularFeatures::Get().RegisterModularFeature(FAngelscriptNativeModuleFunctionBindingBridge::FeatureName(), &Feature);
		bFunctionBindingVisibleOnWorkerThread = FindActorFunctionBinding(WorkerFunctionName) != nullptr;
	});
	Worker.Wait();

	ON_SCOPE_EXIT
	{
		IModularFeatures::Get().UnregisterModularFeature(FAngelscriptNativeModuleFunctionBindingBridge::FeatureName(), &Feature);
	};

	bool bPassed = true;
	bPassed &= Test.TestFalse(TEXT("Worker-thread registration should not mutate ClassFunctionBindings on the worker thread"), bFunctionBindingVisibleOnWorkerThread.Load());
	bPassed &= Test.TestTrue(TEXT("Worker-thread registration should inject on the game thread"), WaitForGameThreadFeatureInjection(WorkerFunctionName));

	const FAngelscriptFunctionBinding* InjectedFunctionBinding = FindActorFunctionBinding(WorkerFunctionName);
	if (!Test.TestNotNull(TEXT("Worker-thread native module function binding feature should inject a ClassFunctionBindings binding"), InjectedFunctionBinding))
	{
		return false;
	}

	bPassed &= Test.TestTrue(TEXT("Worker-thread native module function binding should be bound"), IsFunctionBindingBound(*InjectedFunctionBinding));
	bPassed &= Test.TestTrue(TEXT("Worker-thread native module function binding should use the generic bridge"), InjectedFunctionBinding->bUsesGenericCall);
	bPassed &= Test.TestEqual(TEXT("Worker-thread native module function binding should carry the source binding as user data"), InjectedFunctionBinding->UserData, static_cast<void*>(const_cast<FAngelscriptNativeModuleFunctionBinding*>(&Binding)));
	return bPassed;
}

bool FAngelscriptNativeModuleFunctionBindingRuntimeTests::RunDeferredUnregister(FAutomationTestBase& Test)
{
	if (!EnsureNativeModuleFunctionBindingSubscriptionReady())
	{
		return true;
	}

	FScopedClassFuncMapRestore RestoreActorEntries(AActor::StaticClass());
	const FAngelscriptNativeModuleFunctionBinding Binding = { TestClassName, DeferredUnregisterFunctionName, &NoOpThunk, 0, 0, 0 };
	FTestNativeModuleFunctionBindingFeature Feature(&Binding, 1, TestModuleName, FAngelscriptNativeModuleFunctionBindingBridge::LayoutVersionExpected);

	TFuture<void> Worker = Async(EAsyncExecution::ThreadPool, [&Feature]()
	{
		IModularFeatures::Get().RegisterModularFeature(FAngelscriptNativeModuleFunctionBindingBridge::FeatureName(), &Feature);
		IModularFeatures::Get().UnregisterModularFeature(FAngelscriptNativeModuleFunctionBindingBridge::FeatureName(), &Feature);
	});
	Worker.Wait();

	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);

	return Test.TestNull(
		TEXT("A feature unregistered before deferred injection should not leave a binding behind"),
		FindActorFunctionBinding(DeferredUnregisterFunctionName));
}

bool FAngelscriptNativeModuleFunctionBindingRuntimeTests::RunModuleReload(FAutomationTestBase& Test)
{
	if (!EnsureNativeModuleFunctionBindingSubscriptionReady())
	{
		return true;
	}

	FScopedClassFuncMapRestore RestoreActorEntries(AActor::StaticClass());
	const FAngelscriptNativeModuleFunctionBinding FirstBinding = { TestClassName, ReloadFunctionName, &NoOpThunk, 0, 0, 0 };
	const FAngelscriptNativeModuleFunctionBinding SecondBinding = { TestClassName, ReloadFunctionName, &SumThunk, 2, sizeof(int32), FAngelscriptNativeModuleFunctionBindingBridge::FlagStatic };
	FTestNativeModuleFunctionBindingFeature FirstFeature(&FirstBinding, 1, TestModuleName, FAngelscriptNativeModuleFunctionBindingBridge::LayoutVersionExpected);
	FTestNativeModuleFunctionBindingFeature SecondFeature(&SecondBinding, 1, TestModuleName, FAngelscriptNativeModuleFunctionBindingBridge::LayoutVersionExpected);

	IModularFeatures::Get().RegisterModularFeature(FAngelscriptNativeModuleFunctionBindingBridge::FeatureName(), &FirstFeature);
	const FAngelscriptFunctionBinding* FirstInjectedBinding = FindActorFunctionBinding(ReloadFunctionName);
	if (!Test.TestNotNull(TEXT("The first module load should inject its binding"), FirstInjectedBinding))
	{
		IModularFeatures::Get().UnregisterModularFeature(FAngelscriptNativeModuleFunctionBindingBridge::FeatureName(), &FirstFeature);
		return false;
	}

	const void* FirstUserData = FirstInjectedBinding->UserData;
	IModularFeatures::Get().UnregisterModularFeature(FAngelscriptNativeModuleFunctionBindingBridge::FeatureName(), &FirstFeature);
	if (!Test.TestNull(TEXT("Unloading a target module should remove its binding before reload"), FindActorFunctionBinding(ReloadFunctionName)))
	{
		return false;
	}

	IModularFeatures::Get().RegisterModularFeature(FAngelscriptNativeModuleFunctionBindingBridge::FeatureName(), &SecondFeature);
	ON_SCOPE_EXIT
	{
		IModularFeatures::Get().UnregisterModularFeature(FAngelscriptNativeModuleFunctionBindingBridge::FeatureName(), &SecondFeature);
	};

	const FAngelscriptFunctionBinding* SecondInjectedBinding = FindActorFunctionBinding(ReloadFunctionName);
	if (!Test.TestNotNull(TEXT("The reloaded module should inject a new binding"), SecondInjectedBinding))
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= Test.TestNotEqual(TEXT("Reloaded binding should not retain the unloaded module descriptor"), SecondInjectedBinding->UserData, FirstUserData);
	bPassed &= Test.TestEqual(TEXT("Reloaded binding should point at the new descriptor"), SecondInjectedBinding->UserData, static_cast<void*>(const_cast<FAngelscriptNativeModuleFunctionBinding*>(&SecondBinding)));
	return bPassed;
}

bool FAngelscriptNativeModuleFunctionBindingRuntimeTests::RunGenericHookFrameThunk(FAutomationTestBase& Test)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	FAngelscriptEngineScope Scope(Engine);

	FGenericHookProbeState ProbeState;
	TGuardValue<FGenericHookProbeState*> ProbeStateGuard(GGenericHookProbeState, &ProbeState);

	FAngelscriptNativeModuleFunctionBinding Binding = {
		nullptr,
		TEXT("NativeModuleFunctionBindingGenericHookProbe"),
		&SumThunk,
		2,
		sizeof(int32),
		FAngelscriptNativeModuleFunctionBindingBridge::FlagStatic
	};

	const int32 FunctionId = GAngelscriptNativeModuleFunctionBindingBindGlobalFunctionForTesting(
		Engine,
		"int NativeModuleFunctionBindingGenericHookProbe(int Left, int Right)",
		&Binding);
	if (!Test.TestTrue(TEXT("Native module function binding generic hook should register a global function"), FunctionId >= 0))
	{
		return false;
	}

	FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("NativeModuleFunctionBindingGenericHookFrameThunk"), TEXT(R"(
int Run()
{
	return NativeModuleFunctionBindingGenericHookProbe(17, 25);
}
)"));
	if (!Test.TestTrue(TEXT("Native module function binding generic hook script should compile"), ModuleScope.IsValid()))
	{
		return false;
	}

	FAngelscriptTestExecutor Executor(Test, Engine, ModuleScope.GetModule(), TEXT("int Run()"));
	if (!Test.TestTrue(TEXT("Native module function binding generic hook script should be invokable"), Executor.IsValid()))
	{
		return false;
	}

	const int32 Result = Executor.ExecuteAndGet<int32>(INDEX_NONE);

	bool bPassed = true;
	bPassed &= Test.TestEqual(TEXT("Native module function binding generic hook should return the frame thunk result"), Result, 42);
	bPassed &= Test.TestEqual(TEXT("Native module function binding generic hook should invoke the frame thunk once"), ProbeState.HitCount, 1);
	bPassed &= Test.TestEqual(TEXT("Native module function binding generic hook frame should expose arg count"), ProbeState.ArgCount, 2);
	bPassed &= Test.TestEqual(TEXT("Native module function binding generic hook should pass arg 0 through a frame slot"), ProbeState.Left, 17);
	bPassed &= Test.TestEqual(TEXT("Native module function binding generic hook should pass arg 1 through a frame slot"), ProbeState.Right, 25);
	bPassed &= Test.TestTrue(TEXT("Native module function binding static generic hook should pass null Self"), ProbeState.bSelfWasNull);
	bPassed &= Test.TestTrue(TEXT("Native module function binding generic hook should provide valid frame arg slots"), ProbeState.bArgSlotsWereValid);
	bPassed &= Test.TestTrue(TEXT("Native module function binding generic hook should provide a return slot"), ProbeState.bReturnWasValid);
	bPassed &= Test.TestTrue(TEXT("Native module function binding static generic hook should leave ScriptSelf null"), ProbeState.bScriptSelfWasNull);
	bPassed &= Test.TestTrue(TEXT("Native module function binding generic hook should leave WorldContext null until a policy exists"), ProbeState.bWorldContextWasNull);
	bPassed &= Test.TestEqual(TEXT("Native module function binding generic hook should copy binding flags into the frame"), ProbeState.FrameFlags, FAngelscriptNativeModuleFunctionBindingBridge::FlagStatic);
	return bPassed;
}

bool FAngelscriptNativeModuleFunctionBindingRuntimeTests::RunSameModuleMultipleFeature(FAutomationTestBase& Test)
{
	if (!EnsureNativeModuleFunctionBindingSubscriptionReady())
	{
		return true;
	}

	FScopedClassFuncMapRestore RestoreActorEntries(AActor::StaticClass());
	const FAngelscriptNativeModuleFunctionBinding BindingA = { TestClassName, MultiShardFunctionNameA, &NoOpThunk, 0, 0, 0 };
	const FAngelscriptNativeModuleFunctionBinding BindingB = { TestClassName, MultiShardFunctionNameB, &NoOpThunk, 0, 0, 0 };
	FTestNativeModuleFunctionBindingFeature FeatureA(&BindingA, 1, TestModuleName, FAngelscriptNativeModuleFunctionBindingBridge::LayoutVersionExpected);
	FTestNativeModuleFunctionBindingFeature FeatureB(&BindingB, 1, TestModuleName, FAngelscriptNativeModuleFunctionBindingBridge::LayoutVersionExpected);

	IModularFeatures::Get().RegisterModularFeature(FAngelscriptNativeModuleFunctionBindingBridge::FeatureName(), &FeatureA);
	IModularFeatures::Get().RegisterModularFeature(FAngelscriptNativeModuleFunctionBindingBridge::FeatureName(), &FeatureB);
	ON_SCOPE_EXIT
	{
		IModularFeatures::Get().UnregisterModularFeature(FAngelscriptNativeModuleFunctionBindingBridge::FeatureName(), &FeatureB);
		IModularFeatures::Get().UnregisterModularFeature(FAngelscriptNativeModuleFunctionBindingBridge::FeatureName(), &FeatureA);
	};

	const FAngelscriptFunctionBinding* InjectedFunctionBindingA = FindActorFunctionBinding(MultiShardFunctionNameA);
	const FAngelscriptFunctionBinding* InjectedFunctionBindingB = FindActorFunctionBinding(MultiShardFunctionNameB);
	if (!Test.TestNotNull(TEXT("First same-module feature should inject a function binding"), InjectedFunctionBindingA) ||
		!Test.TestNotNull(TEXT("Second same-module feature should inject a function binding"), InjectedFunctionBindingB))
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= Test.TestTrue(TEXT("First same-module feature binding should be bound"), IsFunctionBindingBound(*InjectedFunctionBindingA));
	bPassed &= Test.TestTrue(TEXT("Second same-module feature binding should be bound"), IsFunctionBindingBound(*InjectedFunctionBindingB));
	bPassed &= Test.TestEqual(TEXT("First same-module feature should keep its own user data"), InjectedFunctionBindingA->UserData, static_cast<void*>(const_cast<FAngelscriptNativeModuleFunctionBinding*>(&BindingA)));
	bPassed &= Test.TestEqual(TEXT("Second same-module feature should keep its own user data"), InjectedFunctionBindingB->UserData, static_cast<void*>(const_cast<FAngelscriptNativeModuleFunctionBinding*>(&BindingB)));
	return bPassed;
}

bool FAngelscriptNativeModuleFunctionBindingRuntimeTests::RunLayoutMismatch(FAutomationTestBase& Test)
{
	if (!EnsureNativeModuleFunctionBindingSubscriptionReady())
	{
		return true;
	}

	FScopedClassFuncMapRestore RestoreActorEntries(AActor::StaticClass());
	const FAngelscriptNativeModuleFunctionBinding Binding = { TestClassName, MismatchFunctionName, &NoOpThunk, 0, 0, 0 };
	FTestNativeModuleFunctionBindingFeature Feature(&Binding, 1, TestModuleName, 0xDEADBEEFu);

	Test.AddExpectedErrorPlain(TEXT("Native module function binding feature skipped because layout version"), EAutomationExpectedErrorFlags::Contains, 1);
	RegisterAndUnregisterFeature(Feature);

	return Test.TestNull(TEXT("Layout-mismatched native module function binding feature should not inject a binding"), FindActorFunctionBinding(MismatchFunctionName));
}

bool FAngelscriptNativeModuleFunctionBindingRuntimeTests::RunMalformedFeature(FAutomationTestBase& Test)
{
	if (!EnsureNativeModuleFunctionBindingSubscriptionReady())
	{
		return true;
	}

	FScopedClassFuncMapRestore RestoreActorEntries(AActor::StaticClass());
	const FAngelscriptNativeModuleFunctionBinding Binding = { TestClassName, MalformedFunctionName, &NoOpThunk, 0, 0, 0 };

	FTestNativeModuleFunctionBindingFeature NegativeCountFeature(&Binding, -1, TestModuleName, FAngelscriptNativeModuleFunctionBindingBridge::LayoutVersionExpected);
	FTestNativeModuleFunctionBindingFeature NullBindingsFeature(nullptr, 1, TestModuleName, FAngelscriptNativeModuleFunctionBindingBridge::LayoutVersionExpected);
	FTestNativeModuleFunctionBindingFeature NullModuleFeature(&Binding, 1, nullptr, FAngelscriptNativeModuleFunctionBindingBridge::LayoutVersionExpected);

	Test.AddExpectedErrorPlain(TEXT("Native module function binding feature skipped because its payload is malformed."), EAutomationExpectedErrorFlags::Contains, 3);
	RegisterAndUnregisterFeature(NegativeCountFeature);
	RegisterAndUnregisterFeature(NullBindingsFeature);
	RegisterAndUnregisterFeature(NullModuleFeature);

	return Test.TestNull(TEXT("Malformed native module function binding features should not inject bindings"), FindActorFunctionBinding(MalformedFunctionName));
}

#endif
