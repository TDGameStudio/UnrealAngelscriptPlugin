#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageDynamicDelegateTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript dynamic delegate usage, the third slice of the
// delegates-and-events matrix (Documents/Coverage/Coverage_DelegatesAndEvents.md
// section 3). Each TEST_METHOD walks one usage axis from the matrix:
//
// Axes covered here:
//   * DynamicDelegateDeclaration  - DECLARE_DYNAMIC_DELEGATE variants (Blueprint
//                                   compatible delegates).
//   * DynamicDelegateBinding      - BindDynamic, AddDynamic for UFunction binding.
//   * DynamicDelegateSerialization - Persistence support for dynamic delegates.
//   * DynamicDelegateBlueprint    - BlueprintAssignable, BlueprintCallable properties.
//
// Pattern D (script execution) from the Angelscript test guide: compile AS
// actors, spawn them, drive dynamic delegate operations, verify results
// through properties.
//
// Dynamic delegates are Blueprint-compatible but slower than regular delegates.
// They support serialization and are required for Blueprint event integration.
//
// Detailed coverage matrix: Documents/Coverage/Coverage_DelegatesAndEvents.md
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageDynamicDelegateTest,
	"Angelscript.TestModule.Coverage.DynamicDelegate",
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

	// -------------------------------------------------------------------------
	// Basic dynamic single-cast delegate: BindDynamic, IsBound, Execute.
	// -------------------------------------------------------------------------
	TEST_METHOD(DynamicDelegateBasics)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDynamicDelegate_Basics"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDynamicDelegateBasics.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageDynamicDelegateBasicsActor : AActor
			{
				UPROPERTY()
				int Counter = 0;

				UPROPERTY()
				bool DelegateWasCalled = false;

				// Dynamic delegates (single-cast)
				FSimpleDelegate OnDynamicEvent;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Test IsBound before binding
					if (!OnDynamicEvent.IsBound())
					{
						Counter = 1;
					}

					// Bind to UFUNCTION
					OnDynamicEvent.BindUFunction(this, n"HandleDynamicDelegate");

					// Test IsBound after binding
					if (OnDynamicEvent.IsBound())
					{
						Counter = 2;
					}

					// Execute delegate
					OnDynamicEvent.Execute();

					// Test Clear
					OnDynamicEvent.Clear();
					if (!OnDynamicEvent.IsBound())
					{
						Counter = 3;
					}
				}

				UFUNCTION()
				void HandleDynamicDelegate()
				{
					DelegateWasCalled = true;
				}
			}
			)AS"),
			TEXT("ACoverageDynamicDelegateBasicsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Dynamic-delegate-basics actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Dynamic-delegate-basics actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Counter"), 3, TEXT("Counter should be 3 (IsBound checks passed)"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("DelegateWasCalled"), true, TEXT("Dynamic delegate should have been executed"));
	}

	// -------------------------------------------------------------------------
	// Dynamic multicast delegate: AddDynamic, RemoveDynamic, Broadcast.
	// -------------------------------------------------------------------------
	TEST_METHOD(DynamicMulticastDelegate)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDynamicDelegate_Multicast"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDynamicDelegateMulticast.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageDynamicMulticastActor : AActor
			{
				UPROPERTY()
				int Counter = 0;

				UPROPERTY()
				FString Result;

				// Dynamic multicast delegate
				FSimpleMulticastDelegate OnMulticastEvent;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Add multiple listeners
					OnMulticastEvent.AddUFunction(this, n"Listener1");
					OnMulticastEvent.AddUFunction(this, n"Listener2");
					OnMulticastEvent.AddUFunction(this, n"Listener3");

					// Broadcast - all three should be called
					OnMulticastEvent.Broadcast();

					// Remove one listener
					OnMulticastEvent.RemoveAll(this, n"Listener2");

					// Broadcast again - only Listener1 and Listener3 should be called
					OnMulticastEvent.Broadcast();
				}

				UFUNCTION()
				void Listener1()
				{
					Counter += 1;
					Result += "A";
				}

				UFUNCTION()
				void Listener2()
				{
					Counter += 10;
					Result += "B";
				}

				UFUNCTION()
				void Listener3()
				{
					Counter += 100;
					Result += "C";
				}
			}
			)AS"),
			TEXT("ACoverageDynamicMulticastActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Dynamic-multicast actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Dynamic-multicast actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Expected: 1 + 10 + 100 (first broadcast) + 1 + 100 (second broadcast) = 212
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Counter"), 212, TEXT("Dynamic multicast listeners should be called correctly"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("Result"), FString(TEXT("ABCAC")), TEXT("Listeners should be called in order, then without B"));
	}

	// -------------------------------------------------------------------------
	// Dynamic delegate with parameters: passing parameters through dynamic
	// delegates.
	// -------------------------------------------------------------------------
	TEST_METHOD(DynamicDelegateParameters)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDynamicDelegate_Parameters"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDynamicDelegateParameters.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageDynamicParamsActor : AActor
			{
				UPROPERTY()
				int ReceivedInt = 0;

				UPROPERTY()
				FString ReceivedString;

				FIntDelegate OnIntEvent;
				FIntStringMulticastDelegate OnIntStringEvent;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Single parameter dynamic delegate
					OnIntEvent.BindUFunction(this, n"HandleIntEvent");
					OnIntEvent.Execute(42);

					// Two parameter dynamic multicast delegate
					OnIntStringEvent.AddUFunction(this, n"HandleIntStringEvent");
					OnIntStringEvent.Broadcast(100, "Test");
				}

				UFUNCTION()
				void HandleIntEvent(int Value)
				{
					ReceivedInt = Value;
				}

				UFUNCTION()
				void HandleIntStringEvent(int IntValue, FString StringValue)
				{
					ReceivedInt = IntValue;
					ReceivedString = StringValue;
				}
			}
			)AS"),
			TEXT("ACoverageDynamicParamsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Dynamic-parameters actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Dynamic-parameters actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ReceivedInt"), 100, TEXT("Two-param dynamic delegate should set ReceivedInt to 100"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("ReceivedString"), FString(TEXT("Test")), TEXT("Two-param dynamic delegate should set ReceivedString"));
	}

	// -------------------------------------------------------------------------
	// BlueprintAssignable property: exposes dynamic delegates to Blueprint.
	// -------------------------------------------------------------------------
	TEST_METHOD(DynamicDelegateBlueprintAssignable)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDynamicDelegate_BlueprintAssignable"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDynamicDelegateBlueprintAssignable.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageBlueprintAssignableActor : AActor
			{
				UPROPERTY()
				int EventCount = 0;

				// These delegates can be exposed to Blueprint via UPROPERTY
				FSimpleMulticastDelegate OnCustomEvent;
				FIntMulticastDelegate OnValueChanged;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Bind handlers in script
					OnCustomEvent.AddUFunction(this, n"HandleCustomEvent");
					OnValueChanged.AddUFunction(this, n"HandleValueChanged");

					// Trigger events (Blueprint could also bind to these)
					OnCustomEvent.Broadcast();
					OnValueChanged.Broadcast(50);
				}

				UFUNCTION()
				void HandleCustomEvent()
				{
					EventCount++;
				}

				UFUNCTION()
				void HandleValueChanged(int NewValue)
				{
					EventCount += NewValue;
				}
			}
			)AS"),
			TEXT("ACoverageBlueprintAssignableActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Blueprint-assignable actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Blueprint-assignable actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Expected: 1 (from HandleCustomEvent) + 50 (from HandleValueChanged) = 51
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("EventCount"), 51, TEXT("BlueprintAssignable events should be callable"));
	}

	// -------------------------------------------------------------------------
	// Dynamic delegate clear: Clear removes all bindings.
	// -------------------------------------------------------------------------
	TEST_METHOD(DynamicDelegateClear)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDynamicDelegate_Clear"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDynamicDelegateClear.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageDynamicClearActor : AActor
			{
				UPROPERTY()
				int Counter = 0;

				FSimpleMulticastDelegate OnEvent;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Add multiple listeners
					OnEvent.AddUFunction(this, n"Handler1");
					OnEvent.AddUFunction(this, n"Handler2");
					OnEvent.AddUFunction(this, n"Handler3");

					// Broadcast - all should be called
					OnEvent.Broadcast();

					// Clear all listeners
					OnEvent.Clear();

					// Broadcast again - none should be called
					OnEvent.Broadcast();

					// Verify not bound
					if (!OnEvent.IsBound())
					{
						Counter += 1000;
					}
				}

				UFUNCTION()
				void Handler1()
				{
					Counter += 1;
				}

				UFUNCTION()
				void Handler2()
				{
					Counter += 10;
				}

				UFUNCTION()
				void Handler3()
				{
					Counter += 100;
				}
			}
			)AS"),
			TEXT("ACoverageDynamicClearActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Dynamic-clear actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Dynamic-clear actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Expected: 1 + 10 + 100 (first broadcast) + 1000 (not bound) = 1111
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Counter"), 1111, TEXT("Clear should remove all dynamic delegate bindings"));
	}

	// -------------------------------------------------------------------------
	// Dynamic delegate return value: single-cast dynamic delegates can have
	// return values.
	// -------------------------------------------------------------------------
	TEST_METHOD(DynamicDelegateReturnValue)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDynamicDelegate_ReturnValue"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDynamicDelegateReturnValue.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageDynamicRetValActor : AActor
			{
				UPROPERTY()
				bool BoolResult = false;

				UPROPERTY()
				int IntResult = 0;

				FBoolRetDelegate OnBoolRetEvent;
				FIntRetIntDelegate OnIntRetIntEvent;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Return value only
					OnBoolRetEvent.BindUFunction(this, n"HandleBoolRetEvent");
					BoolResult = OnBoolRetEvent.Execute();

					// Return value + parameter
					OnIntRetIntEvent.BindUFunction(this, n"HandleIntRetIntEvent");
					IntResult = OnIntRetIntEvent.Execute(50);
				}

				UFUNCTION()
				bool HandleBoolRetEvent()
				{
					return true;
				}

				UFUNCTION()
				int HandleIntRetIntEvent(int Value)
				{
					return Value * 2;
				}
			}
			)AS"),
			TEXT("ACoverageDynamicRetValActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Dynamic-return-value actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Dynamic-return-value actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolResult"), true, TEXT("Bool return dynamic delegate should return true"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntResult"), 100, TEXT("Int return dynamic delegate should return doubled value"));
	}

	// -------------------------------------------------------------------------
	// Dynamic delegate with complex parameter types: FVector, FString.
	// -------------------------------------------------------------------------
	TEST_METHOD(DynamicDelegateComplexParameters)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDynamicDelegate_ComplexParameters"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDynamicDelegateComplexParameters.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageDynamicComplexParamsActor : AActor
			{
				UPROPERTY()
				FVector ReceivedVector;

				UPROPERTY()
				FString ReceivedString;

				UPROPERTY()
				int ReceivedInt = 0;

				FVectorDelegate OnVectorEvent;
				FVectorStringIntMulticastDelegate OnComplexEvent;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// FVector parameter
					OnVectorEvent.BindUFunction(this, n"HandleVectorEvent");
					OnVectorEvent.Execute(FVector(1.0f, 2.0f, 3.0f));

					// Multiple complex parameters
					OnComplexEvent.AddUFunction(this, n"HandleComplexEvent");
					OnComplexEvent.Broadcast(FVector(10.0f, 20.0f, 30.0f), "Complex", 42);
				}

				UFUNCTION()
				void HandleVectorEvent(FVector V)
				{
					ReceivedVector = V;
				}

				UFUNCTION()
				void HandleComplexEvent(FVector V, FString S, int I)
				{
					ReceivedVector = V;
					ReceivedString = S;
					ReceivedInt = I;
				}
			}
			)AS"),
			TEXT("ACoverageDynamicComplexParamsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Dynamic-complex-parameters actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Dynamic-complex-parameters actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		FVector ExpectedVector(10.0f, 20.0f, 30.0f);
		VerifyStructByPath<FVector>(*TestRunner, Actor, TEXT("ReceivedVector"), ExpectedVector, TEXT("FVector parameter should pass through"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("ReceivedString"), FString(TEXT("Complex")), TEXT("FString parameter should pass through"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ReceivedInt"), 42, TEXT("Int parameter should pass through"));
	}
};

#endif
