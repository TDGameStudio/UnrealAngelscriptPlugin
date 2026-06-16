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


}


TEST_CLASS_WITH_FLAGS(FAngelscriptSDKGlobalPropertyTests,
	"Angelscript.TestModule.AngelScriptSDK.GlobalProperty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ScriptReads)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* SE = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("Should create engine"), SE)) return;
		ON_SCOPE_EXIT { DestroyNativeEngine(SE); };

		GTestValue = 42;
		int R = SE->RegisterGlobalProperty("int GTestValue", &GTestValue);
		TestRunner->TestTrue(TEXT("RegisterGlobalProperty should succeed"), R >= 0);

		asIScriptModule* M = BuildNativeModule(SE, "GPRead", "int Entry() { return GTestValue; }\n");
		if (!TestRunner->TestNotNull(TEXT("Should compile"), M)) { TestRunner->AddInfo(CollectMessages(Messages)); return; }

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "int Entry()", Result)) return;
		TestRunner->TestEqual(TEXT("Script should read C++ global value 42"), Result, 42);
	}

	TEST_METHOD(ScriptWrites)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* SE = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("Should create engine"), SE)) return;
		ON_SCOPE_EXIT { DestroyNativeEngine(SE); };

		GTestValue = 0;
		SE->RegisterGlobalProperty("int GTestValue", &GTestValue);

		asIScriptModule* M = BuildNativeModule(SE, "GPWrite", "void Entry() { GTestValue = 99; }\n");
		if (!TestRunner->TestNotNull(TEXT("Should compile"), M)) { TestRunner->AddInfo(CollectMessages(Messages)); return; }

		ExecuteScriptVoidFunction(*TestRunner, SE, M, "void Entry()");
		TestRunner->TestEqual(TEXT("C++ should see script-written value 99"), GTestValue, 99);
	}

	TEST_METHOD(MultipleGlobals)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* SE = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("Should create engine"), SE)) return;
		ON_SCOPE_EXIT { DestroyNativeEngine(SE); };

		GTestA = 10;
		GTestB = 20;
		SE->RegisterGlobalProperty("int GTestA", &GTestA);
		SE->RegisterGlobalProperty("int GTestB", &GTestB);

		asIScriptModule* M = BuildNativeModule(SE, "GPMulti", "int Entry() { return GTestA + GTestB; }\n");
		if (!TestRunner->TestNotNull(TEXT("Should compile"), M)) { TestRunner->AddInfo(CollectMessages(Messages)); return; }

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "int Entry()", Result)) return;
		TestRunner->TestEqual(TEXT("Script reads both globals: 10+20=30"), Result, 30);
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
