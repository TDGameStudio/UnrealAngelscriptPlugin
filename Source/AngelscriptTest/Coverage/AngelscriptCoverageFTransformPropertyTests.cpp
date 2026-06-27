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
// AngelscriptCoverageFTransformPropertyTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript FTransform *UPROPERTY usage* -- the FProperty
// reflection half of the FTransform matrix.
//
// FTransform represents a 3D transformation with:
//   - Location (FVector): translation
//   - Rotation (FQuat): rotation as quaternion
//   - Scale3D (FVector): 3D scale
//
// Test pattern: Pattern D (Actor + FProperty reflection)
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageFTransformPropertyTest,
	"Angelscript.TestModule.Coverage.FTransformProperty",
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
	// FTransform declaration defaults: identity, custom values.
	// -------------------------------------------------------------------------
	TEST_METHOD(FTransformDeclarationDefaults)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFTransformProperty_Defaults"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFTransformPropertyDefaults.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFTransformDefaultsActor : AActor
			{
				UPROPERTY()
				FTransform IdentityTransform = FTransform::Identity;

				UPROPERTY()
				FTransform CustomTransform = FTransform(FVector(100, 200, 300));

				UPROPERTY()
				FTransform NoDefaultTransform;

				UPROPERTY()
				FTransform FullTransform = FTransform(FQuat::Identity, FVector(10, 20, 30), FVector(2, 2, 2));
			}
			)AS"),
			TEXT("ACoverageFTransformDefaultsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FTransform-defaults actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FTransform-defaults actor should spawn")));

		// Identity transform
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("IdentityTransform.Translation.X"), 0.0, TEXT("FTransform::Identity.Translation.X"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("IdentityTransform.Translation.Y"), 0.0, TEXT("FTransform::Identity.Translation.Y"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("IdentityTransform.Translation.Z"), 0.0, TEXT("FTransform::Identity.Translation.Z"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("IdentityTransform.Scale3D.X"), 1.0, TEXT("FTransform::Identity.Scale3D.X"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("IdentityTransform.Scale3D.Y"), 1.0, TEXT("FTransform::Identity.Scale3D.Y"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("IdentityTransform.Scale3D.Z"), 1.0, TEXT("FTransform::Identity.Scale3D.Z"));

		// Custom transform (location only)
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("CustomTransform.Translation.X"), 100.0, TEXT("FTransform custom Translation.X"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("CustomTransform.Translation.Y"), 200.0, TEXT("FTransform custom Translation.Y"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("CustomTransform.Translation.Z"), 300.0, TEXT("FTransform custom Translation.Z"));

		// No default (should be identity)
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("NoDefaultTransform.Translation.X"), 0.0, TEXT("FTransform no default Translation.X"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("NoDefaultTransform.Scale3D.X"), 1.0, TEXT("FTransform no default Scale3D.X"));

		// Full transform (rotation, location, scale)
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("FullTransform.Translation.X"), 10.0, TEXT("FTransform full Translation.X"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("FullTransform.Translation.Y"), 20.0, TEXT("FTransform full Translation.Y"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("FullTransform.Translation.Z"), 30.0, TEXT("FTransform full Translation.Z"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("FullTransform.Scale3D.X"), 2.0, TEXT("FTransform full Scale3D.X"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("FullTransform.Scale3D.Y"), 2.0, TEXT("FTransform full Scale3D.Y"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("FullTransform.Scale3D.Z"), 2.0, TEXT("FTransform full Scale3D.Z"));
	}

	// -------------------------------------------------------------------------
	// FTransform write round-trip: SetByPath → read back.
	// -------------------------------------------------------------------------
	TEST_METHOD(FTransformWriteRoundTrip)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFTransformProperty_WriteRoundTrip"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFTransformPropertyWriteRoundTrip.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFTransformWriteActor : AActor
			{
				UPROPERTY()
				FTransform TransformValue;
			}
			)AS"),
			TEXT("ACoverageFTransformWriteActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FTransform-write actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FTransform-write actor should spawn")));

		// Write translation values
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("TransformValue.Translation.X"), 100.0)));
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("TransformValue.Translation.Y"), 200.0)));
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("TransformValue.Translation.Z"), 300.0)));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("TransformValue.Translation.X"), 100.0, TEXT("FTransform write Translation.X"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("TransformValue.Translation.Y"), 200.0, TEXT("FTransform write Translation.Y"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("TransformValue.Translation.Z"), 300.0, TEXT("FTransform write Translation.Z"));

		// Write scale values
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("TransformValue.Scale3D.X"), 2.0)));
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("TransformValue.Scale3D.Y"), 3.0)));
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("TransformValue.Scale3D.Z"), 4.0)));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("TransformValue.Scale3D.X"), 2.0, TEXT("FTransform write Scale3D.X"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("TransformValue.Scale3D.Y"), 3.0, TEXT("FTransform write Scale3D.Y"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("TransformValue.Scale3D.Z"), 4.0, TEXT("FTransform write Scale3D.Z"));

		// Write negative translation
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("TransformValue.Translation.X"), -50.0)));
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("TransformValue.Translation.Y"), -100.0)));
		ASSERT_THAT(IsTrue(SetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("TransformValue.Translation.Z"), -150.0)));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("TransformValue.Translation.X"), -50.0, TEXT("FTransform write negative Translation.X"));
	}

	// -------------------------------------------------------------------------
	// FTransform member access: Location, Rotation, Scale3D.
	// -------------------------------------------------------------------------
	TEST_METHOD(FTransformMemberAccess)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFTransformProperty_MemberAccess"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFTransformPropertyMemberAccess.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFTransformMemberActor : AActor
			{
				UPROPERTY()
				FTransform MyTransform = FTransform(FQuat::Identity, FVector(10, 20, 30), FVector(2, 2, 2));

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Modify through member access
					MyTransform.Location = FVector(100, 200, 300);
					MyTransform.Scale3D = FVector(5, 5, 5);
				}
			}
			)AS"),
			TEXT("ACoverageFTransformMemberActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FTransform-member actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FTransform-member actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Verify modified location
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("MyTransform.Translation.X"), 100.0, TEXT("FTransform.Location modified X"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("MyTransform.Translation.Y"), 200.0, TEXT("FTransform.Location modified Y"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("MyTransform.Translation.Z"), 300.0, TEXT("FTransform.Location modified Z"));

		// Verify modified scale
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("MyTransform.Scale3D.X"), 5.0, TEXT("FTransform.Scale3D modified X"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("MyTransform.Scale3D.Y"), 5.0, TEXT("FTransform.Scale3D modified Y"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("MyTransform.Scale3D.Z"), 5.0, TEXT("FTransform.Scale3D modified Z"));
	}

	// -------------------------------------------------------------------------
	// FTransform containers: TArray.
	// -------------------------------------------------------------------------
	TEST_METHOD(FTransformContainerProperties)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFTransformProperty_Container"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFTransformPropertyContainer.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFTransformContainerActor : AActor
			{
				UPROPERTY()
				TArray<FTransform> TransformArray;

				UPROPERTY()
				TMap<int, FTransform> IntToTransformMap;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					TransformArray.Add(FTransform(FVector(100, 0, 0)));
					TransformArray.Add(FTransform(FVector(0, 200, 0)));
					TransformArray.Add(FTransform(FVector(0, 0, 300)));

					IntToTransformMap.Add(1, FTransform(FVector(10, 0, 0)));
					IntToTransformMap.Add(2, FTransform(FVector(0, 20, 0)));
					IntToTransformMap.Add(3, FTransform(FVector(0, 0, 30)));
				}
			}
			)AS"),
			TEXT("ACoverageFTransformContainerActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FTransform-container actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FTransform-container actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// TArray<FTransform>
		{
			int32 Length = 0;
			ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("TransformArray"), Length)));
			ASSERT_THAT(AreEqual(3, Length, TEXT("TArray<FTransform> should have 3 elements")));

			VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("TransformArray[0].Translation.X"), 100.0, TEXT("TArray<FTransform>[0].Translation.X"));
			VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("TransformArray[0].Translation.Y"), 0.0, TEXT("TArray<FTransform>[0].Translation.Y"));
			VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("TransformArray[1].Translation.Y"), 200.0, TEXT("TArray<FTransform>[1].Translation.Y"));
			VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("TransformArray[2].Translation.Z"), 300.0, TEXT("TArray<FTransform>[2].Translation.Z"));
		}

		// TMap<int, FTransform>
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("IntToTransformMap"), Count)));
			ASSERT_THAT(AreEqual(3, Count, TEXT("TMap<int,FTransform> should have 3 entries")));

			VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("IntToTransformMap[1].Translation.X"), 10.0, TEXT("TMap<int,FTransform>[1].Translation.X"));
			VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("IntToTransformMap[2].Translation.Y"), 20.0, TEXT("TMap<int,FTransform>[2].Translation.Y"));
			VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("IntToTransformMap[3].Translation.Z"), 30.0, TEXT("TMap<int,FTransform>[3].Translation.Z"));
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
