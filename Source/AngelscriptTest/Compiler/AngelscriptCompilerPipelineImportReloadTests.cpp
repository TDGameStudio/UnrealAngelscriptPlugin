#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptTestMacros.h"

#include "Core/AngelscriptEngine.h"
#include "Preprocessor/AngelscriptPreprocessor.h"

#include "HAL/FileManager.h"
#include "CQTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CompilerPipelineImportReloadTest
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
		const FString& ModuleName)
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
}

using namespace CompilerPipelineImportReloadTest;

TEST_CLASS_WITH_FLAGS(FCompilerPipelineImportReloadTests,
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

		const FString ProviderAbsoluteScriptPath = CompilerPipelineImportReloadTest::WriteFixture(
			CompilerPipelineImportReloadTest::ProviderRelativeScriptPath,
			ProviderScriptSourceV1);
		const FString ConsumerAbsoluteScriptPath = CompilerPipelineImportReloadTest::WriteFixture(
			CompilerPipelineImportReloadTest::ConsumerRelativeScriptPath,
			ConsumerScriptSource);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*CompilerPipelineImportReloadTest::ConsumerModuleName.ToString());
			Engine.DiscardModule(*CompilerPipelineImportReloadTest::ProviderModuleName.ToString());
			IFileManager::Get().Delete(*ConsumerAbsoluteScriptPath, false, true);
			IFileManager::Get().Delete(*ProviderAbsoluteScriptPath, false, true);
		};

		TGuardValue<bool> AutomaticImportGuard(Engine.bUseAutomaticImportMethod, false);
		FScopedAutomaticImportsOverride AutomaticImportsOverride(Engine.GetScriptEngine());

		Engine.ResetDiagnostics();

		FAngelscriptPreprocessor InitialPreprocessor;
		InitialPreprocessor.AddFile(
			CompilerPipelineImportReloadTest::ProviderRelativeScriptPath,
			ProviderAbsoluteScriptPath);
		InitialPreprocessor.AddFile(
			CompilerPipelineImportReloadTest::ConsumerRelativeScriptPath,
			ConsumerAbsoluteScriptPath);

		const bool bInitialPreprocessSucceeded = InitialPreprocessor.Preprocess();
		const TArray<TSharedRef<FAngelscriptModuleDesc>> InitialModulesToCompile = InitialPreprocessor.GetModulesToCompile();

		int32 InitialPreprocessErrorCount = 0;
		const FString InitialPreprocessDiagnostics = FString::Join(
			CompilerPipelineImportReloadTest::CollectDiagnosticMessages(
				Engine,
				{ProviderAbsoluteScriptPath, ConsumerAbsoluteScriptPath},
				InitialPreprocessErrorCount),
			TEXT("\n"));

		TestRunner->TestTrue(
			TEXT("Declared import reload should preprocess the initial provider and consumer"),
			bInitialPreprocessSucceeded);
		TestRunner->TestEqual(
			TEXT("Declared import reload should keep initial preprocessing diagnostics empty"),
			InitialPreprocessErrorCount,
			0);
		TestRunner->TestTrue(
			TEXT("Declared import reload should not accumulate initial preprocessing messages"),
			InitialPreprocessDiagnostics.IsEmpty());
		TestRunner->TestEqual(
			TEXT("Declared import reload should emit two module descriptors on the initial compile"),
			InitialModulesToCompile.Num(),
			2);
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
			CompilerPipelineImportReloadTest::CollectDiagnosticMessages(
				Engine,
				{ProviderAbsoluteScriptPath, ConsumerAbsoluteScriptPath},
				InitialCompileErrorCount),
			TEXT("\n"));

		TestRunner->TestEqual(
			TEXT("Declared import reload should compile the initial provider and consumer as FullyHandled"),
			InitialCompileResult,
			ECompileResult::FullyHandled);
		TestRunner->TestEqual(
			TEXT("Declared import reload should keep initial compile diagnostics empty"),
			InitialCompileErrorCount,
			0);
		TestRunner->TestTrue(
			TEXT("Declared import reload should not accumulate initial compile messages"),
			InitialCompileDiagnostics.IsEmpty());
		TestRunner->TestEqual(
			TEXT("Declared import reload should materialize two compiled modules on the initial compile"),
			InitialCompiledModules.Num(),
			2);
		if (InitialCompileResult != ECompileResult::FullyHandled || InitialCompiledModules.Num() != 2)
		{
			return;
		}

		TSharedPtr<FAngelscriptModuleDesc> InitialConsumerModule = Engine.GetModule(
			CompilerPipelineImportReloadTest::ConsumerModuleName.ToString());
		if (!TestRunner->TestTrue(
				TEXT("Declared import reload should keep the consumer module active after the initial compile"),
				InitialConsumerModule.IsValid()))
		{
			return;
		}

		const int ImportedFunctionCount = static_cast<int32>(InitialConsumerModule->ScriptModule->GetImportedFunctionCount());
		TestRunner->TestEqual(
			TEXT("Declared import reload should preserve exactly one declared imported function on the consumer"),
			ImportedFunctionCount,
			1);
		if (ImportedFunctionCount > 0)
		{
			TestRunner->TestEqual(
				TEXT("Declared import reload should preserve the consumer imported function source module"),
				FString(UTF8_TO_TCHAR(InitialConsumerModule->ScriptModule->GetImportedFunctionSourceModule(0))),
				CompilerPipelineImportReloadTest::ProviderModuleName.ToString());
			TestRunner->TestEqual(
				TEXT("Declared import reload should preserve the consumer imported function declaration"),
				FString(UTF8_TO_TCHAR(InitialConsumerModule->ScriptModule->GetImportedFunctionDeclaration(0))),
				CompilerPipelineImportReloadTest::ImportedFunctionDeclaration);
		}

		int32 EntryResultBeforeReload = 0;
		const bool bExecutedBeforeReload = ExecuteIntFunction(
			&Engine,
			CompilerPipelineImportReloadTest::ConsumerRelativeScriptPath,
			CompilerPipelineImportReloadTest::ConsumerModuleName,
			CompilerPipelineImportReloadTest::EntryFunctionDeclaration,
			EntryResultBeforeReload);
		TestRunner->TestTrue(
			TEXT("Declared import reload should execute the consumer entry point before the provider reload"),
			bExecutedBeforeReload);
		if (bExecutedBeforeReload)
		{
			TestRunner->TestEqual(
				TEXT("Declared import reload should execute the initial provider implementation before the provider reload"),
				EntryResultBeforeReload,
				1);
		}

		CompilerPipelineImportReloadTest::WriteFixture(
			CompilerPipelineImportReloadTest::ProviderRelativeScriptPath,
			ProviderScriptSourceV2);

		Engine.ResetDiagnostics();

		FAngelscriptPreprocessor ReloadPreprocessor;
		ReloadPreprocessor.AddFile(
			CompilerPipelineImportReloadTest::ProviderRelativeScriptPath,
			ProviderAbsoluteScriptPath);

		const bool bReloadPreprocessSucceeded = ReloadPreprocessor.Preprocess();
		const TArray<TSharedRef<FAngelscriptModuleDesc>> ReloadModulesToCompile = ReloadPreprocessor.GetModulesToCompile();

		int32 ReloadPreprocessErrorCount = 0;
		const FString ReloadPreprocessDiagnostics = FString::Join(
			CompilerPipelineImportReloadTest::CollectDiagnosticMessages(
				Engine,
				{ProviderAbsoluteScriptPath},
				ReloadPreprocessErrorCount),
			TEXT("\n"));

		TestRunner->TestTrue(
			TEXT("Declared import reload should preprocess the provider-only reload successfully"),
			bReloadPreprocessSucceeded);
		TestRunner->TestEqual(
			TEXT("Declared import reload should keep provider-only preprocessing diagnostics empty"),
			ReloadPreprocessErrorCount,
			0);
		TestRunner->TestTrue(
			TEXT("Declared import reload should not accumulate provider-only preprocessing messages"),
			ReloadPreprocessDiagnostics.IsEmpty());
		TestRunner->TestEqual(
			TEXT("Declared import reload should emit a single module descriptor when only the provider reloads"),
			ReloadModulesToCompile.Num(),
			1);
		if (!bReloadPreprocessSucceeded || ReloadModulesToCompile.Num() != 1)
		{
			return;
		}

		const FAngelscriptModuleDesc* ReloadProviderModuleDesc = CompilerPipelineImportReloadTest::FindModuleByName(
			ReloadModulesToCompile,
			CompilerPipelineImportReloadTest::ProviderModuleName.ToString());
		if (!TestRunner->TestNotNull(
				TEXT("Declared import reload should only emit the provider module descriptor during the provider-only reload"),
				ReloadProviderModuleDesc))
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
			CompilerPipelineImportReloadTest::CollectDiagnosticMessages(
				Engine,
				{ProviderAbsoluteScriptPath, ConsumerAbsoluteScriptPath},
				ReloadCompileErrorCount),
			TEXT("\n"));

		TestRunner->TestEqual(
			TEXT("Declared import reload should compile the provider-only reload as FullyHandled"),
			ReloadCompileResult,
			ECompileResult::FullyHandled);
		TestRunner->TestEqual(
			TEXT("Declared import reload should keep provider-only compile diagnostics empty"),
			ReloadCompileErrorCount,
			0);
		TestRunner->TestTrue(
			TEXT("Declared import reload should not accumulate provider-only compile messages"),
			ReloadCompileDiagnostics.IsEmpty());
		TestRunner->TestEqual(
			TEXT("Declared import reload should materialize only one compiled module when only the provider reloads"),
			ReloadCompiledModules.Num(),
			1);
		if (ReloadCompileResult != ECompileResult::FullyHandled || ReloadCompiledModules.Num() != 1)
		{
			return;
		}

		TSharedPtr<FAngelscriptModuleDesc> ReloadedConsumerModule = Engine.GetModule(
			CompilerPipelineImportReloadTest::ConsumerModuleName.ToString());
		TestRunner->TestTrue(
			TEXT("Declared import reload should keep the consumer module active after the provider-only reload"),
			ReloadedConsumerModule.IsValid());
		if (ReloadedConsumerModule.IsValid())
		{
			const int ReloadImportedFunctionCount = static_cast<int32>(ReloadedConsumerModule->ScriptModule->GetImportedFunctionCount());
			TestRunner->TestEqual(
				TEXT("Declared import reload should keep exactly one declared imported function on the active consumer after the provider-only reload"),
				ReloadImportedFunctionCount,
				1);
			if (ReloadImportedFunctionCount > 0)
			{
				TestRunner->TestEqual(
					TEXT("Declared import reload should keep the reloaded consumer import source module stable"),
					FString(UTF8_TO_TCHAR(ReloadedConsumerModule->ScriptModule->GetImportedFunctionSourceModule(0))),
					CompilerPipelineImportReloadTest::ProviderModuleName.ToString());
				TestRunner->TestEqual(
					TEXT("Declared import reload should keep the reloaded consumer import declaration stable"),
					FString(UTF8_TO_TCHAR(ReloadedConsumerModule->ScriptModule->GetImportedFunctionDeclaration(0))),
					CompilerPipelineImportReloadTest::ImportedFunctionDeclaration);
			}
		}

		int32 EntryResultAfterReload = 0;
		const bool bExecutedAfterReload = ExecuteIntFunction(
			&Engine,
			CompilerPipelineImportReloadTest::ConsumerRelativeScriptPath,
			CompilerPipelineImportReloadTest::ConsumerModuleName,
			CompilerPipelineImportReloadTest::EntryFunctionDeclaration,
			EntryResultAfterReload);
		TestRunner->TestTrue(
			TEXT("Declared import reload should execute the same consumer entry point after the provider-only reload"),
			bExecutedAfterReload);
		if (bExecutedAfterReload)
		{
			TestRunner->TestEqual(
				TEXT("Declared import reload should rebind the active consumer import to the reloaded provider implementation"),
				EntryResultAfterReload,
				2);
		}

		}

	}

};

#endif
