#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_Angelscript_AngelscriptExecutionScriptRangeTests_Private
{
	constexpr ANSICHAR ModuleName[] = "ASExecutionScriptRangeBoundaries";
	const TCHAR* const ScriptSource =
		TEXT("int Calculate(int Start, int End) { int Result = 0; for (int Index = Start; Index <= End; ++Index) { Result += Index; } return Result; }");

	struct FRangeCase
	{
		const TCHAR* Name;
		int32 Start = 0;
		int32 End = 0;
		int32 Expected = 0;
	};

	bool ExecuteRangeCase(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		asIScriptFunction& Function,
		const FRangeCase& RangeCase)
	{
		asIScriptContext* Context = Engine.CreateContext();
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s should create a context"), RangeCase.Name), Context))
		{
			return false;
		}

		const int PrepareResult = Context->Prepare(&Function);
		if (!Test.TestEqual(*FString::Printf(TEXT("%s should prepare the entry point"), RangeCase.Name), PrepareResult, static_cast<int32>(asSUCCESS)))
		{
			Context->Release();
			return false;
		}

		Context->SetArgDWord(0, static_cast<asDWORD>(RangeCase.Start));
		Context->SetArgDWord(1, static_cast<asDWORD>(RangeCase.End));

		const int ExecuteResult = Context->Execute();
		if (!Test.TestEqual(*FString::Printf(TEXT("%s should execute the entry point"), RangeCase.Name), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED)))
		{
			Context->Release();
			return false;
		}

		const bool bMatched = Test.TestEqual(
			*FString::Printf(TEXT("%s should return the expected inclusive range sum"), RangeCase.Name),
			static_cast<int32>(Context->GetReturnDWord()),
			RangeCase.Expected);
		Context->Release();
		return bMatched;
	}
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptExecutionScriptRangeBoundariesTest,
	"Angelscript.TestModule.Functional.Execute.Script.RangeBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptExecutionScriptRangeBoundariesTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptExecutionScriptRangeTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	asIScriptModule* Module = BuildModule(*this, Engine, ModuleName, ScriptSource);
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Calculate(int, int)"));
	if (Function == nullptr)
	{
		return false;
	}

	const FRangeCase Cases[] =
	{
		{ TEXT("Execution.Script.RangeBoundaries single-element case"), 1, 1, 1 },
		{ TEXT("Execution.Script.RangeBoundaries reverse-empty case"), 5, 4, 0 },
		{ TEXT("Execution.Script.RangeBoundaries negative-to-positive case"), -2, 2, 0 },
	};

	for (const FRangeCase& RangeCase : Cases)
	{
		if (!ExecuteRangeCase(*this, Engine, *Function, RangeCase))
		{
			return false;
		}
	}

	}
	return true;
}

#endif
