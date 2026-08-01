#include "CQTest.h"

#include "Offline/AngelscriptOfflineExportCommand.h"
#include "Dump/AngelscriptOfflineBundleFixtureReader.h"
#include "Dump/AngelscriptOfflineContractIdentity.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptOfflineExportCommandletTests,
	"Angelscript.Editor.OfflineContract.Commandlet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static FString MakeTestRoot()
	{
		return FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation"),
			TEXT("OfflineExportCommandlet"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	static AngelscriptOfflineContract::FBundleWriteRequest
		MakePreparedBundle(
			const AngelscriptOfflineContract::EBundleKind BundleKind,
			const bool bSymbolScopeComplete = true,
			const bool bAssetScopeComplete = true)
	{
		using namespace AngelscriptOfflineContract;

		FBundleWriteRequest Bundle;
		Bundle.Manifest.BundleKind = BundleKind;
		Bundle.Manifest.ProducerName = TEXT("commandlet-test");
		Bundle.Manifest.ProducerVersion = TEXT("1");
		Bundle.Manifest.UnrealVersion = TEXT("test");
		Bundle.Manifest.PluginVersion = TEXT("test");
		Bundle.Manifest.ForkVersion = TEXT("2.33+selective-2.38");
		Bundle.Manifest.CompilerContractVersion =
			TEXT("ue-as-standalone-v1");
		Bundle.Manifest.Platform = TEXT("test");
		Bundle.Manifest.Configuration = TEXT("test");
		Bundle.Manifest.SymbolScope.bComplete =
			bSymbolScopeComplete;
		Bundle.Manifest.SymbolScope.State =
			TEXT("synthetic-complete-final-engine");
		Bundle.Manifest.AssetScope.bComplete =
			bAssetScopeComplete;
		Bundle.Manifest.AssetScope.State =
			bAssetScopeComplete
				? TEXT("synthetic-complete-assets")
				: TEXT("synthetic-incomplete-assets");
		Bundle.Manifest.RequiredFields = {
			TEXT("manifest.schema"),
			TEXT("manifest.symbolScope"),
			TEXT("records.stableId"),
		};

		FSymbolRecord Type;
		Type.Kind = ESymbolKind::Type;
		Type.Type.Kind = ETypeKind::Value;
		Type.Type.Name = TEXT("FCommandletFixture");
		Type.Type.CompleteDeclaration =
			TEXT("class FCommandletFixture");
		Type.Origin.Layer = EOriginLayer::HostSurface;
		Type.Origin.Kind = EOriginKind::Manual;
		FSymbolIdentityInput Identity;
		Identity.Kind = Type.Kind;
		Identity.CompleteDeclaration =
			Type.Type.CompleteDeclaration;
		Type.CanonicalIdentity =
			MakeCanonicalSymbolIdentity(Identity);
		Type.StableId = MakeStableSymbolId(Identity);
		Type.Type.StableId = Type.StableId;
		Bundle.Symbols.Add(MoveTemp(Type));
		return Bundle;
	}

	static bool LoadBundleFile(
		const FString& Directory,
		const TCHAR* Filename,
		TArray<uint8>& OutBytes)
	{
		return FFileHelper::LoadFileToArray(
			OutBytes,
			*FPaths::Combine(Directory, Filename));
	}

public:
	TEST_METHOD(ArgumentsMapToOneUnfilteredBundleRequest)
	{
		using namespace AngelscriptEditor::Offline;
		using namespace AngelscriptOfflineContract;

		const FString ExplicitOutput =
			FPaths::Combine(MakeTestRoot(), TEXT("with space"));
		const FString Params = FString::Printf(
			TEXT(
				"-run=AngelscriptOfflineExport -BUILDMACHINE -Unattended -stdout -ABSLOG=Commandlet.log -NullRHI -Output=\"%s\" -BundleKind=DefaultEngine -AssetRoots=\"/Game,/Engine\" -AssetExcludeRoots=/Game/Secret -AllowIncompleteAssets"),
			*ExplicitOutput);

		FOfflineExportCommandOptions Options;
		FString Error;
		const bool bParsed =
			TryParseOfflineExportCommandOptions(
				Params,
				Options,
				Error);
		ASSERT_THAT(IsTrue(bParsed,
			TEXT("documented commandlet arguments should parse")));
		ASSERT_THAT(IsTrue(Options.bOutputWasExplicit,
			TEXT("explicit output should be distinguished from the ignored default")));
		ASSERT_THAT(IsTrue(Options.bAssetRootsWereExplicit,
			TEXT("explicit asset scope should be retained")));
		ASSERT_THAT(IsTrue(Options.bAllowIncompleteAssets,
			TEXT("incomplete asset allowance should map to the request")));
		ASSERT_THAT(IsTrue(
			Options.BundleKind == EBundleKind::DefaultEngine,
			TEXT("DefaultEngine should select exactly the default-engine bundle kind")));
		ASSERT_THAT(AreEqual(2, Options.AssetRoots.Num(),
			TEXT("comma-separated asset roots should be normalized")));
		ASSERT_THAT(AreEqual(1, Options.ExcludedAssetRoots.Num(),
			TEXT("asset exclusion roots should map independently")));
		ASSERT_THAT(AreEqual(
			FPaths::ConvertRelativePathToFull(ExplicitOutput),
			Options.OutputDirectory,
			TEXT("quoted output paths should be preserved")));

		FOfflineExportCommandOptions Rejected;
		FString RejectedError;
		const bool bAcceptedSymbolFilter =
			TryParseOfflineExportCommandOptions(
				TEXT("-Modules=Engine"),
				Rejected,
				RejectedError);
		ASSERT_THAT(IsFalse(bAcceptedSymbolFilter,
			TEXT("module symbol filters must not be exposed")));
		ASSERT_THAT(IsTrue(
			RejectedError.Contains(TEXT("Unknown commandlet switch")),
			TEXT("unsupported filters should fail as invalid arguments")));
	}

	TEST_METHOD(DefaultOutputIsIgnoredAndKindSpecific)
	{
		using namespace AngelscriptEditor::Offline;
		using namespace AngelscriptOfflineContract;

		FOfflineExportCommandOptions Project;
		FString Error;
		const bool bProjectParsed =
			TryParseOfflineExportCommandOptions(
				FString(),
				Project,
				Error);
		ASSERT_THAT(IsTrue(bProjectParsed,
			TEXT("empty arguments should select project defaults")));
		ASSERT_THAT(IsFalse(Project.bOutputWasExplicit,
			TEXT("default destination should be marked implicit")));
		ASSERT_THAT(IsTrue(
			Project.OutputDirectory.StartsWith(
				FPaths::ConvertRelativePathToFull(
					FPaths::ProjectSavedDir())),
			TEXT("default output must live under ignored Saved")));
		ASSERT_THAT(AreEqual(1, Project.AssetRoots.Num(),
			TEXT("project export should default to the /Game scope")));

		FOfflineExportCommandOptions DefaultEngine;
		const bool bDefaultParsed =
			TryParseOfflineExportCommandOptions(
				TEXT("-BundleKind=DefaultEngine"),
				DefaultEngine,
				Error);
		ASSERT_THAT(IsTrue(bDefaultParsed,
			TEXT("default-engine arguments should parse")));
		ASSERT_THAT(IsTrue(DefaultEngine.AssetRoots.IsEmpty(),
			TEXT("DefaultEngine must not inject an asset root; the release workflow declares /Game explicitly")));
		ASSERT_THAT(IsFalse(
			Project.OutputDirectory
				.Equals(DefaultEngine.OutputDirectory),
			TEXT("project and default-engine implicit destinations must not overwrite each other")));
	}

	TEST_METHOD(ProjectAndDefaultBundlesPublishAtomically)
	{
		using namespace AngelscriptEditor::Offline;
		using namespace AngelscriptOfflineContract;

		const FString TestRoot = MakeTestRoot();
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(
				*TestRoot,
				false,
				true);
		};

		FOfflineExportCommandOptions ProjectOptions;
		ProjectOptions.BundleKind = EBundleKind::Project;
		ProjectOptions.OutputDirectory =
			FPaths::Combine(TestRoot, TEXT("project"));
		const FOfflineExportPublicationResult Project =
			PublishPreparedOfflineBundle(
				MakePreparedBundle(EBundleKind::Project),
				ProjectOptions,
				true);
		ASSERT_THAT(IsTrue(Project.IsSuccess(),
			TEXT("complete project bundle should publish")));

		FOfflineExportCommandOptions DefaultOptions;
		DefaultOptions.BundleKind = EBundleKind::DefaultEngine;
		DefaultOptions.OutputDirectory =
			FPaths::Combine(TestRoot, TEXT("default-engine"));
		const FOfflineExportPublicationResult Default =
			PublishPreparedOfflineBundle(
				MakePreparedBundle(EBundleKind::DefaultEngine),
				DefaultOptions,
				true);
		ASSERT_THAT(IsTrue(Default.IsSuccess(),
			TEXT("complete packaged-role default-engine bundle should publish")));

		const auto ProjectRead =
			FAngelscriptOfflineBundleFixtureReader::Read(
				ProjectOptions.OutputDirectory);
		const auto DefaultRead =
			FAngelscriptOfflineBundleFixtureReader::Read(
				DefaultOptions.OutputDirectory);
		ASSERT_THAT(IsTrue(
			ProjectRead.bSuccess && DefaultRead.bSuccess,
			TEXT("both commandlet bundle kinds should pass strict fixture reading")));
	}

	TEST_METHOD(CompletenessGatesLeaveNoManifestValidPartialDestination)
	{
		using namespace AngelscriptEditor::Offline;
		using namespace AngelscriptOfflineContract;

		const FString TestRoot = MakeTestRoot();
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(
				*TestRoot,
				false,
				true);
		};

		FOfflineExportCommandOptions Options;
		Options.OutputDirectory =
			FPaths::Combine(TestRoot, TEXT("blocked"));

		const FOfflineExportPublicationResult EngineBlocked =
			PublishPreparedOfflineBundle(
				MakePreparedBundle(EBundleKind::Project),
				Options,
				false);
		ASSERT_THAT(IsTrue(
			EngineBlocked.ExitCode
				== EOfflineExportCommandletExitCode::EngineNotReady,
			TEXT("engine readiness must gate publication")));

		const FOfflineExportPublicationResult SymbolsBlocked =
			PublishPreparedOfflineBundle(
				MakePreparedBundle(
					EBundleKind::Project,
					false,
					true),
				Options,
				true);
		ASSERT_THAT(IsTrue(
			SymbolsBlocked.ExitCode
				== EOfflineExportCommandletExitCode::IncompleteSymbolScope,
			TEXT("incomplete final symbol scope must be nonzero")));

		const FOfflineExportPublicationResult AssetsBlocked =
			PublishPreparedOfflineBundle(
				MakePreparedBundle(
					EBundleKind::Project,
					true,
					false),
				Options,
				true);
		ASSERT_THAT(IsTrue(
			AssetsBlocked.ExitCode
				== EOfflineExportCommandletExitCode::IncompleteAssetScope,
			TEXT("incomplete assets require explicit allowance")));
		ASSERT_THAT(IsFalse(
			IFileManager::Get().FileExists(
				*FPaths::Combine(
					Options.OutputDirectory,
					TEXT("manifest.json"))),
			TEXT("a gated failure must leave no manifest-valid partial destination")));

		Options.bAllowIncompleteAssets = true;
		const FOfflineExportPublicationResult AssetsAllowed =
			PublishPreparedOfflineBundle(
				MakePreparedBundle(
					EBundleKind::Project,
					true,
					false),
				Options,
				true);
		ASSERT_THAT(IsTrue(AssetsAllowed.IsSuccess(),
			TEXT("explicit allowance should publish asset-only incompleteness")));
		const auto Read =
			FAngelscriptOfflineBundleFixtureReader::Read(
				Options.OutputDirectory);
		ASSERT_THAT(IsTrue(Read.bSuccess,
			TEXT("explicitly incomplete asset scope should remain a valid, integrity-checked bundle")));
	}

	TEST_METHOD(RepeatedPreparedPublicationIsByteIdentical)
	{
		using namespace AngelscriptEditor::Offline;
		using namespace AngelscriptOfflineContract;

		const FString TestRoot = MakeTestRoot();
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(
				*TestRoot,
				false,
				true);
		};

		FOfflineExportCommandOptions FirstOptions;
		FirstOptions.OutputDirectory =
			FPaths::Combine(TestRoot, TEXT("first"));
		FOfflineExportCommandOptions SecondOptions;
		SecondOptions.OutputDirectory =
			FPaths::Combine(TestRoot, TEXT("second"));

		const FBundleWriteRequest Fixture =
			MakePreparedBundle(EBundleKind::Project);
		const FOfflineExportPublicationResult First =
			PublishPreparedOfflineBundle(
				Fixture,
				FirstOptions,
				true);
		const FOfflineExportPublicationResult Second =
			PublishPreparedOfflineBundle(
				Fixture,
				SecondOptions,
				true);
		ASSERT_THAT(IsTrue(
			First.IsSuccess() && Second.IsSuccess(),
			TEXT("both repeated publications should succeed")));

		for (const TCHAR* Filename : {
			TEXT("manifest.json"),
			TEXT("symbols.jsonl"),
			TEXT("assets.jsonl")})
		{
			TArray<uint8> FirstBytes;
			TArray<uint8> SecondBytes;
			const bool bLoaded =
				LoadBundleFile(
					FirstOptions.OutputDirectory,
					Filename,
					FirstBytes)
				&& LoadBundleFile(
					SecondOptions.OutputDirectory,
					Filename,
					SecondBytes);
			ASSERT_THAT(IsTrue(bLoaded,
				TEXT("published canonical files should be readable")));
			ASSERT_THAT(IsTrue(FirstBytes == SecondBytes,
				TEXT("repeated commandlet publication should be byte-identical")));
		}
	}

	TEST_METHOD(RepeatedPackagedRolePublicationIsByteIdentical)
	{
		using namespace AngelscriptEditor::Offline;
		using namespace AngelscriptOfflineContract;

		const FString TestRoot = MakeTestRoot();
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(
				*TestRoot,
				false,
				true);
		};

		FOfflineExportCommandOptions FirstOptions;
		FirstOptions.BundleKind = EBundleKind::DefaultEngine;
		FirstOptions.OutputDirectory =
			FPaths::Combine(TestRoot, TEXT("default-first"));
		FOfflineExportCommandOptions SecondOptions = FirstOptions;
		SecondOptions.OutputDirectory =
			FPaths::Combine(TestRoot, TEXT("default-second"));

		const FBundleWriteRequest Fixture =
			MakePreparedBundle(EBundleKind::DefaultEngine);
		const FOfflineExportPublicationResult First =
			PublishPreparedOfflineBundle(
				Fixture,
				FirstOptions,
				true);
		const FOfflineExportPublicationResult Second =
			PublishPreparedOfflineBundle(
				Fixture,
				SecondOptions,
				true);
		ASSERT_THAT(IsTrue(
			First.IsSuccess() && Second.IsSuccess(),
			TEXT("both packaged-role publications should succeed")));
		ASSERT_THAT(AreEqual(
			First.WriteResult.BundleIdentity,
			Second.WriteResult.BundleIdentity,
			TEXT("packaged-role bundle identity should be deterministic")));

		for (const TCHAR* Filename : {
			TEXT("manifest.json"),
			TEXT("symbols.jsonl"),
			TEXT("assets.jsonl")})
		{
			TArray<uint8> FirstBytes;
			TArray<uint8> SecondBytes;
			const bool bLoaded =
				LoadBundleFile(
					FirstOptions.OutputDirectory,
					Filename,
					FirstBytes)
				&& LoadBundleFile(
					SecondOptions.OutputDirectory,
					Filename,
					SecondBytes);
			ASSERT_THAT(IsTrue(bLoaded,
				TEXT("packaged-role canonical files should be readable")));
			ASSERT_THAT(IsTrue(FirstBytes == SecondBytes,
				TEXT("repeated packaged-role publication should be byte-identical")));
		}
	}
};

#endif
