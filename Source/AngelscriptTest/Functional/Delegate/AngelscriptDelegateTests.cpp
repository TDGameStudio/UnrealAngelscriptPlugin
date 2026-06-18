#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptNativeScriptTestObject.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"

#include "Components/ActorTestSpawner.h"
#include "Misc/ScopeExit.h"
#include "UObject/ScriptDelegates.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

namespace
{
	struct FTestCaseIntStringParams
	{
		int32 Value = 0;
		FString Label;
	};
}

// ============================================================================
// Unicast Delegate Tests
// ============================================================================

TEST_CLASS_WITH_FLAGS(FAngelscriptDelegateUnicastTest,
	"Angelscript.TestModule.Delegate.Unicast",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(ExecuteWithBoundNativeCallback)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestDelegateUnicast"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestDelegateUnicast.as"),
			TEXT(R"AS(
delegate void FOnHealthChanged(int32 NewHealth, const FString& Label);

UCLASS()
class ATestDelegateUnicast : AActor
{
	UPROPERTY()
	FOnHealthChanged OnHealthChanged;

	UFUNCTION()
	void TriggerHealthChanged(int32 NewHealth, const FString& Label)
	{
		if (OnHealthChanged.IsBound())
		{
			OnHealthChanged.Execute(NewHealth, Label);
		}
	}
}
)AS"),
			TEXT("ATestDelegateUnicast"));
		if (ScriptClass == nullptr) return;

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;
		BeginPlayActor(Engine, *Actor);

		UAngelscriptNativeScriptTestObject* NativeReceiver = NewObject<UAngelscriptNativeScriptTestObject>(GetTransientPackage());
		if (!TestRunner->TestNotNull(TEXT("Native receiver should be created"), NativeReceiver)) return;
		NativeReceiver->NameCounts.Reset();

		FDelegateProperty* DelegateProperty = FindFProperty<FDelegateProperty>(Actor->GetClass(), TEXT("OnHealthChanged"));
		if (!TestRunner->TestNotNull(TEXT("Delegate property should exist"), DelegateProperty)) return;

		FScriptDelegate BoundDelegate;
		BoundDelegate.BindUFunction(NativeReceiver, TEXT("SetIntStringFromDelegate"));
		*DelegateProperty->ContainerPtrToValuePtr<FScriptDelegate>(Actor) = BoundDelegate;

		UFunction* TriggerFunction = FindGeneratedFunction(ScriptClass, TEXT("TriggerHealthChanged"));
		if (!TestRunner->TestNotNull(TEXT("Trigger function should exist"), TriggerFunction)) return;

		FTestCaseIntStringParams Params;
		Params.Value = 77;
		Params.Label = TEXT("Unicast");
		{
			FAngelscriptEngineScope ExecutionScope(Engine, Actor);
			Actor->ProcessEvent(TriggerFunction, &Params);
		}

		TestRunner->TestEqual(TEXT("Unicast Execute should invoke bound C++ callback with correct parameters"),
			NativeReceiver->NameCounts.FindRef(TEXT("Unicast")), 77);
	}

	TEST_METHOD(IsBoundReturnsFalseWhenUnbound)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestDelegateIsBound"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestDelegateIsBound.as"),
			TEXT(R"AS(
delegate void FSimpleNotify();

UCLASS()
class ATestDelegateIsBound : AActor
{
	UPROPERTY()
	FSimpleNotify OnNotify;

	UFUNCTION()
	int RunIsBoundTest()
	{
		if (OnNotify.IsBound())
			return 10;
		return 1;
	}
}
)AS"),
			TEXT("ATestDelegateIsBound"));
		if (ScriptClass == nullptr) return;

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;
		BeginPlayActor(Engine, *Actor);

		FFunctionInvoker Invoker(*TestRunner, Actor, FName(TEXT("RunIsBoundTest")));
		if (!Invoker.IsValid()) return;
		TestRunner->TestEqual(TEXT("IsBound should return false when delegate has no binding"),
			Invoker.CallAndReturn<int32>(INDEX_NONE), 1);
	}

	TEST_METHOD(ClearRemovesBinding)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestDelegateClear"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestDelegateClear.as"),
			TEXT(R"AS(
delegate void FSimpleNotify();

UCLASS()
class ATestDelegateClear : AActor
{
	UPROPERTY()
	FSimpleNotify OnNotify;

	UPROPERTY()
	int CallCount = 0;

	UFUNCTION()
	void HandleNotify()
	{
		CallCount += 1;
	}

	UFUNCTION()
	int RunClearTest()
	{
		OnNotify.BindUFunction(this, n"HandleNotify");
		if (!OnNotify.IsBound())
			return 10;

		OnNotify.Execute();
		if (CallCount != 1)
			return 20;

		OnNotify.Clear();
		if (OnNotify.IsBound())
			return 30;

		return 1;
	}
}
)AS"),
			TEXT("ATestDelegateClear"));
		if (ScriptClass == nullptr) return;

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;
		BeginPlayActor(Engine, *Actor);

		FFunctionInvoker Invoker(*TestRunner, Actor, FName(TEXT("RunClearTest")));
		if (!Invoker.IsValid()) return;
		TestRunner->TestEqual(TEXT("Clear should unbind the delegate after a successful Execute"),
			Invoker.CallAndReturn<int32>(INDEX_NONE), 1);
	}

	TEST_METHOD(GetUObjectReturnsTarget)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestDelegateGetUObject"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestDelegateGetUObject.as"),
			TEXT(R"AS(
delegate void FSimpleNotify();

UCLASS()
class ATestDelegateGetUObject : AActor
{
	UPROPERTY()
	FSimpleNotify OnNotify;

	UFUNCTION()
	void HandleNotify() {}

	UFUNCTION()
	int RunGetUObjectTest()
	{
		if (OnNotify.GetUObject() != nullptr)
			return 10;

		OnNotify.BindUFunction(this, n"HandleNotify");
		UObject Target = OnNotify.GetUObject();
		if (Target == nullptr)
			return 20;
		if (Target != this)
			return 30;

		return 1;
	}
}
)AS"),
			TEXT("ATestDelegateGetUObject"));
		if (ScriptClass == nullptr) return;

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;
		BeginPlayActor(Engine, *Actor);

		FFunctionInvoker Invoker(*TestRunner, Actor, FName(TEXT("RunGetUObjectTest")));
		if (!Invoker.IsValid()) return;
		TestRunner->TestEqual(TEXT("GetUObject should return null when unbound and target when bound"),
			Invoker.CallAndReturn<int32>(INDEX_NONE), 1);
	}

	TEST_METHOD(SignatureMismatchDoesNotInvoke)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestDelegateUnicastSigMismatch"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestDelegateUnicastSigMismatch.as"),
			TEXT(R"AS(
delegate void FOnHealthChanged(int32 NewHealth, const FString& Label);

UCLASS()
class ATestDelegateUnicastSigMismatch : AActor
{
	UPROPERTY()
	FOnHealthChanged OnHealthChanged;

	UFUNCTION()
	void TriggerHealthChanged(int32 NewHealth, const FString& Label)
	{
		if (OnHealthChanged.IsBound())
		{
			OnHealthChanged.Execute(NewHealth, Label);
		}
	}
}
)AS"),
			TEXT("ATestDelegateUnicastSigMismatch"));
		if (ScriptClass == nullptr) return;

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;
		BeginPlayActor(Engine, *Actor);

		UAngelscriptNativeScriptTestObject* NativeReceiver = NewObject<UAngelscriptNativeScriptTestObject>(GetTransientPackage());
		if (!TestRunner->TestNotNull(TEXT("Native receiver should be created"), NativeReceiver)) return;
		NativeReceiver->bNativeFlag = false;

		FDelegateProperty* DelegateProperty = FindFProperty<FDelegateProperty>(Actor->GetClass(), TEXT("OnHealthChanged"));
		if (!TestRunner->TestNotNull(TEXT("Delegate property should exist"), DelegateProperty)) return;

		FScriptDelegate BoundDelegate;
		BoundDelegate.BindUFunction(NativeReceiver, TEXT("MarkNativeFlagFromDelegate"));
		*DelegateProperty->ContainerPtrToValuePtr<FScriptDelegate>(Actor) = BoundDelegate;

		UFunction* TriggerFunction = FindGeneratedFunction(ScriptClass, TEXT("TriggerHealthChanged"));
		if (!TestRunner->TestNotNull(TEXT("Trigger function should exist"), TriggerFunction)) return;

		TestRunner->AddExpectedError(TEXT("Signature mismatch"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("Angelscript"), EAutomationExpectedErrorFlags::Contains, 0);

		FTestCaseIntStringParams Params;
		Params.Value = 91;
		Params.Label = TEXT("UnicastMismatch");
		{
			FAngelscriptEngineScope ExecutionScope(Engine, Actor);
			Actor->ProcessEvent(TriggerFunction, &Params);
		}

		TestRunner->TestFalse(TEXT("Signature-mismatched unicast should not invoke zero-arg native receiver"),
			NativeReceiver->bNativeFlag);
	}
};

// ============================================================================
// Multicast Delegate Tests
// ============================================================================

TEST_CLASS_WITH_FLAGS(FAngelscriptDelegateMulticastTest,
	"Angelscript.TestModule.Delegate.Multicast",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(BroadcastInvokesScriptHandler)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestDelegateMulticast"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestDelegateMulticast.as"),
			TEXT(R"AS(
event void FOnDamaged(int32 NewHealth, const FString& Label);

UCLASS()
class ATestDelegateMulticast : AActor
{
	UPROPERTY()
	FOnDamaged OnDamaged;

	UPROPERTY()
	int EventTriggerCount = 0;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		OnDamaged.AddUFunction(this, n"HandleDamaged");
	}

	UFUNCTION()
	void HandleDamaged(int32 NewHealth, const FString& Label)
	{
		EventTriggerCount += 1;
	}
}
)AS"),
			TEXT("ATestDelegateMulticast"));
		if (ScriptClass == nullptr) return;

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;
		BeginPlayActor(Engine, *Actor);

		FMulticastInlineDelegateProperty* MulticastProperty = FindFProperty<FMulticastInlineDelegateProperty>(Actor->GetClass(), TEXT("OnDamaged"));
		if (!TestRunner->TestNotNull(TEXT("Multicast delegate property should exist"), MulticastProperty)) return;

		FMulticastScriptDelegate* MulticastDelegate = MulticastProperty->ContainerPtrToValuePtr<FMulticastScriptDelegate>(Actor);
		if (!TestRunner->TestNotNull(TEXT("Multicast delegate storage should exist"), MulticastDelegate)) return;

		FTestCaseIntStringParams Params;
		Params.Value = 33;
		Params.Label = TEXT("Multicast");
		{
			FAngelscriptEngineScope ExecutionScope(Engine, Actor);
		MulticastDelegate->ProcessDelegate<UObject>(&Params);
		}

		int32 EventTriggerCount = 0;
		if (!GetByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("EventTriggerCount"), EventTriggerCount)) return;
		TestRunner->TestTrue(TEXT("Multicast Broadcast from C++ should invoke the script handler"),
			EventTriggerCount > 0);
	}

	TEST_METHOD(AddUFunctionAndBroadcastFromScript)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestDelegateMulticastScript"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestDelegateMulticastScript.as"),
			TEXT(R"AS(
event void FOnScoreChanged(int32 Score);

UCLASS()
class ATestDelegateMulticastScript : AActor
{
	UPROPERTY()
	FOnScoreChanged OnScoreChanged;

	UPROPERTY()
	int TotalReceived = 0;

	UFUNCTION()
	void HandleScore(int32 Score)
	{
		TotalReceived += Score;
	}

	UFUNCTION()
	int RunMulticastTest()
	{
		OnScoreChanged.AddUFunction(this, n"HandleScore");

		if (!OnScoreChanged.IsBound())
			return 10;

		OnScoreChanged.Broadcast(50);
		if (TotalReceived != 50)
			return 20;

		OnScoreChanged.Broadcast(25);
		if (TotalReceived != 75)
			return 30;

		return 1;
	}
}
)AS"),
			TEXT("ATestDelegateMulticastScript"));
		if (ScriptClass == nullptr) return;

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;
		BeginPlayActor(Engine, *Actor);

		FFunctionInvoker Invoker(*TestRunner, Actor, FName(TEXT("RunMulticastTest")));
		if (!Invoker.IsValid()) return;
		TestRunner->TestEqual(TEXT("AddUFunction + Broadcast from script should accumulate values"),
			Invoker.CallAndReturn<int32>(INDEX_NONE), 1);
	}

	TEST_METHOD(MultipleSubscribers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestDelegateMultiSub"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestDelegateMultiSub.as"),
			TEXT(R"AS(
event void FOnTick();

UCLASS()
class ATestDelegateMultiSub : AActor
{
	UPROPERTY()
	FOnTick OnTick;

	UPROPERTY()
	int CountA = 0;
	UPROPERTY()
	int CountB = 0;

	UFUNCTION()
	void HandlerA() { CountA += 1; }

	UFUNCTION()
	void HandlerB() { CountB += 1; }

	UFUNCTION()
	int RunMultiSubTest()
	{
		OnTick.AddUFunction(this, n"HandlerA");
		OnTick.AddUFunction(this, n"HandlerB");

		OnTick.Broadcast();

		if (CountA != 1)
			return 10;
		if (CountB != 1)
			return 20;

		OnTick.Broadcast();
		if (CountA != 2)
			return 30;
		if (CountB != 2)
			return 40;

		return 1;
	}
}
)AS"),
			TEXT("ATestDelegateMultiSub"));
		if (ScriptClass == nullptr) return;

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;
		BeginPlayActor(Engine, *Actor);

		FFunctionInvoker Invoker(*TestRunner, Actor, FName(TEXT("RunMultiSubTest")));
		if (!Invoker.IsValid()) return;
		TestRunner->TestEqual(TEXT("Multiple subscribers should all receive each Broadcast"),
			Invoker.CallAndReturn<int32>(INDEX_NONE), 1);
	}

	TEST_METHOD(UnbindRemovesSpecificSubscriber)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestDelegateUnbind"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestDelegateUnbind.as"),
			TEXT(R"AS(
event void FOnPulse();

UCLASS()
class ATestDelegateUnbind : AActor
{
	UPROPERTY()
	FOnPulse OnPulse;

	UPROPERTY()
	int CountA = 0;
	UPROPERTY()
	int CountB = 0;

	UFUNCTION()
	void HandlerA() { CountA += 1; }

	UFUNCTION()
	void HandlerB() { CountB += 1; }

	UFUNCTION()
	int RunUnbindTest()
	{
		OnPulse.AddUFunction(this, n"HandlerA");
		OnPulse.AddUFunction(this, n"HandlerB");

		OnPulse.Broadcast();
		if (CountA != 1 || CountB != 1)
			return 10;

		OnPulse.Unbind(this, n"HandlerA");
		OnPulse.Broadcast();
		if (CountA != 1)
			return 20;
		if (CountB != 2)
			return 30;

		return 1;
	}
}
)AS"),
			TEXT("ATestDelegateUnbind"));
		if (ScriptClass == nullptr) return;

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;
		BeginPlayActor(Engine, *Actor);

		FFunctionInvoker Invoker(*TestRunner, Actor, FName(TEXT("RunUnbindTest")));
		if (!Invoker.IsValid()) return;
		TestRunner->TestEqual(TEXT("Unbind should remove only the specified handler"),
			Invoker.CallAndReturn<int32>(INDEX_NONE), 1);
	}

	TEST_METHOD(ClearRemovesAllSubscribers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestDelegateMCClear"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestDelegateMCClear.as"),
			TEXT(R"AS(
event void FOnPing();

UCLASS()
class ATestDelegateMCClear : AActor
{
	UPROPERTY()
	FOnPing OnPing;

	UPROPERTY()
	int Count = 0;

	UFUNCTION()
	void Handler() { Count += 1; }

	UFUNCTION()
	int RunClearTest()
	{
		OnPing.AddUFunction(this, n"Handler");
		OnPing.Broadcast();
		if (Count != 1)
			return 10;

		OnPing.Clear();
		if (OnPing.IsBound())
			return 20;

		OnPing.Broadcast();
		if (Count != 1)
			return 30;

		return 1;
	}
}
)AS"),
			TEXT("ATestDelegateMCClear"));
		if (ScriptClass == nullptr) return;

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;
		BeginPlayActor(Engine, *Actor);

		FFunctionInvoker Invoker(*TestRunner, Actor, FName(TEXT("RunClearTest")));
		if (!Invoker.IsValid()) return;
		TestRunner->TestEqual(TEXT("Clear should remove all subscribers and IsBound returns false"),
			Invoker.CallAndReturn<int32>(INDEX_NONE), 1);
	}

	TEST_METHOD(UnbindObjectRemovesAllForTarget)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestDelegateUnbindObj"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestDelegateUnbindObj.as"),
			TEXT(R"AS(
event void FOnSignal();

UCLASS()
class ATestDelegateUnbindObj : AActor
{
	UPROPERTY()
	FOnSignal OnSignal;

	UPROPERTY()
	int CountA = 0;
	UPROPERTY()
	int CountB = 0;

	UFUNCTION()
	void HandlerA() { CountA += 1; }

	UFUNCTION()
	void HandlerB() { CountB += 1; }

	UFUNCTION()
	int RunUnbindObjTest()
	{
		OnSignal.AddUFunction(this, n"HandlerA");
		OnSignal.AddUFunction(this, n"HandlerB");

		OnSignal.Broadcast();
		if (CountA != 1 || CountB != 1)
			return 10;

		OnSignal.UnbindObject(this);
		if (OnSignal.IsBound())
			return 20;

		OnSignal.Broadcast();
		if (CountA != 1 || CountB != 1)
			return 30;

		return 1;
	}
}
)AS"),
			TEXT("ATestDelegateUnbindObj"));
		if (ScriptClass == nullptr) return;

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;
		BeginPlayActor(Engine, *Actor);

		FFunctionInvoker Invoker(*TestRunner, Actor, FName(TEXT("RunUnbindObjTest")));
		if (!Invoker.IsValid()) return;
		TestRunner->TestEqual(TEXT("UnbindObject should remove all handlers bound to the target"),
			Invoker.CallAndReturn<int32>(INDEX_NONE), 1);
	}

	TEST_METHOD(SignatureMismatchDoesNotInvoke)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestDelegateMulticastSigMismatch"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("TestDelegateMulticastSigMismatch.as"),
			TEXT(R"AS(
event void FOnDamaged(int32 NewHealth, const FString& Label);

UCLASS()
class ATestDelegateMulticastSigMismatch : AActor
{
	UPROPERTY()
	FOnDamaged OnDamaged;

	UFUNCTION()
	void TriggerDamaged(int32 NewHealth, const FString& Label)
	{
		OnDamaged.Broadcast(NewHealth, Label);
	}
}
)AS"),
			TEXT("ATestDelegateMulticastSigMismatch"));
		if (ScriptClass == nullptr) return;

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should spawn"), Actor)) return;
		BeginPlayActor(Engine, *Actor);

		UAngelscriptNativeScriptTestObject* NativeReceiver = NewObject<UAngelscriptNativeScriptTestObject>(GetTransientPackage());
		if (!TestRunner->TestNotNull(TEXT("Native receiver should be created"), NativeReceiver)) return;
		NativeReceiver->bNativeFlag = false;

		FMulticastInlineDelegateProperty* MulticastProperty = FindFProperty<FMulticastInlineDelegateProperty>(Actor->GetClass(), TEXT("OnDamaged"));
		if (!TestRunner->TestNotNull(TEXT("Multicast property should exist"), MulticastProperty)) return;

		FMulticastScriptDelegate* MulticastDelegate = MulticastProperty->ContainerPtrToValuePtr<FMulticastScriptDelegate>(Actor);
		if (!TestRunner->TestNotNull(TEXT("Multicast delegate storage should exist"), MulticastDelegate)) return;

		FScriptDelegate BoundDelegate;
		BoundDelegate.BindUFunction(NativeReceiver, TEXT("MarkNativeFlagFromDelegate"));
		MulticastDelegate->Add(BoundDelegate);

		UFunction* TriggerFunction = FindGeneratedFunction(ScriptClass, TEXT("TriggerDamaged"));
		if (!TestRunner->TestNotNull(TEXT("Trigger function should exist"), TriggerFunction)) return;

		TestRunner->AddExpectedError(TEXT("Signature mismatch"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("Angelscript"), EAutomationExpectedErrorFlags::Contains, 0);

		FTestCaseIntStringParams Params;
		Params.Value = 45;
		Params.Label = TEXT("MulticastMismatch");
		{
			FAngelscriptEngineScope ExecutionScope(Engine, Actor);
			Actor->ProcessEvent(TriggerFunction, &Params);
		}

		TestRunner->TestFalse(TEXT("Signature-mismatched multicast should not invoke zero-arg native receiver"),
			NativeReceiver->bNativeFlag);
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
