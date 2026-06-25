#include "CQTest.h"
#include "Core/AngelscriptEditorModule.h"

#include "ClassGenerator/ASClass.h"
#include "Core/AngelscriptEditorDebugBridge.h"
#include "AngelscriptEngine.h"

#include "Engine/DataAsset.h"
#include "HAL/FileManager.h"
#include "IContentBrowserSingleton.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Preprocessor/AngelscriptPreprocessor.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptEditor_Private_Tests_AngelscriptEditorModuleAssetCreationTests_Private
{
	struct FCreateBlueprintPopupDefaultsCallLog
	{
		int32 ForceEditorWindowToFrontCalls = 0;
		int32 SaveDialogCalls = 0;
		int32 HasAssetsCalls = 0;
		TArray<FString> HasAssetsPaths;
		TArray<FString> DialogDefaultPaths;
		TArray<FString> DialogDefaultAssetNames;

		void Reset()
		{
			ForceEditorWindowToFrontCalls = 0;
			SaveDialogCalls = 0;
			HasAssetsCalls = 0;
			HasAssetsPaths.Reset();
			DialogDefaultPaths.Reset();
			DialogDefaultAssetNames.Reset();
		}
	};

	struct FCreateBlueprintPopupInvalidObjectPathCallLog
	{
		int32 ForceEditorWindowToFrontCalls = 0;
		int32 SaveDialogCalls = 0;
		int32 MessageDialogCalls = 0;
		int32 AssetCreatedCalls = 0;
		int32 PromptForCheckoutAndSaveCalls = 0;
		int32 OpenEditorForAssetCalls = 0;
		TArray<FString> MessageDialogTexts;

		void Reset()
		{
			ForceEditorWindowToFrontCalls = 0;
			SaveDialogCalls = 0;
			MessageDialogCalls = 0;
			AssetCreatedCalls = 0;
			PromptForCheckoutAndSaveCalls = 0;
			OpenEditorForAssetCalls = 0;
			MessageDialogTexts.Reset();
		}
	};

	struct FCreateBlueprintPopupFactoryFailureCallLog
	{
		int32 ForceEditorWindowToFrontCalls = 0;
		int32 SaveDialogCalls = 0;
		int32 BlueprintFactoryCalls = 0;
		int32 AssetCreatedCalls = 0;
		int32 PromptForCheckoutAndSaveCalls = 0;
		int32 OpenEditorForAssetCalls = 0;

		void Reset()
		{
			ForceEditorWindowToFrontCalls = 0;
			SaveDialogCalls = 0;
			BlueprintFactoryCalls = 0;
			AssetCreatedCalls = 0;
			PromptForCheckoutAndSaveCalls = 0;
			OpenEditorForAssetCalls = 0;
		}
	};

	TUniquePtr<FAngelscriptEngine> MakePopupAssetCreationTestEngine()
	{
		FAngelscriptEngineConfig Config;
		Config.bSkipInitialCompile = true;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		return FAngelscriptEngine::Create(Config, Dependencies);
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

	bool CompilePopupScriptModuleWithRelativePath(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FString& ModuleName,
		const FString& RelativeFilename,
		const FString& ScriptSource)
	{
		const FString AbsoluteFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), RelativeFilename);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(AbsoluteFilename), true);
		if (!FFileHelper::SaveStringToFile(ScriptSource, *AbsoluteFilename))
		{
			Test.AddError(FString::Printf(TEXT("ShowCreateBlueprintPopup defaults test should write script file '%s'"), *AbsoluteFilename));
			return false;
		}

		FAngelscriptEngineScope EngineScope(Engine);
		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddFile(RelativeFilename, AbsoluteFilename);
		if (!Preprocessor.Preprocess())
		{
			Test.AddError(FString::Printf(TEXT("ShowCreateBlueprintPopup defaults test failed to preprocess popup script: %s"), *Engine.FormatDiagnostics()));
			return false;
		}

		TArray<TSharedRef<FAngelscriptModuleDesc>> ModulesToCompile = Preprocessor.GetModulesToCompile();
		TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
		TGuardValue<bool> AutomaticImportGuard(Engine.bUseAutomaticImportMethod, false);
		FScopedAutomaticImportsOverride AutomaticImportsOverride(Engine.GetScriptEngine());
		const ECompileResult CompileResult = Engine.CompileModules(ECompileType::Initial, ModulesToCompile, CompiledModules);
		if (CompileResult == ECompileResult::Error || CompileResult == ECompileResult::ErrorNeedFullReload)
		{
			Test.AddError(FString::Printf(TEXT("ShowCreateBlueprintPopup defaults test failed to compile popup script: %s"), *Engine.FormatDiagnostics()));
			return false;
		}

		return Test.TestTrue(TEXT("ShowCreateBlueprintPopup defaults test should compile at least one popup module"), CompiledModules.Num() > 0);
	}

	UASClass* FindPopupScriptClass(FAutomationTestBase& Test, FAngelscriptEngine& Engine, const TCHAR* ClassName)
	{
		const TSharedPtr<FAngelscriptClassDesc> ClassDesc = Engine.GetClass(ClassName);
		UASClass* ScriptClass = ClassDesc.IsValid() ? Cast<UASClass>(ClassDesc->Class) : nullptr;
		Test.TestNotNull(*FString::Printf(TEXT("ShowCreateBlueprintPopup defaults test should generate script class '%s'"), ClassName), ScriptClass);
		return ScriptClass;
	}
}


#define TestTrue(...) Test.TestTrue(__VA_ARGS__)
#define TestEqual(...) Test.TestEqual(__VA_ARGS__)
#define TestNotNull(...) Test.TestNotNull(__VA_ARGS__)

static bool RunShowCreateBlueprintPopupSeedsDialogFromScriptPathAndAssetKind(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptEditorModuleAssetCreationTests_Private;
	FCreateBlueprintPopupDefaultsCallLog CallLog;
	TArray<FAngelscriptEngine*> SavedStack = FAngelscriptEngineContextStack::SnapshotAndClear();
	TUniquePtr<FAngelscriptEngine> Engine = MakePopupAssetCreationTestEngine();
	TUniquePtr<FAngelscriptEngineScope> EngineScope;
	FAngelscriptEditorGetCreateBlueprintDefaultAssetPath SavedDefaultPathDelegate = FAngelscriptEditorDebugBridge::GetEditorGetCreateBlueprintDefaultAssetPath();

	ON_SCOPE_EXIT
	{
		FAngelscriptEditorModuleTestAccess::ResetCreateBlueprintPopupTestHooks();
		FAngelscriptEditorDebugBridge::GetEditorGetCreateBlueprintDefaultAssetPath() = SavedDefaultPathDelegate;
		EngineScope.Reset();
		FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(SavedStack));
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	};

	if (!TestNotNull(TEXT("ShowCreateBlueprintPopup defaults test should create a testing engine"), Engine.Get()))
	{
		return false;
	}

	EngineScope = MakeUnique<FAngelscriptEngineScope>(*Engine);

	if (!CompilePopupScriptModuleWithRelativePath(
			Test,
			*Engine,
			TEXT("ASEditorPopupDefaultBlueprint"),
			TEXT("Gameplay/Enemies/Boss/BossLogic.as"),
			TEXT(R"AS(UCLASS() class ABossPopupBlueprintScript : AActor {})AS"))
		|| !CompilePopupScriptModuleWithRelativePath(
			Test,
			*Engine,
			TEXT("ASEditorPopupDefaultDataAsset"),
			TEXT("Gameplay/Enemies/Boss/BossData.as"),
			TEXT(R"AS(UCLASS() class UBossPopupDataAssetScript : UDataAsset {})AS")))
	{
		return false;
	}

	UASClass* const BlueprintScriptClass = FindPopupScriptClass(Test, *Engine, TEXT("ABossPopupBlueprintScript"));
	UASClass* const DataAssetScriptClass = FindPopupScriptClass(Test, *Engine, TEXT("UBossPopupDataAssetScript"));
	if (BlueprintScriptClass == nullptr || DataAssetScriptClass == nullptr)
	{
		return false;
	}

	FAngelscriptEditorModuleCreateBlueprintPopupTestHooks Hooks;
	Hooks.ForceEditorWindowToFront = [&CallLog]()
	{
		++CallLog.ForceEditorWindowToFrontCalls;
	};
	Hooks.HasAssetsInPath = [&CallLog](const FString& Path, bool bRecursive)
	{
		++CallLog.HasAssetsCalls;
		CallLog.HasAssetsPaths.Add(FString::Printf(TEXT("%s|%d"), *Path, bRecursive ? 1 : 0));
		return Path == TEXT("/Game/Gameplay/Enemies");
	};
	Hooks.CreateSaveAssetDialog = [&CallLog](const FSaveAssetDialogConfig& SaveAssetDialogConfig)
	{
		++CallLog.SaveDialogCalls;
		CallLog.DialogDefaultPaths.Add(SaveAssetDialogConfig.DefaultPath);
		CallLog.DialogDefaultAssetNames.Add(SaveAssetDialogConfig.DefaultAssetName);
		return FString();
	};
	FAngelscriptEditorModuleTestAccess::SetCreateBlueprintPopupTestHooks(MoveTemp(Hooks));

	CallLog.Reset();
	FAngelscriptEditorDebugBridge::GetEditorGetCreateBlueprintDefaultAssetPath().Unbind();
	FAngelscriptEditorModule::ShowCreateBlueprintPopup(BlueprintScriptClass);
	if (!TestEqual(TEXT("ShowCreateBlueprintPopup defaults test should bring the editor window to front for blueprint popup defaults"), CallLog.ForceEditorWindowToFrontCalls, 1)
		|| !TestEqual(TEXT("ShowCreateBlueprintPopup defaults test should open the save dialog once for blueprint popup defaults"), CallLog.SaveDialogCalls, 1)
		|| !TestEqual(TEXT("ShowCreateBlueprintPopup defaults test should probe script directories until it finds the deepest existing content folder"), CallLog.HasAssetsCalls, 2)
		|| !TestEqual(TEXT("ShowCreateBlueprintPopup defaults test should first probe the boss leaf folder"), CallLog.HasAssetsPaths[0], FString(TEXT("/Game/Gameplay/Enemies/Boss|1")))
		|| !TestEqual(TEXT("ShowCreateBlueprintPopup defaults test should then fall back to the nearest populated parent folder"), CallLog.HasAssetsPaths[1], FString(TEXT("/Game/Gameplay/Enemies|1")))
		|| !TestEqual(TEXT("ShowCreateBlueprintPopup defaults test should seed the blueprint dialog path from the matched script folder"), CallLog.DialogDefaultPaths[0], FString(TEXT("/Game/Gameplay/Enemies")))
		|| !TestEqual(TEXT("ShowCreateBlueprintPopup defaults test should prefix blueprint asset names with BP_"), CallLog.DialogDefaultAssetNames[0], FString::Printf(TEXT("BP_%s"), *BlueprintScriptClass->GetName())))
	{
		return false;
	}

	CallLog.Reset();
	FAngelscriptEditorDebugBridge::GetEditorGetCreateBlueprintDefaultAssetPath().Unbind();
	FAngelscriptEditorModule::ShowCreateBlueprintPopup(DataAssetScriptClass);
	if (!TestEqual(TEXT("ShowCreateBlueprintPopup defaults test should bring the editor window to front for data-asset popup defaults"), CallLog.ForceEditorWindowToFrontCalls, 1)
		|| !TestEqual(TEXT("ShowCreateBlueprintPopup defaults test should open the save dialog once for data-asset popup defaults"), CallLog.SaveDialogCalls, 1)
		|| !TestEqual(TEXT("ShowCreateBlueprintPopup defaults test should reuse the nearest populated script folder for data assets"), CallLog.DialogDefaultPaths[0], FString(TEXT("/Game/Gameplay/Enemies")))
		|| !TestEqual(TEXT("ShowCreateBlueprintPopup defaults test should prefix data-asset names with DA_"), CallLog.DialogDefaultAssetNames[0], FString::Printf(TEXT("DA_%s"), *DataAssetScriptClass->GetName())))
	{
		return false;
	}

	CallLog.Reset();
	FAngelscriptEditorDebugBridge::GetEditorGetCreateBlueprintDefaultAssetPath().BindLambda(
		[](UASClass* Class)
		{
			return Class != nullptr && Class->IsChildOf<UDataAsset>()
				? FString(TEXT("/Game/Custom/DA_CustomBossData"))
				: FString(TEXT("/Game/Custom/BP_CustomBoss"));
		});
	FAngelscriptEditorModule::ShowCreateBlueprintPopup(BlueprintScriptClass);
	if (!TestEqual(TEXT("ShowCreateBlueprintPopup defaults test should still bring the editor window to front when an explicit default path delegate is bound"), CallLog.ForceEditorWindowToFrontCalls, 1)
		|| !TestEqual(TEXT("ShowCreateBlueprintPopup defaults test should still open the save dialog once when an explicit default path delegate is bound"), CallLog.SaveDialogCalls, 1)
		|| !TestEqual(TEXT("ShowCreateBlueprintPopup defaults test should skip script-directory probing when an explicit default object path is supplied"), CallLog.HasAssetsCalls, 0)
		|| !TestEqual(TEXT("ShowCreateBlueprintPopup defaults test should use the delegate-provided default path without fallback"), CallLog.DialogDefaultPaths[0], FString(TEXT("/Game/Custom")))
		|| !TestEqual(TEXT("ShowCreateBlueprintPopup defaults test should use the delegate-provided default asset name without fallback"), CallLog.DialogDefaultAssetNames[0], FString(TEXT("BP_CustomBoss"))))
	{
		return false;
	}

	return true;
}

static bool RunShowCreateBlueprintPopupRejectsInvalidDialogObjectPathBeforePackageCreation(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptEditorModuleAssetCreationTests_Private;
	FCreateBlueprintPopupInvalidObjectPathCallLog CallLog;
	TArray<FAngelscriptEngine*> SavedStack = FAngelscriptEngineContextStack::SnapshotAndClear();
	TUniquePtr<FAngelscriptEngine> Engine = MakePopupAssetCreationTestEngine();
	TUniquePtr<FAngelscriptEngineScope> EngineScope;
	FAngelscriptEditorGetCreateBlueprintDefaultAssetPath SavedDefaultPathDelegate = FAngelscriptEditorDebugBridge::GetEditorGetCreateBlueprintDefaultAssetPath();

	ON_SCOPE_EXIT
	{
		FAngelscriptEditorModuleTestAccess::ResetCreateBlueprintPopupTestHooks();
		FAngelscriptEditorDebugBridge::GetEditorGetCreateBlueprintDefaultAssetPath() = SavedDefaultPathDelegate;
		EngineScope.Reset();
		FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(SavedStack));
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	};

	if (!TestNotNull(TEXT("ShowCreateBlueprintPopup invalid object-path test should create a testing engine"), Engine.Get()))
	{
		return false;
	}

	EngineScope = MakeUnique<FAngelscriptEngineScope>(*Engine);
	FAngelscriptEditorDebugBridge::GetEditorGetCreateBlueprintDefaultAssetPath().Unbind();

	if (!CompilePopupScriptModuleWithRelativePath(
			Test,
			*Engine,
			TEXT("ASEditorPopupInvalidDialogPath"),
			TEXT("Gameplay/Enemies/Boss/InvalidDialogPath.as"),
			TEXT(R"AS(UCLASS() class AInvalidDialogPopupBlueprintScript : AActor {})AS")))
	{
		return false;
	}

	UASClass* const BlueprintScriptClass = FindPopupScriptClass(Test, *Engine, TEXT("AInvalidDialogPopupBlueprintScript"));
	if (BlueprintScriptClass == nullptr)
	{
		return false;
	}

	const FString UniqueSuffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString InvalidObjectPath = FString::Printf(TEXT("/Game/Automation/PopupInvalid_%s/"), *UniqueSuffix);
	const FString InvalidPackageName = FPackageName::ObjectPathToPackageName(InvalidObjectPath);
	if (!TestTrue(
			TEXT("ShowCreateBlueprintPopup invalid object-path test should use a dialog response that resolves to an empty asset name"),
			FPackageName::GetLongPackageAssetName(InvalidPackageName).IsEmpty()))
	{
		return false;
	}
	if (!TestTrue(
			TEXT("ShowCreateBlueprintPopup invalid object-path test should start without an existing package at the invalid path"),
			FindPackage(nullptr, *InvalidPackageName) == nullptr))
	{
		return false;
	}

	FAngelscriptEditorModuleCreateBlueprintPopupTestHooks Hooks;
	Hooks.ForceEditorWindowToFront = [&CallLog]()
	{
		++CallLog.ForceEditorWindowToFrontCalls;
	};
	Hooks.HasAssetsInPath = [](const FString&, bool)
	{
		return false;
	};
	Hooks.CreateSaveAssetDialog = [&CallLog, &InvalidObjectPath](const FSaveAssetDialogConfig&)
	{
		++CallLog.SaveDialogCalls;
		return InvalidObjectPath;
	};
	Hooks.OpenMessageDialog = [&CallLog](const FText& Message)
	{
		++CallLog.MessageDialogCalls;
		CallLog.MessageDialogTexts.Add(Message.ToString());
	};
	Hooks.AssetCreated = [&CallLog](UObject*)
	{
		++CallLog.AssetCreatedCalls;
	};
	Hooks.PromptForCheckoutAndSave = [&CallLog](const TArray<UPackage*>&)
	{
		++CallLog.PromptForCheckoutAndSaveCalls;
	};
	Hooks.OpenEditorForAsset = [&CallLog](UObject*)
	{
		++CallLog.OpenEditorForAssetCalls;
	};
	FAngelscriptEditorModuleTestAccess::SetCreateBlueprintPopupTestHooks(MoveTemp(Hooks));

	CallLog.Reset();
	FAngelscriptEditorModule::ShowCreateBlueprintPopup(BlueprintScriptClass);

	bool bPassed = true;
	bPassed &= TestEqual(
		TEXT("ShowCreateBlueprintPopup invalid object-path test should still force the editor window to the front before failing"),
		CallLog.ForceEditorWindowToFrontCalls,
		1);
	bPassed &= TestEqual(
		TEXT("ShowCreateBlueprintPopup invalid object-path test should still show the save dialog once"),
		CallLog.SaveDialogCalls,
		1);
	bPassed &= TestEqual(
		TEXT("ShowCreateBlueprintPopup invalid object-path test should open exactly one validation message dialog"),
		CallLog.MessageDialogCalls,
		1);
	bPassed &= TestEqual(
		TEXT("ShowCreateBlueprintPopup invalid object-path test should not report any asset creation on the invalid-name path"),
		CallLog.AssetCreatedCalls,
		0);
	bPassed &= TestEqual(
		TEXT("ShowCreateBlueprintPopup invalid object-path test should not prompt package save on the invalid-name path"),
		CallLog.PromptForCheckoutAndSaveCalls,
		0);
	bPassed &= TestEqual(
		TEXT("ShowCreateBlueprintPopup invalid object-path test should not open any asset editor on the invalid-name path"),
		CallLog.OpenEditorForAssetCalls,
		0);
	if (CallLog.MessageDialogTexts.Num() == 1)
	{
		bPassed &= TestEqual(
			TEXT("ShowCreateBlueprintPopup invalid object-path test should report the invalid-name error text"),
			CallLog.MessageDialogTexts[0],
			FString(TEXT("Error: Invalid name for new asset.")));
	}
	bPassed &= TestTrue(
		TEXT("ShowCreateBlueprintPopup invalid object-path test should not leave a created package behind after early return"),
		FindPackage(nullptr, *InvalidPackageName) == nullptr);

	return bPassed;
}

static bool RunShowCreateBlueprintPopupDoesNotPromptSaveOrOpenEditorWhenBlueprintFactoryFails(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptEditorModuleAssetCreationTests_Private;
	FCreateBlueprintPopupFactoryFailureCallLog CallLog;
	TArray<FAngelscriptEngine*> SavedStack = FAngelscriptEngineContextStack::SnapshotAndClear();
	TUniquePtr<FAngelscriptEngine> Engine = MakePopupAssetCreationTestEngine();
	TUniquePtr<FAngelscriptEngineScope> EngineScope;
	FAngelscriptEditorGetCreateBlueprintDefaultAssetPath SavedDefaultPathDelegate = FAngelscriptEditorDebugBridge::GetEditorGetCreateBlueprintDefaultAssetPath();
	UPackage* PackageToCleanup = nullptr;
	UObject* CreatedObjectToCleanup = nullptr;

	ON_SCOPE_EXIT
	{
		FAngelscriptEditorModuleTestAccess::ResetCreateBlueprintPopupTestHooks();
		FAngelscriptEditorDebugBridge::GetEditorGetCreateBlueprintDefaultAssetPath() = SavedDefaultPathDelegate;
		EngineScope.Reset();
		if (CreatedObjectToCleanup != nullptr)
		{
			CreatedObjectToCleanup->ClearFlags(RF_Public | RF_Standalone);
			CreatedObjectToCleanup->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_AllowPackageLinkerMismatch | REN_NonTransactional);
			CreatedObjectToCleanup->MarkAsGarbage();
		}
		if (PackageToCleanup != nullptr)
		{
			PackageToCleanup->MarkAsGarbage();
		}
		FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(SavedStack));
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	};

	if (!TestNotNull(TEXT("ShowCreateBlueprintPopup blueprint-factory failure test should create a testing engine"), Engine.Get()))
	{
		return false;
	}

	EngineScope = MakeUnique<FAngelscriptEngineScope>(*Engine);
	FAngelscriptEditorDebugBridge::GetEditorGetCreateBlueprintDefaultAssetPath().Unbind();

	if (!CompilePopupScriptModuleWithRelativePath(
			Test,
			*Engine,
			TEXT("ASEditorPopupBlueprintFactoryFailure"),
			TEXT("Gameplay/Enemies/Boss/BlueprintFactoryFailure.as"),
			TEXT(R"AS(UCLASS() class ABlueprintFactoryFailurePopupScript : AActor {})AS")))
	{
		return false;
	}

	UASClass* const BlueprintScriptClass = FindPopupScriptClass(Test, *Engine, TEXT("ABlueprintFactoryFailurePopupScript"));
	if (BlueprintScriptClass == nullptr)
	{
		return false;
	}

	const FString UniqueSuffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString SaveObjectPath = FString::Printf(TEXT("/Game/Automation/PopupFactoryFailure_%s.PopupFactoryFailure_%s"), *UniqueSuffix, *UniqueSuffix);
	const FString SavePackageName = FPackageName::ObjectPathToPackageName(SaveObjectPath);

	FAngelscriptEditorModuleCreateBlueprintPopupTestHooks Hooks;
	Hooks.ForceEditorWindowToFront = [&CallLog]()
	{
		++CallLog.ForceEditorWindowToFrontCalls;
	};
	Hooks.HasAssetsInPath = [](const FString&, bool)
	{
		return false;
	};
	Hooks.CreateSaveAssetDialog = [&CallLog, &SaveObjectPath](const FSaveAssetDialogConfig&)
	{
		++CallLog.SaveDialogCalls;
		return SaveObjectPath;
	};
	Hooks.CreateBlueprintAsset = [&CallLog](UASClass*, UPackage*, FName, UClass*, UClass*) -> UObject*
	{
		++CallLog.BlueprintFactoryCalls;
		return nullptr;
	};
	Hooks.AssetCreated = [&CallLog](UObject*)
	{
		++CallLog.AssetCreatedCalls;
	};
	Hooks.PromptForCheckoutAndSave = [&CallLog](const TArray<UPackage*>&)
	{
		++CallLog.PromptForCheckoutAndSaveCalls;
	};
	Hooks.OpenEditorForAsset = [&CallLog](UObject*)
	{
		++CallLog.OpenEditorForAssetCalls;
	};
	FAngelscriptEditorModuleTestAccess::SetCreateBlueprintPopupTestHooks(MoveTemp(Hooks));

	CallLog.Reset();
	FAngelscriptEditorModule::ShowCreateBlueprintPopup(BlueprintScriptClass);

	PackageToCleanup = FindPackage(nullptr, *SavePackageName);
	CreatedObjectToCleanup = FindObject<UObject>(nullptr, *SaveObjectPath);

	bool bPassed = true;
	bPassed &= TestEqual(TEXT("ShowCreateBlueprintPopup blueprint-factory failure test should still bring the editor window to front"), CallLog.ForceEditorWindowToFrontCalls, 1);
	bPassed &= TestEqual(TEXT("ShowCreateBlueprintPopup blueprint-factory failure test should still show the save dialog once"), CallLog.SaveDialogCalls, 1);
	bPassed &= TestEqual(TEXT("ShowCreateBlueprintPopup blueprint-factory failure test should invoke the blueprint factory exactly once"), CallLog.BlueprintFactoryCalls, 1);
	bPassed &= TestEqual(TEXT("ShowCreateBlueprintPopup blueprint-factory failure test should not report asset creation when blueprint factory returns null"), CallLog.AssetCreatedCalls, 0);
	bPassed &= TestEqual(TEXT("ShowCreateBlueprintPopup blueprint-factory failure test should not prompt package save when blueprint factory returns null"), CallLog.PromptForCheckoutAndSaveCalls, 0);
	bPassed &= TestEqual(TEXT("ShowCreateBlueprintPopup blueprint-factory failure test should not open any asset editor when blueprint factory returns null"), CallLog.OpenEditorForAssetCalls, 0);
	bPassed &= TestTrue(TEXT("ShowCreateBlueprintPopup blueprint-factory failure test should not leave a created asset object behind"), CreatedObjectToCleanup == nullptr);
	bPassed &= TestTrue(TEXT("ShowCreateBlueprintPopup blueprint-factory failure test should leave any transient package created along the way in a non-dirty state"), PackageToCleanup == nullptr || !PackageToCleanup->IsDirty());

	return bPassed;
}

#undef TestTrue
#undef TestEqual
#undef TestNotNull

TEST_CLASS_WITH_FLAGS(
	FAngelscriptEditorModuleAssetCreationTests,
	"Angelscript.Editor.Module",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ShowCreateBlueprintPopupSeedsDialogFromScriptPathAndAssetKind)
	{
		ASSERT_THAT(IsTrue(RunShowCreateBlueprintPopupSeedsDialogFromScriptPathAndAssetKind(*TestRunner)));
	}

	TEST_METHOD(ShowCreateBlueprintPopupDoesNotPromptSaveOrOpenEditorWhenBlueprintFactoryFails)
	{
		ASSERT_THAT(IsTrue(RunShowCreateBlueprintPopupDoesNotPromptSaveOrOpenEditorWhenBlueprintFactoryFails(*TestRunner)));
	}
};

TEST_CLASS_WITH_FLAGS(
	FAngelscriptEditorModuleAssetCreationTestModuleTests,
	"Angelscript.TestModule.Editor.Module",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ShowCreateBlueprintPopupRejectsInvalidDialogObjectPathBeforePackageCreation)
	{
		ASSERT_THAT(IsTrue(RunShowCreateBlueprintPopupRejectsInvalidDialogObjectPathBeforePackageCreation(*TestRunner)));
	}
};

#endif
