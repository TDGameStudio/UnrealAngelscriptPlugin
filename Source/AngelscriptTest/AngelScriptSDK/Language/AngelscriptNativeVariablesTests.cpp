#include "../Support/AngelscriptNativeExecutionTestSupport.h"
#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FVariablesTests, "Angelscript.TestModule.AngelScriptSDK.Language.Variables", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(InitializerExpression)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"LANG-VAR-INIT-STORAGE and LANG-EXPR-EVAL-ORDER supersede this one const-global initializer with type/storage/initializer and composed-expression order products");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		FScopedNativeModule Module(*TestRunner, Engine, "VariablesInitializer", ASTEST_AS_ANSI(R"AS(
			const int computed = 10 * 3 + 7;
		)AS"));
		if (!Module.IsValid())
		{
			return;
		}
		const int* const Value = static_cast<const int*>(Module->GetAddressOfGlobalVar(0));
		ASSERT_THAT(IsNotNull(Value, TEXT("Initializer expression should expose global storage")));
		if (Value != nullptr)
		{
			ASSERT_THAT(AreEqual(37, *Value, TEXT("Initializer expression should evaluate at module build time")));
		}
	}

	TEST_METHOD(VariablesConstReadAccess)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"LANG-VAR-INIT-STORAGE, LANG-VAR-ASSIGN-TARGET, and TYPE-GLOBAL-PROPERTY-ACCESS-SHAPES supersede this const-global read sample with metadata, storage, access, runtime, and mutation/rejection products");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		FScopedNativeModule Module(*TestRunner, Engine, "VariablesConstRead", ASTEST_AS_ANSI(R"AS(
			const int limit = 200;

			int Entry()
			{
				return limit * 2;
			}
		)AS"));
		if (!Module.IsValid())
		{
			return;
		}
		FSdkFunctionInvoker Invoker(*TestRunner, Engine.Get(), Module, "int Entry()");
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("Const global test should resolve its entry")));
		if (Invoker.IsValid())
		{
			ASSERT_THAT(AreEqual(400, Invoker.CallAndReturn<int32>(INDEX_NONE), TEXT("Const global should remain readable from script")));
		}
	}

	TEST_METHOD(DeclarationString)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"LANG-VAR-INIT-STORAGE and MOD-GLOBAL-STATE-LIFECYCLE supersede this non-empty declaration smoke with exact type/storage/initializer and module inventory metadata");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		FScopedNativeModule Module(*TestRunner, Engine, "VariablesDeclaration", ASTEST_AS_ANSI(R"AS(
			const double pi = 3.14159;
			const int answer = 42;
		)AS"));
		if (!Module.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(2, static_cast<int32>(Module->GetGlobalVarCount()), TEXT("Declaration test should enumerate both globals")));
		for (asUINT Index = 0; Index < Module->GetGlobalVarCount(); ++Index)
		{
			ASSERT_THAT(IsTrue(
				Module->GetGlobalVarDeclaration(Index) != nullptr
					&& std::strlen(Module->GetGlobalVarDeclaration(Index)) > 0,
				TEXT("Declaration test should return non-empty declarations")));
		}
	}
};
#endif
