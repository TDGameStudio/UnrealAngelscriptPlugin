#include "AngelscriptEngine.h"
#include "AngelscriptSourceProvider.h"
#include "CQTest.h"
#include "Preprocessor/AngelscriptPreprocessor.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_FileSystem_AngelscriptScriptSourceProviderTests_Private
{
	FString NormalizePath(const FString& InPath)
	{
		FString Normalized = InPath;
		FPaths::NormalizeFilename(Normalized);
		Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));
		return Normalized;
	}

	FString GetProviderFixtureRoot()
	{
		return NormalizePath(FPaths::ProjectSavedDir() / TEXT("Automation") / TEXT("ScriptSourceProvider"));
	}

	void CleanProviderFixtureRoot()
	{
		IFileManager::Get().DeleteDirectory(*GetProviderFixtureRoot(), false, true);
	}

	bool WriteProviderFixtureFile(const FString& AbsoluteRoot, const FString& RelativePath, const FString& Content, FString& OutAbsolutePath)
	{
		OutAbsolutePath = NormalizePath(FPaths::Combine(AbsoluteRoot, RelativePath));
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutAbsolutePath), true);
		return FFileHelper::SaveStringToFile(Content, *OutAbsolutePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	struct FRecordingSourceProvider final : IAngelscriptSourceProvider
	{
		TArray<FAngelscriptSource> Sources;
		TMap<FString, FString> SourceTextByVirtualPath;
		int32 FindSourcesCallCount = 0;
		int32 LoadSourceTextCallCount = 0;
		int32 QuerySourceStateCallCount = 0;

		virtual void FindSources(
			const TArray<FAngelscriptSourceRoot>& ScriptRoots,
			bool bSkipDevelopmentScripts,
			bool bSkipEditorScripts,
			TArray<FAngelscriptSource>& OutSources) override
		{
			++FindSourcesCallCount;
			LastScriptRoots = ScriptRoots;
			bLastSkipDevelopmentScripts = bSkipDevelopmentScripts;
			bLastSkipEditorScripts = bSkipEditorScripts;
			OutSources.Append(Sources);
		}

		virtual bool LoadSourceText(const FAngelscriptSource& Source, FString& OutSourceText) override
		{
			++LoadSourceTextCallCount;
			const FString* Text = SourceTextByVirtualPath.Find(Source.VirtualPath.ToString());
			if (Text == nullptr)
			{
				return false;
			}
			OutSourceText = *Text;
			return true;
		}

		virtual bool QuerySourceState(const FAngelscriptSource& Source, FAngelscriptSourceState& OutState) override
		{
			++QuerySourceStateCallCount;
			OutState.Timestamp = FDateTime(2026, 6, 16, 10, 0, 0);
			OutState.ContentHash = GetTypeHash(Source.VirtualPath.ToString());
			OutState.bHasContentHash = true;
			return true;
		}

		TArray<FAngelscriptSourceRoot> LastScriptRoots;
		bool bLastSkipDevelopmentScripts = false;
		bool bLastSkipEditorScripts = false;
	};
}

TEST_CLASS_WITH_FLAGS(FAngelscriptScriptSourceProviderTest,
	"Angelscript.TestModule.FileSystem.ScriptSourceProvider",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(EngineDiscoveryUsesInjectedSourceProvider)
	{
		using namespace AngelscriptTest_FileSystem_AngelscriptScriptSourceProviderTests_Private;

		TSharedRef<FRecordingSourceProvider> Provider = MakeShared<FRecordingSourceProvider>();
		Provider->Sources.Add(FAngelscriptSource::FromGameFile(
			TEXT("Gameplay/Player.as"),
			TEXT("J:/ProviderProject/Script/Gameplay/Player.as")));
		Provider->Sources.Add(FAngelscriptSource::FromPluginFile(
			TEXT("Inventory"),
			TEXT("Gameplay/Item.as"),
			TEXT("J:/ProviderProject/Plugins/Inventory/Script/Gameplay/Item.as")));

		FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		Dependencies.SourceProvider = Provider;

		FAngelscriptEngine Engine(FAngelscriptEngineConfig(), Dependencies);
		Engine.AllScriptRoots.Add(FAngelscriptSourceRoot::FromGameRoot(TEXT("J:/ProviderProject/Script")));
		Engine.AllScriptRoots.Add(FAngelscriptSourceRoot::FromPluginRoot(TEXT("Inventory"), TEXT("J:/ProviderProject/Plugins/Inventory/Script")));

		TGuardValue<bool> UseEditorScriptsGuard(Engine.bUseEditorScripts, false);

		TArray<FAngelscriptSource> Sources;
		Engine.FindAllScriptSources(Sources);

		ASSERT_THAT(AreEqual(1, Provider->FindSourcesCallCount, TEXT("Discovery should ask the injected provider once")));
		ASSERT_THAT(AreEqual(2, Sources.Num(), TEXT("Provider-backed discovery should return the provider sources")));
		ASSERT_THAT(AreEqual(2, Provider->LastScriptRoots.Num(), TEXT("Provider should receive the effective script roots")));
		ASSERT_THAT(IsTrue(Provider->bLastSkipDevelopmentScripts, TEXT("Provider discovery should inherit development-script skip flag")));
		ASSERT_THAT(IsTrue(Provider->bLastSkipEditorScripts, TEXT("Provider discovery should inherit editor-script skip flag")));

		ASSERT_THAT(AreEqual(
			FString(TEXT("/Angelscript/Game/Gameplay/Player.as")),
			Sources[0].VirtualPath.ToString(),
			TEXT("Game source should keep its canonical virtual path")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("/Angelscript/Plugin/Inventory/Gameplay/Item.as")),
			Sources[1].VirtualPath.ToString(),
			TEXT("Plugin source should keep its canonical virtual path")));

		TArray<FAngelscriptEngine::FFilenamePair> FilenamePairs;
		Engine.FindAllScriptFilenames(FilenamePairs);
		ASSERT_THAT(AreEqual(2, FilenamePairs.Num(), TEXT("Filename compatibility adapter should use provider-backed discovery")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("/Angelscript/Plugin/Inventory/Gameplay/Item.as")),
			FilenamePairs[1].VirtualPath,
			TEXT("Filename adapter should preserve provider virtual path")));
	}

	TEST_METHOD(PreprocessorLoadsDiskBackedSourceThroughProvider)
	{
		using namespace AngelscriptTest_FileSystem_AngelscriptScriptSourceProviderTests_Private;

		TSharedRef<FRecordingSourceProvider> Provider = MakeShared<FRecordingSourceProvider>();
		const FAngelscriptSource Source = FAngelscriptSource::FromGameFile(
			TEXT("Provider/Loaded.as"),
			TEXT("J:/ProviderProject/Script/Provider/Loaded.as"));
		Provider->SourceTextByVirtualPath.Add(Source.VirtualPath.ToString(), TEXT(R"(
int Entry()
{
	return 31;
}
)"));

		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.SetSourceProvider(&Provider.Get());
		Preprocessor.AddSource(Source);

		ASSERT_THAT(IsTrue(Preprocessor.Preprocess(), TEXT("Provider-loaded source should preprocess")));
		ASSERT_THAT(AreEqual(1, Provider->LoadSourceTextCallCount, TEXT("Provider should be asked to load disk-backed source once")));

		TArray<TSharedRef<FAngelscriptModuleDesc>> Modules = Preprocessor.GetModulesToCompile();
		ASSERT_THAT(AreEqual(1, Modules.Num(), TEXT("Provider-loaded source should emit one module")));
		ASSERT_THAT(AreEqual(FString(TEXT("Provider.Loaded")), Modules[0]->ModuleName, TEXT("Provider-loaded module should keep the source module name")));
		ASSERT_THAT(AreEqual(Source.VirtualPath.ToString(), Modules[0]->Code[0].VirtualPath, TEXT("Provider-loaded module should keep source virtual path")));
	}

	TEST_METHOD(PreprocessorBypassesProviderForMemorySource)
	{
		using namespace AngelscriptTest_FileSystem_AngelscriptScriptSourceProviderTests_Private;

		TSharedRef<FRecordingSourceProvider> Provider = MakeShared<FRecordingSourceProvider>();

		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.SetSourceProvider(&Provider.Get());
		Preprocessor.AddSource(FAngelscriptSource::FromMemorySource(
			TEXT("/Angelscript/Memory/Immediate/ProviderBypass.as"),
			TEXT(R"(
int Entry()
{
	return 37;
}
)")));

		ASSERT_THAT(IsTrue(Preprocessor.Preprocess(), TEXT("Memory source should preprocess without provider text loading")));
		ASSERT_THAT(AreEqual(0, Provider->LoadSourceTextCallCount, TEXT("Provider should not load memory-backed source text")));
	}

	TEST_METHOD(DiskProviderDiscoversProjectPluginAndLegacyRoots)
	{
		using namespace AngelscriptTest_FileSystem_AngelscriptScriptSourceProviderTests_Private;

		CleanProviderFixtureRoot();
		ON_SCOPE_EXIT
		{
			CleanProviderFixtureRoot();
		};

		const FString FixtureRoot = GetProviderFixtureRoot();
		const FString ProjectRoot = NormalizePath(FixtureRoot / TEXT("ProjectScript"));
		const FString PluginRoot = NormalizePath(FixtureRoot / TEXT("InventoryScript"));
		const FString LegacyRoot = NormalizePath(FixtureRoot / TEXT("LegacyScript"));

		FString ProjectScriptPath;
		FString PluginScriptPath;
		FString LegacyScriptPath;
		if (!this->Assert.IsTrue(
				WriteProviderFixtureFile(ProjectRoot, TEXT("Gameplay/Player.as"), TEXT("int PlayerEntry() { return 1; }"), ProjectScriptPath),
				TEXT("Project provider fixture should write a script file"))
			|| !this->Assert.IsTrue(
				WriteProviderFixtureFile(PluginRoot, TEXT("Gameplay/Item.as"), TEXT("int ItemEntry() { return 2; }"), PluginScriptPath),
				TEXT("Plugin provider fixture should write a script file"))
			|| !this->Assert.IsTrue(
				WriteProviderFixtureFile(LegacyRoot, TEXT("Gameplay/Legacy.as"), TEXT("int LegacyEntry() { return 3; }"), LegacyScriptPath),
				TEXT("Legacy provider fixture should write a script file")))
		{
			return;
		}

		FAngelscriptEngine Engine;
		Engine.AllScriptRoots = {
			FAngelscriptSourceRoot::FromGameRoot(ProjectRoot),
			FAngelscriptSourceRoot::FromPluginRoot(TEXT("Inventory"), PluginRoot),
		};
		TGuardValue<bool> UseEditorScriptsGuard(Engine.bUseEditorScripts, true);

		TArray<FAngelscriptSource> Sources;
		Engine.FindAllScriptSources(Sources);

		TMap<FString, FAngelscriptSource> SourcesByVirtualPath;
		for (const FAngelscriptSource& Source : Sources)
		{
			SourcesByVirtualPath.Add(Source.VirtualPath.ToString(), Source);
		}

		ASSERT_THAT(IsTrue(
			SourcesByVirtualPath.Contains(TEXT("/Angelscript/Game/Gameplay/Player.as")),
			TEXT("Disk provider should discover project root as /Angelscript/Game")));
		ASSERT_THAT(IsTrue(
			SourcesByVirtualPath.Contains(TEXT("/Angelscript/Plugin/Inventory/Gameplay/Item.as")),
			TEXT("Disk provider should discover plugin root with plugin mount name")));

		Engine.AllScriptRoots = {
			FAngelscriptSourceRoot::FromPluginRoot(TEXT("StalePlugin"), ProjectRoot),
		};
		Engine.AllRootPaths = {
			LegacyRoot,
		};

		TArray<FAngelscriptEngine::FFilenamePair> FilenamePairs;
		Engine.FindAllScriptFilenames(FilenamePairs);

		ASSERT_THAT(AreEqual(1, FilenamePairs.Num(), TEXT("Legacy AllRootPaths fallback should still discover one source")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("/Angelscript/Game/Gameplay/Legacy.as")),
			FilenamePairs[0].VirtualPath,
			TEXT("Legacy AllRootPaths fallback should synthesize game virtual path metadata")));
		ASSERT_THAT(AreEqual(
			LegacyScriptPath,
			NormalizePath(FilenamePairs[0].AbsolutePath),
			TEXT("Legacy AllRootPaths fallback should keep the legacy absolute path")));
	}
};

#endif
