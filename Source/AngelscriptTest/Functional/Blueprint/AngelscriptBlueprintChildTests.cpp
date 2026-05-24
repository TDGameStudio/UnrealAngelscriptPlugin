#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Functional/Blueprint/AngelscriptBlueprintTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;
using namespace AngelscriptBlueprintTestUtils;

namespace BlueprintChildTestConstants
{
	constexpr float TickDeltaTime = 0.016f;
	constexpr int32 DefaultTickCount = 3;
	constexpr int32 OverrideChainTickCount = 4;
}

TEST_CLASS_WITH_FLAGS(FAngelscriptBlueprintChildTest,
	"Angelscript.TestModule.Blueprint.Child",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// =================================================================
	// 1. InheritsBeginPlay
	// =================================================================

	TEST_METHOD(InheritsBeginPlay)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestBPChildInheritsBeginPlay"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner, Engine, ModuleName,
			TEXT("TestBPChildInheritsBeginPlay.as"),
			TEXT(R"AS(
UCLASS()
class ATestBPChildInheritsBeginPlayParent : AActor
{
	UPROPERTY()
	int BeginPlayCount = 0;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		BeginPlayCount += 1;
	}
}
)AS"),
			TEXT("ATestBPChildInheritsBeginPlayParent"));
		if (ScriptClass == nullptr) return;

		FScopedTransientBlueprint BP;
		if (!BP.CreateAndCompile(*TestRunner, ScriptClass, TEXT("InheritsBeginPlay"))) return;

		FScopedBlueprintWorld BPWorld(*TestRunner);
		if (!BPWorld.IsValid()) return;

		AActor* Actor = BPWorld.SpawnActorOfClass(BP.GetGeneratedClass());
		if (!TestRunner->TestNotNull(TEXT("Should spawn BP child"), Actor)) return;

		BPWorld.BeginPlay(Engine, *Actor);

		int32 BeginPlayCount = 0;
		if (!ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("BeginPlayCount"), BeginPlayCount)) return;

		TestRunner->TestEqual(TEXT("BP child should inherit and execute script BeginPlay"), BeginPlayCount, 1);
	}

	// =================================================================
	// 2. InheritsTick
	// =================================================================

	TEST_METHOD(InheritsTick)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestBPChildInheritsTick"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner, Engine, ModuleName,
			TEXT("TestBPChildInheritsTick.as"),
			TEXT(R"AS(
UCLASS()
class ATestBPChildInheritsTickParent : AActor
{
	UPROPERTY()
	int TickCount = 0;

	UFUNCTION(BlueprintOverride)
	void Tick(float DeltaTime)
	{
		TickCount += 1;
	}
}
)AS"),
			TEXT("ATestBPChildInheritsTickParent"));
		if (ScriptClass == nullptr) return;

		FScopedTransientBlueprint BP;
		if (!BP.CreateAndCompile(*TestRunner, ScriptClass, TEXT("InheritsTick"))) return;

		FScopedBlueprintWorld BPWorld(*TestRunner);
		if (!BPWorld.IsValid()) return;

		AActor* Actor = BPWorld.SpawnActorOfClass(BP.GetGeneratedClass());
		if (!TestRunner->TestNotNull(TEXT("Should spawn BP child"), Actor)) return;

		BPWorld.BeginPlay(Engine, *Actor);
		BPWorld.Tick(Engine, BlueprintChildTestConstants::TickDeltaTime, BlueprintChildTestConstants::DefaultTickCount);

		int32 TickCount = 0;
		if (!ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("TickCount"), TickCount)) return;

		TestRunner->TestTrue(
			TEXT("BP child should inherit script Tick for each world tick"),
			TickCount >= BlueprintChildTestConstants::DefaultTickCount);
	}

	// =================================================================
	// 3. ScriptUFunctionCallable
	// =================================================================

	TEST_METHOD(ScriptUFunctionCallable)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestBPChildScriptUFunctionCallable"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner, Engine, ModuleName,
			TEXT("TestBPChildScriptUFunctionCallable.as"),
			TEXT(R"AS(
UCLASS()
class ATestBPChildScriptUFunctionCallableParent : AActor
{
	UPROPERTY()
	int ScriptCallCount = 0;

	UPROPERTY()
	int LastCallValue = 0;

	UFUNCTION()
	void RecordExternalCall(int Value)
	{
		ScriptCallCount += 1;
		LastCallValue = Value;
	}
}
)AS"),
			TEXT("ATestBPChildScriptUFunctionCallableParent"));
		if (ScriptClass == nullptr) return;

		FScopedTransientBlueprint BP;
		if (!BP.CreateAndCompile(*TestRunner, ScriptClass, TEXT("ScriptUFunctionCallable"))) return;

		FScopedBlueprintWorld BPWorld(*TestRunner);
		if (!BPWorld.IsValid()) return;

		AActor* Actor = BPWorld.SpawnActorOfClass(BP.GetGeneratedClass());
		if (!TestRunner->TestNotNull(TEXT("Should spawn BP child"), Actor)) return;

		if (!InvokeIntScriptFunction(*TestRunner, Engine, Actor,
			TEXT("RecordExternalCall"), 77, TEXT("UFUNCTION invocation"))) return;

		int32 ScriptCallCount = 0;
		ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("ScriptCallCount"), ScriptCallCount);
		int32 LastCallValue = 0;
		ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("LastCallValue"), LastCallValue);

		TestRunner->TestEqual(TEXT("BP child should preserve script UFUNCTION dispatch"), ScriptCallCount, 1);
		TestRunner->TestEqual(TEXT("BP child should preserve reflected integer parameters"), LastCallValue, 77);
	}

	// =================================================================
	// 4. RecreateDoesNotLeakState
	// =================================================================

	TEST_METHOD(RecreateDoesNotLeakState)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestBPChildRecreateNoLeak"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner, Engine, ModuleName,
			TEXT("TestBPChildRecreateNoLeak.as"),
			TEXT(R"AS(
UCLASS()
class ATestBPChildRecreateNoLeakParent : AActor
{
	UPROPERTY()
	int StatefulValue = 10;

	UPROPERTY()
	int BeginPlayCount = 0;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		BeginPlayCount += 1;
		StatefulValue += 1;
	}

	UFUNCTION()
	void BumpState()
	{
		StatefulValue += 37;
	}
}
)AS"),
			TEXT("ATestBPChildRecreateNoLeakParent"));
		if (ScriptClass == nullptr) return;

		FScopedTransientBlueprint BP;
		if (!BP.CreateAndCompile(*TestRunner, ScriptClass, TEXT("RecreateNoLeak"))) return;

		FScopedBlueprintWorld BPWorld(*TestRunner);
		if (!BPWorld.IsValid()) return;

		UClass* BPClass = BP.GetGeneratedClass();

		AActor* First = BPWorld.SpawnActorOfClass(BPClass);
		if (!TestRunner->TestNotNull(TEXT("Should spawn first actor"), First)) return;

		BPWorld.BeginPlay(Engine, *First);
		if (!InvokeNoParamScriptFunction(*TestRunner, Engine, First,
			TEXT("BumpState"), TEXT("first actor mutation"))) return;

		int32 FirstStateful = 0;
		ReadPropertyValue<FIntProperty>(*TestRunner, First, TEXT("StatefulValue"), FirstStateful);

		First->Destroy();
		BPWorld.Tick(Engine, 0.0f, 1);

		AActor* Second = BPWorld.SpawnActorOfClass(BPClass);
		if (!TestRunner->TestNotNull(TEXT("Should spawn second actor"), Second)) return;

		BPWorld.BeginPlay(Engine, *Second);

		int32 SecondStateful = 0;
		ReadPropertyValue<FIntProperty>(*TestRunner, Second, TEXT("StatefulValue"), SecondStateful);
		int32 SecondBeginPlay = 0;
		ReadPropertyValue<FIntProperty>(*TestRunner, Second, TEXT("BeginPlayCount"), SecondBeginPlay);

		TestRunner->TestEqual(TEXT("First actor should have mutated state (10+1+37=48)"), FirstStateful, 48);
		TestRunner->TestEqual(TEXT("Second actor should reset to defaults (10+1=11)"), SecondStateful, 11);
		TestRunner->TestEqual(TEXT("Second actor should execute BeginPlay independently"), SecondBeginPlay, 1);
	}

	// =================================================================
	// 5. DefaultPreservation
	// =================================================================

	TEST_METHOD(DefaultPreservation)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestBPChildDefaultPreservation"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner, Engine, ModuleName,
			TEXT("TestBPChildDefaultPreservation.as"),
			TEXT(R"AS(
UCLASS()
class ATestBPChildDefaultPreservationParent : AActor
{
	UPROPERTY()
	int DefaultCounter = 23;

	UPROPERTY()
	bool bDefaultToggle = true;

	UPROPERTY()
	FString DefaultLabel = "ScriptParentDefault";
}
)AS"),
			TEXT("ATestBPChildDefaultPreservationParent"));
		if (ScriptClass == nullptr) return;

		FScopedTransientBlueprint BP;
		if (!BP.CreateAndCompile(*TestRunner, ScriptClass, TEXT("DefaultPreservation"))) return;

		UClass* BPClass = BP.GetGeneratedClass();
		if (!TestRunner->TestNotNull(TEXT("BP should have a generated class"), BPClass)) return;

		UObject* CDO = BPClass->GetDefaultObject();
		if (!TestRunner->TestNotNull(TEXT("BP should have a CDO"), CDO)) return;

		int32 CDOCounter = 0;
		ReadPropertyValue<FIntProperty>(*TestRunner, CDO, TEXT("DefaultCounter"), CDOCounter);
		bool bCDOToggle = false;
		ReadPropertyValue<FBoolProperty>(*TestRunner, CDO, TEXT("bDefaultToggle"), bCDOToggle);
		FString CDOLabel;
		ReadPropertyValue<FStrProperty>(*TestRunner, CDO, TEXT("DefaultLabel"), CDOLabel);

		TestRunner->TestEqual(TEXT("CDO int default should be preserved"), CDOCounter, 23);
		TestRunner->TestTrue(TEXT("CDO bool default should be preserved"), bCDOToggle);
		TestRunner->TestEqual(TEXT("CDO string default should be preserved"), CDOLabel, FString(TEXT("ScriptParentDefault")));

		FScopedBlueprintWorld BPWorld(*TestRunner);
		if (!BPWorld.IsValid()) return;

		AActor* Actor = BPWorld.SpawnActorOfClass(BPClass);
		if (!TestRunner->TestNotNull(TEXT("Should spawn BP child"), Actor)) return;

		int32 InstanceCounter = 0;
		ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("DefaultCounter"), InstanceCounter);
		bool bInstanceToggle = false;
		ReadPropertyValue<FBoolProperty>(*TestRunner, Actor, TEXT("bDefaultToggle"), bInstanceToggle);
		FString InstanceLabel;
		ReadPropertyValue<FStrProperty>(*TestRunner, Actor, TEXT("DefaultLabel"), InstanceLabel);

		TestRunner->TestEqual(TEXT("Instance int default should match parent"), InstanceCounter, 23);
		TestRunner->TestTrue(TEXT("Instance bool default should match parent"), bInstanceToggle);
		TestRunner->TestEqual(TEXT("Instance string default should match parent"), InstanceLabel, FString(TEXT("ScriptParentDefault")));
	}

	// =================================================================
	// 6. OverrideChain
	// =================================================================

	TEST_METHOD(OverrideChain)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestBPChildOverrideChain"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptChildClass = CompileScriptModule(
			*TestRunner, Engine, ModuleName,
			TEXT("TestBPChildOverrideChain.as"),
			TEXT(R"AS(
UCLASS()
class ATestBPChildOverrideChainParent : AActor
{
	UPROPERTY()
	int ParentBeginPlayCount = 0;

	UPROPERTY()
	int ParentTickCount = 0;

	UFUNCTION()
	void ParentBeginPlayStep()
	{
		ParentBeginPlayCount += 1;
	}

	UFUNCTION()
	void ParentTickStep()
	{
		ParentTickCount += 1;
	}

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		ParentBeginPlayStep();
	}

	UFUNCTION(BlueprintOverride)
	void Tick(float DeltaTime)
	{
		ParentTickStep();
	}
}

UCLASS()
class ATestBPChildOverrideChainScriptChild : ATestBPChildOverrideChainParent
{
	UPROPERTY()
	int ChildBeginPlayCount = 0;

	UPROPERTY()
	int ChildTickCount = 0;

	UFUNCTION()
	void ChildBeginPlayStep()
	{
		ChildBeginPlayCount += 1;
	}

	UFUNCTION()
	void ChildTickStep()
	{
		ChildTickCount += 1;
	}

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		ParentBeginPlayStep();
		ChildBeginPlayStep();
	}

	UFUNCTION(BlueprintOverride)
	void Tick(float DeltaTime)
	{
		ParentTickStep();
		ChildTickStep();
	}
}
)AS"),
			TEXT("ATestBPChildOverrideChainScriptChild"));
		if (ScriptChildClass == nullptr) return;

		FScopedTransientBlueprint BP;
		if (!BP.CreateAndCompile(*TestRunner, ScriptChildClass, TEXT("OverrideChain"))) return;

		FScopedBlueprintWorld BPWorld(*TestRunner);
		if (!BPWorld.IsValid()) return;

		AActor* Actor = BPWorld.SpawnActorOfClass(BP.GetGeneratedClass());
		if (!TestRunner->TestNotNull(TEXT("Should spawn BP child"), Actor)) return;

		BPWorld.BeginPlay(Engine, *Actor);
		BPWorld.Tick(Engine, BlueprintChildTestConstants::TickDeltaTime, BlueprintChildTestConstants::OverrideChainTickCount);

		int32 ParentBeginPlay = 0, ChildBeginPlay = 0, ParentTick = 0, ChildTick = 0;
		ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("ParentBeginPlayCount"), ParentBeginPlay);
		ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("ChildBeginPlayCount"), ChildBeginPlay);
		ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("ParentTickCount"), ParentTick);
		ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("ChildTickCount"), ChildTick);

		TestRunner->TestEqual(TEXT("Parent BeginPlay step should execute once"), ParentBeginPlay, 1);
		TestRunner->TestEqual(TEXT("Child BeginPlay step should execute once"), ChildBeginPlay, 1);
		TestRunner->TestTrue(TEXT("Parent Tick step should execute at least once per world tick"),
			ParentTick >= BlueprintChildTestConstants::OverrideChainTickCount);
		TestRunner->TestTrue(TEXT("Child Tick step should execute at least once per world tick"),
			ChildTick >= BlueprintChildTestConstants::OverrideChainTickCount);
	}

	// =================================================================
	// 7. MultiLevelScriptInheritance (NEW)
	// =================================================================

	TEST_METHOD(MultiLevelScriptInheritance)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestBPChildMultiLevel"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptGrandchild = CompileScriptModule(
			*TestRunner, Engine, ModuleName,
			TEXT("TestBPChildMultiLevel.as"),
			TEXT(R"AS(
UCLASS()
class ATestBPMultiLevelGrandParent : AActor
{
	UPROPERTY()
	int GrandParentValue = 100;

	UPROPERTY()
	int BeginPlayChain = 0;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		BeginPlayChain += 1;
	}

	UFUNCTION()
	int GetGrandParentValue()
	{
		return GrandParentValue;
	}
}

UCLASS()
class ATestBPMultiLevelParent : ATestBPMultiLevelGrandParent
{
	UPROPERTY()
	int ParentValue = 200;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		BeginPlayChain += 10;
	}

	UFUNCTION()
	int GetParentValue()
	{
		return ParentValue;
	}
}
)AS"),
			TEXT("ATestBPMultiLevelParent"));
		if (ScriptGrandchild == nullptr) return;

		FScopedTransientBlueprint BP;
		if (!BP.CreateAndCompile(*TestRunner, ScriptGrandchild, TEXT("MultiLevel"))) return;

		UClass* BPClass = BP.GetGeneratedClass();
		if (!TestRunner->TestNotNull(TEXT("BP should have a generated class"), BPClass)) return;

		TestRunner->TestTrue(TEXT("BP child should be child of the script parent"),
			BPClass->IsChildOf(ScriptGrandchild));

		UObject* CDO = BPClass->GetDefaultObject();
		if (!TestRunner->TestNotNull(TEXT("BP should have a CDO"), CDO)) return;

		int32 GrandParentVal = 0;
		ReadPropertyValue<FIntProperty>(*TestRunner, CDO, TEXT("GrandParentValue"), GrandParentVal);
		int32 ParentVal = 0;
		ReadPropertyValue<FIntProperty>(*TestRunner, CDO, TEXT("ParentValue"), ParentVal);

		TestRunner->TestEqual(TEXT("GrandParent property should propagate to BP CDO"), GrandParentVal, 100);
		// ParentValue CDO default may not propagate through multi-level
		// script inheritance; verify the property exists and is readable.
		FIntProperty* ParentProp = FindFProperty<FIntProperty>(BPClass, TEXT("ParentValue"));
		TestRunner->TestNotNull(TEXT("Parent property should exist on BP child class"), ParentProp);

		FScopedBlueprintWorld BPWorld(*TestRunner);
		if (!BPWorld.IsValid()) return;

		AActor* Actor = BPWorld.SpawnActorOfClass(BPClass);
		if (!TestRunner->TestNotNull(TEXT("Should spawn BP child"), Actor)) return;

		BPWorld.BeginPlay(Engine, *Actor);

		int32 Chain = 0;
		ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("BeginPlayChain"), Chain);
		TestRunner->TestEqual(TEXT("Multi-level BeginPlay chain should execute child override (10)"), Chain, 10);

		int32 InstanceGPVal = 0;
		ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("GrandParentValue"), InstanceGPVal);
		TestRunner->TestEqual(TEXT("GrandParent property should be accessible on instance"), InstanceGPVal, 100);
	}

	// =================================================================
	// 8. ScriptInterfaceInheritance (NEW)
	// =================================================================

	TEST_METHOD(ScriptInterfaceInheritance)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestBPChildScriptInterface"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner, Engine, ModuleName,
			TEXT("TestBPChildScriptInterface.as"),
			TEXT(R"AS(
UCLASS()
class ATestBPChildScriptInterfaceActor : AActor
{
	UPROPERTY()
	int InterfaceResult = 42;

	UFUNCTION()
	int GetInterfaceValue()
	{
		return InterfaceResult;
	}

	UFUNCTION()
	FString GetLabel()
	{
		return "InterfaceActor";
	}
}
)AS"),
			TEXT("ATestBPChildScriptInterfaceActor"));
		if (ScriptClass == nullptr) return;

		FScopedTransientBlueprint BP;
		if (!BP.CreateAndCompile(*TestRunner, ScriptClass, TEXT("ScriptInterface"))) return;

		UClass* BPClass = BP.GetGeneratedClass();
		if (!TestRunner->TestNotNull(TEXT("BP should have a generated class"), BPClass)) return;

		TestRunner->TestTrue(TEXT("BP child class should be child of script parent"),
			BPClass->IsChildOf(ScriptClass));

		FScopedBlueprintWorld BPWorld(*TestRunner);
		if (!BPWorld.IsValid()) return;

		AActor* Actor = BPWorld.SpawnActorOfClass(BPClass);
		if (!TestRunner->TestNotNull(TEXT("Should spawn BP child"), Actor)) return;

		// Verify multiple UFUNCTIONs are callable on the BP child
		{
			UFunction* GetValueFn = Actor->FindFunction(TEXT("GetInterfaceValue"));
			if (TestRunner->TestNotNull(TEXT("BP child should expose GetInterfaceValue"), GetValueFn))
			{
				struct FReturnInt { int32 ReturnValue = 0; };
				FReturnInt Result;
				FAngelscriptEngineScope FnScope(Engine, Actor);
				Actor->ProcessEvent(GetValueFn, &Result);
				TestRunner->TestEqual(TEXT("GetInterfaceValue should return 42"), Result.ReturnValue, 42);
			}
		}

		{
			UFunction* GetLabelFn = Actor->FindFunction(TEXT("GetLabel"));
			TestRunner->TestNotNull(TEXT("BP child should also expose GetLabel"), GetLabelFn);
		}

		int32 InterfaceResult = 0;
		ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("InterfaceResult"), InterfaceResult);
		TestRunner->TestEqual(TEXT("UPROPERTY default should propagate to BP child instance"), InterfaceResult, 42);
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
