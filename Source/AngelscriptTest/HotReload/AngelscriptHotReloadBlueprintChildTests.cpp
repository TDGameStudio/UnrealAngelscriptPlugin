#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "ClassGenerator/ASClass.h"
#include "Components/ActorTestSpawner.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadBlueprintChildTests,
	"Angelscript.TestModule.HotReload.BlueprintChild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName EditSpecifierModuleName = FName(TEXT("HotReloadBlueprintChildEditSpecifier"));
	inline static const FString EditSpecifierFilename = FString(TEXT("HotReloadBlueprintChildEditSpecifier.as"));
	inline static const FName EditSpecifierParentClassName = FName(TEXT("AHotReloadBlueprintChildEditSpecifierParent"));

	inline static const FName SoftReloadModuleName = FName(TEXT("HotReloadBlueprintChildSoftReload"));
	inline static const FString SoftReloadFilename = FString(TEXT("HotReloadBlueprintChildSoftReload.as"));
	inline static const FName SoftReloadParentClassName = FName(TEXT("AHotReloadBlueprintChildSoftReloadParent"));

	inline static const FName SoftSequenceModuleName = FName(TEXT("HotReloadBlueprintChildSoftSequence"));
	inline static const FString SoftSequenceFilename = FString(TEXT("HotReloadBlueprintChildSoftSequence.as"));
	inline static const FName SoftSequenceParentClassName = FName(TEXT("AHotReloadBlueprintChildSoftSequenceParent"));

	inline static const FName StructuralModuleName = FName(TEXT("HotReloadBlueprintChildStructural"));
	inline static const FString StructuralFilename = FString(TEXT("HotReloadBlueprintChildStructural.as"));
	inline static const FName StructuralParentClassName = FName(TEXT("AHotReloadBlueprintChildStructuralParent"));

	struct FScopedTransientBlueprint
	{
		UBlueprint* Blueprint = nullptr;

		~FScopedTransientBlueprint()
		{
			Cleanup();
		}

		bool CreateAndCompile(FAutomationTestBase& Test, UClass* ParentClass, FStringView Suffix)
		{
			FNoDiscardAsserter LocalAssert(Test);
			if (!LocalAssert.IsNotNull(ParentClass, TEXT("HotReload Blueprint-child test should have a script parent class")))
			{
				return false;
			}

			const FString PackagePath = FString::Printf(
				TEXT("/Temp/AngelscriptHotReloadBlueprintChild_%.*s_%s"),
				Suffix.Len(),
				Suffix.GetData(),
				*FGuid::NewGuid().ToString(EGuidFormats::Digits));
			UPackage* BlueprintPackage = CreatePackage(*PackagePath);
			if (!LocalAssert.IsNotNull(BlueprintPackage, TEXT("HotReload Blueprint-child test should create a transient Blueprint package")))
			{
				return false;
			}

			BlueprintPackage->SetFlags(RF_Transient);
			const FName BlueprintName(*FPackageName::GetLongPackageAssetName(PackagePath));

			Blueprint = FKismetEditorUtilities::CreateBlueprint(
				ParentClass,
				BlueprintPackage,
				BlueprintName,
				BPTYPE_Normal,
				UBlueprint::StaticClass(),
				UBlueprintGeneratedClass::StaticClass(),
				TEXT("AngelscriptHotReloadBlueprintChildTest"));
			if (!LocalAssert.IsNotNull(Blueprint, TEXT("HotReload Blueprint-child test should create a transient Blueprint asset")))
			{
				return false;
			}

			return Compile(Test);
		}

		bool Compile(FAutomationTestBase& Test)
		{
			FNoDiscardAsserter LocalAssert(Test);
			if (!LocalAssert.IsNotNull(Blueprint, TEXT("HotReload Blueprint-child test should have a transient Blueprint asset to compile")))
			{
				return false;
			}

			FKismetEditorUtilities::CompileBlueprint(Blueprint);
			return LocalAssert.IsNotNull(Blueprint->GeneratedClass.Get(), TEXT("HotReload Blueprint-child test should compile a generated Blueprint class"));
		}

		UClass* GetGeneratedClass() const
		{
			return Blueprint != nullptr ? Blueprint->GeneratedClass.Get() : nullptr;
		}

		void Cleanup()
		{
			if (Blueprint == nullptr)
			{
				return;
			}

			if (UClass* BlueprintClass = Blueprint->GeneratedClass)
			{
				BlueprintClass->MarkAsGarbage();
			}

			if (UPackage* BlueprintPackage = Blueprint->GetOutermost())
			{
				BlueprintPackage->MarkAsGarbage();
			}

			Blueprint->MarkAsGarbage();
			CollectGarbage(RF_NoFlags, true);
			Blueprint = nullptr;
		}
	};

	struct FSoftReloadStep
	{
		const TCHAR* StepName = TEXT("");
		const FString* ScriptSource = nullptr;
		int32 ExpectedValue = 0;
	};

	static bool IsHandledReloadResult(const ECompileResult ReloadResult)
	{
		return ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled;
	}

	static void DiscardModule(FAngelscriptEngine& Engine, const FName ModuleName)
	{
		Engine.DiscardModule(*ModuleName.ToString());
	}

	static AActor* SpawnBlueprintActorAndBeginPlay(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		FActorTestSpawner& Spawner,
		UClass* BlueprintClass,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(BlueprintClass, *FString::Printf(TEXT("%s should have a Blueprint class"), Context)))
		{
			return nullptr;
		}

		AActor* Actor = AngelscriptFunctionalTestUtils::SpawnScriptActor(Test, Spawner, BlueprintClass);
		if (!LocalAssert.IsNotNull(Actor, *FString::Printf(TEXT("%s should spawn a Blueprint child actor"), Context)))
		{
			return nullptr;
		}

		AngelscriptFunctionalTestUtils::BeginPlayActor(Engine, *Actor);
		return Actor;
	}

	static bool InvokeGeneratedGetValue(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		UObject* Object,
		UClass* OwnerClass,
		const int32 ExpectedValue,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(Object, *FString::Printf(TEXT("%s should have a target object"), Context)))
		{
			return false;
		}

		if (!LocalAssert.IsNotNull(OwnerClass, *FString::Printf(TEXT("%s should have an owner class"), Context)))
		{
			return false;
		}

		UFunction* GetValueFunction = FindGeneratedFunction(OwnerClass, TEXT("GetValue"));
		if (!LocalAssert.IsNotNull(GetValueFunction, *FString::Printf(TEXT("%s should expose GetValue"), Context)))
		{
			return false;
		}

		int32 ActualValue = 0;
		if (!LocalAssert.IsTrue(
				ExecuteGeneratedIntEventOnGameThread(&Engine, Object, GetValueFunction, ActualValue),
				*FString::Printf(TEXT("%s should execute GetValue"), Context)))
		{
			return false;
		}

		return LocalAssert.AreEqual(
			ExpectedValue,
			ActualValue,
			*FString::Printf(TEXT("%s should observe the expected GetValue result"), Context));
	}

	static bool BlueprintParentChainResolvesTo(
		FAutomationTestBase& Test,
		UClass* BlueprintClass,
		UClass* ExpectedMostUpToDateClass,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		UASClass* ASParent = UASClass::GetFirstASClass(BlueprintClass);
		if (!LocalAssert.IsNotNull(ASParent, *FString::Printf(TEXT("%s should resolve an AS parent through the Blueprint parent chain"), Context)))
		{
			return false;
		}

		return LocalAssert.AreEqual(
			ExpectedMostUpToDateClass,
			ASParent->GetMostUpToDateClass(),
			*FString::Printf(TEXT("%s should resolve the expected most up-to-date AS parent"), Context));
	}

	static bool ReadIntProperty(
		FAutomationTestBase& Test,
		UObject* Object,
		FName PropertyName,
		int32& OutValue,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		const bool bRead = AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(Test, Object, PropertyName, OutValue);
		return LocalAssert.IsTrue(bRead, *FString::Printf(TEXT("%s should read property '%s'"), Context, *PropertyName.ToString()));
	}

	static bool ApplySoftReloadStep(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FSoftReloadStep& Step,
		UClass* InitialParentClass,
		UASClass* InitialASClass,
		UClass* BlueprintClass,
		AActor* BlueprintActor)
	{
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(Step.ScriptSource, *FString::Printf(TEXT("%s should provide script source"), Step.StepName)))
		{
			return false;
		}

		ECompileResult ReloadResult = ECompileResult::Error;
		if (!LocalAssert.IsTrue(
				CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, SoftSequenceModuleName, SoftSequenceFilename, *Step.ScriptSource, ReloadResult),
				*FString::Printf(TEXT("%s should compile on the soft reload path"), Step.StepName)))
		{
			return false;
		}

		if (!LocalAssert.AreEqual(
				ECompileResult::FullyHandled,
				ReloadResult,
				*FString::Printf(TEXT("%s should stay fully handled by soft reload"), Step.StepName)))
		{
			return false;
		}

		UClass* ReloadedParentClass = FindGeneratedClass(&Engine, SoftSequenceParentClassName);
		if (!LocalAssert.IsNotNull(ReloadedParentClass, *FString::Printf(TEXT("%s should resolve the reloaded AS parent class"), Step.StepName)))
		{
			return false;
		}

		bool bPassed = true;
		bPassed &= LocalAssert.AreEqual(
			InitialParentClass,
			ReloadedParentClass,
			*FString::Printf(TEXT("%s should preserve parent UClass identity"), Step.StepName));
		bPassed &= LocalAssert.AreEqual(
			InitialASClass,
			UASClass::GetFirstASClass(BlueprintClass),
			*FString::Printf(TEXT("%s should keep the Blueprint generated class on the same AS parent chain"), Step.StepName));
		bPassed &= LocalAssert.AreEqual(
			BlueprintClass,
			BlueprintActor->GetClass(),
			*FString::Printf(TEXT("%s should keep the live actor on the same Blueprint class"), Step.StepName));
		bPassed &= LocalAssert.IsTrue(
			BlueprintActor->HasActorBegunPlay(),
			*FString::Printf(TEXT("%s should keep the Blueprint actor in begun-play state"), Step.StepName));
		bPassed &= InvokeGeneratedGetValue(Test, Engine, BlueprintActor, ReloadedParentClass, Step.ExpectedValue, Step.StepName);
		return bPassed;
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

	TEST_METHOD(EditSpecifierReloadKeepsBlueprintChildInstanceAlive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			DiscardModule(Engine, EditSpecifierModuleName);
		};

		const FString ScriptV1 = ASTEST_AS(R"AS(
			UCLASS()
			class AHotReloadBlueprintChildEditSpecifierParent : AActor
			{
				UPROPERTY(NotEditable)
				int ExampleValue = 15;

				UFUNCTION()
				int GetValue()
				{
					return ExampleValue;
				}
			}
			)AS");

		const FString ScriptV2 = ASTEST_AS(R"AS(
			UCLASS()
			class AHotReloadBlueprintChildEditSpecifierParent : AActor
			{
				UPROPERTY(EditAnywhere)
				int ExampleValue = 15;

				UFUNCTION()
				int GetValue()
				{
					return ExampleValue + 1;
				}
			}
			)AS");

		UClass* ParentClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			EditSpecifierModuleName,
			EditSpecifierFilename,
			ScriptV1,
			EditSpecifierParentClassName);
		ASSERT_THAT(IsNotNull(ParentClass, TEXT("HotReload Blueprint-child test should compile the initial parent")));

		FScopedTransientBlueprint Blueprint;
		ASSERT_THAT(IsTrue(Blueprint.CreateAndCompile(*TestRunner, ParentClass, TEXT("EditSpecifier"))));

		UClass* BlueprintClass = Blueprint.GetGeneratedClass();
		ASSERT_THAT(IsNotNull(BlueprintClass, TEXT("HotReload Blueprint-child test should expose the generated Blueprint class")));
		ASSERT_THAT(IsTrue(BlueprintClass->IsChildOf(ParentClass), TEXT("HotReload Blueprint-child test should derive from the script parent")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* BlueprintActor = SpawnBlueprintActorAndBeginPlay(*TestRunner, Engine, Spawner, BlueprintClass, TEXT("HotReload Blueprint-child edit-specifier baseline"));
		ASSERT_THAT(IsNotNull(BlueprintActor, TEXT("HotReload Blueprint-child test should spawn a Blueprint child actor instance before reload")));

		FIntProperty* ExampleValueBeforeReload = FindFProperty<FIntProperty>(ParentClass, TEXT("ExampleValue"));
		ASSERT_THAT(IsNotNull(ExampleValueBeforeReload, TEXT("HotReload Blueprint-child test should expose ExampleValue before reload")));
		ASSERT_THAT(IsFalse(ExampleValueBeforeReload->HasAnyPropertyFlags(CPF_Edit), TEXT("NotEditable should clear CPF_Edit before reload")));

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::FullReload, EditSpecifierModuleName, EditSpecifierFilename, ScriptV2, ReloadResult),
			TEXT("HotReload Blueprint-child test should compile the EditAnywhere update")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(ReloadResult), TEXT("HotReload Blueprint-child test should finish on a handled reload path")));

		UClass* ReloadedParentClass = FindGeneratedClass(&Engine, EditSpecifierParentClassName);
		ASSERT_THAT(IsNotNull(ReloadedParentClass, TEXT("HotReload Blueprint-child test should resolve the parent class after reload")));

		FIntProperty* ExampleValueAfterReload = FindFProperty<FIntProperty>(ReloadedParentClass, TEXT("ExampleValue"));
		ASSERT_THAT(IsNotNull(ExampleValueAfterReload, TEXT("HotReload Blueprint-child test should keep ExampleValue after reload")));
		ASSERT_THAT(IsTrue(ExampleValueAfterReload->HasAnyPropertyFlags(CPF_Edit), TEXT("EditAnywhere should set CPF_Edit after reload")));

		ASSERT_THAT(IsNotNull(BlueprintActor, TEXT("HotReload Blueprint-child test should keep the pre-existing Blueprint actor pointer usable after reload")));
	}

	TEST_METHOD(SoftReloadKeepsBlueprintChildInstanceOnUpdatedParentBody)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			DiscardModule(Engine, SoftReloadModuleName);
		};

		const FString ScriptV1 = ASTEST_AS(R"AS(
			UCLASS()
			class AHotReloadBlueprintChildSoftReloadParent : AActor
			{
				UPROPERTY()
				int ExampleValue = 30;

				UFUNCTION()
				int GetValue()
				{
					return ExampleValue;
				}
			}
			)AS");

		const FString ScriptV2 = ASTEST_AS(R"AS(
			UCLASS()
			class AHotReloadBlueprintChildSoftReloadParent : AActor
			{
				UPROPERTY()
				int ExampleValue = 30;

				UFUNCTION()
				int GetValue()
				{
					return ExampleValue + 12;
				}
			}
			)AS");

		UClass* ParentClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			SoftReloadModuleName,
			SoftReloadFilename,
			ScriptV1,
			SoftReloadParentClassName);
		ASSERT_THAT(IsNotNull(ParentClass, TEXT("HotReload Blueprint-child soft test should compile the initial parent")));

		UASClass* ParentASClass = Cast<UASClass>(ParentClass);
		ASSERT_THAT(IsNotNull(ParentASClass, TEXT("HotReload Blueprint-child soft test should start from an AS parent class")));

		FScopedTransientBlueprint Blueprint;
		ASSERT_THAT(IsTrue(Blueprint.CreateAndCompile(*TestRunner, ParentClass, TEXT("SoftReload"))));

		UClass* BlueprintClass = Blueprint.GetGeneratedClass();
		ASSERT_THAT(IsNotNull(BlueprintClass, TEXT("HotReload Blueprint-child soft test should expose the generated Blueprint class")));
		ASSERT_THAT(IsTrue(BlueprintClass->IsChildOf(ParentClass), TEXT("HotReload Blueprint-child soft test should derive from the script parent")));
		ASSERT_THAT(IsNull(Cast<UASClass>(BlueprintClass), TEXT("Generated Blueprint child should not be a UASClass")));
		ASSERT_THAT(AreEqual(ParentASClass, UASClass::GetFirstASClass(BlueprintClass), TEXT("Generated Blueprint child should resolve its first AS class through the parent chain")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* BlueprintActor = SpawnBlueprintActorAndBeginPlay(*TestRunner, Engine, Spawner, BlueprintClass, TEXT("HotReload Blueprint-child soft baseline"));
		ASSERT_THAT(IsNotNull(BlueprintActor, TEXT("HotReload Blueprint-child soft test should spawn a Blueprint child actor instance before reload")));

		ASSERT_THAT(IsTrue(InvokeGeneratedGetValue(*TestRunner, Engine, BlueprintActor, ParentClass, 30, TEXT("HotReload Blueprint-child soft test before reload"))));

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, SoftReloadModuleName, SoftReloadFilename, ScriptV2, ReloadResult),
			TEXT("HotReload Blueprint-child soft test should compile the body-only update")));
		ASSERT_THAT(AreEqual(ECompileResult::FullyHandled, ReloadResult, TEXT("HotReload Blueprint-child soft test should stay on the pure soft reload path")));

		UClass* ReloadedParentClass = FindGeneratedClass(&Engine, SoftReloadParentClassName);
		ASSERT_THAT(IsNotNull(ReloadedParentClass, TEXT("HotReload Blueprint-child soft test should resolve the parent class after reload")));
		ASSERT_THAT(AreEqual(ParentClass, ReloadedParentClass, TEXT("HotReload Blueprint-child soft test should keep the same parent UClass on soft reload")));
		ASSERT_THAT(AreEqual(ParentASClass, UASClass::GetFirstASClass(BlueprintClass), TEXT("Generated Blueprint child should still resolve the AS parent after soft reload")));
		ASSERT_THAT(AreEqual(BlueprintClass, BlueprintActor->GetClass(), TEXT("Existing Blueprint child actor should keep its generated class after soft reload")));

		ASSERT_THAT(IsTrue(InvokeGeneratedGetValue(*TestRunner, Engine, BlueprintActor, ReloadedParentClass, 42, TEXT("HotReload Blueprint-child soft test after reload"))));
	}

	TEST_METHOD(MultipleSoftReloadsUpdateRunningBlueprintChildFunction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			DiscardModule(Engine, SoftSequenceModuleName);
		};

		const FString ScriptV1 = ASTEST_AS(R"AS(
			UCLASS()
			class AHotReloadBlueprintChildSoftSequenceParent : AActor
			{
				UPROPERTY()
				int Value = 10;

				UPROPERTY()
				int BeginPlayCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					BeginPlayCount += 1;
				}

				int ComputeBonus()
				{
					return 0;
				}

				UFUNCTION()
				int GetValue()
				{
					return Value + ComputeBonus();
				}
			}
			)AS");

		const FString ScriptV2 = ASTEST_AS(R"AS(
			UCLASS()
			class AHotReloadBlueprintChildSoftSequenceParent : AActor
			{
				UPROPERTY()
				int Value = 10;

				UPROPERTY()
				int BeginPlayCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					BeginPlayCount += 1;
				}

				int ComputeBonus()
				{
					return 1;
				}

				UFUNCTION()
				int GetValue()
				{
					return Value + ComputeBonus();
				}
			}
			)AS");

		const FString ScriptV3 = ASTEST_AS(R"AS(
			UCLASS()
			class AHotReloadBlueprintChildSoftSequenceParent : AActor
			{
				UPROPERTY()
				int Value = 10;

				UPROPERTY()
				int BeginPlayCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					BeginPlayCount += 1;
				}

				int ComputeBonus()
				{
					return BeginPlayCount + 20;
				}

				UFUNCTION()
				int GetValue()
				{
					return Value + ComputeBonus();
				}
			}
			)AS");

		const FString ScriptV4 = ASTEST_AS(R"AS(
			UCLASS()
			class AHotReloadBlueprintChildSoftSequenceParent : AActor
			{
				UPROPERTY()
				int Value = 10;

				UPROPERTY()
				int BeginPlayCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					BeginPlayCount += 1;
				}

				int ComputeBonus()
				{
					return 30;
				}

				UFUNCTION()
				int GetValue()
				{
					return Value + ComputeBonus();
				}
			}
			)AS");

		UClass* ParentClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			SoftSequenceModuleName,
			SoftSequenceFilename,
			ScriptV1,
			SoftSequenceParentClassName);
		ASSERT_THAT(IsNotNull(ParentClass, TEXT("HotReload Blueprint-child soft sequence should compile the initial parent")));

		UASClass* ParentASClass = Cast<UASClass>(ParentClass);
		ASSERT_THAT(IsNotNull(ParentASClass, TEXT("HotReload Blueprint-child soft sequence should start from an AS parent class")));

		FScopedTransientBlueprint Blueprint;
		ASSERT_THAT(IsTrue(Blueprint.CreateAndCompile(*TestRunner, ParentClass, TEXT("SoftSequence"))));

		UClass* BlueprintClass = Blueprint.GetGeneratedClass();
		ASSERT_THAT(IsNotNull(BlueprintClass, TEXT("HotReload Blueprint-child soft sequence should expose a Blueprint generated class")));
		ASSERT_THAT(IsTrue(BlueprintClass->IsChildOf(ParentClass), TEXT("HotReload Blueprint-child soft sequence Blueprint class should inherit from the AS parent")));
		ASSERT_THAT(IsNull(Cast<UASClass>(BlueprintClass), TEXT("HotReload Blueprint-child soft sequence Blueprint generated class should not be a UASClass")));
		ASSERT_THAT(AreEqual(ParentASClass, UASClass::GetFirstASClass(BlueprintClass), TEXT("HotReload Blueprint-child soft sequence should resolve AS parent through Blueprint parent chain")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* BlueprintActor = SpawnBlueprintActorAndBeginPlay(*TestRunner, Engine, Spawner, BlueprintClass, TEXT("HotReload Blueprint-child soft sequence baseline"));
		ASSERT_THAT(IsNotNull(BlueprintActor, TEXT("HotReload Blueprint-child soft sequence should spawn a Blueprint child actor")));

		Spawner.GetWorld().Tick(ELevelTick::LEVELTICK_All, 0.016f);

		ASSERT_THAT(IsTrue(InvokeGeneratedGetValue(*TestRunner, Engine, BlueprintActor, ParentClass, 10, TEXT("HotReload Blueprint-child soft sequence baseline"))));

		int32 BeginPlayCountBeforeReload = 0;
		ASSERT_THAT(IsTrue(ReadIntProperty(*TestRunner, BlueprintActor, TEXT("BeginPlayCount"), BeginPlayCountBeforeReload, TEXT("HotReload Blueprint-child soft sequence baseline"))));
		ASSERT_THAT(AreEqual(1, BeginPlayCountBeforeReload, TEXT("HotReload Blueprint-child soft sequence should run BeginPlay once before reload")));

		const FSoftReloadStep Steps[] =
		{
			{ TEXT("HotReload Blueprint-child soft sequence step V2 helper body"), &ScriptV2, 11 },
			{ TEXT("HotReload Blueprint-child soft sequence step V3 helper reads runtime state"), &ScriptV3, 31 },
			{ TEXT("HotReload Blueprint-child soft sequence step V4 helper body changes again"), &ScriptV4, 40 },
		};

		for (const FSoftReloadStep& Step : Steps)
		{
			ASSERT_THAT(IsTrue(ApplySoftReloadStep(
				*TestRunner,
				Engine,
				Step,
				ParentClass,
				ParentASClass,
				BlueprintClass,
				BlueprintActor)));

			int32 BeginPlayCountAfterReload = 0;
			ASSERT_THAT(IsTrue(ReadIntProperty(*TestRunner, BlueprintActor, TEXT("BeginPlayCount"), BeginPlayCountAfterReload, Step.StepName)));
			ASSERT_THAT(AreEqual(1, BeginPlayCountAfterReload, TEXT("HotReload Blueprint-child soft sequence should not replay BeginPlay during soft reload steps")));
		}
	}

	TEST_METHOD(StructuralReloadKeepsExistingAndFreshBlueprintChildDefaults)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			DiscardModule(Engine, StructuralModuleName);
		};

		const FString ScriptV1 = ASTEST_AS(R"AS(
			UCLASS()
			class AHotReloadBlueprintChildStructuralParent : AActor
			{
				UPROPERTY(NotEditable)
				int Value = 10;

				UFUNCTION()
				int GetValue()
				{
					return Value;
				}
			}
			)AS");

		const FString ScriptV2 = ASTEST_AS(R"AS(
			UCLASS()
			class AHotReloadBlueprintChildStructuralParent : AActor
			{
				UPROPERTY(NotEditable)
				int Value = 10;

				UPROPERTY()
				int Bonus = 5;

				UFUNCTION()
				int GetValue()
				{
					return Value + Bonus;
				}
			}
			)AS");

		const FString ScriptV3 = ASTEST_AS(R"AS(
			UCLASS()
			class AHotReloadBlueprintChildStructuralParent : AActor
			{
				UPROPERTY(EditAnywhere)
				int Value = 20;

				UPROPERTY()
				int Bonus = 7;

				UFUNCTION()
				int GetValue()
				{
					return Value + Bonus;
				}
			}
			)AS");

		UClass* InitialParentClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			StructuralModuleName,
			StructuralFilename,
			ScriptV1,
			StructuralParentClassName);
		ASSERT_THAT(IsNotNull(InitialParentClass, TEXT("HotReload Blueprint-child structural test should compile the initial parent")));

		UASClass* InitialASClass = Cast<UASClass>(InitialParentClass);
		ASSERT_THAT(IsNotNull(InitialASClass, TEXT("HotReload Blueprint-child structural test should start from an AS parent class")));

		FIntProperty* InitialValueProperty = FindFProperty<FIntProperty>(InitialParentClass, TEXT("Value"));
		ASSERT_THAT(IsNotNull(InitialValueProperty, TEXT("HotReload Blueprint-child structural test should expose Value before reload")));
		ASSERT_THAT(IsFalse(InitialValueProperty->HasAnyPropertyFlags(CPF_Edit), TEXT("Value should start as NotEditable before structural reload")));

		FScopedTransientBlueprint Blueprint;
		ASSERT_THAT(IsTrue(Blueprint.CreateAndCompile(*TestRunner, InitialParentClass, TEXT("Structural"))));

		UClass* InitialBlueprintClass = Blueprint.GetGeneratedClass();
		ASSERT_THAT(IsNotNull(InitialBlueprintClass, TEXT("HotReload Blueprint-child structural test should expose an initial Blueprint generated class")));
		ASSERT_THAT(IsTrue(InitialBlueprintClass->IsChildOf(InitialParentClass), TEXT("HotReload Blueprint-child structural test Blueprint should inherit from initial AS parent")));
		ASSERT_THAT(AreEqual(InitialASClass, UASClass::GetFirstASClass(InitialBlueprintClass), TEXT("HotReload Blueprint-child structural test should resolve initial AS parent through Blueprint parent chain")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* InitialBlueprintActor = SpawnBlueprintActorAndBeginPlay(*TestRunner, Engine, Spawner, InitialBlueprintClass, TEXT("HotReload Blueprint-child structural baseline"));
		ASSERT_THAT(IsNotNull(InitialBlueprintActor, TEXT("HotReload Blueprint-child structural test should spawn the initial Blueprint child actor")));
		ASSERT_THAT(IsTrue(InvokeGeneratedGetValue(*TestRunner, Engine, InitialBlueprintActor, InitialParentClass, 10, TEXT("HotReload Blueprint-child structural baseline"))));

		ECompileResult AddPropertyReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::FullReload, StructuralModuleName, StructuralFilename, ScriptV2, AddPropertyReloadResult),
			TEXT("HotReload Blueprint-child structural test should compile the added-property full reload")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(AddPropertyReloadResult), TEXT("HotReload Blueprint-child structural test added-property reload should finish on a handled path")));

		UClass* AddedPropertyParentClass = FindGeneratedClass(&Engine, StructuralParentClassName);
		ASSERT_THAT(IsNotNull(AddedPropertyParentClass, TEXT("HotReload Blueprint-child structural test should resolve parent class after added-property reload")));
		ASSERT_THAT(IsTrue(AddedPropertyParentClass != InitialParentClass, TEXT("HotReload Blueprint-child structural test should replace parent UClass on structural full reload")));
		ASSERT_THAT(AreEqual(AddedPropertyParentClass, InitialASClass->GetMostUpToDateClass(), TEXT("HotReload Blueprint-child structural test should resolve added-property class as most up to date")));

		ASSERT_THAT(IsTrue(Blueprint.Compile(*TestRunner), TEXT("HotReload Blueprint-child structural test should recompile Blueprint after added-property reload")));

		UClass* AddedPropertyBlueprintClass = Blueprint.GetGeneratedClass();
		ASSERT_THAT(IsNotNull(AddedPropertyBlueprintClass, TEXT("HotReload Blueprint-child structural test should expose Blueprint class after added-property reload")));
		ASSERT_THAT(IsTrue(BlueprintParentChainResolvesTo(
			*TestRunner,
			AddedPropertyBlueprintClass,
			AddedPropertyParentClass,
			TEXT("HotReload Blueprint-child structural test added-property Blueprint"))));

		AActor* AddedPropertyBlueprintActor = SpawnBlueprintActorAndBeginPlay(*TestRunner, Engine, Spawner, AddedPropertyBlueprintClass, TEXT("HotReload Blueprint-child structural test added-property Blueprint"));
		ASSERT_THAT(IsNotNull(AddedPropertyBlueprintActor, TEXT("HotReload Blueprint-child structural test should spawn Blueprint actor after added-property reload")));
		ASSERT_THAT(IsTrue(InvokeGeneratedGetValue(*TestRunner, Engine, AddedPropertyBlueprintActor, AddedPropertyParentClass, 15, TEXT("HotReload Blueprint-child structural test added-property version"))));

		FIntProperty* AddedBonusProperty = FindFProperty<FIntProperty>(AddedPropertyParentClass, TEXT("Bonus"));
		ASSERT_THAT(IsNotNull(AddedBonusProperty, TEXT("HotReload Blueprint-child structural test should expose Bonus after added-property reload")));

		ECompileResult SpecifierReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::FullReload, StructuralModuleName, StructuralFilename, ScriptV3, SpecifierReloadResult),
			TEXT("HotReload Blueprint-child structural test should compile the specifier/default full reload")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(SpecifierReloadResult), TEXT("HotReload Blueprint-child structural test specifier/default reload should finish on a handled path")));

		UClass* SpecifierParentClass = FindGeneratedClass(&Engine, StructuralParentClassName);
		ASSERT_THAT(IsNotNull(SpecifierParentClass, TEXT("HotReload Blueprint-child structural test should resolve parent class after specifier/default reload")));
		ASSERT_THAT(IsTrue(SpecifierParentClass != AddedPropertyParentClass, TEXT("HotReload Blueprint-child structural test should replace parent UClass after second structural full reload")));
		ASSERT_THAT(AreEqual(SpecifierParentClass, InitialASClass->GetMostUpToDateClass(), TEXT("HotReload Blueprint-child structural test should resolve final class as most up to date from initial AS class")));

		ASSERT_THAT(IsTrue(Blueprint.Compile(*TestRunner), TEXT("HotReload Blueprint-child structural test should recompile Blueprint after specifier/default reload")));

		UClass* SpecifierBlueprintClass = Blueprint.GetGeneratedClass();
		ASSERT_THAT(IsNotNull(SpecifierBlueprintClass, TEXT("HotReload Blueprint-child structural test should expose Blueprint class after specifier/default reload")));
		ASSERT_THAT(IsTrue(BlueprintParentChainResolvesTo(
			*TestRunner,
			SpecifierBlueprintClass,
			SpecifierParentClass,
			TEXT("HotReload Blueprint-child structural test final Blueprint"))));

		FIntProperty* FinalValueProperty = FindFProperty<FIntProperty>(SpecifierParentClass, TEXT("Value"));
		ASSERT_THAT(IsNotNull(FinalValueProperty, TEXT("HotReload Blueprint-child structural test should expose final Value property")));
		ASSERT_THAT(IsTrue(FinalValueProperty->HasAnyPropertyFlags(CPF_Edit), TEXT("HotReload Blueprint-child structural test should expose Value as EditAnywhere after final reload")));

		AActor* FinalBlueprintActor = SpawnBlueprintActorAndBeginPlay(*TestRunner, Engine, Spawner, SpecifierBlueprintClass, TEXT("HotReload Blueprint-child structural test preserved Blueprint child"));
		ASSERT_THAT(IsNotNull(FinalBlueprintActor, TEXT("HotReload Blueprint-child structural test should spawn Blueprint actor after final reload")));
		ASSERT_THAT(IsTrue(InvokeGeneratedGetValue(*TestRunner, Engine, FinalBlueprintActor, SpecifierParentClass, 15, TEXT("HotReload Blueprint-child structural test preserved Blueprint child"))));

		int32 FinalValue = 0;
		ASSERT_THAT(IsTrue(ReadIntProperty(*TestRunner, FinalBlueprintActor, TEXT("Value"), FinalValue, TEXT("HotReload Blueprint-child structural test preserved Blueprint child"))));
		ASSERT_THAT(AreEqual(10, FinalValue, TEXT("HotReload Blueprint-child structural test existing Blueprint child should preserve its prior Value default")));

		int32 FinalBonus = 0;
		ASSERT_THAT(IsTrue(ReadIntProperty(*TestRunner, FinalBlueprintActor, TEXT("Bonus"), FinalBonus, TEXT("HotReload Blueprint-child structural test preserved Blueprint child"))));
		ASSERT_THAT(AreEqual(5, FinalBonus, TEXT("HotReload Blueprint-child structural test existing Blueprint child should preserve its prior Bonus default")));

		AActor* FinalParentActor = SpawnBlueprintActorAndBeginPlay(*TestRunner, Engine, Spawner, SpecifierParentClass, TEXT("HotReload Blueprint-child structural test final AS parent"));
		ASSERT_THAT(IsNotNull(FinalParentActor, TEXT("HotReload Blueprint-child structural test should spawn the final AS parent actor")));
		ASSERT_THAT(IsTrue(InvokeGeneratedGetValue(*TestRunner, Engine, FinalParentActor, SpecifierParentClass, 27, TEXT("HotReload Blueprint-child structural test final AS parent"))));

		FScopedTransientBlueprint FreshBlueprint;
		ASSERT_THAT(IsTrue(FreshBlueprint.CreateAndCompile(*TestRunner, SpecifierParentClass, TEXT("StructuralFresh"))));

		UClass* FreshBlueprintClass = FreshBlueprint.GetGeneratedClass();
		ASSERT_THAT(IsNotNull(FreshBlueprintClass, TEXT("HotReload Blueprint-child structural test should expose a fresh Blueprint class after final reload")));
		ASSERT_THAT(IsTrue(FreshBlueprintClass->IsChildOf(SpecifierParentClass), TEXT("HotReload Blueprint-child structural test fresh Blueprint should inherit from final AS parent")));

		AActor* FreshBlueprintActor = SpawnBlueprintActorAndBeginPlay(*TestRunner, Engine, Spawner, FreshBlueprintClass, TEXT("HotReload Blueprint-child structural test fresh Blueprint final version"));
		ASSERT_THAT(IsNotNull(FreshBlueprintActor, TEXT("HotReload Blueprint-child structural test should spawn a fresh Blueprint child actor after final reload")));
		ASSERT_THAT(IsTrue(InvokeGeneratedGetValue(*TestRunner, Engine, FreshBlueprintActor, SpecifierParentClass, 27, TEXT("HotReload Blueprint-child structural test fresh Blueprint final version"))));

		int32 FreshValue = 0;
		ASSERT_THAT(IsTrue(ReadIntProperty(*TestRunner, FreshBlueprintActor, TEXT("Value"), FreshValue, TEXT("HotReload Blueprint-child structural test fresh Blueprint final version"))));
		ASSERT_THAT(AreEqual(20, FreshValue, TEXT("HotReload Blueprint-child structural test fresh Blueprint actor should inherit updated Value default")));

		int32 FreshBonus = 0;
		ASSERT_THAT(IsTrue(ReadIntProperty(*TestRunner, FreshBlueprintActor, TEXT("Bonus"), FreshBonus, TEXT("HotReload Blueprint-child structural test fresh Blueprint final version"))));
		ASSERT_THAT(AreEqual(7, FreshBonus, TEXT("HotReload Blueprint-child structural test fresh Blueprint actor should inherit updated Bonus default")));
	}
};
