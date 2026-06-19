#include "AngelscriptSDKTestExecutionHelpers.h"
#include "AngelscriptTestAdapter.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;
using namespace AngelscriptSDKTestSupport;


TEST_CLASS_WITH_FLAGS(FAngelscriptSDKRuntimeTests, "Angelscript.TestModule.AngelScriptSDK.Runtime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	inline static FNativeTestEngine Engine;

	BEFORE_ALL()
	{
		Engine.Create(*TestRunner);
	}

	AFTER_ALL()
	{
		Engine.Destroy();
	}

	BEFORE_EACH()
	{
		Engine.ResetMessages();
	}
	TEST_METHOD(Context)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("SDK runtime context test should create a standalone engine")));

		FScopedNativeModule Module(*TestRunner, Engine, "SDKRuntimeContext", R"(
int Compute(int N)
{
	int Result = 0;
	for (int i = 1; i <= N; i++)
	{
		Result += i;
	}
	return Result;
}

bool Entry()
{
	return Compute(10) == 55;
}
)");
		if (!Module.IsValid())
		{
			return;
		}

		bool bResult = false;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "bool Entry()", bResult))
		{
			return;
		}

		ASSERT_THAT(IsTrue(bResult,
			TEXT("SDK runtime context test should execute context operations")));
	}

	TEST_METHOD(Exception)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("SDK runtime exception test should create a standalone engine")));

		FScopedNativeModule Module(*TestRunner, Engine, "SDKRuntimeException", R"(
void ThrowException()
{
	int a = 0;
	int b = 1 / a;
}

bool Entry()
{
	ThrowException();
	return true;
}
)");
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "bool Entry()");
		ASSERT_THAT(IsNotNull(Function,
			TEXT("SDK runtime exception test should resolve entry function")));

		asIScriptContext* Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			TEXT("SDK runtime exception test should create context")));
		ON_SCOPE_EXIT { Context->Release(); };

		const int ExecuteResult = PrepareAndExecute(Context, Function);

		// Expect exception from divide by zero
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), ExecuteResult,
			TEXT("SDK runtime exception test should detect exception")));
	}

	TEST_METHOD(Suspend)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("SDK runtime suspend test should create a standalone engine")));

		FScopedNativeModule Module(*TestRunner, Engine, "SDKRuntimeSuspend", R"(
int Sum(int N)
{
	int Result = 0;
	for (int i = 1; i <= N; i++)
	{
		Result += i;
	}
	return Result;
}

bool Entry()
{
	return Sum(10) == 55;
}
)");
		if (!Module.IsValid())
		{
			return;
		}

		bool bResult = false;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "bool Entry()", bResult))
		{
			return;
		}

		ASSERT_THAT(IsTrue(bResult,
			TEXT("SDK runtime suspend test should execute loop with suspend support")));
	}

	TEST_METHOD(ExceptionDetails)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("SDK runtime exception-details test should create a standalone engine")));

		FScopedNativeModule Module(*TestRunner, Engine, "SDKRuntimeExceptionDetails", R"(
int Divide(int A, int B)
{
	return A / B;
}

int Entry()
{
	return Divide(10, 0);
}
)");
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "int Entry()");
		ASSERT_THAT(IsNotNull(Function,
			TEXT("SDK runtime exception-details test should resolve the entry function")));

		asIScriptContext* Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			TEXT("SDK runtime exception-details test should create a context")));
		ON_SCOPE_EXIT { Context->Release(); };

		const int ExecuteResult = PrepareAndExecute(Context, Function);
		const FString ExceptionString = UTF8_TO_TCHAR(Context->GetExceptionString() != nullptr ? Context->GetExceptionString() : "");
		const int ExceptionLine = Context->GetExceptionLineNumber();
		asIScriptFunction* ExceptionFunction = Context->GetExceptionFunction();
		const FString ExceptionFunctionName = (ExceptionFunction != nullptr && ExceptionFunction->GetName() != nullptr)
			? UTF8_TO_TCHAR(ExceptionFunction->GetName())
			: FString();

		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), ExecuteResult,
			TEXT("SDK runtime exception-details test should raise an execution exception")));

		ASSERT_THAT(AreEqual(FString(TEXT("Divide by zero")), ExceptionString,
			TEXT("SDK runtime exception-details test should report the divide-by-zero exception text")));
		ASSERT_THAT(IsTrue(ExceptionLine > 0,
			TEXT("SDK runtime exception-details test should report a positive exception line")));
		ASSERT_THAT(AreEqual(FString(TEXT("Divide")), ExceptionFunctionName,
			TEXT("SDK runtime exception-details test should attribute the exception to Divide")));
	}

	TEST_METHOD(ModuloByZero)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("SDK runtime modulo-by-zero test should create a standalone engine")));

		FScopedNativeModule Module(*TestRunner, Engine, "SDKRuntimeModuloByZero", R"(
int Entry()
{
	int a = 7;
	int b = 0;
	return a % b;
}
)");
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "int Entry()");
		ASSERT_THAT(IsNotNull(Function,
			TEXT("SDK runtime modulo-by-zero test should resolve the entry function")));

		asIScriptContext* Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			TEXT("SDK runtime modulo-by-zero test should create a context")));
		ON_SCOPE_EXIT { Context->Release(); };

		const int ExecuteResult = PrepareAndExecute(Context, Function);
		const FString ExceptionString = UTF8_TO_TCHAR(Context->GetExceptionString() != nullptr ? Context->GetExceptionString() : "");

		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), ExecuteResult,
			TEXT("SDK runtime modulo-by-zero test should raise an execution exception")));

		ASSERT_THAT(AreEqual(FString(TEXT("Divide by zero")), ExceptionString,
			TEXT("SDK runtime modulo-by-zero test should report the divide-by-zero exception text")));
	}

	TEST_METHOD(ContextReuseAfterException)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("SDK runtime context-reuse test should create a standalone engine")));

		FScopedNativeModule Module(*TestRunner, Engine, "SDKRuntimeContextReuse", R"(
int Boom()
{
	int a = 0;
	return 1 / a;
}

int SafeSum()
{
	int total = 0;
	for (int i = 1; i <= 5; i++) { total += i; }
	return total;
}
)");
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptFunction* BoomFn = GetNativeFunctionByDecl(Module, "int Boom()");
		asIScriptFunction* SafeFn = GetNativeFunctionByDecl(Module, "int SafeSum()");
		ASSERT_THAT(IsNotNull(BoomFn,
			TEXT("SDK runtime context-reuse test should resolve Boom")));
		ASSERT_THAT(IsNotNull(SafeFn,
			TEXT("SDK runtime context-reuse test should resolve SafeSum")));

		asIScriptContext* Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			TEXT("SDK runtime context-reuse test should create a context")));
		ON_SCOPE_EXIT { Context->Release(); };

		// First execution throws.
		const int FirstResult = PrepareAndExecute(Context, BoomFn);
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), FirstResult,
			TEXT("SDK runtime context-reuse test should throw on the first call")));

		// The same context must be reusable for a fresh, successful execution.
		const int SecondResult = PrepareAndExecute(Context, SafeFn);
		const int32 Sum = static_cast<int32>(Context->GetReturnDWord());

		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), SecondResult,
			TEXT("SDK runtime context-reuse test should finish the second call after re-Prepare")));
		ASSERT_THAT(AreEqual(15, Sum,
			TEXT("SDK runtime context-reuse test should compute SafeSum = 15 after recovering from exception")));
	}
};

#endif
