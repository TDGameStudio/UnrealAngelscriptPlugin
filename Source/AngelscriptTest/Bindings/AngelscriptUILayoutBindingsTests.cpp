// =============================================================================
// AngelscriptUILayoutBindingsTests.cpp
//
// CQTest coverage for FAnchors, FMargin bindings.
// Automation IDs: Angelscript.TestModule.Bindings.UILayout.*
// =============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestModuleScope.h"
#include "Shared/AngelscriptTestExecute.h"

#if WITH_DEV_AUTOMATION_TESTS



TEST_CLASS_WITH_FLAGS(FAngelscriptUILayoutBindingsTest,
	"Angelscript.TestModule.Bindings.UILayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	TEST_METHOD(FMarginBasics)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASUILayout_Margin"), TEXT(R"(
int Margin_DefaultZero()
{
	FMargin M;
	return (M.Left == 0.0 && M.Top == 0.0 && M.Right == 0.0 && M.Bottom == 0.0) ? 1 : 0;
}
int Margin_UniformCtor()
{
	FMargin M = FMargin(5.0);
	return (M.Left == 5.0 && M.Top == 5.0 && M.Right == 5.0 && M.Bottom == 5.0) ? 1 : 0;
}
int Margin_ComponentCtor()
{
	FMargin M = FMargin(1.0, 2.0, 3.0, 4.0);
	return (M.Left == 1.0 && M.Top == 2.0 && M.Right == 3.0 && M.Bottom == 4.0) ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int Margin_DefaultZero()"),    TEXT("Default FMargin is zero"), 1 },
			{ TEXT("int Margin_UniformCtor()"),    TEXT("Uniform FMargin ctor"), 1 },
			{ TEXT("int Margin_ComponentCtor()"),  TEXT("Component FMargin ctor"), 1 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}

	TEST_METHOD(FAnchorsBasics)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASUILayout_Anchors"), TEXT(R"(
int Anchors_DefaultZero()
{
	FAnchors A;
	return (A.Minimum.X == 0.0 && A.Minimum.Y == 0.0 && A.Maximum.X == 0.0 && A.Maximum.Y == 0.0) ? 1 : 0;
}
int Anchors_UniformCtor()
{
	FAnchors A = FAnchors(0.5, 0.5);
	return (A.Minimum.X == 0.5 && A.Minimum.Y == 0.5 && A.Maximum.X == 0.5 && A.Maximum.Y == 0.5) ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int Anchors_DefaultZero()"),   TEXT("Default FAnchors is zero"), 1 },
			{ TEXT("int Anchors_UniformCtor()"),   TEXT("Uniform FAnchors ctor"), 1 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
