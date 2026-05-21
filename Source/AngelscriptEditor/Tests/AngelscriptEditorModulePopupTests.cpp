#include "Core/AngelscriptEditorModule.h"

#include "ClassGenerator/ASClass.h"
#include "Core/AngelscriptRuntimeModule.h"
#include "AngelscriptEngine.h"

#include "AssetRegistry/AssetData.h"
#include "Curves/CurveFloat.h"
#include "Engine/Blueprint.h"
#include "Engine/DataAsset.h"
#include "HAL/FileManager.h"
#include "IContentBrowserSingleton.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Preprocessor/AngelscriptPreprocessor.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptEditorModuleShowAssetListPopupTest,
	"Angelscript.Editor.Module.ShowAssetListPopupHonorsInitGateAndBuildsExpectedOpenFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace AngelscriptEditor_Private_Tests_AngelscriptEditorModulePopupTests_Private
{
	struct FAssetListPopupCallLog
	{
		int32 ForceEditorWindowToFrontCalls = 0;
		TArray<FString> OpenedAssetPaths;
		TArray<TWeakObjectPtr<UObject>> OpenedAssetObjects;
		int32 PopupCalls = 0;
		TArray<FString> PopupFilterPackageNames;
		UASClass* PopupBaseClass = nullptr;
		EAssetViewType::Type PopupInitialAssetViewType = EAssetViewType::Tile;
		bool bAllowNullSelection = true;
		bool bShowBottomToolbar = false;
		bool bAutohideSearchBar = true;
		bool bCanShowClasses = true;
		bool bAddFilterUI = false;
		FString PopupSaveSettingsName;
		bool bInvokedDoubleClick = false;
		bool bInvokedEnterPressed = false;

		void Reset()
		{
			ForceEditorWindowToFrontCalls = 0;
			OpenedAssetPaths.Reset();
			OpenedAssetObjects.Reset();
			PopupCalls = 0;
			PopupFilterPackageNames.Reset();
			PopupBaseClass = nullptr;
			PopupInitialAssetViewType = EAssetViewType::Tile;
			bAllowNullSelection = true;
			bShowBottomToolbar = false;
			bAutohideSearchBar = true;
			bCanShowClasses = true;
			bAddFilterUI = false;
			PopupSaveSettingsName.Reset();
			bInvokedDoubleClick = false;
			bInvokedEnterPressed = false;
		}
	};

	TUniquePtr<FAngelscriptEngine> MakePopupTestEngine()
	{
		FAngelscriptEngineConfig Config;
		Config.bSkipInitialCompile = true;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		return FAngelscriptEngine::Create(Config, Dependencies);
	}

	UCurveFloat* CreatePopupTestAsset(
		FAutomationTestBase& Test,
		const TCHAR* PackageBaseName,
		FString& OutPackageName,
		UPackage*& OutPackage)
	{
		const FString UniqueSuffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		OutPackageName = FString::Printf(TEXT("/Temp/%s_%s"), PackageBaseName, *UniqueSuffix);
		OutPackage = CreatePackage(*OutPackageName);
		if (!Test.TestNotNull(
			*FString::Printf(TEXT("ShowAssetListPopup test should create package '%s'"), *OutPackageName),
			OutPackage))
		{
			return nullptr;
		}

		const FName AssetName(*FString::Printf(TEXT("%sAsset"), PackageBaseName));
		return NewObject<UCurveFloat>(OutPackage, AssetName, RF_Public | RF_Standalone | RF_Transient);
	}

	void CleanupPopupTestObject(UObject*& Object)
	{
		if (Object == nullptr)
		{
			return;
		}

		Object->ClearFlags(RF_Public | RF_Standalone);
		Object->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_NonTransactional);
		Object->MarkAsGarbage();
		Object = nullptr;
	}

	void CleanupPopupTestPackage(UPackage*& Package)
	{
		if (Package == nullptr)
		{
			return;
		}

		Package->MarkAsGarbage();
		Package = nullptr;
	}

	struct FScopedAutomaticImportsOverride
	{
		explicit FScopedAutomaticImportsOverride(asIScriptEngine* InScriptEngine)
			: ScriptEngine(InScriptEngine)
			, PreviousValue(InScriptEngine != nullptr ? InScriptEngine->GetEngineProperty(asEP_AUTOMATIC_IMPORTS) : 0)
		{
			if (ScriptEngine != nullptr)
			{
				ScriptEngine->SetEngineProperty(asEP_AUTOMATIC_IMPORTS, 0);
			}
		}

		~FScopedAutomaticImportsOverride()
		{
			if (ScriptEngine != nullptr)
			{
				ScriptEngine->SetEngineProperty(asEP_AUTOMATIC_IMPORTS, PreviousValue);
			}
		}

		asIScriptEngine* ScriptEngine = nullptr;
		asPWORD PreviousValue = 0;
	};

	bool CompilePopupScriptModule(FAutomationTestBase& Test, FAngelscriptEngine& Engine, const FString& ModuleName, const FString& ScriptSource)
	{
		const FString RelativeFilename = FString::Printf(TEXT("%s_%s.as"), *ModuleName, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
		const FString AbsoluteFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), RelativeFilename);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(AbsoluteFilename), true);
		if (!FFileHelper::SaveStringToFile(ScriptSource, *AbsoluteFilename))
		{
			Test.AddError(FString::Printf(TEXT("ShowCreateBlueprintPopup test should write script file '%s'"), *AbsoluteFilename));
			return false;
		}

		FAngelscriptEngineScope EngineScope(Engine);
		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddFile(RelativeFilename, AbsoluteFilename);
		if (!Preprocessor.Preprocess())
		{
			Test.AddError(FString::Printf(TEXT("ShowCreateBlueprintPopup test failed to preprocess popup script: %s"), *Engine.FormatDiagnostics()));
			return false;
		}

		TArray<TSharedRef<FAngelscriptModuleDesc>> ModulesToCompile = Preprocessor.GetModulesToCompile();
		TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
		TGuardValue<bool> AutomaticImportGuard(Engine.bUseAutomaticImportMethod, false);
		FScopedAutomaticImportsOverride AutomaticImportsOverride(Engine.GetScriptEngine());
		const ECompileResult CompileResult = Engine.CompileModules(ECompileType::Initial, ModulesToCompile, CompiledModules);
		if (CompileResult == ECompileResult::Error || CompileResult == ECompileResult::ErrorNeedFullReload)
		{
			Test.AddError(FString::Printf(TEXT("ShowCreateBlueprintPopup test failed to compile popup script: %s"), *Engine.FormatDiagnostics()));
			return false;
		}

		return Test.TestTrue(TEXT("ShowCreateBlueprintPopup test should compile at least one preprocessed module"), CompiledModules.Num() > 0);
	}

	UASClass* FindPopupScriptClass(FAutomationTestBase& Test, FAngelscriptEngine& Engine, const TCHAR* ClassName)
	{
		const TSharedPtr<FAngelscriptClassDesc> ClassDesc = Engine.GetClass(ClassName);
		UASClass* ScriptClass = ClassDesc.IsValid() ? Cast<UASClass>(ClassDesc->Class) : nullptr;
		Test.TestNotNull(*FString::Printf(TEXT("ShowCreateBlueprintPopup test should generate script class '%s'"), ClassName), ScriptClass);
		return ScriptClass;
	}

	struct FCreateBlueprintPopupCallLog
	{
		int32 ForceEditorWindowToFrontCalls = 0;
		int32 SaveDialogCalls = 0;
		TArray<TWeakObjectPtr<UObject>> AssetCreatedObjects;
		int32 PromptForCheckoutAndSaveCalls = 0;
		TArray<FString> PromptPackageNames;
		TArray<TWeakObjectPtr<UObject>> OpenedAssetObjects;

		void Reset()
		{
			ForceEditorWindowToFrontCalls = 0;
			SaveDialogCalls = 0;
			AssetCreatedObjects.Reset();
			PromptForCheckoutAndSaveCalls = 0;
			PromptPackageNames.Reset();
			OpenedAssetObjects.Reset();
		}
	};
}


bool FAngelscriptEditorModuleShowAssetListPopupTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptEditorModulePopupTests_Private;
	FAssetListPopupCallLog CallLog;
	TArray<FAngelscriptEngine*> SavedStack = FAngelscriptEngineContextStack::SnapshotAndClear();
	TUniquePtr<FAngelscriptEngine> Engine = MakePopupTestEngine();
	TUniquePtr<FAngelscriptEngineScope> EngineScope;
	UPackage* FirstPackage = nullptr;
	UPackage* SecondPackage = nullptr;
	FString FirstPackageName;
	FString SecondPackageName;
	UObject* FirstAssetObject = CreatePopupTestAsset(*this, TEXT("AssetListPopupFirst"), FirstPackageName, FirstPackage);
	UObject* SecondAssetObject = CreatePopupTestAsset(*this, TEXT("AssetListPopupSecond"), SecondPackageName, SecondPackage);
	const FAssetData FirstAssetData(FirstAssetObject);
	const FAssetData SecondAssetData(SecondAssetObject);
	UASClass* const BaseClassMarker = reinterpret_cast<UASClass*>(static_cast<UPTRINT>(1));

	if (!TestNotNull(TEXT("ShowAssetListPopup test should create a testing engine"), Engine.Get())
		|| !TestNotNull(TEXT("ShowAssetListPopup test should create the first popup asset"), FirstAssetObject)
		|| !TestNotNull(TEXT("ShowAssetListPopup test should create the second popup asset"), SecondAssetObject))
	{
		return false;
	}

	FAngelscriptEditorModuleAssetListPopupTestHooks Hooks;
	Hooks.ForceEditorWindowToFront = [&CallLog]()
	{
		++CallLog.ForceEditorWindowToFrontCalls;
	};
	Hooks.OpenAssetByPath = [&CallLog](const FString& AssetPath)
	{
		CallLog.OpenedAssetPaths.Add(AssetPath);
	};
	Hooks.OpenAssetByObject = [&CallLog](UObject* AssetObject)
	{
		CallLog.OpenedAssetObjects.Add(AssetObject);
	};
	Hooks.ShowAssetPickerMenu = [&CallLog, FirstAssetData, SecondAssetData](const FAssetPickerConfig& AssetPickerConfig, UASClass* BaseClass)
	{
		++CallLog.PopupCalls;
		CallLog.PopupBaseClass = BaseClass;
		CallLog.PopupInitialAssetViewType = AssetPickerConfig.InitialAssetViewType;
		CallLog.bAllowNullSelection = AssetPickerConfig.bAllowNullSelection;
		CallLog.bShowBottomToolbar = AssetPickerConfig.bShowBottomToolbar;
		CallLog.bAutohideSearchBar = AssetPickerConfig.bAutohideSearchBar;
		CallLog.bCanShowClasses = AssetPickerConfig.bCanShowClasses;
		CallLog.bAddFilterUI = AssetPickerConfig.bAddFilterUI;
		CallLog.PopupSaveSettingsName = AssetPickerConfig.SaveSettingsName;

		for (const FName PackageName : AssetPickerConfig.Filter.PackageNames)
		{
			CallLog.PopupFilterPackageNames.Add(PackageName.ToString());
		}

		if (AssetPickerConfig.OnAssetDoubleClicked.IsBound())
		{
			CallLog.bInvokedDoubleClick = true;
			AssetPickerConfig.OnAssetDoubleClicked.Execute(FirstAssetData);
		}

		if (AssetPickerConfig.OnAssetEnterPressed.IsBound())
		{
			CallLog.bInvokedEnterPressed = true;
			TArray<FAssetData> SelectedAssets;
			SelectedAssets.Add(SecondAssetData);
			AssetPickerConfig.OnAssetEnterPressed.Execute(SelectedAssets);
		}
	};

	FAngelscriptEditorModuleTestAccess::SetAssetListPopupTestHooks(MoveTemp(Hooks));

	ON_SCOPE_EXIT
	{
		FAngelscriptEditorModuleTestAccess::ResetAssetListPopupTestHooks();
		EngineScope.Reset();
		FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(SavedStack));
		CleanupPopupTestObject(FirstAssetObject);
		CleanupPopupTestObject(SecondAssetObject);
		CleanupPopupTestPackage(FirstPackage);
		CleanupPopupTestPackage(SecondPackage);
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	};

	CallLog.Reset();
	FAngelscriptEditorModule::ShowAssetListPopup({ FirstPackageName }, nullptr);
	if (!TestEqual(TEXT("ShowAssetListPopup should not bring any editor window to front before the engine is initialized"), CallLog.ForceEditorWindowToFrontCalls, 0)
		|| !TestEqual(TEXT("ShowAssetListPopup should not open a single asset before the engine is initialized"), CallLog.OpenedAssetPaths.Num(), 0)
		|| !TestEqual(TEXT("ShowAssetListPopup should not build a popup before the engine is initialized"), CallLog.PopupCalls, 0))
	{
		return false;
	}

	EngineScope = MakeUnique<FAngelscriptEngineScope>(*Engine);
	Engine->bIsInitialCompileFinished = false;

	CallLog.Reset();
	FAngelscriptEditorModule::ShowAssetListPopup({ FirstPackageName }, nullptr);
	if (!TestEqual(TEXT("ShowAssetListPopup should not bring any editor window to front before initial compile finishes"), CallLog.ForceEditorWindowToFrontCalls, 0)
		|| !TestEqual(TEXT("ShowAssetListPopup should not open a single asset before initial compile finishes"), CallLog.OpenedAssetPaths.Num(), 0)
		|| !TestEqual(TEXT("ShowAssetListPopup should not build a popup before initial compile finishes"), CallLog.PopupCalls, 0))
	{
		return false;
	}

	Engine->bIsInitialCompileFinished = true;

	CallLog.Reset();
	FAngelscriptEditorModule::ShowAssetListPopup({ FirstPackageName }, nullptr);
	if (!TestEqual(TEXT("ShowAssetListPopup should bring an editor window to front once for a single asset"), CallLog.ForceEditorWindowToFrontCalls, 1)
		|| !TestEqual(TEXT("ShowAssetListPopup should open exactly one single asset path when only one asset is requested"), CallLog.OpenedAssetPaths.Num(), 1)
		|| !TestEqual(TEXT("ShowAssetListPopup should not build a popup when only one asset is requested"), CallLog.PopupCalls, 0))
	{
		return false;
	}

	if (!TestEqual(TEXT("ShowAssetListPopup should forward the requested single asset path"), CallLog.OpenedAssetPaths[0], FirstPackageName))
	{
		return false;
	}

	CallLog.Reset();
	FAngelscriptEditorModule::ShowAssetListPopup({ FirstPackageName, SecondPackageName }, BaseClassMarker);
	if (!TestEqual(TEXT("ShowAssetListPopup should bring an editor window to front once for popup flow"), CallLog.ForceEditorWindowToFrontCalls, 1)
		|| !TestEqual(TEXT("ShowAssetListPopup should build exactly one popup for multiple assets"), CallLog.PopupCalls, 1)
		|| !TestEqual(TEXT("ShowAssetListPopup should not direct-open asset paths in popup flow"), CallLog.OpenedAssetPaths.Num(), 0)
		|| !TestEqual(TEXT("ShowAssetListPopup should open two objects when popup delegates are exercised"), CallLog.OpenedAssetObjects.Num(), 2))
	{
		return false;
	}

	if (!TestTrue(TEXT("ShowAssetListPopup should wire the popup double-click delegate"), CallLog.bInvokedDoubleClick)
		|| !TestTrue(TEXT("ShowAssetListPopup should wire the popup enter-pressed delegate"), CallLog.bInvokedEnterPressed)
		|| !TestEqual(TEXT("ShowAssetListPopup should pass the popup base class through the menu builder"), CallLog.PopupBaseClass, BaseClassMarker))
	{
		return false;
	}

	if (!TestEqual(TEXT("ShowAssetListPopup should configure popup filter package count to match the requested assets"), CallLog.PopupFilterPackageNames.Num(), 2)
		|| !TestEqual(TEXT("ShowAssetListPopup should preserve the first popup filter package name"), CallLog.PopupFilterPackageNames[0], FirstPackageName)
		|| !TestEqual(TEXT("ShowAssetListPopup should preserve the second popup filter package name"), CallLog.PopupFilterPackageNames[1], SecondPackageName))
	{
		return false;
	}

	if (!TestEqual(TEXT("ShowAssetListPopup should use list view for popup asset picker"), CallLog.PopupInitialAssetViewType, EAssetViewType::List)
		|| !TestFalse(TEXT("ShowAssetListPopup should disallow null selection in popup asset picker"), CallLog.bAllowNullSelection)
		|| !TestTrue(TEXT("ShowAssetListPopup should show the bottom toolbar in popup asset picker"), CallLog.bShowBottomToolbar)
		|| !TestFalse(TEXT("ShowAssetListPopup should keep the search bar visible in popup asset picker"), CallLog.bAutohideSearchBar)
		|| !TestFalse(TEXT("ShowAssetListPopup should hide classes in popup asset picker"), CallLog.bCanShowClasses)
		|| !TestTrue(TEXT("ShowAssetListPopup should enable filter UI in popup asset picker"), CallLog.bAddFilterUI)
		|| !TestEqual(TEXT("ShowAssetListPopup should use the shared popup save-settings key"), CallLog.PopupSaveSettingsName, FString(TEXT("GlobalAssetPicker"))))
	{
		return false;
	}

	if (!TestEqual(TEXT("ShowAssetListPopup should open the object resolved from double click first"), CallLog.OpenedAssetObjects[0].Get(), FirstAssetObject)
		|| !TestEqual(TEXT("ShowAssetListPopup should open the object resolved from enter pressed second"), CallLog.OpenedAssetObjects[1].Get(), SecondAssetObject))
	{
		return false;
	}

	if (!TestTrue(TEXT("ShowAssetListPopup should expose the create button when a base class is present"), FAngelscriptEditorModuleTestAccess::ShouldShowAssetListPopupCreateButton(BaseClassMarker))
		|| !TestFalse(TEXT("ShowAssetListPopup should collapse the create button when no base class is provided"), FAngelscriptEditorModuleTestAccess::ShouldShowAssetListPopupCreateButton(nullptr)))
	{
		return false;
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptEditorModuleShowCreateBlueprintPopupTest,
	"Angelscript.TestModule.Editor.Module.ShowCreateBlueprintPopupCreatesExpectedAssetAtDialogPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptEditorModuleShowCreateBlueprintPopupTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptEditorModulePopupTests_Private;
	FCreateBlueprintPopupCallLog CallLog;
	TArray<FAngelscriptEngine*> SavedStack = FAngelscriptEngineContextStack::SnapshotAndClear();
	TUniquePtr<FAngelscriptEngine> Engine = MakePopupTestEngine();
	TUniquePtr<FAngelscriptEngineScope> EngineScope;
	TSet<UObject*> AssetsToCleanup;
	TSet<UPackage*> PackagesToCleanup;
	const FString UniqueSuffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString BlueprintObjectPath = FString::Printf(TEXT("/Game/Automation/PopupBlueprint_%s.PopupBlueprint_%s"), *UniqueSuffix, *UniqueSuffix);
	const FString DataAssetObjectPath = FString::Printf(TEXT("/Game/Automation/PopupDataAsset_%s.PopupDataAsset_%s"), *UniqueSuffix, *UniqueSuffix);
	TArray<FString> DialogResponses = { BlueprintObjectPath, DataAssetObjectPath, FString() };
	int32 DialogResponseIndex = 0;

	if (!TestNotNull(TEXT("ShowCreateBlueprintPopup test should create a testing engine"), Engine.Get()))
	{
		return false;
	}

	EngineScope = MakeUnique<FAngelscriptEngineScope>(*Engine);

	const FString PopupScript = TEXT(R"AS(UCLASS() class APopupCreateBlueprintScript : AActor {} UCLASS() class UPopupCreateDataAssetScript : UDataAsset {})AS");
	if (!CompilePopupScriptModule(*this, *Engine, TEXT("ASEditorCreateBlueprintPopup"), PopupScript))
	{
		return false;
	}

	UASClass* const BlueprintScriptClass = FindPopupScriptClass(*this, *Engine, TEXT("APopupCreateBlueprintScript"));
	UASClass* const DataAssetScriptClass = FindPopupScriptClass(*this, *Engine, TEXT("UPopupCreateDataAssetScript"));
	if (BlueprintScriptClass == nullptr || DataAssetScriptClass == nullptr)
	{
		return false;
	}

	FAngelscriptEditorGetCreateBlueprintDefaultAssetPath SavedDefaultPathDelegate = FAngelscriptRuntimeModule::GetEditorGetCreateBlueprintDefaultAssetPath();
	FAngelscriptRuntimeModule::GetEditorGetCreateBlueprintDefaultAssetPath().Unbind();

	FAngelscriptEditorModuleCreateBlueprintPopupTestHooks Hooks;
	Hooks.ForceEditorWindowToFront = [&CallLog]()
	{
		++CallLog.ForceEditorWindowToFrontCalls;
	};
	Hooks.CreateSaveAssetDialog = [&CallLog, &DialogResponses, &DialogResponseIndex](const FSaveAssetDialogConfig&)
	{
		++CallLog.SaveDialogCalls;
		return DialogResponses.IsValidIndex(DialogResponseIndex) ? DialogResponses[DialogResponseIndex++] : FString();
	};
	Hooks.AssetCreated = [&CallLog, &AssetsToCleanup, &PackagesToCleanup](UObject* Asset)
	{
		CallLog.AssetCreatedObjects.Add(Asset);
		if (Asset != nullptr)
		{
			AssetsToCleanup.Add(Asset);
			PackagesToCleanup.Add(Asset->GetOutermost());
		}
	};
	Hooks.PromptForCheckoutAndSave = [&CallLog](const TArray<UPackage*>& Packages)
	{
		++CallLog.PromptForCheckoutAndSaveCalls;
		for (UPackage* Package : Packages)
		{
			CallLog.PromptPackageNames.Add(Package != nullptr ? Package->GetName() : FString());
		}
	};
	Hooks.OpenEditorForAsset = [&CallLog, &AssetsToCleanup, &PackagesToCleanup](UObject* Asset)
	{
		CallLog.OpenedAssetObjects.Add(Asset);
		if (Asset != nullptr)
		{
			AssetsToCleanup.Add(Asset);
			PackagesToCleanup.Add(Asset->GetOutermost());
		}
	};
	FAngelscriptEditorModuleTestAccess::SetCreateBlueprintPopupTestHooks(MoveTemp(Hooks));

	ON_SCOPE_EXIT
	{
		FAngelscriptEditorModuleTestAccess::ResetCreateBlueprintPopupTestHooks();
		FAngelscriptRuntimeModule::GetEditorGetCreateBlueprintDefaultAssetPath() = SavedDefaultPathDelegate;
		EngineScope.Reset();
		FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(SavedStack));
		for (UObject* Asset : AssetsToCleanup)
		{
			UObject* MutableAsset = Asset;
			CleanupPopupTestObject(MutableAsset);
		}
		for (UPackage* Package : PackagesToCleanup)
		{
			UPackage* MutablePackage = Package;
			CleanupPopupTestPackage(MutablePackage);
		}
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	};

	CallLog.Reset();
	FAngelscriptEditorModule::ShowCreateBlueprintPopup(BlueprintScriptClass);
	if (!TestEqual(TEXT("ShowCreateBlueprintPopup should bring an editor window to front for blueprint creation"), CallLog.ForceEditorWindowToFrontCalls, 1)
		|| !TestEqual(TEXT("ShowCreateBlueprintPopup should open the save dialog once for blueprint creation"), CallLog.SaveDialogCalls, 1)
		|| !TestEqual(TEXT("ShowCreateBlueprintPopup should notify asset creation once for blueprint creation"), CallLog.AssetCreatedObjects.Num(), 1)
		|| !TestEqual(TEXT("ShowCreateBlueprintPopup should prompt package save once for blueprint creation"), CallLog.PromptForCheckoutAndSaveCalls, 1)
		|| !TestEqual(TEXT("ShowCreateBlueprintPopup should open exactly one created blueprint asset"), CallLog.OpenedAssetObjects.Num(), 1))
	{
		return false;
	}

	UBlueprint* CreatedBlueprint = Cast<UBlueprint>(CallLog.AssetCreatedObjects[0].Get());
	if (!TestNotNull(TEXT("ShowCreateBlueprintPopup should create a UBlueprint for non-data-asset script classes"), CreatedBlueprint)
		|| !TestEqual(TEXT("ShowCreateBlueprintPopup should create the blueprint at the dialog-selected object path"), CreatedBlueprint->GetPathName(), BlueprintObjectPath)
		|| !TestEqual(TEXT("ShowCreateBlueprintPopup should set the created blueprint parent class to the script class"), CreatedBlueprint->ParentClass.Get(), static_cast<UClass*>(BlueprintScriptClass))
		|| !TestEqual(TEXT("ShowCreateBlueprintPopup should open the same created blueprint asset in the editor"), CallLog.OpenedAssetObjects[0].Get(), static_cast<UObject*>(CreatedBlueprint))
		|| !TestEqual(TEXT("ShowCreateBlueprintPopup should prompt save for the created blueprint package"), CallLog.PromptPackageNames[0], FPackageName::ObjectPathToPackageName(BlueprintObjectPath)))
	{
		return false;
	}

	CallLog.Reset();
	FAngelscriptEditorModule::ShowCreateBlueprintPopup(DataAssetScriptClass);
	if (!TestEqual(TEXT("ShowCreateBlueprintPopup should bring an editor window to front for data-asset creation"), CallLog.ForceEditorWindowToFrontCalls, 1)
		|| !TestEqual(TEXT("ShowCreateBlueprintPopup should open the save dialog once for data-asset creation"), CallLog.SaveDialogCalls, 1)
		|| !TestEqual(TEXT("ShowCreateBlueprintPopup should notify asset creation once for data-asset creation"), CallLog.AssetCreatedObjects.Num(), 1)
		|| !TestEqual(TEXT("ShowCreateBlueprintPopup should prompt package save once for data-asset creation"), CallLog.PromptForCheckoutAndSaveCalls, 1)
		|| !TestEqual(TEXT("ShowCreateBlueprintPopup should open exactly one created data asset"), CallLog.OpenedAssetObjects.Num(), 1))
	{
		return false;
	}

	UDataAsset* CreatedDataAsset = Cast<UDataAsset>(CallLog.AssetCreatedObjects[0].Get());
	if (!TestNotNull(TEXT("ShowCreateBlueprintPopup should create a UDataAsset for data-asset script classes"), CreatedDataAsset)
		|| !TestEqual(TEXT("ShowCreateBlueprintPopup should create the data asset at the dialog-selected object path"), CreatedDataAsset->GetPathName(), DataAssetObjectPath)
		|| !TestEqual(TEXT("ShowCreateBlueprintPopup should instantiate the requested script data-asset class"), CreatedDataAsset->GetClass(), static_cast<UClass*>(DataAssetScriptClass))
		|| !TestEqual(TEXT("ShowCreateBlueprintPopup should open the same created data asset in the editor"), CallLog.OpenedAssetObjects[0].Get(), static_cast<UObject*>(CreatedDataAsset))
		|| !TestEqual(TEXT("ShowCreateBlueprintPopup should prompt save for the created data-asset package"), CallLog.PromptPackageNames[0], FPackageName::ObjectPathToPackageName(DataAssetObjectPath)))
	{
		return false;
	}

	CallLog.Reset();
	FAngelscriptEditorModule::ShowCreateBlueprintPopup(BlueprintScriptClass);
	if (!TestEqual(TEXT("ShowCreateBlueprintPopup should still bring an editor window to front before a cancelled dialog"), CallLog.ForceEditorWindowToFrontCalls, 1)
		|| !TestEqual(TEXT("ShowCreateBlueprintPopup should still open the save dialog once for the cancelled path"), CallLog.SaveDialogCalls, 1)
		|| !TestEqual(TEXT("ShowCreateBlueprintPopup should not create any asset when the save dialog returns an empty object path"), CallLog.AssetCreatedObjects.Num(), 0)
		|| !TestEqual(TEXT("ShowCreateBlueprintPopup should not prompt package save when the save dialog returns an empty object path"), CallLog.PromptForCheckoutAndSaveCalls, 0)
		|| !TestEqual(TEXT("ShowCreateBlueprintPopup should not open any asset editor when the save dialog returns an empty object path"), CallLog.OpenedAssetObjects.Num(), 0))
	{
		return false;
	}

	return true;
}

#endif
