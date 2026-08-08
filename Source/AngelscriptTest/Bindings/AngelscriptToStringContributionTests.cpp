#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptToStringContributionTests,
	"Angelscript.TestModule.Bindings.ToStringContribution",
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

	TEST_METHOD(DirectProviderContributionsAreConsumed)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASToString_DirectProviderContributions"), ASTEST_AS(R"AS(
			int DirectProviderToStringMethodsExist()
			{
				FColor Color(1, 2, 3, 4);
				FVector Vector(1.0, 2.0, 3.0);
				FRandomStream Stream(123);
				FIntPoint Point(4, 9);
				FDateTime DateTime(2024, 12, 25);
				FIntVector IntVector(1, 2, 3);
				FIntVector2 IntVector2(4, 5);
				FIntVector4 IntVector4(6, 7, 8, 9);
				FBox Box(FVector(-1.0, -2.0, -3.0), FVector(1.0, 2.0, 3.0));
				FBox3f Box3f(FVector3f(-1.0f, -2.0f, -3.0f), FVector3f(1.0f, 2.0f, 3.0f));
				FBoxSphereBounds Bounds(FVector(0.0, 0.0, 0.0), FVector(1.0, 2.0, 3.0), 4.0);
				FBoxSphereBounds3f Bounds3f(FVector3f(0.0f, 0.0f, 0.0f), FVector3f(1.0f, 2.0f, 3.0f), 4.0f);
				FVector4 Vector4(1.0, 2.0, 3.0, 4.0);
				FVector4f Vector4f(5.0f, 6.0f, 7.0f, 8.0f);
				FVector2D Vector2D(1.0, 2.0);
				FVector2f Vector2f(3.0f, 4.0f);
				FPrimaryAssetType PrimaryAssetType(n"DirectBindAsset");
				FPrimaryAssetId PrimaryAssetId("DirectBindAsset:Entry");
				FQuat Quat(0.0, 0.0, 0.0, 1.0);
				FQuat4f Quat4f(0.0f, 0.0f, 0.0f, 1.0f);
				FRotator Rotator(10.0, 20.0, 30.0);
				FRotator3f Rotator3f(10.0f, 20.0f, 30.0f);

				return Color.ToString().IsEmpty()
					|| Vector.ToString().IsEmpty()
					|| Stream.ToString().IsEmpty()
					|| Point.ToString().IsEmpty()
					|| DateTime.ToString().IsEmpty()
					|| IntVector.ToString().IsEmpty()
					|| IntVector2.ToString().IsEmpty()
					|| IntVector4.ToString().IsEmpty()
					|| Box.ToString().IsEmpty()
					|| Box3f.ToString().IsEmpty()
					|| Bounds.ToString().IsEmpty()
					|| Bounds3f.ToString().IsEmpty()
					|| Vector4.ToString().IsEmpty()
					|| Vector4f.ToString().IsEmpty()
					|| Vector2D.ToString().IsEmpty()
					|| Vector2f.ToString().IsEmpty()
					|| PrimaryAssetType.ToString().IsEmpty()
					|| PrimaryAssetId.ToString().IsEmpty()
					|| Quat.ToString().IsEmpty()
					|| Quat4f.ToString().IsEmpty()
					|| Rotator.ToString().IsEmpty()
					|| Rotator3f.ToString().IsEmpty()
					? 0
					: 1;
			}
			)AS"));

		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("Direct-provider ToString methods should compile after contribution finalization")));
		if (!Module.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(
				*TestRunner,
				Engine,
				Module.GetModule(),
				TEXT("int DirectProviderToStringMethodsExist()"),
				TEXT("Every migrated formatter contribution should become a script-visible ToString method"),
				1),
			TEXT("Direct-provider ToString methods should return non-empty values")));
	}
};

#endif
