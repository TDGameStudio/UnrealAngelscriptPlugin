// ============================================================================
// AngelscriptPreprocessorAsyncTests.cpp
//
// Preprocessor tests for asynchronous loading, async/sync parity, zero-byte
// file handling, and treat-as-deleted semantics.
//
// Migrated from:
//   - AngelscriptPreprocessorAsyncLoadTests.cpp (AsyncMatchesSynchronousPreprocess)
//   - AngelscriptPreprocessorAsyncTests.cpp (AsyncZeroByteFileMatchesSyncPath)
//   - AngelscriptPreprocessorDeletedFileTests.cpp (TreatAsDeletedProducesEmptyModule)
//
// Automation prefix: Angelscript.TestModule.Preprocessor.Async.*
// ============================================================================

#include "CQTest.h"
#include "Preprocessor/AngelscriptPreprocessorTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace PreprocessorTestHelpers;

// ============================================================================
// Test class
// ============================================================================

TEST_CLASS_WITH_FLAGS(FAngelscriptPreprocessorAsyncTest,
	"Angelscript.TestModule.Preprocessor.Async",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	// ========================================================================
	// Snapshot types for async/sync parity comparison
	// ========================================================================

	struct FCodeSectionSnapshot
	{
		FString RelativeFilename;
		FString AbsoluteFilename;
		FString Code;
		int64 CodeHash = 0;
	};

	struct FModuleSnapshot
	{
		FString ModuleName;
		TArray<FString> ImportedModules;
		TArray<FCodeSectionSnapshot> CodeSections;
		int64 CodeHash = 0;
	};

	// ========================================================================
	// Private helpers
	// ========================================================================

	static FString MakeAsyncLoadPadding(int32 RepeatCount)
	{
		FString Padding;
		Padding.Reserve(RepeatCount * 64);
		for (int32 Index = 0; Index < RepeatCount; ++Index)
		{
			Padding += FString::Printf(TEXT("// AsyncLoadPadding_%05d_abcdefghijklmnopqrstuvwxyz0123456789\n"), Index);
		}
		return Padding;
	}

	static TArray<FModuleSnapshot> CaptureModuleSnapshots(
		const TArray<TSharedRef<FAngelscriptModuleDesc>>& Modules)
	{
		TArray<FModuleSnapshot> Snapshots;
		Snapshots.Reserve(Modules.Num());
		for (const TSharedRef<FAngelscriptModuleDesc>& Module : Modules)
		{
			FModuleSnapshot Snapshot;
			Snapshot.ModuleName = Module->ModuleName;
			Snapshot.ImportedModules = Module->ImportedModules;
			Snapshot.CodeHash = Module->CodeHash;
			Snapshot.CodeSections.Reserve(Module->Code.Num());
			for (const FAngelscriptModuleDesc::FCodeSection& Section : Module->Code)
			{
				FCodeSectionSnapshot SectionSnapshot;
				SectionSnapshot.RelativeFilename = Section.RelativeFilename;
				SectionSnapshot.AbsoluteFilename = Section.AbsoluteFilename;
				SectionSnapshot.Code = Section.Code;
				SectionSnapshot.CodeHash = Section.CodeHash;
				Snapshot.CodeSections.Add(MoveTemp(SectionSnapshot));
			}
			Snapshots.Add(MoveTemp(Snapshot));
		}
		return Snapshots;
	}

	static TArray<FString> CollectDiagnosticMessages(
		const FAngelscriptEngine& Engine,
		const TArray<FString>& AbsoluteFilenames,
		int32& OutErrorCount)
	{
		TArray<FString> Messages;
		OutErrorCount = 0;
		for (const FString& AbsoluteFilename : AbsoluteFilenames)
		{
			const FAngelscriptEngine::FDiagnostics* Diagnostics = Engine.Diagnostics.Find(AbsoluteFilename);
			if (Diagnostics == nullptr)
			{
				continue;
			}
			for (const FAngelscriptEngine::FDiagnostic& Diagnostic : Diagnostics->Diagnostics)
			{
				Messages.Add(Diagnostic.Message);
				if (Diagnostic.bIsError)
				{
					++OutErrorCount;
				}
			}
		}
		return Messages;
	}

	bool TestModuleSnapshotsMatch(
		const TArray<FModuleSnapshot>& ExpectedModules,
		const TArray<FModuleSnapshot>& ActualModules)
	{
		bool bMatched = true;

		if (!this->Assert.AreEqual(
				ExpectedModules.Num(),
				ActualModules.Num(),
				TEXT("Async preprocessor path should emit the same module count as the synchronous path")))
		{
			return false;
		}

		for (int32 ModuleIndex = 0; ModuleIndex < ExpectedModules.Num(); ++ModuleIndex)
		{
			const FModuleSnapshot& Expected = ExpectedModules[ModuleIndex];
			const FModuleSnapshot& Actual = ActualModules[ModuleIndex];

			bMatched &= this->Assert.AreEqual(
				Expected.ModuleName,
				Actual.ModuleName,
				*FString::Printf(TEXT("Module[%d] name should match between sync and async preprocess"), ModuleIndex));

			bMatched &= this->Assert.AreEqual(
				Expected.ImportedModules.Num(),
				Actual.ImportedModules.Num(),
				*FString::Printf(TEXT("Module[%d] imported-module count should match between sync and async preprocess"), ModuleIndex));

			const int32 ComparedImportCount = FMath::Min(Expected.ImportedModules.Num(), Actual.ImportedModules.Num());
			for (int32 ImportIndex = 0; ImportIndex < ComparedImportCount; ++ImportIndex)
			{
				bMatched &= this->Assert.AreEqual(
					Expected.ImportedModules[ImportIndex],
					Actual.ImportedModules[ImportIndex],
					*FString::Printf(TEXT("Module[%d] import[%d] should keep the same dependency order"), ModuleIndex, ImportIndex));
			}

			bMatched &= this->Assert.AreEqual(
				Expected.CodeSections.Num(),
				Actual.CodeSections.Num(),
				*FString::Printf(TEXT("Module[%d] code section count should match between sync and async preprocess"), ModuleIndex));

			bMatched &= this->Assert.AreEqual(
				Expected.CodeHash,
				Actual.CodeHash,
				*FString::Printf(TEXT("Module[%d] aggregate code hash should match between sync and async preprocess"), ModuleIndex));

			const int32 ComparedSectionCount = FMath::Min(Expected.CodeSections.Num(), Actual.CodeSections.Num());
			for (int32 SectionIndex = 0; SectionIndex < ComparedSectionCount; ++SectionIndex)
			{
				const FCodeSectionSnapshot& ExpectedSection = Expected.CodeSections[SectionIndex];
				const FCodeSectionSnapshot& ActualSection = Actual.CodeSections[SectionIndex];

				bMatched &= this->Assert.AreEqual(
					ExpectedSection.RelativeFilename,
					ActualSection.RelativeFilename,
					*FString::Printf(TEXT("Module[%d] section[%d] relative filename should match"), ModuleIndex, SectionIndex));

				bMatched &= this->Assert.AreEqual(
					ExpectedSection.AbsoluteFilename,
					ActualSection.AbsoluteFilename,
					*FString::Printf(TEXT("Module[%d] section[%d] absolute filename should match"), ModuleIndex, SectionIndex));

				bMatched &= this->Assert.AreEqual(
					ExpectedSection.CodeHash,
					ActualSection.CodeHash,
					*FString::Printf(TEXT("Module[%d] section[%d] code hash should match"), ModuleIndex, SectionIndex));

				bMatched &= this->Assert.IsTrue(
					ActualSection.Code == ExpectedSection.Code,
					*FString::Printf(TEXT("Module[%d] section[%d] processed code should match exactly"), ModuleIndex, SectionIndex));
			}
		}

		return bMatched;
	}

	bool CompareZeroByteSnapshots(
		const FModuleSnapshot& Expected,
		const FModuleSnapshot& Actual)
	{
		bool bMatched = true;

		bMatched &= this->Assert.AreEqual(
			Expected.ModuleName,
			Actual.ModuleName,
			TEXT("Async zero-byte preprocess should keep the same module name as the sync path"));

		bMatched &= this->Assert.AreEqual(
			Expected.ImportedModules.Num(),
			Actual.ImportedModules.Num(),
			TEXT("Async zero-byte preprocess should keep the same imported-module count as the sync path"));

		bMatched &= this->Assert.AreEqual(
			Expected.CodeSections.Num(),
			Actual.CodeSections.Num(),
			TEXT("Async zero-byte preprocess should keep the same code section count as the sync path"));

		bMatched &= this->Assert.AreEqual(
			0ll,
			Expected.CodeHash,
			TEXT("Synchronous zero-byte preprocess should keep a zero aggregate code hash"));

		bMatched &= this->Assert.AreEqual(
			Expected.CodeHash,
			Actual.CodeHash,
			TEXT("Async zero-byte preprocess should keep the same aggregate code hash as the sync path"));

		bMatched &= this->Assert.AreEqual(
			0,
			Expected.ImportedModules.Num(),
			TEXT("Synchronous zero-byte preprocess should keep zero imported modules"));

		const int32 ComparedSectionCount = FMath::Min(Expected.CodeSections.Num(), Actual.CodeSections.Num());
		for (int32 SectionIndex = 0; SectionIndex < ComparedSectionCount; ++SectionIndex)
		{
			const FCodeSectionSnapshot& ExpectedSection = Expected.CodeSections[SectionIndex];
			const FCodeSectionSnapshot& ActualSection = Actual.CodeSections[SectionIndex];

			bMatched &= this->Assert.AreEqual(
				ExpectedSection.RelativeFilename,
				ActualSection.RelativeFilename,
				*FString::Printf(TEXT("Code section[%d] relative filename should match between sync and async zero-byte preprocess"), SectionIndex));

			bMatched &= this->Assert.AreEqual(
				ExpectedSection.AbsoluteFilename,
				ActualSection.AbsoluteFilename,
				*FString::Printf(TEXT("Code section[%d] absolute filename should match between sync and async zero-byte preprocess"), SectionIndex));

			bMatched &= this->Assert.AreEqual(
				ExpectedSection.CodeHash,
				ActualSection.CodeHash,
				*FString::Printf(TEXT("Code section[%d] hash should match between sync and async zero-byte preprocess"), SectionIndex));

			bMatched &= this->Assert.AreEqual(
				0ll,
				ExpectedSection.CodeHash,
				*FString::Printf(TEXT("Synchronous zero-byte preprocess should keep code section[%d] hash at zero"), SectionIndex));

			bMatched &= this->Assert.IsTrue(
				ExpectedSection.Code.IsEmpty(),
				*FString::Printf(TEXT("Synchronous zero-byte preprocess should keep code section[%d] empty"), SectionIndex));

			bMatched &= this->Assert.IsTrue(
				ActualSection.Code == ExpectedSection.Code,
				*FString::Printf(TEXT("Code section[%d] processed code should match exactly between sync and async zero-byte preprocess"), SectionIndex));

			bMatched &= this->Assert.IsTrue(
				ActualSection.Code.IsEmpty(),
				*FString::Printf(TEXT("Asynchronous zero-byte preprocess should keep code section[%d] empty"), SectionIndex));
		}

		return bMatched;
	}

	bool CaptureSingleModuleSnapshot(
		const TArray<TSharedRef<FAngelscriptModuleDesc>>& Modules,
		FModuleSnapshot& OutSnapshot,
		const TCHAR* ContextLabel)
	{
		if (!this->Assert.AreEqual(
				1,
				Modules.Num(),
				*FString::Printf(TEXT("%s should produce exactly one module"), ContextLabel)))
		{
			return false;
		}

		const FAngelscriptModuleDesc& Module = Modules[0].Get();
		OutSnapshot.ModuleName = Module.ModuleName;
		OutSnapshot.ImportedModules = Module.ImportedModules;
		OutSnapshot.CodeHash = Module.CodeHash;
		OutSnapshot.CodeSections.Reserve(Module.Code.Num());
		for (const FAngelscriptModuleDesc::FCodeSection& Section : Module.Code)
		{
			FCodeSectionSnapshot SectionSnapshot;
			SectionSnapshot.RelativeFilename = Section.RelativeFilename;
			SectionSnapshot.AbsoluteFilename = Section.AbsoluteFilename;
			SectionSnapshot.Code = Section.Code;
			SectionSnapshot.CodeHash = Section.CodeHash;
			OutSnapshot.CodeSections.Add(MoveTemp(SectionSnapshot));
		}
		return true;
	}

public:
	// ========================================================================
	// AsyncMatchesSynchronousPreprocess — async-loaded multi-file preprocessing
	// produces the same modules, code, and hashes as the synchronous path
	// ========================================================================
	TEST_METHOD(AsyncMatchesSynchronousPreprocess)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		const FString SharedPadding = MakeAsyncLoadPadding(60000);
		const FString ProviderRelativePath = TEXT("Tests/Preprocessor/AsyncLoad/Provider.as");
		const FString ProviderContents =
			FString(TEXT(R"(
const int ProviderMultiplier = 3;
int ProvideValue()
{
    return 7;
}
)"))
			+ SharedPadding;

		const FString ConsumerRelativePath = TEXT("Tests/Preprocessor/AsyncLoad/Consumer.as");
		const FString ConsumerContents =
			FString(TEXT(R"(
import Tests.Preprocessor.AsyncLoad.Provider;
class AAsyncLoadMacroActor : AActor
{
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int StoredValue = ProviderMultiplier;
}
int UseProvider()
{
    return ProvideValue() * ProviderMultiplier;
}
)"))
			+ SharedPadding;

		FFixtureFile ProviderFile(ProviderRelativePath, ProviderContents);
		FFixtureFile ConsumerFile(ConsumerRelativePath, ConsumerContents);

		TGuardValue<bool> AutomaticImportGuard(Engine.bUseAutomaticImportMethod, false);

		// Lambda to preprocess with a given async flag
		auto DoPreprocess = [&](
			bool bLoadAsynchronous,
			TArray<FModuleSnapshot>& OutModules,
			TArray<FString>& OutDiagnostics,
			int32& OutErrorCount) -> bool
		{
			Engine.ResetDiagnostics();

			FAngelscriptPreprocessor Preprocessor;
			Preprocessor.AddFile(ProviderFile.RelativePath, ProviderFile.AbsolutePath, bLoadAsynchronous);
			Preprocessor.AddFile(ConsumerFile.RelativePath, ConsumerFile.AbsolutePath, bLoadAsynchronous);

			const bool bSucceeded = Preprocessor.Preprocess();
			OutModules = CaptureModuleSnapshots(Preprocessor.GetModulesToCompile());
			OutDiagnostics = CollectDiagnosticMessages(
				Engine,
				{ProviderFile.AbsolutePath, ConsumerFile.AbsolutePath},
				OutErrorCount);
			return bSucceeded;
		};

		// Synchronous path
		TArray<FModuleSnapshot> SynchronousModules;
		TArray<FString> SynchronousDiagnostics;
		int32 SynchronousErrorCount = 0;
		const bool bSynchronousSucceeded = DoPreprocess(
			false, SynchronousModules, SynchronousDiagnostics, SynchronousErrorCount);

		// Asynchronous path
		TArray<FModuleSnapshot> AsynchronousModules;
		TArray<FString> AsynchronousDiagnostics;
		int32 AsynchronousErrorCount = 0;
		const bool bAsynchronousSucceeded = DoPreprocess(
			true, AsynchronousModules, AsynchronousDiagnostics, AsynchronousErrorCount);

		// Verify both paths succeeded without diagnostics
		const bool bSyncOk = this->Assert.IsTrue(
			bSynchronousSucceeded,
			TEXT("Synchronous preprocess should succeed for the async-load comparison fixture"));
		const bool bAsyncOk = this->Assert.IsTrue(
			bAsynchronousSucceeded,
			TEXT("Asynchronous preprocess should succeed for the async-load comparison fixture"));
		const bool bSyncDiagEmpty = this->Assert.AreEqual(
			0,
			SynchronousDiagnostics.Num(),
			TEXT("Synchronous preprocess should not emit diagnostics for the async-load comparison fixture"));
		const bool bAsyncDiagEmpty = this->Assert.AreEqual(
			0,
			AsynchronousDiagnostics.Num(),
			TEXT("Asynchronous preprocess should not emit diagnostics for the async-load comparison fixture"));
		const bool bSyncErrZero = this->Assert.AreEqual(
			0,
			SynchronousErrorCount,
			TEXT("Synchronous preprocess should not emit errors for the async-load comparison fixture"));
		const bool bAsyncErrZero = this->Assert.AreEqual(
			0,
			AsynchronousErrorCount,
			TEXT("Asynchronous preprocess should not emit errors for the async-load comparison fixture"));

		if (bSyncOk && bAsyncOk && bSyncDiagEmpty && bAsyncDiagEmpty && bSyncErrZero && bAsyncErrZero)
		{
			TestModuleSnapshotsMatch(SynchronousModules, AsynchronousModules);
		}

		}
	}

	// ========================================================================
	// AsyncZeroByteFileMatchesSyncPath — zero-byte .as file produces identical
	// (empty) modules whether loaded synchronously or asynchronously
	// ========================================================================
	TEST_METHOD(AsyncZeroByteFileMatchesSyncPath)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		const FString RelativeFilename = TEXT("Tests/Preprocessor/AsyncZeroByte/ZeroByte.as");
		FFixtureFile ZeroByteFile = FFixtureFile::CreateZeroByte(RelativeFilename);

		ASSERT_THAT(AreEqual(
			0ll,
			IFileManager::Get().FileSize(*ZeroByteFile.AbsolutePath),
			TEXT("Async zero-byte preprocessor fixture should stay at file size 0")));

		TGuardValue<bool> AutomaticImportGuard(Engine.bUseAutomaticImportMethod, false);

		// Lambda to preprocess a single file with a given async flag and capture a snapshot
		auto DoPreprocessForSnapshot = [&](
			bool bLoadAsynchronous,
			FModuleSnapshot& OutSnapshot,
			TArray<FString>& OutDiagnostics,
			int32& OutErrorCount) -> bool
		{
			Engine.ResetDiagnostics();

			FAngelscriptPreprocessor Preprocessor;
			Preprocessor.AddFile(ZeroByteFile.RelativePath, ZeroByteFile.AbsolutePath, bLoadAsynchronous);
			const bool bSucceeded = Preprocessor.Preprocess();

			if (!CaptureSingleModuleSnapshot(
					Preprocessor.GetModulesToCompile(),
					OutSnapshot,
					bLoadAsynchronous
						? TEXT("Asynchronous zero-byte preprocess")
						: TEXT("Synchronous zero-byte preprocess")))
			{
				return false;
			}

			OutDiagnostics = CollectDiagnosticMessages(
				Engine,
				{ZeroByteFile.AbsolutePath},
				OutErrorCount);
			return bSucceeded;
		};

		// Synchronous path
		FModuleSnapshot SyncSnapshot;
		TArray<FString> SyncDiagnostics;
		int32 SyncErrorCount = 0;
		const bool bSyncSucceeded = DoPreprocessForSnapshot(
			false, SyncSnapshot, SyncDiagnostics, SyncErrorCount);

		// Asynchronous path
		FModuleSnapshot AsyncSnapshot;
		TArray<FString> AsyncDiagnostics;
		int32 AsyncErrorCount = 0;
		const bool bAsyncSucceeded = DoPreprocessForSnapshot(
			true, AsyncSnapshot, AsyncDiagnostics, AsyncErrorCount);

		// Verify both paths succeeded without diagnostics
		const bool bSyncOk = this->Assert.IsTrue(
			bSyncSucceeded,
			TEXT("Synchronous zero-byte preprocess should return success"));
		const bool bAsyncOk = this->Assert.IsTrue(
			bAsyncSucceeded,
			TEXT("Asynchronous zero-byte preprocess should return success"));
		const bool bSyncDiagEmpty = this->Assert.AreEqual(
			0,
			SyncDiagnostics.Num(),
			TEXT("Synchronous zero-byte preprocess should not emit diagnostics"));
		const bool bAsyncDiagEmpty = this->Assert.AreEqual(
			0,
			AsyncDiagnostics.Num(),
			TEXT("Asynchronous zero-byte preprocess should not emit diagnostics"));
		const bool bSyncErrZero = this->Assert.AreEqual(
			0,
			SyncErrorCount,
			TEXT("Synchronous zero-byte preprocess should not emit errors"));
		const bool bAsyncErrZero = this->Assert.AreEqual(
			0,
			AsyncErrorCount,
			TEXT("Asynchronous zero-byte preprocess should not emit errors"));

		if (bSyncOk && bAsyncOk && bSyncDiagEmpty && bAsyncDiagEmpty && bSyncErrZero && bAsyncErrZero)
		{
			CompareZeroByteSnapshots(SyncSnapshot, AsyncSnapshot);
		}

		}
	}

	// ========================================================================
	// TreatAsDeletedProducesEmptyModule — AddFile with bTreatAsDeleted=true
	// produces an empty module even when a real file exists on disk
	// ========================================================================
	TEST_METHOD(TreatAsDeletedProducesEmptyModule)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		Engine.ResetDiagnostics();

		const FString RelativeScriptPath = TEXT("Tests/Preprocessor/DeletedFile/TreatAsDeletedProducesEmptyModule.as");
		FFixtureFile DeletedFile(RelativeScriptPath, TEXT(R"(
import Tests.Preprocessor.DeletedFile.MissingProvider;
UCLASS()
class UDeletedFileProbe : UObject
{
    UFUNCTION()
    int Entry()
    {
        return 7;
    }
}
)"));

		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddFile(DeletedFile.RelativePath, DeletedFile.AbsolutePath, false, true);

		const bool bPreprocessSucceeded = Preprocessor.Preprocess();
		const TArray<TSharedRef<FAngelscriptModuleDesc>> Modules = Preprocessor.GetModulesToCompile();

		int32 ErrorCount = 0;
		const TArray<FString> DiagnosticMessages = CollectDiagnosticMessages(
			Engine,
			{DeletedFile.AbsolutePath},
			ErrorCount);
		const FString DiagnosticSummary = FString::Join(DiagnosticMessages, TEXT("\n"));

		ASSERT_THAT(IsTrue(
			bPreprocessSucceeded,
			TEXT("Treat-as-deleted preprocessing should succeed even when a real file exists on disk")));
		ASSERT_THAT(AreEqual(
			1,
			Modules.Num(),
			TEXT("Treat-as-deleted preprocessing should materialize exactly one module descriptor")));
		ASSERT_THAT(AreEqual(
			0,
			ErrorCount,
			TEXT("Treat-as-deleted preprocessing should not emit diagnostics")));
		ASSERT_THAT(IsTrue(
			DiagnosticSummary.IsEmpty(),
			TEXT("Treat-as-deleted preprocessing should keep the diagnostic summary empty")));

		const FAngelscriptModuleDesc* Module = nullptr;
		for (const TSharedRef<FAngelscriptModuleDesc>& M : Modules)
		{
			if (M->ModuleName == TEXT("Tests.Preprocessor.DeletedFile.TreatAsDeletedProducesEmptyModule"))
			{
				Module = &M.Get();
				break;
			}
		}

		if (!this->Assert.IsNotNull(
				Module,
				TEXT("Treat-as-deleted preprocessing should normalize the deleted file path into a module name")))
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			1,
			Module->Code.Num(),
			TEXT("Treat-as-deleted preprocessing should keep exactly one empty code section")));
		if (Module->Code.Num() > 0)
		{
			ASSERT_THAT(AreEqual(
				RelativeScriptPath,
				Module->Code[0].RelativeFilename,
				TEXT("Treat-as-deleted preprocessing should preserve the relative filename on the emitted code section")));
			ASSERT_THAT(AreEqual(
				DeletedFile.AbsolutePath,
				Module->Code[0].AbsoluteFilename,
				TEXT("Treat-as-deleted preprocessing should preserve the absolute filename on the emitted code section")));
			ASSERT_THAT(IsTrue(
				Module->Code[0].Code.IsEmpty(),
				TEXT("Treat-as-deleted preprocessing should emit an empty processed code section")));
			ASSERT_THAT(AreEqual(
				static_cast<int64>(0),
				Module->Code[0].CodeHash,
				TEXT("Treat-as-deleted preprocessing should zero the empty code section hash")));
		}

		ASSERT_THAT(AreEqual(
			static_cast<int64>(0),
			Module->CodeHash,
			TEXT("Treat-as-deleted preprocessing should keep the module code hash at zero")));
		ASSERT_THAT(AreEqual(
			0,
			Module->ImportedModules.Num(),
			TEXT("Treat-as-deleted preprocessing should not record imported modules from deleted source")));
		ASSERT_THAT(AreEqual(
			0,
			Module->PostInitFunctions.Num(),
			TEXT("Treat-as-deleted preprocessing should not record post-init functions from deleted source")));
		ASSERT_THAT(AreEqual(
			0,
			Module->Classes.Num(),
			TEXT("Treat-as-deleted preprocessing should not materialize classes from deleted source")));
		ASSERT_THAT(AreEqual(
			0,
			Module->Enums.Num(),
			TEXT("Treat-as-deleted preprocessing should not materialize enums from deleted source")));
		ASSERT_THAT(AreEqual(
			0,
			Module->Delegates.Num(),
			TEXT("Treat-as-deleted preprocessing should not materialize delegates from deleted source")));
#if WITH_EDITOR
		ASSERT_THAT(AreEqual(
			0,
			Module->UsageRestrictions.Num(),
			TEXT("Treat-as-deleted preprocessing should not materialize usage restrictions from deleted source")));
		ASSERT_THAT(AreEqual(
			0,
			Module->EditorOnlyBlockLines.Num(),
			TEXT("Treat-as-deleted preprocessing should not record editor-only block lines from deleted source")));
#endif

		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
