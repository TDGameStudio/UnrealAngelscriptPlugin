#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "Math/UnrealMathUtility.h"
#include "Templates/Function.h"

#include <type_traits>

// -----------------------------------------------------------------------------
// AngelscriptCoverageMathGeometricStructs
// -----------------------------------------------------------------------------
// Coverage for geometric structures identified in Coverage_MathStructs.md:
// - FBox: AABB bounding box operations
// - FBox2D: unbound type boundary
// - FPlane: Plane operations
// - FTransform: Construction patterns not yet covered
//
// Test patterns: Pattern B (global functions)
// -----------------------------------------------------------------------------

#if WITH_ANGELSCRIPT_UNITTESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageMathGeometricStructsTest,
	"Angelscript.TestModule.Coverage.MathGeometricStructs",
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
		ASSERT_THAT(IsNotNull(Module, TEXT("math geometric module should compile before executing global function")));
		if (Module == nullptr)
		{
			return;
		}

		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("math geometric global function should resolve and prepare")));
		if (!Invoker.IsValid())
		{
			return;
		}
		T Result{};
		if constexpr (std::is_same_v<T, float>)
		{
			// AS `float` is double-backed on this fork (asEP_FLOAT_IS_FLOAT64=1):
			// read the return register as double before narrowing to float.
			Result = static_cast<float>(Invoker.ExecuteAndGet<double>(0.0));
		}
		else if constexpr (std::is_same_v<T, bool>
			|| std::is_same_v<T, int32>
			|| std::is_same_v<T, uint32>
			|| std::is_same_v<T, double>)
		{
			Result = Invoker.ExecuteAndGet<T>(T{});
		}
		else
		{
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
		}
		if constexpr (std::is_floating_point_v<T>)
		{
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Expected, Result, static_cast<T>(0.001)), Message));
		}
		else
		{
			ASSERT_THAT(AreEqual(Expected, Result, Message));
		}
	}

	template <typename T>
	void ExpectGlobalStructSatisfies(FAngelscriptEngine& Engine, asIScriptModule* Module, const TCHAR* Declaration, TFunctionRef<bool(const T&)> Predicate, const TCHAR* Message)
	{
		ASSERT_THAT(IsNotNull(Module, TEXT("math geometric module should compile before extracting struct")));
		if (Module == nullptr)
		{
			return;
		}

		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("math geometric struct function should resolve and prepare")));
		if (!Invoker.IsValid())
		{
			return;
		}

		T Result{};
		ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result)));
		ASSERT_THAT(IsTrue(Predicate(Result), Message));
	}

	// -------------------------------------------------------------------------
	// FTransform construction patterns
	// -------------------------------------------------------------------------
	TEST_METHOD(FTransformConstruction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovMathGeom_FTransformConstruct", ASTEST_AS(R"AS(
		FTransform TestDefaultConstruction()
		{
			return FTransform();
		}

		FTransform TestIdentity()
		{
			return FTransform::Identity;
		}

		FTransform TestLocationOnly()
		{
			return FTransform(FVector(100, 200, 300));
		}

		FTransform TestFullConstruction()
		{
			FQuat rot = FQuat(FRotator(0, 90, 0));
			FVector loc = FVector(100, 200, 300);
			FVector scale = FVector(2, 2, 2);
			return FTransform(rot, loc, scale);
		}

		FVector TestGetLocation()
		{
			FTransform t = FTransform(FVector(100, 200, 300));
			return t.GetLocation();
		}

		FQuat TestGetRotation()
		{
			FQuat rot = FQuat(FRotator(0, 90, 0));
			FTransform t = FTransform(rot, FVector::ZeroVector, FVector(1,1,1));
			return t.GetRotation();
		}

		FVector TestGetScale()
		{
			FTransform t = FTransform(FQuat::Identity, FVector::ZeroVector, FVector(2, 3, 4));
			return t.GetScale3D();
		}

		FTransform TestSetLocation()
		{
			FTransform t = FTransform::Identity;
			t.SetLocation(FVector(50, 100, 150));
			return t;
		}

		FTransform TestSetScale()
		{
			FTransform t = FTransform::Identity;
			t.SetScale3D(FVector(3, 3, 3));
			return t;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalStructSatisfies<FTransform>(
			Engine,
			Module,
			TEXT("FTransform TestDefaultConstruction()"),
			[](const FTransform& Result) { return Result.Equals(FTransform::Identity, 0.001); },
			TEXT("FTransform default construction"));
		ExpectGlobalStructSatisfies<FTransform>(
			Engine,
			Module,
			TEXT("FTransform TestIdentity()"),
			[](const FTransform& Result) { return Result.Equals(FTransform::Identity, 0.001); },
			TEXT("FTransform::Identity"));
		ExpectGlobalStructSatisfies<FTransform>(
			Engine,
			Module,
			TEXT("FTransform TestLocationOnly()"),
			[](const FTransform& Result) { return Result.GetLocation().Equals(FVector(100, 200, 300), 0.001); },
			TEXT("FTransform location-only construction"));
		ExpectGlobalStructSatisfies<FTransform>(
			Engine,
			Module,
			TEXT("FTransform TestFullConstruction()"),
			[](const FTransform& Result) { return Result.GetLocation().Equals(FVector(100, 200, 300), 0.001) && Result.GetScale3D().Equals(FVector(2, 2, 2), 0.001); },
			TEXT("FTransform full construction"));
		ExpectGlobalStructSatisfies<FVector>(
			Engine,
			Module,
			TEXT("FVector TestGetLocation()"),
			[](const FVector& Result) { return Result.Equals(FVector(100, 200, 300), 0.001); },
			TEXT("FTransform.Location accessor"));
		ExpectGlobalStructSatisfies<FQuat>(
			Engine,
			Module,
			TEXT("FQuat TestGetRotation()"),
			[](const FQuat& Result) { return !Result.Equals(FQuat::Identity, 0.001); },
			TEXT("FTransform.Rotation accessor"));
		ExpectGlobalStructSatisfies<FVector>(
			Engine,
			Module,
			TEXT("FVector TestGetScale()"),
			[](const FVector& Result) { return Result.Equals(FVector(2, 3, 4), 0.001); },
			TEXT("FTransform.Scale3D accessor"));
		ExpectGlobalStructSatisfies<FTransform>(
			Engine,
			Module,
			TEXT("FTransform TestSetLocation()"),
			[](const FTransform& Result) { return Result.GetLocation().Equals(FVector(50, 100, 150), 0.001); },
			TEXT("FTransform.Location setter"));
		ExpectGlobalStructSatisfies<FTransform>(
			Engine,
			Module,
			TEXT("FTransform TestSetScale()"),
			[](const FTransform& Result) { return Result.GetScale3D().Equals(FVector(3, 3, 3), 0.001); },
			TEXT("FTransform.Scale3D setter"));
	}

	TEST_METHOD(FTransformOperations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovMathGeom_FTransformOperations", ASTEST_AS(R"AS(
		FVector TestMultiplyTransformLocation()
		{
			FTransform First = FTransform(FVector(10, 0, 0));
			FTransform Second = FTransform(FVector(0, 20, 0));
			FTransform Combined = First * Second;
			return Combined.GetLocation();
		}

		FVector TestTransformPosition()
		{
			FTransform Transform = FTransform(FQuat::Identity, FVector(10, 20, 30), FVector(2, 2, 2));
			return Transform.TransformPosition(FVector(1, 2, 3));
		}

		FVector TestTransformVector()
		{
			FTransform Transform = FTransform(FQuat::Identity, FVector(10, 20, 30), FVector(2, 3, 4));
			return Transform.TransformVector(FVector(1, 2, 3));
		}

		FVector TestInverseTransformPosition()
		{
			FTransform Transform = FTransform(FQuat::Identity, FVector(10, 20, 30), FVector(2, 2, 2));
			return Transform.InverseTransformPosition(FVector(12, 24, 36));
		}

		FVector TestInverseTransformRoundTrip()
		{
			FTransform Transform = FTransform(FQuat::Identity, FVector(10, 20, 30), FVector(2, 3, 4));
			FVector Local = FVector(3, 4, 5);
			return Transform.Inverse().TransformPosition(Transform.TransformPosition(Local));
		}

		FVector TestTransformPositionNoScale()
		{
			FTransform Transform = FTransform(FQuat::Identity, FVector(10, 20, 30), FVector(2, 3, 4));
			return Transform.TransformPositionNoScale(FVector(1, 2, 3));
		}

		FVector TestInverseTransformVector()
		{
			FTransform Transform = FTransform(FQuat::Identity, FVector::ZeroVector, FVector(2, 4, 5));
			return Transform.InverseTransformVector(FVector(4, 8, 10));
		}

		FVector TestInverseTransformVectorNoScale()
		{
			FTransform Transform = FTransform(FQuat::Identity, FVector::ZeroVector, FVector(2, 4, 5));
			return Transform.InverseTransformVectorNoScale(FVector(4, 8, 10));
		}

		FVector TestScaleTranslationAndAdd()
		{
			FTransform Transform = FTransform(FVector(10, 20, 30));
			Transform.ScaleTranslation(2.0);
			Transform.AddToTranslation(FVector(1, 2, 3));
			return Transform.GetTranslation();
		}

		FVector TestBlendLocation()
		{
			FTransform A = FTransform(FVector(0, 0, 0));
			FTransform B = FTransform(FVector(10, 20, 30));
			FTransform Result;
			Result.Blend(A, B, 0.5f);
			return Result.GetTranslation();
		}

		bool TestEqualsNoScale()
		{
			FTransform A = FTransform(FQuat::Identity, FVector(1, 2, 3), FVector(1, 1, 1));
			FTransform B = FTransform(FQuat::Identity, FVector(1, 2, 3), FVector(4, 5, 6));
			return A.EqualsNoScale(B, 0.001);
		}

		bool TestTranslationEqualsAndSubtract()
		{
			FTransform A = FTransform(FQuat::Identity, FVector(10, 20, 30), FVector(1, 1, 1));
			FTransform B = FTransform(FQuat::Identity, FVector(3, 5, 7), FVector(9, 9, 9));
			FVector Difference = A.SubtractTranslations(B);
			return A.TranslationEquals(FTransform(FVector(10, 20, 30)), 0.001)
				&& Difference.Equals(FVector(7, 15, 23), 0.001);
		}

		bool TestSetTranslationScaleAndDeterminant()
		{
			FTransform Transform = FTransform::Identity;
			Transform.SetTranslationAndScale3D(FVector(1, 2, 3), FVector(2, 3, 4));
			return Transform.GetTranslation().Equals(FVector(1, 2, 3), 0.001)
				&& Transform.GetScale3D().Equals(FVector(2, 3, 4), 0.001)
				&& Math::IsNearlyEqual(Transform.GetDeterminant(), 24.0, 0.001);
		}

		bool TestRotatorConversion()
		{
			FTransform Transform = FTransform(FRotator(10, 20, 30));
			FRotator Rotator = Transform.Rotator();
			return Math::IsNearlyEqual(Rotator.Pitch, 10.0, 0.001)
				&& Math::IsNearlyEqual(Rotator.Yaw, 20.0, 0.001)
				&& Math::IsNearlyEqual(Rotator.Roll, 30.0, 0.001);
		}

		bool TestValidityHelpers()
		{
			FTransform Transform = FTransform(FQuat::Identity, FVector(1, 2, 3), FVector(2, 2, 2));
			return Transform.IsValid() && !Transform.ContainsNaN();
		}

		float TestAxisScale()
		{
			FTransform Transform = FTransform(FQuat::Identity, FVector::ZeroVector, FVector(2, 5, 3));
			return Transform.GetMaximumAxisScale() + Transform.GetMinimumAxisScale();
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};
		ASSERT_THAT(IsNotNull(Module, TEXT("FTransform operations module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		auto ExpectVectorReturn = [this, &Engine, Module](const TCHAR* Declaration, const FVector& Expected, const TCHAR* Message)
		{
			ExpectGlobalStructSatisfies<FVector>(
				Engine,
				Module,
				Declaration,
				[Expected](const FVector& Result) { return Result.Equals(Expected, 0.001); },
				Message);
		};

		ExpectVectorReturn(TEXT("FVector TestMultiplyTransformLocation()"), FVector(10, 20, 0), TEXT("FTransform multiplication should combine translations"));
		ExpectVectorReturn(TEXT("FVector TestTransformPosition()"), FVector(12, 24, 36), TEXT("FTransform.TransformPosition() should apply scale and translation"));
		ExpectVectorReturn(TEXT("FVector TestTransformVector()"), FVector(2, 6, 12), TEXT("FTransform.TransformVector() should apply scale without translation"));
		ExpectVectorReturn(TEXT("FVector TestInverseTransformPosition()"), FVector(1, 2, 3), TEXT("FTransform.InverseTransformPosition() should reverse TransformPosition"));
		ExpectVectorReturn(TEXT("FVector TestInverseTransformRoundTrip()"), FVector(3, 4, 5), TEXT("FTransform.Inverse() should reverse TransformPosition"));
		ExpectVectorReturn(TEXT("FVector TestTransformPositionNoScale()"), FVector(11, 22, 33), TEXT("FTransform.TransformPositionNoScale() should ignore scale"));
		ExpectVectorReturn(TEXT("FVector TestInverseTransformVector()"), FVector(2, 2, 2), TEXT("FTransform.InverseTransformVector() should reverse vector scale"));
		ExpectVectorReturn(TEXT("FVector TestInverseTransformVectorNoScale()"), FVector(4, 8, 10), TEXT("FTransform.InverseTransformVectorNoScale() should ignore scale"));
		ExpectVectorReturn(TEXT("FVector TestScaleTranslationAndAdd()"), FVector(21, 42, 63), TEXT("ScaleTranslation and AddToTranslation should mutate translation"));
		ExpectVectorReturn(TEXT("FVector TestBlendLocation()"), FVector(5, 10, 15), TEXT("FTransform.Blend() should interpolate translation"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestEqualsNoScale()"), true, TEXT("FTransform.EqualsNoScale() should ignore scale"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestTranslationEqualsAndSubtract()"), true, TEXT("FTransform translation comparison and subtraction helpers"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestSetTranslationScaleAndDeterminant()"), true, TEXT("FTransform SetTranslationAndScale3D/GetDeterminant helpers"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestRotatorConversion()"), true, TEXT("FTransform.Rotator() should expose rotation conversion"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestValidityHelpers()"), true, TEXT("FTransform validity helpers"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestAxisScale()"), 7.0f, TEXT("FTransform axis scale helpers"));
	}

	TEST_METHOD(Vector4IntPointIntVectorExpressions)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovMathGeom_Vector4IntStructExpressions", ASTEST_AS(R"AS(
		FVector4 TestVector4Construction()
		{
			return FVector4(1, 2, 3, 4);
		}

		FVector4 TestVector4FromVector()
		{
			return FVector4(FVector(5, 6, 7), 8);
		}

		float64 TestVector4MembersAndIndex()
		{
			FVector4 Value = FVector4(1, 2, 3, 4);
			return Value.X + Value.Y + Value.Z + Value.W + Value[2];
		}

		FVector4 TestVector4Arithmetic()
		{
			FVector4 Value = FVector4(1, 2, 3, 4);
			Value = (Value + FVector4(1, 1, 1, 1)) * 2.0;
			return Value / 2.0;
		}

		FIntPoint TestIntPointConstruction()
		{
			return FIntPoint(3, 4);
		}

		int TestIntPointMembersIndexAndMethods()
		{
			FIntPoint Point = FIntPoint(3, 7);
			return Point.X + Point.Y + Point[0] + Point.GetMax() + Point.GetMin() + Point.Size();
		}

		FIntPoint TestIntPointArithmetic()
		{
			FIntPoint Point = FIntPoint(2, 4);
			Point += FIntPoint(3, 5);
			Point *= 2;
			Point /= 2;
			return Point - FIntPoint(1, 1);
		}

		FIntVector TestIntVectorConstruction()
		{
			return FIntVector(1, 2, 3);
		}

		int TestIntVectorMembersIndexAndMethods()
		{
			FIntVector Value = FIntVector(2, 5, 8);
			return Value.X + Value.Y + Value.Z + Value[2] + Value.GetMax() + Value.GetMin() + Value.Size();
		}

		FIntVector TestIntVectorArithmetic()
		{
			FIntVector Value = FIntVector(2, 4, 6);
			Value += FIntVector(3, 5, 7);
			Value -= FIntVector(1, 1, 1);
			Value *= 2;
			Value /= 2;
			return -Value;
		}

		bool TestIntVectorIsZero()
		{
			return FIntVector().IsZero() && !FIntVector(1, 0, 0).IsZero();
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalStructSatisfies<FVector4>(
			Engine,
			Module,
			TEXT("FVector4 TestVector4Construction()"),
			[](const FVector4& Result) { return Result == FVector4(1, 2, 3, 4); },
			TEXT("FVector4 construction should expose XYZ/W"));
		ExpectGlobalStructSatisfies<FVector4>(
			Engine,
			Module,
			TEXT("FVector4 TestVector4FromVector()"),
			[](const FVector4& Result) { return Result == FVector4(5, 6, 7, 8); },
			TEXT("FVector4 FVector+W construction"));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("float64 TestVector4MembersAndIndex()"), 13.0, TEXT("FVector4 members and index access"));
		ExpectGlobalStructSatisfies<FVector4>(
			Engine,
			Module,
			TEXT("FVector4 TestVector4Arithmetic()"),
			[](const FVector4& Result) { return Result == FVector4(2, 3, 4, 5); },
			TEXT("FVector4 arithmetic"));
		ExpectGlobalReturn<FIntPoint>(Engine, Module, TEXT("FIntPoint TestIntPointConstruction()"), FIntPoint(3, 4), TEXT("FIntPoint construction"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int TestIntPointMembersIndexAndMethods()"), 30, TEXT("FIntPoint members/index/methods"));
		ExpectGlobalReturn<FIntPoint>(Engine, Module, TEXT("FIntPoint TestIntPointArithmetic()"), FIntPoint(4, 8), TEXT("FIntPoint arithmetic"));
		ExpectGlobalReturn<FIntVector>(Engine, Module, TEXT("FIntVector TestIntVectorConstruction()"), FIntVector(1, 2, 3), TEXT("FIntVector construction"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int TestIntVectorMembersIndexAndMethods()"), 42, TEXT("FIntVector members/index/methods"));
		ExpectGlobalReturn<FIntVector>(Engine, Module, TEXT("FIntVector TestIntVectorArithmetic()"), FIntVector(-4, -8, -12), TEXT("FIntVector arithmetic"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestIntVectorIsZero()"), true, TEXT("FIntVector IsZero"));
	}

	TEST_METHOD(Vector4IntPointIntVectorReflection)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMathGeom_Vector4IntStructReflection"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMathGeomVector4IntStructReflection.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageMathVector4IntStructActor : AActor
			{
				UPROPERTY()
				FVector4 Vector4Value = FVector4(1, 2, 3, 4);

				UPROPERTY()
				FIntPoint IntPointValue = FIntPoint(5, 6);

				UPROPERTY()
				FIntVector IntVectorValue = FIntVector(7, 8, 9);

				UPROPERTY()
				TArray<FVector4> Vector4Array;

				UPROPERTY()
				TArray<FIntPoint> IntPointArray;

				UPROPERTY()
				TArray<FIntVector> IntVectorArray;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Vector4Array.Add(FVector4(10, 11, 12, 13));
					IntPointArray.Add(FIntPoint(14, 15));
					IntVectorArray.Add(FIntVector(16, 17, 18));
				}
			}
			)AS"),
			TEXT("ACoverageMathVector4IntStructActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FVector4/FIntPoint/FIntVector reflection actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		auto ExpectStructProperty = [this, ScriptClass](const TCHAR* PropertyName, UScriptStruct* ExpectedStruct, const TCHAR* Message)
		{
			const FProperty* Property = ScriptClass->FindPropertyByName(FName(PropertyName));
			ASSERT_THAT(IsNotNull(Property, Message));
			const FStructProperty* StructProperty = CastField<const FStructProperty>(Property);
			ASSERT_THAT(IsNotNull(StructProperty, Message));
			if (StructProperty != nullptr)
			{
				ASSERT_THAT(AreEqual(ExpectedStruct, StructProperty->Struct, Message));
			}
		};

		ExpectStructProperty(TEXT("Vector4Value"), TBaseStructure<FVector4>::Get(), TEXT("FVector4 UPROPERTY should reflect as FVector4 struct"));
		ExpectStructProperty(TEXT("IntPointValue"), TBaseStructure<FIntPoint>::Get(), TEXT("FIntPoint UPROPERTY should reflect as FIntPoint struct"));
		ExpectStructProperty(TEXT("IntVectorValue"), TBaseStructure<FIntVector>::Get(), TEXT("FIntVector UPROPERTY should reflect as FIntVector struct"));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FVector4/FIntPoint/FIntVector reflection actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("Vector4Value.X"), 1.0, TEXT("FVector4.X via nested path"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("Vector4Value.W"), 4.0, TEXT("FVector4.W via nested path"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntPointValue.X"), 5, TEXT("FIntPoint.X via nested path"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntPointValue.Y"), 6, TEXT("FIntPoint.Y via nested path"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntVectorValue.X"), 7, TEXT("FIntVector.X via nested path"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntVectorValue.Z"), 9, TEXT("FIntVector.Z via nested path"))));

		int32 ArrayNum = 0;
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("Vector4Array"), ArrayNum)));
		ASSERT_THAT(AreEqual(1, ArrayNum, TEXT("TArray<FVector4> should reflect one element")));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("Vector4Array[0].W"), 13.0, TEXT("TArray<FVector4>[0].W via nested path"))));

		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("IntPointArray"), ArrayNum)));
		ASSERT_THAT(AreEqual(1, ArrayNum, TEXT("TArray<FIntPoint> should reflect one element")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntPointArray[0].Y"), 15, TEXT("TArray<FIntPoint>[0].Y via nested path"))));

		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("IntVectorArray"), ArrayNum)));
		ASSERT_THAT(AreEqual(1, ArrayNum, TEXT("TArray<FIntVector> should reflect one element")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntVectorArray[0].Z"), 18, TEXT("TArray<FIntVector>[0].Z via nested path"))));
	}

	TEST_METHOD(FMatrixReturnApiCompiles)
	{
		// FMatrix is now a registered return type, so FTransform matrix-return APIs
		// (historically a compile-failure boundary) compile successfully.
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovMathGeom_FMatrixReturn", ASTEST_AS(R"AS(
		FMatrix TriggerMatrixReturn()
		{
			return FTransform::Identity.ToMatrixWithScale();
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};
		ASSERT_THAT(IsNotNull(Module, TEXT("FTransform.ToMatrixWithScale() should compile now that FMatrix is a registered return type")));
	}

	TEST_METHOD(GeometricStructReflectionPropertiesAndContainers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMathGeom_GeometricStructReflection"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMathGeomGeometricStructReflection.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageMathGeometricStructActor : AActor
			{
				UPROPERTY()
				FBox BoxValue = FBox(FVector(-1, -2, -3), FVector(4, 5, 6));

				UPROPERTY()
				FPlane PlaneValue = FPlane(FVector(0, 0, 10), FVector(0, 0, 1));

				UPROPERTY()
				TArray<FBox> BoxArray;

				UPROPERTY()
				TArray<FPlane> PlaneArray;

				UPROPERTY()
				TMap<int, FBox> BoxMap;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					BoxArray.Add(FBox(FVector(1, 2, 3), FVector(4, 5, 6)));
					BoxArray.Add(FBox(FVector(-10, -20, -30), FVector(-1, -2, -3)));

					PlaneArray.Add(FPlane(FVector(0, 0, 8), FVector(0, 0, 1)));
					PlaneArray.Add(FPlane(FVector(2, 0, 0), FVector(1, 0, 0)));

					BoxMap.Add(7, FBox(FVector(10, 20, 30), FVector(40, 50, 60)));
					BoxMap.Add(8, FBox(FVector(-4, -5, -6), FVector(-1, -2, -3)));
				}
			}
			)AS"),
			TEXT("ACoverageMathGeometricStructActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FBox/FPlane reflection actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		auto ExpectStructProperty = [this, ScriptClass](const TCHAR* PropertyName, const TCHAR* ExpectedStructCppName, const TCHAR* Message)
		{
			const FProperty* Property = ScriptClass->FindPropertyByName(FName(PropertyName));
			ASSERT_THAT(IsNotNull(Property, Message));
			if (Property == nullptr)
			{
				return;
			}

			const FStructProperty* StructProperty = CastField<const FStructProperty>(Property);
			ASSERT_THAT(IsNotNull(StructProperty, Message));
			if (StructProperty == nullptr)
			{
				return;
			}

			ASSERT_THAT(IsNotNull(StructProperty->Struct, Message));
			if (StructProperty->Struct == nullptr)
			{
				return;
			}

			ASSERT_THAT(AreEqual(FString(ExpectedStructCppName), StructProperty->Struct->GetStructCPPName(), Message));
		};

		auto ExpectArrayStructProperty = [this, ScriptClass](const TCHAR* PropertyName, const TCHAR* ExpectedStructCppName, const TCHAR* Message)
		{
			const FProperty* Property = ScriptClass->FindPropertyByName(FName(PropertyName));
			ASSERT_THAT(IsNotNull(Property, Message));
			if (Property == nullptr)
			{
				return;
			}

			const FArrayProperty* ArrayProperty = CastField<const FArrayProperty>(Property);
			ASSERT_THAT(IsNotNull(ArrayProperty, Message));
			if (ArrayProperty == nullptr)
			{
				return;
			}

			const FStructProperty* ElementProperty = CastField<const FStructProperty>(ArrayProperty->Inner);
			ASSERT_THAT(IsNotNull(ElementProperty, Message));
			if (ElementProperty == nullptr)
			{
				return;
			}

			ASSERT_THAT(IsNotNull(ElementProperty->Struct, Message));
			if (ElementProperty->Struct == nullptr)
			{
				return;
			}

			ASSERT_THAT(AreEqual(FString(ExpectedStructCppName), ElementProperty->Struct->GetStructCPPName(), Message));
		};

		ExpectStructProperty(TEXT("BoxValue"), TEXT("FBox"), TEXT("FBox UPROPERTY should reflect as FBox struct"));
		ExpectStructProperty(TEXT("PlaneValue"), TEXT("FPlane"), TEXT("FPlane UPROPERTY should reflect as FPlane struct"));
		ExpectArrayStructProperty(TEXT("BoxArray"), TEXT("FBox"), TEXT("TArray<FBox> element should reflect as FBox struct"));
		ExpectArrayStructProperty(TEXT("PlaneArray"), TEXT("FPlane"), TEXT("TArray<FPlane> element should reflect as FPlane struct"));

		const FProperty* BoxMapPropertyBase = ScriptClass->FindPropertyByName(FName(TEXT("BoxMap")));
		ASSERT_THAT(IsNotNull(BoxMapPropertyBase, TEXT("TMap<int, FBox> property should reflect")));
		if (BoxMapPropertyBase == nullptr)
		{
			return;
		}

		const FMapProperty* BoxMapProperty = CastField<const FMapProperty>(BoxMapPropertyBase);
		ASSERT_THAT(IsNotNull(BoxMapProperty, TEXT("BoxMap should reflect as FMapProperty")));
		if (BoxMapProperty == nullptr)
		{
			return;
		}

		const FStructProperty* BoxMapValueProperty = CastField<const FStructProperty>(BoxMapProperty->ValueProp);
		ASSERT_THAT(IsNotNull(BoxMapValueProperty, TEXT("TMap<int, FBox> value should reflect as FStructProperty")));
		if (BoxMapValueProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsNotNull(BoxMapValueProperty->Struct, TEXT("TMap<int, FBox> value should expose a struct")));
		if (BoxMapValueProperty->Struct == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(FString(TEXT("FBox")), BoxMapValueProperty->Struct->GetStructCPPName(), TEXT("TMap<int, FBox> value should use FBox struct")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FBox/FPlane reflection actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("BoxValue.Min.X"), -1.0, TEXT("FBox.Min.X via nested path"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("BoxValue.Max.Z"), 6.0, TEXT("FBox.Max.Z via nested path"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("PlaneValue.Z"), 1.0, TEXT("FPlane.Z via nested path"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("PlaneValue.W"), 10.0, TEXT("FPlane.W via nested path"))));

		int32 Count = 0;
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("BoxArray"), Count), TEXT("TArray<FBox> length should resolve")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TArray<FBox> should have two elements")));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("BoxArray[0].Min.Y"), 2.0, TEXT("TArray<FBox>[0].Min.Y via nested path"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("BoxArray[1].Max.Z"), -3.0, TEXT("TArray<FBox>[1].Max.Z via nested path"))));

		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("PlaneArray"), Count), TEXT("TArray<FPlane> length should resolve")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TArray<FPlane> should have two elements")));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("PlaneArray[0].W"), 8.0, TEXT("TArray<FPlane>[0].W via nested path"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("PlaneArray[1].X"), 1.0, TEXT("TArray<FPlane>[1].X via nested path"))));

		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("BoxMap"), Count), TEXT("TMap<int, FBox> length should resolve")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<int, FBox> should have two entries")));
		// NOTE: the reflective property-path resolver treats `[N]` as a static-array
		// index, so keyed TMap value access (`BoxMap[7].Max.Z`) is unsupported. The
		// map is still verified above through FMapProperty reflection + entry count.
	}

	TEST_METHOD(GeometricStructFunctionParametersAndReturns)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovMathGeom_FunctionParameters", ASTEST_AS(R"AS(
		FBox TranslateBox(FBox Value, const FVector&in Offset)
		{
			return Value.ShiftBy(Offset);
		}

		double ReadBoxVolume(const FBox&in Value)
		{
			return Value.GetVolume();
		}

		double SumBoxArrayVolumes(const TArray<FBox>&in Values)
		{
			double Total = 0.0;
			for (int Index = 0; Index < Values.Num(); ++Index)
			{
				Total += Values[Index].GetVolume();
			}
			return Total;
		}

		double ReadBoxMapVolume(const TMap<int, FBox>&in Values, int Key)
		{
			if (!Values.Contains(Key))
			{
				return -1.0;
			}

			return Values[Key].GetVolume();
		}

		FPlane OffsetPlane(FPlane Value, double Offset)
		{
			FVector Normal = Value.GetNormal();
			return FPlane(Value.GetOrigin() + Normal * Offset, Normal);
		}

		double ReadPlaneDistance(const FPlane&in Value, const FVector&in Point)
		{
			return Value.PlaneDot(Point);
		}

		double SumPlaneDistances(const TArray<FPlane>&in Values, const FVector&in Point)
		{
			double Total = 0.0;
			for (int Index = 0; Index < Values.Num(); ++Index)
			{
				Total += Values[Index].PlaneDot(Point);
			}
			return Total;
		}

		FIntPoint SumIntPoints(const TArray<FIntPoint>&in Values)
		{
			FIntPoint Total;
			for (int Index = 0; Index < Values.Num(); ++Index)
			{
				Total += Values[Index];
			}
			return Total;
		}

		FIntVector SumIntVectorMap(const TMap<int, FIntVector>&in Values)
		{
			FIntVector Total;
			if (Values.Contains(1))
			{
				Total += Values[1];
			}
			if (Values.Contains(2))
			{
				Total += Values[2];
			}
			return Total;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};
		ASSERT_THAT(IsNotNull(Module, TEXT("geometric struct function parameter module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FBox TranslateBox(FBox, const FVector&in)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("TranslateBox should resolve")));
			if (!Invoker.IsValid())
			{
				return;
			}

			FBox Input(FVector(1, 2, 3), FVector(4, 5, 6));
			const FVector Offset(10, 20, 30);
			Invoker.AddArgStruct(Input);
			Invoker.AddArgRef(Offset);
			FBox Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result), TEXT("TranslateBox should execute")));
			ASSERT_THAT(IsTrue(Result.Min.Equals(FVector(11, 22, 33), 0.001), TEXT("FBox value parameter should translate Min")));
			ASSERT_THAT(IsTrue(Result.Max.Equals(FVector(14, 25, 36), 0.001), TEXT("FBox value parameter should translate Max")));
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("double ReadBoxVolume(const FBox&in)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("ReadBoxVolume should resolve")));
			if (!Invoker.IsValid())
			{
				return;
			}

			const FBox Input(FVector(0, 0, 0), FVector(2, 3, 4));
			Invoker.AddArgRef(Input);
			const double Result = Invoker.ExecuteAndGet<double>(-1.0);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Result, 24.0, 0.001), TEXT("const FBox&in parameter should read volume")));
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("double SumBoxArrayVolumes(const TArray<FBox>&in)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("SumBoxArrayVolumes should resolve")));
			if (!Invoker.IsValid())
			{
				return;
			}

			TArray<FBox> Values;
			Values.Add(FBox(FVector(0, 0, 0), FVector(1, 2, 3)));
			Values.Add(FBox(FVector(0, 0, 0), FVector(2, 3, 4)));
			Invoker.AddArgRef(Values);
			const double Result = Invoker.ExecuteAndGet<double>(-1.0);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Result, 30.0, 0.001), TEXT("const TArray<FBox>&in should pass box values")));
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("double ReadBoxMapVolume(const TMap<int, FBox>&in, int)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("ReadBoxMapVolume should resolve")));
			if (!Invoker.IsValid())
			{
				return;
			}

			TMap<int32, FBox> Values;
			Values.Add(3, FBox(FVector(0, 0, 0), FVector(3, 4, 5)));
			const int32 Key = 3;
			Invoker.AddArgRef(Values);
			Invoker.AddArg(Key);
			const double Result = Invoker.ExecuteAndGet<double>(-1.0);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Result, 60.0, 0.001), TEXT("const TMap<int, FBox>&in should pass map values")));
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FPlane OffsetPlane(FPlane, double)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("OffsetPlane should resolve")));
			if (!Invoker.IsValid())
			{
				return;
			}

			FPlane Input(FVector(0, 0, 10), FVector(0, 0, 1));
			const double Offset = 5.0;
			Invoker.AddArgStruct(Input);
			Invoker.AddArg(Offset);
			FPlane Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result), TEXT("OffsetPlane should execute")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Result.W, 15.0, 0.001), TEXT("FPlane value parameter should return offset plane")));
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("double ReadPlaneDistance(const FPlane&in, const FVector&in)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("ReadPlaneDistance should resolve")));
			if (!Invoker.IsValid())
			{
				return;
			}

			const FPlane Plane(FVector(0, 0, 10), FVector(0, 0, 1));
			const FVector Point(0, 0, 25);
			Invoker.AddArgRef(Plane);
			Invoker.AddArgRef(Point);
			const double Result = Invoker.ExecuteAndGet<double>(-1.0);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Result, 15.0, 0.001), TEXT("const FPlane&in parameter should read plane distance")));
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("double SumPlaneDistances(const TArray<FPlane>&in, const FVector&in)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("SumPlaneDistances should resolve")));
			if (!Invoker.IsValid())
			{
				return;
			}

			TArray<FPlane> Values;
			Values.Add(FPlane(FVector(0, 0, 2), FVector(0, 0, 1)));
			Values.Add(FPlane(FVector(5, 0, 0), FVector(1, 0, 0)));
			const FVector Point(10, 0, 8);
			Invoker.AddArgRef(Values);
			Invoker.AddArgRef(Point);
			const double Result = Invoker.ExecuteAndGet<double>(-1.0);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Result, 11.0, 0.001), TEXT("const TArray<FPlane>&in should pass plane values")));
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FIntPoint SumIntPoints(const TArray<FIntPoint>&in)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("SumIntPoints should resolve")));
			if (!Invoker.IsValid())
			{
				return;
			}

			TArray<FIntPoint> Values;
			Values.Add(FIntPoint(1, 2));
			Values.Add(FIntPoint(3, 4));
			Invoker.AddArgRef(Values);
			FIntPoint Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result), TEXT("SumIntPoints should execute")));
			ASSERT_THAT(AreEqual(FIntPoint(4, 6), Result, TEXT("const TArray<FIntPoint>&in should pass int point values")));
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FIntVector SumIntVectorMap(const TMap<int, FIntVector>&in)"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("SumIntVectorMap should resolve")));
			if (!Invoker.IsValid())
			{
				return;
			}

			TMap<int32, FIntVector> Values;
			Values.Add(1, FIntVector(1, 2, 3));
			Values.Add(2, FIntVector(4, 5, 6));
			Invoker.AddArgRef(Values);
			FIntVector Result;
			ASSERT_THAT(IsTrue(Invoker.ExecuteAndExtractStruct(Result), TEXT("SumIntVectorMap should execute")));
			ASSERT_THAT(AreEqual(FIntVector(5, 7, 9), Result, TEXT("const TMap<int, FIntVector>&in should pass int vector values")));
		}
	}

	TEST_METHOD(ColorAndRandomStreamStructExpressions)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const int32 ExpectedInitialSeed = 12345;
		FRandomStream NativeStream(ExpectedInitialSeed);
		const int32 ExpectedFirstRange = NativeStream.RandRange(10, 20);
		const double ExpectedDoubleRange = NativeStream.FRandRange(1.0, 2.0);
		const uint32 ExpectedUnsigned = NativeStream.GetUnsignedInt();

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovMathGeom_ColorRandomStream", ASTEST_AS(R"AS(
		FColor TestFColorConstruction()
		{
			return FColor(10, 20, 30, 40);
		}

		FColor TestFColorConstantsAndHex()
		{
			return FColor::FromHex(FColor::Red.ToHex());
		}

		FLinearColor TestFColorReinterpretAsLinear()
		{
			return FColor(128, 64, 32, 255).ReinterpretAsLinear();
		}

		FColor TestFLinearColorToFColor()
		{
			return FLinearColor(1.0, 0.5, 0.0, 1.0).ToFColor(false);
		}

		bool TestFColorInitFromStringAndAddAssign()
		{
			FColor Color;
			bool bInitialized = Color.InitFromString("(R=10,G=20,B=30,A=40)");
			Color += FColor(1, 2, 3, 4);
			return bInitialized
				&& Color == FColor(11, 22, 33, 44)
				&& FColor::White == FColor(255, 255, 255, 255)
				&& FColor::Transparent == FColor(0, 0, 0, 0);
		}

		int TestRandomStreamIntRange()
		{
			FRandomStream Stream(12345);
			return Stream.RandRange(10, 20);
		}

		double TestRandomStreamDoubleRange()
		{
			FRandomStream Stream(12345);
			Stream.RandRange(10, 20);
			return Stream.RandRange(1.0, 2.0);
		}

		uint TestRandomStreamUnsigned()
		{
			FRandomStream Stream(12345);
			Stream.RandRange(10, 20);
			Stream.RandRange(1.0, 2.0);
			return Stream.GetUnsignedInt();
		}

		bool TestRandomStreamResetAndSeed()
		{
			FRandomStream Stream(12345);
			int First = Stream.RandRange(10, 20);
			Stream.RandRange(1.0, 2.0);
			Stream.Reset();
			int ResetFirst = Stream.RandRange(10, 20);
			return Stream.GetInitialSeed() == 12345
				&& First == ResetFirst;
		}

		bool TestRandomStreamVectors()
		{
			FRandomStream Stream(12345);
			FVector Unit = Stream.GetUnitVector();
			FVector Random = Stream.VRand();
			FVector Cone = Stream.VRandCone(FVector(1, 0, 0), 0.25f);
			return Unit.IsUnit(0.001)
				&& Random.IsUnit(0.001)
				&& Cone.IsUnit(0.001)
				&& Cone.X > 0.9;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<FColor>(Engine, Module, TEXT("FColor TestFColorConstruction()"), FColor(10, 20, 30, 40), TEXT("FColor constructor should set packed color channels"));
		ExpectGlobalReturn<FColor>(Engine, Module, TEXT("FColor TestFColorConstantsAndHex()"), FColor::Red, TEXT("FColor constants and FromHex/ToHex should round-trip"));
		ExpectGlobalStructSatisfies<FLinearColor>(
			Engine,
			Module,
			TEXT("FLinearColor TestFColorReinterpretAsLinear()"),
			[](const FLinearColor& Result)
			{
				return FMath::IsNearlyEqual(Result.R, 128.0f / 255.0f, 0.001f)
					&& FMath::IsNearlyEqual(Result.G, 64.0f / 255.0f, 0.001f)
					&& FMath::IsNearlyEqual(Result.B, 32.0f / 255.0f, 0.001f)
					&& FMath::IsNearlyEqual(Result.A, 1.0f, 0.001f);
			},
			TEXT("FColor.ReinterpretAsLinear should expose linear channel values"));
		ExpectGlobalReturn<FColor>(Engine, Module, TEXT("FColor TestFLinearColorToFColor()"), FColor(255, 128, 0, 255), TEXT("FLinearColor.ToFColor(false) should produce deterministic channel values"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestFColorInitFromStringAndAddAssign()"), true, TEXT("FColor InitFromString/add/constant checks"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int TestRandomStreamIntRange()"), ExpectedFirstRange, TEXT("FRandomStream int RandRange should match native deterministic sequence"));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("double TestRandomStreamDoubleRange()"), ExpectedDoubleRange, TEXT("FRandomStream double RandRange should match native deterministic sequence"));
		ExpectGlobalReturn<uint32>(Engine, Module, TEXT("uint TestRandomStreamUnsigned()"), ExpectedUnsigned, TEXT("FRandomStream GetUnsignedInt should match native deterministic sequence"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestRandomStreamResetAndSeed()"), true, TEXT("FRandomStream Reset and GetInitialSeed"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestRandomStreamVectors()"), true, TEXT("FRandomStream vector helpers should return unit vectors"));
	}

	// -------------------------------------------------------------------------
	// FBox: AABB bounding box operations
	// -------------------------------------------------------------------------
	TEST_METHOD(FBoxOperations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovMathGeom_FBox", ASTEST_AS(R"AS(
		FBox TestConstruction()
		{
			FVector min = FVector(0, 0, 0);
			FVector max = FVector(100, 100, 100);
			return FBox(min, max);
		}

		FBox TestBuildAABB()
		{
			return FBox::BuildAABB(FVector(50, 50, 50), FVector(50, 50, 50));
		}

		bool TestIsInside()
		{
			FBox box = FBox(FVector(0, 0, 0), FVector(100, 100, 100));
			FVector point = FVector(50, 50, 50);
			return box.IsInside(point);
		}

		bool TestIsInsideOutside()
		{
			FBox box = FBox(FVector(0, 0, 0), FVector(100, 100, 100));
			FVector point = FVector(200, 200, 200);
			return box.IsInside(point);
		}

		FVector TestGetCenter()
		{
			FBox box = FBox(FVector(0, 0, 0), FVector(100, 100, 100));
			return box.GetCenter();
		}

		FVector TestGetExtent()
		{
			FBox box = FBox(FVector(0, 0, 0), FVector(100, 100, 100));
			return box.GetExtent();
		}

		FVector TestGetSize()
		{
			FBox box = FBox(FVector(0, 0, 0), FVector(100, 100, 100));
			return box.Max - box.Min;
		}

		float TestGetVolume()
		{
			FBox box = FBox(FVector(0, 0, 0), FVector(10, 10, 10));
			return box.GetVolume();
		}

		FBox TestExpandBy()
		{
			FBox box = FBox(FVector(0, 0, 0), FVector(100, 100, 100));
			return box.ExpandBy(10.0);
		}

		FBox TestPlusOperator()
		{
			FBox box = FBox(FVector(0, 0, 0), FVector(100, 100, 100));
			FVector point = FVector(200, 200, 200);
			return box + point;
		}

		bool TestIsValid()
		{
			// FBox::IsValid (uint8 member) is not exposed on the AS binding surface;
			// verify validity through the supported volume/extent surface instead.
			FBox box = FBox(FVector(0, 0, 0), FVector(100, 100, 100));
			return box.GetVolume() > 0.0 && box.Max.Equals(FVector(100, 100, 100), 0.001);
		}

		bool TestIntersectAndOverlap()
		{
			FBox box = FBox(FVector(0, 0, 0), FVector(10, 10, 10));
			FBox other = FBox(FVector(5, 5, 5), FVector(20, 20, 20));
			FBox overlap = box.Overlap(other);
			return box.Intersect(other)
				&& box.IntersectXY(other)
				&& overlap.Min.Equals(FVector(5, 5, 5), 0.001)
				&& overlap.Max.Equals(FVector(10, 10, 10), 0.001);
		}

		bool TestInsideBoxAndBoundaryVariants()
		{
			FBox box = FBox(FVector(0, 0, 0), FVector(10, 10, 10));
			FBox inner = FBox(FVector(2, 2, 2), FVector(8, 8, 8));
			FVector boundary = FVector(10, 5, 5);
			FVector outsideZ = FVector(5, 5, 20);
			return box.IsInside(inner)
				&& !box.IsInside(boundary)
				&& box.IsInsideOrOn(boundary)
				&& box.IsInsideXY(outsideZ)
				&& box.IsInsideOrOnXY(FVector(10, 5, 20));
		}

		bool TestGetCenterAndExtentsOutParams()
		{
			FBox box = FBox(FVector(-2, -4, -6), FVector(6, 8, 10));
			FVector center;
			FVector extents;
			box.GetCenterAndExtents(center, extents);
			return center.Equals(FVector(2, 2, 2), 0.001)
				&& extents.Equals(FVector(4, 6, 8), 0.001);
		}

		bool TestClosestShiftMoveAndVectorExpand()
		{
			FBox box = FBox(FVector(0, 0, 0), FVector(10, 10, 10));
			FBox expanded = box.ExpandBy(FVector(1, 2, 3));
			FBox shifted = box.ShiftBy(FVector(5, 0, 0));
			FBox moved = box.MoveTo(FVector(100, 100, 100));
			FVector closest = box.GetClosestPointTo(FVector(20, 5, -5));
			return expanded.Min.Equals(FVector(-1, -2, -3), 0.001)
				&& expanded.Max.Equals(FVector(11, 12, 13), 0.001)
				&& shifted.Min.Equals(FVector(5, 0, 0), 0.001)
				&& shifted.Max.Equals(FVector(15, 10, 10), 0.001)
				&& moved.GetCenter().Equals(FVector(100, 100, 100), 0.001)
				&& closest.Equals(FVector(10, 5, 0), 0.001);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalStructSatisfies<FBox>(
			Engine,
			Module,
			TEXT("FBox TestConstruction()"),
			[](const FBox& Result) { return Result.Min.Equals(FVector(0, 0, 0), 0.001) && Result.Max.Equals(FVector(100, 100, 100), 0.001); },
			TEXT("FBox construction"));
		ExpectGlobalStructSatisfies<FBox>(
			Engine,
			Module,
			TEXT("FBox TestBuildAABB()"),
			[](const FBox& Result) { return Result.Min.Equals(FVector(0, 0, 0), 0.001) && Result.Max.Equals(FVector(100, 100, 100), 0.001); },
			TEXT("FBox BuildAABB"));

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestIsInside()"), true, TEXT("FBox.IsInside() point inside"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestIsInsideOutside()"), false, TEXT("FBox.IsInside() point outside"));

		ExpectGlobalStructSatisfies<FVector>(
			Engine,
			Module,
			TEXT("FVector TestGetCenter()"),
			[](const FVector& Result) { return Result.Equals(FVector(50, 50, 50), 0.001); },
			TEXT("FBox.GetCenter()"));
		ExpectGlobalStructSatisfies<FVector>(
			Engine,
			Module,
			TEXT("FVector TestGetExtent()"),
			[](const FVector& Result) { return Result.Equals(FVector(50, 50, 50), 0.001); },
			TEXT("FBox.GetExtent()"));
		ExpectGlobalStructSatisfies<FVector>(
			Engine,
			Module,
			TEXT("FVector TestGetSize()"),
			[](const FVector& Result) { return Result.Equals(FVector(100, 100, 100), 0.001); },
			TEXT("FBox.GetSize()"));

		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestGetVolume()"), 1000.0f, TEXT("FBox.GetVolume()"));

		ExpectGlobalStructSatisfies<FBox>(
			Engine,
			Module,
			TEXT("FBox TestExpandBy()"),
			[](const FBox& Result) { return (Result.Max - Result.Min).Equals(FVector(120, 120, 120), 0.001); },
			TEXT("FBox.ExpandBy()"));
		ExpectGlobalStructSatisfies<FBox>(
			Engine,
			Module,
			TEXT("FBox TestPlusOperator()"),
			[](const FBox& Result) { return Result.Max.Equals(FVector(200, 200, 200), 0.001); },
			TEXT("FBox + FVector operator"));

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestIsValid()"), true, TEXT("FBox validity via supported volume/extent surface (IsValid member not bound)"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestIntersectAndOverlap()"), true, TEXT("FBox Intersect/IntersectXY/Overlap"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestInsideBoxAndBoundaryVariants()"), true, TEXT("FBox IsInside variants should distinguish boundary and XY checks"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestGetCenterAndExtentsOutParams()"), true, TEXT("FBox.GetCenterAndExtents() out params"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestClosestShiftMoveAndVectorExpand()"), true, TEXT("FBox closest/shift/move/vector ExpandBy helpers"));
	}

	TEST_METHOD(FBox2DUnsupportedBoundary)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString UnsupportedSource = ASTEST_AS(R"AS(
		bool TriggerUnsupportedFBox2D()
		{
			FBox2D Box = FBox2D(FVector2D(0, 0), FVector2D(100, 100));
			return Box.IsInside(FVector2D(50, 50));
		}
		)AS");
		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("FBox2D"));
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCovMathGeom_FBox2DUnsupported"),
			UnsupportedSource,
			TEXT("FBox2D should remain an explicit compile-failure boundary until a runtime bind exists"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	// -------------------------------------------------------------------------
	// FPlane: Plane operations
	// -------------------------------------------------------------------------
	TEST_METHOD(FPlaneOperations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovMathGeom_FPlane", ASTEST_AS(R"AS(
		FPlane TestConstruction()
		{
			FVector normal = FVector(0, 0, 1);
			FVector location = FVector(0, 0, 10);
			return FPlane(location, normal);
		}

		FPlane TestConstructionFromPoints()
		{
			FVector a = FVector(0, 0, 0);
			FVector b = FVector(1, 0, 0);
			FVector c = FVector(0, 1, 0);
			return FPlane(a, b, c);
		}

		FPlane TestConstructionFromVector()
		{
			return FPlane(FVector::ZeroVector, FVector(0, 0, 1));
		}

		float TestPlaneDot()
		{
			FPlane plane = FPlane(FVector::ZeroVector, FVector(0, 0, 1));
			FVector point = FVector(0, 0, 10);
			return plane.PlaneDot(point);
		}

		FVector TestGetNormal()
		{
			FPlane plane = FPlane(FVector(0, 0, 10), FVector(0, 0, 2));
			return plane.GetNormal();
		}

		FVector TestGetOrigin()
		{
			FPlane plane = FPlane(FVector(0, 0, 10), FVector(0, 0, 1));
			return plane.GetOrigin();
		}

		bool TestEquals()
		{
			// FPlane has no AS-facing opEquals/Equals; compare via the supported
			// normal + plane-distance surface instead.
			FPlane a = FPlane(FVector(0, 0, 10), FVector(0, 0, 1));
			FPlane b = FPlane(FVector(0, 0, 10), FVector(0, 0, 1));
			return a.GetNormal().Equals(b.GetNormal(), 0.001)
				&& Math::Abs(a.PlaneDot(FVector::ZeroVector) - b.PlaneDot(FVector::ZeroVector)) < 0.001;
		}

		bool TestRayPlaneIntersection()
		{
			FPlane plane = FPlane(FVector::ZeroVector, FVector(0, 0, 1));
			FVector intersection = plane.RayPlaneIntersection(FVector(0, 0, -5), FVector(0, 0, 1));
			return intersection.Equals(FVector::ZeroVector, 0.001);
		}

		bool TestSegmentPlaneIntersection()
		{
			FPlane plane = FPlane(FVector::ZeroVector, FVector(0, 0, 1));
			FVector intersection;
			bool hit = plane.SegmentPlaneIntersection(FVector(0, 0, -5), FVector(0, 0, 5), intersection);
			return hit && intersection.Equals(FVector::ZeroVector, 0.001);
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalStructSatisfies<FPlane>(
			Engine,
			Module,
			TEXT("FPlane TestConstruction()"),
			[](const FPlane& Result) { return Result.Z > 0.9f && FMath::Abs(Result.W - 10.0f) < 0.1f; },
			TEXT("FPlane construction"));
		ExpectGlobalStructSatisfies<FPlane>(
			Engine,
			Module,
			TEXT("FPlane TestConstructionFromPoints()"),
			[](const FPlane& Result) { return Result.Z > 0.9f || Result.Z < -0.9f; },
			TEXT("FPlane construction from points"));
		ExpectGlobalStructSatisfies<FPlane>(
			Engine,
			Module,
			TEXT("FPlane TestConstructionFromVector()"),
			[](const FPlane& Result) { return Result.Z > 0.9f; },
			TEXT("FPlane construction from vector"));
		ExpectGlobalReturn<float>(Engine, Module, TEXT("float TestPlaneDot()"), 10.0f, TEXT("FPlane.PlaneDot()"));
		ExpectGlobalStructSatisfies<FVector>(
			Engine,
			Module,
			TEXT("FVector TestGetNormal()"),
			[](const FVector& Result) { return Result.Equals(FVector(0, 0, 1), 0.001); },
			TEXT("FPlane.GetNormal()"));
		ExpectGlobalStructSatisfies<FVector>(
			Engine,
			Module,
			TEXT("FVector TestGetOrigin()"),
			[](const FVector& Result) { return Result.Equals(FVector(0, 0, 10), 0.001); },
			TEXT("FPlane.GetOrigin()"));

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestEquals()"), true, TEXT("FPlane equality"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestRayPlaneIntersection()"), true, TEXT("FPlane.RayPlaneIntersection()"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool TestSegmentPlaneIntersection()"), true, TEXT("FPlane.SegmentPlaneIntersection() out param"));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
