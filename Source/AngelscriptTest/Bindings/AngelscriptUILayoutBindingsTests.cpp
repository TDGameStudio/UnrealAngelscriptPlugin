// =============================================================================
// AngelscriptUILayoutBindingsTests.cpp
//
// CQTest coverage for FAnchors, FMargin bindings.
// Automation IDs: Angelscript.TestModule.Bindings.UILayout.*
// =============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#if WITH_ANGELSCRIPT_UNITTESTS



TEST_CLASS_WITH_FLAGS(FAngelscriptUILayoutBindingsTest,
	"Angelscript.TestModule.Bindings.UILayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(FMarginBasics)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASUILayout_Margin"), ASTEST_AS(R"AS(
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
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int Margin_DefaultZero()"),    TEXT("Default FMargin is zero"), 1 },
			{ TEXT("int Margin_UniformCtor()"),    TEXT("Uniform FMargin ctor"), 1 },
			{ TEXT("int Margin_ComponentCtor()"),  TEXT("Component FMargin ctor"), 1 },
		};
		ASSERT_THAT(IsTrue(
			ExpectGlobalInts(*TestRunner, Engine, M,  Cases),
			TEXT("ExpectGlobalInts should pass")));
	}

	TEST_METHOD(FAnchorsBasics)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASUILayout_Anchors"), ASTEST_AS(R"AS(
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
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int Anchors_DefaultZero()"),   TEXT("Default FAnchors is zero"), 1 },
			{ TEXT("int Anchors_UniformCtor()"),   TEXT("Uniform FAnchors ctor"), 1 },
		};
		ASSERT_THAT(IsTrue(
			ExpectGlobalInts(*TestRunner, Engine, M,  Cases),
			TEXT("ExpectGlobalInts should pass")));
	}

	TEST_METHOD(FGeometryMethodsCompileTogether)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASUILayout_Geometry"), ASTEST_AS(R"AS(
			FVector2D ExerciseGeometry(const FGeometry& Geometry, const FVector2D& AbsolutePosition)
			{
				FVector2D LocalPosition = Geometry.AbsoluteToLocal(AbsolutePosition);
				FVector2D RoundTrip = Geometry.LocalToAbsolute(LocalPosition);
				FGeometry Child = Geometry.MakeChild(LocalPosition, Geometry.GetLocalSize());
				return RoundTrip + Geometry.GetAbsoluteSize() + Child.GetLocalSize();
			}
			)AS"));

		ASSERT_THAT(IsTrue(Mod.IsValid(), TEXT("FGeometry method signatures should compile together")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
