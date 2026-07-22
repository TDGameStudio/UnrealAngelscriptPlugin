#include "Support/AngelscriptNativeExecutionTestSupport.h"

#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FEnumTypeTests,
	"Angelscript.TestModule.AngelScriptSDK.TypeSystem.Enums",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(EnumTypeEnum)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Enum type test should create a standalone engine")));

		ASSERT_THAT(IsTrue(ScriptEngine->RegisterEnum("myenum") >= 0, TEXT("Enum type test should register the global enum")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterEnumValue("myenum", "value", 1) >= 0, TEXT("Enum type test should register the global enum value")));
		ASSERT_THAT(AreEqual(asSUCCESS, ScriptEngine->SetDefaultNamespace("foo"), TEXT("Enum type test should select a namespace before namespace-local registration")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterEnum("myenum") >= 0, TEXT("Enum type test should permit a namespace-local enum with the same short name")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterEnumValue("myenum", "value", 1) >= 0, TEXT("Enum type test should register the namespace-local enum value")));
		ASSERT_THAT(AreEqual(asSUCCESS, ScriptEngine->SetDefaultNamespace(""), TEXT("Enum type test should restore the global namespace")));

		ASSERT_THAT(IsTrue(ScriptEngine->RegisterEnum("TEST_ENUM") >= 0, TEXT("Enum type test should register a second enum")));
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterEnumValue("TEST_ENUM", "ENUM1", 1) >= 0, TEXT("Enum type test should register its value")));

		FScopedNativeModule Module(*TestRunner, Engine, "EnumTypeEnum", ASTEST_AS_ANSI(R"AS(
			enum LocalEnum
			{
				LocalValue = 1
			}

			int ReturnLocalEnumValue()
			{
				LocalEnum Value = LocalEnum::LocalValue;
				return Value == LocalEnum::LocalValue ? 1 : 0;
			}
			)AS"));
		if (!Module.IsValid())
		{
			return;
		}

		FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int ReturnLocalEnumValue()");
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("Enum type test should resolve its exact script entry declaration")));
		if (Invoker.IsValid())
		{
			ASSERT_THAT(AreEqual(1, Invoker.CallAndReturn<int32>(INDEX_NONE), TEXT("Enum type test should execute local-enum equality")));
		}

		const int TypeId = ScriptEngine->GetTypeIdByDecl("TEST_ENUM");
		ASSERT_THAT(IsTrue(TypeId >= 0, TEXT("Enum type test should resolve the registered enum type id")));
		ASSERT_THAT(AreEqual(FString(TEXT("TEST_ENUM")), FString(UTF8_TO_TCHAR(ScriptEngine->GetTypeDeclaration(TypeId))),
			TEXT("Enum type test should preserve its registered declaration")));
	}

	TEST_METHOD(EnumUnderlyingValues)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Enum underlying-values test should create a standalone engine")));

		FScopedNativeModule Module(*TestRunner, Engine, "EnumUnderlyingValues", ASTEST_AS_ANSI(R"AS(
			enum EFlags { None = 0, A = 1, B = 2, C = 4, D = 8 }

			bool CheckEnumUnderlyingValues()
			{
				int composed = int(EFlags::A) | int(EFlags::B);
				return int(EFlags::None) == 0 && int(EFlags::C) == 4 && composed == 3
					&& EFlags(composed) == EFlags(3) && int(EFlags::D) > int(EFlags::C);
			}
			)AS"));
		if (!Module.IsValid())
		{
			return;
		}

		FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "bool CheckEnumUnderlyingValues()");
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("Enum underlying-values test should resolve its exact script entry declaration")));
		if (Invoker.IsValid())
		{
			ASSERT_THAT(IsTrue(Invoker.CallAndReturn<bool>(false), TEXT("Enum underlying-values test should preserve conversion and flag composition")));
		}
	}
};

#endif
