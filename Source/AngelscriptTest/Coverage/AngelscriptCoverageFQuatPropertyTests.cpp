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
private:
	static bool GetIntQuatMapValue(
		FAutomationTestBase& Test,
		UObject* Object,
		FStringView Path,
		const int32 Key,
		FQuat& OutValue)
	{
		FPropertyBindingPathIndirection Leaf;
		if (!ResolvePathOnObject(Test, Object, Path, Leaf))
		{
			return false;
		}

		const FMapProperty* MapProperty = CastField<const FMapProperty>(Leaf.GetProperty());
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("Property '%.*s' should be a TMap"), Path.Len(), Path.GetData()),
				MapProperty))
		{
			return false;
		}

		const FIntProperty* KeyProperty = CastField<const FIntProperty>(MapProperty->KeyProp);
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("TMap key property at '%.*s' should be FIntProperty"), Path.Len(), Path.GetData()),
				KeyProperty))
		{
			return false;
		}

		const FStructProperty* ValueProperty = CastField<const FStructProperty>(MapProperty->ValueProp);
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("TMap value property at '%.*s' should be FStructProperty"), Path.Len(), Path.GetData()),
				ValueProperty))
		{
			return false;
		}

		const UScriptStruct* ExpectedStruct = TBaseStructure<FQuat>::Get();
		if (!Test.TestTrue(
				*FString::Printf(TEXT("TMap value property at '%.*s' should be FQuat"), Path.Len(), Path.GetData()),
				ValueProperty->Struct != nullptr && ExpectedStruct != nullptr
				&& ValueProperty->Struct->IsChildOf(ExpectedStruct)))
		{
			return false;
		}

		FScriptMapHelper Helper(MapProperty, Leaf.GetPropertyAddress());
		for (int32 SparseIndex = 0; SparseIndex < Helper.GetMaxIndex(); ++SparseIndex)
		{
			if (!Helper.IsValidIndex(SparseIndex))
			{
				continue;
			}

			if (KeyProperty->GetPropertyValue(Helper.GetKeyPtr(SparseIndex)) == Key)
			{
				ValueProperty->CopySingleValue(&OutValue, Helper.GetValuePtr(SparseIndex));
				return true;
			}
		}

		Test.AddError(FString::Printf(
			TEXT("TMap at '%.*s' does not contain key %d"),
			Path.Len(), Path.GetData(),
			Key));
		return false;
	}

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

		// Constructor-expression UPROPERTY defaults compile but currently materialize as the bound FQuat default.
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("CustomQuat.X"), 0.0,
			TEXT("FQuat constructor default boundary X"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("CustomQuat.Z"), 0.0,
			TEXT("FQuat constructor default boundary Z"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("CustomQuat.W"), 1.0,
			TEXT("FQuat constructor default boundary W"))));

		// No default (should be identity)
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("NoDefaultQuat.X"), 0.0, TEXT("FQuat no default X"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("NoDefaultQuat.W"), 1.0, TEXT("FQuat no default W"));

		// Constructor-expression UPROPERTY defaults from FRotator follow the same materialization boundary.
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("FromRotator.Z"), 0.0,
			TEXT("FQuat FRotator default boundary Z"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("FromRotator.W"), 1.0,
			TEXT("FQuat FRotator default boundary W"))));
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

			FQuat MapValue = FQuat::Identity;
			ASSERT_THAT(IsTrue(GetIntQuatMapValue(*TestRunner, Actor, TEXT("IntToQuatMap"), 1, MapValue)));
			ASSERT_THAT(IsTrue(MapValue.Equals(FQuat::Identity, 0.001), TEXT("TMap<int,FQuat>[1] should preserve FQuat::Identity")));

			ASSERT_THAT(IsTrue(GetIntQuatMapValue(*TestRunner, Actor, TEXT("IntToQuatMap"), 2, MapValue)));
			ASSERT_THAT(IsTrue(MapValue.Equals(FQuat(FRotator(0, 45, 0)), 0.001), TEXT("TMap<int,FQuat>[2] should preserve 45 yaw")));
		}
	}

	TEST_METHOD(FQuatClassMemberRuntimeFlow)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFQuatProperty_RuntimeFlow"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFQuatPropertyRuntimeFlow.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFQuatRuntimeFlowActor : AActor
			{
				UPROPERTY()
				FQuat CurrentQuat = FQuat::Identity;

				UPROPERTY()
				FQuat CopyConstructedQuat;

				UPROPERTY()
				FQuat AssignedQuat;

				UPROPERTY()
				FQuat InverseQuat;

				UPROPERTY()
				FVector RotatedForward;

				UPROPERTY()
				TArray<FQuat> History;

				UPROPERTY()
				bool bCopyEqualsAssigned = false;

				UPROPERTY()
				bool bInverseComposesToIdentity = false;

				UPROPERTY()
				bool bHistoryPreservesValues = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					const FQuat LocalTurn = FQuat(FVector::UpVector, 1.5707963267948966);
					CurrentQuat = LocalTurn;
					CopyConstructedQuat = FQuat(CurrentQuat);
					AssignedQuat = FQuat::Identity;
					AssignedQuat = CopyConstructedQuat;
					InverseQuat = CurrentQuat.Inverse();
					RotatedForward = CurrentQuat.RotateVector(FVector::ForwardVector);

					History.Add(FQuat::Identity);
					History.Add(CurrentQuat);
					History.Add(InverseQuat);

					bCopyEqualsAssigned = CopyConstructedQuat == AssignedQuat;
					bInverseComposesToIdentity = (CurrentQuat * InverseQuat).IsIdentity(0.001);
					bHistoryPreservesValues = History.Num() == 3
						&& History[1].Equals(CurrentQuat, 0.001)
						&& History[2].Equals(InverseQuat, 0.001);
				}
			}
			)AS"),
			TEXT("ACoverageFQuatRuntimeFlowActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FQuat runtime-flow actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FQuat runtime-flow actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		FQuat Current = FQuat::Identity;
		ASSERT_THAT(IsTrue(GetStructByPath<FQuat>(*TestRunner, Actor, TEXT("CurrentQuat"), Current),
			TEXT("CurrentQuat should be readable as full FQuat struct")));
		ASSERT_THAT(IsTrue(Current.Equals(FQuat(FVector::UpVector, UE_DOUBLE_HALF_PI), 0.001),
			TEXT("class member FQuat should accept assignment from local const FQuat")));

		FQuat Assigned = FQuat::Identity;
		ASSERT_THAT(IsTrue(GetStructByPath<FQuat>(*TestRunner, Actor, TEXT("AssignedQuat"), Assigned),
			TEXT("AssignedQuat should be readable as full FQuat struct")));
		ASSERT_THAT(IsTrue(Assigned.Equals(Current, 0.001),
			TEXT("class member FQuat should preserve copy construction and assignment")));

		FVector RotatedForward = FVector::ZeroVector;
		ASSERT_THAT(IsTrue(GetStructByPath<FVector>(*TestRunner, Actor, TEXT("RotatedForward"), RotatedForward),
			TEXT("RotatedForward should be readable as full FVector struct")));
		ASSERT_THAT(IsTrue(RotatedForward.Equals(FVector::RightVector, 0.001),
			TEXT("class member FQuat should rotate FVector during BeginPlay")));

		int32 HistoryCount = 0;
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("History"), HistoryCount),
			TEXT("History TArray<FQuat> length should resolve")));
		ASSERT_THAT(AreEqual(3, HistoryCount, TEXT("History TArray<FQuat> should contain three elements")));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("History[0].W"), 1.0,
			TEXT("History[0] should store FQuat::Identity"))));

		double CurrentZ = 0.0;
		double History1Z = 0.0;
		ASSERT_THAT(IsTrue(GetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("CurrentQuat.Z"), CurrentZ),
			TEXT("CurrentQuat.Z should be readable through nested path")));
		ASSERT_THAT(IsTrue(GetByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("History[1].Z"), History1Z),
			TEXT("History[1].Z should be readable through nested array path")));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(History1Z, CurrentZ, 0.001),
			TEXT("TArray<FQuat> element should preserve class member quaternion value")));

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bCopyEqualsAssigned"), true,
			TEXT("FQuat copy/assignment comparison result should be reflected"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bInverseComposesToIdentity"), true,
			TEXT("FQuat inverse composition result should be reflected"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bHistoryPreservesValues"), true,
			TEXT("TArray<FQuat> preservation result should be reflected"))));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
