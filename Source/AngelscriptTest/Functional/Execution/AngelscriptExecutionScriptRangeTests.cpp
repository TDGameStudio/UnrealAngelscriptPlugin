#include "CQTest.h"

#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"

// Test Layer: Runtime Integration
#if WITH_ANGELSCRIPT_UNITTESTS



TEST_CLASS_WITH_FLAGS(
	FAngelscriptExecutionScriptRangeBoundariesTest,
	"Angelscript.TestModule.Functional.Execute.Script",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
static constexpr ANSICHAR ModuleName[] = "ASExecutionScriptRangeBoundaries";
const TCHAR* const ScriptSource =
	TEXT("int Calculate(int Start, int End) { int Result = 0; for (int Index = Start; Index <= End; ++Index) { Result += Index; } return Result; }");

struct FRangeCase
{
	const TCHAR* Name;
	int32 Start = 0;
	int32 End = 0;
	int32 Expected = 0;
};

static bool ExecuteRangeCase(
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

public:
	TEST_METHOD(RangeBoundaries)
	{
FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, ModuleName, ScriptSource);
		ASSERT_THAT(IsNotNull(Module));

		asIScriptFunction* Function = GetFunctionByDecl(*TestRunner, *Module, TEXT("int Calculate(int, int)"));
		ASSERT_THAT(IsNotNull(Function));

		const FRangeCase Cases[] =
		{
			{ TEXT("Execution.Script.RangeBoundaries single-element case"), 1, 1, 1 },
			{ TEXT("Execution.Script.RangeBoundaries reverse-empty case"), 5, 4, 0 },
			{ TEXT("Execution.Script.RangeBoundaries negative-to-positive case"), -2, 2, 0 },
		};

		for (const FRangeCase& RangeCase : Cases)
		{
			ASSERT_THAT(IsTrue(ExecuteRangeCase(*TestRunner, Engine, *Function, RangeCase)));
		}

		}
	}
};

#endif
