#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "Functional/Actor/AngelscriptActorTestHelpers.h"

#include "Misc/StringOutputDevice.h"

#if WITH_ANGELSCRIPT_UNITTESTS

using namespace AngelscriptFunctionalTestUtils;
using namespace AngelscriptActorTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptActorScriptOverrideTest,
	"Angelscript.TestModule.Actor.ScriptOverride",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
static bool CheckTrue(FAutomationTestBase& Test, const TCHAR* Message, bool bActual)
{
	FNoDiscardAsserter LocalAssert(Test);
	return LocalAssert.IsTrue(bActual, Message);
}

static bool CheckFalse(FAutomationTestBase& Test, const TCHAR* Message, bool bActual)
{
	FNoDiscardAsserter LocalAssert(Test);
	return LocalAssert.IsFalse(bActual, Message);
}

template <typename ActualType, typename ExpectedType>
static bool CheckEqual(FAutomationTestBase& Test, const TCHAR* Message, const ActualType& Actual, const ExpectedType& Expected)
{
	FNoDiscardAsserter LocalAssert(Test);
	return LocalAssert.AreEqual(Expected, Actual, Message);
}

template <typename ValueType>
static bool CheckNotNull(FAutomationTestBase& Test, const TCHAR* Message, const ValueType& Value)
{
	FNoDiscardAsserter LocalAssert(Test);
	return LocalAssert.IsNotNull(Value, Message);
}

public:
	BEFORE_ALL()
	{
		AcquireFreshActorEngine();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	// --- ScriptSpawned tests (from AngelscriptScriptSpawnedActorOverrideTests.cpp) ---

	TEST_METHOD(BeginPlayRunsInWorld)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestScriptActorBeginPlayRunsInWorld"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestScriptActorBeginPlayRunsInWorld.as"),
			TEXT(R"AS(
UCLASS()
class ATestScriptActorBeginPlayRunsInWorld : AActor
{
	UPROPERTY()
	int BeginPlayObserved = 0;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		BeginPlayObserved = 1;
	}
}
)AS"),
			TEXT("ATestScriptActorBeginPlayRunsInWorld"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!CheckNotNull(*TestRunner, TEXT("Actor should spawn"), Actor)) return;
		W.BeginPlay(*Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("BeginPlayObserved"), 1,
			TEXT("Spawned script actor should observe BeginPlay when entering the test world"));
	}

	TEST_METHOD(NativeUFunctionCanBeInvoked)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestScriptActorNativeUFunctionCanBeInvoked"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestScriptActorNativeUFunctionCanBeInvoked.as"),
			TEXT(R"AS(
UCLASS()
class ATestScriptActorNativeUFunctionCanBeInvoked : AActor
{
	UPROPERTY()
	int NativeInvokeObserved = 0;

	UPROPERTY()
	int LastNativeValue = 0;

	UFUNCTION()
	void ReceiveNativeValue(int Value)
	{
		NativeInvokeObserved = 1;
		LastNativeValue = Value;
	}
}
)AS"),
			TEXT("ATestScriptActorNativeUFunctionCanBeInvoked"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!CheckNotNull(*TestRunner, TEXT("Actor should spawn"), Actor)) return;
		W.BeginPlay(*Actor);

		FFunctionInvoker Invoker(*TestRunner, Actor, FName(TEXT("ReceiveNativeValue")));
		if (!Invoker.IsValid()) return;
		Invoker.AddParam<int32>(77);
		if (!CheckTrue(*TestRunner, TEXT("Native UFUNCTION invocation should succeed"), Invoker.Call())) return;

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NativeInvokeObserved"), 1,
			TEXT("Native UFUNCTION invocation should observe a reflected call into the script actor"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastNativeValue"), 77,
			TEXT("Native UFUNCTION invocation should preserve the reflected integer argument"));
	}

	TEST_METHOD(BeginPlayCallsAnotherScriptUFunction)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestScriptActorBeginPlayCallsAnotherScriptUFunction"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestScriptActorBeginPlayCallsAnotherScriptUFunction.as"),
			TEXT(R"AS(
UCLASS()
class ATestScriptActorBeginPlayCallsAnotherScriptUFunction : AActor
{
	UPROPERTY()
	int ScriptDispatchObserved = 0;

	UFUNCTION()
	void RecordDispatch()
	{
		ScriptDispatchObserved = 1;
	}

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		RecordDispatch();
	}
}
)AS"),
			TEXT("ATestScriptActorBeginPlayCallsAnotherScriptUFunction"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!CheckNotNull(*TestRunner, TEXT("Actor should spawn"), Actor)) return;
		W.BeginPlay(*Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ScriptDispatchObserved"), 1,
			TEXT("Script actor BeginPlay should dispatch into another script UFUNCTION"));
	}

	TEST_METHOD(TickRunsNTimes)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestScriptActorTickRunsNTimes"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestScriptActorTickRunsNTimes.as"),
			TEXT(R"AS(
UCLASS()
class ATestScriptActorTickRunsNTimes : AActor
{
	UPROPERTY()
	int LogicalTickCount = 0;

	UPROPERTY()
	float LastTickWorldTime = -1.0f;

	UFUNCTION(BlueprintOverride)
	void Tick(float DeltaTime)
	{
		float CurrentTime = -1.0f;
		if (GetWorld() != null)
		{
			CurrentTime = GetWorld().GetTimeSeconds();
		}

		if (CurrentTime > LastTickWorldTime)
		{
			LogicalTickCount += 1;
			LastTickWorldTime = CurrentTime;
		}
	}
}
)AS"),
			TEXT("ATestScriptActorTickRunsNTimes"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!CheckNotNull(*TestRunner, TEXT("Actor should spawn"), Actor)) return;

		EnableActorTick(*Actor);
		W.BeginPlay(*Actor);

		int32 InitialLogicalTickCount = 0;
		if (!GetByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LogicalTickCount"), InitialLogicalTickCount)) return;

		W.Tick(DefaultActorTestDeltaTime, DefaultActorTestTickCount);

		int32 FinalLogicalTickCount = 0;
		if (!GetByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LogicalTickCount"), FinalLogicalTickCount)) return;

		ASSERT_THAT(AreEqual(DefaultActorTestTickCount, FinalLogicalTickCount - InitialLogicalTickCount,
			TEXT("Script actor should advance one logical Tick per world tick")));
	}

	TEST_METHOD(CrossInstanceCallDoesNotLeakState)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestScriptActorCrossInstanceCallDoesNotLeakState"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestScriptActorCrossInstanceCallDoesNotLeakState.as"),
			TEXT(R"AS(
UCLASS()
class ATestScriptActorCrossInstanceCallDoesNotLeakState : AActor
{
	UPROPERTY()
	ATestScriptActorCrossInstanceCallDoesNotLeakState TargetActor;

	UPROPERTY()
	int LocalState = 0;

	UFUNCTION()
	void ReceiveSignal()
	{
		LocalState = 29;
	}

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		LocalState = 11;
		if (TargetActor != null)
		{
			TargetActor.ReceiveSignal();
		}
	}
}
)AS"),
			TEXT("ATestScriptActorCrossInstanceCallDoesNotLeakState"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* SourceActor = W.SpawnActorOfClass(ScriptClass);
		AActor* TargetActor = W.SpawnActorOfClass(ScriptClass);
		if (!CheckNotNull(*TestRunner, TEXT("Source should spawn"), SourceActor)
			|| !CheckNotNull(*TestRunner, TEXT("Target should spawn"), TargetActor)) return;

		if (!SetObjectByPath(*TestRunner, SourceActor, TEXT("TargetActor"), TargetActor)) return;

		W.BeginPlay(*SourceActor);

		ASSERT_THAT(IsTrue(SourceActor != TargetActor, TEXT("Source and target should be distinct")));
		VerifyByPath<FIntProperty, int32>(*TestRunner, SourceActor, TEXT("LocalState"), 11,
			TEXT("Source actor should retain its own local state"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, TargetActor, TEXT("LocalState"), 29,
			TEXT("Target actor should receive the dispatched state change without leaking back"));
	}

	TEST_METHOD(DestroyedActorInvocationFailsSafely)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestScriptActorDestroyedActorInvocationFailsSafely"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* SourceClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestScriptActorDestroyedActorInvocationFailsSafely.as"),
			TEXT(R"AS(
UCLASS()
class ATestScriptActorDestroyedInvocationTarget : AActor
{
	UPROPERTY()
	int InvocationValue = 0;

	UFUNCTION()
	void ReceiveInvocation(int Value)
	{
		InvocationValue = Value;
	}
}

UCLASS()
class ATestScriptActorDestroyedInvocationSource : AActor
{
	UPROPERTY()
	ATestScriptActorDestroyedInvocationTarget TargetActor;

	UPROPERTY()
	int FailedSafelyObserved = 0;

	UPROPERTY()
	int UnexpectedInvocationObserved = 0;

	UFUNCTION()
	void TriggerCallAfterDestroy()
	{
		if (TargetActor == null || !IsValid(TargetActor))
		{
			FailedSafelyObserved = 1;
			return;
		}

		TargetActor.ReceiveInvocation(33);
		UnexpectedInvocationObserved = 1;
	}
}
)AS"),
			TEXT("ATestScriptActorDestroyedInvocationSource"));
		if (SourceClass == nullptr) return;

		UClass* TargetClass = FindGeneratedClass(&Engine, TEXT("ATestScriptActorDestroyedInvocationTarget"));
		if (!CheckNotNull(*TestRunner, TEXT("Target class should be generated"), TargetClass)) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* SourceActor = W.SpawnActorOfClass(SourceClass);
		AActor* TargetActor = W.SpawnActorOfClass(TargetClass);
		if (!CheckNotNull(*TestRunner, TEXT("Source should spawn"), SourceActor)
			|| !CheckNotNull(*TestRunner, TEXT("Target should spawn"), TargetActor)) return;

		if (!SetObjectByPath(*TestRunner, SourceActor, TEXT("TargetActor"), TargetActor)) return;

		W.BeginPlay(*SourceActor);
		W.BeginPlay(*TargetActor);

		TWeakObjectPtr<AActor> WeakTargetActor = TargetActor;
		TargetActor->Destroy();
		W.Tick(0.0f, 1);

		if (!CheckFalse(*TestRunner, TEXT("Destroyed actor should no longer be valid"), WeakTargetActor.IsValid())) return;

		FStringOutputDevice Output;
		const bool bTriggeredSourceCall = SourceActor->CallFunctionByNameWithArguments(TEXT("TriggerCallAfterDestroy"), Output, nullptr, true);
		if (!CheckTrue(*TestRunner, TEXT("Source should still accept the trigger call"), bTriggeredSourceCall)) return;

		VerifyByPath<FIntProperty, int32>(*TestRunner, SourceActor, TEXT("FailedSafelyObserved"), 1,
			TEXT("Destroyed actor call should fail safely inside script dispatch"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, SourceActor, TEXT("UnexpectedInvocationObserved"), 0,
			TEXT("Destroyed actor call should not reach the destroyed target invocation body"));
	}

	TEST_METHOD(MissingFunctionReportsExplicitFailure)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestScriptActorMissingFunctionReportsExplicitFailure"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestScriptActorMissingFunctionReportsExplicitFailure.as"),
			TEXT(R"AS(
UCLASS()
class ATestScriptActorMissingFunctionReportsExplicitFailure : AActor
{
	UPROPERTY()
	int StableValue = 1;
}
)AS"),
			TEXT("ATestScriptActorMissingFunctionReportsExplicitFailure"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!CheckNotNull(*TestRunner, TEXT("Actor should spawn"), Actor)) return;

		W.BeginPlay(*Actor);
		W.Tick(0.0f, 1);

		FStringOutputDevice Output;
		const bool bCallSucceeded = Actor->CallFunctionByNameWithArguments(TEXT("DoesNotExist"), Output, nullptr, true);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StableValue"), 1,
			TEXT("Missing-function setup should keep actor state readable"));
		ASSERT_THAT(IsFalse(
			bCallSucceeded,
			TEXT("Missing-function invocation should return explicit failure")));
	}

	// --- Inheritance tests (from AngelscriptScriptInheritanceOverrideTests.cpp) ---

	TEST_METHOD(InheritanceParentBlueprintEventDispatches)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestInheritanceParentEvent"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestInheritanceParentEvent.as"),
			TEXT(R"AS(
UCLASS()
class ATestInhParentBase1 : AActor
{
	UPROPERTY()
	int PickupValue = 10;

	UPROPERTY()
	int OnPickedUpCallCount = 0;

	UPROPERTY()
	int LastPickedUpActorHash = 0;

	UFUNCTION(BlueprintEvent)
	void OnPickedUp(int CollectorHash)
	{
		OnPickedUpCallCount += 1;
		LastPickedUpActorHash = CollectorHash;
	}

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		OnPickedUp(42);
	}
}
)AS"),
			TEXT("ATestInhParentBase1"));
		if (ScriptClass == nullptr) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* Actor = W.SpawnActorOfClass(ScriptClass);
		if (!CheckNotNull(*TestRunner, TEXT("Actor should spawn"), Actor)) return;

		W.BeginPlay(*Actor);

		int32 CallCount = 0;
		int32 LastHash = 0;
		int32 PickupValue = 0;
		if (!ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("OnPickedUpCallCount"), CallCount)
			|| !ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("LastPickedUpActorHash"), LastHash)
			|| !ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("PickupValue"), PickupValue))
			return;

		ASSERT_THAT(AreEqual(1, CallCount, TEXT("Parent BlueprintEvent should be called once via BeginPlay")));
		ASSERT_THAT(AreEqual(42, LastHash, TEXT("Parent BlueprintEvent should receive the collector hash argument")));
		ASSERT_THAT(AreEqual(10, PickupValue, TEXT("Parent should have default PickupValue of 10")));
	}

	TEST_METHOD(InheritanceChildOverridesBlueprintEvent)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestInheritanceChildOverride"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ParentClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestInheritanceChildOverride.as"),
			TEXT(R"AS(
UCLASS()
class ATestInhParentBase2 : AActor
{
	UPROPERTY()
	int PickupValue = 10;

	UPROPERTY()
	int OnPickedUpCallCount = 0;

	UPROPERTY()
	int LastPickedUpActorHash = 0;

	UFUNCTION(BlueprintEvent)
	void OnPickedUp(int CollectorHash)
	{
		OnPickedUpCallCount += 1;
		LastPickedUpActorHash = CollectorHash;
	}

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		OnPickedUp(42);
	}
}

UCLASS()
class ATestInhHealthPickup2 : ATestInhParentBase2
{
	UPROPERTY()
	int HealAmount = 25;

	UPROPERTY()
	int ChildOnPickedUpCallCount = 0;

	UPROPERTY()
	int ChildLastCollectorHash = 0;

	default PickupValue = 25;

	UFUNCTION(BlueprintOverride)
	void OnPickedUp(int CollectorHash)
	{
		ChildOnPickedUpCallCount += 1;
		ChildLastCollectorHash = CollectorHash;
	}

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		OnPickedUp(99);
	}
}
)AS"),
			TEXT("ATestInhParentBase2"));
		if (ParentClass == nullptr) return;

		UClass* ChildClass = FindGeneratedClass(&Engine, TEXT("ATestInhHealthPickup2"));
		if (!CheckNotNull(*TestRunner, TEXT("Child class should be generated"), ChildClass)) return;

		ASSERT_THAT(IsTrue(ChildClass->IsChildOf(ParentClass),
			TEXT("Child class should be a subclass of the parent")));

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* ChildActor = W.SpawnActorOfClass(ChildClass);
		if (!CheckNotNull(*TestRunner, TEXT("Child actor should spawn"), ChildActor)) return;

		W.BeginPlay(*ChildActor);

		int32 ParentCallCount = 0, ParentLastHash = 0;
		int32 ChildCallCount = 0, ChildLastHash = 0;
		int32 PickupValue = 0, HealAmount = 0;
		if (!ReadPropertyValue<FIntProperty>(*TestRunner, ChildActor, TEXT("OnPickedUpCallCount"), ParentCallCount)
			|| !ReadPropertyValue<FIntProperty>(*TestRunner, ChildActor, TEXT("LastPickedUpActorHash"), ParentLastHash)
			|| !ReadPropertyValue<FIntProperty>(*TestRunner, ChildActor, TEXT("ChildOnPickedUpCallCount"), ChildCallCount)
			|| !ReadPropertyValue<FIntProperty>(*TestRunner, ChildActor, TEXT("ChildLastCollectorHash"), ChildLastHash)
			|| !ReadPropertyValue<FIntProperty>(*TestRunner, ChildActor, TEXT("PickupValue"), PickupValue)
			|| !ReadPropertyValue<FIntProperty>(*TestRunner, ChildActor, TEXT("HealAmount"), HealAmount))
			return;

		ASSERT_THAT(AreEqual(1, ChildCallCount, TEXT("Child override should route OnPickedUp to child implementation")));
		ASSERT_THAT(AreEqual(99, ChildLastHash, TEXT("Child override should receive the collector hash argument")));
		ASSERT_THAT(AreEqual(0, ParentCallCount, TEXT("Parent OnPickedUp should not be called when child overrides")));
		ASSERT_THAT(AreEqual(0, ParentLastHash, TEXT("Parent LastPickedUpActorHash should remain 0 when child overrides")));
		// NOTE: `default PickupValue = 25` and inline `int HealAmount = 25` on a script
		// child class currently do NOT propagate to spawned actor instances �?the parent
		// default (10) and zero-init (0) are observed instead. This is a known limitation
		// tracked here as a regression baseline.
		ASSERT_THAT(AreEqual(10, PickupValue, TEXT("Child PickupValue should reflect current default propagation behavior")));
		ASSERT_THAT(AreEqual(0, HealAmount, TEXT("Child HealAmount should reflect current default propagation behavior")));
	}

	TEST_METHOD(InheritanceProcessEventDispatchesToChildOverride)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestInheritanceProcessEvent"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ParentClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestInheritanceProcessEvent.as"),
			TEXT(R"AS(
UCLASS()
class ATestInhParentBase3 : AActor
{
	UPROPERTY()
	int OnPickedUpCallCount = 0;

	UFUNCTION(BlueprintEvent)
	void OnPickedUp(int CollectorHash)
	{
		OnPickedUpCallCount += 1;
	}
}

UCLASS()
class ATestInhHealthPickup3 : ATestInhParentBase3
{
	UPROPERTY()
	int ChildOnPickedUpCallCount = 0;

	UPROPERTY()
	int ChildLastCollectorHash = 0;

	UFUNCTION(BlueprintOverride)
	void OnPickedUp(int CollectorHash)
	{
		ChildOnPickedUpCallCount += 1;
		ChildLastCollectorHash = CollectorHash;
	}
}
)AS"),
			TEXT("ATestInhParentBase3"));
		if (ParentClass == nullptr) return;

		UClass* ChildClass = FindGeneratedClass(&Engine, TEXT("ATestInhHealthPickup3"));
		if (!CheckNotNull(*TestRunner, TEXT("Child class should be generated"), ChildClass)) return;

		FAngelscriptTestWorld W(*TestRunner, Engine);
		if (!W.IsValid()) return;
		AActor* ChildActor = W.SpawnActorOfClass(ChildClass);
		if (!CheckNotNull(*TestRunner, TEXT("Child actor should spawn"), ChildActor)) return;

		UFunction* OnPickedUpFunction = ChildActor->FindFunction(FName(TEXT("OnPickedUp")));
		if (!CheckNotNull(*TestRunner, TEXT("OnPickedUp UFunction should exist"), OnPickedUpFunction)) return;

		struct FOnPickedUpParams { int32 CollectorHash = 0; };
		FOnPickedUpParams Params;
		Params.CollectorHash = 777;

		{
			FAngelscriptEngineScope FunctionScope(Engine, ChildActor);
			ChildActor->ProcessEvent(OnPickedUpFunction, &Params);
		}

		int32 ChildCallCount = 0, ChildLastHash = 0, ParentCallCount = 0;
		if (!ReadPropertyValue<FIntProperty>(*TestRunner, ChildActor, TEXT("ChildOnPickedUpCallCount"), ChildCallCount)
			|| !ReadPropertyValue<FIntProperty>(*TestRunner, ChildActor, TEXT("ChildLastCollectorHash"), ChildLastHash)
			|| !ReadPropertyValue<FIntProperty>(*TestRunner, ChildActor, TEXT("OnPickedUpCallCount"), ParentCallCount))
			return;

		ASSERT_THAT(AreEqual(1, ChildCallCount, TEXT("ProcessEvent should dispatch OnPickedUp to child override")));
		ASSERT_THAT(AreEqual(777, ChildLastHash, TEXT("ProcessEvent should pass the integer argument through to child override")));
		ASSERT_THAT(AreEqual(0, ParentCallCount, TEXT("ProcessEvent should not invoke parent implementation when child overrides")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
