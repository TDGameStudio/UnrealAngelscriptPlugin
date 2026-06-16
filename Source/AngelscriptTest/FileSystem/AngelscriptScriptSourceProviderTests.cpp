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

		TestRunner->TestEqual(TEXT("Discovery should ask the injected provider once"), Provider->FindSourcesCallCount, 1);
		TestRunner->TestEqual(TEXT("Provider-backed discovery should return the provider sources"), Sources.Num(), 2);
		TestRunner->TestEqual(TEXT("Provider should receive the effective script roots"), Provider->LastScriptRoots.Num(), 2);
		TestRunner->TestTrue(TEXT("Provider discovery should inherit development-script skip flag"), Provider->bLastSkipDevelopmentScripts);
		TestRunner->TestTrue(TEXT("Provider discovery should inherit editor-script skip flag"), Provider->bLastSkipEditorScripts);

		if (Sources.Num() == 2)
		{
			TestRunner->TestEqual(
				TEXT("Game source should keep its canonical virtual path"),
				Sources[0].VirtualPath.ToString(),
				FString(TEXT("/Angelscript/Game/Gameplay/Player.as")));
			TestRunner->TestEqual(
				TEXT("Plugin source should keep its canonical virtual path"),
				Sources[1].VirtualPath.ToString(),
				FString(TEXT("/Angelscript/Plugin/Inventory/Gameplay/Item.as")));
		}

		TArray<FAngelscriptEngine::FFilenamePair> FilenamePairs;
		Engine.FindAllScriptFilenames(FilenamePairs);
		TestRunner->TestEqual(TEXT("Filename compatibility adapter should use provider-backed discovery"), FilenamePairs.Num(), 2);
		if (FilenamePairs.Num() == 2)
		{
			TestRunner->TestEqual(
				TEXT("Filename adapter should preserve provider virtual path"),
				FilenamePairs[1].VirtualPath,
				FString(TEXT("/Angelscript/Plugin/Inventory/Gameplay/Item.as")));
		}
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

		TestRunner->TestTrue(TEXT("Provider-loaded source should preprocess"), Preprocessor.Preprocess());
		TestRunner->TestEqual(TEXT("Provider should be asked to load disk-backed source once"), Provider->LoadSourceTextCallCount, 1);

		TArray<TSharedRef<FAngelscriptModuleDesc>> Modules = Preprocessor.GetModulesToCompile();
		TestRunner->TestEqual(TEXT("Provider-loaded source should emit one module"), Modules.Num(), 1);
		if (Modules.Num() == 1)
		{
			TestRunner->TestEqual(TEXT("Provider-loaded module should keep the source module name"), Modules[0]->ModuleName, FString(TEXT("Provider.Loaded")));
			TestRunner->TestEqual(TEXT("Provider-loaded module should keep source virtual path"), Modules[0]->Code[0].VirtualPath, Source.VirtualPath.ToString());
		}
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

		TestRunner->TestTrue(TEXT("Memory source should preprocess without provider text loading"), Preprocessor.Preprocess());
		TestRunner->TestEqual(TEXT("Provider should not load memory-backed source text"), Provider->LoadSourceTextCallCount, 0);
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
		if (!TestRunner->TestTrue(
				TEXT("Project provider fixture should write a script file"),
				WriteProviderFixtureFile(ProjectRoot, TEXT("Gameplay/Player.as"), TEXT("int PlayerEntry() { return 1; }"), ProjectScriptPath))
			|| !TestRunner->TestTrue(
				TEXT("Plugin provider fixture should write a script file"),
				WriteProviderFixtureFile(PluginRoot, TEXT("Gameplay/Item.as"), TEXT("int ItemEntry() { return 2; }"), PluginScriptPath))
			|| !TestRunner->TestTrue(
				TEXT("Legacy provider fixture should write a script file"),
				WriteProviderFixtureFile(LegacyRoot, TEXT("Gameplay/Legacy.as"), TEXT("int LegacyEntry() { return 3; }"), LegacyScriptPath)))
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

		TestRunner->TestTrue(
			TEXT("Disk provider should discover project root as /Angelscript/Game"),
			SourcesByVirtualPath.Contains(TEXT("/Angelscript/Game/Gameplay/Player.as")));
		TestRunner->TestTrue(
			TEXT("Disk provider should discover plugin root with plugin mount name"),
			SourcesByVirtualPath.Contains(TEXT("/Angelscript/Plugin/Inventory/Gameplay/Item.as")));

		Engine.AllScriptRoots = {
			FAngelscriptSourceRoot::FromPluginRoot(TEXT("StalePlugin"), ProjectRoot),
		};
		Engine.AllRootPaths = {
			LegacyRoot,
		};

		TArray<FAngelscriptEngine::FFilenamePair> FilenamePairs;
		Engine.FindAllScriptFilenames(FilenamePairs);

		TestRunner->TestEqual(TEXT("Legacy AllRootPaths fallback should still discover one source"), FilenamePairs.Num(), 1);
		if (FilenamePairs.Num() == 1)
		{
			TestRunner->TestEqual(
				TEXT("Legacy AllRootPaths fallback should synthesize game virtual path metadata"),
				FilenamePairs[0].VirtualPath,
				FString(TEXT("/Angelscript/Game/Gameplay/Legacy.as")));
			TestRunner->TestEqual(
				TEXT("Legacy AllRootPaths fallback should keep the legacy absolute path"),
				NormalizePath(FilenamePairs[0].AbsolutePath),
				LegacyScriptPath);
		}
	}
};

#endif
