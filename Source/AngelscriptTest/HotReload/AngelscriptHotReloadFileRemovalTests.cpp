#include "CQTest.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"
#include "HotReload/AngelscriptDirectoryWatcherInternal.h"

#include "HAL/FileManager.h"
#include "IDirectoryWatcher.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadFileRemovalTests,
	"Angelscript.TestModule.HotReload.FileRemoval",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName RemovalModuleName = FName(TEXT("HotReloadFileRemovalTarget"));
	inline static const FString RemovalRelativeScriptPath = FString(TEXT("HotReload/FileRemoval/HotReloadFileRemovalTarget.as"));

	static FString GetFixtureRoot()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("HotReloadFileRemoval")));
	}

	static FString WriteFixture(const FString& RelativePath, const FString& Contents)
	{
		const FString AbsolutePath = FPaths::Combine(GetFixtureRoot(), RelativePath);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(AbsolutePath), true);
		FFileHelper::SaveStringToFile(Contents, *AbsolutePath);
		return AbsolutePath;
	}

	static void CleanupFixtureRoot()
	{
		IFileManager::Get().DeleteDirectory(*GetFixtureRoot(), false, true);
	}

	static void OverrideScriptRootsForFixture(FAngelscriptEngine& Engine, TArray<FString>& OutPreviousRootPaths, TArray<FAngelscriptSourceRoot>& OutPreviousScriptRoots)
	{
		OutPreviousRootPaths = Engine.AllRootPaths;
		OutPreviousScriptRoots = Engine.AllScriptRoots;
		Engine.AllRootPaths = { GetFixtureRoot() };
		Engine.AllScriptRoots.Reset();
	}

	static void RestoreScriptRoots(FAngelscriptEngine& Engine, const TArray<FString>& PreviousRootPaths, const TArray<FAngelscriptSourceRoot>& PreviousScriptRoots)
	{
		Engine.AllRootPaths = PreviousRootPaths;
		Engine.AllScriptRoots = PreviousScriptRoots;
	}

	static int32 CountDeletionQueueEntry(const FAngelscriptEngine& Engine, const FString& RelativePath)
	{
		int32 Count = 0;
		for (const FAngelscriptEngine::FFilenamePair& Filename : Engine.FileDeletionsDetectedForReload)
		{
			if (Filename.RelativePath.Equals(RelativePath, ESearchCase::IgnoreCase))
			{
				++Count;
			}
		}
		return Count;
	}

	static const FAngelscriptEngine::FFilenamePair* FindDeletionQueueEntry(const FAngelscriptEngine& Engine, const FString& RelativePath)
	{
		return Engine.FileDeletionsDetectedForReload.FindByPredicate(
			[&RelativePath](const FAngelscriptEngine::FFilenamePair& Filename)
			{
				return Filename.RelativePath.Equals(RelativePath, ESearchCase::IgnoreCase);
			});
	}

public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(RemovedScriptFileQueuesDeletionReload)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UHotReloadFileRemovalTarget : UObject
			{
				UFUNCTION()
				int GetValue()
				{
					return 5;
				}
			}
			)AS");

		TArray<FString> PreviousRootPaths;
		TArray<FAngelscriptSourceRoot> PreviousScriptRoots;
		OverrideScriptRootsForFixture(Engine, PreviousRootPaths, PreviousScriptRoots);

		const FString AbsoluteScriptPath = WriteFixture(RemovalRelativeScriptPath, ScriptSource);
		ON_SCOPE_EXIT
		{
			Engine.FileChangesDetectedForReload.Reset();
			Engine.FileDeletionsDetectedForReload.Reset();
			RestoreScriptRoots(Engine, PreviousRootPaths, PreviousScriptRoots);
			Engine.DiscardModule(*RemovalModuleName.ToString());
			CleanupFixtureRoot();
		};

		ASSERT_THAT(IsTrue(
			CompileModuleFromDiskPath(&Engine, RemovalModuleName, AbsoluteScriptPath),
			TEXT("File removal target should compile from disk before deletion")));
		ASSERT_THAT(IsNotNull(FindGeneratedClass(&Engine, TEXT("UHotReloadFileRemovalTarget")), TEXT("File removal target class should exist before deletion is queued")));

		Engine.FileChangesDetectedForReload.Reset();
		Engine.FileDeletionsDetectedForReload.Reset();

		ASSERT_THAT(IsTrue(IFileManager::Get().Delete(*AbsoluteScriptPath, false, true), TEXT("File removal test should remove the source fixture")));

		const FFileChangeData RemovedScriptChange(AbsoluteScriptPath, FFileChangeData::FCA_Removed);
		AngelscriptEditor::Private::QueueScriptFileChanges(
			{ RemovedScriptChange },
			{ GetFixtureRoot() },
			Engine,
			IFileManager::Get(),
			[](const FString&)
			{
				return TArray<FAngelscriptEngine::FFilenamePair>();
			});

		ASSERT_THAT(AreEqual(0, Engine.FileChangesDetectedForReload.Num(), TEXT("Removed .as file should not enter the changed-file queue")));
		ASSERT_THAT(AreEqual(1, Engine.FileDeletionsDetectedForReload.Num(), TEXT("Removed .as file should queue exactly one deletion reload")));
		ASSERT_THAT(AreEqual(1, CountDeletionQueueEntry(Engine, RemovalRelativeScriptPath), TEXT("Removed .as file should queue its relative script path once")));

		const FAngelscriptEngine::FFilenamePair* QueuedDeletion = FindDeletionQueueEntry(Engine, RemovalRelativeScriptPath);
		ASSERT_THAT(IsNotNull(QueuedDeletion, TEXT("Removed .as file should expose its deletion queue entry")));
		ASSERT_THAT(IsTrue(QueuedDeletion->AbsolutePath.Equals(AbsoluteScriptPath, ESearchCase::IgnoreCase), TEXT("Deletion queue should preserve the absolute script path")));
		ASSERT_THAT(IsFalse(QueuedDeletion->VirtualPath.IsEmpty(), TEXT("Deletion queue should preserve a virtual path for the removed script")));
	}

	TEST_METHOD(RemovedDirectoryQueuesLoadedScriptsUnderDirectory)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		const FString FirstRelativePath = TEXT("HotReload/FileRemoval/Folder/FirstRemoved.as");
		const FString SecondRelativePath = TEXT("HotReload/FileRemoval/Folder/Nested/SecondRemoved.as");
		const FString OutsideRelativePath = TEXT("HotReload/FileRemoval/OutsideRemoved.as");
		const FString AbsoluteFolderPath = FPaths::Combine(GetFixtureRoot(), TEXT("HotReload/FileRemoval/Folder"));
		const FString FirstAbsolutePath = FPaths::Combine(GetFixtureRoot(), FirstRelativePath);
		const FString SecondAbsolutePath = FPaths::Combine(GetFixtureRoot(), SecondRelativePath);
		const FString OutsideAbsolutePath = FPaths::Combine(GetFixtureRoot(), OutsideRelativePath);

		TArray<FString> PreviousRootPaths;
		TArray<FAngelscriptSourceRoot> PreviousScriptRoots;
		OverrideScriptRootsForFixture(Engine, PreviousRootPaths, PreviousScriptRoots);

		ON_SCOPE_EXIT
		{
			Engine.FileChangesDetectedForReload.Reset();
			Engine.FileDeletionsDetectedForReload.Reset();
			RestoreScriptRoots(Engine, PreviousRootPaths, PreviousScriptRoots);
			CleanupFixtureRoot();
		};

		IFileManager::Get().MakeDirectory(*FPaths::GetPath(FirstAbsolutePath), true);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(SecondAbsolutePath), true);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutsideAbsolutePath), true);
		FFileHelper::SaveStringToFile(TEXT("// first"), *FirstAbsolutePath);
		FFileHelper::SaveStringToFile(TEXT("// second"), *SecondAbsolutePath);
		FFileHelper::SaveStringToFile(TEXT("// outside"), *OutsideAbsolutePath);

		const FAngelscriptEngine::FFilenamePair LoadedFirst{ FirstAbsolutePath, FirstRelativePath, TEXT("/Game/HotReload/FileRemoval/Folder/FirstRemoved.as") };
		const FAngelscriptEngine::FFilenamePair LoadedSecond{ SecondAbsolutePath, SecondRelativePath, TEXT("/Game/HotReload/FileRemoval/Folder/Nested/SecondRemoved.as") };
		const FAngelscriptEngine::FFilenamePair LoadedOutside{ OutsideAbsolutePath, OutsideRelativePath, TEXT("/Game/HotReload/FileRemoval/OutsideRemoved.as") };

		Engine.FileChangesDetectedForReload.Reset();
		Engine.FileDeletionsDetectedForReload.Reset();

		const FFileChangeData RemovedDirectoryChange(AbsoluteFolderPath, FFileChangeData::FCA_Removed);
		AngelscriptEditor::Private::QueueScriptFileChanges(
			{ RemovedDirectoryChange },
			{ GetFixtureRoot() },
			Engine,
			IFileManager::Get(),
			[LoadedFirst, LoadedSecond, LoadedOutside](const FString& AbsoluteFolder)
			{
				const TArray<FAngelscriptEngine::FFilenamePair> Candidates = { LoadedFirst, LoadedSecond, LoadedOutside };
				TArray<FAngelscriptEngine::FFilenamePair> LoadedScripts;
				for (const FAngelscriptEngine::FFilenamePair& Candidate : Candidates)
				{
					if (Candidate.AbsolutePath.StartsWith(AbsoluteFolder, ESearchCase::IgnoreCase))
					{
						LoadedScripts.Add(Candidate);
					}
				}
				return LoadedScripts;
			});

		ASSERT_THAT(AreEqual(0, Engine.FileChangesDetectedForReload.Num(), TEXT("Removed directory should not queue changed-file reloads")));
		ASSERT_THAT(AreEqual(2, Engine.FileDeletionsDetectedForReload.Num(), TEXT("Removed directory should queue deletions for loaded scripts under it")));
		ASSERT_THAT(AreEqual(1, CountDeletionQueueEntry(Engine, FirstRelativePath), TEXT("Removed directory should queue the first loaded script once")));
		ASSERT_THAT(AreEqual(1, CountDeletionQueueEntry(Engine, SecondRelativePath), TEXT("Removed directory should queue the nested loaded script once")));
		ASSERT_THAT(AreEqual(0, CountDeletionQueueEntry(Engine, OutsideRelativePath), TEXT("Removed directory should not queue loaded scripts outside the removed folder")));
	}
};

#endif
