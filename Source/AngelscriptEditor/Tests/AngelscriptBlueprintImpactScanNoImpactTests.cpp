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

namespace AngelscriptEditor_Private_Tests_AngelscriptBlueprintImpactScanNoImpactTests_Private
{
	TUniquePtr<FAngelscriptEngine> CreateBlueprintImpactNoImpactTestEngine()
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

	bool CompileBlueprintImpactScriptModule(
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
			Test.AddError(FString::Printf(TEXT("BlueprintImpact.ScanBlueprintAssets no-impact test should write script file '%s'"), *OutAbsoluteFilename));
			return false;
		}

		FAngelscriptEngineScope EngineScope(Engine);
		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddFile(RelativeFilename, OutAbsoluteFilename);
		if (!Preprocessor.Preprocess())
		{
			Test.AddError(FString::Printf(TEXT("BlueprintImpact.ScanBlueprintAssets no-impact test failed to preprocess '%s': %s"), *RelativeFilename, *Engine.FormatDiagnostics()));
			return false;
		}

		TArray<TSharedRef<FAngelscriptModuleDesc>> ModulesToCompile = Preprocessor.GetModulesToCompile();
		TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
		TGuardValue<bool> AutomaticImportGuard(Engine.bUseAutomaticImportMethod, false);
		FScopedAutomaticImportsOverride AutomaticImportsOverride(Engine.GetScriptEngine());
		const ECompileResult CompileResult = Engine.CompileModules(ECompileType::Initial, ModulesToCompile, CompiledModules);
		if (CompileResult == ECompileResult::Error || CompileResult == ECompileResult::ErrorNeedFullReload)
		{
			Test.AddError(FString::Printf(TEXT("BlueprintImpact.ScanBlueprintAssets no-impact test failed to compile '%s': %s"), *RelativeFilename, *Engine.FormatDiagnostics()));
			return false;
		}

		if (!Test.TestEqual(
				*FString::Printf(TEXT("BlueprintImpact.ScanBlueprintAssets no-impact test should compile exactly one module from '%s'"), *RelativeFilename),
				CompiledModules.Num(),
				1))
		{
			return false;
		}

		const TSharedPtr<FAngelscriptClassDesc> ClassDesc = Engine.GetClass(ClassName);
		OutGeneratedClass = ClassDesc.IsValid() ? ClassDesc->Class : nullptr;
		return Test.TestNotNull(
			*FString::Printf(TEXT("BlueprintImpact.ScanBlueprintAssets no-impact test should generate class '%s'"), *ClassName),
			OutGeneratedClass);
	}

	bool SaveBlueprintToDisk(
		FAutomationTestBase& Test,
		UBlueprint& Blueprint,
		const FString& PackagePath,
		FString& OutPackageFilename)
	{
		UPackage* Package = Blueprint.GetOutermost();
		if (!Test.TestNotNull(TEXT("BlueprintImpact.ScanBlueprintAssets no-impact test should resolve the blueprint package before save"), Package))
		{
			return false;
		}

		Package->SetDirtyFlag(true);
		OutPackageFilename = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		return Test.TestTrue(
			TEXT("BlueprintImpact.ScanBlueprintAssets no-impact test should save the disk-backed blueprint package"),
			UPackage::SavePackage(Package, &Blueprint, *OutPackageFilename, SaveArgs));
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

	UBlueprint* CreateDiskBackedBlueprintChild(
		FAutomationTestBase& Test,
		FAssetRegistryModule& AssetRegistryModule,
		UClass* ParentClass,
		FStringView Suffix,
		FString& OutPackagePath,
		FString& OutPackageFilename)
	{
		if (!Test.TestNotNull(TEXT("BlueprintImpact.ScanBlueprintAssets no-impact test should receive a valid parent class"), ParentClass))
		{
			return nullptr;
		}

		const FString AssetName = FString::Printf(
			TEXT("BP_EditorImpactScanNoImpact_%.*s_%s"),
			Suffix.Len(),
			Suffix.GetData(),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		OutPackagePath = FString::Printf(TEXT("/Game/Automation/%s"), *AssetName);

		UPackage* Package = CreatePackage(*OutPackagePath);
		if (!Test.TestNotNull(TEXT("BlueprintImpact.ScanBlueprintAssets no-impact test should create a disk-backed package"), Package))
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
			TEXT("AngelscriptBlueprintImpactScanNoImpactTests"));
		if (Blueprint == nullptr)
		{
			Package->MarkAsGarbage();
			return nullptr;
		}

		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		AssetRegistryModule.AssetCreated(Blueprint);
		if (!SaveBlueprintToDisk(Test, *Blueprint, OutPackagePath, OutPackageFilename))
		{
			CleanupBlueprintAsset(AssetRegistryModule, Blueprint, OutPackageFilename);
			return nullptr;
		}

		Blueprint->AddToRoot();
		return Blueprint;
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

static bool RunNoMatchingModulesProducesNoMatches(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptBlueprintImpactScanNoImpactTests_Private;
	TArray<FAngelscriptEngine*> SavedStack = FAngelscriptEngineContextStack::SnapshotAndClear();
	TUniquePtr<FAngelscriptEngine> Engine = CreateBlueprintImpactNoImpactTestEngine();
	TUniquePtr<FAngelscriptEngineScope> EngineScope;
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	UBlueprint* Blueprint = nullptr;
	FString BlueprintPackagePath;
	FString BlueprintPackageFilename;
	FString ImpactedAbsoluteFilename;
	ON_SCOPE_EXIT
	{
		EngineScope.Reset();
		CleanupBlueprintAsset(AssetRegistryModule, Blueprint, BlueprintPackageFilename);
		if (!ImpactedAbsoluteFilename.IsEmpty())
		{
			IFileManager::Get().Delete(*ImpactedAbsoluteFilename, false, true, true);
		}
		CollectGarbage(RF_NoFlags, true);
		FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(SavedStack));
	};

	if (!TestNotNull(TEXT("BlueprintImpact.ScanBlueprintAssets no-impact test should create a testing engine"), Engine.Get()))
	{
		return false;
	}

	EngineScope = MakeUnique<FAngelscriptEngineScope>(*Engine);

	const FString UniqueSuffix = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
	const FString ImpactedRelativeFilename = FString::Printf(TEXT("Scripts/BlueprintImpact/NoImpact_%s.as"), *UniqueSuffix);
	const FString ImpactedClassName = FString::Printf(TEXT("ABlueprintImpactNoImpact_%s"), *UniqueSuffix);
	const FString ImpactedScriptSource = FString::Printf(TEXT(R"AS(
UCLASS()
class %s : AActor
{
	UPROPERTY()
	int Marker = 17;
}
)AS"), *ImpactedClassName);

	UClass* ImpactedClass = nullptr;
	if (!CompileBlueprintImpactScriptModule(
			Test,
			*Engine,
			ImpactedRelativeFilename,
			ImpactedClassName,
			ImpactedScriptSource,
			ImpactedAbsoluteFilename,
			ImpactedClass))
	{
		return false;
	}

	Blueprint = CreateDiskBackedBlueprintChild(
		Test,
		AssetRegistryModule,
		ImpactedClass,
		TEXT("NoMatchingModules"),
		BlueprintPackagePath,
		BlueprintPackageFilename);
	if (!TestNotNull(TEXT("BlueprintImpact.ScanBlueprintAssets no-impact test should create a saved blueprint child of the impacted script class"), Blueprint))
	{
		return false;
	}

	AssetRegistryModule.Get().ScanModifiedAssetFiles({ BlueprintPackageFilename });

	if (!TestEqual(
			TEXT("BlueprintImpact.ScanBlueprintAssets no-impact test should only have the one synthetic active module"),
			Engine->GetActiveModules().Num(),
			1))
	{
		return false;
	}

	AngelscriptEditor::BlueprintImpact::FBlueprintImpactRequest Request;
	Request.ChangedScripts = { TEXT("Scripts/Gameplay/Unrelated.as") };
	const AngelscriptEditor::BlueprintImpact::FBlueprintImpactScanResult Result =
		AngelscriptEditor::BlueprintImpact::ScanBlueprintAssets(*Engine, AssetRegistryModule.Get(), Request);

	if (!TestEqual(
			TEXT("BlueprintImpact.ScanBlueprintAssets no-impact scan should keep one normalized changed script"),
			Result.NormalizedChangedScripts.Num(),
			1))
	{
		return false;
	}
	if (!TestEqual(
			TEXT("BlueprintImpact.ScanBlueprintAssets no-impact scan should normalize the unmatched path"),
			Result.NormalizedChangedScripts[0],
			FString(TEXT("scripts/gameplay/unrelated.as"))))
	{
		return false;
	}
	if (!TestEqual(
			TEXT("BlueprintImpact.ScanBlueprintAssets no-impact scan should not match any active modules"),
			Result.MatchingModules.Num(),
			0))
	{
		return false;
	}
	if (!TestTrue(
			TEXT("BlueprintImpact.ScanBlueprintAssets no-impact scan should produce an empty impact symbol set"),
			Result.Symbols.IsEmpty()))
	{
		return false;
	}
	if (!TestTrue(
			TEXT("BlueprintImpact.ScanBlueprintAssets no-impact scan should still enumerate the saved blueprint asset as a candidate"),
			Result.CandidateAssets.ContainsByPredicate([&BlueprintPackagePath](const FAssetData& AssetData)
			{
				return AssetData.PackageName.ToString() == BlueprintPackagePath;
			})))
	{
		return false;
	}
	if (!TestEqual(
			TEXT("BlueprintImpact.ScanBlueprintAssets no-impact scan should not report any impacted matches"),
			Result.Matches.Num(),
			0))
	{
		return false;
	}
	if (!TestFalse(
			TEXT("BlueprintImpact.ScanBlueprintAssets no-impact scan should not keep a match entry for the blueprint"),
			FindMatchByPackage(Result, BlueprintPackagePath) != nullptr))
	{
		return false;
	}
	return TestEqual(
		TEXT("BlueprintImpact.ScanBlueprintAssets no-impact scan should not fail to load any discovered blueprint assets"),
		Result.FailedAssetLoads,
		0);
}

#undef TestTrue
#undef TestNotNull
#undef TestFalse
#undef TestEqual

TEST_CLASS_WITH_FLAGS(FAngelscriptBlueprintImpactScanNoImpactTests,
	"Angelscript.TestModule.Editor.BlueprintImpact.ScanBlueprintAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(NoMatchingModulesProducesNoMatches)
	{
		ASSERT_THAT(IsTrue(RunNoMatchingModulesProducesNoMatches(*TestRunner)));
	}
};

#endif
