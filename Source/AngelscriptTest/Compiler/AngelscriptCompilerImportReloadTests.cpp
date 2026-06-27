#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "Core/AngelscriptEngine.h"
#include "Preprocessor/AngelscriptPreprocessor.h"

#include "HAL/FileManager.h"
#include "CQTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CompilerImportReloadTest
{
	static const FName ProviderModuleName(TEXT("Tests.Compiler.ImportReloadSource"));
	static const FName ConsumerModuleName(TEXT("Tests.Compiler.ImportReloadConsumer"));
	static const FString ProviderRelativeScriptPath(TEXT("Tests/Compiler/ImportReloadSource.as"));
	static const FString ConsumerRelativeScriptPath(TEXT("Tests/Compiler/ImportReloadConsumer.as"));
	static const FString EntryFunctionDeclaration(TEXT("int Entry()"));
	static const FString ImportedFunctionDeclaration(TEXT("int SharedValue()"));

	FString GetFixtureRoot()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("CompilerImportReloadFixtures"));
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
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCompilerImportReloadTests,
	"Angelscript.TestModule.Compiler.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(DeclaredFunctionImportRebindsAfterProviderReload)
	{


		const FString ProviderScriptSourceV1 = TEXT(R"AS(
	int SharedValue()
	{
		return 1;
	}
	)AS");

		const FString ProviderScriptSourceV2 = TEXT(R"AS(
	int SharedValue()
	{
		return 2;
	}
	)AS");

		const FString ConsumerScriptSource = TEXT(R"AS(
	import int SharedValue() from "Tests.Compiler.ImportReloadSource";

	int Entry()
	{
		return SharedValue();
	}
	)AS");

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		const FString ProviderAbsoluteScriptPath = CompilerImportReloadTest::WriteFixture(
			CompilerImportReloadTest::ProviderRelativeScriptPath,
			ProviderScriptSourceV1);
		const FString ConsumerAbsoluteScriptPath = CompilerImportReloadTest::WriteFixture(
			CompilerImportReloadTest::ConsumerRelativeScriptPath,
			ConsumerScriptSource);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*CompilerImportReloadTest::ConsumerModuleName.ToString());
			Engine.DiscardModule(*CompilerImportReloadTest::ProviderModuleName.ToString());
			IFileManager::Get().Delete(*ConsumerAbsoluteScriptPath, false, true);
			IFileManager::Get().Delete(*ProviderAbsoluteScriptPath, false, true);
		};

		TGuardValue<bool> AutomaticImportGuard(Engine.bUseAutomaticImportMethod, false);
		FScopedAutomaticImportsOverride AutomaticImportsOverride(Engine.GetScriptEngine());

		Engine.ResetDiagnostics();

		FAngelscriptPreprocessor InitialPreprocessor;
		InitialPreprocessor.AddFile(
			CompilerImportReloadTest::ProviderRelativeScriptPath,
			ProviderAbsoluteScriptPath);
		InitialPreprocessor.AddFile(
			CompilerImportReloadTest::ConsumerRelativeScriptPath,
			ConsumerAbsoluteScriptPath);

		const bool bInitialPreprocessSucceeded = InitialPreprocessor.Preprocess();
		const TArray<TSharedRef<FAngelscriptModuleDesc>> InitialModulesToCompile = InitialPreprocessor.GetModulesToCompile();

		int32 InitialPreprocessErrorCount = 0;
		const FString InitialPreprocessDiagnostics = FString::Join(
			CompilerImportReloadTest::CollectDiagnosticMessages(
				Engine,
				{ProviderAbsoluteScriptPath, ConsumerAbsoluteScriptPath},
				InitialPreprocessErrorCount),
			TEXT("\n"));

		ASSERT_THAT(IsTrue(
			bInitialPreprocessSucceeded,
			TEXT("Declared import reload should preprocess the initial provider and consumer")));
		ASSERT_THAT(AreEqual(
			0,
			InitialPreprocessErrorCount,
			TEXT("Declared import reload should keep initial preprocessing diagnostics empty")));
		ASSERT_THAT(IsTrue(
			InitialPreprocessDiagnostics.IsEmpty(),
			TEXT("Declared import reload should not accumulate initial preprocessing messages")));
		ASSERT_THAT(AreEqual(
			2,
			InitialModulesToCompile.Num(),
			TEXT("Declared import reload should emit two module descriptors on the initial compile")));
		if (!bInitialPreprocessSucceeded || InitialModulesToCompile.Num() != 2)
		{
			return;
		}

		Engine.ResetDiagnostics();

		TArray<TSharedRef<FAngelscriptModuleDesc>> InitialCompiledModules;
		const ECompileResult InitialCompileResult = Engine.CompileModules(
			ECompileType::SoftReloadOnly,
			InitialModulesToCompile,
			InitialCompiledModules);

		int32 InitialCompileErrorCount = 0;
		const FString InitialCompileDiagnostics = FString::Join(
			CompilerImportReloadTest::CollectDiagnosticMessages(
				Engine,
				{ProviderAbsoluteScriptPath, ConsumerAbsoluteScriptPath},
				InitialCompileErrorCount),
			TEXT("\n"));

		ASSERT_THAT(AreEqual(
			ECompileResult::FullyHandled,
			InitialCompileResult,
			TEXT("Declared import reload should compile the initial provider and consumer as FullyHandled")));
		ASSERT_THAT(AreEqual(
			0,
			InitialCompileErrorCount,
			TEXT("Declared import reload should keep initial compile diagnostics empty")));
		ASSERT_THAT(IsTrue(
			InitialCompileDiagnostics.IsEmpty(),
			TEXT("Declared import reload should not accumulate initial compile messages")));
		ASSERT_THAT(AreEqual(
			2,
			InitialCompiledModules.Num(),
			TEXT("Declared import reload should materialize two compiled modules on the initial compile")));
		if (InitialCompileResult != ECompileResult::FullyHandled || InitialCompiledModules.Num() != 2)
		{
			return;
		}

		TSharedPtr<FAngelscriptModuleDesc> InitialConsumerModule = Engine.GetModule(
			CompilerImportReloadTest::ConsumerModuleName.ToString());
		if (!this->Assert.IsTrue(
				InitialConsumerModule.IsValid(),
				TEXT("Declared import reload should keep the consumer module active after the initial compile")))
		{
			return;
		}

		const int ImportedFunctionCount = static_cast<int32>(InitialConsumerModule->ScriptModule->GetImportedFunctionCount());
		ASSERT_THAT(AreEqual(
			1,
			ImportedFunctionCount,
			TEXT("Declared import reload should preserve exactly one declared imported function on the consumer")));
		if (ImportedFunctionCount > 0)
		{
			ASSERT_THAT(AreEqual(
				CompilerImportReloadTest::ProviderModuleName.ToString(),
				FString(UTF8_TO_TCHAR(InitialConsumerModule->ScriptModule->GetImportedFunctionSourceModule(0))),
				TEXT("Declared import reload should preserve the consumer imported function source module")));
			ASSERT_THAT(AreEqual(
				CompilerImportReloadTest::ImportedFunctionDeclaration,
				FString(UTF8_TO_TCHAR(InitialConsumerModule->ScriptModule->GetImportedFunctionDeclaration(0))),
				TEXT("Declared import reload should preserve the consumer imported function declaration")));
		}

		int32 EntryResultBeforeReload = 0;
		const bool bExecutedBeforeReload = ExecuteIntFunction(
			&Engine,
			CompilerImportReloadTest::ConsumerRelativeScriptPath,
			CompilerImportReloadTest::ConsumerModuleName,
			CompilerImportReloadTest::EntryFunctionDeclaration,
			EntryResultBeforeReload);
		ASSERT_THAT(IsTrue(
			bExecutedBeforeReload,
			TEXT("Declared import reload should execute the consumer entry point before the provider reload")));
		if (bExecutedBeforeReload)
		{
			ASSERT_THAT(AreEqual(
				1,
				EntryResultBeforeReload,
				TEXT("Declared import reload should execute the initial provider implementation before the provider reload")));
		}

		CompilerImportReloadTest::WriteFixture(
			CompilerImportReloadTest::ProviderRelativeScriptPath,
			ProviderScriptSourceV2);

		Engine.ResetDiagnostics();

		FAngelscriptPreprocessor ReloadPreprocessor;
		ReloadPreprocessor.AddFile(
			CompilerImportReloadTest::ProviderRelativeScriptPath,
			ProviderAbsoluteScriptPath);

		const bool bReloadPreprocessSucceeded = ReloadPreprocessor.Preprocess();
		const TArray<TSharedRef<FAngelscriptModuleDesc>> ReloadModulesToCompile = ReloadPreprocessor.GetModulesToCompile();

		int32 ReloadPreprocessErrorCount = 0;
		const FString ReloadPreprocessDiagnostics = FString::Join(
			CompilerImportReloadTest::CollectDiagnosticMessages(
				Engine,
				{ProviderAbsoluteScriptPath},
				ReloadPreprocessErrorCount),
			TEXT("\n"));

		ASSERT_THAT(IsTrue(
			bReloadPreprocessSucceeded,
			TEXT("Declared import reload should preprocess the provider-only reload successfully")));
		ASSERT_THAT(AreEqual(
			0,
			ReloadPreprocessErrorCount,
			TEXT("Declared import reload should keep provider-only preprocessing diagnostics empty")));
		ASSERT_THAT(IsTrue(
			ReloadPreprocessDiagnostics.IsEmpty(),
			TEXT("Declared import reload should not accumulate provider-only preprocessing messages")));
		ASSERT_THAT(AreEqual(
			1,
			ReloadModulesToCompile.Num(),
			TEXT("Declared import reload should emit a single module descriptor when only the provider reloads")));
		if (!bReloadPreprocessSucceeded || ReloadModulesToCompile.Num() != 1)
		{
			return;
		}

		const FAngelscriptModuleDesc* ReloadProviderModuleDesc = CompilerImportReloadTest::FindModuleByName(
			ReloadModulesToCompile,
			CompilerImportReloadTest::ProviderModuleName.ToString());
		if (!this->Assert.IsNotNull(
				ReloadProviderModuleDesc,
				TEXT("Declared import reload should only emit the provider module descriptor during the provider-only reload")))
		{
			return;
		}

		Engine.ResetDiagnostics();

		TArray<TSharedRef<FAngelscriptModuleDesc>> ReloadCompiledModules;
		const ECompileResult ReloadCompileResult = Engine.CompileModules(
			ECompileType::SoftReloadOnly,
			ReloadModulesToCompile,
			ReloadCompiledModules);

		int32 ReloadCompileErrorCount = 0;
		const FString ReloadCompileDiagnostics = FString::Join(
			CompilerImportReloadTest::CollectDiagnosticMessages(
				Engine,
				{ProviderAbsoluteScriptPath, ConsumerAbsoluteScriptPath},
				ReloadCompileErrorCount),
			TEXT("\n"));

		ASSERT_THAT(AreEqual(
			ECompileResult::FullyHandled,
			ReloadCompileResult,
			TEXT("Declared import reload should compile the provider-only reload as FullyHandled")));
		ASSERT_THAT(AreEqual(
			0,
			ReloadCompileErrorCount,
			TEXT("Declared import reload should keep provider-only compile diagnostics empty")));
		ASSERT_THAT(IsTrue(
			ReloadCompileDiagnostics.IsEmpty(),
			TEXT("Declared import reload should not accumulate provider-only compile messages")));
		ASSERT_THAT(AreEqual(
			1,
			ReloadCompiledModules.Num(),
			TEXT("Declared import reload should materialize only one compiled module when only the provider reloads")));
		if (ReloadCompileResult != ECompileResult::FullyHandled || ReloadCompiledModules.Num() != 1)
		{
			return;
		}

		TSharedPtr<FAngelscriptModuleDesc> ReloadedConsumerModule = Engine.GetModule(
			CompilerImportReloadTest::ConsumerModuleName.ToString());
		ASSERT_THAT(IsTrue(
			ReloadedConsumerModule.IsValid(),
			TEXT("Declared import reload should keep the consumer module active after the provider-only reload")));
		if (ReloadedConsumerModule.IsValid())
		{
			const int ReloadImportedFunctionCount = static_cast<int32>(ReloadedConsumerModule->ScriptModule->GetImportedFunctionCount());
			ASSERT_THAT(AreEqual(
				1,
				ReloadImportedFunctionCount,
				TEXT("Declared import reload should keep exactly one declared imported function on the active consumer after the provider-only reload")));
			if (ReloadImportedFunctionCount > 0)
			{
				ASSERT_THAT(AreEqual(
					CompilerImportReloadTest::ProviderModuleName.ToString(),
					FString(UTF8_TO_TCHAR(ReloadedConsumerModule->ScriptModule->GetImportedFunctionSourceModule(0))),
					TEXT("Declared import reload should keep the reloaded consumer import source module stable")));
				ASSERT_THAT(AreEqual(
					CompilerImportReloadTest::ImportedFunctionDeclaration,
					FString(UTF8_TO_TCHAR(ReloadedConsumerModule->ScriptModule->GetImportedFunctionDeclaration(0))),
					TEXT("Declared import reload should keep the reloaded consumer import declaration stable")));
			}
		}

		int32 EntryResultAfterReload = 0;
		const bool bExecutedAfterReload = ExecuteIntFunction(
			&Engine,
			CompilerImportReloadTest::ConsumerRelativeScriptPath,
			CompilerImportReloadTest::ConsumerModuleName,
			CompilerImportReloadTest::EntryFunctionDeclaration,
			EntryResultAfterReload);
		ASSERT_THAT(IsTrue(
			bExecutedAfterReload,
			TEXT("Declared import reload should execute the same consumer entry point after the provider-only reload")));
		if (bExecutedAfterReload)
		{
			ASSERT_THAT(AreEqual(
				2,
				EntryResultAfterReload,
				TEXT("Declared import reload should rebind the active consumer import to the reloaded provider implementation")));
		}

		}

	}

};

#endif
