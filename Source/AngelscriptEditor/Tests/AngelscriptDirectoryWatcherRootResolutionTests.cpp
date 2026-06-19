#include "CQTest.h"
#include "HotReload/AngelscriptDirectoryWatcherInternal.h"

#include "AngelscriptEngine.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

#define TestTrue(...) Test.TestTrue(__VA_ARGS__)
#define TestFalse(...) Test.TestFalse(__VA_ARGS__)
#define TestEqual(...) Test.TestEqual(__VA_ARGS__)
#define TestNotNull(...) Test.TestNotNull(__VA_ARGS__)
#define AddError(...) Test.AddError(__VA_ARGS__)

namespace AngelscriptEditor_Private_Tests_AngelscriptDirectoryWatcherRootResolutionTests_Private
{
	FString MakeTempWatcherRoot(const TCHAR* Prefix)
	{
		return FPaths::ConvertRelativePathToFull(
			FPaths::ProjectSavedDir() / TEXT("Automation") / TEXT("DirectoryWatcherTests") / FString::Printf(TEXT("%s_%s"), Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	}

	FFileChangeData MakeFileChange(const FString& Filename, FFileChangeData::EFileChangeAction Action)
	{
		return FFileChangeData(Filename, Action);
	}

	TUniquePtr<FAngelscriptEngine> MakeTestEngineWithRoots(const TArray<FString>& RootPaths)
	{
		const FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = MakeUnique<FAngelscriptEngine>(Config, Dependencies);

		for (const FString& RootPath : RootPaths)
		{
			Engine->AllRootPaths.Add(FPaths::ConvertRelativePathToFull(RootPath));
		}

		return Engine;
	}

	TUniquePtr<FAngelscriptEngine> MakeTestEngineWithScriptRoots(const TArray<FAngelscriptSourceRoot>& ScriptRoots)
	{
		const FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = MakeUnique<FAngelscriptEngine>(Config, Dependencies);

		for (const FAngelscriptSourceRoot& ScriptRoot : ScriptRoots)
		{
			FAngelscriptSourceRoot NormalizedRoot = ScriptRoot;
			NormalizedRoot.AbsolutePath = FPaths::ConvertRelativePathToFull(NormalizedRoot.AbsolutePath);
			Engine->AllScriptRoots.Add(NormalizedRoot);
			Engine->AllRootPaths.Add(NormalizedRoot.AbsolutePath);
		}

		return Engine;
	}

	bool ContainsFilenamePair(const TArray<FAngelscriptEngine::FFilenamePair>& Files, const FString& AbsolutePath, const FString& RelativePath)
	{
		for (const FAngelscriptEngine::FFilenamePair& File : Files)
		{
			if (File.AbsolutePath == AbsolutePath && File.RelativePath == RelativePath)
			{
				return true;
			}
		}

		return false;
	}
}


static bool RunUsesMatchingRootWhenMultipleRootsSharePrefix(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptDirectoryWatcherRootResolutionTests_Private;
	IFileManager& FileManager = IFileManager::Get();
	const FString BaseRoot = MakeTempWatcherRoot(TEXT("RootResolution"));
	const FString ProjectRoot = FPaths::ConvertRelativePathToFull(BaseRoot / TEXT("Scripts"));
	const FString PluginRoot = FPaths::ConvertRelativePathToFull(BaseRoot / TEXT("ScriptsPlugin"));
	ON_SCOPE_EXIT
	{
		FileManager.DeleteDirectory(*BaseRoot, false, true);
	};

	FileManager.MakeDirectory(*ProjectRoot, true);
	FileManager.MakeDirectory(*(PluginRoot / TEXT("Feature")), true);

	const FString PluginScriptAbsolutePath = FPaths::ConvertRelativePathToFull(PluginRoot / TEXT("Feature/Changed.as"));
	if (!FFileHelper::SaveStringToFile(TEXT("// Changed"), *PluginScriptAbsolutePath))
	{
		AddError(TEXT("DirectoryWatcher.Queue.UsesMatchingRootWhenMultipleRootsSharePrefix should create the plugin-root script fixture"));
		return false;
	}

	TUniquePtr<FAngelscriptEngine> Engine = MakeTestEngineWithRoots({ ProjectRoot, PluginRoot });
	Engine->LastFileChangeDetectedTime = 1234.0;

	int32 EnumerateCallCount = 0;
	const TArray<FFileChangeData> Changes = {
		MakeFileChange(PluginScriptAbsolutePath, FFileChangeData::FCA_Modified)
	};

	AngelscriptEditor::Private::QueueScriptFileChanges(Changes, Engine->AllRootPaths, *Engine, FileManager, [&](const FString&)
	{
		++EnumerateCallCount;
		return TArray<FAngelscriptEngine::FFilenamePair>();
	});

	if (!TestEqual(
			TEXT("DirectoryWatcher.Queue.UsesMatchingRootWhenMultipleRootsSharePrefix should queue exactly one modified script"),
			Engine->FileChangesDetectedForReload.Num(),
			1)
		|| !TestEqual(
			TEXT("DirectoryWatcher.Queue.UsesMatchingRootWhenMultipleRootsSharePrefix should not queue deletions for the modified script"),
			Engine->FileDeletionsDetectedForReload.Num(),
			0)
		|| !TestTrue(
			TEXT("DirectoryWatcher.Queue.UsesMatchingRootWhenMultipleRootsSharePrefix should advance the timestamp for an in-root script change"),
			Engine->LastFileChangeDetectedTime > 1234.0)
		|| !TestEqual(
			TEXT("DirectoryWatcher.Queue.UsesMatchingRootWhenMultipleRootsSharePrefix should not enumerate loaded scripts for file-level modified events"),
			EnumerateCallCount,
			0))
	{
		return false;
	}

	return TestTrue(
		TEXT("DirectoryWatcher.Queue.UsesMatchingRootWhenMultipleRootsSharePrefix should compute the relative path from the matching root instead of preserving the root segment"),
		ContainsFilenamePair(Engine->FileChangesDetectedForReload, PluginScriptAbsolutePath, TEXT("Feature/Changed.as")));
}

static bool RunPreservesPluginVirtualPath(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptDirectoryWatcherRootResolutionTests_Private;
	IFileManager& FileManager = IFileManager::Get();
	const FString BaseRoot = MakeTempWatcherRoot(TEXT("PluginVirtualPath"));
	const FString ProjectRoot = FPaths::ConvertRelativePathToFull(BaseRoot / TEXT("ProjectScript"));
	const FString PluginRoot = FPaths::ConvertRelativePathToFull(BaseRoot / TEXT("InventoryScript"));
	ON_SCOPE_EXIT
	{
		FileManager.DeleteDirectory(*BaseRoot, false, true);
	};

	FileManager.MakeDirectory(*ProjectRoot, true);
	FileManager.MakeDirectory(*(PluginRoot / TEXT("Feature")), true);

	const FString PluginScriptAbsolutePath = FPaths::ConvertRelativePathToFull(PluginRoot / TEXT("Feature/Changed.as"));
	if (!FFileHelper::SaveStringToFile(TEXT("// Changed"), *PluginScriptAbsolutePath))
	{
		AddError(TEXT("DirectoryWatcher.Queue.PreservesPluginVirtualPath should create the plugin-root script fixture"));
		return false;
	}

	TUniquePtr<FAngelscriptEngine> Engine = MakeTestEngineWithScriptRoots({
		FAngelscriptSourceRoot::FromGameRoot(ProjectRoot),
		FAngelscriptSourceRoot::FromPluginRoot(TEXT("Inventory"), PluginRoot),
	});

	const TArray<FFileChangeData> Changes = {
		MakeFileChange(PluginScriptAbsolutePath, FFileChangeData::FCA_Modified)
	};

	AngelscriptEditor::Private::QueueScriptFileChanges(Changes, Engine->AllRootPaths, *Engine, FileManager, [](const FString&)
	{
		return TArray<FAngelscriptEngine::FFilenamePair>();
	});

	if (!TestEqual(
			TEXT("DirectoryWatcher.Queue.PreservesPluginVirtualPath should queue one plugin script"),
			Engine->FileChangesDetectedForReload.Num(),
			1))
	{
		return false;
	}

	const FAngelscriptEngine::FFilenamePair& QueuedFile = Engine->FileChangesDetectedForReload[0];
	bool bPassed = true;
	bPassed &= TestEqual(TEXT("Queued plugin script should keep its absolute path"), QueuedFile.AbsolutePath, PluginScriptAbsolutePath);
	bPassed &= TestEqual(TEXT("Queued plugin script should keep its plugin-root relative path"), QueuedFile.RelativePath, FString(TEXT("Feature/Changed.as")));
	bPassed &= TestEqual(
		TEXT("Queued plugin script should keep its full plugin virtual path"),
		QueuedFile.VirtualPath,
		FString(TEXT("/Angelscript/Plugin/Inventory/Feature/Changed.as")));
	return bPassed;
}

#undef TestTrue
#undef TestFalse
#undef TestEqual
#undef TestNotNull
#undef AddError

TEST_CLASS_WITH_FLAGS(
	FAngelscriptDirectoryWatcherRootResolutionQueueTests,
	"Angelscript.Editor.DirectoryWatcher.Queue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(UsesMatchingRootWhenMultipleRootsSharePrefix)
	{
		ASSERT_THAT(IsTrue(RunUsesMatchingRootWhenMultipleRootsSharePrefix(*TestRunner)));
	}

	TEST_METHOD(PreservesPluginVirtualPath)
	{
		ASSERT_THAT(IsTrue(RunPreservesPluginVirtualPath(*TestRunner)));
	}
};

#endif
