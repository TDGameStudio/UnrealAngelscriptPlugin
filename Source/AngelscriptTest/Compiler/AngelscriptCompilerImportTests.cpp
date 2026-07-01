#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "Core/AngelscriptEngine.h"
#include "Preprocessor/AngelscriptPreprocessor.h"

#include "HAL/FileManager.h"
#include "CQTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace CompilerImportTest
{
	static const FName ProviderModuleName(TEXT("Tests.Compiler.ImportSource"));
	static const FName ConsumerModuleName(TEXT("Tests.Compiler.ImportConsumer"));
	static const FString ProviderRelativeScriptPath(TEXT("Tests/Compiler/ImportSource.as"));
	static const FString ConsumerRelativeScriptPath(TEXT("Tests/Compiler/ImportConsumer.as"));
	static const FString EntryFunctionDeclaration(TEXT("int Entry()"));
	static const FString ImportedFunctionDeclaration(TEXT("int SharedValue()"));
	static const FName MissingSourceModuleName(TEXT("Tests.Compiler.MissingSource"));
	static const FName MissingSourceConsumerModuleName(TEXT("Tests.Compiler.ImportConsumerMissingSource"));
	static const FString MissingSourceConsumerRelativeScriptPath(TEXT("Tests/Compiler/ImportConsumerMissingSource.as"));
	static const FName SignatureMismatchProviderModuleName(TEXT("Tests.Compiler.ImportSourceSignatureMismatch"));
	static const FName SignatureMismatchConsumerModuleName(TEXT("Tests.Compiler.ImportConsumerSignatureMismatch"));
	static const FString SignatureMismatchProviderRelativeScriptPath(TEXT("Tests/Compiler/ImportSourceSignatureMismatch.as"));
	static const FString SignatureMismatchConsumerRelativeScriptPath(TEXT("Tests/Compiler/ImportConsumerSignatureMismatch.as"));

	FString GetFixtureRoot()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("CompilerImportFixtures"));
	}

	FString WriteFixture(const FString& RelativePath, const FString& Contents)
	{
		const FString AbsolutePath = FPaths::Combine(GetFixtureRoot(), RelativePath);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(AbsolutePath), true);
		FFileHelper::SaveStringToFile(Contents, *AbsolutePath);
		return AbsolutePath;
	}

	TArray<FString> CollectDiagnosticMessages(
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

	const FAngelscriptModuleDesc* FindModuleByName(
		const TArray<TSharedRef<FAngelscriptModuleDesc>>& Modules,
		const FString& TargetModuleName)
	{
		for (const TSharedRef<FAngelscriptModuleDesc>& Module : Modules)
		{
			if (Module->ModuleName == TargetModuleName)
			{
				return &Module.Get();
			}
		}

		return nullptr;
	}

	FString JoinModuleNames(const TArray<TSharedRef<FAngelscriptModuleDesc>>& Modules)
	{
		return FString::JoinBy(
			Modules,
			TEXT(" -> "),
			[](const TSharedRef<FAngelscriptModuleDesc>& Module)
			{
				return Module->ModuleName;
			});
	}

	const FAngelscriptEngine::FDiagnostic* FindMatchingErrorDiagnostic(
		const FAngelscriptEngine& Engine,
		const FString& AbsoluteFilename,
		const FString& MessageFragment)
	{
		const FAngelscriptEngine::FDiagnostics* Diagnostics = Engine.Diagnostics.Find(AbsoluteFilename);
		if (Diagnostics == nullptr)
		{
			return nullptr;
		}

		return Diagnostics->Diagnostics.FindByPredicate(
			[&MessageFragment](const FAngelscriptEngine::FDiagnostic& Diagnostic)
			{
				return Diagnostic.bIsError && Diagnostic.Message.Contains(MessageFragment);
			});
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCompilerImportTests,
	"Angelscript.TestModule.Compiler.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(DeclaredFunctionImportRoundTrip)
	{


		const FString ProviderScriptSource = TEXT(R"AS(
	int SharedValue()
	{
		return 77;
	}
	)AS");

		const FString ConsumerScriptSource = TEXT(R"AS(
	import int SharedValue() from "Tests.Compiler.ImportSource";

	int Entry()
	{
		return SharedValue();
	}
	)AS");

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		const FString ProviderAbsoluteScriptPath = CompilerImportTest::WriteFixture(
			CompilerImportTest::ProviderRelativeScriptPath,
			ProviderScriptSource);
		const FString ConsumerAbsoluteScriptPath = CompilerImportTest::WriteFixture(
			CompilerImportTest::ConsumerRelativeScriptPath,
			ConsumerScriptSource);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*CompilerImportTest::ConsumerModuleName.ToString());
			Engine.DiscardModule(*CompilerImportTest::ProviderModuleName.ToString());
			IFileManager::Get().Delete(*ConsumerAbsoluteScriptPath, false, true);
			IFileManager::Get().Delete(*ProviderAbsoluteScriptPath, false, true);
		};

		Engine.ResetDiagnostics();

		TGuardValue<bool> AutomaticImportGuard(Engine.bUseAutomaticImportMethod, false);
		FScopedAutomaticImportsOverride AutomaticImportsOverride(Engine.GetScriptEngine());

		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddFile(CompilerImportTest::ProviderRelativeScriptPath, ProviderAbsoluteScriptPath);
		Preprocessor.AddFile(CompilerImportTest::ConsumerRelativeScriptPath, ConsumerAbsoluteScriptPath);

		const bool bPreprocessSucceeded = Preprocessor.Preprocess();
		const TArray<TSharedRef<FAngelscriptModuleDesc>> ModulesToCompile = Preprocessor.GetModulesToCompile();

		int32 PreprocessErrorCount = 0;
		const TArray<FString> PreprocessMessages = CompilerImportTest::CollectDiagnosticMessages(
			Engine,
			{ProviderAbsoluteScriptPath, ConsumerAbsoluteScriptPath},
			PreprocessErrorCount);
		const FString PreprocessDiagnostics = FString::Join(PreprocessMessages, TEXT("\n"));

		ASSERT_THAT(IsTrue(
			bPreprocessSucceeded,
			TEXT("Declared import round-trip should preprocess successfully")));
		ASSERT_THAT(AreEqual(
			0,
			PreprocessErrorCount,
			TEXT("Declared import round-trip should keep preprocessing diagnostics empty")));
		ASSERT_THAT(IsTrue(
			PreprocessDiagnostics.IsEmpty(),
			TEXT("Declared import round-trip should not accumulate preprocessing messages")));
		ASSERT_THAT(AreEqual(
			2,
			ModulesToCompile.Num(),
			TEXT("Declared import round-trip should produce exactly two module descriptors")));
		if (!bPreprocessSucceeded || ModulesToCompile.Num() != 2)
		{
			return;
		}

		const FString ModuleOrder = CompilerImportTest::JoinModuleNames(ModulesToCompile);
		ASSERT_THAT(AreEqual(
			FString(TEXT("Tests.Compiler.ImportSource -> Tests.Compiler.ImportConsumer")),
			ModuleOrder,
			TEXT("Declared import round-trip should keep provider before consumer in compile order")));

		const FAngelscriptModuleDesc* ProviderModuleDesc = CompilerImportTest::FindModuleByName(
			ModulesToCompile,
			CompilerImportTest::ProviderModuleName.ToString());
		const FAngelscriptModuleDesc* ConsumerModuleDesc = CompilerImportTest::FindModuleByName(
			ModulesToCompile,
			CompilerImportTest::ConsumerModuleName.ToString());
		if (!this->Assert.IsNotNull(
				ProviderModuleDesc,
				TEXT("Declared import round-trip should emit the provider module descriptor"))
			|| !this->Assert.IsNotNull(
				ConsumerModuleDesc,
				TEXT("Declared import round-trip should emit the consumer module descriptor")))
		{
			return;
		}

		Engine.ResetDiagnostics();

		TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
		const ECompileResult CompileResult = Engine.CompileModules(
			ECompileType::SoftReloadOnly,
			ModulesToCompile,
			CompiledModules);

		int32 CompileErrorCount = 0;
		const TArray<FString> CompileMessages = CompilerImportTest::CollectDiagnosticMessages(
			Engine,
			{ProviderAbsoluteScriptPath, ConsumerAbsoluteScriptPath},
			CompileErrorCount);
		const FString CompileDiagnostics = FString::Join(CompileMessages, TEXT("\n"));
		if (!CompileDiagnostics.IsEmpty())
		{
			TestRunner->AddInfo(FString::Printf(TEXT("Declared import diagnostics: %s"), *CompileDiagnostics));
		}

		ASSERT_THAT(AreEqual(
			ECompileResult::FullyHandled,
			CompileResult,
			TEXT("Declared import round-trip should compile as FullyHandled")));
		ASSERT_THAT(AreEqual(
			0,
			CompileErrorCount,
			TEXT("Declared import round-trip should keep compile diagnostics empty")));
		ASSERT_THAT(IsTrue(
			CompileDiagnostics.IsEmpty(),
			TEXT("Declared import round-trip should not accumulate compile messages")));
		ASSERT_THAT(AreEqual(
			2,
			CompiledModules.Num(),
			TEXT("Declared import round-trip should materialize exactly two compiled modules")));
		if (CompileResult != ECompileResult::FullyHandled || CompiledModules.Num() != 2)
		{
			return;
		}

		TSharedPtr<FAngelscriptModuleDesc> CompiledProvider = Engine.GetModule(CompilerImportTest::ProviderModuleName.ToString());
		TSharedPtr<FAngelscriptModuleDesc> CompiledConsumer = Engine.GetModule(CompilerImportTest::ConsumerModuleName.ToString());
		if (!this->Assert.IsTrue(
				CompiledProvider.IsValid(),
				TEXT("Declared import round-trip should register the compiled provider module on the engine"))
			|| !this->Assert.IsTrue(
				CompiledConsumer.IsValid(),
				TEXT("Declared import round-trip should register the compiled consumer module on the engine")))
		{
			return;
		}

		asIScriptModule* ConsumerScriptModule = CompiledConsumer->ScriptModule;
		if (!this->Assert.IsNotNull(
			ConsumerScriptModule,
			TEXT("Declared import round-trip should expose a backing script module for the consumer")))
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			1,
			static_cast<int32>(ConsumerScriptModule->GetImportedFunctionCount()),
			TEXT("Declared import round-trip should preserve exactly one declared imported function")));
		if (ConsumerScriptModule->GetImportedFunctionCount() > 0)
		{
			ASSERT_THAT(AreEqual(
				CompilerImportTest::ProviderModuleName.ToString(),
				FString(UTF8_TO_TCHAR(ConsumerScriptModule->GetImportedFunctionSourceModule(0))),
				TEXT("Declared import round-trip should preserve the imported function source module")));
			ASSERT_THAT(AreEqual(
				CompilerImportTest::ImportedFunctionDeclaration,
				FString(UTF8_TO_TCHAR(ConsumerScriptModule->GetImportedFunctionDeclaration(0))),
				TEXT("Declared import round-trip should preserve the imported function declaration")));
		}

		int32 EntryResult = 0;
		const bool bExecuted = ExecuteIntFunction(
			&Engine,
			CompilerImportTest::ConsumerRelativeScriptPath,
			CompilerImportTest::ConsumerModuleName,
			CompilerImportTest::EntryFunctionDeclaration,
			EntryResult);
		ASSERT_THAT(IsTrue(
			bExecuted,
			TEXT("Declared import round-trip should execute the consumer entry point")));
		if (bExecuted)
		{
			ASSERT_THAT(AreEqual(
				77,
				EntryResult,
				TEXT("Declared import round-trip should route execution through the bound imported function")));
		}

		}

	}

	TEST_METHOD(DeclaredFunctionImportErrorsReportPreciseDiagnostics)
	{


		struct FDeclaredImportErrorTestCase
		{
			const TCHAR* Label = TEXT("");
			FName ProviderModuleName;
			FName ConsumerModuleName;
			FString ProviderRelativeScriptPath;
			FString ConsumerRelativeScriptPath;
			FString ProviderScriptSource;
			FString ConsumerScriptSource;
			FString ExpectedDiagnosticFragment;
			int32 ExpectedModuleDescCount = 0;
			int32 ExpectedCompiledModuleCount = 0;
			int32 ExpectedDiagnosticRow = 0;
		};

		const TArray<FDeclaredImportErrorTestCase> TestCases =
		{
			{TEXT("Missing module"), CompilerImportTest::MissingSourceModuleName, CompilerImportTest::MissingSourceConsumerModuleName, FString(), CompilerImportTest::MissingSourceConsumerRelativeScriptPath, FString(), TEXT("import int SharedValue() from \"Tests.Compiler.MissingSource\";\n\nint Entry()\n{\n\treturn SharedValue();\n}\n"), TEXT("could not find module Tests.Compiler.MissingSource to import from."), 1, 1, 1},
			{TEXT("Signature mismatch"), CompilerImportTest::SignatureMismatchProviderModuleName, CompilerImportTest::SignatureMismatchConsumerModuleName, CompilerImportTest::SignatureMismatchProviderRelativeScriptPath, CompilerImportTest::SignatureMismatchConsumerRelativeScriptPath, TEXT("int SharedValue(int Extra)\n{\n\treturn Extra;\n}\n"), TEXT("import int SharedValue() from \"Tests.Compiler.ImportSourceSignatureMismatch\";\n\nint Entry()\n{\n\treturn SharedValue();\n}\n"), TEXT("could not find function with this signature in module Tests.Compiler.ImportSourceSignatureMismatch."), 2, 2, 1}
		};

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		TestRunner->AddExpectedError(TEXT("Hot reload failed due to script compile errors. Keeping all old script code."), EAutomationExpectedErrorFlags::Contains, 2);

		const FString SignatureMismatchProviderAbsoluteScriptPath = CompilerImportTest::WriteFixture(
			CompilerImportTest::SignatureMismatchProviderRelativeScriptPath,
			TestCases[1].ProviderScriptSource);
		const FString MissingSourceConsumerAbsoluteScriptPath = CompilerImportTest::WriteFixture(
			CompilerImportTest::MissingSourceConsumerRelativeScriptPath,
			TestCases[0].ConsumerScriptSource);
		const FString SignatureMismatchConsumerAbsoluteScriptPath = CompilerImportTest::WriteFixture(
			CompilerImportTest::SignatureMismatchConsumerRelativeScriptPath,
			TestCases[1].ConsumerScriptSource);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*CompilerImportTest::MissingSourceConsumerModuleName.ToString());
			Engine.DiscardModule(*CompilerImportTest::SignatureMismatchConsumerModuleName.ToString());
			Engine.DiscardModule(*CompilerImportTest::SignatureMismatchProviderModuleName.ToString());
			IFileManager::Get().Delete(*MissingSourceConsumerAbsoluteScriptPath, false, true);
			IFileManager::Get().Delete(*SignatureMismatchConsumerAbsoluteScriptPath, false, true);
			IFileManager::Get().Delete(*SignatureMismatchProviderAbsoluteScriptPath, false, true);
		};

		for (const FDeclaredImportErrorTestCase& TestCase : TestCases)
		{
			const FString ConsumerAbsoluteScriptPath = FPaths::Combine(
				CompilerImportTest::GetFixtureRoot(),
				TestCase.ConsumerRelativeScriptPath);
			const FString ProviderAbsoluteScriptPath = TestCase.ProviderRelativeScriptPath.IsEmpty()
				? FString()
				: FPaths::Combine(CompilerImportTest::GetFixtureRoot(), TestCase.ProviderRelativeScriptPath);

			Engine.ResetDiagnostics();

			TGuardValue<bool> AutomaticImportGuard(Engine.bUseAutomaticImportMethod, false);
			FScopedAutomaticImportsOverride AutomaticImportsOverride(Engine.GetScriptEngine());

			FAngelscriptPreprocessor Preprocessor;
			if (!TestCase.ProviderRelativeScriptPath.IsEmpty()) { Preprocessor.AddFile(TestCase.ProviderRelativeScriptPath, ProviderAbsoluteScriptPath); }
			Preprocessor.AddFile(TestCase.ConsumerRelativeScriptPath, ConsumerAbsoluteScriptPath);

			const bool bPreprocessSucceeded = Preprocessor.Preprocess();
			const TArray<TSharedRef<FAngelscriptModuleDesc>> ModulesToCompile = Preprocessor.GetModulesToCompile();

			const TArray<FString> DiagnosticFiles = TestCase.ProviderRelativeScriptPath.IsEmpty()
				? TArray<FString>{ConsumerAbsoluteScriptPath}
				: TArray<FString>{ProviderAbsoluteScriptPath, ConsumerAbsoluteScriptPath};

			int32 PreprocessErrorCount = 0;
			const TArray<FString> PreprocessMessages = CompilerImportTest::CollectDiagnosticMessages(
				Engine,
				DiagnosticFiles,
				PreprocessErrorCount);
			const FString PreprocessDiagnostics = FString::Join(PreprocessMessages, TEXT("\n"));

			ASSERT_THAT(IsTrue(
				bPreprocessSucceeded,
				FString::Printf(TEXT("%s declared-import diagnostics test case should preprocess successfully"), TestCase.Label)));
			ASSERT_THAT(AreEqual(
				0,
				PreprocessErrorCount,
				FString::Printf(TEXT("%s declared-import diagnostics test case should keep preprocessing diagnostics empty"), TestCase.Label)));
			ASSERT_THAT(AreEqual(
				TestCase.ExpectedModuleDescCount,
				ModulesToCompile.Num(),
				FString::Printf(TEXT("%s declared-import diagnostics test case should produce the expected module descriptor count"), TestCase.Label)));
			if (!PreprocessDiagnostics.IsEmpty()) { TestRunner->AddInfo(FString::Printf(TEXT("%s preprocessor diagnostics: %s"), TestCase.Label, *PreprocessDiagnostics)); }
			if (!bPreprocessSucceeded || ModulesToCompile.Num() != TestCase.ExpectedModuleDescCount) { return; }

			Engine.ResetDiagnostics();
			TestRunner->AddExpectedError(*TestCase.ExpectedDiagnosticFragment, EAutomationExpectedErrorFlags::Contains, 1);

			TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
			const ECompileResult CompileResult = Engine.CompileModules(
				ECompileType::SoftReloadOnly,
				ModulesToCompile,
				CompiledModules);

			int32 CompileErrorCount = 0;
			const TArray<FString> CompileMessages = CompilerImportTest::CollectDiagnosticMessages(
				Engine,
				DiagnosticFiles,
				CompileErrorCount);
			const FString CompileDiagnostics = FString::Join(CompileMessages, TEXT("\n"));
			if (!CompileDiagnostics.IsEmpty()) { TestRunner->AddInfo(FString::Printf(TEXT("%s compile diagnostics: %s"), TestCase.Label, *CompileDiagnostics)); }

			ASSERT_THAT(AreEqual(
				ECompileResult::Error,
				CompileResult,
				FString::Printf(TEXT("%s declared-import diagnostics test case should fail compilation"), TestCase.Label)));
			ASSERT_THAT(AreEqual(
				TestCase.ExpectedCompiledModuleCount,
				CompiledModules.Num(),
				FString::Printf(TEXT("%s declared-import diagnostics test case should keep the expected number of compiled module descriptors"), TestCase.Label)));
			ASSERT_THAT(IsTrue(
				CompileErrorCount > 0,
				FString::Printf(TEXT("%s declared-import diagnostics test case should capture at least one compile error"), TestCase.Label)));
			ASSERT_THAT(IsFalse(
				Engine.GetModule(TestCase.ConsumerModuleName.ToString()).IsValid(),
				FString::Printf(TEXT("%s declared-import diagnostics test case should keep the consumer module inactive after the failed compile"), TestCase.Label)));
			if (!TestCase.ProviderRelativeScriptPath.IsEmpty())
				ASSERT_THAT(IsFalse(
					Engine.GetModule(TestCase.ProviderModuleName.ToString()).IsValid(),
					FString::Printf(TEXT("%s declared-import diagnostics test case should avoid swapping in the provider when the batch fails"), TestCase.Label)));

			const FAngelscriptEngine::FDiagnostic* MatchingDiagnostic = CompilerImportTest::FindMatchingErrorDiagnostic(
				Engine,
				ConsumerAbsoluteScriptPath,
				TestCase.ExpectedDiagnosticFragment);
			ASSERT_THAT(IsNotNull(
				MatchingDiagnostic,
				FString::Printf(TEXT("%s declared-import diagnostics test case should attach the expected error to the consumer file"), TestCase.Label)));
			if (MatchingDiagnostic != nullptr)
			{
				ASSERT_THAT(AreEqual(
					TestCase.ExpectedDiagnosticRow,
					MatchingDiagnostic->Row,
					FString::Printf(TEXT("%s declared-import diagnostics test case should keep the diagnostic row pinned to the import line"), TestCase.Label)));
				ASSERT_THAT(AreEqual(
					1,
					MatchingDiagnostic->Column,
					FString::Printf(TEXT("%s declared-import diagnostics test case should keep the diagnostic column pinned to the import line start"), TestCase.Label)));
			}
		}

		}

	}

};

#endif
