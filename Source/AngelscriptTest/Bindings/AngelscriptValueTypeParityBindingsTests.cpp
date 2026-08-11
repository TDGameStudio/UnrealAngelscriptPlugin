#include "CQTest.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptValueTypeParityBindingsTest,
	"Angelscript.TestModule.Bindings.ValueTypeParity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(AssetManagerValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASValueTypeParity_AssetManager"), ASTEST_AS(R"AS(
			int AssetManagerValues()
			{
				FPrimaryAssetType Type(n"Hero");
				FPrimaryAssetId Id(Type, n"Knight");
				if (!Id.IsValid() || Id.ToString() != "Hero:Knight")
				{
					return 10;
				}

				FPrimaryAssetId Parsed = FPrimaryAssetId::ParseTypeAndName("Hero:Knight");
				if (!(Parsed == Id))
				{
					return 20;
				}

				FAssetBundleData Bundles;
				FTopLevelAssetPath Portrait("/Game/Heroes/Knight/T_KnightPortrait.T_KnightPortrait");
				Bundles.AddBundleAsset(n"UI", Portrait);

				FAssetBundleEntry Entry;
				if (!Bundles.FindEntry(n"UI", Entry)
					|| Entry.BundleName != n"UI"
					|| Entry.AssetPaths.Num() != 1
					|| Entry.AssetPaths[0] != Portrait)
				{
					return 30;
				}

				Bundles.Reset();
				return Bundles.GetNumBundles() == 0 ? 1 : 40;
			}
			)AS"));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, ModuleScope.GetModule(), TEXT("int AssetManagerValues()"), TEXT("AssetManager value bindings should construct, parse, and query copied bundle entries"), 1),
			TEXT("AssetManager value bindings should be callable from AngelScript")));
	}

	TEST_METHOD(Box2DValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASValueTypeParity_Box2D"), ASTEST_AS(R"AS(
			int Box2DValues()
			{
				FBox2D Bounds(FVector2D(0.0, 0.0), FVector2D(4.0, 6.0));
				if (Bounds.GetArea() != 24.0 || Bounds.GetCenter() != FVector2D(2.0, 3.0))
				{
					return 10;
				}

				FBox2D Expanded = Bounds.ExpandBy(1.0);
				return Expanded.IsInside(FVector2D(-0.5, 3.0)) && Bounds.Intersect(Expanded) ? 1 : 20;
			}
			)AS"));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, ModuleScope.GetModule(), TEXT("int Box2DValues()"), TEXT("FBox2D should provide construction and common geometry operations"), 1),
			TEXT("FBox2D should be callable from AngelScript")));
	}

	TEST_METHOD(FrameValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASValueTypeParity_Frame"), ASTEST_AS(R"AS(
			int FrameValues()
			{
				FFrameNumber Frame(10);
				FFrameTime Time(Frame, 0.75);
				if (Time.GetFrame().Value != 10 || Time.FloorToFrame().Value != 10 || Time.CeilToFrame().Value != 11)
				{
					return 10;
				}

				FFrameTime Doubled = Time * 2.0;
				return Doubled.RoundToFrame().Value == 22 && Time.AsDecimal() == 10.75 ? 1 : 20;
			}
			)AS"));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, ModuleScope.GetModule(), TEXT("int FrameValues()"), TEXT("Frame values should expose sub-frame construction, rounding, and arithmetic"), 1),
			TEXT("Frame values should be callable from AngelScript")));
	}

	TEST_METHOD(MatrixValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASValueTypeParity_Matrix"), ASTEST_AS(R"AS(
			int MatrixValues()
			{
				FMatrix Matrix = FMatrix::Identity();
				FVector4 Position = Matrix.TransformPosition(FVector(1.0, 2.0, 3.0));
				FMatrix Inverse = Matrix.Inverse();
				return Position == FVector4(1.0, 2.0, 3.0, 1.0) && Inverse == Matrix && Matrix.ToQuat() == FQuat::Identity ? 1 : 10;
			}
			)AS"));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, ModuleScope.GetModule(), TEXT("int MatrixValues()"), TEXT("FMatrix should expose default-alias transform and inverse operations"), 1),
			TEXT("FMatrix should be callable from AngelScript")));
	}
};

#endif
