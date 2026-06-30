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
private:
	static bool GetIntTransformMapValue(
		FAutomationTestBase& Test,
		UObject* Object,
		FStringView Path,
		const int32 Key,
		FTransform& OutValue)
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

		const UScriptStruct* ExpectedStruct = TBaseStructure<FTransform>::Get();
		if (!Test.TestTrue(
				*FString::Printf(TEXT("TMap value property at '%.*s' should be FTransform"), Path.Len(), Path.GetData()),
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
	// FTransform declaration defaults: reflected properties start at identity.
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

		// Non-identity FTransform declaration initializers currently materialize as identity on reflected properties.
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("CustomTransform.Translation.X"), 0.0, TEXT("FTransform custom declaration initializer Translation.X"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("CustomTransform.Translation.Y"), 0.0, TEXT("FTransform custom declaration initializer Translation.Y"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("CustomTransform.Translation.Z"), 0.0, TEXT("FTransform custom declaration initializer Translation.Z"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("CustomTransform.Scale3D.X"), 1.0, TEXT("FTransform custom declaration initializer Scale3D.X"));

		// No default (should be identity)
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("NoDefaultTransform.Translation.X"), 0.0, TEXT("FTransform no default Translation.X"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("NoDefaultTransform.Scale3D.X"), 1.0, TEXT("FTransform no default Scale3D.X"));

		// Full transform declaration initializer follows the same identity materialization boundary.
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("FullTransform.Translation.X"), 0.0, TEXT("FTransform full declaration initializer Translation.X"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("FullTransform.Translation.Y"), 0.0, TEXT("FTransform full declaration initializer Translation.Y"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("FullTransform.Translation.Z"), 0.0, TEXT("FTransform full declaration initializer Translation.Z"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("FullTransform.Scale3D.X"), 1.0, TEXT("FTransform full declaration initializer Scale3D.X"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("FullTransform.Scale3D.Y"), 1.0, TEXT("FTransform full declaration initializer Scale3D.Y"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("FullTransform.Scale3D.Z"), 1.0, TEXT("FTransform full declaration initializer Scale3D.Z"));
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
	// FTransform method access: SetLocation and SetScale3D.
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
				FTransform MyTransform;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					MyTransform.SetLocation(FVector(100, 200, 300));
					MyTransform.SetScale3D(FVector(5, 5, 5));
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

		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("MyTransform.Translation.X"), 100.0, TEXT("FTransform.SetLocation modified X"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("MyTransform.Translation.Y"), 200.0, TEXT("FTransform.SetLocation modified Y"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("MyTransform.Translation.Z"), 300.0, TEXT("FTransform.SetLocation modified Z"));

		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("MyTransform.Scale3D.X"), 5.0, TEXT("FTransform.SetScale3D modified X"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("MyTransform.Scale3D.Y"), 5.0, TEXT("FTransform.SetScale3D modified Y"));
		VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("MyTransform.Scale3D.Z"), 5.0, TEXT("FTransform.SetScale3D modified Z"));
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

			FTransform MapValue = FTransform::Identity;
			ASSERT_THAT(IsTrue(GetIntTransformMapValue(*TestRunner, Actor, TEXT("IntToTransformMap"), 1, MapValue)));
			ASSERT_THAT(IsNear(10.0, MapValue.GetTranslation().X, 0.001, TEXT("TMap<int,FTransform>[1].Translation.X")));

			ASSERT_THAT(IsTrue(GetIntTransformMapValue(*TestRunner, Actor, TEXT("IntToTransformMap"), 2, MapValue)));
			ASSERT_THAT(IsNear(20.0, MapValue.GetTranslation().Y, 0.001, TEXT("TMap<int,FTransform>[2].Translation.Y")));

			ASSERT_THAT(IsTrue(GetIntTransformMapValue(*TestRunner, Actor, TEXT("IntToTransformMap"), 3, MapValue)));
			ASSERT_THAT(IsNear(30.0, MapValue.GetTranslation().Z, 0.001, TEXT("TMap<int,FTransform>[3].Translation.Z")));
		}
	}

	TEST_METHOD(FTransformPropertySpecifierAndRuntimeFlow)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFTransformProperty_RuntimeFlow"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFTransformPropertyRuntimeFlow.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFTransformRuntimeFlowActor : AActor
			{
				UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Coverage|Transform", meta = (DisplayName = "Editable Transform"))
				FTransform EditableTransform;

				UPROPERTY()
				TArray<FTransform> ReflectedTransforms;

				UPROPERTY()
				TMap<int, FTransform> ReflectedTransformMap;

				UPROPERTY()
				FTransform PlainMemberSnapshot;

				UPROPERTY()
				FVector RotatedForward;

				UPROPERTY()
				bool bPlainMemberMatchesSnapshot = false;

				FTransform PlainMember;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					EditableTransform = FTransform(FRotator(0, 90, 0), FVector(10, 20, 30), FVector(2, 3, 4));
					PlainMember = FTransform(FRotator(0, 90, 0), FVector(3, 4, 5), FVector(1, 1, 1));
					PlainMemberSnapshot = PlainMember;
					RotatedForward = PlainMember.TransformVector(FVector::ForwardVector);

					ReflectedTransforms.Add(EditableTransform);
					ReflectedTransforms.Add(PlainMember);

					ReflectedTransformMap.Add(9, EditableTransform);
					ReflectedTransformMap.Add(11, PlainMember);

					bPlainMemberMatchesSnapshot = PlainMemberSnapshot.Equals(PlainMember, 0.001);
				}
			}
			)AS"),
			TEXT("ACoverageFTransformRuntimeFlowActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FTransform runtime-flow actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		const FProperty* EditableTransform = ScriptClass->FindPropertyByName(FName(TEXT("EditableTransform")));
		ASSERT_THAT(IsNotNull(EditableTransform, TEXT("Editable FTransform property should be reflected")));
		if (EditableTransform == nullptr)
		{
			return;
		}

		const FStructProperty* EditableStruct = CastField<const FStructProperty>(EditableTransform);
		ASSERT_THAT(IsNotNull(EditableStruct, TEXT("Editable FTransform should reflect as FStructProperty")));
		if (EditableStruct == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(TBaseStructure<FTransform>::Get(), EditableStruct->Struct, TEXT("Editable FTransform should use the native FTransform struct")));
		ASSERT_THAT(IsTrue(EditableTransform->HasAnyPropertyFlags(CPF_Edit), TEXT("EditAnywhere FTransform should set CPF_Edit")));
		ASSERT_THAT(IsTrue(EditableTransform->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("BlueprintReadOnly FTransform should set CPF_BlueprintVisible")));
		ASSERT_THAT(IsTrue(EditableTransform->HasAnyPropertyFlags(CPF_BlueprintReadOnly), TEXT("BlueprintReadOnly FTransform should set CPF_BlueprintReadOnly")));
		ASSERT_THAT(AreEqual(FString(TEXT("Coverage|Transform")), EditableTransform->GetMetaData(TEXT("Category")), TEXT("FTransform Category metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Editable Transform")), EditableTransform->GetMetaData(TEXT("DisplayName")), TEXT("FTransform DisplayName metadata should round-trip")));

		const FProperty* ArrayPropertyRaw = ScriptClass->FindPropertyByName(FName(TEXT("ReflectedTransforms")));
		ASSERT_THAT(IsNotNull(ArrayPropertyRaw, TEXT("TArray<FTransform> property should be reflected")));
		if (ArrayPropertyRaw == nullptr)
		{
			return;
		}
		const FArrayProperty* ArrayProperty = CastField<const FArrayProperty>(ArrayPropertyRaw);
		ASSERT_THAT(IsNotNull(ArrayProperty, TEXT("ReflectedTransforms should be an FArrayProperty")));
		if (ArrayProperty == nullptr)
		{
			return;
		}
		const FStructProperty* ArrayElementProperty = CastField<const FStructProperty>(ArrayProperty->Inner);
		ASSERT_THAT(IsNotNull(ArrayElementProperty, TEXT("TArray<FTransform> element should reflect as FStructProperty")));
		if (ArrayElementProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(TBaseStructure<FTransform>::Get(), ArrayElementProperty->Struct, TEXT("TArray<FTransform> element should use the native FTransform struct")));

		const FProperty* MapPropertyRaw = ScriptClass->FindPropertyByName(FName(TEXT("ReflectedTransformMap")));
		ASSERT_THAT(IsNotNull(MapPropertyRaw, TEXT("TMap<int, FTransform> property should be reflected")));
		if (MapPropertyRaw == nullptr)
		{
			return;
		}
		const FMapProperty* MapProperty = CastField<const FMapProperty>(MapPropertyRaw);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("ReflectedTransformMap should be an FMapProperty")));
		if (MapProperty == nullptr)
		{
			return;
		}
		const FStructProperty* MapValueProperty = CastField<const FStructProperty>(MapProperty->ValueProp);
		ASSERT_THAT(IsNotNull(MapValueProperty, TEXT("TMap<int, FTransform> value should reflect as FStructProperty")));
		if (MapValueProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(TBaseStructure<FTransform>::Get(), MapValueProperty->Struct, TEXT("TMap<int, FTransform> value should use the native FTransform struct")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FTransform runtime-flow actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		FTransform EditableValue = FTransform::Identity;
		ASSERT_THAT(IsTrue(GetStructByPath<FTransform>(*TestRunner, Actor, TEXT("EditableTransform"), EditableValue),
			TEXT("EditableTransform should be readable as full FTransform struct")));
		ASSERT_THAT(IsTrue(EditableValue.Equals(FTransform(FRotator(0, 90, 0), FVector(10, 20, 30), FVector(2, 3, 4)), 0.001),
			TEXT("Editable FTransform should preserve rotation, translation, and scale defaults")));

		FTransform Snapshot = FTransform::Identity;
		ASSERT_THAT(IsTrue(GetStructByPath<FTransform>(*TestRunner, Actor, TEXT("PlainMemberSnapshot"), Snapshot),
			TEXT("PlainMemberSnapshot should be readable as full FTransform struct")));
		ASSERT_THAT(IsTrue(Snapshot.Equals(FTransform(FRotator(0, 90, 0), FVector(3, 4, 5), FVector(1, 1, 1)), 0.001),
			TEXT("plain class member FTransform should copy into reflected snapshot during BeginPlay")));

		FVector Rotated = FVector::ZeroVector;
		ASSERT_THAT(IsTrue(GetStructByPath<FVector>(*TestRunner, Actor, TEXT("RotatedForward"), Rotated),
			TEXT("RotatedForward should be readable as full FVector struct")));
		ASSERT_THAT(IsTrue(Rotated.Equals(FVector::RightVector, 0.001),
			TEXT("plain class member FTransform should transform vectors during BeginPlay")));

		int32 ArrayCount = 0;
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("ReflectedTransforms"), ArrayCount),
			TEXT("ReflectedTransforms TArray<FTransform> length should resolve")));
		ASSERT_THAT(AreEqual(2, ArrayCount, TEXT("ReflectedTransforms should contain BeginPlay values")));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("ReflectedTransforms[0].Translation.X"), 10.0,
			TEXT("TArray<FTransform> should preserve editable transform translation"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("ReflectedTransforms[1].Translation.Z"), 5.0,
			TEXT("TArray<FTransform> should preserve plain member snapshot translation"))));

		int32 MapCount = 0;
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("ReflectedTransformMap"), MapCount),
			TEXT("ReflectedTransformMap TMap<int, FTransform> length should resolve")));
		ASSERT_THAT(AreEqual(2, MapCount, TEXT("ReflectedTransformMap should contain BeginPlay values")));
		FTransform MapValue = FTransform::Identity;
		ASSERT_THAT(IsTrue(GetIntTransformMapValue(*TestRunner, Actor, TEXT("ReflectedTransformMap"), 9, MapValue),
			TEXT("ReflectedTransformMap should contain editable transform value")));
		ASSERT_THAT(IsNear(3.0, MapValue.GetScale3D().Y, 0.001,
			TEXT("TMap<int, FTransform> should preserve editable transform scale")));
		ASSERT_THAT(IsTrue(GetIntTransformMapValue(*TestRunner, Actor, TEXT("ReflectedTransformMap"), 11, MapValue),
			TEXT("ReflectedTransformMap should contain plain member transform value")));
		ASSERT_THAT(IsNear(4.0, MapValue.GetTranslation().Y, 0.001,
			TEXT("TMap<int, FTransform> should preserve plain member transform translation")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bPlainMemberMatchesSnapshot"), true,
			TEXT("plain class member comparison result should be reflected"))));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
