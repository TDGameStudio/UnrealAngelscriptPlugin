#include "CQTest.h"

#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"

#include "Misc/ScopeExit.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(
	FAngelscriptExecutionNestedCallTests,
	"Angelscript.TestModule.Functional.Execute.Nested",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static constexpr ANSICHAR ModuleName[] = "ASExecutionNestedRecursiveFrameIsolation";
	inline static const TCHAR* const ScriptSource = TEXT(R"AS(
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

public:
	TEST_METHOD(RecursiveFrameIsolation)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		int32 Result = 0;
		asIScriptModule* Module = BuildModule(*TestRunner, Engine, ModuleName, ScriptSource);
		ASSERT_THAT(IsNotNull(Module));

		asIScriptFunction* RunFunction = GetFunctionByDecl(*TestRunner, *Module, TEXT("int Run()"));
		ASSERT_THAT(IsNotNull(RunFunction));

		ASSERT_THAT(IsTrue(ExecuteIntFunction(*TestRunner, Engine, *RunFunction, Result)));

		ASSERT_THAT(AreEqual(1234, Result));

		}
	}
};

TEST_CLASS_WITH_FLAGS(
	FAngelscriptExecutionContextCallstackTests,
	"Angelscript.TestModule.Functional.Execute.Context",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static constexpr ANSICHAR ExceptionModuleName[] = "ASExecutionExceptionCallstackInspection";
	inline static const TCHAR* const ExceptionScriptSource = TEXT(R"AS(
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

public:
	TEST_METHOD(ExceptionCallstackInspection)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, ExceptionModuleName, ExceptionScriptSource);
		ASSERT_THAT(IsNotNull(Module));

		asIScriptFunction* EntryFunction = GetFunctionByDecl(*TestRunner, *Module, TEXT("int Entry()"));
		ASSERT_THAT(IsNotNull(EntryFunction));

		asIScriptContext* Context = Engine.CreateContext();
		ASSERT_THAT(IsNotNull(Context));

		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		const int32 PrepareResult = Context->Prepare(EntryFunction);
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), PrepareResult));

		TestRunner->AddExpectedError(TEXT("ContextCallstackFailure"), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(TEXT("ASExecutionExceptionCallstackInspection"), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(TEXT("void FailInner(int) | Line"), EAutomationExpectedErrorFlags::Contains, 1, false);
		TestRunner->AddExpectedError(TEXT("void TriggerFailure(int) | Line"), EAutomationExpectedErrorFlags::Contains, 1, false);
		TestRunner->AddExpectedError(TEXT("int Entry() | Line"), EAutomationExpectedErrorFlags::Contains, 1, false);

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

		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), ExecuteResult));
		ASSERT_THAT(IsFalse(ExceptionString.IsEmpty()));
		ASSERT_THAT(IsTrue(ExceptionString.Contains(TEXT("ContextCallstackFailure"))));
		ASSERT_THAT(IsTrue(CallstackSize >= 3));
		ASSERT_THAT(IsTrue(bFoundInnerFrame));
		ASSERT_THAT(IsTrue(bFoundMiddleFrame));
		ASSERT_THAT(IsTrue(bFoundEntryFrame));
		ASSERT_THAT(IsTrue(bAllFrameLinesPositive));

		}
	}
};

#endif
