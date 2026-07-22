#include "../Support/AngelscriptNativeExecutionTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace ScriptObjectTest
{
	struct FObject { int32 Value = 0; };
	static void ConstructObject(FObject* Address) { new (Address) FObject(); }
	static void DestructObject(FObject* Address) { Address->~FObject(); }
}

TEST_CLASS_WITH_FLAGS(FScriptObjectTests,
	"Angelscript.TestModule.AngelScriptSDK.Runtime.ScriptObject",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ScriptObjectValueType)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Script object value-type test should create a standalone engine")));
		const ASAutoCaller::FunctionCaller ConstructorCaller = ASAutoCaller::MakeFunctionCaller(ScriptObjectTest::ConstructObject);
		const ASAutoCaller::FunctionCaller DestructorCaller = ASAutoCaller::MakeFunctionCaller(ScriptObjectTest::DestructObject);
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectType("Object", sizeof(ScriptObjectTest::FObject), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<ScriptObjectTest::FObject>() | asOBJ_APP_CLASS_ALLINTS) >= 0, TEXT("Script object value-type test should register the value type")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectBehaviour("Object", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(ScriptObjectTest::ConstructObject), asCALL_CDECL_OBJLAST, *(asFunctionCaller*)&ConstructorCaller) >= 0, TEXT("Script object value-type test should register construction")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectBehaviour("Object", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(ScriptObjectTest::DestructObject), asCALL_CDECL_OBJLAST, *(asFunctionCaller*)&DestructorCaller) >= 0, TEXT("Script object value-type test should register destruction")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectProperty("Object", "int Value", asOFFSET(ScriptObjectTest::FObject, Value)) >= 0, TEXT("Script object value-type test should register the value property")));
		FScopedNativeModule Module(*TestRunner, Engine, "ScriptObjectValueType", "bool CopyObjectValue() { Object value; value.Value = 10; Object copy = value; return copy.Value == 10 && value.Value == 10; }");
		if (!Module.IsValid()) return;
		FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "bool CopyObjectValue()");
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("Script object value-type test should resolve its exact entry declaration")));
		if (Invoker.IsValid()) ASSERT_THAT(IsTrue(Invoker.CallAndReturn<bool>(false), TEXT("Script object value-type test should copy registered native values")));
	}
};

#endif
