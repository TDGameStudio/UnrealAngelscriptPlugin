#include "Misc/AutomationTest.h"

#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "StaticJIT/StaticJITHeader.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_StaticJIT_AngelscriptStaticJITExceptionTests_Private
{
	struct FMappedExceptionCase
	{
		const TCHAR* Label = TEXT("");
		EJITException Exception = EJITException::NullPointer;
		const TCHAR* ExpectedError = TEXT("");
	};

	using FExceptionWrapper = void(*)(FScriptExecution&);

	struct FWrapperExceptionCase
	{
		const TCHAR* Label = TEXT("");
		FExceptionWrapper Wrapper = nullptr;
		const TCHAR* ExpectedError = TEXT("");
	};

	bool RunExceptionCase(
		FAutomationTestBase& Test,
		asCThreadLocalData* ThreadLocalData,
		const TCHAR* CaseLabel,
		const TCHAR* ExpectedError,
		TFunctionRef<void(FScriptExecution&)> Invoke)
	{
		const FString ThreadLocalMessage = FString::Printf(
			TEXT("%s should run with a valid game-thread local data pointer"),
			CaseLabel);
		if (!Test.TestNotNull(*ThreadLocalMessage, ThreadLocalData))
		{
			return false;
		}

		FScriptExecution* PreviousExecution = ThreadLocalData->activeExecution;
		asCContext* PreviousContext = ThreadLocalData->activeContext;
		bool bPassed = false;

		{
			FScriptExecution Execution(ThreadLocalData);
			const FString ActiveExecutionMessage = FString::Printf(
				TEXT("%s should register the temporary execution on the thread-local data"),
				CaseLabel);
			const FString InitialStateMessage = FString::Printf(
				TEXT("%s should start with bExceptionThrown cleared"),
				CaseLabel);
			if (!Test.TestTrue(*ActiveExecutionMessage, ThreadLocalData->activeExecution == &Execution)
				|| !Test.TestFalse(*InitialStateMessage, Execution.bExceptionThrown))
			{
				return false;
			}

			Test.AddExpectedErrorPlain(ExpectedError, EAutomationExpectedErrorFlags::Contains, 1);
			Invoke(Execution);

			const FString ThrownMessage = FString::Printf(
				TEXT("%s should mark the execution as throwing after invoking the helper"),
				CaseLabel);
			bPassed = Test.TestTrue(*ThrownMessage, Execution.bExceptionThrown);
		}

		const FString RestoreExecutionMessage = FString::Printf(
			TEXT("%s should restore the previous active execution after the temporary scope exits"),
			CaseLabel);
		const FString RestoreContextMessage = FString::Printf(
			TEXT("%s should restore the previous active context after the temporary scope exits"),
			CaseLabel);
		const bool bRestoredExecution = Test.TestTrue(
			*RestoreExecutionMessage,
			ThreadLocalData->activeExecution == PreviousExecution);
		const bool bRestoredContext = Test.TestTrue(
			*RestoreContextMessage,
			ThreadLocalData->activeContext == PreviousContext);
		return bPassed && bRestoredExecution && bRestoredContext;
	}

	bool VerifySetExceptionMappings(FAutomationTestBase& Test, asCThreadLocalData* ThreadLocalData)
	{
		static constexpr FMappedExceptionCase Cases[] =
		{
			{ TEXT("StaticJIT.ExceptionHelpers.MapExpectedErrors null-pointer branch"), EJITException::NullPointer, TEXT("Null pointer access") },
			{ TEXT("StaticJIT.ExceptionHelpers.MapExpectedErrors divide-by-zero branch"), EJITException::Div0, TEXT("Divide by zero") },
			{ TEXT("StaticJIT.ExceptionHelpers.MapExpectedErrors overflow branch"), EJITException::Overflow, TEXT("Overflow in integer division") },
			{ TEXT("StaticJIT.ExceptionHelpers.MapExpectedErrors unbound-function branch"), EJITException::UnboundFunction, TEXT("Unbound function called") },
			{ TEXT("StaticJIT.ExceptionHelpers.MapExpectedErrors out-of-bounds branch"), EJITException::OutOfBounds, TEXT("Index out of bounds.") },
			{ TEXT("StaticJIT.ExceptionHelpers.MapExpectedErrors unknown branch"), static_cast<EJITException>(255), TEXT("Unknown exception.") },
		};

		for (const FMappedExceptionCase& Case : Cases)
		{
			if (!RunExceptionCase(
					Test,
					ThreadLocalData,
					Case.Label,
					Case.ExpectedError,
					[&Case](FScriptExecution& Execution)
					{
						FStaticJITFunction::SetException(Execution, Case.Exception);
					}))
			{
				return false;
			}
		}

		return true;
	}

	bool VerifyWrapperMappings(FAutomationTestBase& Test, asCThreadLocalData* ThreadLocalData)
	{
		static constexpr FWrapperExceptionCase Cases[] =
		{
			{ TEXT("StaticJIT.ExceptionHelpers.WrappersMatchMappedMessages null-pointer wrapper"), &FStaticJITFunction::SetNullPointerException, TEXT("Null pointer access") },
			{ TEXT("StaticJIT.ExceptionHelpers.WrappersMatchMappedMessages divide-by-zero wrapper"), &FStaticJITFunction::SetDivByZeroException, TEXT("Divide by zero") },
			{ TEXT("StaticJIT.ExceptionHelpers.WrappersMatchMappedMessages overflow wrapper"), &FStaticJITFunction::SetOverflowException, TEXT("Overflow in integer division") },
			{ TEXT("StaticJIT.ExceptionHelpers.WrappersMatchMappedMessages unbound-function wrapper"), &FStaticJITFunction::SetUnboundFunctionException, TEXT("Unbound function called") },
			{ TEXT("StaticJIT.ExceptionHelpers.WrappersMatchMappedMessages out-of-bounds wrapper"), &FStaticJITFunction::SetOutOfBoundsException, TEXT("Index out of bounds.") },
			{ TEXT("StaticJIT.ExceptionHelpers.WrappersMatchMappedMessages unknown wrapper"), &FStaticJITFunction::SetUnknownException, TEXT("Unknown exception.") },
		};

		for (const FWrapperExceptionCase& Case : Cases)
		{
			if (!RunExceptionCase(
					Test,
					ThreadLocalData,
					Case.Label,
					Case.ExpectedError,
					[&Case](FScriptExecution& Execution)
					{
						Case.Wrapper(Execution);
					}))
			{
				return false;
			}
		}

		return true;
	}
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptStaticJITExceptionHelpersMapExpectedErrorsTest,
	"Angelscript.TestModule.StaticJIT.ExceptionHelpers.MapExpectedErrors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptStaticJITExceptionHelpersWrapperParityTest,
	"Angelscript.TestModule.StaticJIT.ExceptionHelpers.WrappersMatchMappedMessages",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptStaticJITExceptionHelpersSwitchValueInvalidTest,
	"Angelscript.TestModule.StaticJIT.ExceptionHelpers.SwitchValueInvalidUsesDedicatedMessage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptStaticJITExceptionHelpersMapExpectedErrorsTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_StaticJIT_AngelscriptStaticJITExceptionTests_Private;
	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};

	do
	{
		if (!TestTrue(
				TEXT("StaticJIT.ExceptionHelpers.MapExpectedErrors should run with the current engine installed"),
				FAngelscriptEngine::TryGetCurrentEngine() == &Engine))
		{
			break;
		}

		if (!VerifySetExceptionMappings(*this, FAngelscriptEngine::GameThreadTLD))
		{
			break;
		}

		bPassed = true;
	}
	while (false);

	}
	return bPassed;
}

bool FAngelscriptStaticJITExceptionHelpersWrapperParityTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_StaticJIT_AngelscriptStaticJITExceptionTests_Private;
	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};

	do
	{
		if (!TestTrue(
				TEXT("StaticJIT.ExceptionHelpers.WrappersMatchMappedMessages should run with the current engine installed"),
				FAngelscriptEngine::TryGetCurrentEngine() == &Engine))
		{
			break;
		}

		if (!VerifyWrapperMappings(*this, FAngelscriptEngine::GameThreadTLD))
		{
			break;
		}

		bPassed = true;
	}
	while (false);

	}
	return bPassed;
}

bool FAngelscriptStaticJITExceptionHelpersSwitchValueInvalidTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_StaticJIT_AngelscriptStaticJITExceptionTests_Private;
	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};

	do
	{
		if (!TestTrue(
				TEXT("StaticJIT.ExceptionHelpers.SwitchValueInvalidUsesDedicatedMessage should run with the current engine installed"),
				FAngelscriptEngine::TryGetCurrentEngine() == &Engine))
		{
			break;
		}

		if (!RunExceptionCase(
				*this,
				FAngelscriptEngine::GameThreadTLD,
				TEXT("StaticJIT.ExceptionHelpers.SwitchValueInvalidUsesDedicatedMessage dedicated wrapper"),
				TEXT("Invalid enum value passed to switch"),
				[](FScriptExecution& Execution)
				{
					FStaticJITFunction::SetSwitchValueInvalidException(Execution);
				}))
		{
			break;
		}

		bPassed = true;
	}
	while (false);

	}
	return bPassed;
}

#endif
