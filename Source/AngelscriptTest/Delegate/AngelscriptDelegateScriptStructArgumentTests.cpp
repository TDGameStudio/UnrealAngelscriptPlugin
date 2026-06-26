#include "CQTest.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"

#include "Misc/ScopeExit.h"

TEST_CLASS_WITH_FLAGS(FAngelscriptDelegateScriptStructArgumentTests,
	"Angelscript.TestModule.Delegate.ScriptStructArguments",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName ScriptStructArgumentModuleName = FName(TEXT("DelegateScriptStructArgument"));
	inline static const FString ScriptStructArgumentFilename = FString(TEXT("DelegateScriptStructArgument.as"));

	static asIScriptModule* FindScriptModule(FAngelscriptEngine& Engine, FName ModuleName)
	{
		const TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModule(ModuleName.ToString());
		return ModuleDesc.IsValid() ? ModuleDesc->ScriptModule : nullptr;
	}

public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(ExecutesDelegateWithScriptStructValueArgument)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ScriptStructArgumentModuleName.ToString());
		};

		// Script under test: executing this delegate constructs an AS USTRUCT inside the event argument buffer.
		const FString ScriptSource = ASTEST_AS(R"AS(
			USTRUCT()
			struct FDelegateScriptStructPayload
			{
				UPROPERTY()
				int Value = 0;

				UPROPERTY()
				int Bonus = 0;
			}

			delegate int FDelegateScriptStructSignal(FDelegateScriptStructPayload Payload);

			UCLASS()
			class UDelegateScriptStructReceiver : UObject
			{
				UFUNCTION()
				int HandlePayload(FDelegateScriptStructPayload Payload)
				{
					return Payload.Value + Payload.Bonus;
				}
			}

			int RunScriptStructDelegate()
			{
				FDelegateScriptStructPayload Payload;
				Payload.Value = 19;
				Payload.Bonus = 23;

				UDelegateScriptStructReceiver Receiver = UDelegateScriptStructReceiver();
				FDelegateScriptStructSignal Signal;
				Signal.BindUFunction(Receiver, n"HandlePayload");
				return Signal.Execute(Payload);
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, ScriptStructArgumentModuleName, ScriptStructArgumentFilename, ScriptSource),
			TEXT("Delegate script struct argument fixture should compile")));

		asIScriptModule* Module = FindScriptModule(Engine, ScriptStructArgumentModuleName);
		ASSERT_THAT(IsNotNull(Module, TEXT("Delegate script struct argument module should be queryable")));

		FAngelscriptTestExecutor Executor(
			*TestRunner,
			Engine,
			*Module,
			TEXT("int RunScriptStructDelegate()"));
		ASSERT_THAT(IsTrue(Executor.IsValid(), TEXT("Delegate script struct argument entry point should resolve")));

		const int32 Result = Executor.ExecuteAndGet<int32>(INDEX_NONE);
		ASSERT_THAT(AreEqual(42, Result, TEXT("Delegate invocation should pass AS USTRUCT fields through the event argument buffer")));
	}
};
