#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "Functional/Blueprint/AngelscriptBlueprintTestHelpers.h"
#include "BlueprintImpact/AngelscriptBlueprintImpactScanner.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;
using namespace AngelscriptBlueprintTestUtils;

namespace BlueprintImpactTestHelpers
{
	bool SaveBlueprintToDisk(
		FAutomationTestBase& Test,
		UBlueprint& Blueprint,
		const FString& PackagePath,
		FString& OutPackageFilename)
	{
		UPackage* Package = Blueprint.GetOutermost();
		if (!Test.TestNotNull(TEXT("Blueprint should have a package before save"), Package))
		{
			return false;
		}

		Package->SetDirtyFlag(true);
		OutPackageFilename = FPackageName::LongPackageNameToFilename(
			PackagePath, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		return Test.TestTrue(
			TEXT("Blueprint package should save to disk"),
			UPackage::SavePackage(Package, &Blueprint, *OutPackageFilename, SaveArgs));
	}

	UBlueprint* CreateDiskBackedBlueprintChild(
		FAutomationTestBase& Test,
		UClass* ParentClass,
		FStringView Suffix,
		FString& OutPackagePath,
		FString& OutPackageFilename)
	{
		if (!Test.TestNotNull(TEXT("Disk-backed BP parent class should be valid"), ParentClass))
		{
			return nullptr;
		}

		const FString AssetName = FString::Printf(
			TEXT("BP_%.*s_%s"),
			Suffix.Len(),
			Suffix.GetData(),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		OutPackagePath = FString::Printf(TEXT("/Game/Automation/%s"), *AssetName);
		UPackage* BlueprintPackage = CreatePackage(*OutPackagePath);
		if (!Test.TestNotNull(TEXT("Disk-backed BP should create a package"), BlueprintPackage))
		{
			return nullptr;
		}

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			ParentClass,
			BlueprintPackage,
			FName(*AssetName),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("AngelscriptBlueprintImpactTests"));
		if (Blueprint == nullptr)
		{
			return nullptr;
		}

		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		ARM.AssetCreated(Blueprint);
		if (!SaveBlueprintToDisk(Test, *Blueprint, OutPackagePath, OutPackageFilename))
		{
			return nullptr;
		}

		return Blueprint;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptBlueprintImpactTest,
	"Angelscript.TestModule.Blueprint.Impact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// =================================================================
	// 1. ScriptParentMatch
	// =================================================================

	TEST_METHOD(ScriptParentMatch)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestBPImpactScriptParentMatch"));
		FScopedTransientBlueprint BP;
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptParent = CompileScriptModule(
			*TestRunner, Engine, ModuleName,
			TEXT("TestBPImpactScriptParentMatch.as"),
			TEXT(R"AS(
UCLASS()
class ATestBPImpactScriptParentMatch : AActor
{
	UPROPERTY()
	int Marker = 1;
}
)AS"),
			TEXT("ATestBPImpactScriptParentMatch"));
		if (ScriptParent == nullptr) return;

		if (!BP.CreateAndCompile(*TestRunner, ScriptParent, TEXT("ImpactParentMatch"))) return;

		const auto MatchingModules = AngelscriptEditor::BlueprintImpact::FindModulesForChangedScripts(
			Engine.GetActiveModules(),
			{ TEXT("TestBPImpactScriptParentMatch.as") });
		if (!TestRunner->TestEqual(TEXT("Should resolve to exactly one module"), MatchingModules.Num(), 1)) return;

		const auto Symbols = AngelscriptEditor::BlueprintImpact::BuildImpactSymbols(MatchingModules);
		TArray<AngelscriptEditor::BlueprintImpact::EBlueprintImpactReason> Reasons;
		if (!TestRunner->TestTrue(
			TEXT("BP child should be marked as impacted by script parent"),
			AngelscriptEditor::BlueprintImpact::AnalyzeLoadedBlueprint(*BP.Blueprint, Symbols, Reasons))) return;

		TestRunner->TestTrue(
			TEXT("Impact reason should include ScriptParentClass"),
			Reasons.Contains(AngelscriptEditor::BlueprintImpact::EBlueprintImpactReason::ScriptParentClass));
	}

	// =================================================================
	// 2. ChangedScriptFilter
	// =================================================================

	TEST_METHOD(ChangedScriptFilter)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleA(TEXT("TestBPImpactFilterA"));
		static const FName ModuleB(TEXT("TestBPImpactFilterB"));
		FScopedTransientBlueprint BPA, BPB;
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleA.ToString());
			Engine.DiscardModule(*ModuleB.ToString());
		};

		UClass* ScriptParentA = CompileScriptModule(
			*TestRunner, Engine, ModuleA,
			TEXT("TestBPImpactFilterA.as"),
			TEXT(R"AS(
UCLASS()
class ATestBPImpactFilterA : AActor
{
	UPROPERTY()
	int Value = 1;
}
)AS"),
			TEXT("ATestBPImpactFilterA"));
		if (ScriptParentA == nullptr) return;

		UClass* ScriptParentB = CompileScriptModule(
			*TestRunner, Engine, ModuleB,
			TEXT("TestBPImpactFilterB.as"),
			TEXT(R"AS(
UCLASS()
class ATestBPImpactFilterB : AActor
{
	UPROPERTY()
	int Value = 2;
}
)AS"),
			TEXT("ATestBPImpactFilterB"));
		if (ScriptParentB == nullptr) return;

		if (!BPA.CreateAndCompile(*TestRunner, ScriptParentA, TEXT("FilterA"))) return;
		if (!BPB.CreateAndCompile(*TestRunner, ScriptParentB, TEXT("FilterB"))) return;

		const auto MatchingModules = AngelscriptEditor::BlueprintImpact::FindModulesForChangedScripts(
			Engine.GetActiveModules(),
			{ TEXT("TestBPImpactFilterA.as") });
		if (!TestRunner->TestEqual(TEXT("Only module A should match the changed script"), MatchingModules.Num(), 1)) return;

		const auto Symbols = AngelscriptEditor::BlueprintImpact::BuildImpactSymbols(MatchingModules);

		TArray<AngelscriptEditor::BlueprintImpact::EBlueprintImpactReason> ReasonsA, ReasonsB;
		const bool bAImpacted = AngelscriptEditor::BlueprintImpact::AnalyzeLoadedBlueprint(*BPA.Blueprint, Symbols, ReasonsA);
		const bool bBImpacted = AngelscriptEditor::BlueprintImpact::AnalyzeLoadedBlueprint(*BPB.Blueprint, Symbols, ReasonsB);

		TestRunner->TestTrue(TEXT("Blueprint A should be marked as impacted"), bAImpacted);
		TestRunner->TestFalse(TEXT("Blueprint B should NOT be marked as impacted"), bBImpacted);
	}

	// =================================================================
	// 3. DiskBackedAssetScan
	// =================================================================

	TEST_METHOD(DiskBackedAssetScan)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestBPImpactDiskBacked"));
		UBlueprint* DiskBP = nullptr;
		FString PackagePath;
		FString PackageFilename;
		ON_SCOPE_EXIT
		{
			if (!PackageFilename.IsEmpty())
			{
				IFileManager::Get().Delete(*PackageFilename, false, true, true);
			}
			CleanupBlueprint(DiskBP);
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptParent = CompileScriptModule(
			*TestRunner, Engine, ModuleName,
			TEXT("TestBPImpactDiskBacked.as"),
			TEXT(R"AS(
UCLASS()
class ATestBPImpactDiskBacked : AActor
{
	UPROPERTY()
	int Marker = 10;
}
)AS"),
			TEXT("ATestBPImpactDiskBacked"));
		if (ScriptParent == nullptr) return;

		DiskBP = BlueprintImpactTestHelpers::CreateDiskBackedBlueprintChild(
			*TestRunner, ScriptParent, TEXT("DiskBacked"), PackagePath, PackageFilename);
		if (!TestRunner->TestNotNull(TEXT("Should create a saved blueprint asset"), DiskBP)) return;

		FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		ARM.Get().ScanModifiedAssetFiles({ PackageFilename });

		AngelscriptEditor::BlueprintImpact::FBlueprintImpactRequest Request;
		Request.ChangedScripts = { TEXT("TestBPImpactDiskBacked.as") };
		const auto ScanResult = AngelscriptEditor::BlueprintImpact::ScanBlueprintAssets(
			Engine, ARM.Get(), Request);

		TestRunner->TestTrue(
			TEXT("Scanner should discover the saved blueprint as a candidate"),
			ScanResult.CandidateAssets.ContainsByPredicate(
				[&PackagePath](const FAssetData& A) { return A.PackageName.ToString() == PackagePath; }));

		TestRunner->TestTrue(
			TEXT("Scanner should mark the saved blueprint as impacted"),
			ScanResult.Matches.ContainsByPredicate(
				[&PackagePath](const AngelscriptEditor::BlueprintImpact::FBlueprintImpactMatch& M)
				{ return M.AssetData.PackageName.ToString() == PackagePath; }));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
