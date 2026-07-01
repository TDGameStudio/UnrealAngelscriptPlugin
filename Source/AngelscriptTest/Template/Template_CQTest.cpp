// =============================================================================
// Template_CQTest.cpp
//
// Teaching template — demonstrates how to write Angelscript automation tests
// using CQTest (Engine/Source/Developer/CQTest/) with TEST_CLASS_WITH_FLAGS
// and TEST_METHOD registration.
//
// What this file covers (one TEST_METHOD per topic):
//
//   1. BasicCompileAndRun   — minimal CQTest skeleton: compile AS, assert one
//                             int return.
//   2. MultipleAssertions   — batch assertions via FExpectedGlobalInt array +
//                             ExpectGlobalInts.
//   3. ReturnStruct         — AS returns FString / struct, C++ reads it via
//                             ExpectGlobalReturnCustom<T> + validator lambda.
//   4. PassArguments        — C++ passes typed arguments into AS globals via
//                             FASGlobalFunctionInvoker (AddArg / AddArgRef).
//   5. NegativePath         — AS intentionally throws; C++ asserts exception
//                             message via ExecuteFunctionExpectingScriptException.
//   6. AssertThatEarlyReturn — ASSERT_THAT macro: short-circuit the test on
//                             first failure instead of accumulating errors.
//
// ---- Key CQTest macros ----
//
//   TEST_CLASS_WITH_FLAGS(Name, "Automation.Path", Flags)
//       Declares a test class; each TEST_METHOD inside registers as a
//       separate Automation test under the given path prefix.
//
//   BEFORE_ALL()   — static, runs once for the entire class.
//   AFTER_ALL()    — static, runs once after all methods complete.
//   BEFORE_EACH()  — virtual Setup(), runs before each TEST_METHOD.
//   AFTER_EACH()   — virtual TearDown(), runs after each TEST_METHOD.
//   TEST_METHOD(X) — declares a single test case.
//   ASSERT_THAT(x) — if assertion fails, immediately returns from the method.
//
// ---- Angelscript engine lifecycle in CQTest ----
//
//   The recommended pattern:
//
//     BEFORE_ALL  : ASTEST_CREATE_ENGINE()       — one-time acquire (with reset)
//     TEST_METHOD : ASTEST_GET_ENGINE()           — no reset per test
//                   FAngelscriptEngineScope       — local RAII scope
//                   FScopedAngelscriptModule          — module RAII
//     AFTER_ALL   : ASTEST_RESET_ENGINE(Engine)   — one-time cleanup
//
//   FScopedAngelscriptModule automatically calls Engine.DiscardModule() in its
//   destructor, so per-test module isolation is guaranteed without a full
//   engine reset between tests.
//
// ---- Common pitfall: TestRunner is a static pointer ----
//
//   CQTest exposes `TestRunner` as a static pointer of type
//   TTestRunner<FNoDiscardAsserter>*. Functions that take
//   FAutomationTestBase& (like ExpectGlobalInts, FScopedAngelscriptModule)
//   require dereferencing: pass `*TestRunner`, not `TestRunner`.
//
// ---- Reference files ----
//
//   CQTest header       : Engine/Source/Developer/CQTest/Public/CQTest.h
//   Production example  : AngelscriptTest/Bindings/AngelscriptFStringBindingsTests.cpp
//   Assertion helpers   : AngelscriptTest/Shared/AngelscriptBindingsAssertions.h
//   Module builder      : AngelscriptTest/Shared/AngelscriptBindingsModuleBuilder.h
//   Function invoker    : AngelscriptTest/Shared/AngelscriptGlobalFunctionInvoker.h
//   Engine macros       : AngelscriptTest/Shared/AngelscriptTestMacros.h
// =============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#if WITH_ANGELSCRIPT_UNITTESTS




TEST_CLASS_WITH_FLAGS(FAngelscriptTemplateCQTest,
	"Angelscript.Template.CQTest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	// -----------------------------------------------------------------
	// BEFORE_ALL — static, called once before any TEST_METHOD executes.
	// Acquires a clean shared clone engine for the entire test class.
	// -----------------------------------------------------------------
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	// -----------------------------------------------------------------
	// AFTER_ALL — static, called once after all TEST_METHODs complete.
	// Resets the shared engine to leave a clean state for subsequent
	// test classes that share the same process-level engine singleton.
	// -----------------------------------------------------------------
	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// =================================================================
	// 1. BasicCompileAndRun
	//
	// Minimal CQTest skeleton: compile one AS function, call it, and
	// assert its int return value with ExpectGlobalInt.
	// =================================================================

	TEST_METHOD(BasicCompileAndRun)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASTemplate_CQTest_Basic"), TEXT(R"(
int GetFortyTwo()
{
	return 42;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int GetFortyTwo()"),
			TEXT("Literal 42 round-trips through AS"),
			42);
	}

	// =================================================================
	// 2. MultipleAssertions
	//
	// Batch multiple assertions using FExpectedGlobalInt array and
	// ExpectGlobalInts. This is the standard pattern for testing many
	// cases within a single TEST_METHOD — avoids repeating the
	// boilerplate of FScopedAngelscriptModule per case.
	// =================================================================

	TEST_METHOD(MultipleAssertions)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASTemplate_CQTest_Multi"), TEXT(R"(
int Add()      { return 2 + 3; }
int Subtract() { return 10 - 7; }
int Multiply() { return 4 * 5; }
int Divide()   { return 20 / 4; }
int Modulo()   { return 17 % 5; }
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		// Each entry: { AS declaration, human-readable label, expected int }
		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int Add()"),      TEXT("2 + 3 = 5"),      5 },
			{ TEXT("int Subtract()"), TEXT("10 - 7 = 3"),     3 },
			{ TEXT("int Multiply()"), TEXT("4 * 5 = 20"),    20 },
			{ TEXT("int Divide()"),   TEXT("20 / 4 = 5"),     5 },
			{ TEXT("int Modulo()"),   TEXT("17 % 5 = 2"),     2 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}

	// =================================================================
	// 3. ReturnStruct
	//
	// AS functions return FString values. C++ reads them from the AS
	// return register via ExpectGlobalReturnCustom<FString> and
	// validates content with a lambda.
	// =================================================================

	TEST_METHOD(ReturnStruct)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASTemplate_CQTest_RetStruct"), TEXT(R"(
FString BuildGreeting()
{
	return "Hello" + " " + "World";
}

FString ToUpperCase()
{
	return "template".ToUpper();
}

FString FormatValue()
{
	return FString::Format("result={0}", 42);
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		// ExpectGlobalReturnCustom<T> invokes the named no-arg function,
		// reads the return value via GetAddressOfReturnValue, and hands
		// it to the validator lambda.
		ExpectGlobalReturnCustom<FString>(*TestRunner, Engine, M, 
			TEXT("FString BuildGreeting()"),
			TEXT("Concatenation produces Hello World"),
			[](FAutomationTestBase& T, const FString& V) -> bool
			{
				FNoDiscardAsserter LocalAssert(T);
				return LocalAssert.AreEqual(TEXT("Hello World"), V, TEXT("greeting"));
			});

		ExpectGlobalReturnCustom<FString>(*TestRunner, Engine, M, 
			TEXT("FString ToUpperCase()"),
			TEXT("ToUpper transforms to TEMPLATE"),
			[](FAutomationTestBase& T, const FString& V) -> bool
			{
				FNoDiscardAsserter LocalAssert(T);
				return LocalAssert.AreEqual(TEXT("TEMPLATE"), V, TEXT("upper"));
			});

		ExpectGlobalReturnCustom<FString>(*TestRunner, Engine, M, 
			TEXT("FString FormatValue()"),
			TEXT("FString::Format produces result=42"),
			[](FAutomationTestBase& T, const FString& V) -> bool
			{
				FNoDiscardAsserter LocalAssert(T);
				return LocalAssert.AreEqual(TEXT("result=42"), V, TEXT("format"));
			});
	}

	// =================================================================
	// 4. PassArguments
	//
	// C++ constructs values and passes them into AS functions via
	// FASGlobalFunctionInvoker. Demonstrates:
	//   - AddArg(int32)          for primitive types
	//   - AddArgRef(FString&)    for const FString& in parameters
	//   - ReadReturnStruct<T>()  for struct returns
	//   - CallAndReturn<T>()     for primitive returns
	// =================================================================

	TEST_METHOD(PassArguments)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASTemplate_CQTest_PassArgs"), TEXT(R"(
int AddInts(int A, int B)
{
	return A + B;
}

FString Greet(const FString& in Name)
{
	return "Hello, " + Name + "!";
}

int StringLen(const FString& in S)
{
	return S.Len();
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		// Primitive args: two int32 values
		{
			FASGlobalFunctionInvoker Inv(*TestRunner, Engine, M,
				TEXT("int AddInts(int, int)"));
			Inv.AddArg(static_cast<int32>(17)).AddArg(static_cast<int32>(25));
			int32 Result = Inv.CallAndReturn<int32>(-1);
			ASSERT_THAT(AreEqual(42, Result, TEXT("[PassArgs] AddInts(17,25)=42")));
		}

		// FString ref arg: C++ passes a string, AS builds a greeting
		{
			FString Name = TEXT("CQTest");
			FASGlobalFunctionInvoker Inv(*TestRunner, Engine, M,
				TEXT("FString Greet(const FString& in)"));
			Inv.AddArgRef(Name);
			if (Inv.Call())
			{
				FString Result;
				if (Inv.ReadReturnStruct(Result))
				{
					ASSERT_THAT(AreEqual(
						TEXT("Hello, CQTest!"),
						Result,
						TEXT("[PassArgs] Greet returns Hello, CQTest!")));
				}
			}
		}

		// FString ref → int return: mixed argument and return types
		{
			FString Input = TEXT("Template");
			FASGlobalFunctionInvoker Inv(*TestRunner, Engine, M,
				TEXT("int StringLen(const FString& in)"));
			Inv.AddArgRef(Input);
			ASSERT_THAT(AreEqual(
				Input.Len(),
				Inv.CallAndReturn<int32>(0),
				TEXT("[PassArgs] StringLen matches C++ Len()")));
		}
	}

	// =================================================================
	// 5. NegativePath
	//
	// Tests that AS raises the expected exception when code performs an
	// invalid operation. Uses ExecuteFunctionExpectingScriptException
	// which asserts:
	//   - Prepare succeeds
	//   - Execute returns asEXECUTION_EXCEPTION
	//   - Exception message is non-empty and contains expected substring
	//   - Exception line number is positive
	//
	// The caller must register AddExpectedError if the exception message
	// would otherwise trigger a test failure from the log handler.
	// =================================================================

	TEST_METHOD(NegativePath)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// Register expected errors BEFORE module compilation so patterns
		// are in place when AS emits Error-level exception logs.
		//
		// AS exception handling emits log entries at Error level that
		// must be suppressed:
		//   (a) Module name   — e.g. "ASTemplate_CQTest_Negative"
		//   (b) Function info — e.g. "void StringIndexOOB() | Line 5 | Col 2"
		//   (c) Exception msg — e.g. "String index out of bounds"
		// Use count=0 (any number of matches) since the module name
		// appears once per exception.
		TestRunner->AddExpectedErrorPlain(
			TEXT("ASTemplate_CQTest_Negative"),
			EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedErrorPlain(
			TEXT("void StringIndexOOB()"),
			EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedErrorPlain(
			TEXT("String index out of bounds"),
			EAutomationExpectedErrorFlags::Contains, 0);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASTemplate_CQTest_Negative"), TEXT(R"(
void StringIndexOOB()
{
	FString S = "AB";
	int16 C = S[99];
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExecuteFunctionExpectingScriptException(
			*TestRunner, Engine, M, 
			TEXT("void StringIndexOOB()"),
			TEXT("String index beyond length throws"),
			TEXT("out of bounds"));
	}

	// =================================================================
	// 6. AssertThatEarlyReturn
	//
	// ASSERT_THAT(expr) is CQTest's short-circuit assertion: if the
	// expression fails, the TEST_METHOD immediately returns, skipping
	// all subsequent code. Useful when later assertions depend on
	// earlier ones succeeding (e.g. module compilation must pass before
	// any function can be invoked).
	//
	// Contrast with TestTrue/TestEqual which record failures but
	// continue executing, potentially causing cascading errors.
	// =================================================================

	TEST_METHOD(AssertThatEarlyReturn)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASTemplate_CQTest_AssertThat"), TEXT(R"(
int GetValue() { return 100; }
FString GetMessage() { return "success"; }
)"));

		// ASSERT_THAT short-circuits: if the module failed to compile,
		// we skip the function invocations instead of crashing.
		ASSERT_THAT(IsTrue(Mod.IsValid()));

		auto& M = Mod.GetModule();

		// After the guard, we know M is safe to use.
		ExpectGlobalInt(*TestRunner, Engine, M, 
			TEXT("int GetValue()"),
			TEXT("Value is 100"),
			100);

		ExpectGlobalReturnCustom<FString>(*TestRunner, Engine, M, 
			TEXT("FString GetMessage()"),
			TEXT("Message is 'success'"),
			[](FAutomationTestBase& T, const FString& V) -> bool
			{
				FNoDiscardAsserter LocalAssert(T);
				return LocalAssert.AreEqual(TEXT("success"), V, TEXT("message"));
			});
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
