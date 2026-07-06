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
#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptComposeOntoClassTests,
	"Angelscript.TestModule.ClassGenerator.ComposeOntoClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName ModuleName = FName(TEXT("ComposeOntoMissingTargetMod"));
	inline static const FString RelativeFilename = FString(TEXT("ComposeOntoMissingTargetMod.as"));
	inline static const FName GeneratedClassName = FName(TEXT("UComposeOntoProbe"));
	inline static const FName ComposeHostClassName = FName(TEXT("UComposeOntoHost"));
	inline static const FName ComposeProjectedClassName = FName(TEXT("UComposeOntoProjected"));
	inline static const FString MissingComposeTarget = FString(TEXT("UNonexistentComposeHost"));
	inline static const TCHAR* const ExpectedHotReloadError =
		TEXT("An error was encountered during angelscript hot reload. Keeping old angelscript code active.");
	inline static const TCHAR* const ExpectedComposeUnsupportedFragment = TEXT("compose materialization is not implemented yet");

	struct FPreparedAnnotatedModules
	{
		FString AbsoluteFilename;
		TArray<TSharedRef<FAngelscriptModuleDesc>> Modules;
	};

	static FString BuildComposeOntoProbeScript()
	{
		return ASTEST_AS(R"AS(
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
		return ASTEST_AS(R"AS(
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
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.AreEqual(
			1,
			OutPreparedModules.Modules.Num(),
			TEXT("ComposeOntoClass missing-target test should preprocess exactly one module descriptor")))
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

	TEST_METHOD(MissingTargetFailsClosed)
	{
		TestRunner->AddExpectedErrorPlain(
			FAngelscriptComposeOntoClassTests::MissingComposeTarget,
			EAutomationExpectedErrorFlags::Contains,
			1);
		TestRunner->AddExpectedErrorPlain(
			FAngelscriptComposeOntoClassTests::ExpectedHotReloadError,
			EAutomationExpectedErrorFlags::Contains,
			1);

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		Engine.Diagnostics.Empty();
		Engine.LastEmittedDiagnostics.Empty();
		Engine.bDiagnosticsDirty = false;

		FAngelscriptComposeOntoClassTests::FPreparedAnnotatedModules PreparedModules;
		if (!FAngelscriptComposeOntoClassTests::PrepareAnnotatedModulesForGenerator(
			*TestRunner,
			Engine,
			FAngelscriptComposeOntoClassTests::BuildComposeOntoProbeScript(),
			PreparedModules))
		{
			return;
		}

		const FString PreparedModuleName = PreparedModules.Modules[0]->ModuleName;
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*PreparedModuleName);

			if (!PreparedModules.AbsoluteFilename.IsEmpty())
			{
				IFileManager::Get().Delete(*PreparedModules.AbsoluteFilename, false, true, true);
			}
		};

		if (!this->Assert.AreEqual(
			1,
			PreparedModules.Modules[0]->Classes.Num(),
			TEXT("ComposeOntoClass missing-target test should preprocess exactly one class descriptor")))
		{
			return;
		}

		TSharedPtr<FAngelscriptClassDesc> ClassDesc = PreparedModules.Modules[0]->Classes[0];
		if (!this->Assert.IsNotNull(ClassDesc.Get(), TEXT("ComposeOntoClass missing-target test should preprocess a class descriptor")))
		{
			return;
		}

		ClassDesc->ComposeOntoClass = FAngelscriptComposeOntoClassTests::MissingComposeTarget;

		const ECompileResult CompileResult =
			FAngelscriptComposeOntoClassTests::CompilePreparedAnnotatedModules(Engine, PreparedModules.Modules);
		const TArray<FString> ExpectedDiagnosticFragments
		{
			TEXT("ComposeOntoClass"),
			FAngelscriptComposeOntoClassTests::MissingComposeTarget
		};

		ASSERT_THAT(AreEqual(
			ECompileResult::Error,
			CompileResult,
			TEXT("ComposeOntoClass missing-target test should fail compilation instead of silently succeeding")));
		ASSERT_THAT(IsTrue(
			FAngelscriptComposeOntoClassTests::DiagnosticsContainAllFragments(
				Engine,
				PreparedModules.AbsoluteFilename,
				ExpectedDiagnosticFragments),
			TEXT("ComposeOntoClass missing-target test should emit a diagnostic naming the missing compose target")));
		ASSERT_THAT(IsNull(
			FindGeneratedClass(&Engine, FAngelscriptComposeOntoClassTests::GeneratedClassName),
			TEXT("ComposeOntoClass missing-target test should not publish the composed script class")));
		ASSERT_THAT(IsTrue(
			!Engine.GetModuleByModuleName(PreparedModuleName).IsValid(),
			TEXT("ComposeOntoClass missing-target test should not publish a module record after failure")));

	}

	TEST_METHOD(ValidTargetDoesNotSilentlyPublishNoOpClass)
	{
		TestRunner->AddExpectedErrorPlain(
			FAngelscriptComposeOntoClassTests::ExpectedComposeUnsupportedFragment,
			EAutomationExpectedErrorFlags::Contains,
			1);
		TestRunner->AddExpectedErrorPlain(
			FAngelscriptComposeOntoClassTests::ExpectedHotReloadError,
			EAutomationExpectedErrorFlags::Contains,
			1);

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		Engine.Diagnostics.Empty();
		Engine.LastEmittedDiagnostics.Empty();
		Engine.bDiagnosticsDirty = false;

		FAngelscriptComposeOntoClassTests::FPreparedAnnotatedModules PreparedModules;
		if (!FAngelscriptComposeOntoClassTests::PrepareAnnotatedModulesForGenerator(
			*TestRunner,
			Engine,
			FAngelscriptComposeOntoClassTests::BuildComposeOntoValidTargetScript(),
			PreparedModules))
		{
			return;
		}

		const FString PreparedModuleName = PreparedModules.Modules[0]->ModuleName;
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*PreparedModuleName);

			if (!PreparedModules.AbsoluteFilename.IsEmpty())
			{
				IFileManager::Get().Delete(*PreparedModules.AbsoluteFilename, false, true, true);
			}
		};

		if (!this->Assert.AreEqual(
			2,
			PreparedModules.Modules[0]->Classes.Num(),
			TEXT("ComposeOntoClass valid-target test should preprocess exactly two class descriptors")))
		{
			return;
		}

		TSharedPtr<FAngelscriptClassDesc> HostClassDesc;
		TSharedPtr<FAngelscriptClassDesc> ProjectedClassDesc;
		for (const TSharedRef<FAngelscriptClassDesc>& ClassDesc : PreparedModules.Modules[0]->Classes)
		{
			if (ClassDesc->ClassName == FAngelscriptComposeOntoClassTests::ComposeHostClassName.ToString())
			{
				HostClassDesc = ClassDesc;
			}
			else if (ClassDesc->ClassName == FAngelscriptComposeOntoClassTests::ComposeProjectedClassName.ToString())
			{
				ProjectedClassDesc = ClassDesc;
			}
		}

		if (!this->Assert.IsNotNull(HostClassDesc.Get(), TEXT("ComposeOntoClass valid-target test should preprocess the compose host descriptor"))
			|| !this->Assert.IsNotNull(ProjectedClassDesc.Get(), TEXT("ComposeOntoClass valid-target test should preprocess the projected descriptor")))
		{
			return;
		}

		ProjectedClassDesc->ComposeOntoClass = FAngelscriptComposeOntoClassTests::ComposeHostClassName.ToString();

		const ECompileResult CompileResult =
			FAngelscriptComposeOntoClassTests::CompilePreparedAnnotatedModules(Engine, PreparedModules.Modules);
		const TArray<FString> ExpectedDiagnosticFragments
		{
			TEXT("ComposeOntoClass"),
			FAngelscriptComposeOntoClassTests::ComposeHostClassName.ToString(),
			FAngelscriptComposeOntoClassTests::ExpectedComposeUnsupportedFragment
		};

		ASSERT_THAT(AreEqual(
			ECompileResult::Error,
			CompileResult,
			TEXT("ComposeOntoClass valid-target test should fail compilation instead of silently publishing a no-op composed class")));
		ASSERT_THAT(IsTrue(
			FAngelscriptComposeOntoClassTests::DiagnosticsContainAllFragments(
				Engine,
				PreparedModules.AbsoluteFilename,
				ExpectedDiagnosticFragments),
			TEXT("ComposeOntoClass valid-target test should emit an unsupported-yet diagnostic for the real compose target")));
		ASSERT_THAT(IsNull(
			FindGeneratedClass(&Engine, FAngelscriptComposeOntoClassTests::ComposeProjectedClassName),
			TEXT("ComposeOntoClass valid-target test should not publish the projected compose class while compose materialization is unsupported")));
		ASSERT_THAT(IsTrue(
			!Engine.GetModuleByModuleName(PreparedModuleName).IsValid(),
			TEXT("ComposeOntoClass valid-target test should not publish a module record after the unsupported compose path")));

	}
};

#endif
