#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageDelegateTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript single-cast delegate (FDelegate) usage, the first
// slice of the delegates-and-events matrix (Documents/Coverage/Coverage_DelegatesAndEvents.md
// section 1). Each TEST_METHOD walks one usage axis from the matrix:
//
// Axes covered here:
//   * DelegateDeclaration       - DECLARE_DELEGATE variants (no params, with
//                                 params, return value, return value + params).
//   * DelegateBinding           - BindUFunction, BindLambda.
//   * DelegateExecution         - Execute, ExecuteIfBound.
//   * DelegateUnbind            - Unbind, IsBound checks.
//   * DelegateLambda            - Lambda captures (this, variables).
//
// Pattern D (script execution) from the Angelscript test guide: compile AS
// actors, spawn them, drive delegate operations, verify results through
// properties or return values.
//
// Detailed coverage matrix: Documents/Coverage/Coverage_DelegatesAndEvents.md
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageDelegateTest,
	"Angelscript.TestModule.Coverage.Delegate",
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
	// Basic delegate declaration and usage: no parameters, simple binding and
	// execution.
	// -------------------------------------------------------------------------
	TEST_METHOD(DelegateBasics)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDelegate_Basics"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDelegateBasics.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageDelegateBasicsActor : AActor
			{
				UPROPERTY()
				int Counter = 0;

				UPROPERTY()
				bool DelegateWasCalled = false;

				FSimpleDelegate OnSimpleDelegate;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Test IsBound before binding
					if (!OnSimpleDelegate.IsBound())
					{
						Counter = 1;
					}

					// Bind to member function
					OnSimpleDelegate.BindUFunction(this, n"HandleSimpleDelegate");

					// Test IsBound after binding
					if (OnSimpleDelegate.IsBound())
					{
						Counter = 2;
					}

					// Execute delegate
					OnSimpleDelegate.Execute();

					// Test Unbind
					OnSimpleDelegate.Unbind();
					if (!OnSimpleDelegate.IsBound())
					{
						Counter = 3;
					}
				}

				UFUNCTION()
				void HandleSimpleDelegate()
				{
					DelegateWasCalled = true;
				}
			}
			)AS"),
			TEXT("ACoverageDelegateBasicsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Delegate-basics actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Delegate-basics actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Counter"), 3, TEXT("Counter should be 3 (IsBound checks passed)"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("DelegateWasCalled"), true, TEXT("Delegate should have been executed"));
	}

	// -------------------------------------------------------------------------
	// Delegate with parameters: OneParam, TwoParams variants.
	// -------------------------------------------------------------------------
	TEST_METHOD(DelegateParameters)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDelegate_Parameters"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDelegateParameters.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageDelegateParamsActor : AActor
			{
				UPROPERTY()
				int ReceivedInt = 0;

				UPROPERTY()
				FString ReceivedString;

				FIntDelegate OnIntDelegate;
				FIntStringDelegate OnIntStringDelegate;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// One parameter delegate
					OnIntDelegate.BindUFunction(this, n"HandleIntDelegate");
					OnIntDelegate.Execute(42);

					// Two parameters delegate
					OnIntStringDelegate.BindUFunction(this, n"HandleIntStringDelegate");
					OnIntStringDelegate.Execute(100, "Test");
				}

				UFUNCTION()
				void HandleIntDelegate(int Value)
				{
					ReceivedInt = Value;
				}

				UFUNCTION()
				void HandleIntStringDelegate(int IntValue, FString StringValue)
				{
					ReceivedInt = IntValue;
					ReceivedString = StringValue;
				}
			}
			)AS"),
			TEXT("ACoverageDelegateParamsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Delegate-parameters actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Delegate-parameters actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ReceivedInt"), 100, TEXT("Two-param delegate should set ReceivedInt to 100"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("ReceivedString"), FString(TEXT("Test")), TEXT("Two-param delegate should set ReceivedString"));
	}

	// -------------------------------------------------------------------------
	// Delegate with return value: RetVal, RetVal_OneParam variants.
	// -------------------------------------------------------------------------
	TEST_METHOD(DelegateReturnValue)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDelegate_ReturnValue"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDelegateReturnValue.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageDelegateRetValActor : AActor
			{
				UPROPERTY()
				bool BoolResult = false;

				UPROPERTY()
				int IntResult = 0;

				FBoolRetDelegate OnBoolRetDelegate;
				FIntRetIntDelegate OnIntRetIntDelegate;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Return value only
					OnBoolRetDelegate.BindUFunction(this, n"HandleBoolRetDelegate");
					BoolResult = OnBoolRetDelegate.Execute();

					// Return value + parameter
					OnIntRetIntDelegate.BindUFunction(this, n"HandleIntRetIntDelegate");
					IntResult = OnIntRetIntDelegate.Execute(50);
				}

				UFUNCTION()
				bool HandleBoolRetDelegate()
				{
					return true;
				}

				UFUNCTION()
				int HandleIntRetIntDelegate(int Value)
				{
					return Value * 2;
				}
			}
			)AS"),
			TEXT("ACoverageDelegateRetValActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Delegate-return-value actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Delegate-return-value actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolResult"), true, TEXT("Bool return delegate should return true"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntResult"), 100, TEXT("Int return delegate should return doubled value"));
	}

	// -------------------------------------------------------------------------
	// ExecuteIfBound: safe execution that does nothing when delegate is not
	// bound.
	// -------------------------------------------------------------------------
	TEST_METHOD(DelegateExecuteIfBound)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDelegate_ExecuteIfBound"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDelegateExecuteIfBound.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageDelegateExecuteIfBoundActor : AActor
			{
				UPROPERTY()
				int Counter = 0;

				FSimpleDelegate OnDelegate;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// ExecuteIfBound on unbound delegate - should do nothing
					OnDelegate.ExecuteIfBound();
					Counter = 1;

					// Bind and ExecuteIfBound - should execute
					OnDelegate.BindUFunction(this, n"HandleDelegate");
					OnDelegate.ExecuteIfBound();

					// Unbind and ExecuteIfBound again - should do nothing
					OnDelegate.Unbind();
					OnDelegate.ExecuteIfBound();
					Counter = 3;
				}

				UFUNCTION()
				void HandleDelegate()
				{
					Counter = 2;
				}
			}
			)AS"),
			TEXT("ACoverageDelegateExecuteIfBoundActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Delegate-ExecuteIfBound actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Delegate-ExecuteIfBound actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Counter"), 3, TEXT("Counter should be 3 (ExecuteIfBound handled correctly)"));
	}

	// -------------------------------------------------------------------------
	// Lambda binding: BindLambda with various capture modes.
	// -------------------------------------------------------------------------
	TEST_METHOD(DelegateLambda)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDelegate_Lambda"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDelegateLambda.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageDelegateLambdaActor : AActor
			{
				UPROPERTY()
				int Counter = 0;

				UPROPERTY()
				int CapturedValue = 0;

				FSimpleDelegate OnSimpleLambda;
				FIntDelegate OnIntLambda;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Lambda without capture
					OnSimpleLambda.BindLambda([](){
						// Simple lambda execution
					});
					if (OnSimpleLambda.IsBound())
					{
						Counter++;
					}
					OnSimpleLambda.Execute();

					// Lambda with [this] capture
					OnSimpleLambda.BindLambda([this](){
						Counter = 10;
					});
					OnSimpleLambda.Execute();

					// Lambda with parameter and [this] capture
					int LocalValue = 100;
					OnIntLambda.BindLambda([this, LocalValue](int Param){
						CapturedValue = LocalValue + Param;
					});
					OnIntLambda.Execute(50);
				}
			}
			)AS"),
			TEXT("ACoverageDelegateLambdaActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Delegate-lambda actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Delegate-lambda actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Counter"), 10, TEXT("Lambda with [this] capture should set Counter to 10"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CapturedValue"), 150, TEXT("Lambda with captures should compute 100 + 50"));
	}

	// -------------------------------------------------------------------------
	// Delegate rebinding: binding a new function to an already-bound delegate
	// should replace the previous binding.
	// -------------------------------------------------------------------------
	TEST_METHOD(DelegateRebinding)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDelegate_Rebinding"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDelegateRebinding.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageDelegateRebindingActor : AActor
			{
				UPROPERTY()
				int Result = 0;

				FSimpleDelegate OnDelegate;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// First binding
					OnDelegate.BindUFunction(this, n"Handler1");
					OnDelegate.Execute();

					// Rebind to different function - should replace
					OnDelegate.BindUFunction(this, n"Handler2");
					OnDelegate.Execute();
				}

				UFUNCTION()
				void Handler1()
				{
					Result = 1;
				}

				UFUNCTION()
				void Handler2()
				{
					Result = 2;
				}
			}
			)AS"),
			TEXT("ACoverageDelegateRebindingActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Delegate-rebinding actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Delegate-rebinding actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Result"), 2, TEXT("Rebinding should replace handler (Result = 2)"));
	}

	// -------------------------------------------------------------------------
	// Various parameter types: primitives, FString, FVector, struct references.
	// -------------------------------------------------------------------------
	TEST_METHOD(DelegateParameterTypes)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDelegate_ParameterTypes"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDelegateParameterTypes.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageDelegateParamTypesActor : AActor
			{
				UPROPERTY()
				int IntValue = 0;

				UPROPERTY()
				float FloatValue = 0.0f;

				UPROPERTY()
				bool BoolValue = false;

				UPROPERTY()
				FString StringValue;

				UPROPERTY()
				FVector VectorValue;

				FIntFloatBoolDelegate OnIntFloatBoolDelegate;
				FStringDelegate OnStringDelegate;
				FVectorDelegate OnVectorDelegate;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Multiple primitive types
					OnIntFloatBoolDelegate.BindUFunction(this, n"HandleIntFloatBool");
					OnIntFloatBoolDelegate.Execute(42, 3.14f, true);

					// FString
					OnStringDelegate.BindUFunction(this, n"HandleString");
					OnStringDelegate.Execute("Hello");

					// FVector (struct by value)
					OnVectorDelegate.BindUFunction(this, n"HandleVector");
					OnVectorDelegate.Execute(FVector(1.0f, 2.0f, 3.0f));
				}

				UFUNCTION()
				void HandleIntFloatBool(int I, float F, bool B)
				{
					IntValue = I;
					FloatValue = F;
					BoolValue = B;
				}

				UFUNCTION()
				void HandleString(FString S)
				{
					StringValue = S;
				}

				UFUNCTION()
				void HandleVector(FVector V)
				{
					VectorValue = V;
				}
			}
			)AS"),
			TEXT("ACoverageDelegateParamTypesActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Delegate-parameter-types actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Delegate-parameter-types actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntValue"), 42, TEXT("Int parameter should pass through"));
		VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("FloatValue"), 3.14f, TEXT("Float parameter should pass through"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolValue"), true, TEXT("Bool parameter should pass through"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringValue"), FString(TEXT("Hello")), TEXT("FString parameter should pass through"));

		// FVector verification
		FVector ExpectedVector(1.0f, 2.0f, 3.0f);
		VerifyStructByPath<FVector>(*TestRunner, Actor, TEXT("VectorValue"), ExpectedVector, TEXT("FVector parameter should pass through"));
	}
};

#endif
