#pragma once

// ============================================================================
// AngelscriptTestModuleBuilder
// ============================================================================
//
// Themed sub-header split out of `AngelscriptTestUtilities.h` (Phase 1 of
// OpenSpec change `refactor-as-test-shared-layout-and-naming`).
//
// Responsibility:
//   - In-memory module compilation pipeline:
//       * `ReportCompileDiagnostics` — surface compiler diagnostics to the
//         automation test runner.
//       * `FScopedAutomaticImportsOverride` — disable `asEP_AUTOMATIC_IMPORTS`
//         for the lifetime of a build to keep test sources isolated.
//       * `BuildModule` — write a single `.as` script under
//         `Saved/Automation/`, preprocess, compile via `FAngelscriptEngine`,
//         and return the resulting `asIScriptModule*`.
//       * `GetFunctionByDecl` — declaration lookup with name fallback and
//         diagnostic "available functions" listing.
//
// Original location: AngelscriptTestUtilities.h lines 693-871.
// ============================================================================

#include "AngelscriptEngine.h"
#include "Containers/StringConv.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Preprocessor/AngelscriptPreprocessor.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

namespace AngelscriptTestSupport
{
	inline void ReportCompileDiagnostics(FAutomationTestBase& Test, const FAngelscriptEngine& Engine, const FString& AbsoluteFilename)
	{
		const FAngelscriptEngine::FDiagnostics* Diagnostics = Engine.Diagnostics.Find(AbsoluteFilename);
		if (Diagnostics == nullptr || Diagnostics->Diagnostics.IsEmpty())
		{
			return;
		}

		for (const FAngelscriptEngine::FDiagnostic& Diagnostic : Diagnostics->Diagnostics)
		{
			if (!Diagnostic.bIsError)
			{
				continue;
			}

			Test.AddError(FString::Printf(
				TEXT("%s:%d:%d: %s"),
				*AbsoluteFilename,
				Diagnostic.Row,
				Diagnostic.Column,
				*Diagnostic.Message));
		}
	}

	struct FScopedAutomaticImportsOverride
	{
		explicit FScopedAutomaticImportsOverride(asIScriptEngine* InScriptEngine)
			: ScriptEngine(InScriptEngine)
			, PreviousValue(InScriptEngine != nullptr ? InScriptEngine->GetEngineProperty(asEP_AUTOMATIC_IMPORTS) : 0)
		{
			if (ScriptEngine != nullptr)
			{
				ScriptEngine->SetEngineProperty(asEP_AUTOMATIC_IMPORTS, 0);
			}
		}

		~FScopedAutomaticImportsOverride()
		{
			if (ScriptEngine != nullptr)
			{
				ScriptEngine->SetEngineProperty(asEP_AUTOMATIC_IMPORTS, PreviousValue);
			}
		}

		asIScriptEngine* ScriptEngine = nullptr;
		asPWORD PreviousValue = 0;
	};

	inline asIScriptModule* BuildModule(FAutomationTestBase& Test, FAngelscriptEngine& Engine, const char* ModuleName, const FString& Source)
	{
		const FString RequestedModuleName = ANSI_TO_TCHAR(ModuleName);
		const FString UniqueFilename = FString::Printf(TEXT("%s_%s.as"), *RequestedModuleName, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
		const FString RelativeFilename = FString::Printf(TEXT("%s.as"), *RequestedModuleName);
		const FString AbsoluteFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), UniqueFilename);
		const FString AutomationDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"));

		IFileManager::Get().MakeDirectory(*FPaths::GetPath(AbsoluteFilename), true);
		TArray<FString> ExistingAutomationFiles;
		IFileManager::Get().FindFiles(ExistingAutomationFiles, *(AutomationDirectory / (RequestedModuleName + TEXT("*.as"))), true, false);
		for (const FString& ExistingFilename : ExistingAutomationFiles)
		{
			IFileManager::Get().Delete(*(AutomationDirectory / ExistingFilename), false, true, true);
		}

		if (!FFileHelper::SaveStringToFile(Source, *AbsoluteFilename))
		{
			Test.AddError(FString::Printf(TEXT("Failed to write script module '%s' to '%s'"), *RequestedModuleName, *AbsoluteFilename));
			return nullptr;
		}

		FAngelscriptEngineScope EngineScope(Engine);

		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddFile(RelativeFilename, AbsoluteFilename);
		if (!Preprocessor.Preprocess())
		{
			ReportCompileDiagnostics(Test, Engine, AbsoluteFilename);
			Test.AddError(FString::Printf(TEXT("Failed to preprocess script module '%s'"), *RequestedModuleName));
			return nullptr;
		}

		TArray<TSharedRef<FAngelscriptModuleDesc>> ModulesToCompile = Preprocessor.GetModulesToCompile();

		TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
		TGuardValue<bool> AutomaticImportGuard(Engine.bUseAutomaticImportMethod, false);
		FScopedAutomaticImportsOverride AutomaticImportsOverride(Engine.GetScriptEngine());
		const ECompileResult CompileResult = Engine.CompileModules(ECompileType::Initial, ModulesToCompile, CompiledModules);
		if (CompileResult == ECompileResult::Error || CompileResult == ECompileResult::ErrorNeedFullReload)
		{
			ReportCompileDiagnostics(Test, Engine, AbsoluteFilename);
			Test.AddError(FString::Printf(TEXT("Failed to compile script module '%s'"), *RequestedModuleName));
			return nullptr;
		}

		if (!Test.TestTrue(TEXT("Exactly one Angelscript test module should compile"), CompiledModules.Num() == 1))
		{
			return nullptr;
		}

		asIScriptModule* Module = CompiledModules[0]->ScriptModule;
		const FString ModuleContext = FString::Printf(TEXT("Compiled script module '%s' should have a backing asIScriptModule"), *RequestedModuleName);
		if (!Test.TestNotNull(*ModuleContext, Module))
		{
			return nullptr;
		}

		return Module;
	}

	inline asIScriptFunction* GetFunctionByDecl(FAutomationTestBase& Test, asIScriptModule& Module, const FString& Declaration)
	{
		FString FunctionName;
		FTCHARToUTF8 DeclarationUtf8(*Declaration);
		asIScriptFunction* Function = Module.GetFunctionByDecl(DeclarationUtf8.Get());
		if (Function == nullptr)
		{
			int32 OpenParenIndex = INDEX_NONE;
			if (Declaration.FindChar(TEXT('('), OpenParenIndex))
			{
				const FString Prefix = Declaration.Left(OpenParenIndex).TrimStartAndEnd();
				int32 NameSeparatorIndex = INDEX_NONE;
				if (Prefix.FindLastChar(TEXT(' '), NameSeparatorIndex))
				{
					FunctionName = Prefix.Mid(NameSeparatorIndex + 1).TrimStartAndEnd();
					if (!FunctionName.IsEmpty())
					{
						FTCHARToUTF8 FunctionNameUtf8(*FunctionName);
						Function = Module.GetFunctionByName(FunctionNameUtf8.Get());
					}
				}
			}
		}

		if (Function == nullptr && !FunctionName.IsEmpty())
		{
			const asUINT FunctionCount = Module.GetFunctionCount();
			for (asUINT FunctionIndex = 0; FunctionIndex < FunctionCount; ++FunctionIndex)
			{
				asIScriptFunction* CandidateFunction = Module.GetFunctionByIndex(FunctionIndex);
				if (CandidateFunction != nullptr && FunctionName.Equals(UTF8_TO_TCHAR(CandidateFunction->GetName())))
				{
					Function = CandidateFunction;
					break;
				}
			}
		}

		if (Function == nullptr)
		{
			FString AvailableFunctions;
			const asUINT FunctionCount = Module.GetFunctionCount();
			for (asUINT FunctionIndex = 0; FunctionIndex < FunctionCount; ++FunctionIndex)
			{
				asIScriptFunction* CandidateFunction = Module.GetFunctionByIndex(FunctionIndex);
				if (CandidateFunction == nullptr)
				{
					continue;
				}

				if (!AvailableFunctions.IsEmpty())
				{
					AvailableFunctions += TEXT(", ");
				}

				AvailableFunctions += UTF8_TO_TCHAR(CandidateFunction->GetDeclaration());
			}

			if (AvailableFunctions.IsEmpty())
			{
				Test.AddError(FString::Printf(TEXT("Failed to find function declaration '%s'; module exposes no global functions"), *Declaration));
			}
			else
			{
				Test.AddError(FString::Printf(TEXT("Failed to find function declaration '%s'; available functions: %s"), *Declaration, *AvailableFunctions));
			}
		}

		return Function;
	}
}
