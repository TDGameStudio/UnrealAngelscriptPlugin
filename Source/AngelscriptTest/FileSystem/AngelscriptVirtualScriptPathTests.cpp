#include "AngelscriptEngine.h"
#include "AngelscriptSource.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_FileSystem_VirtualScriptPaths_Private
{
	FString NormalizePath(const FString& InPath)
	{
		FString Normalized = InPath;
		FPaths::NormalizeFilename(Normalized);
		Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));
		return Normalized;
	}

	FString GetDiscoveryFixtureRoot()
	{
		return NormalizePath(FPaths::ProjectSavedDir() / TEXT("Automation") / TEXT("VirtualScriptPathDiscovery"));
	}

	void CleanDiscoveryFixtureRoot()
	{
		IFileManager::Get().DeleteDirectory(*GetDiscoveryFixtureRoot(), false, true);
	}

	bool WriteDiscoveryFixtureFile(const FString& AbsoluteRoot, const FString& RelativePath, const FString& Content, FString& OutAbsolutePath)
	{
		OutAbsolutePath = NormalizePath(FPaths::Combine(AbsoluteRoot, RelativePath));
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutAbsolutePath), true);
		return FFileHelper::SaveStringToFile(Content, *OutAbsolutePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	bool ExpectInvalidPath(FAutomationTestBase& Test, const FString& Path)
	{
		FAngelscriptVirtualPath VirtualPath;
		FString Error;
		const bool bParsed = FAngelscriptVirtualPath::TryParse(Path, VirtualPath, &Error);
		Test.TestFalse(*FString::Printf(TEXT("Path '%s' should be rejected"), *Path), bParsed);
		Test.TestFalse(*FString::Printf(TEXT("Path '%s' should report a parse error"), *Path), Error.IsEmpty());
		return !bParsed && !Error.IsEmpty();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptVirtualScriptPathSourceDescriptorsKeepFullNamesTest,
	"Angelscript.TestModule.FileSystem.VirtualScriptPaths.SourceDescriptorsKeepFullNames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptVirtualScriptPathSourceDescriptorsKeepFullNamesTest::RunTest(const FString& Parameters)
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

	bool bPassed = true;
	bPassed &= TestEqual(
		TEXT("Game source descriptor should keep full /Angelscript/Game name"),
		GameSource.VirtualPath.ToString(),
		FString(TEXT("/Angelscript/Game/Gameplay/Enemy.as")));
	bPassed &= TestEqual(
		TEXT("Plugin source descriptor should keep full /Angelscript/Plugin name"),
		PluginSource.VirtualPath.ToString(),
		FString(TEXT("/Angelscript/Plugin/Inventory/Gameplay/Item.as")));
	bPassed &= TestEqual(
		TEXT("Plugin root source descriptor should keep full /Angelscript/Plugin name"),
		PluginRootSource.VirtualPath.ToString(),
		FString(TEXT("/Angelscript/Plugin/Inventory/Item.as")));
	bPassed &= TestEqual(
		TEXT("Memory source descriptor should keep full /Angelscript/Memory name"),
		MemorySource.VirtualPath.ToString(),
		FString(TEXT("/Angelscript/Memory/Immediate/Snippet_001.as")));
	bPassed &= TestEqual(TEXT("Game descriptor module name should remain disk-compatible"), GameSource.ModuleName, FString(TEXT("Gameplay.Enemy")));
	bPassed &= TestEqual(TEXT("Plugin descriptor module name should remain disk-compatible"), PluginSource.ModuleName, FString(TEXT("Gameplay.Item")));
	bPassed &= TestEqual(TEXT("Plugin root descriptor module name should remain disk-compatible"), PluginRootSource.ModuleName, FString(TEXT("Item")));
	bPassed &= TestEqual(TEXT("Memory descriptor module name should be isolated by full memory root"), MemorySource.ModuleName, FString(TEXT("Angelscript.Memory.Immediate.Snippet_001")));
	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptVirtualScriptPathCanonicalRootsTest,
	"Angelscript.TestModule.FileSystem.VirtualScriptPaths.CanonicalRoots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptVirtualScriptPathCanonicalRootsTest::RunTest(const FString& Parameters)
{
	FAngelscriptVirtualPath GamePath;
	FAngelscriptVirtualPath PluginPath;
	FAngelscriptVirtualPath PluginRootPath;
	FAngelscriptVirtualPath MemoryPath;

	bool bPassed = true;
	bPassed &= TestTrue(
		TEXT("Game path should parse"),
		FAngelscriptVirtualPath::TryParse(TEXT("/Angelscript/Game/Gameplay/Enemy.as"), GamePath));
	bPassed &= TestTrue(
		TEXT("Plugin path should parse"),
		FAngelscriptVirtualPath::TryParse(TEXT("/Angelscript/Plugin/Inventory/Gameplay/Item.as"), PluginPath));
	bPassed &= TestTrue(
		TEXT("Plugin root path should parse"),
		FAngelscriptVirtualPath::TryParse(TEXT("/Angelscript/Plugin/Inventory/Item.as"), PluginRootPath));
	bPassed &= TestTrue(
		TEXT("Memory immediate path should parse"),
		FAngelscriptVirtualPath::TryParse(TEXT("/Angelscript/Memory/Immediate/Snippet_001.as"), MemoryPath));

	if (!bPassed)
	{
		return false;
	}

	bPassed &= TestEqual(TEXT("Game path should normalize slashes"), GamePath.ToString(), FString(TEXT("/Angelscript/Game/Gameplay/Enemy.as")));
	bPassed &= TestEqual(TEXT("Game source kind should be game"), GamePath.GetSourceKind(), EAngelscriptSourceKind::Game);
	bPassed &= TestEqual(TEXT("Game relative path should drop mount root"), GamePath.GetRelativePath(), FString(TEXT("Gameplay/Enemy.as")));
	bPassed &= TestEqual(TEXT("Game module name should stay compatible"), GamePath.ToModuleName(), FString(TEXT("Gameplay.Enemy")));

	bPassed &= TestEqual(TEXT("Plugin source kind should be plugin"), PluginPath.GetSourceKind(), EAngelscriptSourceKind::Plugin);
	bPassed &= TestEqual(TEXT("Plugin mount name should be retained"), PluginPath.GetMountName(), FString(TEXT("Inventory")));
	bPassed &= TestEqual(TEXT("Plugin relative path should drop plugin name"), PluginPath.GetRelativePath(), FString(TEXT("Gameplay/Item.as")));
	bPassed &= TestEqual(TEXT("Plugin module name should stay root-relative in v1"), PluginPath.ToModuleName(), FString(TEXT("Gameplay.Item")));
	bPassed &= TestEqual(TEXT("Plugin root path should normalize slashes"), PluginRootPath.ToString(), FString(TEXT("/Angelscript/Plugin/Inventory/Item.as")));
	bPassed &= TestEqual(TEXT("Plugin root relative path should drop plugin name"), PluginRootPath.GetRelativePath(), FString(TEXT("Item.as")));
	bPassed &= TestEqual(TEXT("Plugin root module name should stay root-relative in v1"), PluginRootPath.ToModuleName(), FString(TEXT("Item")));

	bPassed &= TestEqual(TEXT("Memory source kind should be memory"), MemoryPath.GetSourceKind(), EAngelscriptSourceKind::Memory);
	bPassed &= TestEqual(TEXT("Memory provider should be retained"), MemoryPath.GetMountName(), FString(TEXT("Immediate")));
	bPassed &= TestEqual(TEXT("Memory relative path should drop provider name"), MemoryPath.GetRelativePath(), FString(TEXT("Snippet_001.as")));
	bPassed &= TestEqual(TEXT("Memory module name should be isolated"), MemoryPath.ToModuleName(), FString(TEXT("Angelscript.Memory.Immediate.Snippet_001")));

	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptVirtualScriptPathMemorySourceRequiresMemoryRootTest,
	"Angelscript.TestModule.FileSystem.VirtualScriptPaths.MemorySourceRequiresMemoryRoot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptVirtualScriptPathMemorySourceRequiresMemoryRootTest::RunTest(const FString& Parameters)
{
	FAngelscriptSource Source;
	FString Error;

	bool bPassed = true;
	bPassed &= TestFalse(
		TEXT("Memory source descriptors should reject game virtual paths"),
		FAngelscriptSource::TryFromMemorySource(
			TEXT("/Angelscript/Game/Gameplay/Foo.as"),
			TEXT("int Entry() { return 1; }"),
			Source,
			&Error));
	bPassed &= TestFalse(TEXT("Rejected game memory source should report a parse error"), Error.IsEmpty());
	bPassed &= TestFalse(TEXT("Rejected game memory source should not leave a valid descriptor"), Source.VirtualPath.IsValid());

	Error.Reset();
	bPassed &= TestFalse(
		TEXT("Memory source descriptors should reject plugin virtual paths"),
		FAngelscriptSource::TryFromMemorySource(
			TEXT("/Angelscript/Plugin/Inventory/Gameplay/Foo.as"),
			TEXT("int Entry() { return 1; }"),
			Source,
			&Error));
	bPassed &= TestFalse(TEXT("Rejected plugin memory source should report a parse error"), Error.IsEmpty());
	bPassed &= TestFalse(TEXT("Rejected plugin memory source should not leave a valid descriptor"), Source.VirtualPath.IsValid());

	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptVirtualScriptPathInvalidInputsTest,
	"Angelscript.TestModule.FileSystem.VirtualScriptPaths.InvalidInputs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptVirtualScriptPathInvalidInputsTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_FileSystem_VirtualScriptPaths_Private;

	bool bPassed = true;
	bPassed &= ExpectInvalidPath(*this, TEXT("as://project/Gameplay/Enemy.as"));
	bPassed &= ExpectInvalidPath(*this, TEXT("/Game/Gameplay/Enemy.as"));
	bPassed &= ExpectInvalidPath(*this, TEXT("/angelscript/Game/Gameplay/Enemy.as"));
	bPassed &= ExpectInvalidPath(*this, TEXT("/Angelscript/Game/Gameplay/Enemy.txt"));
	bPassed &= ExpectInvalidPath(*this, TEXT("/Angelscript/Game/Gameplay//Enemy.as"));
	bPassed &= ExpectInvalidPath(*this, TEXT("/Angelscript/Game/Gameplay/Enemy.as/"));
	bPassed &= ExpectInvalidPath(*this, TEXT("/Angelscript/Game/Gameplay/../Enemy.as"));
	bPassed &= ExpectInvalidPath(*this, TEXT("/Angelscript/Game/Gameplay\\.as"));
	bPassed &= ExpectInvalidPath(*this, TEXT("/Angelscript/Plugin/Inventory"));
	bPassed &= ExpectInvalidPath(*this, TEXT("/Angelscript/Plugin/../Gameplay/Enemy.as"));
	bPassed &= ExpectInvalidPath(*this, TEXT("/Angelscript/Memory/../Snippet.as"));
	bPassed &= ExpectInvalidPath(*this, TEXT("/Angelscript/Memory/Immediate"));
	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptVirtualScriptPathDiscoveryFullNamesTest,
	"Angelscript.TestModule.FileSystem.VirtualScriptPaths.DiscoveryFullNames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptVirtualScriptPathDiscoveryFullNamesTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_FileSystem_VirtualScriptPaths_Private;

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
	if (!TestTrue(
			TEXT("Project discovery fixture should write a script file"),
			WriteDiscoveryFixtureFile(ProjectScriptRoot, TEXT("Gameplay/Player.as"), TEXT("int PlayerEntry() { return 17; }"), ProjectScriptPath))
		|| !TestTrue(
			TEXT("Plugin discovery fixture should write a script file"),
			WriteDiscoveryFixtureFile(PluginScriptRoot, TEXT("Gameplay/Item.as"), TEXT("int ItemEntry() { return 23; }"), PluginScriptPath)))
	{
		return false;
	}

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

	bool bPassed = TestEqual(TEXT("Descriptor discovery should find project and plugin sources"), Sources.Num(), 2);

	TMap<FString, FAngelscriptSource> SourceByVirtualPath;
	for (const FAngelscriptSource& Source : Sources)
	{
		SourceByVirtualPath.Add(Source.VirtualPath.ToString(), Source);
	}

	const FAngelscriptSource* GameSource = SourceByVirtualPath.Find(TEXT("/Angelscript/Game/Gameplay/Player.as"));
	const FAngelscriptSource* PluginSource = SourceByVirtualPath.Find(TEXT("/Angelscript/Plugin/Inventory/Gameplay/Item.as"));

	bPassed &= TestNotNull(TEXT("Discovery should emit the full game virtual path"), GameSource);
	bPassed &= TestNotNull(TEXT("Discovery should emit the full plugin virtual path"), PluginSource);

	if (GameSource != nullptr)
	{
		bPassed &= TestEqual(TEXT("Game source should retain absolute filename"), NormalizePath(GameSource->AbsoluteFilename), ProjectScriptPath);
		bPassed &= TestEqual(TEXT("Game source should keep disk-compatible module name"), GameSource->ModuleName, FString(TEXT("Gameplay.Player")));
		bPassed &= TestEqual(TEXT("Game source should be marked as game"), GameSource->SourceKind, EAngelscriptSourceKind::Game);
	}

	if (PluginSource != nullptr)
	{
		bPassed &= TestEqual(TEXT("Plugin source should retain absolute filename"), NormalizePath(PluginSource->AbsoluteFilename), PluginScriptPath);
		bPassed &= TestEqual(TEXT("Plugin source should keep disk-compatible module name"), PluginSource->ModuleName, FString(TEXT("Gameplay.Item")));
		bPassed &= TestEqual(TEXT("Plugin source should be marked as plugin"), PluginSource->SourceKind, EAngelscriptSourceKind::Plugin);
		bPassed &= TestEqual(TEXT("Plugin source should keep the plugin mount name"), PluginSource->VirtualPath.GetMountName(), FString(TEXT("Inventory")));
	}

	TArray<FAngelscriptEngine::FFilenamePair> FilenamePairs;
	Engine.FindAllScriptFilenames(FilenamePairs);
	TMap<FString, FAngelscriptEngine::FFilenamePair> FilenameByVirtualPath;
	for (const FAngelscriptEngine::FFilenamePair& FilenamePair : FilenamePairs)
	{
		FilenameByVirtualPath.Add(FilenamePair.VirtualPath, FilenamePair);
	}

	bPassed &= TestTrue(
		TEXT("Legacy filename discovery should preserve the full game virtual path metadata"),
		FilenameByVirtualPath.Contains(TEXT("/Angelscript/Game/Gameplay/Player.as")));
	bPassed &= TestTrue(
		TEXT("Legacy filename discovery should preserve the full plugin virtual path metadata"),
		FilenameByVirtualPath.Contains(TEXT("/Angelscript/Plugin/Inventory/Gameplay/Item.as")));

	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptVirtualScriptPathLegacyRootPathOverrideTest,
	"Angelscript.TestModule.FileSystem.VirtualScriptPaths.LegacyRootPathOverrideWinsWhenDescriptorsAreStale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptVirtualScriptPathLegacyRootPathOverrideTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_FileSystem_VirtualScriptPaths_Private;

	CleanDiscoveryFixtureRoot();
	ON_SCOPE_EXIT
	{
		CleanDiscoveryFixtureRoot();
	};

	const FString FixtureRoot = GetDiscoveryFixtureRoot();
	const FString LegacyScriptRoot = NormalizePath(FixtureRoot / TEXT("LegacyRoot"));
	const FString StaleDescriptorRoot = NormalizePath(FixtureRoot / TEXT("StaleDescriptorRoot"));

	FString LegacyScriptPath;
	if (!TestTrue(
			TEXT("Legacy root override fixture should write a script file"),
			WriteDiscoveryFixtureFile(LegacyScriptRoot, TEXT("Gameplay/Standalone.as"), TEXT("int StandaloneEntry() { return 41; }"), LegacyScriptPath)))
	{
		return false;
	}

	FAngelscriptEngine Engine;
	Engine.AllScriptRoots = {
		FAngelscriptSourceRoot::FromPluginRoot(TEXT("StalePlugin"), StaleDescriptorRoot)
	};
	Engine.AllRootPaths = {
		LegacyScriptRoot
	};

	TArray<FAngelscriptEngine::FFilenamePair> FilenamePairs;
	Engine.FindAllScriptFilenames(FilenamePairs);

	if (!TestEqual(
			TEXT("Legacy AllRootPaths override should be honored when AllScriptRoots are stale"),
			FilenamePairs.Num(),
			1))
	{
		return false;
	}

	const FAngelscriptEngine::FFilenamePair& FilenamePair = FilenamePairs[0];
	bool bPassed = true;
	bPassed &= TestEqual(TEXT("Legacy override should keep the discovered absolute filename"), NormalizePath(FilenamePair.AbsolutePath), LegacyScriptPath);
	bPassed &= TestEqual(TEXT("Legacy override should keep the root-relative filename"), FilenamePair.RelativePath.Replace(TEXT("\\"), TEXT("/")), FString(TEXT("Gameplay/Standalone.as")));
	bPassed &= TestEqual(TEXT("Legacy override should synthesize a game virtual path"), FilenamePair.VirtualPath, FString(TEXT("/Angelscript/Game/Gameplay/Standalone.as")));
	return bPassed;
}

#endif
