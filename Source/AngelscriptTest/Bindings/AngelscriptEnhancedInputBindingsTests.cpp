// ============================================================================
// AngelscriptEnhancedInputBindingsTests.cpp
//
// Enhanced Input binding coverage — CQTest refactor. Automation ID:
//   Angelscript.TestModule.Bindings.EnhancedInput.FAngelscriptEnhancedInputBindingsTest.*
//
// Sections:
//   InputActionValueMulAssignCompat      — *= chaining preserves value
//   EnhancedInputComponentConstCompat    — const rejection + mutable compilation
//   InputDebugKeyBindingExecuteCompat    — binding handle/execute coexistence
//   InputActionValueConstructorsAndAxisTypes — constructors and axis accessors
//   InputActionValueConvertToType        — ConvertToType dimension preservation
//   EnhancedInputComponentBindActionAcceptsDynamicSignature — BindAction dynamic delegate validation
//   EnhancedInputComponentRemoveBindingCompiles — Clear binding compilation
//   EnhancedInputComponentEditorDelegateFlags — editor delegate flag API
//
// CQTest adaptation notes:
//   Eight legacy automation tests merged into one TEST_CLASS.
//   Each section uses FScopedAngelscriptModule + ExpectGlobalInt where possible.
//   The const-compat test retains its compile-error assertion pattern.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Misc/ScopeExit.h"
#include "UObject/UObjectGlobals.h"

#if WITH_ANGELSCRIPT_UNITTESTS



TEST_CLASS_WITH_FLAGS(FAngelscriptEnhancedInputBindingsTest, "Angelscript.TestModule.Bindings.EnhancedInput",
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

	TEST_METHOD(InputActionValueMulAssign)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASEnhancedInput_MulAssignCompat"), ASTEST_AS(R"AS(
			int MulAssignChaining()
			{
				FInputActionValue Value(2.0f);
				FInputActionValue Delta(1.0f);

				Value.opMulAssign(0.5f).opMulAssign(0.5f);
				if (Value.GetAxis1D() < 0.49f || Value.GetAxis1D() > 0.51f)
				{
					return 0;
				}

				Value += Delta;
				if (Value.GetAxis1D() < 1.49f || Value.GetAxis1D() > 1.51f)
				{
					return 0;
				}

				if (!Value.IsNonZero())
				{
					return 0;
				}

				return 1;
			}
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,
			TEXT("int MulAssignChaining()"),
			TEXT("*= chaining should preserve value and support later +="), 1), TEXT("*= chaining should preserve value and support later +=")));
	}

	TEST_METHOD(EnhancedInputComponentConstAccess)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		struct FConstClearMethodExpectation
		{
			const TCHAR* MethodName = nullptr;
			const TCHAR* ScriptModuleSuffix = nullptr;
			const TCHAR* CallExpression = nullptr;
		};

		static const FConstClearMethodExpectation ConstClearMethods[] =
		{
			{ TEXT("ClearActionEventBindings"), TEXT("ActionEvent"), TEXT("Comp.ClearActionEventBindings();") },
			{ TEXT("ClearActionValueBindings"), TEXT("ActionValue"), TEXT("Comp.ClearActionValueBindings();") },
			{ TEXT("ClearDebugKeyBindings"), TEXT("DebugKey"), TEXT("Comp.ClearDebugKeyBindings();") },
			{ TEXT("ClearActionBindings"), TEXT("ActionBindings"), TEXT("Comp.ClearActionBindings();") },
		};

		for (const FConstClearMethodExpectation& Expectation : ConstClearMethods)
		{
			const FName ModuleName(*FString::Printf(TEXT("ASEnhancedInputComponentConstCompat_%s"), Expectation.ScriptModuleSuffix));
			const FString ScriptFilename = FString::Printf(TEXT("ASEnhancedInputComponentConstCompat_%s.as"), Expectation.ScriptModuleSuffix);
			FString ConstMutationScriptSource = ASTEST_AS(R"AS(
				bool ReadConst(const UEnhancedInputComponent Comp)
				{
					return Comp.HasBindings();
				}

				void MutateConst(const UEnhancedInputComponent Comp)
				{
					$CALL_EXPRESSION$
				}

				int Entry()
				{
					return 1;
				}
				)AS");
			ConstMutationScriptSource.ReplaceInline(TEXT("$CALL_EXPRESSION$"), Expectation.CallExpression, ESearchCase::CaseSensitive);

			FAngelscriptCompileTraceSummary CompileSummary;
			const bool bCompiled = CompileModuleWithSummary(
				&Engine,
				ECompileType::SoftReloadOnly,
				ModuleName,
				ScriptFilename,
				ConstMutationScriptSource,
				false,
				CompileSummary,
				true);
			if (!this->Assert.IsFalse(
				bCompiled,
				FString::Printf(TEXT("const UEnhancedInputComponent should reject %s"), Expectation.MethodName)))
			{
				return;
			}

			ASSERT_THAT(AreEqual(
				ECompileResult::Error,
				CompileSummary.CompileResult,
				FString::Printf(TEXT("Rejecting const %s should produce a regular compile error"), Expectation.MethodName)));
		}

		// Mutable path should compile and execute
		FScopedAngelscriptModule MutableMod(*TestRunner, Engine, TEXT("ASEnhancedInput_MutableCompat"), ASTEST_AS(R"AS(
			bool ReadConst(const UEnhancedInputComponent Comp)
			{
				return Comp.HasBindings();
			}

			bool MutateMutable(UEnhancedInputComponent Comp)
			{
				Comp.ClearActionEventBindings();
				Comp.ClearActionValueBindings();
				Comp.ClearDebugKeyBindings();
				Comp.ClearActionBindings();
				return Comp.HasBindings();
			}

			int MutableEntry()
			{
				return 1;
			}
			)AS"));
		if (!MutableMod.IsValid()) return;
		auto& MM = MutableMod.GetModule();
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, MM,
			TEXT("int MutableEntry()"),
			TEXT("Mutable UEnhancedInputComponent should compile and execute"), 1), TEXT("Mutable UEnhancedInputComponent should compile and execute")));
	}

	TEST_METHOD(InputDebugKeyBindingExecute)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASEnhancedInput_DebugKeyBindingCompat"), ASTEST_AS(R"AS(
			int VerifyBindingCompat(
			FEnhancedInputActionEventBinding& EventBinding,
			FEnhancedInputActionValueBinding& ValueBinding,
			FInputDebugKeyBinding& DebugBinding,
			const FInputActionInstance& ActionInstance,
			const FInputActionValue& ActionValue)
			{
				const uint EventHandle = EventBinding.GetHandle();
				EventBinding.Execute(ActionInstance);

				const uint ValueHandle = ValueBinding.GetHandle();
				const FInputActionValue CurrentValue = ValueBinding.GetValue();

				const uint DebugHandle = DebugBinding.GetHandle();
				DebugBinding.Execute(ActionValue);

				if (CurrentValue.IsNonZero() && EventHandle == ValueHandle && ValueHandle == DebugHandle)
				{
					return 2;
				}

				return 1;
			}

			int DebugKeyEntry()
			{
				return 1;
			}
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,
			TEXT("int DebugKeyEntry()"),
			TEXT("FInputDebugKeyBinding.Execute should coexist with binding handle helpers"), 1), TEXT("FInputDebugKeyBinding.Execute should coexist with binding handle helpers")));
	}

	TEST_METHOD(InputActionValueConstructorsAndAxisTypes)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASEnhancedInput_ConstructorsAxisTypes"), ASTEST_AS(R"AS(
			int VerifyConstructorsAndAxisTypes()
			{
				FInputActionValue Val1D = FInputActionValue(5.0f);
				if (Val1D.GetAxis1D() < 4.9f || Val1D.GetAxis1D() > 5.1f)
				{
					return 0;
				}

				FInputActionValue Val2D = FInputActionValue(FVector2D(3.0f, 4.0f));
				FVector2D V2 = Val2D.GetAxis2D();
				if (V2.X < 2.9f || V2.X > 3.1f)
				{
					return 0;
				}
				if (V2.Y < 3.9f || V2.Y > 4.1f)
				{
					return 0;
				}

				FInputActionValue Val3D = FInputActionValue(FVector(1.0f, 2.0f, 3.0f));
				FVector V3 = Val3D.GetAxis3D();
				if (V3.X < 0.9f || V3.X > 1.1f)
				{
					return 0;
				}
				if (V3.Z < 2.9f || V3.Z > 3.1f)
				{
					return 0;
				}

				return 1;
			}
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,
			TEXT("int VerifyConstructorsAndAxisTypes()"),
			TEXT("FInputActionValue constructors and axis accessors should work correctly"), 1), TEXT("FInputActionValue constructors and axis accessors should work correctly")));
	}

	TEST_METHOD(InputActionValueConvertToType)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASEnhancedInput_ConvertToType"), ASTEST_AS(R"AS(
			int VerifyConvertToType()
			{
				FInputActionValue Val3D = FInputActionValue(FVector(7.0f, 8.0f, 9.0f));

				FInputActionValue Converted1D = Val3D.ConvertToType(EInputActionValueType::Axis1D);
				float Axis1 = Converted1D.GetAxis1D();
				if (Axis1 < 6.9f || Axis1 > 7.1f)
				{
					return 0;
				}

				FInputActionValue Converted2D = Val3D.ConvertToType(EInputActionValueType::Axis2D);
				FVector2D Axis2 = Converted2D.GetAxis2D();
				if (Axis2.X < 6.9f || Axis2.X > 7.1f)
				{
					return 0;
				}

				return 1;
			}
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,
			TEXT("int VerifyConvertToType()"),
			TEXT("ConvertToType should preserve dimension data correctly"), 1), TEXT("ConvertToType should preserve dimension data correctly")));
	}

	TEST_METHOD(EnhancedInputComponentBindActionAcceptsDynamicSignature)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const TCHAR* ModuleName = TEXT("ASAnnotatedEnhancedInputDynamicSignature");
		const bool bCompiled = CompileAnnotatedModuleFromMemory(
			&Engine,
			ModuleName,
			TEXT("ASAnnotatedEnhancedInputDynamicSignature.as"),
			ASTEST_AS(R"AS(
				UCLASS()
				class ABindActionTestActor : AActor
				{
					UPROPERTY()
					UInputAction InputJump;

					UFUNCTION()
					void OnJumpTriggered(FInputActionValue ActionValue, float32 ElapsedTime, float32 TriggeredTime, const UInputAction SourceAction)
					{
					}

					UFUNCTION()
					void SetupInput(UEnhancedInputComponent InputComp)
					{
						FEnhancedInputActionHandlerDynamicSignature Delegate;
						Delegate.BindUFunction(this, n"OnJumpTriggered");
						InputComp.BindAction(InputJump, ETriggerEvent::Triggered, Delegate);
					}
				}

				int BindActionEntry()
				{
					return 1;
				}
				)AS"));
		ON_SCOPE_EXIT { Engine.DiscardModule(ModuleName); };

		if (!this->Assert.IsTrue(bCompiled, TEXT("EnhancedInput dynamic signature module should compile")))
		{
			return;
		}

		UClass* RuntimeActorClass = FindGeneratedClass(&Engine, TEXT("ABindActionTestActor"));
		if (!this->Assert.IsNotNull(RuntimeActorClass, TEXT("EnhancedInput dynamic signature actor class should exist")))
		{
			return;
		}

		AActor* RuntimeActor = NewObject<AActor>(GetTransientPackage(), RuntimeActorClass);
		UEnhancedInputComponent* InputComponent = NewObject<UEnhancedInputComponent>(RuntimeActor);
		UInputAction* InputAction = NewObject<UInputAction>(RuntimeActor);
		if (!this->Assert.IsNotNull(RuntimeActor, TEXT("EnhancedInput dynamic signature actor should instantiate"))
			|| !this->Assert.IsNotNull(InputComponent, TEXT("EnhancedInput component should instantiate"))
			|| !this->Assert.IsNotNull(InputAction, TEXT("InputAction should instantiate")))
		{
			return;
		}

		FObjectPropertyBase* InputJumpProperty = FindFProperty<FObjectPropertyBase>(RuntimeActorClass, TEXT("InputJump"));
		if (!this->Assert.IsNotNull(InputJumpProperty, TEXT("InputJump property should exist")))
		{
			return;
		}
		InputJumpProperty->SetObjectPropertyValue_InContainer(RuntimeActor, InputAction);

		if (!this->Assert.IsNotNull(
			FindGeneratedFunction(RuntimeActorClass, TEXT("SetupInput")),
			TEXT("SetupInput function should exist")))
		{
			return;
		}

		FFunctionInvoker SetupInputInvoker(*TestRunner, RuntimeActor, TEXT("SetupInput"));
		SetupInputInvoker.AddParam<UEnhancedInputComponent*>(InputComponent);
		if (!this->Assert.IsTrue(SetupInputInvoker.Call(), TEXT("SetupInput should execute through generated UFUNCTION dispatch")))
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			InputComponent->HasBindings(),
			TEXT("BindAction should create one action binding for a compatible script UFUNCTION")));

		const TArray<TUniquePtr<FEnhancedInputActionEventBinding>>& Bindings = InputComponent->GetActionEventBindings();
		if (!this->Assert.AreEqual(1, Bindings.Num(), TEXT("BindAction should add one action event binding")))
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			Bindings[0]->GetAction() == InputAction,
			TEXT("BindAction should store the requested input action")));
		ASSERT_THAT(AreEqual(
			ETriggerEvent::Triggered,
			Bindings[0]->GetTriggerEvent(),
			TEXT("BindAction should store the requested trigger event")));
		ASSERT_THAT(IsTrue(
			Bindings[0]->GetUObject() == RuntimeActor,
			TEXT("BindAction should bind the script actor as delegate object")));
		ASSERT_THAT(IsTrue(
			Bindings[0]->IsBoundToObject(RuntimeActor),
			TEXT("BindAction delegate should report the script actor as bound")));
	}

	TEST_METHOD(EnhancedInputComponentRemoveBindingCompiles)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASEnhancedInput_RemoveBinding"), ASTEST_AS(R"AS(
			void VerifyRemoveSignatures(UEnhancedInputComponent Comp)
			{
				Comp.ClearActionEventBindings();
				Comp.ClearActionValueBindings();
				Comp.ClearDebugKeyBindings();
				Comp.ClearActionBindings();
			}

			int RemoveBindingEntry()
			{
				return 1;
			}
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,
			TEXT("int RemoveBindingEntry()"),
			TEXT("Remove/Clear binding signatures should compile"), 1), TEXT("Remove/Clear binding signatures should compile")));
	}

	TEST_METHOD(EnhancedInputComponentEditorDelegateFlags)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASEnhancedInput_EditorDelegateFlags"), ASTEST_AS(R"AS(
			void VerifyEditorDelegateFlags(UEnhancedInputComponent Comp)
			{
				Comp.SetShouldFireDelegatesInEditor(true);
				bool bFires = Comp.ShouldFireDelegatesInEditor();
			}

			int EditorFlagsEntry()
			{
				return 1;
			}
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,
			TEXT("int EditorFlagsEntry()"),
			TEXT("Editor delegate flag API should compile"), 1), TEXT("Editor delegate flag API should compile")));
	}
};

#endif
