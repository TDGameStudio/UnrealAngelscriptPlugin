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

#if WITH_ANGELSCRIPT_UNITTESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageFRotatorPropertyTest,
	"Angelscript.TestModule.Coverage.FRotatorProperty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool GetIntRotatorMapValue(
		FAutomationTestBase& Test,
		UObject* Object,
		FStringView Path,
		const int32 Key,
		FRotator& OutValue)
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

		const UScriptStruct* ExpectedStruct = TBaseStructure<FRotator>::Get();
		if (!Test.TestTrue(
				*FString::Printf(TEXT("TMap value property at '%.*s' should be FRotator"), Path.Len(), Path.GetData()),
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

			FRotator MapValue = FRotator::ZeroRotator;
			ASSERT_THAT(IsTrue(GetIntRotatorMapValue(*TestRunner, Actor, TEXT("IntToRotatorMap"), 1, MapValue)));
			ASSERT_THAT(IsNear(45.0, MapValue.Pitch, 0.001, TEXT("TMap<int,FRotator>[1].Pitch")));

			ASSERT_THAT(IsTrue(GetIntRotatorMapValue(*TestRunner, Actor, TEXT("IntToRotatorMap"), 2, MapValue)));
			ASSERT_THAT(IsNear(90.0, MapValue.Yaw, 0.001, TEXT("TMap<int,FRotator>[2].Yaw")));

			ASSERT_THAT(IsTrue(GetIntRotatorMapValue(*TestRunner, Actor, TEXT("IntToRotatorMap"), 3, MapValue)));
			ASSERT_THAT(IsNear(45.0, MapValue.Roll, 0.001, TEXT("TMap<int,FRotator>[3].Roll")));
		}
	}

	TEST_METHOD(FRotatorPropertySpecifierFlags)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFRotatorProperty_SpecifierFlags"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFRotatorPropertySpecifierFlags.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFRotatorSpecifierActor : AActor
			{
				UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Coverage|Rotator", meta = (ClampMin = "-180.0", ClampMax = "180.0"))
				FRotator EditableRotation = FRotator(10, -20, 30);

				UPROPERTY()
				TArray<FRotator> ReflectedRotators;

				UPROPERTY()
				TMap<int, FRotator> ReflectedRotatorMap;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					ReflectedRotators.Add(EditableRotation);
					ReflectedRotators.Add(FRotator::ZeroRotator);

					ReflectedRotatorMap.Add(7, FRotator(70, 80, 90));
				}
			}
			)AS"),
			TEXT("ACoverageFRotatorSpecifierActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FRotator specifier actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		const FProperty* EditableRotation = ScriptClass->FindPropertyByName(FName(TEXT("EditableRotation")));
		ASSERT_THAT(IsNotNull(EditableRotation, TEXT("Editable FRotator property should be reflected")));
		if (EditableRotation == nullptr)
		{
			return;
		}

		const FStructProperty* EditableStruct = CastField<const FStructProperty>(EditableRotation);
		ASSERT_THAT(IsNotNull(EditableStruct, TEXT("Editable FRotator should reflect as FStructProperty")));
		if (EditableStruct == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(TBaseStructure<FRotator>::Get(), EditableStruct->Struct, TEXT("Editable FRotator should use the native FRotator struct")));
		ASSERT_THAT(IsTrue(EditableRotation->HasAnyPropertyFlags(CPF_Edit), TEXT("EditAnywhere FRotator should set CPF_Edit")));
		ASSERT_THAT(IsTrue(EditableRotation->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("BlueprintReadOnly FRotator should set CPF_BlueprintVisible")));
		ASSERT_THAT(IsTrue(EditableRotation->HasAnyPropertyFlags(CPF_BlueprintReadOnly), TEXT("BlueprintReadOnly FRotator should set CPF_BlueprintReadOnly")));
		ASSERT_THAT(AreEqual(FString(TEXT("Coverage|Rotator")), EditableRotation->GetMetaData(TEXT("Category")), TEXT("FRotator Category metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("-180.0")), EditableRotation->GetMetaData(TEXT("ClampMin")), TEXT("FRotator ClampMin metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("180.0")), EditableRotation->GetMetaData(TEXT("ClampMax")), TEXT("FRotator ClampMax metadata should round-trip")));

		const FProperty* ArrayPropertyRaw = ScriptClass->FindPropertyByName(FName(TEXT("ReflectedRotators")));
		ASSERT_THAT(IsNotNull(ArrayPropertyRaw, TEXT("TArray<FRotator> property should be reflected")));
		if (ArrayPropertyRaw == nullptr)
		{
			return;
		}
		const FArrayProperty* ArrayProperty = CastField<const FArrayProperty>(ArrayPropertyRaw);
		ASSERT_THAT(IsNotNull(ArrayProperty, TEXT("ReflectedRotators should be an FArrayProperty")));
		if (ArrayProperty == nullptr)
		{
			return;
		}
		const FStructProperty* ArrayElementProperty = CastField<const FStructProperty>(ArrayProperty->Inner);
		ASSERT_THAT(IsNotNull(ArrayElementProperty, TEXT("TArray<FRotator> element should reflect as FStructProperty")));
		if (ArrayElementProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(TBaseStructure<FRotator>::Get(), ArrayElementProperty->Struct, TEXT("TArray<FRotator> element should use the native FRotator struct")));

		const FProperty* MapPropertyRaw = ScriptClass->FindPropertyByName(FName(TEXT("ReflectedRotatorMap")));
		ASSERT_THAT(IsNotNull(MapPropertyRaw, TEXT("TMap<int, FRotator> property should be reflected")));
		if (MapPropertyRaw == nullptr)
		{
			return;
		}
		const FMapProperty* MapProperty = CastField<const FMapProperty>(MapPropertyRaw);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("ReflectedRotatorMap should be an FMapProperty")));
		if (MapProperty == nullptr)
		{
			return;
		}
		const FStructProperty* MapValueProperty = CastField<const FStructProperty>(MapProperty->ValueProp);
		ASSERT_THAT(IsNotNull(MapValueProperty, TEXT("TMap<int, FRotator> value should reflect as FStructProperty")));
		if (MapValueProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(TBaseStructure<FRotator>::Get(), MapValueProperty->Struct, TEXT("TMap<int, FRotator> value should use the native FRotator struct")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FRotator specifier actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("EditableRotation.Pitch"), 10.0, TEXT("Editable FRotator Pitch should read back"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("EditableRotation.Yaw"), -20.0, TEXT("Editable FRotator Yaw should read back"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("EditableRotation.Roll"), 30.0, TEXT("Editable FRotator Roll should read back"))));

		int32 ArrayCount = 0;
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("ReflectedRotators"), ArrayCount)));
		ASSERT_THAT(AreEqual(2, ArrayCount, TEXT("TArray<FRotator> reflected property should contain BeginPlay values")));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("ReflectedRotators[0].Yaw"), -20.0, TEXT("TArray<FRotator> should preserve first value"))));

		int32 MapCount = 0;
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("ReflectedRotatorMap"), MapCount)));
		ASSERT_THAT(AreEqual(1, MapCount, TEXT("TMap<int, FRotator> reflected property should contain BeginPlay value")));

		FRotator MapValue = FRotator::ZeroRotator;
		ASSERT_THAT(IsTrue(GetIntRotatorMapValue(*TestRunner, Actor, TEXT("ReflectedRotatorMap"), 7, MapValue)));
		ASSERT_THAT(IsNear(90.0, MapValue.Roll, 0.001, TEXT("TMap<int, FRotator> should preserve mapped value")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
