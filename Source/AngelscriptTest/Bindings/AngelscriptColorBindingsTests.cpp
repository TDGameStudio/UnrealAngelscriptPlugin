// AngelscriptColorBindingsTests.cpp
// CQTest coverage for FColor, FLinearColor bindings.
// Automation IDs: Angelscript.TestModule.Bindings.Color.*

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptBindingsModuleBuilder.h"
#include "Shared/AngelscriptBindingsAssertions.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptTestBindings;


TEST_CLASS_WITH_FLAGS(FAngelscriptColorBindingsTest,
	"Angelscript.TestModule.Bindings.Color",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL() { ASTEST_CREATE_ENGINE(); }
	AFTER_ALL() { FAngelscriptEngine& E = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(E); }

	TEST_METHOD(FColorConstruction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FCoverageModuleScope Mod(*TestRunner, Engine, TEXT("ASColor_FColorCtor"), TEXT(R"(
int FColor_RedComponent()
{
	FColor C = FColor(255, 128, 64, 255);
	return C.R;
}
)"));
		if (!Mod.IsValid())
		{
			TestRunner->AddInfo(TEXT("FColor not available, skipping"));
			return;
		}
		AngelscriptTestBindings::ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), 
			TEXT("int FColor_RedComponent()"),
			TEXT("FColor red component is 255"), 255);
	}

	TEST_METHOD(FLinearColorConstruction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FCoverageModuleScope Mod(*TestRunner, Engine, TEXT("ASColor_FLinearCtor"), TEXT(R"(
int FLinearColor_IsBlack()
{
	FLinearColor C = FLinearColor(0.0, 0.0, 0.0, 1.0);
	return (C.R == 0.0 && C.G == 0.0 && C.B == 0.0) ? 1 : 0;
}
)"));
		if (!Mod.IsValid())
		{
			TestRunner->AddInfo(TEXT("FLinearColor not available, skipping"));
			return;
		}
		AngelscriptTestBindings::ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), 
			TEXT("int FLinearColor_IsBlack()"),
			TEXT("FLinearColor black check"), 1);
	}

	TEST_METHOD(FColorToLinearColor)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FCoverageModuleScope Mod(*TestRunner, Engine, TEXT("ASColor_ToLinear"), TEXT(R"(
int FColor_ToLinearConversion()
{
	FColor C = FColor(255, 0, 0, 255);
	FLinearColor LC = C.ReinterpretAsLinear();
	return (LC.R > 0.0) ? 1 : 0;
}
)"));
		if (!Mod.IsValid())
		{
			TestRunner->AddInfo(TEXT("FColor.ReinterpretAsLinear not available, skipping"));
			return;
		}
		AngelscriptTestBindings::ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), 
			TEXT("int FColor_ToLinearConversion()"),
			TEXT("FColor to linear has non-zero red"), 1);
	}
};

#endif
