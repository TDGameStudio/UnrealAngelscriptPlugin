#include "CQTest.h"

#include "Offline/AngelscriptOfflineAssetExporter.h"
#include "Dump/AngelscriptOfflineContractSerializer.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptOfflineAssetExporterTests,
	"Angelscript.Editor.OfflineContract.Assets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static const AngelscriptOfflineContract::FAssetRecord* FindAsset(
		const TArray<AngelscriptOfflineContract::FAssetRecord>& Assets,
		const FStringView ObjectPath)
	{
		return Assets.FindByPredicate([ObjectPath](
			const AngelscriptOfflineContract::FAssetRecord& Asset)
		{
			return Asset.ObjectPath == ObjectPath;
		});
	}

public:
	TEST_METHOD(RepresentativeAssetSurfaceIsNormalizedAndDeterministic)
	{
		using namespace AngelscriptEditor::Offline;

		TArray<FOfflineAssetSourceRecord> Source;

		FOfflineAssetSourceRecord EngineAsset;
		EngineAsset.PackagePath = TEXT("/Engine/BasicShapes");
		EngineAsset.ObjectPath =
			TEXT("/Engine/BasicShapes/Cube.Cube");
		EngineAsset.AssetClassPath =
			TEXT("/Script/Engine.StaticMesh");
		EngineAsset.Availability =
			AngelscriptOfflineContract::EAvailability::EditorOnly;
		Source.Add(EngineAsset);

		FOfflineAssetSourceRecord BlueprintAsset;
		BlueprintAsset.PackagePath = TEXT("/Game/UI");
		BlueprintAsset.ObjectPath =
			TEXT("/Game/UI/BP_Widget.BP_Widget");
		BlueprintAsset.GeneratedClassPath =
			TEXT("BlueprintGeneratedClass'/Game/UI/BP_Widget.BP_Widget_C'");
		BlueprintAsset.BaseClassPath =
			TEXT("Class'/Script/UMG.UserWidget'");
		BlueprintAsset.AssetClassPath =
			TEXT("/Script/Engine.Blueprint");
		BlueprintAsset.TypeCheckTags.Add(
			TEXT("generatedClass"),
			BlueprintAsset.GeneratedClassPath);
		BlueprintAsset.TypeCheckTags.Add(
			TEXT("baseClass"),
			BlueprintAsset.BaseClassPath);
		BlueprintAsset.TypeCheckTags.Add(
			TEXT("privatePayload"),
			TEXT("must-not-be-exported"));
		Source.Add(BlueprintAsset);

		FOfflineAssetSourceRecord ScriptAsset;
		ScriptAsset.PackagePath = TEXT("/Script/Engine");
		ScriptAsset.ObjectPath = TEXT("/Script/Engine.Actor");
		ScriptAsset.AssetClassPath = TEXT("/Script/CoreUObject.Class");
		Source.Add(ScriptAsset);

		FOfflineAssetSourceRecord PluginAsset;
		PluginAsset.PackagePath =
			TEXT("/AngelscriptGameplayTags/Data");
		PluginAsset.ObjectPath =
			TEXT("/AngelscriptGameplayTags/Data/Tags.Tags");
		PluginAsset.AssetClassPath =
			TEXT("/Script/GameplayTags.GameplayTagsList");
		PluginAsset.OriginPlugin =
			TEXT("AngelscriptGameplayTags");
		Source.Add(PluginAsset);

		FOfflineAssetSourceRecord Redirect;
		Redirect.PackagePath = TEXT("/Game/Redirects");
		Redirect.ObjectPath =
			TEXT("/Game/Redirects/OldAsset.OldAsset");
		Redirect.AssetClassPath =
			TEXT("/Script/CoreUObject.ObjectRedirector");
		Redirect.RedirectSource = Redirect.ObjectPath;
		Redirect.RedirectTarget =
			TEXT("Object'/Game/New/NewAsset.NewAsset'");
		Source.Add(Redirect);

		FOfflineAssetSourceRecord Excluded;
		Excluded.PackagePath = TEXT("/Game/Excluded");
		Excluded.ObjectPath =
			TEXT("/Game/Excluded/Secret.Secret");
		Source.Add(Excluded);

		FOfflineAssetExportRequest Request;
		Request.Roots = {
			TEXT("/Game"),
			TEXT("/Engine"),
			TEXT("/Script"),
			TEXT("/AngelscriptGameplayTags"),
		};
		Request.ExcludedRoots = {TEXT("/Game/Excluded")};

		const FOfflineAssetExportResult First =
			ExportAssetRecords(Source, Request);
		Algo::Reverse(Source);
		const FOfflineAssetExportResult Second =
			ExportAssetRecords(Source, Request);

		ASSERT_THAT(IsTrue(First.bSuccess,
			TEXT("representative asset export should succeed")));
		ASSERT_THAT(IsTrue(First.Scope.bComplete,
			TEXT("a complete registry with no skipped records should remain complete")));
		ASSERT_THAT(AreEqual(5, First.Assets.Num(),
			TEXT("excluded roots should not enter the asset contract")));
		ASSERT_THAT(IsTrue(
			AngelscriptOfflineContract::SerializeAssetRecords(First.Assets)
				== AngelscriptOfflineContract::SerializeAssetRecords(Second.Assets),
			TEXT("asset JSONL should be byte-identical regardless of registry order")));

		const auto* Blueprint = FindAsset(
			First.Assets,
			TEXT("/Game/UI/BP_Widget.BP_Widget"));
		ASSERT_THAT(IsNotNull(Blueprint,
			TEXT("Blueprint asset should be exported")));
		if (Blueprint != nullptr)
		{
			ASSERT_THAT(AreEqual(
				FString(TEXT("/Game/UI/BP_Widget.BP_Widget_C")),
				Blueprint->GeneratedClassPath,
				TEXT("Blueprint generated class should retain its _C object path")));
			ASSERT_THAT(AreEqual(
				FString(TEXT("/Script/UMG.UserWidget")),
				Blueprint->BaseClassPath,
				TEXT("Blueprint base class should be normalized from export text")));
			ASSERT_THAT(IsFalse(
				Blueprint->TypeCheckTags.Contains(TEXT("privatePayload")),
				TEXT("arbitrary searchable tags and payload data must not be exported")));
		}

		const auto* Engine = FindAsset(
			First.Assets,
			TEXT("/Engine/BasicShapes/Cube.Cube"));
		ASSERT_THAT(IsNotNull(Engine,
			TEXT("engine asset should be exported")));
		if (Engine != nullptr)
		{
			ASSERT_THAT(IsTrue(
				Engine->Availability
					== AngelscriptOfflineContract::EAvailability::EditorOnly,
				TEXT("asset availability classification should survive export")));
			ASSERT_THAT(AreEqual(
				FString(TEXT("Engine")),
				Engine->OriginModule,
				TEXT("/Engine assets should retain their host origin")));
		}

		const auto* Script = FindAsset(
			First.Assets,
			TEXT("/Script/Engine.Actor"));
		ASSERT_THAT(IsNotNull(Script,
			TEXT("/Script type asset should be exported")));
		if (Script != nullptr)
		{
			ASSERT_THAT(AreEqual(
				FString(TEXT("Engine")),
				Script->OriginModule,
				TEXT("/Script package identity should provide the origin module")));
		}

		const auto* Plugin = FindAsset(
			First.Assets,
			TEXT("/AngelscriptGameplayTags/Data/Tags.Tags"));
		ASSERT_THAT(IsNotNull(Plugin,
			TEXT("plugin-mounted asset should be exported")));
		if (Plugin != nullptr)
		{
			ASSERT_THAT(AreEqual(
				FString(TEXT("/AngelscriptGameplayTags")),
				Plugin->MountPoint,
				TEXT("plugin mount point should be preserved")));
			ASSERT_THAT(AreEqual(
				FString(TEXT("AngelscriptGameplayTags")),
				Plugin->OriginPlugin,
				TEXT("plugin origin should be preserved")));
		}

		const auto* RedirectAsset = FindAsset(
			First.Assets,
			TEXT("/Game/Redirects/OldAsset.OldAsset"));
		ASSERT_THAT(IsNotNull(RedirectAsset,
			TEXT("redirector should be exported")));
		if (RedirectAsset != nullptr)
		{
			ASSERT_THAT(AreEqual(
				FString(TEXT("/Game/New/NewAsset.NewAsset")),
				RedirectAsset->RedirectTarget,
				TEXT("redirect target should be normalized")));
		}
	}

	TEST_METHOD(InvalidOrLoadingRegistryMakesOnlyAssetScopeIncomplete)
	{
		using namespace AngelscriptEditor::Offline;

		FOfflineAssetSourceRecord Valid;
		Valid.ObjectPath = TEXT("/Game/Valid.Valid");

		FOfflineAssetSourceRecord Invalid;
		Invalid.ObjectPath = TEXT("/Game/InvalidWithoutObjectName");

		FOfflineAssetExportRequest CompleteRequest;
		CompleteRequest.Roots = {TEXT("/Game")};
		const FOfflineAssetExportResult Skipped =
			ExportAssetRecords({Valid, Invalid}, CompleteRequest);

		ASSERT_THAT(IsTrue(Skipped.bSuccess,
			TEXT("invalid input should be reported as skipped rather than crash export")));
		ASSERT_THAT(IsFalse(Skipped.Scope.bComplete,
			TEXT("skipping an in-scope asset must make the asset scope incomplete")));
		ASSERT_THAT(AreEqual(1, Skipped.Scope.Skipped.Num(),
			TEXT("the invalid in-scope asset should be explained")));

		FOfflineAssetExportRequest LoadingRequest = CompleteRequest;
		LoadingRequest.bRegistryComplete = false;
		const FOfflineAssetExportResult Loading =
			ExportAssetRecords({Valid}, LoadingRequest);
		ASSERT_THAT(IsTrue(Loading.bSuccess,
			TEXT("a loading registry may still produce a diagnostic bundle request")));
		ASSERT_THAT(IsFalse(Loading.Scope.bComplete,
			TEXT("registry loading state must independently mark asset scope incomplete")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("asset-registry-incomplete")),
			Loading.Scope.State,
			TEXT("asset scope should retain an explicit incompleteness state")));
	}

	TEST_METHOD(DuplicateNormalizedObjectPathsAreRejected)
	{
		using namespace AngelscriptEditor::Offline;

		FOfflineAssetSourceRecord First;
		First.ObjectPath = TEXT("/Game/Duplicate.Duplicate");
		FOfflineAssetSourceRecord Second;
		Second.ObjectPath =
			TEXT("Object'/Game/Duplicate.Duplicate'");

		const FOfflineAssetExportResult Result =
			ExportAssetRecords({First, Second}, {});
		ASSERT_THAT(IsFalse(Result.bSuccess,
			TEXT("duplicate normalized object paths must fail publication")));
		ASSERT_THAT(IsTrue(
			Result.Error.Contains(TEXT("Duplicate normalized asset")),
			TEXT("duplicate failure should identify the contract violation")));
	}
};

#endif
