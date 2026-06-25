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

// Test Layer: UE Functional
#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptHotReloadSequenceTest
{
	static const FName SoftSequenceModuleName(TEXT("HotReloadSequenceSoftRuntime"));
	static const FString SoftSequenceFilename(TEXT("HotReloadSequenceSoftRuntime.as"));
	static const FName SoftSequenceClassName(TEXT("AHotReloadSequenceSoftRuntimeParent"));

	static const FName StructuralSequenceModuleName(TEXT("HotReloadSequenceStructuralBlueprint"));
	static const FString StructuralSequenceFilename(TEXT("HotReloadSequenceStructuralBlueprint.as"));
	static const FName StructuralSequenceClassName(TEXT("AHotReloadSequenceStructuralBlueprintParent"));

	struct FScopedTransientBlueprint
	{
		UBlueprint* Blueprint = nullptr;

		~FScopedTransientBlueprint()
		{
			Cleanup();
		}

		bool CreateAndCompile(FAutomationTestBase& Test, UClass* ParentClass, FStringView Suffix)
		{
			if (!Test.TestNotNull(TEXT("HotReload sequence should have a valid script parent class"), ParentClass))
			{
				return false;
			}

			const FString PackagePath = FString::Printf(
				TEXT("/Temp/AngelscriptHotReloadSequence_%.*s_%s"),
				Suffix.Len(),
				Suffix.GetData(),
				*FGuid::NewGuid().ToString(EGuidFormats::Digits));
			UPackage* BlueprintPackage = CreatePackage(*PackagePath);
			if (!Test.TestNotNull(TEXT("HotReload sequence should create a transient Blueprint package"), BlueprintPackage))
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
				TEXT("AngelscriptHotReloadSequenceTest"));
			if (!Test.TestNotNull(TEXT("HotReload sequence should create a transient Blueprint asset"), Blueprint))
			{
				return false;
			}

			return Compile(Test);
		}

		bool Compile(FAutomationTestBase& Test)
		{
			if (!Test.TestNotNull(TEXT("HotReload sequence should have a transient Blueprint asset to compile"), Blueprint))
			{
				return false;
			}

			FKismetEditorUtilities::CompileBlueprint(Blueprint);
			return Test.TestNotNull(TEXT("HotReload sequence should compile a generated Blueprint class"), Blueprint->GeneratedClass.Get());
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
		const TCHAR* ScriptSource = TEXT("");
		int32 ExpectedValue = 0;
	};

	static bool IsHandledReloadResult(const ECompileResult ReloadResult)
	{
		return ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled;
	}

	static AActor* SpawnBlueprintActorAndBeginPlay(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		FActorTestSpawner& Spawner,
		UClass* BlueprintClass)
	{
		AActor* Actor = AngelscriptFunctionalTestUtils::SpawnScriptActor(Test, Spawner, BlueprintClass);
		if (Actor != nullptr)
		{
			AngelscriptFunctionalTestUtils::BeginPlayActor(Engine, *Actor);
		}
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
		UFunction* GetValueFunction = FindGeneratedFunction(OwnerClass, TEXT("GetValue"));
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s should expose GetValue"), Context), GetValueFunction))
		{
			return false;
		}

		int32 ActualValue = 0;
		if (!Test.TestTrue(
				*FString::Printf(TEXT("%s should execute GetValue"), Context),
				ExecuteGeneratedIntEventOnGameThread(&Engine, Object, GetValueFunction, ActualValue)))
		{
			return false;
		}

		return Test.TestEqual(
			*FString::Printf(TEXT("%s should observe the expected GetValue result"), Context),
			ActualValue,
			ExpectedValue);
	}

	static bool BlueprintParentChainResolvesTo(
		FAutomationTestBase& Test,
		UClass* BlueprintClass,
		UClass* ExpectedMostUpToDateClass,
		const TCHAR* Context)
	{
		UASClass* ASParent = UASClass::GetFirstASClass(BlueprintClass);
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s should resolve an AS parent through the Blueprint parent chain"), Context), ASParent))
		{
			return false;
		}

		return Test.TestEqual(
			*FString::Printf(TEXT("%s should resolve the expected most up-to-date AS parent"), Context),
			ASParent->GetMostUpToDateClass(),
			ExpectedMostUpToDateClass);
	}

	static bool ReadIntProperty(
		FAutomationTestBase& Test,
		UObject* Object,
		FName PropertyName,
		int32& OutValue,
		const TCHAR* Context)
	{
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(Test, Object, PropertyName, OutValue))
		{
			Test.AddError(FString::Printf(TEXT("%s should read property '%s'"), Context, *PropertyName.ToString()));
			return false;
		}

		return true;
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
		ECompileResult ReloadResult = ECompileResult::Error;
		if (!Test.TestTrue(
				*FString::Printf(TEXT("%s should compile on the soft reload path"), Step.StepName),
				CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, SoftSequenceModuleName, SoftSequenceFilename, Step.ScriptSource, ReloadResult)))
		{
			return false;
		}

		if (!Test.TestEqual(
				*FString::Printf(TEXT("%s should stay fully handled by soft reload"), Step.StepName),
				ReloadResult,
				ECompileResult::FullyHandled))
		{
			return false;
		}

		UClass* ReloadedParentClass = FindGeneratedClass(&Engine, SoftSequenceClassName);
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s should resolve the reloaded AS parent class"), Step.StepName), ReloadedParentClass))
		{
			return false;
		}

		bool bPassed = true;
		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("%s should preserve parent UClass identity"), Step.StepName),
			ReloadedParentClass,
			InitialParentClass);
		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("%s should keep the Blueprint generated class on the same AS parent chain"), Step.StepName),
			UASClass::GetFirstASClass(BlueprintClass),
			InitialASClass);
		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("%s should keep the live actor on the same Blueprint class"), Step.StepName),
			BlueprintActor->GetClass(),
			BlueprintClass);
		bPassed &= Test.TestTrue(
			*FString::Printf(TEXT("%s should keep the Blueprint actor in begun-play state"), Step.StepName),
			BlueprintActor->HasActorBegunPlay());
		bPassed &= InvokeGeneratedGetValue(Test, Engine, BlueprintActor, ReloadedParentClass, Step.ExpectedValue, Step.StepName);
		return bPassed;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadSequenceTests,
	"Angelscript.TestModule.HotReload.Sequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(MultipleSoftReloadsUpdateRunningBlueprintChildFunction)
	{
		using namespace AngelscriptHotReloadSequenceTest;

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		{ FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*SoftSequenceModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class AHotReloadSequenceSoftRuntimeParent : AActor
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

		const FString ScriptV2 = TEXT(R"AS(
UCLASS()
class AHotReloadSequenceSoftRuntimeParent : AActor
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

		const FString ScriptV3 = TEXT(R"AS(
UCLASS()
class AHotReloadSequenceSoftRuntimeParent : AActor
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

		const FString ScriptV4 = TEXT(R"AS(
UCLASS()
class AHotReloadSequenceSoftRuntimeParent : AActor
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
			SoftSequenceClassName);
		ASSERT_THAT(IsNotNull(ParentClass));

		UASClass* ParentASClass = Cast<UASClass>(ParentClass);
		ASSERT_THAT(IsNotNull(ParentASClass, TEXT("HotReload soft sequence should start from an AS parent class")));

		FScopedTransientBlueprint Blueprint;
		ASSERT_THAT(IsTrue(Blueprint.CreateAndCompile(*TestRunner, ParentClass, TEXT("SoftRuntime"))));

		UClass* BlueprintClass = Blueprint.GetGeneratedClass();
		ASSERT_THAT(IsNotNull(BlueprintClass, TEXT("HotReload soft sequence should expose a Blueprint generated class")));
		ASSERT_THAT(IsTrue(BlueprintClass->IsChildOf(ParentClass), TEXT("HotReload soft sequence Blueprint class should inherit from the AS parent")));
		ASSERT_THAT(IsNull(Cast<UASClass>(BlueprintClass), TEXT("HotReload soft sequence Blueprint generated class should not be a UASClass")));
		ASSERT_THAT(AreEqual(ParentASClass, UASClass::GetFirstASClass(BlueprintClass), TEXT("HotReload soft sequence should resolve AS parent through Blueprint parent chain")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* BlueprintActor = SpawnBlueprintActorAndBeginPlay(*TestRunner, Engine, Spawner, BlueprintClass);
		ASSERT_THAT(IsNotNull(BlueprintActor, TEXT("HotReload soft sequence should spawn a Blueprint child actor")));

		UWorld& World = Spawner.GetWorld();
		World.Tick(ELevelTick::LEVELTICK_All, 0.016f);

		ASSERT_THAT(IsTrue(InvokeGeneratedGetValue(*TestRunner, Engine, BlueprintActor, ParentClass, 10, TEXT("HotReload soft sequence baseline"))));

		int32 BeginPlayCountBeforeReload = 0;
		ASSERT_THAT(IsTrue(ReadIntProperty(*TestRunner, BlueprintActor, TEXT("BeginPlayCount"), BeginPlayCountBeforeReload, TEXT("HotReload soft sequence baseline"))));
		ASSERT_THAT(AreEqual(1, BeginPlayCountBeforeReload, TEXT("HotReload soft sequence should run BeginPlay once before reload")));

		const FSoftReloadStep Steps[] =
		{
			{ TEXT("HotReload soft sequence step V2 helper body"), *ScriptV2, 11 },
			{ TEXT("HotReload soft sequence step V3 helper reads runtime state"), *ScriptV3, 31 },
			{ TEXT("HotReload soft sequence step V4 helper body changes again"), *ScriptV4, 40 },
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
			ASSERT_THAT(AreEqual(1, BeginPlayCountAfterReload, TEXT("HotReload soft sequence should not replay BeginPlay during soft reload steps")));
		}
		}
	}

	TEST_METHOD(StructuralReloadKeepsBlueprintChildRecoverable)
	{
		using namespace AngelscriptHotReloadSequenceTest;

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		{ FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*StructuralSequenceModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class AHotReloadSequenceStructuralBlueprintParent : AActor
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

		const FString ScriptV2 = TEXT(R"AS(
UCLASS()
class AHotReloadSequenceStructuralBlueprintParent : AActor
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

		const FString ScriptV3 = TEXT(R"AS(
UCLASS()
class AHotReloadSequenceStructuralBlueprintParent : AActor
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
			StructuralSequenceModuleName,
			StructuralSequenceFilename,
			ScriptV1,
			StructuralSequenceClassName);
		ASSERT_THAT(IsNotNull(InitialParentClass));

		UASClass* InitialASClass = Cast<UASClass>(InitialParentClass);
		ASSERT_THAT(IsNotNull(InitialASClass, TEXT("HotReload structural sequence should start from an AS parent class")));

		FIntProperty* InitialValueProperty = FindFProperty<FIntProperty>(InitialParentClass, TEXT("Value"));
		ASSERT_THAT(IsNotNull(InitialValueProperty, TEXT("HotReload structural sequence should expose Value before reload")));
		ASSERT_THAT(IsFalse(InitialValueProperty->HasAnyPropertyFlags(CPF_Edit), TEXT("Value should start as NotEditable before structural reload")));

		FScopedTransientBlueprint Blueprint;
		ASSERT_THAT(IsTrue(Blueprint.CreateAndCompile(*TestRunner, InitialParentClass, TEXT("StructuralRuntime"))));

		UClass* InitialBlueprintClass = Blueprint.GetGeneratedClass();
		ASSERT_THAT(IsNotNull(InitialBlueprintClass, TEXT("HotReload structural sequence should expose an initial Blueprint generated class")));
		ASSERT_THAT(IsTrue(InitialBlueprintClass->IsChildOf(InitialParentClass), TEXT("HotReload structural sequence Blueprint should inherit from initial AS parent")));
		ASSERT_THAT(AreEqual(InitialASClass, UASClass::GetFirstASClass(InitialBlueprintClass), TEXT("HotReload structural sequence should resolve initial AS parent through Blueprint parent chain")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* InitialBlueprintActor = SpawnBlueprintActorAndBeginPlay(*TestRunner, Engine, Spawner, InitialBlueprintClass);
		ASSERT_THAT(IsNotNull(InitialBlueprintActor, TEXT("HotReload structural sequence should spawn the initial Blueprint child actor")));
		ASSERT_THAT(IsTrue(InvokeGeneratedGetValue(*TestRunner, Engine, InitialBlueprintActor, InitialParentClass, 10, TEXT("HotReload structural sequence baseline"))));

		ECompileResult AddPropertyReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::FullReload, StructuralSequenceModuleName, StructuralSequenceFilename, ScriptV2, AddPropertyReloadResult),
			TEXT("HotReload structural sequence should compile the added-property full reload")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(AddPropertyReloadResult), TEXT("HotReload structural sequence added-property reload should finish on a handled path")));

		UClass* AddedPropertyParentClass = FindGeneratedClass(&Engine, StructuralSequenceClassName);
		ASSERT_THAT(IsNotNull(AddedPropertyParentClass, TEXT("HotReload structural sequence should resolve parent class after added-property reload")));
		ASSERT_THAT(IsTrue(AddedPropertyParentClass != InitialParentClass, TEXT("HotReload structural sequence should replace parent UClass on structural full reload")));

		UASClass* AddedPropertyASClass = Cast<UASClass>(AddedPropertyParentClass);
		ASSERT_THAT(IsNotNull(AddedPropertyASClass, TEXT("HotReload structural sequence added-property parent should still be UASClass")));
		ASSERT_THAT(AreEqual(AddedPropertyASClass, InitialASClass->NewerVersion, TEXT("HotReload structural sequence should chain initial AS class to added-property version")));
		ASSERT_THAT(AreEqual(AddedPropertyParentClass, InitialASClass->GetMostUpToDateClass(), TEXT("HotReload structural sequence should resolve added-property class as most up to date")));

		ASSERT_THAT(IsNotNull(Blueprint.Blueprint, TEXT("HotReload structural sequence should keep the transient Blueprint asset after full reload")));
		ASSERT_THAT(IsTrue(Blueprint.Compile(*TestRunner), TEXT("HotReload structural sequence should recompile Blueprint after added-property reload")));

		UClass* AddedPropertyBlueprintClass = Blueprint.GetGeneratedClass();
		ASSERT_THAT(IsNotNull(AddedPropertyBlueprintClass, TEXT("HotReload structural sequence should expose Blueprint class after added-property reload")));
		ASSERT_THAT(IsTrue(BlueprintParentChainResolvesTo(
			*TestRunner,
			AddedPropertyBlueprintClass,
			AddedPropertyParentClass,
			TEXT("HotReload structural sequence added-property Blueprint"))));

		AActor* AddedPropertyBlueprintActor = SpawnBlueprintActorAndBeginPlay(*TestRunner, Engine, Spawner, AddedPropertyBlueprintClass);
		ASSERT_THAT(IsNotNull(AddedPropertyBlueprintActor, TEXT("HotReload structural sequence should spawn Blueprint actor after added-property reload")));
		ASSERT_THAT(IsTrue(InvokeGeneratedGetValue(*TestRunner, Engine, AddedPropertyBlueprintActor, AddedPropertyParentClass, 15, TEXT("HotReload structural sequence added-property version"))));

		FIntProperty* AddedBonusProperty = FindFProperty<FIntProperty>(AddedPropertyParentClass, TEXT("Bonus"));
		ASSERT_THAT(IsNotNull(AddedBonusProperty, TEXT("HotReload structural sequence should expose Bonus after added-property reload")));

		ECompileResult SpecifierReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::FullReload, StructuralSequenceModuleName, StructuralSequenceFilename, ScriptV3, SpecifierReloadResult),
			TEXT("HotReload structural sequence should compile the specifier/default full reload")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(SpecifierReloadResult), TEXT("HotReload structural sequence specifier/default reload should finish on a handled path")));

		UClass* SpecifierParentClass = FindGeneratedClass(&Engine, StructuralSequenceClassName);
		ASSERT_THAT(IsNotNull(SpecifierParentClass, TEXT("HotReload structural sequence should resolve parent class after specifier/default reload")));
		ASSERT_THAT(IsTrue(SpecifierParentClass != AddedPropertyParentClass, TEXT("HotReload structural sequence should replace parent UClass after second structural full reload")));

		UASClass* SpecifierASClass = Cast<UASClass>(SpecifierParentClass);
		ASSERT_THAT(IsNotNull(SpecifierASClass, TEXT("HotReload structural sequence specifier/default parent should still be UASClass")));
		ASSERT_THAT(AreEqual(SpecifierASClass, AddedPropertyASClass->NewerVersion, TEXT("HotReload structural sequence should chain added-property AS class to specifier/default version")));
		ASSERT_THAT(AreEqual(SpecifierParentClass, InitialASClass->GetMostUpToDateClass(), TEXT("HotReload structural sequence should resolve final class as most up to date from initial AS class")));

		ASSERT_THAT(IsTrue(Blueprint.Compile(*TestRunner), TEXT("HotReload structural sequence should recompile Blueprint after specifier/default reload")));

		UClass* SpecifierBlueprintClass = Blueprint.GetGeneratedClass();
		ASSERT_THAT(IsNotNull(SpecifierBlueprintClass, TEXT("HotReload structural sequence should expose Blueprint class after specifier/default reload")));
		ASSERT_THAT(IsTrue(BlueprintParentChainResolvesTo(
			*TestRunner,
			SpecifierBlueprintClass,
			SpecifierParentClass,
			TEXT("HotReload structural sequence final Blueprint"))));

		FIntProperty* FinalValueProperty = FindFProperty<FIntProperty>(SpecifierParentClass, TEXT("Value"));
		ASSERT_THAT(IsNotNull(FinalValueProperty, TEXT("HotReload structural sequence should expose final Value property")));
		ASSERT_THAT(IsTrue(FinalValueProperty->HasAnyPropertyFlags(CPF_Edit), TEXT("HotReload structural sequence should expose Value as EditAnywhere after final reload")));

		AActor* FinalBlueprintActor = SpawnBlueprintActorAndBeginPlay(*TestRunner, Engine, Spawner, SpecifierBlueprintClass);
		ASSERT_THAT(IsNotNull(FinalBlueprintActor, TEXT("HotReload structural sequence should spawn Blueprint actor after final reload")));
		ASSERT_THAT(IsTrue(InvokeGeneratedGetValue(*TestRunner, Engine, FinalBlueprintActor, SpecifierParentClass, 15, TEXT("HotReload structural sequence preserved Blueprint child"))));

		int32 FinalValue = 0;
		ASSERT_THAT(IsTrue(ReadIntProperty(*TestRunner, FinalBlueprintActor, TEXT("Value"), FinalValue, TEXT("HotReload structural sequence preserved Blueprint child"))));
		ASSERT_THAT(AreEqual(10, FinalValue, TEXT("HotReload structural sequence existing Blueprint child should preserve its prior Value default")));

		int32 FinalBonus = 0;
		ASSERT_THAT(IsTrue(ReadIntProperty(*TestRunner, FinalBlueprintActor, TEXT("Bonus"), FinalBonus, TEXT("HotReload structural sequence preserved Blueprint child"))));
		ASSERT_THAT(AreEqual(5, FinalBonus, TEXT("HotReload structural sequence existing Blueprint child should preserve its prior Bonus default")));

		AActor* FinalParentActor = SpawnBlueprintActorAndBeginPlay(*TestRunner, Engine, Spawner, SpecifierParentClass);
		ASSERT_THAT(IsNotNull(FinalParentActor, TEXT("HotReload structural sequence should spawn the final AS parent actor")));
		ASSERT_THAT(IsTrue(InvokeGeneratedGetValue(*TestRunner, Engine, FinalParentActor, SpecifierParentClass, 27, TEXT("HotReload structural sequence final AS parent"))));

		FScopedTransientBlueprint FreshBlueprint;
		ASSERT_THAT(IsTrue(FreshBlueprint.CreateAndCompile(*TestRunner, SpecifierParentClass, TEXT("StructuralRuntimeFinal"))));

		UClass* FreshBlueprintClass = FreshBlueprint.GetGeneratedClass();
		ASSERT_THAT(IsNotNull(FreshBlueprintClass, TEXT("HotReload structural sequence should expose a fresh Blueprint class after final reload")));
		ASSERT_THAT(IsTrue(FreshBlueprintClass->IsChildOf(SpecifierParentClass), TEXT("HotReload structural sequence fresh Blueprint should inherit from final AS parent")));

		AActor* FreshBlueprintActor = SpawnBlueprintActorAndBeginPlay(*TestRunner, Engine, Spawner, FreshBlueprintClass);
		ASSERT_THAT(IsNotNull(FreshBlueprintActor, TEXT("HotReload structural sequence should spawn a fresh Blueprint child actor after final reload")));
		ASSERT_THAT(IsTrue(InvokeGeneratedGetValue(*TestRunner, Engine, FreshBlueprintActor, SpecifierParentClass, 27, TEXT("HotReload structural sequence fresh Blueprint final version"))));

		int32 FreshValue = 0;
		ASSERT_THAT(IsTrue(ReadIntProperty(*TestRunner, FreshBlueprintActor, TEXT("Value"), FreshValue, TEXT("HotReload structural sequence fresh Blueprint final version"))));
		ASSERT_THAT(AreEqual(20, FreshValue, TEXT("HotReload structural sequence fresh Blueprint actor should inherit updated Value default")));

		int32 FreshBonus = 0;
		ASSERT_THAT(IsTrue(ReadIntProperty(*TestRunner, FreshBlueprintActor, TEXT("Bonus"), FreshBonus, TEXT("HotReload structural sequence fresh Blueprint final version"))));
		ASSERT_THAT(AreEqual(7, FreshBonus, TEXT("HotReload structural sequence fresh Blueprint actor should inherit updated Bonus default")));
		}
	}
};

#endif
