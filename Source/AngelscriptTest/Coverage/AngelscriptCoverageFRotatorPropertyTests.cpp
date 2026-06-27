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
// AngelscriptCoverageFRotatorPropertyTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript FRotator *UPROPERTY usage* -- the FProperty
// reflection half of the FRotator matrix.
//
// FRotator is a rotation representation using Euler angles (Pitch, Yaw, Roll):
//   - Construction: FRotator(Pitch, Yaw, Roll), FRotator::ZeroRotator, etc.
//   - Operations: +, -, *, ==, !=
//   - Methods: Normalize(), Clamp(), Vector(), Quaternion(), etc.
//
// Test pattern: Pattern D (Actor + FProperty reflection)
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageFRotatorPropertyTest,
	"Angelscript.TestModule.Coverage.FRotatorProperty",
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
	// FRotator declaration defaults: zero, custom values.
	// -------------------------------------------------------------------------
	TEST_METHOD(FRotatorDeclarationDefaults)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFRotatorProperty_Defaults"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFRotatorPropertyDefaults.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFRotatorDefaultsActor : AActor
			{
				UPROPERTY()
				FRotator ZeroRot = FRotator::ZeroRotator;

				UPROPERTY()
				FRotator CustomRot = FRotator(10, 20, 30);

				UPROPERTY()
				FRotator NoDefaultRot;

				UPROPERTY()
				FRotator PitchOnly = FRotator(45, 0, 0);
			}
			)AS"),
			TEXT("ACoverageFRotatorDefaultsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FRotator-defaults actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FRotator-defaults actor should spawn")));

		// Zero rotator
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("ZeroRot.Pitch"), 0.0, TEXT("FRotator::ZeroRotator.Pitch"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("ZeroRot.Yaw"), 0.0, TEXT("FRotator::ZeroRotator.Yaw"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("ZeroRot.Roll"), 0.0, TEXT("FRotator::ZeroRotator.Roll"));

		// Custom rotator
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("CustomRot.Pitch"), 10.0, TEXT("FRotator custom Pitch"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("CustomRot.Yaw"), 20.0, TEXT("FRotator custom Yaw"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("CustomRot.Roll"), 30.0, TEXT("FRotator custom Roll"));

		// No default (should be zero)
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("NoDefaultRot.Pitch"), 0.0, TEXT("FRotator no default Pitch"));

		// Pitch only
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("PitchOnly.Pitch"), 45.0, TEXT("FRotator PitchOnly Pitch"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("PitchOnly.Yaw"), 0.0, TEXT("FRotator PitchOnly Yaw"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("PitchOnly.Roll"), 0.0, TEXT("FRotator PitchOnly Roll"));
	}

	// -------------------------------------------------------------------------
	// FRotator write round-trip: SetByPath → read back.
	// -------------------------------------------------------------------------
	TEST_METHOD(FRotatorWriteRoundTrip)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFRotatorProperty_WriteRoundTrip"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFRotatorPropertyWriteRoundTrip.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFRotatorWriteActor : AActor
			{
				UPROPERTY()
				FRotator RotatorValue;
			}
			)AS"),
			TEXT("ACoverageFRotatorWriteActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FRotator-write actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FRotator-write actor should spawn")));

		// Write positive values (set components individually)
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("RotatorValue.Pitch"), 45.0)));
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("RotatorValue.Yaw"), 90.0)));
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("RotatorValue.Roll"), 180.0)));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("RotatorValue.Pitch"), 45.0, TEXT("FRotator write Pitch"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("RotatorValue.Yaw"), 90.0, TEXT("FRotator write Yaw"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("RotatorValue.Roll"), 180.0, TEXT("FRotator write Roll"));

		// Write negative values
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("RotatorValue.Pitch"), -30.0)));
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("RotatorValue.Yaw"), -60.0)));
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("RotatorValue.Roll"), -90.0)));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("RotatorValue.Pitch"), -30.0, TEXT("FRotator write negative Pitch"));

		// Write zero
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("RotatorValue.Pitch"), 0.0)));
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("RotatorValue.Yaw"), 0.0)));
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("RotatorValue.Roll"), 0.0)));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("RotatorValue.Pitch"), 0.0, TEXT("FRotator write zero Pitch"));
	}

	// -------------------------------------------------------------------------
	// FRotator containers: TArray.
	// -------------------------------------------------------------------------
	TEST_METHOD(FRotatorContainerProperties)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFRotatorProperty_Container"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFRotatorPropertyContainer.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFRotatorContainerActor : AActor
			{
				UPROPERTY()
				TArray<FRotator> RotatorArray;

				UPROPERTY()
				TMap<int, FRotator> IntToRotatorMap;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					RotatorArray.Add(FRotator(0, 0, 0));
					RotatorArray.Add(FRotator(90, 0, 0));
					RotatorArray.Add(FRotator(0, 180, 0));

					IntToRotatorMap.Add(1, FRotator(45, 0, 0));
					IntToRotatorMap.Add(2, FRotator(0, 90, 0));
					IntToRotatorMap.Add(3, FRotator(0, 0, 45));
				}
			}
			)AS"),
			TEXT("ACoverageFRotatorContainerActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FRotator-container actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FRotator-container actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// TArray<FRotator>
		{
			int32 Length = 0;
			ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("RotatorArray"), Length)));
			ASSERT_THAT(AreEqual(3, Length, TEXT("TArray<FRotator> should have 3 elements")));

			VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("RotatorArray[0].Pitch"), 0.0, TEXT("TArray<FRotator>[0].Pitch"));
			VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("RotatorArray[1].Pitch"), 90.0, TEXT("TArray<FRotator>[1].Pitch"));
			VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("RotatorArray[2].Yaw"), 180.0, TEXT("TArray<FRotator>[2].Yaw"));
		}

		// TMap<int, FRotator>
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("IntToRotatorMap"), Count)));
			ASSERT_THAT(AreEqual(3, Count, TEXT("TMap<int,FRotator> should have 3 entries")));

			// Access map values through nested path
			VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("IntToRotatorMap[1].Pitch"), 45.0, TEXT("TMap<int,FRotator>[1].Pitch"));
			VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("IntToRotatorMap[2].Yaw"), 90.0, TEXT("TMap<int,FRotator>[2].Yaw"));
			VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("IntToRotatorMap[3].Roll"), 45.0, TEXT("TMap<int,FRotator>[3].Roll"));
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
