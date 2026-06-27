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
// AngelscriptCoverageFQuatPropertyTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript FQuat *UPROPERTY usage* -- the FProperty
// reflection half of the FQuat matrix.
//
// FQuat is a quaternion rotation representation:
//   - Construction: FQuat(X, Y, Z, W), FQuat::Identity, FQuat(Rotator), etc.
//   - Operations: *, RotateVector
//   - Methods: Inverse(), Normalize(), Slerp(), GetAxisX/Y/Z(), etc.
//
// Test pattern: Pattern D (Actor + FProperty reflection)
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageFQuatPropertyTest,
	"Angelscript.TestModule.Coverage.FQuatProperty",
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
	// FQuat declaration defaults: identity, custom values.
	// -------------------------------------------------------------------------
	TEST_METHOD(FQuatDeclarationDefaults)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFQuatProperty_Defaults"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFQuatPropertyDefaults.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFQuatDefaultsActor : AActor
			{
				UPROPERTY()
				FQuat IdentityQuat = FQuat::Identity;

				UPROPERTY()
				FQuat CustomQuat = FQuat(0, 0, 0.707107, 0.707107);

				UPROPERTY()
				FQuat NoDefaultQuat;

				UPROPERTY()
				FQuat FromRotator = FQuat(FRotator(0, 90, 0));
			}
			)AS"),
			TEXT("ACoverageFQuatDefaultsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FQuat-defaults actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FQuat-defaults actor should spawn")));

		// Identity quaternion
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("IdentityQuat.X"), 0.0, TEXT("FQuat::Identity.X"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("IdentityQuat.Y"), 0.0, TEXT("FQuat::Identity.Y"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("IdentityQuat.Z"), 0.0, TEXT("FQuat::Identity.Z"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("IdentityQuat.W"), 1.0, TEXT("FQuat::Identity.W"));

		// Custom quaternion (approximate 90-degree yaw rotation)
		double CustomZ = 0.0;
		double CustomW = 0.0;
		ASSERT_THAT(IsTrue(GetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("CustomQuat.Z"), CustomZ)));
		ASSERT_THAT(IsTrue(GetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("CustomQuat.W"), CustomW)));
		TestRunner->TestTrue(TEXT("FQuat custom Z near 0.707"), FMath::IsNearlyEqual(CustomZ, 0.707107, 0.001));
		TestRunner->TestTrue(TEXT("FQuat custom W near 0.707"), FMath::IsNearlyEqual(CustomW, 0.707107, 0.001));

		// No default (should be identity)
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("NoDefaultQuat.X"), 0.0, TEXT("FQuat no default X"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("NoDefaultQuat.W"), 1.0, TEXT("FQuat no default W"));

		// From rotator
		double FromRotZ = 0.0;
		ASSERT_THAT(IsTrue(GetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("FromRotator.Z"), FromRotZ)));
		TestRunner->TestTrue(TEXT("FQuat from FRotator(0,90,0) has non-zero Z"), FMath::Abs(FromRotZ) > 0.5);
	}

	// -------------------------------------------------------------------------
	// FQuat write round-trip: SetByPath → read back.
	// -------------------------------------------------------------------------
	TEST_METHOD(FQuatWriteRoundTrip)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFQuatProperty_WriteRoundTrip"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFQuatPropertyWriteRoundTrip.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFQuatWriteActor : AActor
			{
				UPROPERTY()
				FQuat QuatValue;
			}
			)AS"),
			TEXT("ACoverageFQuatWriteActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FQuat-write actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FQuat-write actor should spawn")));

		// Write custom values (set components individually)
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("QuatValue.X"), 0.1)));
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("QuatValue.Y"), 0.2)));
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("QuatValue.Z"), 0.3)));
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("QuatValue.W"), 0.9)));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("QuatValue.X"), 0.1, TEXT("FQuat write X"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("QuatValue.Y"), 0.2, TEXT("FQuat write Y"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("QuatValue.Z"), 0.3, TEXT("FQuat write Z"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("QuatValue.W"), 0.9, TEXT("FQuat write W"));

		// Write identity quaternion
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("QuatValue.X"), 0.0)));
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("QuatValue.Y"), 0.0)));
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("QuatValue.Z"), 0.0)));
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("QuatValue.W"), 1.0)));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("QuatValue.W"), 1.0, TEXT("FQuat write identity W"));
	}

	// -------------------------------------------------------------------------
	// FQuat containers: TArray.
	// -------------------------------------------------------------------------
	TEST_METHOD(FQuatContainerProperties)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFQuatProperty_Container"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFQuatPropertyContainer.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFQuatContainerActor : AActor
			{
				UPROPERTY()
				TArray<FQuat> QuatArray;

				UPROPERTY()
				TMap<int, FQuat> IntToQuatMap;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					QuatArray.Add(FQuat::Identity);
					QuatArray.Add(FQuat(FRotator(0, 90, 0)));
					QuatArray.Add(FQuat(FRotator(90, 0, 0)));

					IntToQuatMap.Add(1, FQuat::Identity);
					IntToQuatMap.Add(2, FQuat(FRotator(0, 45, 0)));
					IntToQuatMap.Add(3, FQuat(FRotator(45, 0, 0)));
				}
			}
			)AS"),
			TEXT("ACoverageFQuatContainerActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FQuat-container actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FQuat-container actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// TArray<FQuat>
		{
			int32 Length = 0;
			ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("QuatArray"), Length)));
			ASSERT_THAT(AreEqual(3, Length, TEXT("TArray<FQuat> should have 3 elements")));

			VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("QuatArray[0].W"), 1.0, TEXT("TArray<FQuat>[0].W (Identity)"));

			double Quat1Z = 0.0;
			ASSERT_THAT(IsTrue(GetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("QuatArray[1].Z"), Quat1Z)));
			TestRunner->TestTrue(TEXT("TArray<FQuat>[1].Z (90 yaw) non-zero"), FMath::Abs(Quat1Z) > 0.5);
		}

		// TMap<int, FQuat>
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("IntToQuatMap"), Count)));
			ASSERT_THAT(AreEqual(3, Count, TEXT("TMap<int,FQuat> should have 3 entries")));

			// Access map values through nested path
			VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("IntToQuatMap[1].W"), 1.0, TEXT("TMap<int,FQuat>[1].W (Identity)"));

			double Map2Z = 0.0;
			ASSERT_THAT(IsTrue(GetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("IntToQuatMap[2].Z"), Map2Z)));
			TestRunner->TestTrue(TEXT("TMap<int,FQuat>[2].Z (45 yaw) non-zero"), FMath::Abs(Map2Z) > 0.3);
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
