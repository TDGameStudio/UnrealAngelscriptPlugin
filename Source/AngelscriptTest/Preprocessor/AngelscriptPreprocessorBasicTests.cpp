// ============================================================================
// AngelscriptPreprocessorBasicTests.cpp
//
// Preprocessor tests for core functionality: basic parsing, macro detection,
// import parsing, stress/determinism, and API contract (single-use semantics).
//
// Migrated from:
//   - AngelscriptPreprocessorTests.cpp (BasicParse, MacroDetection, ImportParsing)
//   - AngelscriptPreprocessorStressTests.cpp (LongSourceRemainsDeterministic)
//   - AngelscriptPreprocessorApiContractTests.cpp (PreprocessIsSingleUse)
//
// Automation prefix: Angelscript.TestModule.Preprocessor.Basic.*
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "Preprocessor/AngelscriptPreprocessorTestHelpers.h"

#if WITH_ANGELSCRIPT_UNITTESTS

// ============================================================================
// Test class
// ============================================================================

TEST_CLASS_WITH_FLAGS(FAngelscriptPreprocessorBasicTest,
	"Angelscript.TestModule.Preprocessor.Basic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	// ========================================================================
	// BasicParse — minimal script produces one module with correct name
	// ========================================================================
	TEST_METHOD(BasicParse)
	{
		using namespace PreprocessorTestHelpers;

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		FFixtureFile File(TEXT("Tests/Preprocessor/BasicModule.as"), TEXT(R"(
int ReturnSeven()
{
    return 7;
}
)"));

		auto Result = RunPreprocess(Engine, File);
		LogProcessedCode(Result, TEXT("BasicParse"));

		AssertPreprocessSucceeded(*TestRunner, Result);
		AssertModuleCount(*TestRunner, Result, 1);

		const FAngelscriptModuleDesc* Module = AssertModuleExists(
			*TestRunner, Result, TEXT("Tests.Preprocessor.BasicModule"));
		if (Module != nullptr)
		{
			ASSERT_THAT(AreEqual(1, Module->Code.Num(), TEXT("Basic parse should emit a single code section")));
			ASSERT_THAT(IsTrue(Module->Code[0].Code.Contains(TEXT("ReturnSeven")), TEXT("Processed code should contain the function body")));
		}

		}
	}

	// ========================================================================
	// MacroDetection — UPROPERTY and UFUNCTION macros are recorded
	// ========================================================================
	TEST_METHOD(MacroDetection)
	{
		using namespace PreprocessorTestHelpers;

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		FFixtureFile File(TEXT("Tests/Preprocessor/MacroActor.as"), TEXT(R"(
class AMacroActor : AActor
{
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UStaticMesh Mesh;

    UFUNCTION(BlueprintOverride)
    void BeginPlay()
    {
    }
}
)"));

		auto Session = RunPreprocessSession(Engine, File);

		AssertPreprocessSucceeded(*TestRunner, Session.Result);

		const TArray<const FAngelscriptPreprocessor::FMacro*> Macros = Session.GatherMacros();

		const bool bHasPropertyMacro = Macros.ContainsByPredicate(
			[](const FAngelscriptPreprocessor::FMacro* Macro)
			{
				return Macro->Type == FAngelscriptPreprocessor::EMacroType::Property
					&& Macro->Name == TEXT("Mesh");
			});
		const bool bHasFunctionMacro = Macros.ContainsByPredicate(
			[](const FAngelscriptPreprocessor::FMacro* Macro)
			{
				return Macro->Type == FAngelscriptPreprocessor::EMacroType::Function
					&& Macro->Name == TEXT("BeginPlay");
			});

		ASSERT_THAT(IsTrue(bHasPropertyMacro, TEXT("Should record UPROPERTY macro for Mesh")));
		ASSERT_THAT(IsTrue(bHasFunctionMacro, TEXT("Should record UFUNCTION macro for BeginPlay")));

		}
	}

	// ========================================================================
	// ImportParsing — manual import is resolved and stripped from code
	// ========================================================================
	TEST_METHOD(ImportParsing)
	{
		using namespace PreprocessorTestHelpers;

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		FFixtureFile SharedFile(TEXT("Tests/Preprocessor/Shared.as"), TEXT(R"(
int SharedValue()
{
    return 11;
}
)"));

		FFixtureFile ImportingFile(TEXT("Tests/Preprocessor/UsesImport.as"), TEXT(R"(
import Tests.Preprocessor.Shared;
int UseShared()
{
    return SharedValue();
}
)"));

		TArray<FFixtureFile> Files;
		Files.Emplace(MoveTemp(SharedFile));
		Files.Emplace(MoveTemp(ImportingFile));

		auto Result = RunPreprocess(Engine, Files);

		// On-demand inspection of preprocessor output. Silent by default
		// (LogPreprocessorDump=NoLogging). Enable via:
		//   -LogCmds="LogPreprocessorDump Verbose"
		LogProcessedCode(Result, TEXT("ImportParsing"));

		AssertPreprocessSucceeded(*TestRunner, Result);
		AssertModuleCount(*TestRunner, Result, 2);

		const FAngelscriptModuleDesc* ImportingModule = AssertModuleExists(
			*TestRunner, Result, TEXT("Tests.Preprocessor.UsesImport"));
		if (ImportingModule != nullptr)
		{
			AssertModuleImports(*TestRunner, *ImportingModule, TEXT("Tests.Preprocessor.Shared"));
			AssertModuleCodeNotContains(*TestRunner, Result, *ImportingModule,
				TEXT("import Tests.Preprocessor.Shared;"));
		}

		}
	}

	// ========================================================================
	// LongSourceRemainsDeterministic — 320 chained functions preprocess
	// deterministically across repeated runs, compile, and execute correctly
	// ========================================================================
	TEST_METHOD(LongSourceRemainsDeterministic)
	{
		using namespace PreprocessorTestHelpers;

		static constexpr int32 FunctionCount = 320;
		static constexpr int32 BaseReturnValue = 17;
		static constexpr int32 MinimumCodeLength = 30000;
		static constexpr int32 ExpectedEntryResult = 336;
		static const FName ModuleName(TEXT("Tests.Preprocessor.Stress.LongSourceRemainsDeterministic"));

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		// Build a long script with chained functions
		FString Source;
		Source.Reserve(FunctionCount * 160);
		Source += TEXT("// Long-source preprocessor stress fixture\n\n");

		for (int32 I = 0; I < FunctionCount; ++I)
		{
			Source += FString::Printf(
				TEXT("// Padding_%03d_abcdefghijklmnopqrstuvwxyz0123456789\n"), I);
			Source += FString::Printf(TEXT("int Value_%03d()\n{\n"), I);
			if (I == 0)
			{
				Source += FString::Printf(TEXT("    return %d;\n"), BaseReturnValue);
			}
			else
			{
				Source += FString::Printf(TEXT("    return Value_%03d() + 1;\n"), I - 1);
			}
			Source += TEXT("}\n\n");
		}
		Source += TEXT("int Entry()\n{\n");
		Source += FString::Printf(TEXT("    return Value_%03d();\n"), FunctionCount - 1);
		Source += TEXT("}\n");

		ASSERT_THAT(IsTrue(Source.Len() > MinimumCodeLength, TEXT("Stress fixture should exceed minimum length")));

		const FString RelativePath = TEXT("Tests/Preprocessor/Stress/LongSourceRemainsDeterministic.as");
		FFixtureFile File(RelativePath, Source);

		// First preprocess
		auto Result1 = RunPreprocess(Engine, File);
		LogProcessedCode(Result1, TEXT("LongSource_Pass1"));
		AssertPreprocessSucceeded(*TestRunner, Result1);
		AssertModuleCount(*TestRunner, Result1, 1);

		const FAngelscriptModuleDesc* Module1 = Result1.FindModule(ModuleName.ToString());
		if (!this->Assert.IsNotNull(Module1, TEXT("First preprocess should produce the module")))
		{
			return;
		}

		ASSERT_THAT(IsTrue(!Module1->Code[0].Code.IsEmpty(), TEXT("Code should be non-empty")));
		ASSERT_THAT(IsTrue(Module1->Code[0].Code.Len() > MinimumCodeLength, TEXT("Code should exceed minimum length")));

		const int64 FirstCodeHash = Module1->CodeHash;
		const FString FirstCode = Module1->Code[0].Code;

		// Second preprocess — should be deterministic
		auto Result2 = RunPreprocess(Engine, File);
		LogProcessedCode(Result2, TEXT("LongSource_Pass2"));
		AssertPreprocessSucceeded(*TestRunner, Result2);

		const FAngelscriptModuleDesc* Module2 = Result2.FindModule(ModuleName.ToString());
		ASSERT_THAT(IsNotNull(Module2, TEXT("Second preprocess should produce the module")));
		ASSERT_THAT(AreEqual(FirstCodeHash, Module2->CodeHash, TEXT("Repeated preprocess should keep same code hash")));
		ASSERT_THAT(AreEqual(FirstCode, Module2->Code[0].Code, TEXT("Repeated preprocess should keep same code")));

		// Compile and execute
		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine, ECompileType::SoftReloadOnly, ModuleName,
			RelativePath, Source, true, Summary);

		ASSERT_THAT(IsTrue(bCompiled, TEXT("Long source should compile successfully")));
		ASSERT_THAT(AreEqual(1, Summary.CompiledModuleCount, TEXT("Should compile exactly one module")));
		ASSERT_THAT(AreEqual(0, Summary.Diagnostics.Num(), TEXT("Should emit no diagnostics")));

		int32 EntryResult = 0;
		const bool bExecuted = bCompiled
			&& ExecuteIntFunction(&Engine, RelativePath, ModuleName, TEXT("int Entry()"), EntryResult);
		ASSERT_THAT(IsTrue(bExecuted, TEXT("Entry should execute")));
		ASSERT_THAT(AreEqual(ExpectedEntryResult, EntryResult, TEXT("Entry should return expected chained value")));

		}
	}

	// ========================================================================
	// PreprocessIsSingleUse — Preprocess() can only be called once;
	// late AddFile() and second Preprocess() must not change results
	// ========================================================================
	TEST_METHOD(PreprocessIsSingleUse)
	{
		using namespace PreprocessorTestHelpers;

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		TestRunner->AddExpectedErrorPlain(
			TEXT("Ensure condition failed: !bIsPreprocessed"),
			EAutomationExpectedErrorFlags::Contains, 4);
		TestRunner->AddExpectedErrorPlain(
			TEXT("LogOutputDevice:"),
			EAutomationExpectedErrorFlags::Contains, 0);

		Engine.ResetDiagnostics();

		FFixtureFile FirstFile(TEXT("Tests/Preprocessor/ApiContract/First.as"), TEXT(R"(
int Entry()
{
    return 7;
}
)"));

		FFixtureFile SecondFile(TEXT("Tests/Preprocessor/ApiContract/Second.as"), TEXT(R"(
int Entry()
{
    return 11;
}
)"));

		// First: normal preprocess with one file
		TOptional<TGuardValue<bool>> ImportGuard;
		ImportGuard.Emplace(Engine.bUseAutomaticImportMethod, false);

		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddFile(FirstFile.RelativePath, FirstFile.AbsolutePath);

		const bool bFirstSucceeded = Preprocessor.Preprocess();
		const TArray<TSharedRef<FAngelscriptModuleDesc>> FirstModules = Preprocessor.GetModulesToCompile();

		ASSERT_THAT(IsTrue(bFirstSucceeded, TEXT("First Preprocess() should succeed")));
		ASSERT_THAT(AreEqual(1, FirstModules.Num(), TEXT("First Preprocess() should emit one module")));

		// Capture snapshot of first module
		const FString FirstModuleName = TEXT("Tests.Preprocessor.ApiContract.First");
		const FString SecondModuleName = TEXT("Tests.Preprocessor.ApiContract.Second");

		const FAngelscriptModuleDesc* FirstModule = nullptr;
		for (const auto& M : FirstModules)
		{
			if (M->ModuleName == FirstModuleName)
			{
				FirstModule = &M.Get();
				break;
			}
		}
		if (!this->Assert.IsNotNull(FirstModule, TEXT("First module should exist")))
		{
			return;
		}

		const int64 OriginalCodeHash = FirstModule->CodeHash;
		const FString OriginalCode = FirstModule->Code.Num() > 0 ? FirstModule->Code[0].Code : FString();

		// Late AddFile should not change results
		Preprocessor.AddFile(SecondFile.RelativePath, SecondFile.AbsolutePath);
		const TArray<TSharedRef<FAngelscriptModuleDesc>> ModulesAfterLateAdd = Preprocessor.GetModulesToCompile();

		ASSERT_THAT(AreEqual(1, ModulesAfterLateAdd.Num(), TEXT("Late AddFile should not change module count")));

		bool bSecondModuleFound = false;
		for (const auto& M : ModulesAfterLateAdd)
		{
			if (M->ModuleName == SecondModuleName) { bSecondModuleFound = true; break; }
		}
		ASSERT_THAT(IsFalse(bSecondModuleFound, TEXT("Late AddFile should not materialize second module")));

		// Second Preprocess() should fail
		const bool bSecondSucceeded = Preprocessor.Preprocess();
		ASSERT_THAT(IsFalse(bSecondSucceeded, TEXT("Second Preprocess() should fail")));

		const TArray<TSharedRef<FAngelscriptModuleDesc>> ModulesAfterSecond = Preprocessor.GetModulesToCompile();
		ASSERT_THAT(AreEqual(1, ModulesAfterSecond.Num(), TEXT("Second Preprocess() should keep module count unchanged")));

		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
