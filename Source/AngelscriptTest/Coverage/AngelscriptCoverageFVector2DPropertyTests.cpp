#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageFVector2DPropertyTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript FVector2D *UPROPERTY usage* -- the FProperty
// reflection half of the FVector2D matrix.
//
// FVector2D is a 2D vector type (double precision in UE5):
//   - Construction: FVector2D(x, y), FVector2D::ZeroVector, etc.
//   - Operations: +, -, *, /, dot (|)
//   - Methods: Length(), Normalize(), Distance(), etc.
//
// Test pattern: Pattern D (Actor + FProperty reflection)
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageFVector2DPropertyTest,
	"Angelscript.TestModule.Coverage.FVector2DProperty",
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

	// -------------------------------------------------------------------------
	// FVector2D declaration defaults: zero, unit, custom values.
	// -------------------------------------------------------------------------
	TEST_METHOD(FVector2DDeclarationDefaults)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFVector2DProperty_Defaults"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFVector2DPropertyDefaults.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFVector2DDefaultsActor : AActor
			{
				UPROPERTY()
				FVector2D ZeroVec = FVector2D::ZeroVector;

				UPROPERTY()
				FVector2D OneVec = FVector2D(1, 1);

				UPROPERTY()
				FVector2D CustomVec = FVector2D(5, 10);

				UPROPERTY()
				FVector2D NoDefaultVec;

				UPROPERTY()
				FVector2D UnitXVec = FVector2D(1, 0);
			}
			)AS"),
			TEXT("ACoverageFVector2DDefaultsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FVector2D-defaults actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FVector2D-defaults actor should spawn")));

		// Zero vector
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("ZeroVec.X"), 0.0, TEXT("FVector2D::ZeroVector.X"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("ZeroVec.Y"), 0.0, TEXT("FVector2D::ZeroVector.Y"));

		// One vector
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("OneVec.X"), 1.0, TEXT("FVector2D(1,1).X"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("OneVec.Y"), 1.0, TEXT("FVector2D(1,1).Y"));

		// Custom vector
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("CustomVec.X"), 5.0, TEXT("FVector2D custom X"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("CustomVec.Y"), 10.0, TEXT("FVector2D custom Y"));

		// No default (should be zero)
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("NoDefaultVec.X"), 0.0, TEXT("FVector2D no default X"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("NoDefaultVec.Y"), 0.0, TEXT("FVector2D no default Y"));

		// Unit X vector
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("UnitXVec.X"), 1.0, TEXT("FVector2D unit X.X"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("UnitXVec.Y"), 0.0, TEXT("FVector2D unit X.Y"));
	}

	// -------------------------------------------------------------------------
	// FVector2D write round-trip: SetByPath → read back.
	// -------------------------------------------------------------------------
	TEST_METHOD(FVector2DWriteRoundTrip)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFVector2DProperty_WriteRoundTrip"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFVector2DPropertyWriteRoundTrip.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFVector2DWriteActor : AActor
			{
				UPROPERTY()
				FVector2D VectorValue;
			}
			)AS"),
			TEXT("ACoverageFVector2DWriteActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FVector2D-write actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FVector2D-write actor should spawn")));

		// Write positive values
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorValue.X"), 100.0)));
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorValue.Y"), 200.0)));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorValue.X"), 100.0, TEXT("FVector2D write X"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorValue.Y"), 200.0, TEXT("FVector2D write Y"));

		// Write negative values
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorValue.X"), -50.0)));
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorValue.Y"), -75.0)));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorValue.X"), -50.0, TEXT("FVector2D write negative X"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorValue.Y"), -75.0, TEXT("FVector2D write negative Y"));

		// Write zero
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorValue.X"), 0.0)));
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorValue.Y"), 0.0)));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorValue.X"), 0.0, TEXT("FVector2D write zero X"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorValue.Y"), 0.0, TEXT("FVector2D write zero Y"));
	}

	// -------------------------------------------------------------------------
	// FVector2D containers: TArray.
	// -------------------------------------------------------------------------
	TEST_METHOD(FVector2DContainerProperties)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFVector2DProperty_Container"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFVector2DPropertyContainer.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFVector2DContainerActor : AActor
			{
				UPROPERTY()
				TArray<FVector2D> VectorArray;

				UPROPERTY()
				TMap<int, FVector2D> IntToVectorMap;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					VectorArray.Add(FVector2D(1, 0));
					VectorArray.Add(FVector2D(0, 1));
					VectorArray.Add(FVector2D(1, 1));

					IntToVectorMap.Add(1, FVector2D(10, 20));
					IntToVectorMap.Add(2, FVector2D(30, 40));
					IntToVectorMap.Add(3, FVector2D::ZeroVector);
				}
			}
			)AS"),
			TEXT("ACoverageFVector2DContainerActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FVector2D-container actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FVector2D-container actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// TArray<FVector2D>
		{
			int32 Length = 0;
			ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("VectorArray"), Length)));
			ASSERT_THAT(AreEqual(3, Length, TEXT("TArray<FVector2D> should have 3 elements")));

			VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorArray[0].X"), 1.0, TEXT("TArray<FVector2D>[0].X"));
			VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorArray[0].Y"), 0.0, TEXT("TArray<FVector2D>[0].Y"));
			VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorArray[1].Y"), 1.0, TEXT("TArray<FVector2D>[1].Y"));
			VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorArray[2].X"), 1.0, TEXT("TArray<FVector2D>[2].X"));
			VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorArray[2].Y"), 1.0, TEXT("TArray<FVector2D>[2].Y"));
		}

		// TMap<int, FVector2D>
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("IntToVectorMap"), Count)));
			ASSERT_THAT(AreEqual(3, Count, TEXT("TMap<int,FVector2D> should have 3 entries")));

			VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("IntToVectorMap[1].X"), 10.0, TEXT("TMap<int,FVector2D>[1].X"));
			VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("IntToVectorMap[1].Y"), 20.0, TEXT("TMap<int,FVector2D>[1].Y"));
			VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("IntToVectorMap[3].X"), 0.0, TEXT("TMap<int,FVector2D>[3].X (ZeroVector)"));
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
