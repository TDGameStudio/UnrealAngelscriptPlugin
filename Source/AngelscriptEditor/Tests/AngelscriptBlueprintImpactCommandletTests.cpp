#include "BlueprintImpact/AngelscriptBlueprintImpactScanCommandlet.h"
#include "BlueprintImpact/AngelscriptBlueprintImpactScanner.h"

#include "Core/AngelscriptEngine.h"
#include "Core/AngelscriptRuntimeModule.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "CQTest.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptEditor_Private_Tests_AngelscriptBlueprintImpactCommandletTests_Private
{
	FAngelscriptModuleDesc::FCodeSection MakeCommandletCodeSection(const FString& RelativeFilename)
	{
		FAngelscriptModuleDesc::FCodeSection Section;
		Section.RelativeFilename = RelativeFilename;
		Section.AbsoluteFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("BlueprintImpact"), RelativeFilename);
		Section.Code = TEXT("// BlueprintImpact commandlet merge test");
		Section.CodeHash = GetTypeHash(RelativeFilename);
		return Section;
	}

	TSharedRef<FAngelscriptModuleDesc> MakeCommandletModule(const FString& ModuleName, const FString& RelativeFilename)
	{
		TSharedRef<FAngelscriptModuleDesc> Module = MakeShared<FAngelscriptModuleDesc>();
		Module->ModuleName = ModuleName;
		Module->Code.Add(MakeCommandletCodeSection(RelativeFilename));
		return Module;
	}

	FAssetData MakeUnloadableBlueprintAssetData()
	{
		const FString AssetName = FString::Printf(
			TEXT("BP_EditorImpactMissing_%s"),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		const FString PackagePath = FString::Printf(TEXT("/Game/Automation/%s"), *AssetName);
		const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *PackagePath, *AssetName);
		return FAssetData(PackagePath, ObjectPath, FTopLevelAssetPath(UBlueprint::StaticClass()));
	}
}

#define TestEqual(...) Test.TestEqual(__VA_ARGS__)
#define TestNotNull(...) Test.TestNotNull(__VA_ARGS__)
#define TestTrue(...) Test.TestTrue(__VA_ARGS__)

static bool RunCommandletMergesInlineAndFileChangedScripts(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptBlueprintImpactCommandletTests_Private;
	const FString TempDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("BlueprintImpact"));
	IFileManager::Get().MakeDirectory(*TempDirectory, true);

	const FString TempFilename = FPaths::Combine(
		TempDirectory,
		FString::Printf(TEXT("ChangedScripts_%s.txt"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	ON_SCOPE_EXIT
	{
		IFileManager::Get().Delete(*TempFilename, false, true, true);
	};

	if (!TestTrue(
		TEXT("BlueprintImpact.CommandletMergesInlineAndFileChangedScripts should write the temporary ChangedScriptFile"),
		FFileHelper::SaveStringToFile(TEXT("\n  ./Scripts/C.as  \n\n"), *TempFilename)))
	{
		return false;
	}

	FAngelscriptRuntimeModule::InitializeAngelscript();
	FAngelscriptRuntimeModule::InitializeAngelscript();
	FAngelscriptEngine* EnginePtr = FAngelscriptEngine::TryGetCurrentEngine();
	if (!TestNotNull(TEXT("BlueprintImpact.CommandletMergesInlineAndFileChangedScripts requires a live Angelscript engine on the context stack"), EnginePtr))
	{
		return false;
	}
	FAngelscriptEngine& Engine = *EnginePtr;
	const bool bOriginalDidInitialCompileSucceed = Engine.bDidInitialCompileSucceed;
	ON_SCOPE_EXIT
	{
		Engine.bDidInitialCompileSucceed = bOriginalDidInitialCompileSucceed;
	};
	Engine.bDidInitialCompileSucceed = true;

	const FString ParamsString = FString::Printf(
		TEXT("ChangedScript=Scripts/A.as; Scripts/B.as ChangedScriptFile=%s"),
		*TempFilename);

	AngelscriptEditor::BlueprintImpact::FBlueprintImpactRequest Request;
	FString ErrorMessage;
	if (!TestTrue(
		TEXT("BlueprintImpact.CommandletMergesInlineAndFileChangedScripts should parse inline and file changed scripts"),
		UAngelscriptBlueprintImpactScanCommandlet::BuildRequestForTesting(ParamsString, Request, ErrorMessage)))
	{
		return false;
	}

	if (!TestTrue(TEXT("BlueprintImpact.CommandletMergesInlineAndFileChangedScripts should not produce a parse error"), ErrorMessage.IsEmpty()))
	{
		return false;
	}

	if (!TestEqual(
		TEXT("BlueprintImpact.CommandletMergesInlineAndFileChangedScripts should merge three trimmed changed scripts before normalization"),
		Request.ChangedScripts.Num(),
		3))
	{
		return false;
	}

	if (!TestEqual(TEXT("BlueprintImpact.CommandletMergesInlineAndFileChangedScripts should keep the first inline script"), Request.ChangedScripts[0], FString(TEXT("Scripts/A.as"))))
	{
		return false;
	}

	if (!TestEqual(TEXT("BlueprintImpact.CommandletMergesInlineAndFileChangedScripts should trim the second inline script"), Request.ChangedScripts[1], FString(TEXT("Scripts/B.as"))))
	{
		return false;
	}

	if (!TestEqual(TEXT("BlueprintImpact.CommandletMergesInlineAndFileChangedScripts should trim the file-provided script and preserve its relative marker"), Request.ChangedScripts[2], FString(TEXT("./Scripts/C.as"))))
	{
		return false;
	}

	const TArray<TSharedRef<FAngelscriptModuleDesc>> Modules = {
		MakeCommandletModule(TEXT("BlueprintImpact.Commandlet.A"), TEXT("Scripts/A.as")),
		MakeCommandletModule(TEXT("BlueprintImpact.Commandlet.B"), TEXT("Scripts/B.as")),
		MakeCommandletModule(TEXT("BlueprintImpact.Commandlet.C"), TEXT("Scripts/C.as")),
		MakeCommandletModule(TEXT("BlueprintImpact.Commandlet.Other"), TEXT("Scripts/D.as"))
	};

	const TArray<TSharedRef<FAngelscriptModuleDesc>> MatchingModules =
		AngelscriptEditor::BlueprintImpact::FindModulesForChangedScripts(Modules, Request.ChangedScripts);
	if (!TestEqual(
		TEXT("BlueprintImpact.CommandletMergesInlineAndFileChangedScripts should match every module referenced by inline or file inputs"),
		MatchingModules.Num(),
		3))
	{
		return false;
	}

	if (!TestEqual(TEXT("BlueprintImpact.CommandletMergesInlineAndFileChangedScripts should keep the first matching module"), MatchingModules[0]->ModuleName, FString(TEXT("BlueprintImpact.Commandlet.A"))))
	{
		return false;
	}

	if (!TestEqual(TEXT("BlueprintImpact.CommandletMergesInlineAndFileChangedScripts should keep the second matching module"), MatchingModules[1]->ModuleName, FString(TEXT("BlueprintImpact.Commandlet.B"))))
	{
		return false;
	}

	if (!TestEqual(TEXT("BlueprintImpact.CommandletMergesInlineAndFileChangedScripts should include the file-backed matching module"), MatchingModules[2]->ModuleName, FString(TEXT("BlueprintImpact.Commandlet.C"))))
	{
		return false;
	}

	UAngelscriptBlueprintImpactScanCommandlet* Commandlet = NewObject<UAngelscriptBlueprintImpactScanCommandlet>();
	if (!TestNotNull(TEXT("BlueprintImpact.CommandletMergesInlineAndFileChangedScripts should create the commandlet object"), Commandlet))
	{
		return false;
	}

	return TestEqual(
		TEXT("BlueprintImpact.CommandletMergesInlineAndFileChangedScripts should return the success exit code for valid merged inputs"),
		Commandlet->Main(ParamsString),
		0);
}

static bool RunCommandletFailedAssetLoadsReturnExitCode3(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptBlueprintImpactCommandletTests_Private;
	UAngelscriptBlueprintImpactScanCommandlet* Commandlet = NewObject<UAngelscriptBlueprintImpactScanCommandlet>();
	if (!TestNotNull(TEXT("BlueprintImpact.CommandletFailedAssetLoadsReturnExitCode3 should create the commandlet object"), Commandlet))
	{
		return false;
	}

	FAngelscriptRuntimeModule::InitializeAngelscript();
	FAngelscriptEngine* EnginePtr = FAngelscriptEngine::TryGetCurrentEngine();
	if (!TestNotNull(TEXT("BlueprintImpact.CommandletFailedAssetLoadsReturnExitCode3 requires a live Angelscript engine on the context stack"), EnginePtr))
	{
		return false;
	}
	FAngelscriptEngine& Engine = *EnginePtr;
	const bool bOriginalDidInitialCompileSucceed = Engine.bDidInitialCompileSucceed;
	ON_SCOPE_EXIT
	{
		Engine.bDidInitialCompileSucceed = bOriginalDidInitialCompileSucceed;
		AngelscriptEditor::BlueprintImpact::ClearBlueprintAssetsOverrideForTesting();
	};
	Engine.bDidInitialCompileSucceed = true;

	TArray<FAssetData> CandidateAssets;
	CandidateAssets.Add(MakeUnloadableBlueprintAssetData());
	AngelscriptEditor::BlueprintImpact::SetBlueprintAssetsOverrideForTesting(CandidateAssets);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AngelscriptEditor::BlueprintImpact::FBlueprintImpactRequest Request;
	const AngelscriptEditor::BlueprintImpact::FBlueprintImpactScanResult ScanResult =
		AngelscriptEditor::BlueprintImpact::ScanBlueprintAssets(Engine, AssetRegistryModule.Get(), Request);

	if (!TestEqual(
			TEXT("BlueprintImpact.CommandletFailedAssetLoadsReturnExitCode3 should scan exactly one injected candidate asset"),
			ScanResult.CandidateAssets.Num(),
			1))
	{
		return false;
	}

	if (!TestEqual(
			TEXT("BlueprintImpact.CommandletFailedAssetLoadsReturnExitCode3 should count the unloadable candidate as a failed asset load"),
			ScanResult.FailedAssetLoads,
			1))
	{
		return false;
	}

	if (!TestEqual(
			TEXT("BlueprintImpact.CommandletFailedAssetLoadsReturnExitCode3 should not report a match when the only candidate blueprint cannot load"),
			ScanResult.Matches.Num(),
			0))
	{
		return false;
	}

	return TestEqual(
		TEXT("BlueprintImpact.CommandletFailedAssetLoadsReturnExitCode3 should return the asset-scan-failure exit code when FailedAssetLoads is non-zero"),
		Commandlet->Main(TEXT("")),
		3);
}

#undef TestTrue
#undef TestNotNull
#undef TestEqual

TEST_CLASS_WITH_FLAGS(FAngelscriptBlueprintImpactCommandletTests,
	"Angelscript.Editor.BlueprintImpact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CommandletMergesInlineAndFileChangedScripts)
	{
		ASSERT_THAT(IsTrue(RunCommandletMergesInlineAndFileChangedScripts(*TestRunner)));
	}

	TEST_METHOD(CommandletFailedAssetLoadsReturnExitCode3)
	{
		ASSERT_THAT(IsTrue(RunCommandletFailedAssetLoadsReturnExitCode3(*TestRunner)));
	}
};

#endif
