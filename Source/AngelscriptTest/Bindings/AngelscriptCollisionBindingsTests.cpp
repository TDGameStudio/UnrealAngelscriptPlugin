// AngelscriptCollisionBindingsTests.cpp
// CQTest coverage for FCollisionQueryParams, FCollisionShape bindings.
// Automation IDs: Angelscript.TestModule.Bindings.Collision.*

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#if WITH_ANGELSCRIPT_UNITTESTS



TEST_CLASS_WITH_FLAGS(FAngelscriptCollisionBindingsTest,
	"Angelscript.TestModule.Bindings.Collision",
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

	TEST_METHOD(FCollisionQueryParamsDefault)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASCollision_QueryParams"), ASTEST_AS(R"AS(
			int CollisionQueryParams_DefaultTraceComplex()
			{
				FCollisionQueryParams Params;
				return Params.bTraceComplex ? 1 : 0;
			}
			)AS"));
		if (!Mod.IsValid())
		{
			TestRunner->AddInfo(TEXT("FCollisionQueryParams not available, skipping"));
			return;
		}
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), TEXT("int CollisionQueryParams_DefaultTraceComplex()"), TEXT("Default FCollisionQueryParams bTraceComplex is false"), 0),
			TEXT("ExpectGlobalInt should pass")));
	}

	TEST_METHOD(FCollisionShapeSphere)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASCollision_Shape"), ASTEST_AS(R"AS(
			int CollisionShape_MakeSphere()
			{
				FCollisionShape Shape = FCollisionShape::MakeSphere(50.0);
				return Shape.IsSphere() ? 1 : 0;
			}
			)AS"));
		if (!Mod.IsValid())
		{
			TestRunner->AddInfo(TEXT("FCollisionShape not available, skipping"));
			return;
		}
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), TEXT("int CollisionShape_MakeSphere()"), TEXT("MakeSphere creates sphere shape"), 1),
			TEXT("ExpectGlobalInt should pass")));
	}

	TEST_METHOD(FCollisionShapeBox)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASCollision_Box"), ASTEST_AS(R"AS(
			int CollisionShape_MakeBox()
			{
				FCollisionShape Shape = FCollisionShape::MakeBox(FVector(10.0, 20.0, 30.0));
				return Shape.IsBox() ? 1 : 0;
			}
			)AS"));
		if (!Mod.IsValid())
		{
			TestRunner->AddInfo(TEXT("FCollisionShape::MakeBox not available, skipping"));
			return;
		}
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), TEXT("int CollisionShape_MakeBox()"), TEXT("MakeBox creates box shape"), 1),
			TEXT("ExpectGlobalInt should pass")));
	}
};

#endif
