#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestLegacyHelpers.h"

#include "Misc/ScopeExit.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_Angelscript_AngelscriptExecutionNestedCallTests_Private
{
	constexpr ANSICHAR ModuleName[] = "ASExecutionNestedRecursiveFrameIsolation";
	const TCHAR* const ScriptSource = TEXT(R"AS(
int Encode(int Value)
{
	if (Value == 0)
	{
		return 0;
	}

	int Local = Value;
	return Local + Encode(Value - 1) * 10;
}

int Run()
{
	return Encode(4);
}
)AS");

	constexpr ANSICHAR ExceptionModuleName[] = "ASExecutionExceptionCallstackInspection";
	const TCHAR* const ExceptionScriptSource = TEXT(R"AS(
void FailInner(int Value)
{
	int Inner = Value * 2;
	if (Inner > 0)
	{
		throw("ContextCallstackFailure");
	}
}

void TriggerFailure(int Seed)
{
	int Local = Seed + 1;
	FailInner(Local);
}

int Entry()
{
	TriggerFailure(20);
	return 0;
}
)AS");
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptExecutionNestedRecursiveFrameIsolationTest,
	"Angelscript.TestModule.Functional.Execute.Nested.RecursiveFrameIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptExecutionExceptionCallstackInspectionTest,
	"Angelscript.TestModule.Functional.Execute.Context.ExceptionCallstackInspection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptExecutionNestedRecursiveFrameIsolationTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptExecutionNestedCallTests_Private;
	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(
		Engine,
		ModuleName,
		ScriptSource,
		TEXT("int Run()"),
		Result);

	bPassed = TestEqual(
		TEXT("Execution.Nested.RecursiveFrameIsolation should preserve frame-local state across recursive calls"),
		Result,
		1234);

	}
	return bPassed;
}

bool FAngelscriptExecutionExceptionCallstackInspectionTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptExecutionNestedCallTests_Private;
	bool bPassed = true;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	asIScriptModule* Module = BuildModule(*this, Engine, ExceptionModuleName, ExceptionScriptSource);
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* EntryFunction = GetFunctionByDecl(*this, *Module, TEXT("int Entry()"));
	if (EntryFunction == nullptr)
	{
		return false;
	}

	asIScriptContext* Context = Engine.CreateContext();
	if (!TestNotNull(TEXT("Execution.Context.ExceptionCallstackInspection should create a script context"), Context))
	{
		return false;
	}

	ON_SCOPE_EXIT
	{
		Context->Release();
	};

	const int32 PrepareResult = Context->Prepare(EntryFunction);
	if (!TestEqual(
		TEXT("Execution.Context.ExceptionCallstackInspection should prepare Entry() successfully"),
		PrepareResult,
		static_cast<int32>(asSUCCESS)))
	{
		return false;
	}

	AddExpectedError(TEXT("ContextCallstackFailure"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("ASExecutionExceptionCallstackInspection"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("void FailInner(int) | Line"), EAutomationExpectedErrorFlags::Contains, 1, false);
	AddExpectedError(TEXT("void TriggerFailure(int) | Line"), EAutomationExpectedErrorFlags::Contains, 1, false);
	AddExpectedError(TEXT("int Entry() | Line"), EAutomationExpectedErrorFlags::Contains, 1, false);

	const int32 ExecuteResult = Context->Execute();
	const char* ExceptionStringAnsi = Context->GetExceptionString();
	const FString ExceptionString = ExceptionStringAnsi != nullptr ? UTF8_TO_TCHAR(ExceptionStringAnsi) : FString();
	const asUINT CallstackSize = Context->GetCallstackSize();

	bool bFoundInnerFrame = false;
	bool bFoundMiddleFrame = false;
	bool bFoundEntryFrame = false;
	bool bAllFrameLinesPositive = true;

	for (asUINT StackLevel = 0; StackLevel < CallstackSize; ++StackLevel)
	{
		asIScriptFunction* StackFunction = Context->GetFunction(StackLevel);
		if (StackFunction == nullptr)
		{
			bAllFrameLinesPositive = false;
			continue;
		}

		const FString Declaration = UTF8_TO_TCHAR(StackFunction->GetDeclaration());
		const int32 LineNumber = Context->GetLineNumber(StackLevel);
		bAllFrameLinesPositive &= LineNumber > 0;

		bFoundInnerFrame |= Declaration.Contains(TEXT("FailInner"));
		bFoundMiddleFrame |= Declaration.Contains(TEXT("TriggerFailure"));
		bFoundEntryFrame |= Declaration.Contains(TEXT("Entry"));
	}

	bPassed &= TestEqual(
		TEXT("Execution.Context.ExceptionCallstackInspection should raise a runtime exception"),
		ExecuteResult,
		static_cast<int32>(asEXECUTION_EXCEPTION));
	bPassed &= TestFalse(
		TEXT("Execution.Context.ExceptionCallstackInspection should expose a non-empty exception string"),
		ExceptionString.IsEmpty());
	bPassed &= TestTrue(
		TEXT("Execution.Context.ExceptionCallstackInspection should surface the thrown failure message"),
		ExceptionString.Contains(TEXT("ContextCallstackFailure")));
	bPassed &= TestTrue(
		TEXT("Execution.Context.ExceptionCallstackInspection should preserve at least three stack frames"),
		CallstackSize >= 3);
	bPassed &= TestTrue(
		TEXT("Execution.Context.ExceptionCallstackInspection should keep the failing inner frame in the callstack"),
		bFoundInnerFrame);
	bPassed &= TestTrue(
		TEXT("Execution.Context.ExceptionCallstackInspection should keep the middle frame in the callstack"),
		bFoundMiddleFrame);
	bPassed &= TestTrue(
		TEXT("Execution.Context.ExceptionCallstackInspection should keep the entry frame in the callstack"),
		bFoundEntryFrame);
	bPassed &= TestTrue(
		TEXT("Execution.Context.ExceptionCallstackInspection should expose positive source line numbers for captured frames"),
		bAllFrameLinesPositive);

	}
	return bPassed;
}

#endif
