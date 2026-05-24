#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "CQTest.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Preprocessor/AngelscriptPreprocessor.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS


namespace ComposeOntoClassTest
{
	static const FName ModuleName(TEXT("ComposeOntoMissingTargetMod"));
	static const FString RelativeFilename(TEXT("ComposeOntoMissingTargetMod.as"));
	static const FName GeneratedClassName(TEXT("UComposeOntoProbe"));
	static const FName ComposeHostClassName(TEXT("UComposeOntoHost"));
	static const FName ComposeProjectedClassName(TEXT("UComposeOntoProjected"));
	static const FString MissingComposeTarget(TEXT("UNonexistentComposeHost"));
	static const TCHAR* ExpectedHotReloadError =
		TEXT("An error was encountered during angelscript hot reload. Keeping old angelscript code active.");
	static const TCHAR* ExpectedComposeUnsupportedFragment = TEXT("compose materialization is not implemented yet");

	struct FPreparedAnnotatedModules
	{
		FString AbsoluteFilename;
		TArray<TSharedRef<FAngelscriptModuleDesc>> Modules;
	};

	static FString BuildComposeOntoProbeScript()
	{
		return TEXT(R"AS(
UCLASS()
class UComposeOntoProbe : UObject
{
	UFUNCTION()
	int GetValue()
	{
		return 7;
	}
}
)AS");
	}

	static FString BuildComposeOntoValidTargetScript()
	{
		return TEXT(R"AS(
UCLASS()
class UComposeOntoHost : UObject
{
	UFUNCTION()
	int GetHostValue()
	{
		return 11;
	}
}

UCLASS()
class UComposeOntoProjected : UObject
{
	UFUNCTION()
	int GetProjectedValue()
	{
		return 17;
	}
}
)AS");
	}

	static bool PrepareAnnotatedModulesForGenerator(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FString& ScriptSource,
		FPreparedAnnotatedModules& OutPreparedModules)
	{
		const FString AutomationDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"));
		const FString UniqueSuffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		OutPreparedModules.AbsoluteFilename = FPaths::Combine(
			AutomationDirectory,
			FString::Printf(TEXT("%s_%s.as"), *ModuleName.ToString(), *UniqueSuffix));

		IFileManager::Get().MakeDirectory(*AutomationDirectory, true);
		if (!FFileHelper::SaveStringToFile(ScriptSource, *OutPreparedModules.AbsoluteFilename))
		{
			Test.AddError(FString::Printf(
				TEXT("ComposeOntoClass missing-target test failed to write fixture script to '%s'."),
				*OutPreparedModules.AbsoluteFilename));
			return false;
		}

		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddFile(RelativeFilename, OutPreparedModules.AbsoluteFilename);
		if (!Preprocessor.Preprocess())
		{
			ReportCompileDiagnostics(Test, Engine, OutPreparedModules.AbsoluteFilename);
			Test.AddError(TEXT("ComposeOntoClass missing-target test failed to preprocess the prepared annotated module."));
			return false;
		}

		OutPreparedModules.Modules = Preprocessor.GetModulesToCompile();
		if (!Test.TestEqual(
			TEXT("ComposeOntoClass missing-target test should preprocess exactly one module descriptor"),
			OutPreparedModules.Modules.Num(),
			1))
		{
			return false;
		}

		return true;
	}

	static bool DiagnosticsContainAllFragments(
		const FAngelscriptEngine& Engine,
		const FString& Section,
		const TArray<FString>& ExpectedFragments)
	{
		const FAngelscriptEngine::FDiagnostics* FileDiagnostics = Engine.Diagnostics.Find(Section);
		if (FileDiagnostics == nullptr)
		{
			return false;
		}

		for (const FAngelscriptEngine::FDiagnostic& Diagnostic : FileDiagnostics->Diagnostics)
		{
			bool bMatchedAllFragments = Diagnostic.bIsError;
			for (const FString& Fragment : ExpectedFragments)
			{
				if (!Diagnostic.Message.Contains(Fragment))
				{
					bMatchedAllFragments = false;
					break;
				}
			}

			if (bMatchedAllFragments)
			{
				return true;
			}
		}

		return false;
	}

	static ECompileResult CompilePreparedAnnotatedModules(
		FAngelscriptEngine& Engine,
		const TArray<TSharedRef<FAngelscriptModuleDesc>>& ModulesToCompile)
	{
		TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
		TGuardValue<bool> AutomaticImportGuard(Engine.bUseAutomaticImportMethod, false);
		FScopedAutomaticImportsOverride AutomaticImportsOverride(Engine.GetScriptEngine());
		FAngelscriptEngineScope EngineScope(Engine);
		return Engine.CompileModules(ECompileType::FullReload, ModulesToCompile, CompiledModules);
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptComposeOntoClassTests,
	"Angelscript.TestModule.ClassGenerator.ComposeOntoClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(MissingTargetFailsClosed)
	{
		using namespace ComposeOntoClassTest;
		TestRunner->AddExpectedErrorPlain(
			ComposeOntoClassTest::MissingComposeTarget,
			EAutomationExpectedErrorFlags::Contains,
			1);
		TestRunner->AddExpectedErrorPlain(
			ComposeOntoClassTest::ExpectedHotReloadError,
			EAutomationExpectedErrorFlags::Contains,
			1);

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		Engine.Diagnostics.Empty();
		Engine.LastEmittedDiagnostics.Empty();
		Engine.bDiagnosticsDirty = false;

		ComposeOntoClassTest::FPreparedAnnotatedModules PreparedModules;
		if (!ComposeOntoClassTest::PrepareAnnotatedModulesForGenerator(
			*TestRunner,
			Engine,
			ComposeOntoClassTest::BuildComposeOntoProbeScript(),
			PreparedModules))
		{
			return;
		}

		const FString PreparedModuleName = PreparedModules.Modules[0]->ModuleName;
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*PreparedModuleName);
			ASTEST_RESET_ENGINE(Engine);

			if (!PreparedModules.AbsoluteFilename.IsEmpty())
			{
				IFileManager::Get().Delete(*PreparedModules.AbsoluteFilename, false, true, true);
			}
		};

		if (!TestRunner->TestEqual(
			TEXT("ComposeOntoClass missing-target test should preprocess exactly one class descriptor"),
			PreparedModules.Modules[0]->Classes.Num(),
			1))
		{
			return;
		}

		TSharedPtr<FAngelscriptClassDesc> ClassDesc = PreparedModules.Modules[0]->Classes[0];
		if (!TestRunner->TestNotNull(TEXT("ComposeOntoClass missing-target test should preprocess a class descriptor"), ClassDesc.Get()))
		{
			return;
		}

		ClassDesc->ComposeOntoClass = ComposeOntoClassTest::MissingComposeTarget;

		const ECompileResult CompileResult =
			ComposeOntoClassTest::CompilePreparedAnnotatedModules(Engine, PreparedModules.Modules);
		const TArray<FString> ExpectedDiagnosticFragments
		{
			TEXT("ComposeOntoClass"),
			ComposeOntoClassTest::MissingComposeTarget
		};

		TestRunner->TestEqual(
			TEXT("ComposeOntoClass missing-target test should fail compilation instead of silently succeeding"),
			CompileResult,
			ECompileResult::Error);
		TestRunner->TestTrue(
			TEXT("ComposeOntoClass missing-target test should emit a diagnostic naming the missing compose target"),
			ComposeOntoClassTest::DiagnosticsContainAllFragments(
				Engine,
				PreparedModules.AbsoluteFilename,
				ExpectedDiagnosticFragments));
		TestRunner->TestNull(
			TEXT("ComposeOntoClass missing-target test should not publish the composed script class"),
			FindGeneratedClass(&Engine, ComposeOntoClassTest::GeneratedClassName));
		TestRunner->TestTrue(
			TEXT("ComposeOntoClass missing-target test should not publish a module record after failure"),
			!Engine.GetModuleByModuleName(PreparedModuleName).IsValid());

		}
	}

	TEST_METHOD(ValidTargetDoesNotSilentlyPublishNoOpClass)
	{
		using namespace ComposeOntoClassTest;
		TestRunner->AddExpectedErrorPlain(
			ComposeOntoClassTest::ExpectedComposeUnsupportedFragment,
			EAutomationExpectedErrorFlags::Contains,
			1);
		TestRunner->AddExpectedErrorPlain(
			ComposeOntoClassTest::ExpectedHotReloadError,
			EAutomationExpectedErrorFlags::Contains,
			1);

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		Engine.Diagnostics.Empty();
		Engine.LastEmittedDiagnostics.Empty();
		Engine.bDiagnosticsDirty = false;

		ComposeOntoClassTest::FPreparedAnnotatedModules PreparedModules;
		if (!ComposeOntoClassTest::PrepareAnnotatedModulesForGenerator(
			*TestRunner,
			Engine,
			ComposeOntoClassTest::BuildComposeOntoValidTargetScript(),
			PreparedModules))
		{
			return;
		}

		const FString PreparedModuleName = PreparedModules.Modules[0]->ModuleName;
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*PreparedModuleName);
			ASTEST_RESET_ENGINE(Engine);

			if (!PreparedModules.AbsoluteFilename.IsEmpty())
			{
				IFileManager::Get().Delete(*PreparedModules.AbsoluteFilename, false, true, true);
			}
		};

		if (!TestRunner->TestEqual(
			TEXT("ComposeOntoClass valid-target test should preprocess exactly two class descriptors"),
			PreparedModules.Modules[0]->Classes.Num(),
			2))
		{
			return;
		}

		TSharedPtr<FAngelscriptClassDesc> HostClassDesc;
		TSharedPtr<FAngelscriptClassDesc> ProjectedClassDesc;
		for (const TSharedRef<FAngelscriptClassDesc>& ClassDesc : PreparedModules.Modules[0]->Classes)
		{
			if (ClassDesc->ClassName == ComposeOntoClassTest::ComposeHostClassName.ToString())
			{
				HostClassDesc = ClassDesc;
			}
			else if (ClassDesc->ClassName == ComposeOntoClassTest::ComposeProjectedClassName.ToString())
			{
				ProjectedClassDesc = ClassDesc;
			}
		}

		if (!TestRunner->TestNotNull(TEXT("ComposeOntoClass valid-target test should preprocess the compose host descriptor"), HostClassDesc.Get())
			|| !TestRunner->TestNotNull(TEXT("ComposeOntoClass valid-target test should preprocess the projected descriptor"), ProjectedClassDesc.Get()))
		{
			return;
		}

		ProjectedClassDesc->ComposeOntoClass = ComposeOntoClassTest::ComposeHostClassName.ToString();

		const ECompileResult CompileResult =
			ComposeOntoClassTest::CompilePreparedAnnotatedModules(Engine, PreparedModules.Modules);
		const TArray<FString> ExpectedDiagnosticFragments
		{
			TEXT("ComposeOntoClass"),
			ComposeOntoClassTest::ComposeHostClassName.ToString(),
			ComposeOntoClassTest::ExpectedComposeUnsupportedFragment
		};

		TestRunner->TestEqual(
			TEXT("ComposeOntoClass valid-target test should fail compilation instead of silently publishing a no-op composed class"),
			CompileResult,
			ECompileResult::Error);
		TestRunner->TestTrue(
			TEXT("ComposeOntoClass valid-target test should emit an unsupported-yet diagnostic for the real compose target"),
			ComposeOntoClassTest::DiagnosticsContainAllFragments(
				Engine,
				PreparedModules.AbsoluteFilename,
				ExpectedDiagnosticFragments));
		TestRunner->TestNull(
			TEXT("ComposeOntoClass valid-target test should not publish the projected compose class while compose materialization is unsupported"),
			FindGeneratedClass(&Engine, ComposeOntoClassTest::ComposeProjectedClassName));
		TestRunner->TestTrue(
			TEXT("ComposeOntoClass valid-target test should not publish a module record after the unsupported compose path"),
			!Engine.GetModuleByModuleName(PreparedModuleName).IsValid());

		}
	}
};

#endif
