#include "CQTest.h"
#include "Tests/AngelscriptEditorMenuExtensionsTestTypes.h"

#include "AngelscriptEngine.h"
#include "ClassGenerator/ASClass.h"
#include "Editor.h"
#include "EditorMenuExtensions/ScriptEditorMenuExtension.h"

#include "AssetRegistry/AssetData.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveVector.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptEditor_Private_Tests_AngelscriptEditorMenuExtensionsTests_Private
{
	struct FResolvedMenuExtensionEngine
	{
		TUniquePtr<FAngelscriptEngine> OwnedEngine;
		TUniquePtr<FAngelscriptEngineScope> Scope;
		FAngelscriptEngine* Engine = nullptr;

		FAngelscriptEngine& Get() const
		{
			check(Engine != nullptr);
			return *Engine;
		}
	};

	bool AcquireMenuExtensionEngine(FAutomationTestBase& Test, FResolvedMenuExtensionEngine& OutResolved)
	{
		if (FAngelscriptEngine::IsInitialized())
		{
			OutResolved.Engine = &FAngelscriptEngine::Get();
			OutResolved.Scope = MakeUnique<FAngelscriptEngineScope>(*OutResolved.Engine);
			return true;
		}

		FAngelscriptEngineConfig Config;
		Config.bSkipInitialCompile = true;
		FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		OutResolved.OwnedEngine = FAngelscriptEngine::Create(Config, Dependencies);
		if (!Test.TestNotNull(TEXT("MenuExtensions test should acquire an Angelscript engine"), OutResolved.OwnedEngine.Get()))
		{
			return false;
		}

		OutResolved.Engine = OutResolved.OwnedEngine.Get();
		OutResolved.Scope = MakeUnique<FAngelscriptEngineScope>(*OutResolved.Engine);
		return true;
	}

	template <typename AssetType>
	AssetType* CreateMenuExtensionTestAsset(FAutomationTestBase& Test, UPackage* AssetsPackage, const TCHAR* BaseName)
	{
		if (!Test.TestNotNull(TEXT("MenuExtensions test should have a valid assets package"), AssetsPackage))
		{
			return nullptr;
		}

		const FString UniqueSuffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FName DesiredName(*FString::Printf(TEXT("%s_%s"), BaseName, *UniqueSuffix));
		const FName UniqueName = MakeUniqueObjectName(AssetsPackage, AssetType::StaticClass(), DesiredName);
		return NewObject<AssetType>(AssetsPackage, UniqueName, RF_Public | RF_Standalone | RF_Transient);
	}

	void CleanupMenuExtensionTestAsset(UObject*& Asset)
	{
		if (Asset == nullptr)
		{
			return;
		}

		Asset->ClearFlags(RF_Public | RF_Standalone);
		Asset->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_AllowPackageLinkerMismatch | REN_NonTransactional);
		Asset->MarkAsGarbage();
		Asset = nullptr;
	}

	template <typename ActorType>
	ActorType* SpawnMenuExtensionTestActor(FAutomationTestBase& Test, UWorld* World, const TCHAR* BaseName)
	{
		if (!Test.TestNotNull(TEXT("MenuExtensions test should resolve an editor world"), World))
		{
			return nullptr;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.ObjectFlags |= RF_Transient;
		SpawnParameters.Name = MakeUniqueObjectName(World, ActorType::StaticClass(), FName(BaseName));
		return World->SpawnActor<ActorType>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
	}

	bool ContainsFunction(const TArray<FName>& FunctionNames, FName FunctionName)
	{
		return FunctionNames.Contains(FunctionName);
	}

	bool ContainsSnapshot(
		const TArray<UScriptEditorMenuExtension::FRegisteredExtensionSnapshot>& Snapshots,
		const EScriptEditorMenuExtensionLocation Location,
		const FName ExtensionPoint,
		const FName SectionName)
	{
		return Snapshots.ContainsByPredicate([Location, ExtensionPoint, SectionName](const UScriptEditorMenuExtension::FRegisteredExtensionSnapshot& Snapshot)
		{
			return Snapshot.Location == Location
				&& Snapshot.ExtensionPoint == ExtensionPoint
				&& Snapshot.SectionName == SectionName;
		});
	}

	bool ContainsNewToolSnapshot(
		const TArray<UScriptEditorMenuExtension::FRegisteredExtensionSnapshot>& BaselineSnapshots,
		const TArray<UScriptEditorMenuExtension::FRegisteredExtensionSnapshot>& Snapshots)
	{
		return Snapshots.ContainsByPredicate([&BaselineSnapshots](const UScriptEditorMenuExtension::FRegisteredExtensionSnapshot& Snapshot)
		{
			if (Snapshot.Location != EScriptEditorMenuExtensionLocation::ToolMenu)
			{
				return false;
			}

			return !BaselineSnapshots.ContainsByPredicate([&Snapshot](const UScriptEditorMenuExtension::FRegisteredExtensionSnapshot& Baseline)
			{
				return Baseline.Location == Snapshot.Location
					&& Baseline.ExtensionPoint == Snapshot.ExtensionPoint
					&& Baseline.SectionName == Snapshot.SectionName;
			});
		});
	}
}


#define TestTrue(...) Test.TestTrue(__VA_ARGS__)
#define TestFalse(...) Test.TestFalse(__VA_ARGS__)
#define TestEqual(...) Test.TestEqual(__VA_ARGS__)
#define TestNotNull(...) Test.TestNotNull(__VA_ARGS__)

static bool RunRegisterFunctionsAndForwardFilteredSelection(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptEditorMenuExtensionsTests_Private;
	UAngelscriptEditorMenuExtensionTestShim* BaseExtension = NewObject<UAngelscriptEditorMenuExtensionTestShim>(GetTransientPackage());
	if (!TestNotNull(TEXT("MenuExtensions test should create a base extension shim"), BaseExtension))
	{
		return false;
	}

	const TArray<FName> BaseFunctionNames = BaseExtension->GatheredFunctionNames();
	TestTrue(TEXT("MenuExtensions should gather CallInEditor functions"), ContainsFunction(BaseFunctionNames, GET_FUNCTION_NAME_CHECKED(UAngelscriptEditorMenuExtensionTestShim, IncludedCommand)));
	TestTrue(TEXT("MenuExtensions should gather multiple CallInEditor functions"), ContainsFunction(BaseFunctionNames, GET_FUNCTION_NAME_CHECKED(UAngelscriptEditorMenuExtensionTestShim, IncludedSecondaryCommand)));
	TestFalse(TEXT("MenuExtensions should skip CallInEditor functions with return values"), ContainsFunction(BaseFunctionNames, GET_FUNCTION_NAME_CHECKED(UAngelscriptEditorMenuExtensionTestShim, ExcludedReturnsValue)));
	TestFalse(TEXT("MenuExtensions should skip functions without CallInEditor metadata"), ContainsFunction(BaseFunctionNames, GET_FUNCTION_NAME_CHECKED(UAngelscriptEditorMenuExtensionTestShim, ExcludedWithoutCallInEditor)));

	UAngelscriptActorMenuExtensionTestShim* ActorExtension = NewObject<UAngelscriptActorMenuExtensionTestShim>(GetTransientPackage());
	if (!TestNotNull(TEXT("MenuExtensions test should create an actor extension shim"), ActorExtension))
	{
		return false;
	}
	ActorExtension->ExtensionPoint = NAME_None;
	TestEqual(TEXT("Actor menu extensions should resolve the default actor context extension point"), ActorExtension->ResolveExtensionPoint(), FName(TEXT("ActorOptions")));

	UAngelscriptAssetMenuExtensionTestShim* AssetExtension = NewObject<UAngelscriptAssetMenuExtensionTestShim>(GetTransientPackage());
	if (!TestNotNull(TEXT("MenuExtensions test should create an asset extension shim"), AssetExtension))
	{
		return false;
	}
	AssetExtension->ExtensionPoint = NAME_None;
	TestEqual(TEXT("Asset menu extensions should resolve the default asset context extension point"), AssetExtension->ResolveExtensionPoint(), FName(TEXT("CommonAssetActions")));

	FResolvedMenuExtensionEngine ResolvedEngine;
	if (!AcquireMenuExtensionEngine(Test, ResolvedEngine))
	{
		return false;
	}
	FAngelscriptEngine& Engine = ResolvedEngine.Get();

	UWorld* EditorWorld = (GEditor != nullptr) ? GEditor->GetEditorWorldContext().World() : nullptr;
	AStaticMeshActor* AllowedActor = SpawnMenuExtensionTestActor<AStaticMeshActor>(Test, EditorWorld, TEXT("MenuExtensionAllowedActor"));
	AActor* UnsupportedActor = SpawnMenuExtensionTestActor<AActor>(Test, EditorWorld, TEXT("MenuExtensionUnsupportedActor"));
	UObject* NonActorSelection = NewObject<UCurveFloat>(GetTransientPackage());
	if (!TestNotNull(TEXT("MenuExtensions test should spawn the allowed actor"), AllowedActor)
		|| !TestNotNull(TEXT("MenuExtensions test should spawn the unsupported actor"), UnsupportedActor))
	{
		return false;
	}

	ActorExtension->SupportedClasses.Add(AStaticMeshActor::StaticClass());

	ON_SCOPE_EXIT
	{
		if (AllowedActor != nullptr)
		{
			AllowedActor->Destroy();
		}
		if (UnsupportedActor != nullptr)
		{
			UnsupportedActor->Destroy();
		}
	};

	const TArray<UObject*> ActorSelection = { AllowedActor, UnsupportedActor, NonActorSelection };
	const TArray<FName> ActorFunctionNames = ActorExtension->GatheredFunctionNamesForSelection(ActorSelection);
	TestTrue(TEXT("Actor menu extensions should keep first-parameter-compatible functions for matching selections"), ContainsFunction(ActorFunctionNames, GET_FUNCTION_NAME_CHECKED(UAngelscriptActorMenuExtensionTestShim, RecordStaticMeshActor)));

	const TArray<FName> UnsupportedActorFunctionNames = ActorExtension->GatheredFunctionNamesForSelection({ UnsupportedActor });
	TestFalse(TEXT("Actor menu extensions should hide all functions when the selection has no supported actors"), ContainsFunction(UnsupportedActorFunctionNames, GET_FUNCTION_NAME_CHECKED(UAngelscriptActorMenuExtensionTestShim, RecordStaticMeshActor)));

	ActorExtension->ResetRecordedActors();
	ActorExtension->InvokeSelectionFunction(GET_FUNCTION_NAME_CHECKED(UAngelscriptActorMenuExtensionTestShim, RecordStaticMeshActor), ActorSelection);
	TestEqual(TEXT("Actor menu extensions should forward only filtered actors to selection calls"), ActorExtension->RecordedStaticMeshActorNames.Num(), 1);
	if (!TestEqual(TEXT("Actor menu extensions should forward the supported actor instance"), ActorExtension->RecordedStaticMeshActorNames[0], AllowedActor->GetFName()))
	{
		return false;
	}

	UPackage* AssetsPackage = Engine.AssetsPackage;
	UObject* AllowedAssetObject = CreateMenuExtensionTestAsset<UCurveFloat>(Test, AssetsPackage, TEXT("AllowedCurve"));
	UObject* UnsupportedAssetObject = CreateMenuExtensionTestAsset<UCurveVector>(Test, AssetsPackage, TEXT("UnsupportedVector"));
	UCurveFloat* AllowedAsset = Cast<UCurveFloat>(AllowedAssetObject);
	UCurveVector* UnsupportedAsset = Cast<UCurveVector>(UnsupportedAssetObject);
	if (!TestNotNull(TEXT("MenuExtensions test should create the allowed asset"), AllowedAsset)
		|| !TestNotNull(TEXT("MenuExtensions test should create the unsupported asset"), UnsupportedAsset))
	{
		CleanupMenuExtensionTestAsset(AllowedAssetObject);
		CleanupMenuExtensionTestAsset(UnsupportedAssetObject);
		CollectGarbage(RF_NoFlags, true);
		return false;
	}

	ON_SCOPE_EXIT
	{
		CleanupMenuExtensionTestAsset(AllowedAssetObject);
		CleanupMenuExtensionTestAsset(UnsupportedAssetObject);
		CollectGarbage(RF_NoFlags, true);
	};

	AssetExtension->SupportedClasses.Add(UCurveFloat::StaticClass());

	const TArray<FAssetData> AssetSelection = { FAssetData(AllowedAsset), FAssetData(UnsupportedAsset) };
	const TArray<FName> AssetFunctionNames = AssetExtension->GatheredFunctionNamesForSelection(AssetSelection);
	TestTrue(TEXT("Asset menu extensions should gather object-parameter functions for matching assets"), ContainsFunction(AssetFunctionNames, GET_FUNCTION_NAME_CHECKED(UAngelscriptAssetMenuExtensionTestShim, RecordAssetObject)));
	TestTrue(TEXT("Asset menu extensions should gather struct-parameter functions for matching assets"), ContainsFunction(AssetFunctionNames, GET_FUNCTION_NAME_CHECKED(UAngelscriptAssetMenuExtensionTestShim, RecordAssetData)));

	AssetExtension->ResetRecordedAssets();
	AssetExtension->InvokeSelectionFunction(GET_FUNCTION_NAME_CHECKED(UAngelscriptAssetMenuExtensionTestShim, RecordAssetObject), AssetSelection);
	TestEqual(TEXT("Asset menu extensions should forward only filtered object assets"), AssetExtension->RecordedObjectAssetNames.Num(), 1);
	if (!TestEqual(TEXT("Asset menu extensions should forward the supported object asset"), AssetExtension->RecordedObjectAssetNames[0], AllowedAsset->GetFName()))
	{
		return false;
	}

	AssetExtension->ResetRecordedAssets();
	AssetExtension->InvokeSelectionFunction(GET_FUNCTION_NAME_CHECKED(UAngelscriptAssetMenuExtensionTestShim, RecordAssetData), AssetSelection);
	TestEqual(TEXT("Asset menu extensions should forward only filtered asset data payloads"), AssetExtension->RecordedStructAssetNames.Num(), 1);
	if (!TestEqual(TEXT("Asset menu extensions should forward the supported asset data"), AssetExtension->RecordedStructAssetNames[0], AllowedAsset->GetFName()))
	{
		return false;
	}

	const TArray<UScriptEditorMenuExtension::FRegisteredExtensionSnapshot> BaselineSnapshots = UScriptEditorMenuExtension::GetRegisteredExtensionSnapshots();
	UAngelscriptEditorMenuExtensionTestShim* ToolSnapshotExtension = NewObject<UAngelscriptEditorMenuExtensionTestShim>(GetTransientPackage());
	UAngelscriptActorMenuExtensionTestShim* ActorSnapshotExtension = NewObject<UAngelscriptActorMenuExtensionTestShim>(GetTransientPackage());
	UAngelscriptAssetMenuExtensionTestShim* AssetSnapshotExtension = NewObject<UAngelscriptAssetMenuExtensionTestShim>(GetTransientPackage());
	if (!TestNotNull(TEXT("MenuExtensions test should create the tool snapshot extension"), ToolSnapshotExtension)
		|| !TestNotNull(TEXT("MenuExtensions test should create the actor snapshot extension"), ActorSnapshotExtension)
		|| !TestNotNull(TEXT("MenuExtensions test should create the asset snapshot extension"), AssetSnapshotExtension))
	{
		return false;
	}

	ActorSnapshotExtension->ExtensionPoint = FName(TEXT("Automation.ActorPreview.EditorMenuExtensions"));
	AssetSnapshotExtension->ExtensionPoint = FName(TEXT("Automation.CommonAssetActions.EditorMenuExtensions"));
	ToolSnapshotExtension->AddToRoot();
	ActorSnapshotExtension->AddToRoot();
	AssetSnapshotExtension->AddToRoot();

	ON_SCOPE_EXIT
	{
		FAngelscriptEditorMenuExtensionTestAccess::UnregisterExtensions();
		ToolSnapshotExtension->RemoveFromRoot();
		ActorSnapshotExtension->RemoveFromRoot();
		AssetSnapshotExtension->RemoveFromRoot();
		FAngelscriptEditorMenuExtensionTestAccess::RegisterExtensions();
	};

	FAngelscriptEditorMenuExtensionTestAccess::RegisterExtension(ToolSnapshotExtension);
	FAngelscriptEditorMenuExtensionTestAccess::RegisterExtension(ActorSnapshotExtension);
	FAngelscriptEditorMenuExtensionTestAccess::RegisterExtension(AssetSnapshotExtension);

	const TArray<UScriptEditorMenuExtension::FRegisteredExtensionSnapshot> RebuiltSnapshots = UScriptEditorMenuExtension::GetRegisteredExtensionSnapshots();
	TestEqual(TEXT("MenuExtensions should register one new snapshot per test menu extension"), RebuiltSnapshots.Num(), BaselineSnapshots.Num() + 3);
	TestTrue(
		TEXT("MenuExtensions should snapshot tool menu registrations"),
		ContainsNewToolSnapshot(BaselineSnapshots, RebuiltSnapshots));
	TestTrue(
		TEXT("MenuExtensions should snapshot actor menu registrations"),
		ContainsSnapshot(
			RebuiltSnapshots,
			EScriptEditorMenuExtensionLocation::LevelViewport_ContextMenu,
			FName(TEXT("Automation.ActorPreview.EditorMenuExtensions")),
			NAME_None));
	TestTrue(
		TEXT("MenuExtensions should snapshot asset menu registrations"),
		ContainsSnapshot(
			RebuiltSnapshots,
			EScriptEditorMenuExtensionLocation::ContentBrowser_AssetViewContextMenu,
			FName(TEXT("Automation.CommonAssetActions.EditorMenuExtensions")),
			NAME_None));

	return true;
}

static bool RunActionMetadataDelegatesAndShiftNavigationFallback(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptEditorMenuExtensionsTests_Private;
	UAngelscriptEditorMenuExtensionActionTestShim* Extension = NewObject<UAngelscriptEditorMenuExtensionActionTestShim>(GetTransientPackage());
	if (!TestNotNull(TEXT("MenuExtensions action metadata test should create an action extension shim"), Extension))
	{
		return false;
	}

	UFunction* MetadataFunction = Extension->FindFunction(GET_FUNCTION_NAME_CHECKED(UAngelscriptEditorMenuExtensionActionTestShim, MetadataDrivenCommand));
	if (!TestNotNull(TEXT("MenuExtensions action metadata test should resolve the metadata-driven command"), MetadataFunction))
	{
		return false;
	}

	UAngelscriptEditorMenuExtensionActionTestShim::ResetRecordedState();
	ON_SCOPE_EXIT
	{
		UAngelscriptEditorMenuExtensionActionTestShim::ResetRecordedState();
		FAngelscriptEditorMenuExtensionTestAccess::ResetShiftModifierOverride();
		FAngelscriptEditorMenuExtensionTestAccess::ResetNavigateToFunctionOverride();
		FAngelscriptEditorMenuExtensionTestAccess::ResetNavigateToClassOverride();
	};

	const FUIAction UIAction = FAngelscriptEditorMenuExtensionTestAccess::CreateUIAction(Extension, MetadataFunction);
	const FToolUIAction ToolAction = FAngelscriptEditorMenuExtensionTestAccess::CreateToolUIAction(Extension, MetadataFunction);
	const FToolMenuContext ToolMenuContext;

	TestEqual(
		TEXT("MenuExtensions action metadata should resolve toggle action type from metadata"),
		FAngelscriptEditorMenuExtensionTestAccess::GetActionType(MetadataFunction),
		EUserInterfaceActionType::ToggleButton);

	UAngelscriptEditorMenuExtensionActionTestShim::ResetRecordedState();
	UAngelscriptEditorMenuExtensionActionTestShim::ConfigureActionResponses(false, false, false);
	TestFalse(TEXT("MenuExtensions UI actions should reflect ActionCanExecute metadata"), UIAction.CanExecute());
	TestEqual(TEXT("MenuExtensions UI actions should collapse when ActionIsVisible returns false"), UIAction.IsVisible(), EVisibility::Collapsed);
	TestEqual(TEXT("MenuExtensions UI actions should report unchecked when ActionIsChecked returns false"), UIAction.GetCheckState(), ECheckBoxState::Unchecked);
	TestFalse(TEXT("MenuExtensions tool actions should reflect ActionCanExecute metadata"), ToolAction.CanExecuteAction.Execute(ToolMenuContext));
	TestFalse(TEXT("MenuExtensions tool actions should hide when ActionIsVisible returns false"), ToolAction.IsActionVisibleDelegate.Execute(ToolMenuContext));
	TestEqual(TEXT("MenuExtensions tool actions should report unchecked when ActionIsChecked returns false"), ToolAction.GetActionCheckState.Execute(ToolMenuContext), ECheckBoxState::Unchecked);
	TestEqual(TEXT("MenuExtensions metadata test should evaluate CanExecute on both UI paths"), UAngelscriptEditorMenuExtensionActionTestShim::GetCanExecuteEvaluationCount(), 2);
	TestEqual(TEXT("MenuExtensions metadata test should evaluate visibility on both UI paths"), UAngelscriptEditorMenuExtensionActionTestShim::GetVisibleEvaluationCount(), 2);
	TestEqual(TEXT("MenuExtensions metadata test should evaluate checked state on both UI paths"), UAngelscriptEditorMenuExtensionActionTestShim::GetCheckedEvaluationCount(), 2);

	UAngelscriptEditorMenuExtensionActionTestShim::ResetRecordedState();
	UAngelscriptEditorMenuExtensionActionTestShim::ConfigureActionResponses(true, true, true);
	TestTrue(TEXT("MenuExtensions UI actions should become executable when ActionCanExecute returns true"), UIAction.CanExecute());
	TestEqual(TEXT("MenuExtensions UI actions should become visible when ActionIsVisible returns true"), UIAction.IsVisible(), EVisibility::Visible);
	TestEqual(TEXT("MenuExtensions UI actions should report checked when ActionIsChecked returns true"), UIAction.GetCheckState(), ECheckBoxState::Checked);
	TestTrue(TEXT("MenuExtensions tool actions should become executable when ActionCanExecute returns true"), ToolAction.CanExecuteAction.Execute(ToolMenuContext));
	TestTrue(TEXT("MenuExtensions tool actions should become visible when ActionIsVisible returns true"), ToolAction.IsActionVisibleDelegate.Execute(ToolMenuContext));
	TestEqual(TEXT("MenuExtensions tool actions should report checked when ActionIsChecked returns true"), ToolAction.GetActionCheckState.Execute(ToolMenuContext), ECheckBoxState::Checked);
	TestEqual(TEXT("MenuExtensions metadata test should evaluate CanExecute twice in the enabled phase"), UAngelscriptEditorMenuExtensionActionTestShim::GetCanExecuteEvaluationCount(), 2);
	TestEqual(TEXT("MenuExtensions metadata test should evaluate visibility twice in the enabled phase"), UAngelscriptEditorMenuExtensionActionTestShim::GetVisibleEvaluationCount(), 2);
	TestEqual(TEXT("MenuExtensions metadata test should evaluate checked twice in the enabled phase"), UAngelscriptEditorMenuExtensionActionTestShim::GetCheckedEvaluationCount(), 2);

	int32 NavigateToFunctionCount = 0;
	int32 NavigateToClassCount = 0;
	const UFunction* LastNavigatedFunction = nullptr;
	const UClass* LastNavigatedClass = nullptr;
	FAngelscriptEditorMenuExtensionTestAccess::SetNavigateToFunctionOverride([&NavigateToFunctionCount, &LastNavigatedFunction](const UFunction* InFunction)
	{
		++NavigateToFunctionCount;
		LastNavigatedFunction = InFunction;
		return true;
	});
	FAngelscriptEditorMenuExtensionTestAccess::SetNavigateToClassOverride([&NavigateToClassCount, &LastNavigatedClass](const UClass* InClass)
	{
		++NavigateToClassCount;
		LastNavigatedClass = InClass;
	});

	FAngelscriptEditorMenuExtensionTestAccess::SetShiftModifierOverride([]()
	{
		return false;
	});

	TestTrue(TEXT("MenuExtensions UI actions should execute the prompt path on normal click"), UIAction.Execute());
	ToolAction.ExecuteAction.Execute(ToolMenuContext);
	TestEqual(TEXT("MenuExtensions normal execution should invoke the prompt path for both UI actions"), UAngelscriptEditorMenuExtensionActionTestShim::GetPromptInvocationCount(), 2);
	TestEqual(TEXT("MenuExtensions normal execution should target the clicked function"), UAngelscriptEditorMenuExtensionActionTestShim::GetLastPromptFunctionName(), MetadataFunction->GetFName());
	TestEqual(TEXT("MenuExtensions normal execution should not navigate to source"), NavigateToFunctionCount, 0);
	TestEqual(TEXT("MenuExtensions normal execution should not navigate to class"), NavigateToClassCount, 0);

	FAngelscriptEditorMenuExtensionTestAccess::SetShiftModifierOverride([]()
	{
		return true;
	});

	TestTrue(TEXT("MenuExtensions UI actions should execute the shift-navigation branch"), UIAction.Execute());
	TestEqual(TEXT("MenuExtensions shift UI action should navigate to the function first"), NavigateToFunctionCount, 1);
	TestEqual(TEXT("MenuExtensions shift UI action should not fall back to class when function navigation succeeds"), NavigateToClassCount, 0);
	TestEqual(TEXT("MenuExtensions shift UI action should not trigger the prompt path"), UAngelscriptEditorMenuExtensionActionTestShim::GetPromptInvocationCount(), 2);
	TestTrue(TEXT("MenuExtensions shift UI action should navigate the requested function"), LastNavigatedFunction == MetadataFunction);

	FAngelscriptEditorMenuExtensionTestAccess::SetNavigateToFunctionOverride([&NavigateToFunctionCount, &LastNavigatedFunction](const UFunction* InFunction)
	{
		++NavigateToFunctionCount;
		LastNavigatedFunction = InFunction;
		return false;
	});

	ToolAction.ExecuteAction.Execute(ToolMenuContext);
	TestEqual(TEXT("MenuExtensions shift tool action should still try function navigation before fallback"), NavigateToFunctionCount, 2);
	TestEqual(TEXT("MenuExtensions shift tool action should fall back to class when function navigation fails"), NavigateToClassCount, 1);
	TestEqual(TEXT("MenuExtensions shift tool action should still avoid the prompt path"), UAngelscriptEditorMenuExtensionActionTestShim::GetPromptInvocationCount(), 2);
	TestTrue(TEXT("MenuExtensions shift tool action should fall back to the owning extension class"), LastNavigatedClass == Extension->GetClass());

	return true;
}

#undef TestTrue
#undef TestFalse
#undef TestEqual
#undef TestNotNull

TEST_CLASS_WITH_FLAGS(
	FAngelscriptEditorMenuExtensionsTests,
	"Angelscript.Editor.MenuExtensions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(RegisterFunctionsAndForwardFilteredSelection)
	{
		ASSERT_THAT(IsTrue(RunRegisterFunctionsAndForwardFilteredSelection(*TestRunner)));
	}

	TEST_METHOD(ActionMetadataDelegatesAndShiftNavigationFallback)
	{
		ASSERT_THAT(IsTrue(RunActionMetadataDelegatesAndShiftNavigationFallback(*TestRunner)));
	}
};

#endif
