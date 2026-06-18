#include "AngelscriptSDKTestExecutionHelpers.h"
// AngelscriptSDKGlobalPropertyTests.cpp
// Tests for as_globalproperty.cpp - global variable registration and access.
// Automation IDs: Angelscript.TestModule.AngelScriptSDK.GlobalProperty.*

#include "AngelscriptNativeTestSupport.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;

namespace
{
	static int32 GTestValue = 0;
	static int32 GTestA = 0;
	static int32 GTestB = 0;
	static double GTestDouble = 0.0;
	static bool GTestBool = false;
}


TEST_CLASS_WITH_FLAGS(FAngelscriptSDKGlobalPropertyTests,
	"Angelscript.TestModule.AngelScriptSDK.GlobalProperty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	inline static FNativeSdkEngineFixture EngineFixture;

	BEFORE_ALL()
	{
		EngineFixture.Create(*TestRunner);
		asIScriptEngine* const ScriptEngine = EngineFixture.Get();
		if (ScriptEngine == nullptr)
		{
			return;
		}

		TestRunner->TestTrue(TEXT("RegisterGlobalProperty int GTestValue should succeed"),
			ScriptEngine->RegisterGlobalProperty("int GTestValue", &GTestValue) >= 0);
		TestRunner->TestTrue(TEXT("RegisterGlobalProperty int GTestA should succeed"),
			ScriptEngine->RegisterGlobalProperty("int GTestA", &GTestA) >= 0);
		TestRunner->TestTrue(TEXT("RegisterGlobalProperty int GTestB should succeed"),
			ScriptEngine->RegisterGlobalProperty("int GTestB", &GTestB) >= 0);
		TestRunner->TestTrue(TEXT("RegisterGlobalProperty double GScalar should succeed"),
			ScriptEngine->RegisterGlobalProperty("double GScalar", &GTestDouble) >= 0);
		TestRunner->TestTrue(TEXT("RegisterGlobalProperty double GTestDouble should succeed"),
			ScriptEngine->RegisterGlobalProperty("double GTestDouble", &GTestDouble) >= 0);
		TestRunner->TestTrue(TEXT("RegisterGlobalProperty bool GTestBool should succeed"),
			ScriptEngine->RegisterGlobalProperty("bool GTestBool", &GTestBool) >= 0);
	}

	AFTER_ALL()
	{
		EngineFixture.Destroy();
	}

	BEFORE_EACH()
	{
		EngineFixture.ResetMessages();
		GTestValue = 0;
		GTestA = 0;
		GTestB = 0;
		GTestDouble = 0.0;
		GTestBool = false;
	}

	TEST_METHOD(ScriptReads)
	{
		asIScriptEngine* SE = EngineFixture.Get();
		if (!TestRunner->TestNotNull(TEXT("Should create engine"), SE)) return;

		GTestValue = 42;

		FScopedNativeModule M(*TestRunner, EngineFixture, "GPRead", "int Entry() { return GTestValue; }\n");
		if (!M.IsValid()) return;

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "int Entry()", Result)) return;
		TestRunner->TestEqual(TEXT("Script should read C++ global value 42"), Result, 42);
	}

	TEST_METHOD(ScriptWrites)
	{
		asIScriptEngine* SE = EngineFixture.Get();
		if (!TestRunner->TestNotNull(TEXT("Should create engine"), SE)) return;

		GTestValue = 0;

		FScopedNativeModule M(*TestRunner, EngineFixture, "GPWrite", "void Entry() { GTestValue = 99; }\n");
		if (!M.IsValid()) return;

		ExecuteScriptVoidFunction(*TestRunner, SE, M, "void Entry()");
		TestRunner->TestEqual(TEXT("C++ should see script-written value 99"), GTestValue, 99);
	}

	TEST_METHOD(MultipleGlobals)
	{
		asIScriptEngine* SE = EngineFixture.Get();
		if (!TestRunner->TestNotNull(TEXT("Should create engine"), SE)) return;

		GTestA = 10;
		GTestB = 20;

		FScopedNativeModule M(*TestRunner, EngineFixture, "GPMulti", "int Entry() { return GTestA + GTestB; }\n");
		if (!M.IsValid()) return;

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "int Entry()", Result)) return;
		TestRunner->TestEqual(TEXT("Script reads both globals: 10+20=30"), Result, 30);
	}

	TEST_METHOD(ScalarReadModifyWrite)
	{
		asIScriptEngine* SE = EngineFixture.Get();
		if (!TestRunner->TestNotNull(TEXT("Should create engine"), SE)) return;

		// This fork runs with asEP_FLOAT_IS_FLOAT64=1: the script-level scalar
		// float type is registered as `double` (8 bytes). Registering a global
		// property declared `float` is rejected (asINVALID_DECLARATION); the
		// supported scalar floating declaration is `double` backed by a C++ double.
		GTestDouble = 1.5;

		FScopedNativeModule M(*TestRunner, EngineFixture, "GPFloat", "void Entry() { GScalar = GScalar * 2.0 + 1.0; }\n");
		if (!M.IsValid()) return;

		ExecuteScriptVoidFunction(*TestRunner, SE, M, "void Entry()");
		TestRunner->TestTrue(TEXT("C++ should see script-written scalar 4.0"), FMath::IsNearlyEqual(GTestDouble, 4.0));
	}

	TEST_METHOD(DoubleProperty)
	{
		asIScriptEngine* SE = EngineFixture.Get();
		if (!TestRunner->TestNotNull(TEXT("Should create engine"), SE)) return;

		GTestDouble = 2.5;

		FScopedNativeModule M(*TestRunner, EngineFixture, "GPDouble", "double Entry() { return GTestDouble * 4.0; }\n");
		if (!M.IsValid()) return;

		double Result = 0.0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "double Entry()", Result)) return;
		TestRunner->TestTrue(TEXT("Script reads C++ double and computes 10.0"), FMath::IsNearlyEqual(Result, 10.0));
	}

	TEST_METHOD(BoolProperty)
	{
		asIScriptEngine* SE = EngineFixture.Get();
		if (!TestRunner->TestNotNull(TEXT("Should create engine"), SE)) return;

		GTestBool = false;

		FScopedNativeModule M(*TestRunner, EngineFixture, "GPBool", "void Entry() { GTestBool = !GTestBool; }\n");
		if (!M.IsValid()) return;

		ExecuteScriptVoidFunction(*TestRunner, SE, M, "void Entry()");
		TestRunner->TestTrue(TEXT("C++ should see script-toggled bool true"), GTestBool);
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
