#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageMathStructsAdditionalTypes
// -----------------------------------------------------------------------------
// Coverage for additional math types identified in Coverage_MathStructs.md:
// - FVector4: 4D vector operations
// - FIntPoint: 2D integer coordinates
// - FIntVector: 3D integer vectors
// - FColor: 8-bit RGBA colors
//
// Test patterns: Pattern B/F (global functions)
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageMathStructsAdditionalTypesTest,
	"Angelscript.TestModule.Coverage.MathStructsAdditionalTypes",
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

	// Helper
	template <typename T>
	void ExpectGlobalReturn(FAngelscriptEngine& Engine, asIScriptModule* Module, const TCHAR* Declaration, const T& Expected, const TCHAR* Message)
	{
		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
		T Result{};
		if constexpr (std::is_same_v<T, bool>
			|| std::is_same_v<T, int32>
			|| std::is_same_v<T, float>
			|| std::is_same_v<T, double>
			|| std::is_same_v<T, uint8>)
		{
			Result = Invoker.ExecuteAndGet<T>(T{});
		}
		else
		{
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
		}
		TestRunner->TestEqual(Message, Result, Expected);
	}

	// -------------------------------------------------------------------------
	// FVector4: 4D vector construction and operations
	// -------------------------------------------------------------------------
	TEST_METHOD(FVector4Operations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovMathAdditional_FVector4", ASTEST_AS(R"AS(
		FVector4 TestConstruction()
		{
			return FVector4(1.0, 2.0, 3.0, 4.0);
		}

		FVector4 TestConstructionXYZ()
		{
			return FVector4(FVector(1, 2, 3), 4.0);
		}

		float TestGetX()
		{
			FVector4 v = FVector4(10, 20, 30, 40);
			return v.X;
		}

		float TestGetW()
		{
			FVector4 v = FVector4(10, 20, 30, 40);
			return v.W;
		}

		FVector4 TestAddition()
		{
			FVector4 a = FVector4(1, 2, 3, 4);
			FVector4 b = FVector4(5, 6, 7, 8);
			return a + b;
		}

		FVector4 TestMultiplication()
		{
			FVector4 v = FVector4(1, 2, 3, 4);
			return v * 2.0;
		}

		float TestDotProduct()
		{
			FVector4 a = FVector4(1, 0, 0, 0);
			FVector4 b = FVector4(0, 1, 0, 0);
			return a | b;
		}

		bool TestEquals()
		{
			FVector4 a = FVector4(1, 2, 3, 4);
			FVector4 b = FVector4(1, 2, 3, 4);
			return a == b;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector4 TestConstruction()"));
			FVector4 Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FVector4 construction X"), Result.X, 1.0);
			TestRunner->TestEqual(TEXT("FVector4 construction Y"), Result.Y, 2.0);
			TestRunner->TestEqual(TEXT("FVector4 construction Z"), Result.Z, 3.0);
			TestRunner->TestEqual(TEXT("FVector4 construction W"), Result.W, 4.0);
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector4 TestConstructionXYZ()"));
			FVector4 Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FVector4 construction from FVector X"), Result.X, 1.0);
			TestRunner->TestEqual(TEXT("FVector4 construction from FVector W"), Result.W, 4.0);
		}

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestGetX()"), 10.0f, TEXT("FVector4.X accessor"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestGetW()"), 40.0f, TEXT("FVector4.W accessor"));

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector4 TestAddition()"));
			FVector4 Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FVector4 addition X"), Result.X, 6.0);
			TestRunner->TestEqual(TEXT("FVector4 addition W"), Result.W, 12.0);
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FVector4 TestMultiplication()"));
			FVector4 Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FVector4 multiplication X"), Result.X, 2.0);
			TestRunner->TestEqual(TEXT("FVector4 multiplication W"), Result.W, 8.0);
		}

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestDotProduct()"), 0.0f, TEXT("FVector4 dot product"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestEquals()"), true, TEXT("FVector4 equality"));
	}

	// -------------------------------------------------------------------------
	// FIntPoint: 2D integer coordinates
	// -------------------------------------------------------------------------
	TEST_METHOD(FIntPointOperations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovMathAdditional_FIntPoint", ASTEST_AS(R"AS(
		FIntPoint TestConstruction()
		{
			return FIntPoint(10, 20);
		}

		FIntPoint TestZeroValue()
		{
			return FIntPoint::ZeroValue;
		}

		int32 TestGetX()
		{
			FIntPoint p = FIntPoint(100, 200);
			return p.X;
		}

		int32 TestGetY()
		{
			FIntPoint p = FIntPoint(100, 200);
			return p.Y;
		}

		FIntPoint TestAddition()
		{
			FIntPoint a = FIntPoint(10, 20);
			FIntPoint b = FIntPoint(5, 15);
			return a + b;
		}

		FIntPoint TestSubtraction()
		{
			FIntPoint a = FIntPoint(100, 200);
			FIntPoint b = FIntPoint(30, 50);
			return a - b;
		}

		FIntPoint TestMultiplication()
		{
			FIntPoint p = FIntPoint(10, 20);
			return p * 3;
		}

		FIntPoint TestDivision()
		{
			FIntPoint p = FIntPoint(30, 60);
			return p / 3;
		}

		bool TestEquals()
		{
			FIntPoint a = FIntPoint(10, 20);
			FIntPoint b = FIntPoint(10, 20);
			return a == b;
		}

		int32 TestSize()
		{
			FIntPoint p = FIntPoint(3, 4);
			return p.Size();
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FIntPoint TestConstruction()"));
			FIntPoint Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FIntPoint construction X"), Result.X, 10);
			TestRunner->TestEqual(TEXT("FIntPoint construction Y"), Result.Y, 20);
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FIntPoint TestZeroValue()"));
			FIntPoint Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FIntPoint::ZeroValue"), Result, FIntPoint::ZeroValue);
		}

		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int32 TestGetX()"), 100, TEXT("FIntPoint.X accessor"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int32 TestGetY()"), 200, TEXT("FIntPoint.Y accessor"));

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FIntPoint TestAddition()"));
			FIntPoint Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FIntPoint addition"), Result, FIntPoint(15, 35));
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FIntPoint TestSubtraction()"));
			FIntPoint Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FIntPoint subtraction"), Result, FIntPoint(70, 150));
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FIntPoint TestMultiplication()"));
			FIntPoint Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FIntPoint multiplication"), Result, FIntPoint(30, 60));
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FIntPoint TestDivision()"));
			FIntPoint Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FIntPoint division"), Result, FIntPoint(10, 20));
		}

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestEquals()"), true, TEXT("FIntPoint equality"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int32 TestSize()"), 5, TEXT("FIntPoint.Size()"));
	}

	// -------------------------------------------------------------------------
	// FIntVector: 3D integer vectors
	// -------------------------------------------------------------------------
	TEST_METHOD(FIntVectorOperations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovMathAdditional_FIntVector", ASTEST_AS(R"AS(
		FIntVector TestConstruction()
		{
			return FIntVector(10, 20, 30);
		}

		FIntVector TestZeroValue()
		{
			return FIntVector::ZeroValue;
		}

		int32 TestGetX()
		{
			FIntVector v = FIntVector(100, 200, 300);
			return v.X;
		}

		int32 TestGetZ()
		{
			FIntVector v = FIntVector(100, 200, 300);
			return v.Z;
		}

		FIntVector TestAddition()
		{
			FIntVector a = FIntVector(10, 20, 30);
			FIntVector b = FIntVector(5, 10, 15);
			return a + b;
		}

		FIntVector TestSubtraction()
		{
			FIntVector a = FIntVector(100, 200, 300);
			FIntVector b = FIntVector(30, 50, 70);
			return a - b;
		}

		FIntVector TestMultiplication()
		{
			FIntVector v = FIntVector(10, 20, 30);
			return v * 2;
		}

		FIntVector TestDivision()
		{
			FIntVector v = FIntVector(30, 60, 90);
			return v / 3;
		}

		bool TestEquals()
		{
			FIntVector a = FIntVector(10, 20, 30);
			FIntVector b = FIntVector(10, 20, 30);
			return a == b;
		}

		int32 TestGetMax()
		{
			FIntVector v = FIntVector(10, 50, 30);
			return v.GetMax();
		}

		int32 TestGetMin()
		{
			FIntVector v = FIntVector(10, 50, 30);
			return v.GetMin();
		}

		int32 TestSize()
		{
			FIntVector v = FIntVector(3, 4, 0);
			return v.Size();
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FIntVector TestConstruction()"));
			FIntVector Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FIntVector construction X"), Result.X, 10);
			TestRunner->TestEqual(TEXT("FIntVector construction Y"), Result.Y, 20);
			TestRunner->TestEqual(TEXT("FIntVector construction Z"), Result.Z, 30);
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FIntVector TestZeroValue()"));
			FIntVector Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FIntVector::ZeroValue"), Result, FIntVector::ZeroValue);
		}

		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int32 TestGetX()"), 100, TEXT("FIntVector.X accessor"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int32 TestGetZ()"), 300, TEXT("FIntVector.Z accessor"));

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FIntVector TestAddition()"));
			FIntVector Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FIntVector addition"), Result, FIntVector(15, 30, 45));
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FIntVector TestSubtraction()"));
			FIntVector Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FIntVector subtraction"), Result, FIntVector(70, 150, 230));
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FIntVector TestMultiplication()"));
			FIntVector Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FIntVector multiplication"), Result, FIntVector(20, 40, 60));
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FIntVector TestDivision()"));
			FIntVector Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FIntVector division"), Result, FIntVector(10, 20, 30));
		}

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestEquals()"), true, TEXT("FIntVector equality"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int32 TestGetMax()"), 50, TEXT("FIntVector.GetMax()"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int32 TestGetMin()"), 10, TEXT("FIntVector.GetMin()"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int32 TestSize()"), 5, TEXT("FIntVector.Size()"));
	}

	// -------------------------------------------------------------------------
	// FColor: 8-bit RGBA color operations
	// -------------------------------------------------------------------------
	TEST_METHOD(FColorOperations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovMathAdditional_FColor", ASTEST_AS(R"AS(
		FColor TestConstruction()
		{
			return FColor(255, 128, 64, 255);
		}

		FColor TestWhite()
		{
			return FColor::White;
		}

		FColor TestBlack()
		{
			return FColor::Black;
		}

		FColor TestRed()
		{
			return FColor::Red;
		}

		FColor TestGreen()
		{
			return FColor::Green;
		}

		FColor TestBlue()
		{
			return FColor::Blue;
		}

		uint8 TestGetR()
		{
			FColor c = FColor(100, 150, 200, 250);
			return c.R;
		}

		uint8 TestGetA()
		{
			FColor c = FColor(100, 150, 200, 250);
			return c.A;
		}

		FColor TestSetComponents()
		{
			FColor c = FColor(0, 0, 0, 0);
			c.R = 255;
			c.G = 128;
			c.B = 64;
			c.A = 255;
			return c;
		}

		bool TestEquals()
		{
			FColor a = FColor(100, 150, 200, 255);
			FColor b = FColor(100, 150, 200, 255);
			return a == b;
		}

		FLinearColor TestReinterpretAsLinear()
		{
			FColor c = FColor::White;
			return c.ReinterpretAsLinear();
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FColor TestConstruction()"));
			FColor Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FColor construction R"), Result.R, (uint8)255);
			TestRunner->TestEqual(TEXT("FColor construction G"), Result.G, (uint8)128);
			TestRunner->TestEqual(TEXT("FColor construction B"), Result.B, (uint8)64);
			TestRunner->TestEqual(TEXT("FColor construction A"), Result.A, (uint8)255);
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FColor TestWhite()"));
			FColor Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FColor::White"), Result, FColor::White);
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FColor TestBlack()"));
			FColor Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FColor::Black"), Result, FColor::Black);
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FColor TestRed()"));
			FColor Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FColor::Red"), Result, FColor::Red);
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FColor TestGreen()"));
			FColor Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FColor::Green"), Result, FColor::Green);
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FColor TestBlue()"));
			FColor Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FColor::Blue"), Result, FColor::Blue);
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("uint8 TestGetR()"));
			uint8 Result = Invoker.ExecuteAndGet<uint8>(0);
			TestRunner->TestEqual(TEXT("FColor.R accessor"), Result, (uint8)100);
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("uint8 TestGetA()"));
			uint8 Result = Invoker.ExecuteAndGet<uint8>(0);
			TestRunner->TestEqual(TEXT("FColor.A accessor"), Result, (uint8)250);
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FColor TestSetComponents()"));
			FColor Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestEqual(TEXT("FColor component setters R"), Result.R, (uint8)255);
			TestRunner->TestEqual(TEXT("FColor component setters G"), Result.G, (uint8)128);
			TestRunner->TestEqual(TEXT("FColor component setters B"), Result.B, (uint8)64);
			TestRunner->TestEqual(TEXT("FColor component setters A"), Result.A, (uint8)255);
		}

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestEquals()"), true, TEXT("FColor equality"));

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FLinearColor TestReinterpretAsLinear()"));
			FLinearColor Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
			TestRunner->TestTrue(TEXT("FColor.ReinterpretAsLinear()"), Result.R > 0.9f && Result.G > 0.9f && Result.B > 0.9f);
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
