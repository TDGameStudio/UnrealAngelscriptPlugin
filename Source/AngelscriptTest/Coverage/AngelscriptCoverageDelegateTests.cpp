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
//   * DelegateBinding           - BindUFunction; BindLambda is a negative
//                                 AS-facing boundary.
//   * DelegateExecution         - Execute, ExecuteIfBound.
//   * DelegateUnbind            - Unbind, IsBound checks.
//   * DelegateLambda            - C++ lambda syntax and capture forms are
//                                 negative AS-facing boundaries.
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Delegate-basics actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Counter"), 3, TEXT("Counter should be 3 (IsBound checks passed)"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("DelegateWasCalled"), true, TEXT("Delegate should have been executed"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Delegate-parameters actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ReceivedInt"), 100, TEXT("Two-param delegate should set ReceivedInt to 100"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("ReceivedString"), FString(TEXT("Test")), TEXT("Two-param delegate should set ReceivedString"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Delegate-return-value actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolResult"), true, TEXT("Bool return delegate should return true"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntResult"), 100, TEXT("Int return delegate should return doubled value"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Delegate-ExecuteIfBound actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Counter"), 3, TEXT("Counter should be 3 (ExecuteIfBound handled correctly)"))));
	}

	// -------------------------------------------------------------------------
	// Lambda syntax boundary: the current AS fork does not expose BindLambda-
	// style delegate binding from script.
	// -------------------------------------------------------------------------
	TEST_METHOD(DelegateLambdaSyntaxIsUnsupported)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			delegate void FLambdaUnsupportedSignal();

			UCLASS()
			class ACoverageDelegateLambdaUnsupportedActor : AActor
			{
				FLambdaUnsupportedSignal OnSignal;

				UFUNCTION()
				void Handler()
				{
				}

				void TryBindLambda()
				{
					OnSignal.BindLambda(this, n"Handler");
				}
			}
			)AS");

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("No matching signatures to 'FDelegate::BindLambda"));

		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageDelegate_LambdaUnsupported"),
			*ScriptSource,
			TEXT("BindLambda should remain an explicit unsupported AS-facing boundary"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	// -------------------------------------------------------------------------
	// Delegate signatures: parameter count, parameter types, return types, and
	// delegate-as-argument callback flow.
	// -------------------------------------------------------------------------
	TEST_METHOD(DelegateSignatureMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageDelegate_SignatureMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageDelegateSignatureMatrix.as"),
			ASTEST_AS(R"AS(
			UENUM()
			enum ECoverageDelegateState
			{
				Idle,
				Active
			}

			delegate void FSignatureNoParam();
			delegate void FSignatureOneParam(int Value);
			delegate void FSignatureTwoParams(int Value, const FString& Label);
			delegate void FSignatureThreeParams(int Value, float Weight, bool bEnabled);
			delegate void FSignatureFourParams(int Value, float Weight, bool bEnabled, const FString& Label);
			delegate void FSignatureTypeMatrix(int Value, float Weight, bool bEnabled, const FString& Label, FName Tag, FVector Location, const FVector& Direction, AActor ActorValue, ECoverageDelegateState State);
			delegate void FSignatureCallback(int Value);
			delegate bool FSignatureBoolReturn();
			delegate int FSignatureIntReturn(int Value);
			delegate float FSignatureFloatReturn(float Value);
			delegate FString FSignatureStringReturn();
			delegate FVector FSignatureVectorReturn();

			UCLASS()
			class ACoverageDelegateSignatureMatrixActor : AActor
			{
				UPROPERTY()
				int CountResult = 0;

				UPROPERTY()
				int TypeIntValue = 0;

				UPROPERTY()
				float TypeFloatValue = 0.0f;

				UPROPERTY()
				bool TypeBoolValue = false;

				UPROPERTY()
				FString TypeStringValue;

				UPROPERTY()
				FName TypeNameValue;

				UPROPERTY()
				FVector TypeVectorValue;

				UPROPERTY()
				FVector TypeVectorRefValue;

				UPROPERTY()
				AActor TypeActorValue;

				UPROPERTY()
				ECoverageDelegateState TypeEnumValue = ECoverageDelegateState::Idle;

				UPROPERTY()
				bool BoolReturnValue = false;

				UPROPERTY()
				int IntReturnValue = 0;

				UPROPERTY()
				float FloatReturnValue = 0.0f;

				UPROPERTY()
				FString StringReturnValue;

				UPROPERTY()
				FVector VectorReturnValue;

				UPROPERTY()
				int CallbackValue = 0;

				UPROPERTY()
				int ConstRefCallbackValue = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					FSignatureNoParam NoParam;
					FSignatureOneParam OneParam;
					FSignatureTwoParams TwoParams;
					FSignatureThreeParams ThreeParams;
					FSignatureFourParams FourParams;

					NoParam.BindUFunction(this, n"HandleNoParam");
					OneParam.BindUFunction(this, n"HandleOneParam");
					TwoParams.BindUFunction(this, n"HandleTwoParams");
					ThreeParams.BindUFunction(this, n"HandleThreeParams");
					FourParams.BindUFunction(this, n"HandleFourParams");

					NoParam.Execute();
					OneParam.Execute(2);
					TwoParams.Execute(3, "two");
					ThreeParams.Execute(4, 1.5f, true);
					FourParams.Execute(5, 2.5f, false, "four");

					FSignatureTypeMatrix TypeMatrix;
					TypeMatrix.BindUFunction(this, n"HandleTypeMatrix");
					TypeMatrix.Execute(42, 3.5f, true, "Label", n"NameTag", FVector(1.0f, 2.0f, 3.0f), FVector(4.0f, 5.0f, 6.0f), this, ECoverageDelegateState::Active);

					FSignatureBoolReturn BoolReturn;
					FSignatureIntReturn IntReturn;
					FSignatureFloatReturn FloatReturn;
					FSignatureStringReturn StringReturn;
					FSignatureVectorReturn VectorReturn;

					BoolReturn.BindUFunction(this, n"ReturnBool");
					IntReturn.BindUFunction(this, n"ReturnInt");
					FloatReturn.BindUFunction(this, n"ReturnFloat");
					StringReturn.BindUFunction(this, n"ReturnString");
					VectorReturn.BindUFunction(this, n"ReturnVector");

					BoolReturnValue = BoolReturn.Execute();
					IntReturnValue = IntReturn.Execute(12);
					FloatReturnValue = FloatReturn.Execute(2.0f);
					StringReturnValue = StringReturn.Execute();
					VectorReturnValue = VectorReturn.Execute();

					FSignatureCallback Callback;
					Callback.BindUFunction(this, n"HandleCallback");
					UseCallback(Callback);
					UseCallbackConstRef(Callback);
				}

				UFUNCTION()
				void HandleNoParam()
				{
					CountResult += 1;
				}

				UFUNCTION()
				void HandleOneParam(int Value)
				{
					CountResult += Value;
				}

				UFUNCTION()
				void HandleTwoParams(int Value, const FString& Label)
				{
					if (Label == "two")
					{
						CountResult += Value;
					}
				}

				UFUNCTION()
				void HandleThreeParams(int Value, float Weight, bool bEnabled)
				{
					if (bEnabled && Weight == 1.5f)
					{
						CountResult += Value;
					}
				}

				UFUNCTION()
				void HandleFourParams(int Value, float Weight, bool bEnabled, const FString& Label)
				{
					if (!bEnabled && Weight == 2.5f && Label == "four")
					{
						CountResult += Value;
					}
				}

				UFUNCTION()
				void HandleTypeMatrix(int Value, float Weight, bool bEnabled, const FString& Label, FName Tag, FVector Location, const FVector& Direction, AActor ActorValue, ECoverageDelegateState State)
				{
					TypeIntValue = Value;
					TypeFloatValue = Weight;
					TypeBoolValue = bEnabled;
					TypeStringValue = Label;
					TypeNameValue = Tag;
					TypeVectorValue = Location;
					TypeVectorRefValue = Direction;
					TypeActorValue = ActorValue;
					TypeEnumValue = State;
				}

				UFUNCTION()
				bool ReturnBool()
				{
					return true;
				}

				UFUNCTION()
				int ReturnInt(int Value)
				{
					return Value * 3;
				}

				UFUNCTION()
				float ReturnFloat(float Value)
				{
					return Value + 0.75f;
				}

				UFUNCTION()
				FString ReturnString()
				{
					return "delegate-string";
				}

				UFUNCTION()
				FVector ReturnVector()
				{
					return FVector(7.0f, 8.0f, 9.0f);
				}

				void UseCallback(FSignatureCallback Callback)
				{
					Callback.Execute(70);
				}

				void UseCallbackConstRef(const FSignatureCallback&in Callback)
				{
					Callback.Execute(30);
				}

				UFUNCTION()
				void HandleCallback(int Value)
				{
					CallbackValue += Value;
					ConstRefCallbackValue += Value;
				}
			}
			)AS"),
			TEXT("ACoverageDelegateSignatureMatrixActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Delegate-signature-matrix actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Delegate-signature-matrix actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CountResult"), 15, TEXT("Delegate parameter-count matrix should execute 0, 1, 2, 3, and 4 parameter delegates"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("TypeIntValue"), 42, TEXT("Delegate should pass int parameters"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("TypeFloatValue"), 3.5, TEXT("Delegate should pass float parameters"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("TypeBoolValue"), true, TEXT("Delegate should pass bool parameters"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("TypeStringValue"), FString(TEXT("Label")), TEXT("Delegate should pass FString parameters"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("TypeNameValue"), FName(TEXT("NameTag")), TEXT("Delegate should pass FName parameters"))));
		ASSERT_THAT(IsTrue(VerifyStructByPath<FVector>(*TestRunner, Actor, TEXT("TypeVectorValue"), FVector(1.0f, 2.0f, 3.0f), TEXT("Delegate should pass FVector by value"))));
		ASSERT_THAT(IsTrue(VerifyStructByPath<FVector>(*TestRunner, Actor, TEXT("TypeVectorRefValue"), FVector(4.0f, 5.0f, 6.0f), TEXT("Delegate should pass const FVector&"))));
		UObject* TypeActorValue = nullptr;
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("TypeActorValue"), TypeActorValue), TEXT("Delegate UObject parameter should be readable")));
		ASSERT_THAT(AreEqual(static_cast<UObject*>(Actor), TypeActorValue, TEXT("Delegate should pass AActor UObject references")));
		int64 EnumValue = 0;
		ASSERT_THAT(IsTrue(GetEnumByPath(*TestRunner, Actor, TEXT("TypeEnumValue"), EnumValue), TEXT("Delegate enum parameter should be readable")));
		ASSERT_THAT(AreEqual(static_cast<int64>(1), EnumValue, TEXT("Delegate should pass enum parameters")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolReturnValue"), true, TEXT("Delegate bool return should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntReturnValue"), 36, TEXT("Delegate int return should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("FloatReturnValue"), 2.75, TEXT("Delegate float return should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringReturnValue"), FString(TEXT("delegate-string")), TEXT("Delegate FString return should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyStructByPath<FVector>(*TestRunner, Actor, TEXT("VectorReturnValue"), FVector(7.0f, 8.0f, 9.0f), TEXT("Delegate struct return should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CallbackValue"), 100, TEXT("Delegate parameter callback should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ConstRefCallbackValue"), 100, TEXT("const delegate reference callback should execute"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Delegate-rebinding actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Result"), 2, TEXT("Rebinding should replace handler (Result = 2)"))));
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
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Delegate-parameter-types actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntValue"), 42, TEXT("Int parameter should pass through"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FFloatProperty, float>(*TestRunner, Actor, TEXT("FloatValue"), 3.14f, TEXT("Float parameter should pass through"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolValue"), true, TEXT("Bool parameter should pass through"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringValue"), FString(TEXT("Hello")), TEXT("FString parameter should pass through"))));

		// FVector verification
		FVector ExpectedVector(1.0f, 2.0f, 3.0f);
		VerifyStructByPath<FVector>(*TestRunner, Actor, TEXT("VectorValue"), ExpectedVector, TEXT("FVector parameter should pass through"));
	}
};

#endif
