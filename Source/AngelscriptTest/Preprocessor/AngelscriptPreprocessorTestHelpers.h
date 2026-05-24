#pragma once

// ============================================================================
// AngelscriptPreprocessorTestHelpers.h
//
// Shared test utilities for Preprocessor tests. Provides:
//   - FFixtureFile: RAII fixture .as file writer (write on construct, delete on destruct)
//   - FPreprocessResult: captures preprocessing output + diagnostics in one bundle
//   - RunPreprocess(): one-call preprocess runner
//   - Assertion helpers: success/failure, diagnostics, module inspection
//   - FPreprocessSession: low-level access to Preprocessor internals (FFile/FMacro/FChunk)
//
// Usage:
//   TEST_METHOD(SomeName)
//   {
//       FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
//       { FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);
//
//       FFixtureFile File(TEXT("Tests/PP/Feature/Test.as"), TEXT("int Entry() { return 7; }"));
//       auto Result = RunPreprocess(Engine, File);
//       AssertPreprocessSucceeded(*TestRunner, Result);
//       auto* Module = AssertModuleExists(*TestRunner, Result, TEXT("Tests.PP.Feature.Test"));
//       if (Module) AssertModuleCodeContains(*TestRunner, *Module, TEXT("return 7;"));
//
//       }
//   }
// ============================================================================

#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestEngineHelper.h"
#include "Preprocessor/AngelscriptPreprocessor.h"

#include "HAL/FileManager.h"
#include "Logging/LogMacros.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

// Dedicated log category for on-demand inspection of preprocessor outputs from
// any *Tests.cpp under Preprocessor/. Default verbosity is NoLogging so the
// payload (which can be the full processed source of every module) is never
// formatted in normal runs. To enable, either:
//   * pass -LogCmds="LogPreprocessorDump Verbose" on the editor command line,
//   * or call LogPreprocessorDump.SetVerbosity(ELogVerbosity::Verbose) inside
//     the specific TEST_METHOD that needs the dump.
// Compile-time verbosity is Verbose so all sites stay live in the binary.
DECLARE_LOG_CATEGORY_EXTERN(LogPreprocessorDump, NoLogging, Verbose);

namespace PreprocessorTestHelpers
{

	// =========================================================================
	// 1. Fixture File Management (RAII)
	// =========================================================================

	/** Unified root directory for all preprocessor test fixtures.
	 *  Replaces 26 per-file GetFixtureRoot / Get*FixtureRoot functions. */
	inline FString GetFixtureRoot()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("PreprocessorTestFixtures"));
	}

	/** RAII fixture file: writes a .as file on construction, deletes on destruction.
	 *  Replaces 25 per-file WriteFixture / Write*Fixture functions.
	 *
	 *  Content normalization: the constructor strips at most ONE leading newline
	 *  (\r?\n) from Content before writing to disk. Trailing newlines are
	 *  preserved verbatim. This makes TEXT(R"(...)") raw-string literals
	 *  practical without changing the byte layout that legacy
	 *      TEXT("...\n") TEXT("...\n")
	 *  fixtures wrote (their trailing \n is significant: many tests embed code
	 *  whose last line ends with a newline, and that final \n must be kept).
	 *  A typical raw-string fixture therefore looks like:
	 *
	 *      FFixtureFile File(TEXT("Tests/PP/Foo.as"), TEXT(R"(
	 *      int Entry() { return 7; }
	 *      )"));
	 *
	 *  which writes "int Entry() { return 7; }\n" to disk — byte-equivalent to
	 *      TEXT("int Entry() { return 7; }\n").
	 *
	 *  Trim is intentionally limited to one outer leading newline so callers can
	 *  still embed deliberate leading blank lines by adding extra newlines. */
	struct FFixtureFile
	{
		FString RelativePath;
		FString AbsolutePath;

		FFixtureFile(const FString& InRelativePath, const FString& Content)
			: RelativePath(InRelativePath)
			, AbsolutePath(FPaths::Combine(GetFixtureRoot(), InRelativePath))
			, bOwnsFile(true)
		{
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(AbsolutePath), true);
			FFileHelper::SaveStringToFile(TrimLeadingNewline(Content), *AbsolutePath);
		}

		~FFixtureFile()
		{
			if (bOwnsFile && !AbsolutePath.IsEmpty())
			{
				IFileManager::Get().Delete(*AbsolutePath, false, true, true);
			}
		}

		// Non-copyable
		FFixtureFile(const FFixtureFile&) = delete;
		FFixtureFile& operator=(const FFixtureFile&) = delete;

		// Movable
		FFixtureFile(FFixtureFile&& Other) noexcept
			: RelativePath(MoveTemp(Other.RelativePath))
			, AbsolutePath(MoveTemp(Other.AbsolutePath))
			, bOwnsFile(Other.bOwnsFile)
		{
			Other.bOwnsFile = false;
		}

		FFixtureFile& operator=(FFixtureFile&& Other) noexcept
		{
			if (this != &Other)
			{
				if (bOwnsFile && !AbsolutePath.IsEmpty())
				{
					IFileManager::Get().Delete(*AbsolutePath, false, true, true);
				}
				RelativePath = MoveTemp(Other.RelativePath);
				AbsolutePath = MoveTemp(Other.AbsolutePath);
				bOwnsFile = Other.bOwnsFile;
				Other.bOwnsFile = false;
			}
			return *this;
		}

		/** Derive the dot-separated module name the preprocessor will emit for this
		 *  fixture, based purely on RelativePath. Mirrors the runtime convention:
		 *      "Tests/Preprocessor/Foo.as" -> "Tests.Preprocessor.Foo"
		 *  Strips a trailing ".as" if present, normalizes both '\\' and '/' to '.'. */
		FString ExpectedModuleName() const
		{
			FString Name = RelativePath;
			if (Name.EndsWith(TEXT(".as"), ESearchCase::IgnoreCase))
			{
				Name.LeftChopInline(3, EAllowShrinking::No);
			}
			Name.ReplaceInline(TEXT("\\"), TEXT("."));
			Name.ReplaceInline(TEXT("/"), TEXT("."));
			return Name;
		}

		/** Factory for zero-byte fixture files (true 0 bytes on disk). */
		static FFixtureFile CreateZeroByte(const FString& InRelativePath)
		{
			FFixtureFile Result;
			Result.RelativePath = InRelativePath;
			Result.AbsolutePath = FPaths::Combine(GetFixtureRoot(), InRelativePath);
			Result.bOwnsFile = true;

			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Result.AbsolutePath), true);
			FArchive* Writer = IFileManager::Get().CreateFileWriter(*Result.AbsolutePath);
			if (Writer != nullptr)
			{
				Writer->Close();
				delete Writer;
			}
			return Result;
		}

	private:
		FFixtureFile() = default;
		bool bOwnsFile = false;

		/** Strip at most ONE leading newline (\r?\n). Trailing whitespace is
		 *  preserved verbatim — many AngelScript fixtures rely on a final '\n'. */
		static FString TrimLeadingNewline(const FString& In)
		{
			int32 Start = 0;
			const int32 Len = In.Len();
			if (Start < Len && In[Start] == TCHAR('\r'))
			{
				++Start;
			}
			if (Start < Len && In[Start] == TCHAR('\n'))
			{
				++Start;
			}
			else
			{
				// Only strip a leading "\n" or "\r\n" pair; a lone leading '\r'
				// must not be consumed.
				Start = 0;
			}

			if (Start == 0)
			{
				return In;
			}
			return In.Mid(Start, Len - Start);
		}
	};

	/** Batch-write multiple fixture files. */
	inline TArray<FFixtureFile> WriteFixtures(
		TArrayView<const TPair<FString, FString>> PathsAndContents)
	{
		TArray<FFixtureFile> Files;
		Files.Reserve(PathsAndContents.Num());
		for (const TPair<FString, FString>& Pair : PathsAndContents)
		{
			Files.Emplace(Pair.Key, Pair.Value);
		}
		return Files;
	}

	// =========================================================================
	// 2. Preprocessing Result Capture
	// =========================================================================

	/** Encapsulates the complete output of a preprocessing run:
	 *  success flag, module descriptors, and all diagnostics collected from the engine. */
	struct FPreprocessResult
	{
		bool bSuccess = false;
		TArray<TSharedRef<FAngelscriptModuleDesc>> Modules;

		/** All diagnostic entries collected from the engine after preprocessing.
		 *  Includes both errors and warnings. */
		TArray<FAngelscriptEngine::FDiagnostic> AllDiagnostics;

		/** Count of error diagnostics only. */
		int32 ErrorCount = 0;

		/** Find a module by its fully-qualified dot-separated name. Returns nullptr if not found.
		 *  Returns non-const pointer because callers commonly invoke non-const FAngelscriptModuleDesc::GetClass.
		 *  Replaces 13 per-file FindModuleByName functions. */
		FAngelscriptModuleDesc* FindModule(const FString& ModuleName) const
		{
			for (const TSharedRef<FAngelscriptModuleDesc>& Module : Modules)
			{
				if (Module->ModuleName == ModuleName)
				{
					return &Module.Get();
				}
			}
			return nullptr;
		}

		/** Join all code sections of a module into one string. */
		FString JoinedCode(const FAngelscriptModuleDesc& Module) const
		{
			FString Joined;
			for (const FAngelscriptModuleDesc::FCodeSection& Section : Module.Code)
			{
				if (!Joined.IsEmpty())
				{
					Joined += TEXT("\n");
				}
				Joined += Section.Code;
			}
			return Joined;
		}

		/** Get the ordered list of module names for topology checks. */
		FString ModuleOrder() const
		{
			return FString::JoinBy(
				Modules,
				TEXT(" -> "),
				[](const TSharedRef<FAngelscriptModuleDesc>& M) { return M->ModuleName; });
		}
	};

	/** Append engine diagnostics for the given absolute filenames into the result.
	 *  Shared internal helper used by both RunPreprocess and RunPreprocessSession. */
	inline void AppendEngineDiagnostics(
		FAngelscriptEngine& Engine,
		const TArray<FString>& AbsoluteFilenames,
		FPreprocessResult& InOutResult)
	{
		for (const FString& AbsoluteFilename : AbsoluteFilenames)
		{
			const FAngelscriptEngine::FDiagnostics* DiagSet = Engine.Diagnostics.Find(AbsoluteFilename);
			if (DiagSet == nullptr)
			{
				continue;
			}
			for (const FAngelscriptEngine::FDiagnostic& Diag : DiagSet->Diagnostics)
			{
				InOutResult.AllDiagnostics.Add(Diag);
				if (Diag.bIsError)
				{
					++InOutResult.ErrorCount;
				}
			}
		}
	}

	inline void AddFilesToPreprocessor(
		FAngelscriptPreprocessor& Preprocessor,
		TArrayView<const FFixtureFile* const> Files,
		TArray<FString>& OutAbsoluteFilenames)
	{
		OutAbsoluteFilenames.Reserve(Files.Num());
		for (const FFixtureFile* File : Files)
		{
			Preprocessor.AddFile(File->RelativePath, File->AbsolutePath);
			OutAbsoluteFilenames.Add(File->AbsolutePath);
		}
	}

	/** Run preprocessing on multiple fixture files with optional flag overrides.
	 *  Handles engine diagnostic reset, AddFile, Preprocess, and diagnostic collection. */
	inline FPreprocessResult RunPreprocess(
		FAngelscriptEngine& Engine,
		TArrayView<const FFixtureFile* const> Files,
		const FAngelscriptPreprocessorContext& Context)
	{
		Engine.ResetDiagnostics();

		FAngelscriptPreprocessor Preprocessor(Context);

		TArray<FString> AbsoluteFilenames;
		AddFilesToPreprocessor(Preprocessor, Files, AbsoluteFilenames);

		FPreprocessResult Result;
		Result.bSuccess = Preprocessor.Preprocess();
		Result.Modules = Preprocessor.GetModulesToCompile();
		AppendEngineDiagnostics(Engine, AbsoluteFilenames, Result);
		return Result;
	}

	/** Run preprocessing on multiple fixture files with optional flag overrides.
	 *  Handles engine diagnostic reset, AddFile, Preprocess, and diagnostic collection. */
	inline FPreprocessResult RunPreprocess(
		FAngelscriptEngine& Engine,
		TArrayView<const FFixtureFile* const> Files,
		const TMap<FString, bool>& FlagOverrides = {},
		bool bDisableAutomaticImports = true)
	{
		Engine.ResetDiagnostics();

		// Guard automatic imports for the full duration of preprocessing
		TOptional<TGuardValue<bool>> AutomaticImportGuard;
		if (bDisableAutomaticImports)
		{
			AutomaticImportGuard.Emplace(Engine.bUseAutomaticImportMethod, false);
		}

		FAngelscriptPreprocessor Preprocessor;

		// Apply flag overrides
		for (const TPair<FString, bool>& Flag : FlagOverrides)
		{
			Preprocessor.PreprocessorFlags.Add(Flag.Key, Flag.Value);
		}

		TArray<FString> AbsoluteFilenames;
		AddFilesToPreprocessor(Preprocessor, Files, AbsoluteFilenames);

		FPreprocessResult Result;
		Result.bSuccess = Preprocessor.Preprocess();
		Result.Modules = Preprocessor.GetModulesToCompile();
		AppendEngineDiagnostics(Engine, AbsoluteFilenames, Result);
		return Result;
	}

	/** Single-file convenience overload. */
	inline FPreprocessResult RunPreprocess(
		FAngelscriptEngine& Engine,
		const FFixtureFile& File,
		const FAngelscriptPreprocessorContext& Context)
	{
		const FFixtureFile* FilePtr = &File;
		return RunPreprocess(Engine, MakeArrayView(&FilePtr, 1), Context);
	}

	/** Single-file convenience overload. */
	inline FPreprocessResult RunPreprocess(
		FAngelscriptEngine& Engine,
		const FFixtureFile& File,
		const TMap<FString, bool>& FlagOverrides = {},
		bool bDisableAutomaticImports = true)
	{
		const FFixtureFile* FilePtr = &File;
		return RunPreprocess(Engine, MakeArrayView(&FilePtr, 1), FlagOverrides, bDisableAutomaticImports);
	}

	/** Multi-file convenience overload using TArray<FFixtureFile>. */
	inline FPreprocessResult RunPreprocess(
		FAngelscriptEngine& Engine,
		const TArray<FFixtureFile>& Files,
		const FAngelscriptPreprocessorContext& Context)
	{
		TArray<const FFixtureFile*> Ptrs;
		Ptrs.Reserve(Files.Num());
		for (const FFixtureFile& F : Files)
		{
			Ptrs.Add(&F);
		}
		return RunPreprocess(Engine, MakeArrayView(Ptrs), Context);
	}

	/** Multi-file convenience overload using TArray<FFixtureFile>. */
	inline FPreprocessResult RunPreprocess(
		FAngelscriptEngine& Engine,
		const TArray<FFixtureFile>& Files,
		const TMap<FString, bool>& FlagOverrides = {},
		bool bDisableAutomaticImports = true)
	{
		TArray<const FFixtureFile*> Ptrs;
		Ptrs.Reserve(Files.Num());
		for (const FFixtureFile& F : Files)
		{
			Ptrs.Add(&F);
		}
		return RunPreprocess(Engine, MakeArrayView(Ptrs), FlagOverrides, bDisableAutomaticImports);
	}

	// =========================================================================
	// 3. Assertion Helpers
	// =========================================================================

	// --- Success / Failure ---

	inline bool AssertPreprocessSucceeded(
		FAutomationTestBase& Test,
		const FPreprocessResult& Result)
	{
		return Test.TestTrue(TEXT("Preprocessing should succeed"), Result.bSuccess);
	}

	inline bool AssertPreprocessFailed(
		FAutomationTestBase& Test,
		const FPreprocessResult& Result)
	{
		return Test.TestFalse(TEXT("Preprocessing should fail"), Result.bSuccess);
	}

	// --- Diagnostics ---

	/** Assert the exact number of error diagnostics. */
	inline bool AssertErrorCount(
		FAutomationTestBase& Test,
		const FPreprocessResult& Result,
		int32 Expected)
	{
		return Test.TestEqual(
			TEXT("Error diagnostic count should match expected"),
			Result.ErrorCount,
			Expected);
	}

	/** Assert at least one diagnostic message contains the given substring. */
	inline bool AssertDiagnosticContains(
		FAutomationTestBase& Test,
		const FPreprocessResult& Result,
		const FString& Substring)
	{
		const bool bFound = Result.AllDiagnostics.ContainsByPredicate(
			[&Substring](const FAngelscriptEngine::FDiagnostic& Diag)
			{
				return Diag.Message.Contains(Substring);
			});
		return Test.TestTrue(
			*FString::Printf(TEXT("Diagnostics should contain message matching '%s'"), *Substring),
			bFound);
	}

	/** Assert NO diagnostic message contains the given substring. */
	inline bool AssertDiagnosticNotContains(
		FAutomationTestBase& Test,
		const FPreprocessResult& Result,
		const FString& Substring)
	{
		const bool bFound = Result.AllDiagnostics.ContainsByPredicate(
			[&Substring](const FAngelscriptEngine::FDiagnostic& Diag)
			{
				return Diag.Message.Contains(Substring);
			});
		return Test.TestFalse(
			*FString::Printf(TEXT("Diagnostics should NOT contain message matching '%s'"), *Substring),
			bFound);
	}

	/** Assert a diagnostic exists with the given message substring at the expected row/column. */
	inline bool AssertDiagnosticAt(
		FAutomationTestBase& Test,
		const FPreprocessResult& Result,
		const FString& MessageSubstring,
		int32 ExpectedRow,
		int32 ExpectedColumn = 1)
	{
		const FAngelscriptEngine::FDiagnostic* Found = nullptr;
		for (const FAngelscriptEngine::FDiagnostic& Diag : Result.AllDiagnostics)
		{
			if (Diag.Message.Contains(MessageSubstring))
			{
				Found = &Diag;
				break;
			}
		}

		if (!Test.TestNotNull(
				*FString::Printf(TEXT("Should find diagnostic containing '%s'"), *MessageSubstring),
				Found))
		{
			return false;
		}

		bool bPassed = true;
		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("Diagnostic '%s' should be at row %d"), *MessageSubstring, ExpectedRow),
			Found->Row, ExpectedRow);
		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("Diagnostic '%s' should be at column %d"), *MessageSubstring, ExpectedColumn),
			Found->Column, ExpectedColumn);
		return bPassed;
	}

	/** Assert diagnostics are completely empty (no errors, no warnings). */
	inline bool AssertNoDiagnostics(
		FAutomationTestBase& Test,
		const FPreprocessResult& Result)
	{
		return Test.TestEqual(
			TEXT("Should emit no diagnostics"),
			Result.AllDiagnostics.Num(), 0);
	}

	// --- Module assertions ---

	/** Assert a module with the given name exists. Returns pointer (nullptr on failure).
	 *  Returns non-const pointer because callers commonly invoke non-const FAngelscriptModuleDesc::GetClass. */
	inline FAngelscriptModuleDesc* AssertModuleExists(
		FAutomationTestBase& Test,
		const FPreprocessResult& Result,
		const FString& ModuleName)
	{
		FAngelscriptModuleDesc* Module = Result.FindModule(ModuleName);
		Test.TestNotNull(
			*FString::Printf(TEXT("Module '%s' should exist"), *ModuleName),
			Module);
		return Module;
	}

	/** Assert module count matches expected. */
	inline bool AssertModuleCount(
		FAutomationTestBase& Test,
		const FPreprocessResult& Result,
		int32 Expected)
	{
		return Test.TestEqual(
			TEXT("Module count should match expected"),
			Result.Modules.Num(), Expected);
	}

	/** Assert a module's concatenated code contains the given substring. */
	inline bool AssertModuleCodeContains(
		FAutomationTestBase& Test,
		const FPreprocessResult& Result,
		const FAngelscriptModuleDesc& Module,
		const FString& Substring)
	{
		const FString Code = Result.JoinedCode(Module);
		return Test.TestTrue(
			*FString::Printf(TEXT("Module '%s' code should contain '%s'"), *Module.ModuleName, *Substring),
			Code.Contains(Substring));
	}

	/** Assert a module's concatenated code does NOT contain the given substring. */
	inline bool AssertModuleCodeNotContains(
		FAutomationTestBase& Test,
		const FPreprocessResult& Result,
		const FAngelscriptModuleDesc& Module,
		const FString& Substring)
	{
		const FString Code = Result.JoinedCode(Module);
		return Test.TestFalse(
			*FString::Printf(TEXT("Module '%s' code should NOT contain '%s'"), *Module.ModuleName, *Substring),
			Code.Contains(Substring));
	}

	/** Assert a module imports another module by name. */
	inline bool AssertModuleImports(
		FAutomationTestBase& Test,
		const FAngelscriptModuleDesc& Module,
		const FString& ImportedModuleName)
	{
		return Test.TestTrue(
			*FString::Printf(TEXT("Module '%s' should import '%s'"), *Module.ModuleName, *ImportedModuleName),
			Module.ImportedModules.Contains(ImportedModuleName));
	}

	/** Assert import count on a module. */
	inline bool AssertImportCount(
		FAutomationTestBase& Test,
		const FAngelscriptModuleDesc& Module,
		int32 Expected)
	{
		return Test.TestEqual(
			*FString::Printf(TEXT("Module '%s' import count"), *Module.ModuleName),
			Module.ImportedModules.Num(), Expected);
	}

	/** Assert a module declares a class descriptor with the given name.
	 *  Module is passed by non-const ref because FAngelscriptModuleDesc::GetClass is non-const. */
	inline bool AssertModuleDeclaresClass(
		FAutomationTestBase& Test,
		FAngelscriptModuleDesc& Module,
		const FString& ClassName)
	{
		return Test.TestTrue(
			*FString::Printf(TEXT("Module '%s' should declare class '%s'"), *Module.ModuleName, *ClassName),
			Module.GetClass(ClassName).IsValid());
	}

	/** Assert a module does NOT declare a class descriptor with the given name.
	 *  Module is passed by non-const ref because FAngelscriptModuleDesc::GetClass is non-const. */
	inline bool AssertModuleNotDeclaresClass(
		FAutomationTestBase& Test,
		FAngelscriptModuleDesc& Module,
		const FString& ClassName)
	{
		return Test.TestFalse(
			*FString::Printf(TEXT("Module '%s' should NOT declare class '%s'"), *Module.ModuleName, *ClassName),
			Module.GetClass(ClassName).IsValid());
	}

	// --- Compilable code check ---

	/** Check whether any module in the result contains non-empty code sections.
	 *  Replaces 6 per-file ContainsCompilableCode functions. */
	inline bool ContainsCompilableCode(const FPreprocessResult& Result)
	{
		for (const TSharedRef<FAngelscriptModuleDesc>& Module : Result.Modules)
		{
			for (const FAngelscriptModuleDesc::FCodeSection& Section : Module->Code)
			{
				if (!Section.Code.IsEmpty())
				{
					return true;
				}
			}
		}
		return false;
	}

	inline bool AssertNoCompilableCode(
		FAutomationTestBase& Test,
		const FPreprocessResult& Result)
	{
		return Test.TestFalse(
			TEXT("Preprocessing should not leave behind compilable code"),
			ContainsCompilableCode(Result));
	}

	inline bool AssertContainsCompilableCode(
		FAutomationTestBase& Test,
		const FPreprocessResult& Result)
	{
		return Test.TestTrue(
			TEXT("Preprocessing should produce compilable code"),
			ContainsCompilableCode(Result));
	}

	// =========================================================================
	// 4. Low-level Session (for tests inspecting FFile/FMacro/FChunk)
	// =========================================================================

	/** Keeps the preprocessor alive after RunPreprocess for internal structure inspection.
	 *  Only needed by a handful of tests that verify macro shapes, chunk types, etc. */
	struct FPreprocessSession
	{
		FPreprocessSession() = default;

		explicit FPreprocessSession(const FAngelscriptPreprocessorContext& Context)
			: Preprocessor(Context)
		{
		}

		FAngelscriptPreprocessor Preprocessor;
		FPreprocessResult Result;

		const TArray<FAngelscriptPreprocessor::FFile>& GetFiles() const
		{
			return Preprocessor.Files;
		}

		/** Gather all macros from all files/chunks. */
		TArray<const FAngelscriptPreprocessor::FMacro*> GatherMacros() const
		{
			TArray<const FAngelscriptPreprocessor::FMacro*> Macros;
			for (const FAngelscriptPreprocessor::FFile& File : Preprocessor.Files)
			{
				for (const FAngelscriptPreprocessor::FChunk& Chunk : File.ChunkedCode)
				{
					for (const FAngelscriptPreprocessor::FMacro& Macro : Chunk.Macros)
					{
						Macros.Add(&Macro);
					}
				}
			}
			return Macros;
		}

		/** Find a macro by type and name. */
		const FAngelscriptPreprocessor::FMacro* FindMacro(
			FAngelscriptPreprocessor::EMacroType Type,
			const FString& Name) const
		{
			for (const FAngelscriptPreprocessor::FFile& File : Preprocessor.Files)
			{
				for (const FAngelscriptPreprocessor::FChunk& Chunk : File.ChunkedCode)
				{
					for (const FAngelscriptPreprocessor::FMacro& Macro : Chunk.Macros)
					{
						if (Macro.Type == Type && Macro.Name == Name)
						{
							return &Macro;
						}
					}
				}
			}
			return nullptr;
		}

		/** Find a macro by type and subject index. */
		const FAngelscriptPreprocessor::FMacro* FindMacroBySubjectIndex(
			FAngelscriptPreprocessor::EMacroType Type,
			int32 SubjectIndex) const
		{
			for (const FAngelscriptPreprocessor::FFile& File : Preprocessor.Files)
			{
				for (const FAngelscriptPreprocessor::FChunk& Chunk : File.ChunkedCode)
				{
					for (const FAngelscriptPreprocessor::FMacro& Macro : Chunk.Macros)
					{
						if (Macro.Type == Type && Macro.SubjectIndex == SubjectIndex)
						{
							return &Macro;
						}
					}
				}
			}
			return nullptr;
		}

		/** Find the first chunk of a given type. */
		const FAngelscriptPreprocessor::FChunk* FindFirstChunkOfType(
			FAngelscriptPreprocessor::EChunkType ChunkType) const
		{
			for (const FAngelscriptPreprocessor::FFile& File : Preprocessor.Files)
			{
				for (const FAngelscriptPreprocessor::FChunk& Chunk : File.ChunkedCode)
				{
					if (Chunk.Type == ChunkType)
					{
						return &Chunk;
					}
				}
			}
			return nullptr;
		}
	};

	/** Run preprocessing and keep the preprocessor alive for internal inspection. */
	inline FPreprocessSession RunPreprocessSession(
		FAngelscriptEngine& Engine,
		TArrayView<const FFixtureFile* const> Files,
		const FAngelscriptPreprocessorContext& Context)
	{
		Engine.ResetDiagnostics();

		FPreprocessSession Session(Context);

		TArray<FString> AbsoluteFilenames;
		AddFilesToPreprocessor(Session.Preprocessor, Files, AbsoluteFilenames);

		Session.Result.bSuccess = Session.Preprocessor.Preprocess();
		Session.Result.Modules = Session.Preprocessor.GetModulesToCompile();
		AppendEngineDiagnostics(Engine, AbsoluteFilenames, Session.Result);
		return Session;
	}

	/** Run preprocessing and keep the preprocessor alive for internal inspection. */
	inline FPreprocessSession RunPreprocessSession(
		FAngelscriptEngine& Engine,
		TArrayView<const FFixtureFile* const> Files,
		const TMap<FString, bool>& FlagOverrides = {},
		bool bDisableAutomaticImports = true)
	{
		Engine.ResetDiagnostics();

		FPreprocessSession Session;

		for (const TPair<FString, bool>& Flag : FlagOverrides)
		{
			Session.Preprocessor.PreprocessorFlags.Add(Flag.Key, Flag.Value);
		}

		// Guard automatic imports for the full duration of preprocessing
		TOptional<TGuardValue<bool>> AutomaticImportGuard;
		if (bDisableAutomaticImports)
		{
			AutomaticImportGuard.Emplace(Engine.bUseAutomaticImportMethod, false);
		}

		TArray<FString> AbsoluteFilenames;
		AddFilesToPreprocessor(Session.Preprocessor, Files, AbsoluteFilenames);

		Session.Result.bSuccess = Session.Preprocessor.Preprocess();
		Session.Result.Modules = Session.Preprocessor.GetModulesToCompile();
		AppendEngineDiagnostics(Engine, AbsoluteFilenames, Session.Result);
		return Session;
	}

	/** Single-file convenience overload. */
	inline FPreprocessSession RunPreprocessSession(
		FAngelscriptEngine& Engine,
		const FFixtureFile& File,
		const FAngelscriptPreprocessorContext& Context)
	{
		const FFixtureFile* FilePtr = &File;
		return RunPreprocessSession(Engine, MakeArrayView(&FilePtr, 1), Context);
	}

	/** Multi-file convenience overload using TArray<FFixtureFile>. */
	inline FPreprocessSession RunPreprocessSession(
		FAngelscriptEngine& Engine,
		const TArray<FFixtureFile>& Files,
		const FAngelscriptPreprocessorContext& Context)
	{
		TArray<const FFixtureFile*> Ptrs;
		Ptrs.Reserve(Files.Num());
		for (const FFixtureFile& F : Files)
		{
			Ptrs.Add(&F);
		}
		return RunPreprocessSession(Engine, MakeArrayView(Ptrs), Context);
	}

	/** Single-file convenience overload. */
	inline FPreprocessSession RunPreprocessSession(
		FAngelscriptEngine& Engine,
		const FFixtureFile& File,
		const TMap<FString, bool>& FlagOverrides = {},
		bool bDisableAutomaticImports = true)
	{
		const FFixtureFile* FilePtr = &File;
		return RunPreprocessSession(Engine, MakeArrayView(&FilePtr, 1), FlagOverrides, bDisableAutomaticImports);
	}

	// =========================================================================
	// 6. Debug: Dump Preprocessing Result To Disk / Log
	// =========================================================================

	namespace Detail
	{
		/** Replace characters illegal on common file systems with '_'. Does NOT
		 *  collapse path separators on purpose: the caller controls the layout. */
		inline FString SanitizeFilenameComponent(const FString& In)
		{
			FString Out = In;
			static const TCHAR* Illegal = TEXT("\\/:*?\"<>|");
			for (int32 i = 0; Illegal[i] != 0; ++i)
			{
				const TCHAR Bad = Illegal[i];
				const TCHAR Replacement = TCHAR('_');
				for (int32 j = 0; j < Out.Len(); ++j)
				{
					if (Out[j] == Bad)
					{
						Out[j] = Replacement;
					}
				}
			}
			return Out;
		}
	}

	/** Root directory for ad-hoc preprocessor result dumps. */
	inline FString GetDumpRoot()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("PreprocessorDumps"));
	}

	/** Dump a preprocessing result to disk for human inspection.
	 *
	 *  Layout (under Saved/Automation/PreprocessorDumps/<Subdir>/):
	 *    Index.txt                         -- one line per module + summary
	 *    <ModuleName>/_meta.txt            -- per-module metadata
	 *    <ModuleName>/<i>_<file>.as        -- each FCodeSection's processed code
	 *
	 *  This helper is intentionally NOT invoked by any TEST_METHOD; it is for
	 *  on-demand developer use. All filesystem failures are silent so the helper
	 *  never alters test outcomes. */
	inline void DumpPreprocessResult(const FPreprocessResult& Result, const FString& Subdir)
	{
		const FString DumpDir = FPaths::Combine(GetDumpRoot(), Subdir);
		IFileManager& FM = IFileManager::Get();
		FM.MakeDirectory(*DumpDir, true);

		FString Index;
		Index += FString::Printf(TEXT("# PreprocessResult dump\n"));
		Index += FString::Printf(TEXT("Subdir         : %s\n"), *Subdir);
		Index += FString::Printf(TEXT("bSuccess       : %s\n"), Result.bSuccess ? TEXT("true") : TEXT("false"));
		Index += FString::Printf(TEXT("ModuleCount    : %d\n"), Result.Modules.Num());
		Index += FString::Printf(TEXT("ErrorCount     : %d\n"), Result.ErrorCount);
		Index += FString::Printf(TEXT("DiagnosticCount: %d\n\n"), Result.AllDiagnostics.Num());

		for (const TSharedRef<FAngelscriptModuleDesc>& ModuleRef : Result.Modules)
		{
			const FAngelscriptModuleDesc& Module = ModuleRef.Get();
			const FString SafeModuleName = Detail::SanitizeFilenameComponent(Module.ModuleName);
			const FString ModuleDir = FPaths::Combine(DumpDir, SafeModuleName);
			FM.MakeDirectory(*ModuleDir, true);

			Index += FString::Printf(
				TEXT("- %s  sections=%d  hash=%lld  combined=%lld\n"),
				*Module.ModuleName,
				Module.Code.Num(),
				static_cast<long long>(Module.CodeHash),
				static_cast<long long>(Module.CombinedDependencyHash));

			FString Meta;
			Meta += FString::Printf(TEXT("ModuleName             : %s\n"), *Module.ModuleName);
			Meta += FString::Printf(TEXT("CodeHash               : %lld\n"), static_cast<long long>(Module.CodeHash));
			Meta += FString::Printf(TEXT("CombinedDependencyHash : %lld\n"), static_cast<long long>(Module.CombinedDependencyHash));
			Meta += FString::Printf(TEXT("CodeSectionCount       : %d\n\n"), Module.Code.Num());
			for (int32 i = 0; i < Module.Code.Num(); ++i)
			{
				const FAngelscriptModuleDesc::FCodeSection& Section = Module.Code[i];
				Meta += FString::Printf(
					TEXT("[%d] relative=%s\n    absolute=%s\n    hash=%lld  bytes=%d\n"),
					i, *Section.RelativeFilename, *Section.AbsoluteFilename,
					static_cast<long long>(Section.CodeHash), Section.Code.Len());

				const FString SectionLeaf = Detail::SanitizeFilenameComponent(
					FPaths::GetCleanFilename(Section.RelativeFilename));
				const FString SectionFile = FPaths::Combine(
					ModuleDir, FString::Printf(TEXT("%d_%s"), i, *SectionLeaf));
				FFileHelper::SaveStringToFile(Section.Code, *SectionFile);
			}
			FFileHelper::SaveStringToFile(Meta, *FPaths::Combine(ModuleDir, TEXT("_meta.txt")));
		}

		if (Result.AllDiagnostics.Num() > 0)
		{
			Index += TEXT("\n# Diagnostics\n");
			for (const FAngelscriptEngine::FDiagnostic& D : Result.AllDiagnostics)
			{
				Index += FString::Printf(
					TEXT("[%s] (%d:%d) %s\n"),
					D.bIsError ? TEXT("error") : (D.bIsInfo ? TEXT("info") : TEXT("warn")),
					D.Row, D.Column, *D.Message);
			}
		}

		FFileHelper::SaveStringToFile(Index, *FPaths::Combine(DumpDir, TEXT("Index.txt")));
	}

	/** Log every module's processed code via LogPreprocessorDump.
	 *
	 *  By default LogPreprocessorDump is NoLogging so this call is effectively
	 *  free at runtime — both the formatting of the per-module string and the
	 *  enclosing UE_LOG body are skipped via UE_LOG_ACTIVE_ON_CHANNEL().
	 *
	 *  To see the output, enable the category in one of these ways:
	 *    1) Editor command line:  -LogCmds="LogPreprocessorDump Verbose"
	 *    2) From a TEST_METHOD:   LogPreprocessorDump.SetVerbosity(ELogVerbosity::Verbose);
	 *
	 *  Output format (one block per module):
	 *      [Preprocessor:<ContextLabel>] module=<Name> imports=[A, B, ...]
	 *      ----- BEGIN -----
	 *      <processed code>
	 *      ----- END -----
	 *
	 *  ContextLabel is intended to be the calling test name so multiple
	 *  invocations in the same run can be told apart in the log. */
	inline void LogProcessedCode(const FPreprocessResult& Result, const TCHAR* ContextLabel)
	{
		if (!UE_LOG_ACTIVE(LogPreprocessorDump, Verbose))
		{
			return;
		}

		for (const TSharedRef<FAngelscriptModuleDesc>& ModuleRef : Result.Modules)
		{
			const FAngelscriptModuleDesc& Module = ModuleRef.Get();
			UE_LOG(LogPreprocessorDump, Verbose,
				TEXT("[Preprocessor:%s] module=%s imports=[%s]\n----- BEGIN -----\n%s\n----- END -----"),
				ContextLabel,
				*Module.ModuleName,
				*FString::Join(Module.ImportedModules, TEXT(", ")),
				*Result.JoinedCode(Module));
		}
	}

} // namespace PreprocessorTestHelpers

#endif // WITH_DEV_AUTOMATION_TESTS
