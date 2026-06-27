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
//   * MulticastBinding          - AddUFunction, AddLambda, multiple listeners.
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

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Multicast-basics actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Counter"), 4, TEXT("Counter should be 4 (all operations completed)"));
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

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Multicast-multiple actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Counter"), 111, TEXT("All three listeners should be called (1+10+100)"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("Result"), FString(TEXT("ABC")), TEXT("Listeners should be called in order"));
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

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Multicast-handle actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Expected: 1 (initial) + 10 + 100 (first broadcast) + 100 (second broadcast) + 1000 (not bound) = 1211
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Counter"), 1211, TEXT("Handle management should work correctly"));
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

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Multicast-clear actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Expected: 1 + 10 + 100 (first broadcast) + 1000 (not bound) = 1111
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Counter"), 1111, TEXT("Clear should remove all listeners"));
	}

	// -------------------------------------------------------------------------
	// Lambda listeners: AddLambda with various capture modes.
	// -------------------------------------------------------------------------
	TEST_METHOD(MulticastLambda)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMulticastDelegate_Lambda"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMulticastDelegateLambda.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageMulticastLambdaActor : AActor
			{
				UPROPERTY()
				int Counter = 0;

				FSimpleMulticastDelegate OnMulticast;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Lambda without capture
					OnMulticast.AddLambda([](){
						// Simple lambda
					});

					// Lambda with [this] capture
					OnMulticast.AddLambda([this](){
						Counter += 10;
					});

					// Lambda with value capture
					int LocalValue = 100;
					OnMulticast.AddLambda([this, LocalValue](){
						Counter += LocalValue;
					});

					// Broadcast - all lambdas should be called
					OnMulticast.Broadcast();
				}
			}
			)AS"),
			TEXT("ACoverageMulticastLambdaActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Multicast-lambda actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Multicast-lambda actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Expected: 10 + 100 = 110
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Counter"), 110, TEXT("Lambda listeners should be called correctly"));
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

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Multicast-parameters actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Expected IntSum: 50 + 100 (first multicast) + 10 + 30 (second multicast) = 190
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntSum"), 190, TEXT("Multicast with parameters should sum correctly"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("ConcatenatedString"), FString(TEXT("TestTest")), TEXT("String parameters should concatenate"));
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

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Multicast-RemoveAll actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Expected: 3 + 100 (first broadcast) + 100 (second broadcast) = 203
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Counter"), 203, TEXT("RemoveAll should remove all bindings of specified function"));
	}

	// -------------------------------------------------------------------------
	// Mixed listeners: combination of UFunctions and Lambdas.
	// -------------------------------------------------------------------------
	TEST_METHOD(MulticastMixedListeners)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMulticastDelegate_Mixed"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMulticastDelegateMixed.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageMulticastMixedActor : AActor
			{
				UPROPERTY()
				int Counter = 0;

				FSimpleMulticastDelegate OnMulticast;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Add UFUNCTION
					OnMulticast.AddUFunction(this, n"Handler");

					// Add Lambda
					OnMulticast.AddLambda([this](){
						Counter += 10;
					});

					// Add another UFUNCTION
					OnMulticast.AddUFunction(this, n"Handler2");

					// Add another Lambda
					OnMulticast.AddLambda([this](){
						Counter += 100;
					});

					// Broadcast - all four should be called
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
					Counter += 1000;
				}
			}
			)AS"),
			TEXT("ACoverageMulticastMixedActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Multicast-mixed actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Multicast-mixed actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Expected: 1 + 10 + 1000 + 100 = 1111
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Counter"), 1111, TEXT("Mixed UFUNCTION and Lambda listeners should all be called"));
	}
};

#endif
