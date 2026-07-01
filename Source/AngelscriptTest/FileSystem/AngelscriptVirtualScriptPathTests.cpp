#include "AngelscriptEngine.h"
#include "AngelscriptSource.h"
#include "CQTest.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptVirtualScriptPathTest,
	"Angelscript.TestModule.FileSystem.VirtualScriptPaths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
static FString NormalizePath(const FString& InPath)
{
	FString Normalized = InPath;
	FPaths::NormalizeFilename(Normalized);
	Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));
	return Normalized;
}

static FString GetDiscoveryFixtureRoot()
{
	return NormalizePath(FPaths::ProjectSavedDir() / TEXT("Automation") / TEXT("VirtualScriptPathDiscovery"));
}

static void CleanDiscoveryFixtureRoot()
{
	IFileManager::Get().DeleteDirectory(*GetDiscoveryFixtureRoot(), false, true);
}

static bool WriteDiscoveryFixtureFile(const FString& AbsoluteRoot, const FString& RelativePath, const FString& Content, FString& OutAbsolutePath)
{
	OutAbsolutePath = NormalizePath(FPaths::Combine(AbsoluteRoot, RelativePath));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutAbsolutePath), true);
	return FFileHelper::SaveStringToFile(Content, *OutAbsolutePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

static bool ExpectInvalidPath(FAutomationTestBase& Test, const FString& Path)
{
	FAngelscriptVirtualPath VirtualPath;
	FString Error;
	const bool bParsed = FAngelscriptVirtualPath::TryParse(Path, VirtualPath, &Error);
	Test.TestFalse(*FString::Printf(TEXT("Path '%s' should be rejected"), *Path), bParsed);
	Test.TestFalse(*FString::Printf(TEXT("Path '%s' should report a parse error"), *Path), Error.IsEmpty());
	return !bParsed && !Error.IsEmpty();
}

public:
	TEST_METHOD(SourceDescriptorsKeepFullNames)
	{
		const FAngelscriptSource GameSource = FAngelscriptSource::FromGameFile(
			TEXT("Gameplay/Enemy.as"),
			TEXT("D:/Project/Script/Gameplay/Enemy.as"));
		const FAngelscriptSource PluginSource = FAngelscriptSource::FromPluginFile(
			TEXT("Inventory"),
			TEXT("Gameplay/Item.as"),
			TEXT("D:/Project/Plugins/Inventory/Script/Gameplay/Item.as"));
		const FAngelscriptSource PluginRootSource = FAngelscriptSource::FromPluginFile(
			TEXT("Inventory"),
			TEXT("Item.as"),
			TEXT("D:/Project/Plugins/Inventory/Script/Item.as"));
		const FAngelscriptSource MemorySource = FAngelscriptSource::FromMemorySource(
			TEXT("/Angelscript/Memory/Immediate/Snippet_001.as"),
			TEXT("int Entry() { return 1; }"));

		ASSERT_THAT(AreEqual(FString(TEXT("/Angelscript/Game/Gameplay/Enemy.as")), GameSource.VirtualPath.ToString(), TEXT("Game source descriptor should keep full /Angelscript/Game name")));
		ASSERT_THAT(AreEqual(FString(TEXT("/Angelscript/Plugin/Inventory/Gameplay/Item.as")), PluginSource.VirtualPath.ToString(), TEXT("Plugin source descriptor should keep full /Angelscript/Plugin name")));
		ASSERT_THAT(AreEqual(FString(TEXT("/Angelscript/Plugin/Inventory/Item.as")), PluginRootSource.VirtualPath.ToString(), TEXT("Plugin root source descriptor should keep full /Angelscript/Plugin name")));
		ASSERT_THAT(AreEqual(FString(TEXT("/Angelscript/Memory/Immediate/Snippet_001.as")), MemorySource.VirtualPath.ToString(), TEXT("Memory source descriptor should keep full /Angelscript/Memory name")));
		ASSERT_THAT(AreEqual(FString(TEXT("Gameplay.Enemy")), GameSource.ModuleName, TEXT("Game descriptor module name should remain disk-compatible")));
		ASSERT_THAT(AreEqual(FString(TEXT("Gameplay.Item")), PluginSource.ModuleName, TEXT("Plugin descriptor module name should remain disk-compatible")));
		ASSERT_THAT(AreEqual(FString(TEXT("Item")), PluginRootSource.ModuleName, TEXT("Plugin root descriptor module name should remain disk-compatible")));
		ASSERT_THAT(AreEqual(FString(TEXT("Angelscript.Memory.Immediate.Snippet_001")), MemorySource.ModuleName, TEXT("Memory descriptor module name should be isolated by full memory root")));
	}

	TEST_METHOD(CanonicalRoots)
	{
		FAngelscriptVirtualPath GamePath;
		FAngelscriptVirtualPath PluginPath;
		FAngelscriptVirtualPath PluginRootPath;
		FAngelscriptVirtualPath MemoryPath;

		ASSERT_THAT(IsTrue(FAngelscriptVirtualPath::TryParse(TEXT("/Angelscript/Game/Gameplay/Enemy.as"), GamePath), TEXT("Game path should parse")));
		ASSERT_THAT(IsTrue(FAngelscriptVirtualPath::TryParse(TEXT("/Angelscript/Plugin/Inventory/Gameplay/Item.as"), PluginPath), TEXT("Plugin path should parse")));
		ASSERT_THAT(IsTrue(FAngelscriptVirtualPath::TryParse(TEXT("/Angelscript/Plugin/Inventory/Item.as"), PluginRootPath), TEXT("Plugin root path should parse")));
		ASSERT_THAT(IsTrue(FAngelscriptVirtualPath::TryParse(TEXT("/Angelscript/Memory/Immediate/Snippet_001.as"), MemoryPath), TEXT("Memory immediate path should parse")));

		ASSERT_THAT(AreEqual(FString(TEXT("/Angelscript/Game/Gameplay/Enemy.as")), GamePath.ToString(), TEXT("Game path should normalize slashes")));
		ASSERT_THAT(AreEqual(EAngelscriptSourceKind::Game, GamePath.GetSourceKind(), TEXT("Game source kind should be game")));
		ASSERT_THAT(AreEqual(FString(TEXT("Gameplay/Enemy.as")), GamePath.GetRelativePath(), TEXT("Game relative path should drop mount root")));
		ASSERT_THAT(AreEqual(FString(TEXT("Gameplay.Enemy")), GamePath.ToModuleName(), TEXT("Game module name should stay compatible")));

		ASSERT_THAT(AreEqual(EAngelscriptSourceKind::Plugin, PluginPath.GetSourceKind(), TEXT("Plugin source kind should be plugin")));
		ASSERT_THAT(AreEqual(FString(TEXT("Inventory")), PluginPath.GetMountName(), TEXT("Plugin mount name should be retained")));
		ASSERT_THAT(AreEqual(FString(TEXT("Gameplay/Item.as")), PluginPath.GetRelativePath(), TEXT("Plugin relative path should drop plugin name")));
		ASSERT_THAT(AreEqual(FString(TEXT("Gameplay.Item")), PluginPath.ToModuleName(), TEXT("Plugin module name should stay root-relative in v1")));
		ASSERT_THAT(AreEqual(FString(TEXT("/Angelscript/Plugin/Inventory/Item.as")), PluginRootPath.ToString(), TEXT("Plugin root path should normalize slashes")));
		ASSERT_THAT(AreEqual(FString(TEXT("Item.as")), PluginRootPath.GetRelativePath(), TEXT("Plugin root relative path should drop plugin name")));
		ASSERT_THAT(AreEqual(FString(TEXT("Item")), PluginRootPath.ToModuleName(), TEXT("Plugin root module name should stay root-relative in v1")));

		ASSERT_THAT(AreEqual(EAngelscriptSourceKind::Memory, MemoryPath.GetSourceKind(), TEXT("Memory source kind should be memory")));
		ASSERT_THAT(AreEqual(FString(TEXT("Immediate")), MemoryPath.GetMountName(), TEXT("Memory provider should be retained")));
		ASSERT_THAT(AreEqual(FString(TEXT("Snippet_001.as")), MemoryPath.GetRelativePath(), TEXT("Memory relative path should drop provider name")));
		ASSERT_THAT(AreEqual(FString(TEXT("Angelscript.Memory.Immediate.Snippet_001")), MemoryPath.ToModuleName(), TEXT("Memory module name should be isolated")));
	}

	TEST_METHOD(MemorySourceRequiresMemoryRoot)
	{
		FAngelscriptSource Source;
		FString Error;

		ASSERT_THAT(IsFalse(FAngelscriptSource::TryFromMemorySource(
			TEXT("/Angelscript/Game/Gameplay/Foo.as"),
			TEXT("int Entry() { return 1; }"),
			Source,
			&Error), TEXT("Memory source descriptors should reject game virtual paths")));
		ASSERT_THAT(IsFalse(Error.IsEmpty(), TEXT("Rejected game memory source should report a parse error")));
		ASSERT_THAT(IsFalse(Source.VirtualPath.IsValid(), TEXT("Rejected game memory source should not leave a valid descriptor")));

		Error.Reset();
		ASSERT_THAT(IsFalse(FAngelscriptSource::TryFromMemorySource(
			TEXT("/Angelscript/Plugin/Inventory/Gameplay/Foo.as"),
			TEXT("int Entry() { return 1; }"),
			Source,
			&Error), TEXT("Memory source descriptors should reject plugin virtual paths")));
		ASSERT_THAT(IsFalse(Error.IsEmpty(), TEXT("Rejected plugin memory source should report a parse error")));
		ASSERT_THAT(IsFalse(Source.VirtualPath.IsValid(), TEXT("Rejected plugin memory source should not leave a valid descriptor")));
	}

	TEST_METHOD(InvalidInputs)
	{
ASSERT_THAT(IsTrue(ExpectInvalidPath(*TestRunner, TEXT("as://project/Gameplay/Enemy.as")), TEXT("as:// path should be rejected")));
		ASSERT_THAT(IsTrue(ExpectInvalidPath(*TestRunner, TEXT("/Game/Gameplay/Enemy.as")), TEXT("/Game path should be rejected")));
		ASSERT_THAT(IsTrue(ExpectInvalidPath(*TestRunner, TEXT("/angelscript/Game/Gameplay/Enemy.as")), TEXT("case-mismatched root should be rejected")));
		ASSERT_THAT(IsTrue(ExpectInvalidPath(*TestRunner, TEXT("/Angelscript/Game/Gameplay/Enemy.txt")), TEXT("non-AS file should be rejected")));
		ASSERT_THAT(IsTrue(ExpectInvalidPath(*TestRunner, TEXT("/Angelscript/Game/Gameplay//Enemy.as")), TEXT("double slash should be rejected")));
		ASSERT_THAT(IsTrue(ExpectInvalidPath(*TestRunner, TEXT("/Angelscript/Game/Gameplay/Enemy.as/")), TEXT("trailing slash should be rejected")));
		ASSERT_THAT(IsTrue(ExpectInvalidPath(*TestRunner, TEXT("/Angelscript/Game/Gameplay/../Enemy.as")), TEXT("parent segment should be rejected")));
		ASSERT_THAT(IsTrue(ExpectInvalidPath(*TestRunner, TEXT("/Angelscript/Game/Gameplay\\.as")), TEXT("backslash should be rejected")));
		ASSERT_THAT(IsTrue(ExpectInvalidPath(*TestRunner, TEXT("/Angelscript/Plugin/Inventory")), TEXT("plugin root without file should be rejected")));
		ASSERT_THAT(IsTrue(ExpectInvalidPath(*TestRunner, TEXT("/Angelscript/Plugin/../Gameplay/Enemy.as")), TEXT("plugin parent segment should be rejected")));
		ASSERT_THAT(IsTrue(ExpectInvalidPath(*TestRunner, TEXT("/Angelscript/Memory/../Snippet.as")), TEXT("memory parent segment should be rejected")));
		ASSERT_THAT(IsTrue(ExpectInvalidPath(*TestRunner, TEXT("/Angelscript/Memory/Immediate")), TEXT("memory root without file should be rejected")));
	}

	TEST_METHOD(DiscoveryFullNames)
	{
CleanDiscoveryFixtureRoot();
		ON_SCOPE_EXIT
		{
			CleanDiscoveryFixtureRoot();
		};

	const FString FixtureRoot = GetDiscoveryFixtureRoot();
	const FString ProjectDir = NormalizePath(FixtureRoot / TEXT("Project"));
	const FString ProjectScriptRoot = NormalizePath(ProjectDir / TEXT("Script"));
	const FString PluginScriptRoot = NormalizePath(FixtureRoot / TEXT("Plugins") / TEXT("Inventory") / TEXT("Script"));

	FString ProjectScriptPath;
	FString PluginScriptPath;
		ASSERT_THAT(IsTrue(WriteDiscoveryFixtureFile(ProjectScriptRoot, TEXT("Gameplay/Player.as"), TEXT("int PlayerEntry() { return 17; }"), ProjectScriptPath), TEXT("Project discovery fixture should write a script file")));
		ASSERT_THAT(IsTrue(WriteDiscoveryFixtureFile(PluginScriptRoot, TEXT("Gameplay/Item.as"), TEXT("int ItemEntry() { return 23; }"), PluginScriptPath), TEXT("Plugin discovery fixture should write a script file")));

	FAngelscriptEngineConfig Config;
	Config.bIsEditor = false;

	FAngelscriptEngineDependencies Dependencies;
	Dependencies.GetProjectDir = [ProjectDir]()
	{
		return ProjectDir;
	};
	Dependencies.ConvertRelativePathToFull = [](const FString& Path)
	{
		return NormalizePath(Path);
	};
	Dependencies.DirectoryExists = [](const FString& Path)
	{
		return IFileManager::Get().DirectoryExists(*Path);
	};
	Dependencies.MakeDirectory = [](const FString& Path, const bool bTree)
	{
		return IFileManager::Get().MakeDirectory(*Path, bTree);
	};
	Dependencies.GetEnabledPluginScriptRoots = [PluginScriptRoot]()
	{
		return TArray<FString>{PluginScriptRoot};
	};
	Dependencies.GetEnabledPluginScriptRootDescriptors = [PluginScriptRoot]()
	{
		return TArray<FAngelscriptPluginScriptRoot>{
			FAngelscriptPluginScriptRoot{TEXT("Inventory"), PluginScriptRoot}
		};
	};

	FAngelscriptEngine Engine(Config, Dependencies);
	Engine.AllScriptRoots = Engine.DiscoverScriptRootDescriptors(false);

	TArray<FAngelscriptSource> Sources;
	Engine.FindAllScriptSources(Sources);

		ASSERT_THAT(AreEqual(2, Sources.Num(), TEXT("Descriptor discovery should find project and plugin sources")));

	TMap<FString, FAngelscriptSource> SourceByVirtualPath;
	for (const FAngelscriptSource& Source : Sources)
	{
		SourceByVirtualPath.Add(Source.VirtualPath.ToString(), Source);
	}

	const FAngelscriptSource* GameSource = SourceByVirtualPath.Find(TEXT("/Angelscript/Game/Gameplay/Player.as"));
	const FAngelscriptSource* PluginSource = SourceByVirtualPath.Find(TEXT("/Angelscript/Plugin/Inventory/Gameplay/Item.as"));

		ASSERT_THAT(IsNotNull(GameSource, TEXT("Discovery should emit the full game virtual path")));
		ASSERT_THAT(IsNotNull(PluginSource, TEXT("Discovery should emit the full plugin virtual path")));

		ASSERT_THAT(AreEqual(ProjectScriptPath, NormalizePath(GameSource->AbsoluteFilename), TEXT("Game source should retain absolute filename")));
		ASSERT_THAT(AreEqual(FString(TEXT("Gameplay.Player")), GameSource->ModuleName, TEXT("Game source should keep disk-compatible module name")));
		ASSERT_THAT(AreEqual(EAngelscriptSourceKind::Game, GameSource->SourceKind, TEXT("Game source should be marked as game")));

		ASSERT_THAT(AreEqual(PluginScriptPath, NormalizePath(PluginSource->AbsoluteFilename), TEXT("Plugin source should retain absolute filename")));
		ASSERT_THAT(AreEqual(FString(TEXT("Gameplay.Item")), PluginSource->ModuleName, TEXT("Plugin source should keep disk-compatible module name")));
		ASSERT_THAT(AreEqual(EAngelscriptSourceKind::Plugin, PluginSource->SourceKind, TEXT("Plugin source should be marked as plugin")));
		ASSERT_THAT(AreEqual(FString(TEXT("Inventory")), PluginSource->VirtualPath.GetMountName(), TEXT("Plugin source should keep the plugin mount name")));

	TArray<FAngelscriptEngine::FFilenamePair> FilenamePairs;
	Engine.FindAllScriptFilenames(FilenamePairs);
	TMap<FString, FAngelscriptEngine::FFilenamePair> FilenameByVirtualPath;
	for (const FAngelscriptEngine::FFilenamePair& FilenamePair : FilenamePairs)
	{
		FilenameByVirtualPath.Add(FilenamePair.VirtualPath, FilenamePair);
	}

		ASSERT_THAT(IsTrue(FilenameByVirtualPath.Contains(TEXT("/Angelscript/Game/Gameplay/Player.as")), TEXT("Legacy filename discovery should preserve the full game virtual path metadata")));
		ASSERT_THAT(IsTrue(FilenameByVirtualPath.Contains(TEXT("/Angelscript/Plugin/Inventory/Gameplay/Item.as")), TEXT("Legacy filename discovery should preserve the full plugin virtual path metadata")));
	}

	TEST_METHOD(LegacyRootPathOverrideWinsWhenDescriptorsAreStale)
	{
CleanDiscoveryFixtureRoot();
		ON_SCOPE_EXIT
		{
			CleanDiscoveryFixtureRoot();
		};

	const FString FixtureRoot = GetDiscoveryFixtureRoot();
	const FString LegacyScriptRoot = NormalizePath(FixtureRoot / TEXT("LegacyRoot"));
	const FString StaleDescriptorRoot = NormalizePath(FixtureRoot / TEXT("StaleDescriptorRoot"));

	FString LegacyScriptPath;
		ASSERT_THAT(IsTrue(WriteDiscoveryFixtureFile(LegacyScriptRoot, TEXT("Gameplay/Standalone.as"), TEXT("int StandaloneEntry() { return 41; }"), LegacyScriptPath), TEXT("Legacy root override fixture should write a script file")));

	FAngelscriptEngine Engine;
	Engine.AllScriptRoots = {
		FAngelscriptSourceRoot::FromPluginRoot(TEXT("StalePlugin"), StaleDescriptorRoot)
	};
	Engine.AllRootPaths = {
		LegacyScriptRoot
	};

	TArray<FAngelscriptEngine::FFilenamePair> FilenamePairs;
	Engine.FindAllScriptFilenames(FilenamePairs);

		ASSERT_THAT(AreEqual(1, FilenamePairs.Num(), TEXT("Legacy AllRootPaths override should be honored when AllScriptRoots are stale")));

		const FAngelscriptEngine::FFilenamePair& FilenamePair = FilenamePairs[0];
		ASSERT_THAT(AreEqual(LegacyScriptPath, NormalizePath(FilenamePair.AbsolutePath), TEXT("Legacy override should keep the discovered absolute filename")));
		ASSERT_THAT(AreEqual(FString(TEXT("Gameplay/Standalone.as")), FilenamePair.RelativePath.Replace(TEXT("\\"), TEXT("/")), TEXT("Legacy override should keep the root-relative filename")));
		ASSERT_THAT(AreEqual(FString(TEXT("/Angelscript/Game/Gameplay/Standalone.as")), FilenamePair.VirtualPath, TEXT("Legacy override should synthesize a game virtual path")));
	}
};

#endif
