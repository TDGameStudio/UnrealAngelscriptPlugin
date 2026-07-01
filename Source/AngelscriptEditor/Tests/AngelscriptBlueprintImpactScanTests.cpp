#include "BlueprintImpact/AngelscriptBlueprintImpactScanner.h"

#include "AngelscriptEngine.h"
#include "AngelscriptInclude.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "CQTest.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Preprocessor/AngelscriptPreprocessor.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptEditor_Private_Tests_AngelscriptBlueprintImpactScanTests_Private
{
	TUniquePtr<FAngelscriptEngine> CreateBlueprintImpactScanTestEngine()
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

	bool CompileBlueprintImpactScanScriptModule(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FString& RelativeFilename,
		const FString& ClassName,
		const FString& ScriptSource,
		FString& OutAbsoluteFilename,
		UClass*& OutGeneratedClass)
	{
		OutAbsoluteFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), RelativeFilename);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutAbsoluteFilename), true);
		if (!FFileHelper::SaveStringToFile(ScriptSource, *OutAbsoluteFilename))
		{
			Test.AddError(FString::Printf(TEXT("BlueprintImpact.ScanBlueprintAssets test should write script file '%s'"), *OutAbsoluteFilename));
			return false;
		}

		FAngelscriptEngineScope EngineScope(Engine);
		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddFile(RelativeFilename, OutAbsoluteFilename);
		if (!Preprocessor.Preprocess())
		{
			Test.AddError(FString::Printf(TEXT("BlueprintImpact.ScanBlueprintAssets test failed to preprocess '%s': %s"), *RelativeFilename, *Engine.FormatDiagnostics()));
			return false;
		}

		TArray<TSharedRef<FAngelscriptModuleDesc>> ModulesToCompile = Preprocessor.GetModulesToCompile();
		TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
		TGuardValue<bool> AutomaticImportGuard(Engine.bUseAutomaticImportMethod, false);
		FScopedAutomaticImportsOverride AutomaticImportsOverride(Engine.GetScriptEngine());
		const ECompileResult CompileResult = Engine.CompileModules(ECompileType::Initial, ModulesToCompile, CompiledModules);
		if (CompileResult == ECompileResult::Error || CompileResult == ECompileResult::ErrorNeedFullReload)
		{
			Test.AddError(FString::Printf(TEXT("BlueprintImpact.ScanBlueprintAssets test failed to compile '%s': %s"), *RelativeFilename, *Engine.FormatDiagnostics()));
			return false;
		}

		if (!Test.TestEqual(
				*FString::Printf(TEXT("BlueprintImpact.ScanBlueprintAssets test should compile exactly one module from '%s'"), *RelativeFilename),
				CompiledModules.Num(),
				1))
		{
			return false;
		}

		const TSharedPtr<FAngelscriptClassDesc> ClassDesc = Engine.GetClass(ClassName);
		OutGeneratedClass = ClassDesc.IsValid() ? ClassDesc->Class : nullptr;
		return Test.TestNotNull(
			*FString::Printf(TEXT("BlueprintImpact.ScanBlueprintAssets test should generate class '%s'"), *ClassName),
			OutGeneratedClass);
	}

	bool SaveBlueprintToDisk(
		FAutomationTestBase& Test,
		UBlueprint& Blueprint,
		const FString& PackagePath,
		FString& OutPackageFilename)
	{
		UPackage* Package = Blueprint.GetOutermost();
		if (!Test.TestNotNull(TEXT("BlueprintImpact.ScanBlueprintAssets test should resolve the blueprint package before save"), Package))
		{
			return false;
		}

		Package->SetDirtyFlag(true);
		OutPackageFilename = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		return Test.TestTrue(
			TEXT("BlueprintImpact.ScanBlueprintAssets test should save the disk-backed blueprint package"),
			UPackage::SavePackage(Package, &Blueprint, *OutPackageFilename, SaveArgs));
	}

	UBlueprint* CreateDiskBackedBlueprintChild(
		FAutomationTestBase& Test,
		FAssetRegistryModule& AssetRegistryModule,
		UClass* ParentClass,
		FStringView Suffix,
		FString& OutPackagePath,
		FString& OutPackageFilename)
	{
		if (!Test.TestNotNull(TEXT("BlueprintImpact.ScanBlueprintAssets test should receive a valid parent class"), ParentClass))
		{
			return nullptr;
		}

		const FString AssetName = FString::Printf(
			TEXT("BP_EditorImpactScan_%.*s_%s"),
			Suffix.Len(),
			Suffix.GetData(),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		OutPackagePath = FString::Printf(TEXT("/Game/Automation/%s"), *AssetName);

		UPackage* Package = CreatePackage(*OutPackagePath);
		if (!Test.TestNotNull(TEXT("BlueprintImpact.ScanBlueprintAssets test should create a disk-backed package"), Package))
		{
			return nullptr;
		}

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			ParentClass,
			Package,
			FName(*AssetName),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("AngelscriptBlueprintImpactScanTests"));
		if (Blueprint == nullptr)
		{
			return nullptr;
		}

		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		AssetRegistryModule.AssetCreated(Blueprint);
		if (!SaveBlueprintToDisk(Test, *Blueprint, OutPackagePath, OutPackageFilename))
		{
			return nullptr;
		}

		Blueprint->AddToRoot();
		return Blueprint;
	}

	void CleanupBlueprintAsset(
		FAssetRegistryModule& AssetRegistryModule,
		UBlueprint*& Blueprint,
		const FString& PackageFilename)
	{
		if (Blueprint == nullptr)
		{
			return;
		}

		AssetRegistryModule.AssetDeleted(Blueprint);
		if (Blueprint->IsRooted())
		{
			Blueprint->RemoveFromRoot();
		}

		if (!PackageFilename.IsEmpty())
		{
			IFileManager::Get().Delete(*PackageFilename, false, true, true);
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
		Blueprint = nullptr;
	}

	bool ContainsModuleWithSection(
		const TArray<TSharedRef<FAngelscriptModuleDesc>>& Modules,
		const FString& RelativeFilename)
	{
		return Modules.ContainsByPredicate([&RelativeFilename](const TSharedRef<FAngelscriptModuleDesc>& Module)
		{
			return Module->Code.ContainsByPredicate([&RelativeFilename](const FAngelscriptModuleDesc::FCodeSection& Section)
			{
				return Section.RelativeFilename == RelativeFilename;
			});
		});
	}

	const AngelscriptEditor::BlueprintImpact::FBlueprintImpactMatch* FindMatchByPackage(
		const AngelscriptEditor::BlueprintImpact::FBlueprintImpactScanResult& Result,
		const FString& PackagePath)
	{
		return Result.Matches.FindByPredicate([&PackagePath](const AngelscriptEditor::BlueprintImpact::FBlueprintImpactMatch& Match)
		{
			return Match.AssetData.PackageName.ToString() == PackagePath;
		});
	}
}

#define TestEqual(...) Test.TestEqual(__VA_ARGS__)
#define TestFalse(...) Test.TestFalse(__VA_ARGS__)
#define TestNotNull(...) Test.TestNotNull(__VA_ARGS__)
#define TestTrue(...) Test.TestTrue(__VA_ARGS__)

static bool RunFullScanUsesAllActiveModulesWhenChangedScriptsEmpty(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptBlueprintImpactScanTests_Private;
	TArray<FAngelscriptEngine*> SavedStack = FAngelscriptEngineContextStack::SnapshotAndClear();
	TUniquePtr<FAngelscriptEngine> Engine = CreateBlueprintImpactScanTestEngine();
	TUniquePtr<FAngelscriptEngineScope> EngineScope;
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	UBlueprint* Blueprint = nullptr;
	FString BlueprintPackagePath;
	FString BlueprintPackageFilename;
	FString ImpactedAbsoluteFilename;
	FString UnrelatedAbsoluteFilename;
	ON_SCOPE_EXIT
	{
		EngineScope.Reset();
		AngelscriptEditor::BlueprintImpact::ClearBlueprintAssetsOverrideForTesting();
		CleanupBlueprintAsset(AssetRegistryModule, Blueprint, BlueprintPackageFilename);
		if (!ImpactedAbsoluteFilename.IsEmpty())
		{
			IFileManager::Get().Delete(*ImpactedAbsoluteFilename, false, true, true);
		}
		if (!UnrelatedAbsoluteFilename.IsEmpty())
		{
			IFileManager::Get().Delete(*UnrelatedAbsoluteFilename, false, true, true);
		}
		CollectGarbage(RF_NoFlags, true);
		FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(SavedStack));
	};

	if (!TestNotNull(TEXT("BlueprintImpact.ScanBlueprintAssets test should create a testing engine"), Engine.Get()))
	{
		return false;
	}

	EngineScope = MakeUnique<FAngelscriptEngineScope>(*Engine);

	const FString UniqueSuffix = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
	const FString ImpactedRelativeFilename = FString::Printf(TEXT("Scripts/BlueprintImpact/FullScanImpacted_%s.as"), *UniqueSuffix);
	const FString UnrelatedRelativeFilename = FString::Printf(TEXT("Scripts/BlueprintImpact/FullScanUnrelated_%s.as"), *UniqueSuffix);
	const FString ImpactedClassName = FString::Printf(TEXT("ABlueprintImpactFullScanImpacted_%s"), *UniqueSuffix);
	const FString UnrelatedClassName = FString::Printf(TEXT("ABlueprintImpactFullScanUnrelated_%s"), *UniqueSuffix);
	const FString ImpactedScriptSource = FString::Printf(TEXT(R"AS(
UCLASS()
class %s : AActor
{
	UPROPERTY()
	int Marker = 7;
}
)AS"), *ImpactedClassName);
	const FString UnrelatedScriptSource = FString::Printf(TEXT(R"AS(
UCLASS()
class %s : AActor
{
	UPROPERTY()
	int Marker = 11;
}
)AS"), *UnrelatedClassName);

	UClass* ImpactedClass = nullptr;
	UClass* UnrelatedClass = nullptr;
	if (!CompileBlueprintImpactScanScriptModule(
			Test,
			*Engine,
			ImpactedRelativeFilename,
			ImpactedClassName,
			ImpactedScriptSource,
			ImpactedAbsoluteFilename,
			ImpactedClass)
		|| !CompileBlueprintImpactScanScriptModule(
			Test,
			*Engine,
			UnrelatedRelativeFilename,
			UnrelatedClassName,
			UnrelatedScriptSource,
			UnrelatedAbsoluteFilename,
			UnrelatedClass))
	{
		return false;
	}

	Blueprint = CreateDiskBackedBlueprintChild(
		Test,
		AssetRegistryModule,
		ImpactedClass,
		TEXT("FullScan"),
		BlueprintPackagePath,
		BlueprintPackageFilename);
	if (!TestNotNull(TEXT("BlueprintImpact.ScanBlueprintAssets test should create a saved blueprint child of the impacted script class"), Blueprint))
	{
		return false;
	}

	AssetRegistryModule.Get().ScanModifiedAssetFiles({ BlueprintPackageFilename });
	TArray<FAssetData> CandidateAssets;
	AssetRegistryModule.Get().GetAssetsByPackageName(FName(*BlueprintPackagePath), CandidateAssets, true);
	if (!TestEqual(
			TEXT("BlueprintImpact.ScanBlueprintAssets test should isolate the saved blueprint asset"),
			CandidateAssets.Num(),
			1))
	{
		return false;
	}
	AngelscriptEditor::BlueprintImpact::SetBlueprintAssetsOverrideForTesting(MoveTemp(CandidateAssets));

	const int32 ExpectedActiveModuleCount = Engine->GetActiveModules().Num();
	if (!TestEqual(
			TEXT("BlueprintImpact.ScanBlueprintAssets test should only have the two synthetic active modules"),
			ExpectedActiveModuleCount,
			2))
	{
		return false;
	}

	const AngelscriptEditor::BlueprintImpact::FBlueprintImpactRequest FullScanRequest;
	const AngelscriptEditor::BlueprintImpact::FBlueprintImpactScanResult FullScanResult =
		AngelscriptEditor::BlueprintImpact::ScanBlueprintAssets(*Engine, AssetRegistryModule.Get(), FullScanRequest);

	if (!TestEqual(
			TEXT("BlueprintImpact.ScanBlueprintAssets should normalize an empty ChangedScripts request to zero entries"),
			FullScanResult.NormalizedChangedScripts.Num(),
			0))
	{
		return false;
	}
	if (!TestEqual(
			TEXT("BlueprintImpact.ScanBlueprintAssets should use every active module for a full scan"),
			FullScanResult.MatchingModules.Num(),
			ExpectedActiveModuleCount))
	{
		return false;
	}
	if (!TestTrue(
			TEXT("BlueprintImpact.ScanBlueprintAssets full scan should include the impacted module"),
			ContainsModuleWithSection(FullScanResult.MatchingModules, ImpactedRelativeFilename))
		|| !TestTrue(
			TEXT("BlueprintImpact.ScanBlueprintAssets full scan should include the unrelated module"),
			ContainsModuleWithSection(FullScanResult.MatchingModules, UnrelatedRelativeFilename)))
	{
		return false;
	}
	if (!TestTrue(
			TEXT("BlueprintImpact.ScanBlueprintAssets full scan should build impact symbols from the impacted module"),
			FullScanResult.Symbols.Classes.Contains(ImpactedClass))
		|| !TestTrue(
			TEXT("BlueprintImpact.ScanBlueprintAssets full scan should build impact symbols from the unrelated module"),
			FullScanResult.Symbols.Classes.Contains(UnrelatedClass)))
	{
		return false;
	}
	if (!TestTrue(
			TEXT("BlueprintImpact.ScanBlueprintAssets full scan should discover the saved blueprint asset as a candidate"),
			FullScanResult.CandidateAssets.ContainsByPredicate([&BlueprintPackagePath](const FAssetData& AssetData)
			{
				return AssetData.PackageName.ToString() == BlueprintPackagePath;
			})))
	{
		return false;
	}

	const AngelscriptEditor::BlueprintImpact::FBlueprintImpactMatch* FullScanMatch = FindMatchByPackage(FullScanResult, BlueprintPackagePath);
	if (!TestNotNull(TEXT("BlueprintImpact.ScanBlueprintAssets full scan should match the blueprint whose parent comes from the impacted module"), FullScanMatch))
	{
		return false;
	}
	if (!TestEqual(
			TEXT("BlueprintImpact.ScanBlueprintAssets full scan should report exactly one impact reason for the parent-class-only blueprint"),
			FullScanMatch->Reasons.Num(),
			1))
	{
		return false;
	}
	if (!TestEqual(
			TEXT("BlueprintImpact.ScanBlueprintAssets full scan should classify the match as a script parent class impact"),
			FullScanMatch->Reasons[0],
			AngelscriptEditor::BlueprintImpact::EBlueprintImpactReason::ScriptParentClass))
	{
		return false;
	}
	if (!TestEqual(
			TEXT("BlueprintImpact.ScanBlueprintAssets full scan should not fail to load any discovered blueprint assets"),
			FullScanResult.FailedAssetLoads,
			0))
	{
		return false;
	}

	AngelscriptEditor::BlueprintImpact::FBlueprintImpactRequest SelectiveRequest;
	SelectiveRequest.ChangedScripts = { UnrelatedRelativeFilename };
	const AngelscriptEditor::BlueprintImpact::FBlueprintImpactScanResult SelectiveResult =
		AngelscriptEditor::BlueprintImpact::ScanBlueprintAssets(*Engine, AssetRegistryModule.Get(), SelectiveRequest);

	if (!TestEqual(
			TEXT("BlueprintImpact.ScanBlueprintAssets selective scan should keep one normalized changed script"),
			SelectiveResult.NormalizedChangedScripts.Num(),
			1))
	{
		return false;
	}
	if (!TestEqual(
			TEXT("BlueprintImpact.ScanBlueprintAssets selective scan should restrict matching modules to the unrelated script"),
			SelectiveResult.MatchingModules.Num(),
			1))
	{
		return false;
	}
	if (!TestTrue(
			TEXT("BlueprintImpact.ScanBlueprintAssets selective scan should include the explicitly changed unrelated module"),
			ContainsModuleWithSection(SelectiveResult.MatchingModules, UnrelatedRelativeFilename)))
	{
		return false;
	}
	if (!TestFalse(
			TEXT("BlueprintImpact.ScanBlueprintAssets selective scan should exclude the untouched impacted module"),
			ContainsModuleWithSection(SelectiveResult.MatchingModules, ImpactedRelativeFilename)))
	{
		return false;
	}
	if (!TestTrue(
			TEXT("BlueprintImpact.ScanBlueprintAssets selective scan should only build symbols from the changed unrelated module"),
			SelectiveResult.Symbols.Classes.Contains(UnrelatedClass)))
	{
		return false;
	}
	if (!TestFalse(
			TEXT("BlueprintImpact.ScanBlueprintAssets selective scan should not build symbols from the untouched impacted module"),
			SelectiveResult.Symbols.Classes.Contains(ImpactedClass)))
	{
		return false;
	}
	if (!TestTrue(
			TEXT("BlueprintImpact.ScanBlueprintAssets selective scan should still enumerate the saved blueprint asset as a candidate"),
			SelectiveResult.CandidateAssets.ContainsByPredicate([&BlueprintPackagePath](const FAssetData& AssetData)
			{
				return AssetData.PackageName.ToString() == BlueprintPackagePath;
			})))
	{
		return false;
	}
	if (!TestFalse(
			TEXT("BlueprintImpact.ScanBlueprintAssets selective scan should not match the blueprint when only the unrelated module is scanned"),
			FindMatchByPackage(SelectiveResult, BlueprintPackagePath) != nullptr))
	{
		return false;
	}

	return TestEqual(
		TEXT("BlueprintImpact.ScanBlueprintAssets selective scan should not fail to load any discovered blueprint assets"),
		SelectiveResult.FailedAssetLoads,
		0);
}

#undef TestTrue
#undef TestNotNull
#undef TestFalse
#undef TestEqual

TEST_CLASS_WITH_FLAGS(FAngelscriptBlueprintImpactScanTests,
	"Angelscript.TestModule.Editor.BlueprintImpact.ScanBlueprintAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(FullScanUsesAllActiveModulesWhenChangedScriptsEmpty)
	{
		ASSERT_THAT(IsTrue(RunFullScanUsesAllActiveModulesWhenChangedScriptsEmpty(*TestRunner)));
	}
};

#endif
