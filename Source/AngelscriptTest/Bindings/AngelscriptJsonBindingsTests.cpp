// ============================================================================
// AngelscriptJsonBindingsTests.cpp
//
// JSON binding coverage — CQTest refactor.
// Automation ID:
//   Angelscript.TestModule.Bindings.Json.*
//
// Sections:
//   ObjectRoundTrip — Full JSON object create/serialize/parse round-trip
//   ErrorPaths      — Type mismatch, out-of-bounds, iterator mutation exceptions
//
// CQTest adaptation notes:
//   Two original IMPLEMENT_SIMPLE_AUTOMATION_TEST classes merged into one
//   TEST_CLASS with two TEST_METHODs. The custom exception execution helper
//   is retained for the error-path tests. The round-trip test uses
//   ExpectGlobalInt via standard `int Entry()` → `int RoundTrip()` rename.
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptBindingsModuleBuilder.h"
#include "Shared/AngelscriptBindingsAssertions.h"

#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptTestBindings;
using namespace AngelscriptReflectiveAccess;

// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Shared helpers
// ----------------------------------------------------------------------------

namespace JsonTestHelpers
{
	// TODO(refactor-as-test-shared-layout-and-naming): migrate ExecuteFunctionExpectingException to Shared/AngelscriptTestExecute.h once the Execute* naming family lands in Phase 3.
	bool ExecuteFunctionExpectingException(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		asIScriptModule& Module,
		const TCHAR* FunctionDecl,
		const TCHAR* ContextLabel,
		FString& OutExceptionString,
		int32& OutExceptionLine)
	{
		asIScriptFunction* Function = GetFunctionByDecl(Test, Module, FunctionDecl);
		if (Function == nullptr)
		{
			return false;
		}

		FAngelscriptEngineScope EngineScope(Engine);
		asIScriptContext* Context = Engine.CreateContext();
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s should create an execution context"), ContextLabel), Context))
		{
			return false;
		}

		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		const int PrepareResult = Context->Prepare(Function);
		if (!Test.TestEqual(
				*FString::Printf(TEXT("%s should prepare successfully"), ContextLabel),
				PrepareResult,
				static_cast<int32>(asSUCCESS)))
		{
			return false;
		}

		const int ExecuteResult = Context->Execute();
		if (!Test.TestEqual(
				*FString::Printf(TEXT("%s should fail with a script exception"), ContextLabel),
				ExecuteResult,
				static_cast<int32>(asEXECUTION_EXCEPTION)))
		{
			return false;
		}

		OutExceptionString = Context->GetExceptionString() != nullptr ? UTF8_TO_TCHAR(Context->GetExceptionString()) : TEXT("");
		OutExceptionLine = Context->GetExceptionLineNumber();
		const bool bHasExceptionString = Test.TestFalse(
			*FString::Printf(TEXT("%s should report a non-empty exception string"), ContextLabel),
			OutExceptionString.IsEmpty());
		const bool bHasExceptionLine = Test.TestTrue(
			*FString::Printf(TEXT("%s should report a positive exception line"), ContextLabel),
			OutExceptionLine > 0);
		return bHasExceptionString && bHasExceptionLine;
	}
}

using namespace JsonTestHelpers;

// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptJsonBindingsTest,
	"Angelscript.TestModule.Bindings.Json",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: ObjectRoundTrip
	// ====================================================================

	TEST_METHOD(ObjectRoundTrip)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FCoverageModuleScope Mod(*TestRunner, Engine, TEXT("ASJson_ObjectRoundTrip"), TEXT(R"(
int RoundTrip()
{
	FJsonObject Root;
	Root.SetStringField("Name", "Alice");
	Root.SetNumberField("Score", 1337.0);
	Root.SetBoolField("Enabled", true);

	FJsonObject Child = Root.CreateObjectField("Child");
	Child.SetStringField("Label", "Nested");
	Child.SetNumberField("Count", 2.0);

	FJsonArray Values;
	Values.AddString("First");
	Values.AddNumber(42);
	Root.SetArrayField("Values", Values);

	FString Serialized = Root.SaveToString(false);
	if (Serialized.IsEmpty())
		return 10;

	FJsonObject Parsed = Json::ParseString(Serialized);
	if (!Parsed.IsValid())
		return 20;

	if (Parsed.GetStringField("Name") != "Alice")
		return 30;
	if (Parsed.GetNumberField("Score") != 1337.0)
		return 40;
	if (!Parsed.GetBoolField("Enabled"))
		return 50;

	FJsonArray ParsedValues = Parsed.GetArrayField("Values");
	if (ParsedValues.Num() != 2)
		return 60;

	FJsonObjectFieldIterator Iterator = Parsed.Iterator();
	bool bSawEnabled = false;
	bool bIteratorBoolValue = false;
	while (Iterator.CanProceed)
	{
		Iterator.Proceed();
		if (Iterator.GetFieldName() == "Enabled")
		{
			bSawEnabled = Iterator.GetValue().TryGetBool(bIteratorBoolValue);
			break;
		}
	}
	if (!bSawEnabled || !bIteratorBoolValue)
		return 65;

	FJsonValue FirstValue = ParsedValues.GetValueAt(0);
	FString FirstString;
	if (!FirstValue.TryGetString(FirstString))
		return 70;
	if (FirstString != "First")
		return 80;

	FJsonValue SecondValue = ParsedValues.GetValueAt(1);
	int32 SecondNumber = 0;
	if (!SecondValue.TryGetNumber(SecondNumber))
		return 90;
	if (SecondNumber != 42)
		return 100;

	FJsonObject ParsedChild;
	if (!Parsed.TryGetObjectField("Child", ParsedChild))
		return 110;
	if (ParsedChild.GetStringField("Label") != "Nested")
		return 120;
	if (ParsedChild.GetNumberField("Count") != 2.0)
		return 130;

	FJsonArray ParsedValuesAgain;
	if (!Parsed.TryGetArrayField("Values", ParsedValuesAgain))
		return 140;
	if (ParsedValuesAgain.Num() != 2)
		return 150;

	if (Json::ValueTypeToString(EJsonType::Array) != "Array")
		return 160;

	return 1;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		AngelscriptTestBindings::ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int RoundTrip()"),
			TEXT("Json object round-trip operations should preserve field values and JSON type strings"),
			1);
	}

	// ====================================================================
	// Section: ErrorPaths
	// ====================================================================

	TEST_METHOD(ErrorPaths)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FCoverageModuleScope Mod(*TestRunner, Engine, TEXT("ASJson_ErrorPaths"), TEXT(R"(
void TriggerTypeError()
{
	FJsonObject Root;
	FJsonObject Child = Root.CreateObjectField("Child");
	Child.SetNumberField("Score", 3.5);

	FJsonArray Values;
	Values.AddNumber(1);
	Root.SetArrayField("Values", Values);

	FString WrongTypeValue = Root.GetStringField("Child");
	if (WrongTypeValue == "NeverReached")
	{
		Root.SetStringField("Unreachable", WrongTypeValue);
	}
}

void TriggerOutOfBounds()
{
	FJsonArray Values;
	Values.AddNumber(1);

	FJsonValue MissingValue = Values.GetValueAt(1);
	if (MissingValue.IsNull())
	{
		Values.AddString("NeverReached");
	}
}

void TriggerIteratorMutation()
{
	FJsonObject Root;
	Root.SetNumberField("Score", 3.5);

	FJsonArray Values;
	Values.AddNumber(1);
	Root.SetArrayField("Values", Values);

	auto Iterator = Root.Iterator();
	while (Iterator.CanProceed)
	{
		Iterator.Proceed();
		Root.SetStringField("Injected", "bad");
	}
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		// Type error path
		TestRunner->AddExpectedError(TEXT("Json Value of type 'Object' used as a 'String'."), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("ASJson_ErrorPaths"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("void TriggerTypeError()"), EAutomationExpectedErrorFlags::Contains, 0, false);

		FString ExceptionString;
		int32 ExceptionLine = INDEX_NONE;
		if (!ExecuteFunctionExpectingException(
				*TestRunner, Engine, M,
				TEXT("void TriggerTypeError()"),
				TEXT("Json type-error path"),
				ExceptionString, ExceptionLine))
		{
			return;
		}

		TestRunner->TestEqual(
			TEXT("Json GetStringField type mismatch should surface the expected runtime exception"),
			ExceptionString,
			FString(TEXT("Json Value of type 'Object' used as a 'String'.")));
		TestRunner->TestTrue(
			TEXT("Json GetStringField type mismatch should report a script source line"),
			ExceptionLine > 0);

		// Out-of-bounds path
		TestRunner->AddExpectedError(TEXT("Array index is out of bounds"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("ASJson_ErrorPaths"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("void TriggerOutOfBounds()"), EAutomationExpectedErrorFlags::Contains, 0, false);

		if (!ExecuteFunctionExpectingException(
				*TestRunner, Engine, M,
				TEXT("void TriggerOutOfBounds()"),
				TEXT("Json out-of-bounds path"),
				ExceptionString, ExceptionLine))
		{
			return;
		}

		TestRunner->TestEqual(
			TEXT("Json GetValueAt out-of-bounds access should surface the expected runtime exception"),
			ExceptionString,
			FString(TEXT("Array index is out of bounds")));
		TestRunner->TestTrue(
			TEXT("Json GetValueAt out-of-bounds access should report a script source line"),
			ExceptionLine > 0);

		// Iterator mutation path
		TestRunner->AddExpectedError(TEXT("FJsonObject is being modified during for loop iteration"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("ASJson_ErrorPaths"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("void TriggerIteratorMutation()"), EAutomationExpectedErrorFlags::Contains, 0, false);

		if (!ExecuteFunctionExpectingException(
				*TestRunner, Engine, M,
				TEXT("void TriggerIteratorMutation()"),
				TEXT("Json iterator-mutation path"),
				ExceptionString, ExceptionLine))
		{
			return;
		}

		TestRunner->TestEqual(
			TEXT("Json iterator mutation should surface the expected runtime exception"),
			ExceptionString,
			FString(TEXT("FJsonObject is being modified during for loop iteration")));
		TestRunner->TestTrue(
			TEXT("Json iterator mutation should report a script source line"),
			ExceptionLine > 0);
	}
};

#endif
