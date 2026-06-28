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
// second slice of the delegates-and-events matrix (Documents/Coverage/Coverage_DelegatesAndEvents.md
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
// Detailed coverage matrix: Documents/Coverage/Coverage_DelegatesAndEvents.md
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
			UCLASS()
			class ACoverageMulticastBasicsActor : AActor
			{
				UPROPERTY()
				int Counter = 0;

				FSimpleMulticastDelegate OnMulticast;

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
			UCLASS()
			class ACoverageMulticastMultipleActor : AActor
			{
				UPROPERTY()
				int Counter = 0;

				UPROPERTY()
				FString Result;

				FSimpleMulticastDelegate OnMulticast;

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

		static const FName ModuleName(TEXT("ASCoverageMulticastDelegate_Handle"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMulticastDelegateHandle.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageMulticastHandleActor : AActor
			{
				UPROPERTY()
				int Counter = 0;

				FSimpleMulticastDelegate OnMulticast;
				FDelegateHandle Handle1;
				FDelegateHandle Handle2;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Add two listeners and save handles
					Handle1 = OnMulticast.AddUFunction(this, n"Listener1");
					Handle2 = OnMulticast.AddUFunction(this, n"Listener2");

					// Test handle validity
					if (Handle1.IsValid() && Handle2.IsValid())
					{
						Counter = 1;
					}

					// Broadcast - both should be called
					OnMulticast.Broadcast();

					// Remove first listener by handle
					OnMulticast.Remove(Handle1);

					// Broadcast again - only second should be called
					OnMulticast.Broadcast();

					// Remove second listener by handle
					OnMulticast.Remove(Handle2);

					// Test IsBound after removing all
					if (!OnMulticast.IsBound())
					{
						Counter += 1000;
					}
				}

				UFUNCTION()
				void Listener1()
				{
					Counter += 10;
				}

				UFUNCTION()
				void Listener2()
				{
					Counter += 100;
				}
			}
			)AS"),
			TEXT("ACoverageMulticastHandleActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Multicast-handle actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Multicast-handle actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Expected: 1 (initial) + 10 + 100 (first broadcast) + 100 (second broadcast) + 1000 (not bound) = 1211
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Counter"), 1211, TEXT("Handle management should work correctly"))));
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
			UCLASS()
			class ACoverageMulticastClearActor : AActor
			{
				UPROPERTY()
				int Counter = 0;

				FSimpleMulticastDelegate OnMulticast;

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
		ExpectedDiagnostics.Add(TEXT("No matching signatures to 'FMulticastDelegate::AddLambda"));

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
			UCLASS()
			class ACoverageMulticastParamsActor : AActor
			{
				UPROPERTY()
				int IntSum = 0;

				UPROPERTY()
				FString ConcatenatedString;

				FIntMulticastDelegate OnIntMulticast;
				FIntStringMulticastDelegate OnIntStringMulticast;

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
			UCLASS()
			class ACoverageMulticastRemoveAllActor : AActor
			{
				UPROPERTY()
				int Counter = 0;

				FSimpleMulticastDelegate OnMulticast;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Add same function multiple times
					OnMulticast.AddUFunction(this, n"Handler");
					OnMulticast.AddUFunction(this, n"Handler");
					OnMulticast.AddUFunction(this, n"Handler");

					// Add different function
					OnMulticast.AddUFunction(this, n"OtherHandler");

					// Broadcast - all four should be called
					OnMulticast.Broadcast();

					// RemoveAll for Handler
					OnMulticast.RemoveAll(this, n"Handler");

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

		// Expected: 3 + 100 (first broadcast) + 100 (second broadcast) = 203
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Counter"), 203, TEXT("RemoveAll should remove all bindings of specified function"))));
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
};

#endif
