#include "CQTest.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptMatrixBindingsTest,
	"Angelscript.TestModule.Bindings.Matrix",
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

	TEST_METHOD(MatrixValueOperations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASMatrix_ValueOperations"), ASTEST_AS(R"AS(
			int MatrixValueOperations()
			{
				FMatrix Matrix = FMatrix::Zero();
				Matrix.SetIdentity();
				Matrix.SetOrigin(FVector(10.0, 20.0, 30.0));

				FVector4 Position = Matrix.TransformPosition(FVector(1.0, 2.0, 3.0));
				FVector4 Vector = Matrix.TransformVector(FVector(1.0, 2.0, 3.0));
				if (Position.X != 11.0 || Position.Y != 22.0 || Position.Z != 33.0 || Position.W != 1.0
					|| Vector.X != 1.0 || Vector.Y != 2.0 || Vector.Z != 3.0 || Vector.W != 0.0)
				{
					return 10;
				}

				FVector XAxis;
				FVector YAxis;
				FVector ZAxis;
				Matrix.GetScaledAxes(XAxis, YAxis, ZAxis);
				Matrix.SetAxis(0, FVector(2.0, 0.0, 0.0));
				Matrix.SetColumn(3, FVector(10.0, 20.0, 30.0));
				FMatrix Product = Matrix * FMatrix::Identity();
				FMatrix Weighted = Product * 0.5;
				FMatrix Inverse = Product.InverseFast();
				FPlane NearPlane;
				Product.GetFrustumNearPlane(NearPlane);

				return Matrix.GetOrigin() == FVector(10.0, 20.0, 30.0)
					&& XAxis == FVector(1.0, 0.0, 0.0)
					&& Weighted.GetMaximumAxisScale() == 1.0
					&& Inverse.ContainsNaN() == false
					&& Product.ComputeHash() != 0 ? 1 : 20;
			}
			)AS"));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, ModuleScope.GetModule(), TEXT("int MatrixValueOperations()"), TEXT("FMatrix should expose UE 5.8 matrix operations with FVector4 transform results"), 1),
			TEXT("FMatrix operations should be callable from AngelScript")));
	}
};

#endif
