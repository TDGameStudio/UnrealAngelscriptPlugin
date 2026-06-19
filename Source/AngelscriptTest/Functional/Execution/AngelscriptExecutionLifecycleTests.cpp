#include "CQTest.h"

#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "ClassGenerator/ASClass.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_Angelscript_AngelscriptExecutionLifecycleTests_Private
{
	static const FName ExecutionDiscardLifecycleModuleName(TEXT("ASExecutionDiscardCleansTypeRegistries"));
	static const FString ExecutionDiscardLifecycleFilename(TEXT("ASExecutionDiscardCleansTypeRegistries.as"));
	static const FName ExecutionDiscardLifecycleClassName(TEXT("ADiscardCarrier"));
	static const FName ExecutionDiscardLifecycleEnumName(TEXT("EDiscardState"));
	static const FName ExecutionDiscardLifecycleDelegateName(TEXT("FDiscardDelegate"));
	static const FName ExecutionDiscardLifecycleGetValueName(TEXT("GetValue"));
	static const FName ExecutionDiscardLifecycleServerFunctionName(TEXT("Server_SetValue"));
	static const FName ExecutionDiscardLifecycleValidateFunctionName(TEXT("Server_SetValue_Validate"));
	static const FString ExecutionDiscardLifecycleRunDecl(TEXT("int Run()"));

	static const FString ExecutionDiscardLifecycleScriptV1 = TEXT(R"AS(
UENUM()
enum class EDiscardState : uint8
{
	Ready = 1
}

delegate void FDiscardDelegate();

UCLASS()
class ADiscardCarrier : AActor
{
	UPROPERTY()
	int Value;

	default Value = 7;

	UFUNCTION()
	int GetValue()
	{
		return Value;
	}

	UFUNCTION(Server, WithValidation)
	void Server_SetValue(int NewValue)
	{
		Value = NewValue;
	}

	UFUNCTION()
	bool Server_SetValue_Validate(int NewValue)
	{
		return NewValue >= 0;
	}
}

int Run()
{
	return int(EDiscardState::Ready) + 41;
}
)AS");

	static const FString ExecutionDiscardLifecycleScriptV2 = TEXT(R"AS(
UENUM()
enum class EDiscardState : uint8
{
	Ready = 2
}

delegate void FDiscardDelegate();

UCLASS()
class ADiscardCarrier : AActor
{
	UPROPERTY()
	int Value;

	default Value = 9;

	UFUNCTION()
	int GetValue()
	{
		return Value;
	}

	UFUNCTION(Server, WithValidation)
	void Server_SetValue(int NewValue)
	{
		Value = NewValue;
	}

	UFUNCTION()
	bool Server_SetValue_Validate(int NewValue)
	{
		return NewValue >= 0;
	}
}

int Run()
{
	return int(EDiscardState::Ready) + 82;
}
)AS");

	struct FExecutionDiscardLifecycleState
	{
		TSharedPtr<FAngelscriptModuleDesc> ModuleRecord;
		TSharedPtr<FAngelscriptClassDesc> ClassDesc;
		TSharedPtr<FAngelscriptEnumDesc> EnumDesc;
		TSharedPtr<FAngelscriptDelegateDesc> DelegateDesc;
		UASClass* GeneratedClass = nullptr;
		UASFunction* GetValueFunction = nullptr;
		UASFunction* ServerFunction = nullptr;
		UFunction* ValidateFunction = nullptr;
		int32 RunResult = 0;
	};

	bool CompileAndCaptureDiscardLifecycleState(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FString& ScriptSource,
		int32 ExpectedRunResult,
		FExecutionDiscardLifecycleState& OutState)
	{
		if (!Test.TestTrue(
				TEXT("Execution.Discard.CleansTypeRegistries should compile the discard lifecycle fixture"),
				CompileAnnotatedModuleFromMemory(
					&Engine,
					ExecutionDiscardLifecycleModuleName,
					ExecutionDiscardLifecycleFilename,
					ScriptSource)))
		{
			return false;
		}

		OutState.ModuleRecord = Engine.GetModuleByModuleName(ExecutionDiscardLifecycleModuleName.ToString());
		OutState.ClassDesc = Engine.GetClass(ExecutionDiscardLifecycleClassName.ToString());
		OutState.EnumDesc = Engine.GetEnum(ExecutionDiscardLifecycleEnumName.ToString());
		OutState.DelegateDesc = Engine.GetDelegate(ExecutionDiscardLifecycleDelegateName.ToString());
		if (!Test.TestTrue(TEXT("Execution.Discard.CleansTypeRegistries should register the compiled module record"), OutState.ModuleRecord.IsValid())
			|| !Test.TestTrue(TEXT("Execution.Discard.CleansTypeRegistries should register the generated class descriptor"), OutState.ClassDesc.IsValid())
			|| !Test.TestTrue(TEXT("Execution.Discard.CleansTypeRegistries should register the generated enum descriptor"), OutState.EnumDesc.IsValid())
			|| !Test.TestTrue(TEXT("Execution.Discard.CleansTypeRegistries should register the generated delegate descriptor"), OutState.DelegateDesc.IsValid()))
		{
			return false;
		}

		if (!Test.TestEqual(TEXT("Execution.Discard.CleansTypeRegistries should track exactly one generated class in the module record"), OutState.ModuleRecord->Classes.Num(), 1)
			|| !Test.TestEqual(TEXT("Execution.Discard.CleansTypeRegistries should track exactly one generated enum in the module record"), OutState.ModuleRecord->Enums.Num(), 1)
			|| !Test.TestEqual(TEXT("Execution.Discard.CleansTypeRegistries should track exactly one generated delegate in the module record"), OutState.ModuleRecord->Delegates.Num(), 1))
		{
			return false;
		}

		OutState.GeneratedClass = Cast<UASClass>(FindGeneratedClass(&Engine, ExecutionDiscardLifecycleClassName));
		if (!Test.TestNotNull(TEXT("Execution.Discard.CleansTypeRegistries should materialize the generated actor class"), OutState.GeneratedClass))
		{
			return false;
		}

		OutState.GetValueFunction = Cast<UASFunction>(FindGeneratedFunction(OutState.GeneratedClass, ExecutionDiscardLifecycleGetValueName));
		OutState.ServerFunction = Cast<UASFunction>(FindGeneratedFunction(OutState.GeneratedClass, ExecutionDiscardLifecycleServerFunctionName));
		OutState.ValidateFunction = FindGeneratedFunction(OutState.GeneratedClass, ExecutionDiscardLifecycleValidateFunctionName);
		if (!Test.TestNotNull(TEXT("Execution.Discard.CleansTypeRegistries should expose the generated GetValue function"), OutState.GetValueFunction)
			|| !Test.TestNotNull(TEXT("Execution.Discard.CleansTypeRegistries should expose the generated RPC function"), OutState.ServerFunction)
			|| !Test.TestNotNull(TEXT("Execution.Discard.CleansTypeRegistries should expose the generated _Validate function"), OutState.ValidateFunction))
		{
			return false;
		}

		if (!Test.TestTrue(TEXT("Execution.Discard.CleansTypeRegistries should bind a live script type before discard"), OutState.GeneratedClass->ScriptTypePtr != nullptr)
			|| !Test.TestTrue(TEXT("Execution.Discard.CleansTypeRegistries should bind a construct function before discard"), OutState.GeneratedClass->ConstructFunction != nullptr)
			|| !Test.TestTrue(TEXT("Execution.Discard.CleansTypeRegistries should bind a defaults function before discard"), OutState.GeneratedClass->DefaultsFunction != nullptr)
			|| !Test.TestTrue(TEXT("Execution.Discard.CleansTypeRegistries should bind a live script function for GetValue before discard"), OutState.GetValueFunction->ScriptFunction != nullptr)
			|| !Test.TestTrue(TEXT("Execution.Discard.CleansTypeRegistries should bind a live script function for the RPC before discard"), OutState.ServerFunction->ScriptFunction != nullptr)
			|| !Test.TestTrue(TEXT("Execution.Discard.CleansTypeRegistries should cache the reflected _Validate function before discard"), OutState.ServerFunction->ValidateFunction == OutState.ValidateFunction)
			|| !Test.TestTrue(TEXT("Execution.Discard.CleansTypeRegistries should return the same reflected _Validate function through the runtime accessor"), OutState.ServerFunction->GetRuntimeValidateFunction() == OutState.ValidateFunction))
		{
			return false;
		}

		if (!Test.TestTrue(
				TEXT("Execution.Discard.CleansTypeRegistries should execute the global entry point before discard"),
				ExecuteIntFunction(
					&Engine,
					ExecutionDiscardLifecycleModuleName,
					ExecutionDiscardLifecycleRunDecl,
					OutState.RunResult)))
		{
			return false;
		}

		return Test.TestEqual(
			TEXT("Execution.Discard.CleansTypeRegistries should produce the expected result before discard"),
			OutState.RunResult,
			ExpectedRunResult);
	}
}


namespace AngelscriptTest_Angelscript_AngelscriptExecutionLifecycleTests_Private
{
bool RunDiscardCleansTypeRegistries(FAutomationTestBase& Test)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ExecutionDiscardLifecycleModuleName.ToString());
		ASTEST_RESET_ENGINE(Engine);
	};

	FExecutionDiscardLifecycleState InitialState;
	if (!CompileAndCaptureDiscardLifecycleState(Test, Engine, ExecutionDiscardLifecycleScriptV1, 42, InitialState))
	{
		return false;
	}

	UASClass* OriginalClass = InitialState.GeneratedClass;
	UASFunction* OriginalGetValueFunction = InitialState.GetValueFunction;
	UASFunction* OriginalServerFunction = InitialState.ServerFunction;
	UFunction* OriginalValidateFunction = InitialState.ValidateFunction;

	if (!Test.TestTrue(TEXT("Execution.Discard.CleansTypeRegistries should discard the compiled module"), Engine.DiscardModule(*ExecutionDiscardLifecycleModuleName.ToString())))
	{
		return false;
	}

	if (!Test.TestTrue(TEXT("Execution.Discard.CleansTypeRegistries should remove the module record after discard"), !Engine.GetModuleByModuleName(ExecutionDiscardLifecycleModuleName.ToString()).IsValid())
		|| !Test.TestTrue(TEXT("Execution.Discard.CleansTypeRegistries should remove the class registry entry after discard"), !Engine.GetClass(ExecutionDiscardLifecycleClassName.ToString()).IsValid())
		|| !Test.TestTrue(TEXT("Execution.Discard.CleansTypeRegistries should remove the enum registry entry after discard"), !Engine.GetEnum(ExecutionDiscardLifecycleEnumName.ToString()).IsValid())
		|| !Test.TestTrue(TEXT("Execution.Discard.CleansTypeRegistries should remove the delegate registry entry after discard"), !Engine.GetDelegate(ExecutionDiscardLifecycleDelegateName.ToString()).IsValid())
		|| !Test.TestTrue(TEXT("Execution.Discard.CleansTypeRegistries should clear ScriptTypePtr on the discarded class"), OriginalClass->ScriptTypePtr == nullptr)
		|| !Test.TestTrue(TEXT("Execution.Discard.CleansTypeRegistries should clear ConstructFunction on the discarded class"), OriginalClass->ConstructFunction == nullptr)
		|| !Test.TestTrue(TEXT("Execution.Discard.CleansTypeRegistries should clear DefaultsFunction on the discarded class"), OriginalClass->DefaultsFunction == nullptr)
		|| !Test.TestTrue(TEXT("Execution.Discard.CleansTypeRegistries should clear ScriptFunction on the discarded GetValue function"), OriginalGetValueFunction->ScriptFunction == nullptr)
		|| !Test.TestTrue(TEXT("Execution.Discard.CleansTypeRegistries should clear ScriptFunction on the discarded RPC function"), OriginalServerFunction->ScriptFunction == nullptr)
		|| !Test.TestTrue(TEXT("Execution.Discard.CleansTypeRegistries should clear ValidateFunction on the discarded RPC function"), OriginalServerFunction->ValidateFunction == nullptr)
		|| !Test.TestTrue(
			TEXT("Execution.Discard.CleansTypeRegistries should clear source metadata on the discarded generated function"),
			OriginalGetValueFunction->GetSourceFilePath().IsEmpty() || OriginalGetValueFunction->GetSourceLineNumber() == -1)
		|| !Test.TestFalse(TEXT("Execution.Discard.CleansTypeRegistries should fail when discarding the same module twice"), Engine.DiscardModule(*ExecutionDiscardLifecycleModuleName.ToString())))
	{
		return false;
	}

	FExecutionDiscardLifecycleState ReloadedState;
	if (!CompileAndCaptureDiscardLifecycleState(Test, Engine, ExecutionDiscardLifecycleScriptV2, 84, ReloadedState))
	{
		return false;
	}

	if (!Test.TestTrue(TEXT("Execution.Discard.CleansTypeRegistries should replace the generated class object on recompile"), ReloadedState.GeneratedClass != OriginalClass)
		|| !Test.TestTrue(TEXT("Execution.Discard.CleansTypeRegistries should replace the generated GetValue function object on recompile"), ReloadedState.GetValueFunction != OriginalGetValueFunction)
		|| !Test.TestTrue(TEXT("Execution.Discard.CleansTypeRegistries should replace the generated RPC function object on recompile"), ReloadedState.ServerFunction != OriginalServerFunction)
		|| !Test.TestTrue(TEXT("Execution.Discard.CleansTypeRegistries should replace the generated _Validate function object on recompile"), ReloadedState.ValidateFunction != OriginalValidateFunction)
		|| !Test.TestTrue(TEXT("Execution.Discard.CleansTypeRegistries should bind a new live script type after recompile"), ReloadedState.GeneratedClass->ScriptTypePtr != nullptr)
		|| !Test.TestTrue(TEXT("Execution.Discard.CleansTypeRegistries should bind a new construct function after recompile"), ReloadedState.GeneratedClass->ConstructFunction != nullptr)
		|| !Test.TestTrue(TEXT("Execution.Discard.CleansTypeRegistries should bind a new defaults function after recompile"), ReloadedState.GeneratedClass->DefaultsFunction != nullptr)
		|| !Test.TestTrue(TEXT("Execution.Discard.CleansTypeRegistries should rebind the RPC validate cache after recompile"), ReloadedState.ServerFunction->ValidateFunction == ReloadedState.ValidateFunction))
	{
		return false;
	}

	}
	return true;
}
}

TEST_CLASS_WITH_FLAGS(
	FAngelscriptExecutionLifecycleTests,
	"Angelscript.TestModule.Functional.Execute.Discard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CleansTypeRegistries)
	{
		ASSERT_THAT(IsTrue(AngelscriptTest_Angelscript_AngelscriptExecutionLifecycleTests_Private::RunDiscardCleansTypeRegistries(*TestRunner)));
	}
};

#endif
