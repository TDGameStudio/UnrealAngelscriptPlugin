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

	TEST_METHOD(FVectorSpecifierAndSetProperties)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFVectorProperty_SpecifierAndSet"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFVectorPropertySpecifierAndSet.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFVectorSpecifierAndSetActor : AActor
			{
				UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Coverage|Vector", meta = (MakeEditWidget, ClampMin = "-100.0", ClampMax = "100.0"))
				FVector EditablePoint = FVector(10, -20, 30);

				UPROPERTY()
				TSet<FVector> VectorSet;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					VectorSet.Add(FVector::ForwardVector);
					VectorSet.Add(FVector::RightVector);
					VectorSet.Add(FVector::UpVector);
				}
			}
			)AS"),
			TEXT("ACoverageFVectorSpecifierAndSetActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FVector specifier/set actor class should compile")));

		const FProperty* EditablePoint = ScriptClass->FindPropertyByName(FName(TEXT("EditablePoint")));
		ASSERT_THAT(IsNotNull(EditablePoint, TEXT("Editable FVector property should be reflected")));
		if (EditablePoint != nullptr)
		{
			ASSERT_THAT(IsNotNull(CastField<const FStructProperty>(EditablePoint), TEXT("Editable FVector should reflect as FStructProperty")));
			ASSERT_THAT(IsTrue(EditablePoint->HasAnyPropertyFlags(CPF_Edit), TEXT("EditAnywhere FVector should set CPF_Edit")));
			ASSERT_THAT(IsTrue(EditablePoint->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("BlueprintReadOnly FVector should set CPF_BlueprintVisible")));
			ASSERT_THAT(IsTrue(EditablePoint->HasAnyPropertyFlags(CPF_BlueprintReadOnly), TEXT("BlueprintReadOnly FVector should set CPF_BlueprintReadOnly")));
			ASSERT_THAT(AreEqual(FString(TEXT("Coverage|Vector")), EditablePoint->GetMetaData(TEXT("Category")), TEXT("FVector Category metadata should round-trip")));
			ASSERT_THAT(AreEqual(FString(TEXT("-100.0")), EditablePoint->GetMetaData(TEXT("ClampMin")), TEXT("FVector ClampMin metadata should round-trip")));
			ASSERT_THAT(AreEqual(FString(TEXT("100.0")), EditablePoint->GetMetaData(TEXT("ClampMax")), TEXT("FVector ClampMax metadata should round-trip")));
			ASSERT_THAT(IsTrue(EditablePoint->HasMetaData(TEXT("MakeEditWidget")), TEXT("FVector MakeEditWidget metadata should be present")));
		}

		const FProperty* VectorSetProperty = ScriptClass->FindPropertyByName(FName(TEXT("VectorSet")));
		ASSERT_THAT(IsNotNull(VectorSetProperty, TEXT("TSet<FVector> property should be reflected")));
		const FSetProperty* SetProperty = CastField<const FSetProperty>(VectorSetProperty);
		ASSERT_THAT(IsNotNull(SetProperty, TEXT("VectorSet should be an FSetProperty")));
		if (SetProperty != nullptr)
		{
			const FStructProperty* ElementProperty = CastField<const FStructProperty>(SetProperty->ElementProp);
			ASSERT_THAT(IsNotNull(ElementProperty, TEXT("TSet<FVector> element should reflect as FStructProperty")));
			if (ElementProperty != nullptr)
			{
				ASSERT_THAT(AreEqual(TBaseStructure<FVector>::Get(), ElementProperty->Struct, TEXT("TSet<FVector> element should use FVector struct")));
			}
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FVector specifier/set actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("EditablePoint.X"), 10.0, TEXT("Editable FVector X should read back"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("EditablePoint.Y"), -20.0, TEXT("Editable FVector Y should read back"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("EditablePoint.Z"), 30.0, TEXT("Editable FVector Z should read back"))));

		int32 SetCount = 0;
		ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("VectorSet"), SetCount)));
		ASSERT_THAT(AreEqual(3, SetCount, TEXT("TSet<FVector> should have 3 entries")));
		ASSERT_THAT(IsTrue(SetContainsByPath<FVector>(*TestRunner, Actor, TEXT("VectorSet"), FVector::ForwardVector), TEXT("TSet<FVector> should contain ForwardVector")));
		ASSERT_THAT(IsTrue(SetContainsByPath<FVector>(*TestRunner, Actor, TEXT("VectorSet"), FVector::RightVector), TEXT("TSet<FVector> should contain RightVector")));
		ASSERT_THAT(IsTrue(SetContainsByPath<FVector>(*TestRunner, Actor, TEXT("VectorSet"), FVector::UpVector), TEXT("TSet<FVector> should contain UpVector")));
	}

	TEST_METHOD(FVectorScriptMemberAndLocalUsage)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFVectorProperty_ScriptMemberAndLocal"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFVectorPropertyScriptMemberAndLocal.as"),
			ASTEST_AS(R"AS(
			const FVector GlobalForward = FVector::ForwardVector;

			UCLASS()
			class ACoverageFVectorScriptMemberActor : AActor
			{
				FVector RawMember = FVector(4, 5, 6);

				UPROPERTY()
				FVector ReflectedMember = FVector::RightVector;

				UFUNCTION()
				int ReadLocalAndConstVectors()
				{
					FVector DefaultLocal;
					FVector CustomLocal = FVector(1, 2, 3);
					const FVector ConstLocal = FVector::UpVector;

					if (DefaultLocal != FVector::ZeroVector)
						return 1;
					if (CustomLocal != FVector(1, 2, 3))
						return 2;
					if (ConstLocal != FVector(0, 0, 1))
						return 3;
					if (GlobalForward != FVector(1, 0, 0))
						return 4;
					return 0;
				}

				UFUNCTION()
				int ReadRawAndReflectedMembers()
				{
					if (RawMember != FVector(4, 5, 6))
						return 10;
					if (ReflectedMember != FVector::RightVector)
						return 20;
					RawMember = RawMember + FVector(1, 1, 1);
					ReflectedMember = FVector(7, 8, 9);
					if (RawMember != FVector(5, 6, 7))
						return 30;
					if (ReflectedMember != FVector(7, 8, 9))
						return 40;
					return 0;
				}
			}
			)AS"),
			TEXT("ACoverageFVectorScriptMemberActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FVector script member/local actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FVector script member/local actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker LocalInvoker(*TestRunner, Actor, TEXT("ReadLocalAndConstVectors"));
		ASSERT_THAT(IsTrue(LocalInvoker.IsValid(), TEXT("ReadLocalAndConstVectors should be invokable")));
		if (!LocalInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(0, LocalInvoker.CallAndReturn<int32>(INDEX_NONE), TEXT("local/default/const/global FVector values should read correctly")));

		FFunctionInvoker MemberInvoker(*TestRunner, Actor, TEXT("ReadRawAndReflectedMembers"));
		ASSERT_THAT(IsTrue(MemberInvoker.IsValid(), TEXT("ReadRawAndReflectedMembers should be invokable")));
		if (!MemberInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(0, MemberInvoker.CallAndReturn<int32>(INDEX_NONE), TEXT("raw and UPROPERTY FVector members should read/write from AS")));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("ReflectedMember.X"), 7.0, TEXT("Reflected member X should reflect AS write"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("ReflectedMember.Y"), 8.0, TEXT("Reflected member Y should reflect AS write"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("ReflectedMember.Z"), 9.0, TEXT("Reflected member Z should reflect AS write"))));
	}

	TEST_METHOD(FVectorUFunctionPropertyRoundTrip)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFVectorProperty_UFunctionRoundTrip"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFVectorPropertyUFunctionRoundTrip.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFVectorUFunctionPropertyActor : AActor
			{
				UPROPERTY()
				FVector StoredVector = FVector(1, 2, 3);

				UFUNCTION()
				FVector StoreAndReturn(FVector Input)
				{
					StoredVector = Input + FVector(10, 20, 30);
					return StoredVector;
				}

				UFUNCTION()
				FVector UseDefaultParameter(FVector Input = FVector::UpVector)
				{
					StoredVector = Input;
					return StoredVector + FVector::ForwardVector;
				}

				UFUNCTION()
				FVector CallDefaultParameter()
				{
					return UseDefaultParameter();
				}
			}
			)AS"),
			TEXT("ACoverageFVectorUFunctionPropertyActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FVector UFUNCTION property actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FVector UFUNCTION property actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker StoreInvoker(*TestRunner, Actor, TEXT("StoreAndReturn"));
		ASSERT_THAT(IsTrue(StoreInvoker.IsValid(), TEXT("StoreAndReturn should be invokable")));
		if (!StoreInvoker.IsValid())
		{
			return;
		}
		StoreInvoker.AddParam<FVector>(FVector(-1, 1000, -3000));
		const FVector StoredResult = StoreInvoker.CallAndReturn<FVector>(FVector::ZeroVector);
		ASSERT_THAT(AreEqual(FVector(9, 1020, -2970), StoredResult, TEXT("UFUNCTION FVector return should include AS property write")));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("StoredVector.X"), 9.0, TEXT("StoredVector.X should reflect UFUNCTION write"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("StoredVector.Y"), 1020.0, TEXT("StoredVector.Y should reflect large UFUNCTION write"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("StoredVector.Z"), -2970.0, TEXT("StoredVector.Z should reflect negative UFUNCTION write"))));

		FFunctionInvoker DefaultInvoker(*TestRunner, Actor, TEXT("CallDefaultParameter"));
		ASSERT_THAT(IsTrue(DefaultInvoker.IsValid(), TEXT("CallDefaultParameter should be invokable")));
		if (!DefaultInvoker.IsValid())
		{
			return;
		}
		const FVector DefaultResult = DefaultInvoker.CallAndReturn<FVector>(FVector::ZeroVector);
		ASSERT_THAT(AreEqual(FVector(1, 0, 1), DefaultResult, TEXT("UFUNCTION FVector default parameter should execute")));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("StoredVector.X"), 0.0, TEXT("StoredVector.X should reflect default parameter write"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("StoredVector.Y"), 0.0, TEXT("StoredVector.Y should reflect default parameter write"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("StoredVector.Z"), 1.0, TEXT("StoredVector.Z should reflect default parameter write"))));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
