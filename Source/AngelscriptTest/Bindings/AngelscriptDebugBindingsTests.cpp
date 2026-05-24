// ============================================================================
// AngelscriptDebugBindingsTests.cpp
//
// Debug drawing/logging API binding coverage — CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Bindings.Debug.FAngelscriptDebugBindingsTest.*
//
// Sections:
//   Callstack    — GetAngelscriptCallstack / FormatAngelscriptCallstack
//   Throw        — throw() propagation, exception capture, function site
//
// CQTest adaptation notes:
//   The throw test requires AddExpectedError and manual context execution.
//   Callstack test is split into a single ExpectGlobalInt check.
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptBindingsModuleBuilder.h"
#include "Shared/AngelscriptBindingsAssertions.h"

#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptTestBindings;

// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptDebugBindingsTest,
	"Angelscript.TestModule.Bindings.Debug",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: Callstack
	// ====================================================================

	TEST_METHOD(Callstack)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FCoverageModuleScope Mod(*TestRunner, Engine, TEXT("ASDebug_Callstack"), TEXT(R"(
bool StackContains(const TArray<FString>& Stack, const FString& Needle)
{
	for (int Index = 0; Index < Stack.Num(); ++Index)
	{
		if (Stack[Index].Contains(Needle))
		{
			return true;
		}
	}
	return false;
}

int Callstack_ProbeCallstack()
{
	TArray<FString> Stack = GetAngelscriptCallstack();
	FString Formatted = FormatAngelscriptCallstack();
	if (Stack.Num() < 3)
		return 0;
	if (!StackContains(Stack, "Callstack_ProbeCallstack"))
		return 0;
	if (!StackContains(Stack, "Callstack_EntryCallstack"))
		return 0;
	if (!Formatted.Contains("Callstack_ProbeCallstack"))
		return 0;
	if (!Formatted.Contains("Callstack_EntryCallstack"))
		return 0;
	return 1;
}

int Callstack_EntryCallstack()
{
	return Callstack_ProbeCallstack();
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		AngelscriptTestBindings::ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Callstack_EntryCallstack()"), TEXT("GetAngelscriptCallstack and FormatAngelscriptCallstack should capture the full call chain"), 1);
	}

	// ====================================================================
	// Section: Throw
	// ====================================================================

	TEST_METHOD(Throw)
	{
		TestRunner->AddExpectedError(TEXT("DebuggingThrowCompat"), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(TEXT("ASDebug"), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(TEXT("void Throw_ThrowLeaf()"), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(TEXT("void Throw_ThrowMiddle()"), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(TEXT("int Throw_Entry()"), EAutomationExpectedErrorFlags::Contains, 1);

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FCoverageModuleScope Mod(*TestRunner, Engine, TEXT("ASDebug_Throw"), TEXT(R"(
void Throw_ThrowLeaf()
{
	throw("DebuggingThrowCompat");
}

void Throw_ThrowMiddle()
{
	Throw_ThrowLeaf();
}

int Throw_Entry()
{
	Throw_ThrowMiddle();
	return 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		asIScriptFunction* ThrowFunction = GetFunctionByDecl(*TestRunner, M, TEXT("int Throw_Entry()"));
		if (ThrowFunction == nullptr) return;

		asIScriptContext* Context = Engine.CreateContext();
		if (!TestRunner->TestNotNull(TEXT("Should create execution context"), Context)) return;

		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		int32 PrepareResult = Context->Prepare(ThrowFunction);
		int32 ExecuteResult = PrepareResult == asSUCCESS ? Context->Execute() : PrepareResult;
		FString ExceptionString = Context->GetExceptionString() != nullptr ? UTF8_TO_TCHAR(Context->GetExceptionString()) : TEXT("");

		FString ExceptionFunctionName;
		FString ExceptionFunctionDeclaration;
		if (asIScriptFunction* ExceptionFunction = Context->GetExceptionFunction())
		{
			ExceptionFunctionName = UTF8_TO_TCHAR(ExceptionFunction->GetName());
			ExceptionFunctionDeclaration = UTF8_TO_TCHAR(ExceptionFunction->GetDeclaration());
		}

		TestRunner->TestEqual(TEXT("Throw path should prepare the entry function"), PrepareResult, static_cast<int32>(asSUCCESS));
		TestRunner->TestEqual(TEXT("Throw path should raise a script exception"), ExecuteResult, static_cast<int32>(asEXECUTION_EXCEPTION));
		TestRunner->TestTrue(TEXT("Throw path should surface the thrown message"), ExceptionString.Contains(TEXT("DebuggingThrowCompat")));
		TestRunner->TestFalse(TEXT("Throw path should capture a non-empty exception function name"), ExceptionFunctionName.IsEmpty());
		TestRunner->TestTrue(TEXT("Throw path should report ThrowLeaf as the exception site"), ExceptionFunctionDeclaration.Contains(TEXT("Throw_ThrowLeaf")));
	}
};

#endif
