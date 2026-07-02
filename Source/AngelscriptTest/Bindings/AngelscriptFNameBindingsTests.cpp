// AngelscriptFNameBindingsTests.cpp
// CQTest coverage for FName binding.
// Automation IDs: Angelscript.TestModule.Bindings.FName.*

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#if WITH_ANGELSCRIPT_UNITTESTS



TEST_CLASS_WITH_FLAGS(FAngelscriptFNameBindingsTest,
	"Angelscript.TestModule.Bindings.FName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}
	AFTER_ALL()
	{
		FAngelscriptEngine& E = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(E);
	}

	TEST_METHOD(FNameConstruction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASFName_Ctor"), ASTEST_AS(R"AS(
			int FName_ConstructAndIsNone()
			{
				FName N = n"TestName";
				return N.IsNone() ? 0 : 1;
			}
			)AS"));
		if (!Mod.IsValid())
		{
			TestRunner->AddInfo(TEXT("FName not available, skipping"));
			return;
		}
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), TEXT("int FName_ConstructAndIsNone()"), TEXT("Constructed FName is not None"), 1),
			TEXT("ExpectGlobalInt should pass")));
	}

	TEST_METHOD(FNameNone)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASFName_None"), ASTEST_AS(R"AS(
			int FName_NoneIsNone()
			{
				FName N;
				return N.IsNone() ? 1 : 0;
			}
			)AS"));
		if (!Mod.IsValid())
		{
			TestRunner->AddInfo(TEXT("FName not available, skipping"));
			return;
		}
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), TEXT("int FName_NoneIsNone()"), TEXT("Default FName is None"), 1),
			TEXT("ExpectGlobalInt should pass")));
	}

	TEST_METHOD(FNameEquality)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASFName_Equal"), ASTEST_AS(R"AS(
			int FName_EqualityCheck()
			{
				FName A = n"Hello";
				FName B = n"Hello";
				return (A == B) ? 1 : 0;
			}
			)AS"));
		if (!Mod.IsValid())
		{
			TestRunner->AddInfo(TEXT("FName equality not available, skipping"));
			return;
		}
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), TEXT("int FName_EqualityCheck()"), TEXT("Same FName values are equal"), 1),
			TEXT("ExpectGlobalInt should pass")));
	}

	TEST_METHOD(FNameToString)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASFName_ToString"), ASTEST_AS(R"AS(
			int FName_ToStringLen()
			{
				FName N = n"TestName";
				FString S = N.ToString();
				return S.Len();
			}
			)AS"));
		if (!Mod.IsValid())
		{
			TestRunner->AddInfo(TEXT("FName.ToString not available, skipping"));
			return;
		}
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), TEXT("int FName_ToStringLen()"), TEXT("FName ToString length is 8"), 8),
			TEXT("ExpectGlobalInt should pass")));
	}
};

#endif
