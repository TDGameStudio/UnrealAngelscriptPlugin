#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageMulticastDelegateTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript multicast delegate (FMulticastDelegate) usage, the
// second slice of the delegates-and-events matrix (OpenSpec: test-coverage-matrix-consolidation/coverage-matrix.md
// section 2). Each TEST_METHOD walks one usage axis from the matrix:
//
// Axes covered here:
//   * MulticastDeclaration      - DECLARE_MULTICAST_DELEGATE variants (no params,
//                                 with params).
//   * MulticastBinding          - AddUFunction, multiple listeners; AddLambda
//                                 is a negative AS-facing boundary.
//   * MulticastBroadcast        - Broadcast to all listeners.
//   * MulticastRemoval          - Remove (by handle), Clear.
//   * MulticastHandleManagement - FDelegateHandle storage and usage.
//
// Pattern D (script execution) from the Angelscript test guide: compile AS
// actors, spawn them, drive multicast delegate operations, verify results
// through properties.
//
// Multicast delegates do NOT support return values (multiple listeners would
// create ambiguity about which return value to use).
//
// Detailed coverage matrix: OpenSpec: test-coverage-matrix-consolidation/coverage-matrix.md
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageMulticastDelegateTest,
	"Angelscript.TestModule.Coverage.MulticastDelegate",
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
	// Event declaration metadata: AS `event` declarations generate multicast
	// delegate signature functions and FMulticastInlineDelegateProperty members.
	// -------------------------------------------------------------------------
	TEST_METHOD(MulticastEventDeclarationMetadata)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMulticastDelegate_Metadata"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMulticastDelegateMetadata.as"),
			ASTEST_AS(R"AS(
			event void FCoverageMetadataEvent();
			event void FCoverageMetadataValueEvent(int Value);

			UCLASS()
			class ACoverageMulticastMetadataActor : AActor
			{
				UPROPERTY()
				FCoverageMetadataEvent OnNoParam;

				UPROPERTY()
				FCoverageMetadataValueEvent OnValue;
			}
			)AS"),
			TEXT("ACoverageMulticastMetadataActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Multicast metadata actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		const TSharedPtr<FAngelscriptDelegateDesc> NoParamDelegate = Engine.GetDelegate(TEXT("FCoverageMetadataEvent"));
		const TSharedPtr<FAngelscriptDelegateDesc> ValueDelegate = Engine.GetDelegate(TEXT("FCoverageMetadataValueEvent"));
		ASSERT_THAT(IsTrue(NoParamDelegate.IsValid(), TEXT("No-param event delegate metadata should be registered")));
		ASSERT_THAT(IsTrue(ValueDelegate.IsValid(), TEXT("Value event delegate metadata should be registered")));
		if (!NoParamDelegate.IsValid() || !ValueDelegate.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(NoParamDelegate->bIsMulticast, TEXT("AS event should be marked multicast")));
		ASSERT_THAT(IsTrue(ValueDelegate->bIsMulticast, TEXT("AS event with parameter should be marked multicast")));
		ASSERT_THAT(IsNotNull(NoParamDelegate->Function, TEXT("No-param event should materialize a signature function")));
		ASSERT_THAT(IsNotNull(ValueDelegate->Function, TEXT("Value event should materialize a signature function")));
		if (NoParamDelegate->Function == nullptr || ValueDelegate->Function == nullptr)
		{
			return;
		}

		const FMulticastDelegateProperty* NoParamProperty = FindFProperty<FMulticastDelegateProperty>(ScriptClass, TEXT("OnNoParam"));
		const FMulticastDelegateProperty* ValueProperty = FindFProperty<FMulticastDelegateProperty>(ScriptClass, TEXT("OnValue"));
		ASSERT_THAT(IsNotNull(NoParamProperty, TEXT("UPROPERTY event member should generate a multicast delegate property")));
		ASSERT_THAT(IsNotNull(ValueProperty, TEXT("Parameterized UPROPERTY event member should generate a multicast delegate property")));
		if (NoParamProperty == nullptr || ValueProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(NoParamDelegate->Function, NoParamProperty->SignatureFunction, TEXT("No-param event property should target its generated signature function")));
		ASSERT_THAT(AreEqual(ValueDelegate->Function, ValueProperty->SignatureFunction, TEXT("Parameterized event property should target its generated signature function")));
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(ValueDelegate->Function, TEXT("Value")), TEXT("Parameterized event signature should expose the named parameter")));
	}

	// -------------------------------------------------------------------------
	// Basic multicast delegate: Add, Broadcast, IsBound.
	// -------------------------------------------------------------------------
	TEST_METHOD(MulticastBasics)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMulticastDelegate_Basics"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMulticastDelegateBasics.as"),
			ASTEST_AS(R"AS(
			event void FCoverageMulticastBasicsSignal();

			UCLASS()
			class ACoverageMulticastBasicsActor : AActor
			{
				UPROPERTY()
				int Counter = 0;

				UPROPERTY()
				FCoverageMulticastBasicsSignal OnMulticast;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Test IsBound before adding listeners
					if (!OnMulticast.IsBound())
					{
						Counter = 1;
					}

					// Add listener
					OnMulticast.AddUFunction(this, n"HandleMulticast");

					// Test IsBound after adding
					if (OnMulticast.IsBound())
					{
						Counter = 2;
					}

					// Broadcast
					OnMulticast.Broadcast();

					// Clear all listeners
					OnMulticast.Clear();
					if (!OnMulticast.IsBound())
					{
						Counter = 4;
					}
				}

				UFUNCTION()
				void HandleMulticast()
				{
					Counter = 3;
				}
			}
			)AS"),
			TEXT("ACoverageMulticastBasicsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Multicast-basics actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Multicast-basics actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Counter"), 4, TEXT("Counter should be 4 (all operations completed)"))));
	}

	// -------------------------------------------------------------------------
	// Multiple listeners: all should be called in order when Broadcast is
	// invoked.
	// -------------------------------------------------------------------------
	TEST_METHOD(MulticastMultipleListeners)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMulticastDelegate_MultipleListeners"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMulticastDelegateMultiple.as"),
			ASTEST_AS(R"AS(
			event void FCoverageMulticastMultipleSignal();

			UCLASS()
			class ACoverageMulticastMultipleActor : AActor
			{
				UPROPERTY()
				int Counter = 0;

				UPROPERTY()
				FString Result;

				UPROPERTY()
				FCoverageMulticastMultipleSignal OnMulticast;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Add three listeners
					OnMulticast.AddUFunction(this, n"Listener1");
					OnMulticast.AddUFunction(this, n"Listener2");
					OnMulticast.AddUFunction(this, n"Listener3");

					// Broadcast - all three should be called
					OnMulticast.Broadcast();
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
			TEXT("ACoverageMulticastMultipleActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Multicast-multiple actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Multicast-multiple actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Counter"), 111, TEXT("All three listeners should be called (1+10+100)"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("Result"), FString(TEXT("ABC")), TEXT("Listeners should be called in order"))));
	}

	// -------------------------------------------------------------------------
	// FDelegateHandle management: Add returns a handle, Remove uses handle,
	// IsValid checks handle validity.
	// -------------------------------------------------------------------------
	TEST_METHOD(MulticastHandleManagement)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// AngelScript dynamic multicast delegates (declared with `event`) manage listeners by
		// (object, function-name) pairs through AddUFunction / Unbind / UnbindObject. They do not
		// expose C++ FDelegateHandle-based Add/Remove: FDelegateHandle is not an AS data type and
		// AddUFunction does not return a handle. Captured as a compile boundary instead of a
		// positive handle-management expectation.
		const FString ScriptSource = ASTEST_AS(R"AS(
			event void FCoverageMulticastHandleSignal();

			UCLASS()
			class ACoverageMulticastHandleActor : AActor
			{
				UPROPERTY()
				FCoverageMulticastHandleSignal OnMulticast;

				FDelegateHandle Handle1;

				UFUNCTION()
				void Listener1()
				{
				}

				void TryHandleManagement()
				{
					Handle1 = OnMulticast.AddUFunction(this, n"Listener1");
					OnMulticast.Remove(Handle1);
				}
			}
			)AS");
		const TArray<FString> ExpectedDiagnostics = { TEXT("FDelegateHandle") };
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageMulticastDelegate_Handle"),
			*ScriptSource,
			TEXT("FDelegateHandle-based multicast add/remove is unsupported; AS uses AddUFunction/Unbind by object and function name"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	// -------------------------------------------------------------------------
	// Clear: removes all listeners at once.
	// -------------------------------------------------------------------------
	TEST_METHOD(MulticastClear)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMulticastDelegate_Clear"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMulticastDelegateClear.as"),
			ASTEST_AS(R"AS(
			event void FCoverageMulticastClearSignal();

			UCLASS()
			class ACoverageMulticastClearActor : AActor
			{
				UPROPERTY()
				int Counter = 0;

				UPROPERTY()
				FCoverageMulticastClearSignal OnMulticast;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Add multiple listeners
					OnMulticast.AddUFunction(this, n"Listener1");
					OnMulticast.AddUFunction(this, n"Listener2");
					OnMulticast.AddUFunction(this, n"Listener3");

					// Broadcast - all should be called
					OnMulticast.Broadcast();

					// Clear all listeners
					OnMulticast.Clear();

					// Broadcast again - none should be called
					OnMulticast.Broadcast();

					// Verify not bound
					if (!OnMulticast.IsBound())
					{
						Counter += 1000;
					}
				}

				UFUNCTION()
				void Listener1()
				{
					Counter += 1;
				}

				UFUNCTION()
				void Listener2()
				{
					Counter += 10;
				}

				UFUNCTION()
				void Listener3()
				{
					Counter += 100;
				}
			}
			)AS"),
			TEXT("ACoverageMulticastClearActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Multicast-clear actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Multicast-clear actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Expected: 1 + 10 + 100 (first broadcast) + 1000 (not bound) = 1111
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Counter"), 1111, TEXT("Clear should remove all listeners"))));
	}

	// -------------------------------------------------------------------------
	// Lambda listener boundary: AS exposes AddUFunction/Unbind for multicast
	// delegates, not C++ AddLambda syntax.
	// -------------------------------------------------------------------------
	TEST_METHOD(MulticastLambdaSyntaxIsUnsupported)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			event void FMulticastLambdaUnsupportedSignal();

			UCLASS()
			class ACoverageMulticastLambdaUnsupportedActor : AActor
			{
				UPROPERTY()
				FMulticastLambdaUnsupportedSignal OnSignal;

				UFUNCTION()
				void Handler()
				{
				}

				void TryAddLambda()
				{
					OnSignal.AddLambda(this, n"Handler");
				}
			}
			)AS");

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("AddLambda"));

		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageMulticastDelegate_LambdaUnsupported"),
			*ScriptSource,
			TEXT("AddLambda should remain an explicit unsupported AS-facing boundary"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	// -------------------------------------------------------------------------
	// Multicast delegates with parameters: OneParam, TwoParams.
	// -------------------------------------------------------------------------
	TEST_METHOD(MulticastParameters)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMulticastDelegate_Parameters"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMulticastDelegateParameters.as"),
			ASTEST_AS(R"AS(
			event void FCoverageMcIntSignal(int Value);
			event void FCoverageMcIntStringSignal(int IntValue, FString StringValue);

			UCLASS()
			class ACoverageMulticastParamsActor : AActor
			{
				UPROPERTY()
				int IntSum = 0;

				UPROPERTY()
				FString ConcatenatedString;

				UPROPERTY()
				FCoverageMcIntSignal OnIntMulticast;

				UPROPERTY()
				FCoverageMcIntStringSignal OnIntStringMulticast;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// One parameter
					OnIntMulticast.AddUFunction(this, n"HandleInt1");
					OnIntMulticast.AddUFunction(this, n"HandleInt2");
					OnIntMulticast.Broadcast(50);

					// Two parameters
					OnIntStringMulticast.AddUFunction(this, n"HandleIntString1");
					OnIntStringMulticast.AddUFunction(this, n"HandleIntString2");
					OnIntStringMulticast.Broadcast(10, "Test");
				}

				UFUNCTION()
				void HandleInt1(int Value)
				{
					IntSum += Value;
				}

				UFUNCTION()
				void HandleInt2(int Value)
				{
					IntSum += Value * 2;
				}

				UFUNCTION()
				void HandleIntString1(int IntValue, FString StringValue)
				{
					IntSum += IntValue;
					ConcatenatedString += StringValue;
				}

				UFUNCTION()
				void HandleIntString2(int IntValue, FString StringValue)
				{
					IntSum += IntValue * 3;
					ConcatenatedString += StringValue;
				}
			}
			)AS"),
			TEXT("ACoverageMulticastParamsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Multicast-parameters actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Multicast-parameters actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Expected IntSum: 50 + 100 (first multicast) + 10 + 30 (second multicast) = 190
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntSum"), 190, TEXT("Multicast with parameters should sum correctly"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("ConcatenatedString"), FString(TEXT("TestTest")), TEXT("String parameters should concatenate"))));
	}

	// -------------------------------------------------------------------------
	// RemoveAll: removes all bindings of a specific object/function pair.
	// -------------------------------------------------------------------------
	TEST_METHOD(MulticastRemoveAll)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMulticastDelegate_RemoveAll"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMulticastDelegateRemoveAll.as"),
			ASTEST_AS(R"AS(
			event void FCoverageMulticastRemoveAllSignal();

			UCLASS()
			class ACoverageMulticastRemoveAllActor : AActor
			{
				UPROPERTY()
				int Counter = 0;

				UPROPERTY()
				FCoverageMulticastRemoveAllSignal OnMulticast;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Add the same (object, function) pair multiple times. AS AddUFunction is
					// add-unique for dynamic multicast delegates, so duplicate adds collapse to a
					// single binding rather than stacking three.
					OnMulticast.AddUFunction(this, n"Handler");
					OnMulticast.AddUFunction(this, n"Handler");
					OnMulticast.AddUFunction(this, n"Handler");

					// Add different function
					OnMulticast.AddUFunction(this, n"OtherHandler");

					// Broadcast - Handler (once, deduped) + OtherHandler.
					OnMulticast.Broadcast();

					// Unbind removes the (object, function) binding for Handler.
					OnMulticast.Unbind(this, n"Handler");

					// Broadcast again - only OtherHandler should be called
					OnMulticast.Broadcast();
				}

				UFUNCTION()
				void Handler()
				{
					Counter += 1;
				}

				UFUNCTION()
				void OtherHandler()
				{
					Counter += 100;
				}
			}
			)AS"),
			TEXT("ACoverageMulticastRemoveAllActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Multicast-RemoveAll actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Multicast-RemoveAll actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Expected: 1 (deduped Handler) + 100 (OtherHandler) on first broadcast, then 100
		// (OtherHandler only) on the second broadcast after Unbind = 201.
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Counter"), 201, TEXT("Unbind should remove the Handler binding; AddUFunction is add-unique"))));
	}

	// -------------------------------------------------------------------------
	// Mixed UFUNCTION listener removal: the supported script-facing path is
	// multiple UFUNCTION subscribers plus targeted Unbind.
	// -------------------------------------------------------------------------
	TEST_METHOD(MulticastMixedUFunctionListeners)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMulticastDelegate_MixedUFunction"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMulticastDelegateMixedUFunction.as"),
			ASTEST_AS(R"AS(
			event void FCoverageMixedUFunctionSignal();

			UCLASS()
			class ACoverageMulticastMixedUFunctionActor : AActor
			{
				UPROPERTY()
				int Counter = 0;

				UPROPERTY()
				FCoverageMixedUFunctionSignal OnMulticast;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					OnMulticast.AddUFunction(this, n"Handler");
					OnMulticast.AddUFunction(this, n"Handler2");
					OnMulticast.Broadcast();

					OnMulticast.Unbind(this, n"Handler");
					OnMulticast.Broadcast();
				}

				UFUNCTION()
				void Handler()
				{
					Counter += 1;
				}

				UFUNCTION()
				void Handler2()
				{
					Counter += 10;
				}
			}
			)AS"),
			TEXT("ACoverageMulticastMixedUFunctionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Multicast-mixed-UFUNCTION actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Multicast-mixed-UFUNCTION actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Counter"), 21, TEXT("Multiple UFUNCTION listeners should broadcast, then targeted Unbind should remove one"))));
	}

	// -------------------------------------------------------------------------
	// Multicast object cleanup: UnbindObject removes every listener registered
	// by that target, including different handler functions.
	// -------------------------------------------------------------------------
	TEST_METHOD(MulticastUnbindObjectRemovesTargetListeners)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMulticastDelegate_UnbindObject"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMulticastDelegateUnbindObject.as"),
			ASTEST_AS(R"AS(
			event void FCoverageUnbindObjectSignal();

			UCLASS()
			class ACoverageMulticastUnbindObjectActor : AActor
			{
				UPROPERTY()
				int CountA = 0;

				UPROPERTY()
				int CountB = 0;

				UPROPERTY()
				int CountC = 0;

				UPROPERTY()
				bool WasBoundBeforeUnbind = false;

				UPROPERTY()
				bool WasBoundAfterUnbind = true;

				UPROPERTY()
				FCoverageUnbindObjectSignal OnSignal;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					OnSignal.AddUFunction(this, n"HandlerA");
					OnSignal.AddUFunction(this, n"HandlerB");
					OnSignal.AddUFunction(this, n"HandlerC");
					WasBoundBeforeUnbind = OnSignal.IsBound();

					OnSignal.Broadcast();
					OnSignal.UnbindObject(this);
					WasBoundAfterUnbind = OnSignal.IsBound();
					OnSignal.Broadcast();
				}

				UFUNCTION()
				void HandlerA()
				{
					CountA += 1;
				}

				UFUNCTION()
				void HandlerB()
				{
					CountB += 1;
				}

				UFUNCTION()
				void HandlerC()
				{
					CountC += 1;
				}
			}
			)AS"),
			TEXT("ACoverageMulticastUnbindObjectActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Multicast UnbindObject actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Multicast UnbindObject actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("WasBoundBeforeUnbind"), true, TEXT("Multicast should be bound before UnbindObject"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("WasBoundAfterUnbind"), false, TEXT("UnbindObject should remove all target listeners"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CountA"), 1, TEXT("HandlerA should only run before UnbindObject"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CountB"), 1, TEXT("HandlerB should only run before UnbindObject"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CountC"), 1, TEXT("HandlerC should only run before UnbindObject"))));
	}

	// -------------------------------------------------------------------------
	// Multicast event parameter matrix: primitive, string/name, FVector value and
	// const-ref, UObject handle.
	// -------------------------------------------------------------------------
	TEST_METHOD(MulticastEventParameterTypeMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMulticastDelegate_ParameterTypeMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMulticastDelegateParameterTypeMatrix.as"),
			ASTEST_AS(R"AS(
			event void FCoveragePrimitiveSignal(int Value, bool bEnabled, float Scale);
			event void FCoverageTextSignal(const FString& Text, FName Tag);
			event void FCoverageVectorValueSignal(FVector Location);
			event void FCoverageVectorRefSignal(const FVector& Direction);
			event void FCoverageObjectSignal(AActor ActorValue);

			UCLASS()
			class ACoverageMulticastParameterTypeMatrixActor : AActor
			{
				UPROPERTY()
				int PrimitiveScore = 0;

				UPROPERTY()
				FString TextResult;

				UPROPERTY()
				FName NameResult;

				UPROPERTY()
				FVector VectorValueResult;

				UPROPERTY()
				FVector VectorRefResult;

				UPROPERTY()
				bool bObjectMatched = false;

				UPROPERTY()
				FCoveragePrimitiveSignal OnPrimitive;

				UPROPERTY()
				FCoverageTextSignal OnText;

				UPROPERTY()
				FCoverageVectorValueSignal OnVectorValue;

				UPROPERTY()
				FCoverageVectorRefSignal OnVectorRef;

				UPROPERTY()
				FCoverageObjectSignal OnObject;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					OnPrimitive.AddUFunction(this, n"HandlePrimitive");
					OnText.AddUFunction(this, n"HandleText");
					OnVectorValue.AddUFunction(this, n"HandleVectorValue");
					OnVectorRef.AddUFunction(this, n"HandleVectorRef");
					OnObject.AddUFunction(this, n"HandleObject");

					OnPrimitive.Broadcast(7, true, 2.5f);
					OnText.Broadcast("Score", n"ReadyTag");
					OnVectorValue.Broadcast(FVector(1.0, 2.0, 3.0));
					OnVectorRef.Broadcast(FVector(4.0, 5.0, 6.0));
					OnObject.Broadcast(this);
				}

				UFUNCTION()
				void HandlePrimitive(int Value, bool bEnabled, float Scale)
				{
					PrimitiveScore = Value;
					if (bEnabled)
					{
						PrimitiveScore += 100;
					}
					PrimitiveScore += int(Scale * 10.0);
				}

				UFUNCTION()
				void HandleText(const FString& Text, FName Tag)
				{
					TextResult = Text + "_handled";
					NameResult = Tag;
				}

				UFUNCTION()
				void HandleVectorValue(FVector Location)
				{
					VectorValueResult = Location + FVector(10.0, 10.0, 10.0);
				}

				UFUNCTION()
				void HandleVectorRef(const FVector& Direction)
				{
					VectorRefResult = Direction * 2.0;
				}

				UFUNCTION()
				void HandleObject(AActor ActorValue)
				{
					bObjectMatched = ActorValue == this;
				}
			}
			)AS"),
			TEXT("ACoverageMulticastParameterTypeMatrixActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Multicast event parameter matrix actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		const TSharedPtr<FAngelscriptDelegateDesc> PrimitiveDelegate = Engine.GetDelegate(TEXT("FCoveragePrimitiveSignal"));
		const TSharedPtr<FAngelscriptDelegateDesc> TextDelegate = Engine.GetDelegate(TEXT("FCoverageTextSignal"));
		const TSharedPtr<FAngelscriptDelegateDesc> VectorValueDelegate = Engine.GetDelegate(TEXT("FCoverageVectorValueSignal"));
		const TSharedPtr<FAngelscriptDelegateDesc> VectorRefDelegate = Engine.GetDelegate(TEXT("FCoverageVectorRefSignal"));
		const TSharedPtr<FAngelscriptDelegateDesc> ObjectDelegate = Engine.GetDelegate(TEXT("FCoverageObjectSignal"));
		ASSERT_THAT(IsTrue(PrimitiveDelegate.IsValid(), TEXT("Primitive event metadata should be registered")));
		ASSERT_THAT(IsTrue(TextDelegate.IsValid(), TEXT("Text event metadata should be registered")));
		ASSERT_THAT(IsTrue(VectorValueDelegate.IsValid(), TEXT("Vector value event metadata should be registered")));
		ASSERT_THAT(IsTrue(VectorRefDelegate.IsValid(), TEXT("Vector ref event metadata should be registered")));
		ASSERT_THAT(IsTrue(ObjectDelegate.IsValid(), TEXT("Object event metadata should be registered")));
		if (!PrimitiveDelegate.IsValid() || !TextDelegate.IsValid() || !VectorValueDelegate.IsValid() || !VectorRefDelegate.IsValid() || !ObjectDelegate.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(PrimitiveDelegate->Function, TEXT("Value")), TEXT("Primitive event should expose int parameter")));
		ASSERT_THAT(IsNotNull(FindFProperty<FBoolProperty>(PrimitiveDelegate->Function, TEXT("bEnabled")), TEXT("Primitive event should expose bool parameter")));
		ASSERT_THAT(IsNotNull(FindFProperty<FStrProperty>(TextDelegate->Function, TEXT("Text")), TEXT("Text event should expose FString parameter")));
		ASSERT_THAT(IsNotNull(FindFProperty<FNameProperty>(TextDelegate->Function, TEXT("Tag")), TEXT("Text event should expose FName parameter")));
		ASSERT_THAT(IsNotNull(FindFProperty<FStructProperty>(VectorValueDelegate->Function, TEXT("Location")), TEXT("Vector value event should expose FVector parameter")));
		ASSERT_THAT(IsNotNull(FindFProperty<FStructProperty>(VectorRefDelegate->Function, TEXT("Direction")), TEXT("Vector ref event should expose const FVector& parameter")));
		ASSERT_THAT(IsNotNull(FindFProperty<FObjectProperty>(ObjectDelegate->Function, TEXT("ActorValue")), TEXT("Object event should expose AActor parameter")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Multicast event parameter matrix actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		FVector VectorValueResult = FVector::ZeroVector;
		FVector VectorRefResult = FVector::ZeroVector;
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("PrimitiveScore"), 132, TEXT("Primitive event parameters should cross multicast broadcast"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("TextResult"), FString(TEXT("Score_handled")), TEXT("FString event parameter should cross multicast broadcast"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("NameResult"), FName(TEXT("ReadyTag")), TEXT("FName event parameter should cross multicast broadcast"))));
		ASSERT_THAT(IsTrue(GetStructByPath<FVector>(*TestRunner, Actor, TEXT("VectorValueResult"), VectorValueResult), TEXT("FVector value result should be readable")));
		ASSERT_THAT(IsTrue(VectorValueResult.Equals(FVector(11.0, 12.0, 13.0), 0.001), TEXT("FVector value event parameter should cross multicast broadcast")));
		ASSERT_THAT(IsTrue(GetStructByPath<FVector>(*TestRunner, Actor, TEXT("VectorRefResult"), VectorRefResult), TEXT("FVector ref result should be readable")));
		ASSERT_THAT(IsTrue(VectorRefResult.Equals(FVector(8.0, 10.0, 12.0), 0.001), TEXT("const FVector& event parameter should cross multicast broadcast")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bObjectMatched"), true, TEXT("AActor event parameter should cross multicast broadcast"))));
	}
};

#endif
