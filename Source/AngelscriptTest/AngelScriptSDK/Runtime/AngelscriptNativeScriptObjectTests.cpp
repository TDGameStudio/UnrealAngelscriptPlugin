#include "../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../Support/AngelscriptNativeCaseTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FScriptObjectTests,
	"Angelscript.TestModule.AngelScriptSDK.Runtime.ScriptObject",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FObject
	{
		int32 Value = 0;
	};

	static void ConstructObject(FObject* Address)
	{
		new (Address) FObject();
	}

	static void DestructObject(FObject* Address)
	{
		Address->~FObject();
	}

public:
	TEST_METHOD(ScriptObjectValueType)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_NON_PRODUCT("AggregateSupport",
			"The native value construction, property mutation, and copy compatibility path is retained beneath RT-OBJ-CONSTRUCT-COPY-ASSIGN-PROPERTY.");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Script object value-type test should create a standalone engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const ASAutoCaller::FunctionCaller ConstructorCaller = ASAutoCaller::MakeFunctionCaller(ConstructObject);
		const ASAutoCaller::FunctionCaller DestructorCaller = ASAutoCaller::MakeFunctionCaller(DestructObject);
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"Object",
				sizeof(FObject),
				asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<FObject>() | asOBJ_APP_CLASS_ALLINTS) >= 0,
			TEXT("Script object value-type test should register the value type")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectBehaviour(
				"Object",
				asBEHAVE_CONSTRUCT,
				"void f()",
				asFUNCTION(ConstructObject),
				asCALL_CDECL_OBJLAST,
				*(asFunctionCaller*)&ConstructorCaller) >= 0,
			TEXT("Script object value-type test should register construction")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectBehaviour(
				"Object",
				asBEHAVE_DESTRUCT,
				"void f()",
				asFUNCTION(DestructObject),
				asCALL_CDECL_OBJLAST,
				*(asFunctionCaller*)&DestructorCaller) >= 0,
			TEXT("Script object value-type test should register destruction")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectProperty(
				"Object",
				"int Value",
				asOFFSET(FObject, Value)) >= 0,
			TEXT("Script object value-type test should register the value property")));

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			bool CopyObjectValue()
			{
				Object Value;
				Value.Value = 10;
				Object Copy = Value;
				return Copy.Value == 10 && Value.Value == 10;
			}
			)AS");
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("RT-OBJ-CONSTRUCT-COPY-ASSIGN-PROPERTY-NATIVE-VALUE-COMPAT"),
			TEXT("ScriptObjectValueType"),
			UTF8_TO_TCHAR(ScriptSource.c_str()));
		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"ScriptObjectValueType",
			ScriptSource);
		if (!Module.IsValid())
		{
			return;
		}
		FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "bool CopyObjectValue()");
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("Script object value-type test should resolve its exact entry declaration")));
		if (Invoker.IsValid())
		{
			ASSERT_THAT(IsTrue(
				Invoker.CallAndReturn<bool>(false),
				TEXT("Script object value-type test should copy registered native values")));
		}
	}
};

#endif
