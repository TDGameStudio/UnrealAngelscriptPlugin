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
	inline static FNativeTestEngine Engine;

	BEFORE_ALL()
	{
		Engine.Create(*TestRunner);
		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FNoDiscardAsserter Assert(*TestRunner);
		if (!Assert.IsTrue(ScriptEngine->RegisterGlobalProperty("int GTestValue", &GTestValue) >= 0,
			TEXT("RegisterGlobalProperty int GTestValue should succeed")))
		{
			return;
		}
		if (!Assert.IsTrue(ScriptEngine->RegisterGlobalProperty("int GTestA", &GTestA) >= 0,
			TEXT("RegisterGlobalProperty int GTestA should succeed")))
		{
			return;
		}
		if (!Assert.IsTrue(ScriptEngine->RegisterGlobalProperty("int GTestB", &GTestB) >= 0,
			TEXT("RegisterGlobalProperty int GTestB should succeed")))
		{
			return;
		}
		if (!Assert.IsTrue(ScriptEngine->RegisterGlobalProperty("double GScalar", &GTestDouble) >= 0,
			TEXT("RegisterGlobalProperty double GScalar should succeed")))
		{
			return;
		}
		if (!Assert.IsTrue(ScriptEngine->RegisterGlobalProperty("double GTestDouble", &GTestDouble) >= 0,
			TEXT("RegisterGlobalProperty double GTestDouble should succeed")))
		{
			return;
		}
		if (!Assert.IsTrue(ScriptEngine->RegisterGlobalProperty("bool GTestBool", &GTestBool) >= 0,
			TEXT("RegisterGlobalProperty bool GTestBool should succeed")))
		{
			return;
		}
	}

	AFTER_ALL()
	{
		Engine.Destroy();
	}

	BEFORE_EACH()
	{
		Engine.ResetMessages();
		GTestValue = 0;
		GTestA = 0;
		GTestB = 0;
		GTestDouble = 0.0;
		GTestBool = false;
	}

	TEST_METHOD(ScriptReads)
	{
		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));

		GTestValue = 42;

		FScopedNativeModule M(*TestRunner, Engine, "GPRead", "int Entry() { return GTestValue; }\n");
		if (!M.IsValid()) return;

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "int Entry()", Result)) return;
		ASSERT_THAT(AreEqual(42, Result, TEXT("Script should read C++ global value 42")));
	}

	TEST_METHOD(ScriptWrites)
	{
		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));

		GTestValue = 0;

		FScopedNativeModule M(*TestRunner, Engine, "GPWrite", "void Entry() { GTestValue = 99; }\n");
		if (!M.IsValid()) return;

		ExecuteScriptVoidFunction(*TestRunner, SE, M, "void Entry()");
		ASSERT_THAT(AreEqual(99, GTestValue, TEXT("C++ should see script-written value 99")));
	}

	TEST_METHOD(MultipleGlobals)
	{
		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));

		GTestA = 10;
		GTestB = 20;

		FScopedNativeModule M(*TestRunner, Engine, "GPMulti", "int Entry() { return GTestA + GTestB; }\n");
		if (!M.IsValid()) return;

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "int Entry()", Result)) return;
		ASSERT_THAT(AreEqual(30, Result, TEXT("Script reads both globals: 10+20=30")));
	}

	TEST_METHOD(ScalarReadModifyWrite)
	{
		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));

		// This fork runs with asEP_FLOAT_IS_FLOAT64=1: the script-level scalar
		// float type is registered as `double` (8 bytes). Registering a global
		// property declared `float` is rejected (asINVALID_DECLARATION); the
		// supported scalar floating declaration is `double` backed by a C++ double.
		GTestDouble = 1.5;

		FScopedNativeModule M(*TestRunner, Engine, "GPFloat", "void Entry() { GScalar = GScalar * 2.0 + 1.0; }\n");
		if (!M.IsValid()) return;

		ExecuteScriptVoidFunction(*TestRunner, SE, M, "void Entry()");
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(GTestDouble, 4.0),
			TEXT("C++ should see script-written scalar 4.0")));
	}

	TEST_METHOD(DoubleProperty)
	{
		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));

		GTestDouble = 2.5;

		FScopedNativeModule M(*TestRunner, Engine, "GPDouble", "double Entry() { return GTestDouble * 4.0; }\n");
		if (!M.IsValid()) return;

		double Result = 0.0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "double Entry()", Result)) return;
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Result, 10.0),
			TEXT("Script reads C++ double and computes 10.0")));
	}

	TEST_METHOD(BoolProperty)
	{
		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));

		GTestBool = false;

		FScopedNativeModule M(*TestRunner, Engine, "GPBool", "void Entry() { GTestBool = !GTestBool; }\n");
		if (!M.IsValid()) return;

		ExecuteScriptVoidFunction(*TestRunner, SE, M, "void Entry()");
		ASSERT_THAT(IsTrue(GTestBool, TEXT("C++ should see script-toggled bool true")));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
