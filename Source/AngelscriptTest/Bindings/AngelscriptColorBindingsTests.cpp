// AngelscriptColorBindingsTests.cpp
// CQTest coverage for FColor, FLinearColor bindings.
// Automation IDs: Angelscript.TestModule.Bindings.Color.*

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#if WITH_ANGELSCRIPT_UNITTESTS



TEST_CLASS_WITH_FLAGS(FAngelscriptColorBindingsTest,
	"Angelscript.TestModule.Bindings.Color",
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

	TEST_METHOD(FColorConstruction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASColor_FColorCtor"), ASTEST_AS(R"AS(
			int FColor_RedComponent()
			{
				FColor C = FColor(255, 128, 64, 255);
				return C.R;
			}
			)AS"));
		if (!Mod.IsValid())
		{
			TestRunner->AddInfo(TEXT("FColor not available, skipping"));
			return;
		}
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), TEXT("int FColor_RedComponent()"), TEXT("FColor red component is 255"), 255),
			TEXT("ExpectGlobalInt should pass")));
	}

	TEST_METHOD(FLinearColorConstruction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASColor_FLinearCtor"), ASTEST_AS(R"AS(
			int FLinearColor_IsBlack()
			{
				FLinearColor C = FLinearColor(0.0, 0.0, 0.0, 1.0);
				return (C.R == 0.0 && C.G == 0.0 && C.B == 0.0) ? 1 : 0;
			}
			)AS"));
		if (!Mod.IsValid())
		{
			TestRunner->AddInfo(TEXT("FLinearColor not available, skipping"));
			return;
		}
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), TEXT("int FLinearColor_IsBlack()"), TEXT("FLinearColor black check"), 1),
			TEXT("ExpectGlobalInt should pass")));
	}

	TEST_METHOD(FColorToLinearColor)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASColor_ToLinear"), ASTEST_AS(R"AS(
			int FColor_ToLinearConversion()
			{
				FColor C = FColor(255, 0, 0, 255);
				FLinearColor LC = C.ReinterpretAsLinear();
				return (LC.R > 0.0) ? 1 : 0;
			}
			)AS"));
		if (!Mod.IsValid())
		{
			TestRunner->AddInfo(TEXT("FColor.ReinterpretAsLinear not available, skipping"));
			return;
		}
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), TEXT("int FColor_ToLinearConversion()"), TEXT("FColor to linear has non-zero red"), 1),
			TEXT("ExpectGlobalInt should pass")));
	}
};

#endif
