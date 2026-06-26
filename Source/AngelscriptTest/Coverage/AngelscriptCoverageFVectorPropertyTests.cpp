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
// AngelscriptCoverageFVectorPropertyTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript FVector *UPROPERTY usage* -- the FProperty
// reflection half of the FVector matrix.
//
// FVector is a 3D vector type (double precision in UE5):
//   - Construction: FVector(x, y, z), FVector::ZeroVector, etc.
//   - Operations: +, -, *, /, dot (|), cross (^)
//   - Methods: Length(), Normalize(), Distance(), etc.
//
// Test pattern: Pattern D (Actor + FProperty reflection)
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageFVectorPropertyTest,
	"Angelscript.TestModule.Coverage.FVectorProperty",
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
	// FVector declaration defaults: zero, unit, custom values.
	// -------------------------------------------------------------------------
	TEST_METHOD(FVectorDeclarationDefaults)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFVectorProperty_Defaults"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFVectorPropertyDefaults.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFVectorDefaultsActor : AActor
			{
				UPROPERTY()
				FVector ZeroVec = FVector::ZeroVector;

				UPROPERTY()
				FVector OneVec = FVector::OneVector;

				UPROPERTY()
				FVector CustomVec = FVector(1, 2, 3);

				UPROPERTY()
				FVector NoDefaultVec;

				UPROPERTY()
				FVector UpVec = FVector::UpVector;
			}
			)AS"),
			TEXT("ACoverageFVectorDefaultsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FVector-defaults actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FVector-defaults actor should spawn")));

		// Zero vector
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("ZeroVec.X"), 0.0, TEXT("FVector::ZeroVector.X"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("ZeroVec.Y"), 0.0, TEXT("FVector::ZeroVector.Y"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("ZeroVec.Z"), 0.0, TEXT("FVector::ZeroVector.Z"));

		// One vector
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("OneVec.X"), 1.0, TEXT("FVector::OneVector.X"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("OneVec.Y"), 1.0, TEXT("FVector::OneVector.Y"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("OneVec.Z"), 1.0, TEXT("FVector::OneVector.Z"));

		// Custom vector
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("CustomVec.X"), 1.0, TEXT("FVector custom X"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("CustomVec.Y"), 2.0, TEXT("FVector custom Y"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("CustomVec.Z"), 3.0, TEXT("FVector custom Z"));

		// No default (should be zero)
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("NoDefaultVec.X"), 0.0, TEXT("FVector no default X"));

		// Up vector
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("UpVec.X"), 0.0, TEXT("FVector::UpVector.X"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("UpVec.Y"), 0.0, TEXT("FVector::UpVector.Y"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("UpVec.Z"), 1.0, TEXT("FVector::UpVector.Z"));
	}

	// -------------------------------------------------------------------------
	// FVector write round-trip: SetByPath → read back.
	// -------------------------------------------------------------------------
	TEST_METHOD(FVectorWriteRoundTrip)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFVectorProperty_WriteRoundTrip"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFVectorPropertyWriteRoundTrip.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFVectorWriteActor : AActor
			{
				UPROPERTY()
				FVector VectorValue;
			}
			)AS"),
			TEXT("ACoverageFVectorWriteActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FVector-write actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FVector-write actor should spawn")));

		// Write positive values (set components individually)
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorValue.X"), 10.0)));
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorValue.Y"), 20.0)));
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorValue.Z"), 30.0)));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorValue.X"), 10.0, TEXT("FVector write X"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorValue.Y"), 20.0, TEXT("FVector write Y"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorValue.Z"), 30.0, TEXT("FVector write Z"));

		// Write negative values
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorValue.X"), -5.0)));
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorValue.Y"), -10.0)));
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorValue.Z"), -15.0)));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorValue.X"), -5.0, TEXT("FVector write negative X"));

		// Write zero
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorValue.X"), 0.0)));
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorValue.Y"), 0.0)));
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorValue.Z"), 0.0)));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorValue.X"), 0.0, TEXT("FVector write zero X"));
	}

	// -------------------------------------------------------------------------
	// FVector containers: TArray.
	// -------------------------------------------------------------------------
	TEST_METHOD(FVectorContainerProperties)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFVectorProperty_Container"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFVectorPropertyContainer.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFVectorContainerActor : AActor
			{
				UPROPERTY()
				TArray<FVector> VectorArray;

				UPROPERTY()
				TMap<int, FVector> IntToVectorMap;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					VectorArray.Add(FVector(1, 0, 0));
					VectorArray.Add(FVector(0, 1, 0));
					VectorArray.Add(FVector(0, 0, 1));

					IntToVectorMap.Add(1, FVector::ForwardVector);
					IntToVectorMap.Add(2, FVector::RightVector);
					IntToVectorMap.Add(3, FVector::UpVector);
				}
			}
			)AS"),
			TEXT("ACoverageFVectorContainerActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FVector-container actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FVector-container actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// TArray<FVector>
		{
			int32 Length = 0;
			ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("VectorArray"), Length)));
			ASSERT_THAT(AreEqual(3, Length, TEXT("TArray<FVector> should have 3 elements")));

			VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorArray[0].X"), 1.0, TEXT("TArray<FVector>[0].X"));
			VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorArray[0].Y"), 0.0, TEXT("TArray<FVector>[0].Y"));
			VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorArray[1].Y"), 1.0, TEXT("TArray<FVector>[1].Y"));
			VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorArray[2].Z"), 1.0, TEXT("TArray<FVector>[2].Z"));
		}

		// TMap<int, FVector>
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("IntToVectorMap"), Count)));
			ASSERT_THAT(AreEqual(3, Count, TEXT("TMap<int,FVector> should have 3 entries")));

			// Access map values through nested path
			VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("IntToVectorMap[1].X"), 1.0, TEXT("TMap<int,FVector>[1].X (ForwardVector)"));
			VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("IntToVectorMap[3].Z"), 1.0, TEXT("TMap<int,FVector>[3].Z (UpVector)"));
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
