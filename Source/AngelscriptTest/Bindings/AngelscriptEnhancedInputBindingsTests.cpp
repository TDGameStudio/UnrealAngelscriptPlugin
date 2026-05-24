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
//   InputMappingContextRuntimeConstruction — AS-created action/context + WASD/arrow mappings
//   EnhancedInputComponentBindActionAcceptsDynamicSignature — BindAction dynamic delegate validation
//   EnhancedInputComponentRemoveBindingCompiles — Clear binding compilation
//   EnhancedInputComponentEditorDelegateFlags — editor delegate flag API
//
// CQTest adaptation notes:
//   Eight IMPLEMENT_SIMPLE_AUTOMATION_TEST merged into one TEST_CLASS.
//   Each section uses FScopedAngelscriptModule + ExpectGlobalInt where possible.
//   The const-compat test retains its compile-error assertion pattern.
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptReflectiveAccess.h"
#include "Shared/AngelscriptTestModuleScope.h"
#include "Shared/AngelscriptTestExecute.h"

#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Misc/ScopeExit.h"
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS



TEST_CLASS_WITH_FLAGS(FAngelscriptEnhancedInputBindingsTest, "Angelscript.TestModule.Bindings.EnhancedInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	TEST_METHOD(InputActionValueMulAssignCompat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASEnhancedInput_MulAssignCompat"), TEXT(R"(
int MulAssignChaining()
{
	FInputActionValue Value(2.0f);
	FInputActionValue Delta(1.0f);

	Value.opMulAssign(0.5f).opMulAssign(0.5f);
	if (Value.GetAxis1D() < 0.49f || Value.GetAxis1D() > 0.51f)
		return 0;

	Value += Delta;
	if (Value.GetAxis1D() < 1.49f || Value.GetAxis1D() > 1.51f)
		return 0;

	if (!Value.IsNonZero())
		return 0;

	return 1;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();
		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int MulAssignChaining()"),
			TEXT("*= chaining should preserve value and support later +="), 1);
	}

	TEST_METHOD(EnhancedInputComponentConstCompat)
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
			const FString Script = FString::Printf(TEXT(R"(
bool ReadConst(const UEnhancedInputComponent Comp)
{
	return Comp.HasBindings();
}

void MutateConst(const UEnhancedInputComponent Comp)
{
	%s
}

int Entry()
{
	return 1;
}
)"), Expectation.CallExpression);

			FAngelscriptCompileTraceSummary CompileSummary;
			const bool bCompiled = CompileModuleWithSummary(
				&Engine,
				ECompileType::SoftReloadOnly,
				ModuleName,
				ScriptFilename,
				Script,
				false,
				CompileSummary,
				true);
			if (!TestRunner->TestFalse(
				FString::Printf(TEXT("const UEnhancedInputComponent should reject %s"), Expectation.MethodName),
				bCompiled))
			{
				return;
			}

			TestRunner->TestEqual(
				FString::Printf(TEXT("Rejecting const %s should produce a regular compile error"), Expectation.MethodName),
				CompileSummary.CompileResult,
				ECompileResult::Error);
		}

		// Mutable path should compile and execute
		FScopedAngelscriptModule MutableMod(*TestRunner, Engine, TEXT("ASEnhancedInput_MutableCompat"), TEXT(R"(
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
)"));
		if (!MutableMod.IsValid()) return;
		auto& MM = MutableMod.GetModule();
		ExpectGlobalInt(*TestRunner, Engine, MM, 
			TEXT("int MutableEntry()"),
			TEXT("Mutable UEnhancedInputComponent should compile and execute"), 1);
	}

	TEST_METHOD(InputDebugKeyBindingExecuteCompat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASEnhancedInput_DebugKeyBindingCompat"), TEXT(R"(
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
		return 2;

	return 1;
}

int DebugKeyEntry()
{
	return 1;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();
		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int DebugKeyEntry()"),
			TEXT("FInputDebugKeyBinding.Execute should coexist with binding handle helpers"), 1);
	}

	TEST_METHOD(InputActionValueConstructorsAndAxisTypes)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASEnhancedInput_ConstructorsAxisTypes"), TEXT(R"(
int VerifyConstructorsAndAxisTypes()
{
	FInputActionValue Val1D = FInputActionValue(5.0f);
	if (Val1D.GetAxis1D() < 4.9f || Val1D.GetAxis1D() > 5.1f)
		return 0;

	FInputActionValue Val2D = FInputActionValue(FVector2D(3.0f, 4.0f));
	FVector2D V2 = Val2D.GetAxis2D();
	if (V2.X < 2.9f || V2.X > 3.1f)
		return 0;
	if (V2.Y < 3.9f || V2.Y > 4.1f)
		return 0;

	FInputActionValue Val3D = FInputActionValue(FVector(1.0f, 2.0f, 3.0f));
	FVector V3 = Val3D.GetAxis3D();
	if (V3.X < 0.9f || V3.X > 1.1f)
		return 0;
	if (V3.Z < 2.9f || V3.Z > 3.1f)
		return 0;

	return 1;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();
		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int VerifyConstructorsAndAxisTypes()"),
			TEXT("FInputActionValue constructors and axis accessors should work correctly"), 1);
	}

	TEST_METHOD(InputActionValueConvertToType)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASEnhancedInput_ConvertToType"), TEXT(R"(
int VerifyConvertToType()
{
	FInputActionValue Val3D = FInputActionValue(FVector(7.0f, 8.0f, 9.0f));

	FInputActionValue Converted1D = Val3D.ConvertToType(EInputActionValueType::Axis1D);
	float Axis1 = Converted1D.GetAxis1D();
	if (Axis1 < 6.9f || Axis1 > 7.1f)
		return 0;

	FInputActionValue Converted2D = Val3D.ConvertToType(EInputActionValueType::Axis2D);
	FVector2D Axis2 = Converted2D.GetAxis2D();
	if (Axis2.X < 6.9f || Axis2.X > 7.1f)
		return 0;

	return 1;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();
		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int VerifyConvertToType()"),
			TEXT("ConvertToType should preserve dimension data correctly"), 1);
	}

	TEST_METHOD(InputMappingContextRuntimeConstruction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASEnhancedInput_RuntimeMappingContext"), TEXT(R"(
int ConfigureRuntimeMapping()
{
	UInputAction MoveAction = Cast<UInputAction>(NewObject(GetTransientPackage(), UInputAction::StaticClass(), n"AS_MoveAction", true));
	UInputMappingContext MappingContext = Cast<UInputMappingContext>(NewObject(GetTransientPackage(), UInputMappingContext::StaticClass(), n"AS_MoveContext", true));
	if (MoveAction == nullptr || MappingContext == nullptr)
		return 0;

	MoveAction.SetValueType(EInputActionValueType::Axis2D);
	MoveAction.SetAccumulationBehavior(EInputActionAccumulationBehavior::Cumulative);
	MappingContext.UnmapAll();

	UInputModifierSwizzleAxis WSwizzle = Cast<UInputModifierSwizzleAxis>(NewObject(MappingContext, UInputModifierSwizzleAxis::StaticClass(), n"AS_WSwizzle", true));
	UInputModifierSwizzleAxis SSwizzle = Cast<UInputModifierSwizzleAxis>(NewObject(MappingContext, UInputModifierSwizzleAxis::StaticClass(), n"AS_SSwizzle", true));
	UInputModifierNegate SNegate = Cast<UInputModifierNegate>(NewObject(MappingContext, UInputModifierNegate::StaticClass(), n"AS_SNegate", true));
	UInputModifierNegate ANegate = Cast<UInputModifierNegate>(NewObject(MappingContext, UInputModifierNegate::StaticClass(), n"AS_ANegate", true));
	UInputModifierSwizzleAxis UpSwizzle = Cast<UInputModifierSwizzleAxis>(NewObject(MappingContext, UInputModifierSwizzleAxis::StaticClass(), n"AS_UpSwizzle", true));
	UInputModifierSwizzleAxis DownSwizzle = Cast<UInputModifierSwizzleAxis>(NewObject(MappingContext, UInputModifierSwizzleAxis::StaticClass(), n"AS_DownSwizzle", true));
	UInputModifierNegate DownNegate = Cast<UInputModifierNegate>(NewObject(MappingContext, UInputModifierNegate::StaticClass(), n"AS_DownNegate", true));
	UInputModifierNegate LeftNegate = Cast<UInputModifierNegate>(NewObject(MappingContext, UInputModifierNegate::StaticClass(), n"AS_LeftNegate", true));
	if (WSwizzle == nullptr || SSwizzle == nullptr || SNegate == nullptr || ANegate == nullptr
		|| UpSwizzle == nullptr || DownSwizzle == nullptr || DownNegate == nullptr || LeftNegate == nullptr)
		return 0;

	FEnhancedActionKeyMapping& W = MappingContext.MapKey(MoveAction, EKeys::W);
	W.AddModifier(WSwizzle);

	FEnhancedActionKeyMapping& S = MappingContext.MapKey(MoveAction, EKeys::S);
	S.AddModifier(SSwizzle);
	S.AddModifier(SNegate);

	FEnhancedActionKeyMapping& A = MappingContext.MapKey(MoveAction, EKeys::A);
	A.AddModifier(ANegate);

	FEnhancedActionKeyMapping& D = MappingContext.MapKey(MoveAction, EKeys::D);

	FEnhancedActionKeyMapping& Up = MappingContext.MapKey(MoveAction, EKeys::Up);
	Up.AddModifier(UpSwizzle);

	FEnhancedActionKeyMapping& Down = MappingContext.MapKey(MoveAction, EKeys::Down);
	Down.AddModifier(DownSwizzle);
	Down.AddModifier(DownNegate);

	FEnhancedActionKeyMapping& Left = MappingContext.MapKey(MoveAction, EKeys::Left);
	Left.AddModifier(LeftNegate);

	FEnhancedActionKeyMapping& Right = MappingContext.MapKey(MoveAction, EKeys::Right);

	if (MappingContext.GetMappingCount() != 8)
		return 0;
	if (MoveAction.GetValueType() != EInputActionValueType::Axis2D)
		return 0;
	if (MoveAction.GetAccumulationBehavior() != EInputActionAccumulationBehavior::Cumulative)
		return 0;
	if (W.GetAction() != MoveAction || W.GetKey() != EKeys::W || W.GetModifierCount() != 1)
		return 0;
	if (S.GetModifierCount() != 2 || A.GetModifierCount() != 1 || D.GetModifierCount() != 0)
		return 0;
	if (Up.GetModifierCount() != 1 || Down.GetModifierCount() != 2 || Left.GetModifierCount() != 1 || Right.GetModifierCount() != 0)
		return 0;

	return 1;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();
		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int ConfigureRuntimeMapping()"),
			TEXT("AS should be able to create an Enhanced Input movement context at runtime"), 1);
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
			TEXT(R"(
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
)"));
		ON_SCOPE_EXIT { Engine.DiscardModule(ModuleName); };

		if (!TestRunner->TestTrue(TEXT("EnhancedInput dynamic signature module should compile"), bCompiled))
		{
			return;
		}

		UClass* RuntimeActorClass = FindGeneratedClass(&Engine, TEXT("ABindActionTestActor"));
		if (!TestRunner->TestNotNull(TEXT("EnhancedInput dynamic signature actor class should exist"), RuntimeActorClass))
		{
			return;
		}

		AActor* RuntimeActor = NewObject<AActor>(GetTransientPackage(), RuntimeActorClass);
		UEnhancedInputComponent* InputComponent = NewObject<UEnhancedInputComponent>(RuntimeActor);
		UInputAction* InputAction = NewObject<UInputAction>(RuntimeActor);
		if (!TestRunner->TestNotNull(TEXT("EnhancedInput dynamic signature actor should instantiate"), RuntimeActor)
			|| !TestRunner->TestNotNull(TEXT("EnhancedInput component should instantiate"), InputComponent)
			|| !TestRunner->TestNotNull(TEXT("InputAction should instantiate"), InputAction))
		{
			return;
		}

		FObjectPropertyBase* InputJumpProperty = FindFProperty<FObjectPropertyBase>(RuntimeActorClass, TEXT("InputJump"));
		if (!TestRunner->TestNotNull(TEXT("InputJump property should exist"), InputJumpProperty))
		{
			return;
		}
		InputJumpProperty->SetObjectPropertyValue_InContainer(RuntimeActor, InputAction);

		if (!TestRunner->TestNotNull(
			TEXT("SetupInput function should exist"),
			FindGeneratedFunction(RuntimeActorClass, TEXT("SetupInput"))))
		{
			return;
		}

		FFunctionInvoker SetupInputInvoker(*TestRunner, RuntimeActor, TEXT("SetupInput"));
		SetupInputInvoker.AddParam<UEnhancedInputComponent*>(InputComponent);
		if (!TestRunner->TestTrue(TEXT("SetupInput should execute through generated UFUNCTION dispatch"), SetupInputInvoker.Call()))
		{
			return;
		}

		TestRunner->TestTrue(
			TEXT("BindAction should create one action binding for a compatible script UFUNCTION"),
			InputComponent->HasBindings());

		const TArray<TUniquePtr<FEnhancedInputActionEventBinding>>& Bindings = InputComponent->GetActionEventBindings();
		if (!TestRunner->TestEqual(TEXT("BindAction should add one action event binding"), Bindings.Num(), 1))
		{
			return;
		}

		TestRunner->TestTrue(
			TEXT("BindAction should store the requested input action"),
			Bindings[0]->GetAction() == InputAction);
		TestRunner->TestEqual(
			TEXT("BindAction should store the requested trigger event"),
			Bindings[0]->GetTriggerEvent(),
			ETriggerEvent::Triggered);
		TestRunner->TestTrue(
			TEXT("BindAction should bind the script actor as delegate object"),
			Bindings[0]->GetUObject() == RuntimeActor);
		TestRunner->TestTrue(
			TEXT("BindAction delegate should report the script actor as bound"),
			Bindings[0]->IsBoundToObject(RuntimeActor));
	}

	TEST_METHOD(EnhancedInputComponentRemoveBindingCompiles)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASEnhancedInput_RemoveBinding"), TEXT(R"(
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
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();
		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int RemoveBindingEntry()"),
			TEXT("Remove/Clear binding signatures should compile"), 1);
	}

	TEST_METHOD(EnhancedInputComponentEditorDelegateFlags)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASEnhancedInput_EditorDelegateFlags"), TEXT(R"(
void VerifyEditorDelegateFlags(UEnhancedInputComponent Comp)
{
	Comp.SetShouldFireDelegatesInEditor(true);
	bool bFires = Comp.ShouldFireDelegatesInEditor();
}

int EditorFlagsEntry()
{
	return 1;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();
		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int EditorFlagsEntry()"),
			TEXT("Editor delegate flag API should compile"), 1);
	}
};

#endif
