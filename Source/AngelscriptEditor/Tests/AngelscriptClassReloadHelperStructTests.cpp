#include "CQTest.h"
#include "HotReload/ClassReloadHelper.h"

#include "AngelscriptEngine.h"

#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/StructureEditorUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptEditor_Private_Tests_AngelscriptClassReloadHelperStructTests_Private
{
	TUniquePtr<FAngelscriptEngine> MakeClassReloadHelperStructTestEngine()
	{
		FAngelscriptEngineConfig Config;
		Config.bSkipInitialCompile = true;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		return FAngelscriptEngine::Create(Config, Dependencies);
	}

	void EnsureClassReloadHelperInitialized(FAngelscriptEngine& Engine)
	{
		if (!Engine.GetOnClassReload().IsBound())
		{
			FClassReloadHelper::Init();
		}
	}

	void RootObject(TArray<UObject*>& RootedObjects, UObject* Object)
	{
		if (Object == nullptr || RootedObjects.Contains(Object))
		{
			return;
		}

		Object->AddToRoot();
		RootedObjects.Add(Object);
	}

	UBlueprint* CreateTransientBlueprintChild(FAutomationTestBase& Test, UClass* ParentClass, FStringView Suffix, TArray<UObject*>& RootedObjects)
	{
		const FString PackagePath = FString::Printf(
			TEXT("/Temp/AngelscriptClassReloadHelperStruct_%.*s_%s"),
			Suffix.Len(),
			Suffix.GetData(),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));

		UPackage* BlueprintPackage = CreatePackage(*PackagePath);
		if (!Test.TestNotNull(TEXT("ClassReloadHelper.StructReinstance test should create a transient blueprint package"), BlueprintPackage))
		{
			return nullptr;
		}

		BlueprintPackage->SetFlags(RF_Transient);
		RootObject(RootedObjects, BlueprintPackage);

		const FName BlueprintName(*FPackageName::GetLongPackageAssetName(PackagePath));
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			ParentClass,
			BlueprintPackage,
			BlueprintName,
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("AngelscriptClassReloadHelperStructTests"));
		if (Blueprint == nullptr)
		{
			return nullptr;
		}

		RootObject(RootedObjects, Blueprint);
		if (UClass* GeneratedClass = Blueprint->GeneratedClass)
		{
			RootObject(RootedObjects, GeneratedClass);
		}

		return Blueprint;
	}

	UUserDefinedStruct* CreateTransientUserDefinedStruct(FAutomationTestBase& Test, FStringView Suffix, TArray<UObject*>& RootedObjects)
	{
		const FName StructName(*FString::Printf(
			TEXT("AngelscriptClassReloadHelperStruct%s_%s"),
			Suffix.GetData(),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits)));

		UUserDefinedStruct* Struct = FStructureEditorUtils::CreateUserDefinedStruct(GetTransientPackage(), StructName, RF_Transient);
		if (!Test.TestNotNull(*FString::Printf(TEXT("ClassReloadHelper.StructReinstance test should create struct fixture %.*s"), Suffix.Len(), Suffix.GetData()), Struct))
		{
			return nullptr;
		}

		RootObject(RootedObjects, Struct);
		FStructureEditorUtils::CompileStructure(Struct);
		return Struct;
	}

	struct FObservedStructChangeListener : FStructureEditorUtils::INotifyOnStructChanged
	{
		UUserDefinedStruct* ExpectedPreStruct = nullptr;
		UUserDefinedStruct* ExpectedPostStruct = nullptr;
		int32 PreChangeCalls = 0;
		int32 PostChangeCalls = 0;
		const UUserDefinedStruct* LastPreChangedStruct = nullptr;
		const UUserDefinedStruct* LastPostChangedStruct = nullptr;

		FObservedStructChangeListener(UUserDefinedStruct* InExpectedPreStruct, UUserDefinedStruct* InExpectedPostStruct)
			: ExpectedPreStruct(InExpectedPreStruct)
			, ExpectedPostStruct(InExpectedPostStruct)
		{
		}

		virtual void PreChange(const UUserDefinedStruct* Changed, FStructureEditorUtils::EStructureEditorChangeInfo ChangeInfo) override
		{
			if (Changed == ExpectedPreStruct && ChangeInfo == FStructureEditorUtils::EStructureEditorChangeInfo::Unknown)
			{
				++PreChangeCalls;
				LastPreChangedStruct = Changed;
			}
		}

		virtual void PostChange(const UUserDefinedStruct* Changed, FStructureEditorUtils::EStructureEditorChangeInfo ChangeInfo) override
		{
			if (Changed == ExpectedPostStruct && ChangeInfo == FStructureEditorUtils::EStructureEditorChangeInfo::Unknown)
			{
				++PostChangeCalls;
				LastPostChangedStruct = Changed;
			}
		}
	};

	struct FPerformReinstanceStructCallLog
	{
		int32 NotifyCustomizationModuleChangedCalls = 0;
		int32 FlushCompilationQueueAndReinstanceCalls = 0;
		TArray<UBlueprint*> QueuedBlueprints;
	};
}

#define TestTrue(...) Test.TestTrue(__VA_ARGS__)
#define TestFalse(...) Test.TestFalse(__VA_ARGS__)
#define TestEqual(...) Test.TestEqual(__VA_ARGS__)
#define TestNotNull(...) Test.TestNotNull(__VA_ARGS__)

static bool RunPerformReinstanceStructDependency(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptClassReloadHelperStructTests_Private;
	const FClassReloadHelper::FReloadState SavedState = FClassReloadHelper::ReloadState();
	TArray<FAngelscriptEngine*> SavedStack = FAngelscriptEngineContextStack::SnapshotAndClear();
	TUniquePtr<FAngelscriptEngine> Engine = MakeClassReloadHelperStructTestEngine();
	TUniquePtr<FAngelscriptEngineScope> EngineScope;
	TArray<UObject*> RootedObjects;

	ON_SCOPE_EXIT
	{
		FClassReloadHelperTestAccess::ResetPerformReinstanceTestHooks();
		EngineScope.Reset();

		for (UObject* Object : RootedObjects)
		{
			if (Object != nullptr)
			{
				Object->RemoveFromRoot();
				Object->MarkAsGarbage();
			}
		}

		CollectGarbage(RF_NoFlags, true);
		FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(SavedStack));
		FClassReloadHelper::ReloadState() = SavedState;
	};

	if (!TestNotNull(TEXT("ClassReloadHelper.StructReinstance test should create a testing engine"), Engine.Get()))
	{
		return false;
	}
	if (!TestNotNull(TEXT("ClassReloadHelper.StructReinstance test should expose GEditor"), GEditor))
	{
		return false;
	}

	EngineScope = MakeUnique<FAngelscriptEngineScope>(*Engine);
	Engine->bIsInitialCompileFinished = true;
	EnsureClassReloadHelperInitialized(*Engine);

	UUserDefinedStruct* OldStruct = CreateTransientUserDefinedStruct(Test, TEXT("Old"), RootedObjects);
	UUserDefinedStruct* NewStruct = CreateTransientUserDefinedStruct(Test, TEXT("New"), RootedObjects);
	if (OldStruct == nullptr || NewStruct == nullptr)
	{
		return false;
	}

	UBlueprint* DependentBlueprint = CreateTransientBlueprintChild(Test, UObject::StaticClass(), TEXT("Dependent"), RootedObjects);
	UBlueprint* ControlBlueprint = CreateTransientBlueprintChild(Test, UObject::StaticClass(), TEXT("Control"), RootedObjects);
	if (DependentBlueprint == nullptr || ControlBlueprint == nullptr)
	{
		return false;
	}

	FBPVariableDescription DependentVariable;
	DependentVariable.VarName = TEXT("ReloadedStructValue");
	DependentVariable.VarType.PinCategory = UEdGraphSchema_K2::PC_Struct;
	DependentVariable.VarType.PinSubCategoryObject = OldStruct;
	DependentBlueprint->NewVariables.Add(DependentVariable);

	UDataTable* DependentTable = NewObject<UDataTable>(
		GetTransientPackage(),
		MakeUniqueObjectName(GetTransientPackage(), UDataTable::StaticClass(), TEXT("ClassReloadHelperStructTable")),
		RF_Transient);
	if (!TestNotNull(TEXT("ClassReloadHelper.StructReinstance test should create a dependent data table"), DependentTable))
	{
		return false;
	}

	RootObject(RootedObjects, DependentTable);
	DependentTable->RowStruct = OldStruct;

	const TArray<UDataTable*> TablesBeforeReload = FClassReloadHelper::GetTablesDependentOnStruct(OldStruct);
	if (!TestTrue(TEXT("ClassReloadHelper.StructReinstance test should expose the dependent data table before reload"), TablesBeforeReload.Contains(DependentTable)))
	{
		return false;
	}

	FObservedStructChangeListener StructChangeListener(OldStruct, NewStruct);
	FPerformReinstanceStructCallLog CallLog;

	FClassReloadHelper::FReloadState& ReloadState = FClassReloadHelper::ReloadState();
	ReloadState = FClassReloadHelper::FReloadState();
	ReloadState.ReloadStructs.Add(OldStruct, NewStruct);

	FClassReloadHelperPerformReinstanceTestHooks Hooks;
	Hooks.NotifyCustomizationModuleChanged = [&CallLog]()
	{
		++CallLog.NotifyCustomizationModuleChangedCalls;
	};
	Hooks.QueueBlueprintForCompilation = [&CallLog](UBlueprint* Blueprint)
	{
		CallLog.QueuedBlueprints.Add(Blueprint);
	};
	Hooks.FlushCompilationQueueAndReinstance = [&CallLog]()
	{
		++CallLog.FlushCompilationQueueAndReinstanceCalls;
	};
	FClassReloadHelperTestAccess::SetPerformReinstanceTestHooks(MoveTemp(Hooks));

	ReloadState.PerformReinstance();

	if (!TestEqual(TEXT("ClassReloadHelper.StructReinstance should broadcast struct pre-change exactly once for the old struct"), StructChangeListener.PreChangeCalls, 1))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.StructReinstance should broadcast struct post-change exactly once for the new struct"), StructChangeListener.PostChangeCalls, 1))
	{
		return false;
	}
	if (!TestTrue(TEXT("ClassReloadHelper.StructReinstance should broadcast pre-change for the old struct instance"), StructChangeListener.LastPreChangedStruct == OldStruct))
	{
		return false;
	}
	if (!TestTrue(TEXT("ClassReloadHelper.StructReinstance should broadcast post-change for the new struct instance"), StructChangeListener.LastPostChangedStruct == NewStruct))
	{
		return false;
	}
	if (!TestTrue(TEXT("ClassReloadHelper.StructReinstance should update dependent data tables to the new row struct"), DependentTable->RowStruct == NewStruct))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.StructReinstance should keep one dependent variable on the impacted blueprint"), DependentBlueprint->NewVariables.Num(), 1))
	{
		return false;
	}
	if (!TestTrue(TEXT("ClassReloadHelper.StructReinstance should rewrite impacted blueprint variables to the replacement struct"), DependentBlueprint->NewVariables[0].VarType.PinSubCategoryObject.Get() == NewStruct))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.StructReinstance should enqueue exactly one dependent blueprint for recompilation"), CallLog.QueuedBlueprints.Num(), 1))
	{
		return false;
	}
	if (!TestTrue(TEXT("ClassReloadHelper.StructReinstance should only enqueue the impacted blueprint"), CallLog.QueuedBlueprints.Num() == 1 && CallLog.QueuedBlueprints[0] == DependentBlueprint))
	{
		return false;
	}
	if (!TestFalse(TEXT("ClassReloadHelper.StructReinstance should not enqueue unrelated blueprints"), CallLog.QueuedBlueprints.Contains(ControlBlueprint)))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.StructReinstance should flush the blueprint compilation queue once after enqueuing dependent blueprints"), CallLog.FlushCompilationQueueAndReinstanceCalls, 1))
	{
		return false;
	}
	return TestEqual(TEXT("ClassReloadHelper.StructReinstance should still refresh property editor customizations after struct reload"), CallLog.NotifyCustomizationModuleChangedCalls, 1);
}

#undef TestTrue
#undef TestFalse
#undef TestEqual
#undef TestNotNull

TEST_CLASS_WITH_FLAGS(FAngelscriptClassReloadHelperStructTests,
	"Angelscript.TestModule.Editor.ClassReloadHelper",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(PerformReinstanceBroadcastsStructChangesAndRecompilesDependentBlueprints)
	{
		ASSERT_THAT(IsTrue(RunPerformReinstanceStructDependency(*TestRunner)));
	}
};

#endif
