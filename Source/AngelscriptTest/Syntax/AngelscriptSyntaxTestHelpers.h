#pragma once

#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptBindingsModuleBuilder.h"
#include "Shared/AngelscriptBindingsAssertions.h"
#include "Misc/Paths.h"

// ============================================================================
// Angelscript Syntax Test Helpers (CQTest Edition)
// ============================================================================
//
// Assertion wrappers for syntax coverage tests using CQTest framework.
// These tests validate compile success/failure of various syntax forms.
//
// CQTest usage pattern:
//   TEST_METHOD(SomeName)
//   {
//       FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
//       FAngelscriptEngineScope Scope(Engine);
//
//       // Positive: use FCoverageModuleScope + ExpectGlobalInts
//       // Negative: use SyntaxTestHelpers::AssertFailsWithError(*TestRunner, ...)
//   }
//
// Key difference from old pattern:
//   - Pass *TestRunner (not *this) as FAutomationTestBase&
//   - Use FAngelscriptEngineScope directly (no ASTEST_BEGIN/END macros)
//   - Negative tests auto-report source code and error diagnostics
// ============================================================================

#if WITH_DEV_AUTOMATION_TESTS

namespace SyntaxTestHelpers
{
	using namespace AngelscriptTestSupport;

	// Returns a fake .as filename for diagnostic isolation.
	inline FString MakeSyntaxFilename(const TCHAR* SectionName)
	{
		return FString::Printf(TEXT("%s.as"), SectionName);
	}

	// -------------------------------------------------------------------------
	// ReportCompileDiagnostics — prints source code and all diagnostics
	// via Test.AddInfo(). Called automatically by negative test helpers.
	// -------------------------------------------------------------------------
	inline void ReportCompileDiagnostics(
		FAutomationTestBase& Test,
		const TCHAR* Description,
		const TCHAR* Source,
		const FAngelscriptCompileTraceSummary& Summary)
	{
		Test.AddInfo(FString::Printf(TEXT("[Syntax] %s"), Description));
		Test.AddInfo(FString::Printf(TEXT("  Source: %s"), Source));
		Test.AddInfo(FString::Printf(TEXT("  CompileResult: %s (Diagnostics: %d)"),
			Summary.bCompileSucceeded ? TEXT("Success") : TEXT("Failed"),
			Summary.Diagnostics.Num()));

		for (int32 I = 0; I < Summary.Diagnostics.Num(); ++I)
		{
			const FAngelscriptCompileTraceDiagnosticSummary& D = Summary.Diagnostics[I];
			const TCHAR* Severity = D.bIsError ? TEXT("ERROR") : (D.bIsInfo ? TEXT("INFO") : TEXT("WARN"));
			Test.AddInfo(FString::Printf(TEXT("  [%d] %s @Row%d:Col%d - %s"),
				I + 1,
				Severity,
				D.Row, D.Column,
				*D.Message));
		}
	}

	// -------------------------------------------------------------------------
	// Positive: Verify source compiles successfully.
	// -------------------------------------------------------------------------
	inline bool AssertCompiles(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const TCHAR* SectionName,
		const TCHAR* Source,
		const TCHAR* Description)
	{
		FAngelscriptCompileTraceSummary Summary;
		const FString Filename = MakeSyntaxFilename(SectionName);
		CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			FName(SectionName),
			Filename,
			FString(Source),
			true,
			Summary,
			false);

		const bool bPassed = Test.TestTrue(
			FString::Printf(TEXT("[Positive] %s — should compile"), Description),
			Summary.bCompileSucceeded);

		if (!bPassed)
		{
			ReportCompileDiagnostics(Test, Description, Source, Summary);
		}

		return bPassed;
	}

	// -------------------------------------------------------------------------
	// Negative: Verify source fails to compile AND contains expected error.
	// Always reports source code and diagnostics via AddInfo().
	// -------------------------------------------------------------------------
	inline bool AssertFailsWithError(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const TCHAR* SectionName,
		const TCHAR* Source,
		const TCHAR* ExpectedErrorFragment,
		const TCHAR* Description)
	{
		FAngelscriptCompileTraceSummary Summary;
		const FString Filename = MakeSyntaxFilename(SectionName);
		CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			FName(SectionName),
			Filename,
			FString(Source),
			true,
			Summary,
			true /* bSuppressCompileErrorLogs */);

		ReportCompileDiagnostics(Test, Description, Source, Summary);

		bool bResult = true;

		bResult &= Test.TestFalse(
			FString::Printf(TEXT("[Negative] %s — should NOT compile"), Description),
			Summary.bCompileSucceeded);

		bResult &= Test.TestEqual(
			FString::Printf(TEXT("[Negative] %s — CompileResult should be Error"), Description),
			Summary.CompileResult,
			ECompileResult::Error);

		// Search diagnostics for expected error fragment.
		const FAngelscriptCompileTraceDiagnosticSummary* FoundDiag = nullptr;
		for (const FAngelscriptCompileTraceDiagnosticSummary& Diag : Summary.Diagnostics)
		{
			if (Diag.bIsError && Diag.Message.Contains(ExpectedErrorFragment))
			{
				FoundDiag = &Diag;
				break;
			}
		}

		bResult &= Test.TestNotNull(
			FString::Printf(TEXT("[Negative] %s — expected error containing: '%s'"), Description, ExpectedErrorFragment),
			FoundDiag);

		if (FoundDiag != nullptr)
		{
			bResult &= Test.TestTrue(
				FString::Printf(TEXT("[Negative] %s — diagnostic row > 0"), Description),
				FoundDiag->Row > 0);
		}

		return bResult;
	}

	// -------------------------------------------------------------------------
	// Negative (relaxed): Verify source fails to compile, report diagnostics,
	// but do not check for a specific error message.
	// -------------------------------------------------------------------------
	inline bool AssertFailsToCompile(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const TCHAR* SectionName,
		const TCHAR* Source,
		const TCHAR* Description)
	{
		FAngelscriptCompileTraceSummary Summary;
		const FString Filename = MakeSyntaxFilename(SectionName);
		CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			FName(SectionName),
			Filename,
			FString(Source),
			true,
			Summary,
			true /* bSuppressCompileErrorLogs */);

		ReportCompileDiagnostics(Test, Description, Source, Summary);

		bool bResult = Test.TestFalse(
			FString::Printf(TEXT("[Negative] %s — should NOT compile"), Description),
			Summary.bCompileSucceeded);

		return bResult;
	}

	// -------------------------------------------------------------------------
	// Warning: Verify source compiles but emits an expected warning.
	// Reports diagnostics on failure.
	// -------------------------------------------------------------------------
	inline bool AssertCompilesWithWarning(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const TCHAR* SectionName,
		const TCHAR* Source,
		const TCHAR* ExpectedWarningFragment,
		const TCHAR* Description)
	{
		FAngelscriptCompileTraceSummary Summary;
		const FString Filename = MakeSyntaxFilename(SectionName);
		CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			FName(SectionName),
			Filename,
			FString(Source),
			true,
			Summary,
			false);

		bool bResult = Test.TestTrue(
			FString::Printf(TEXT("[Warning] %s — should compile successfully"), Description),
			Summary.bCompileSucceeded);

		// Search diagnostics for expected warning fragment.
		const FAngelscriptCompileTraceDiagnosticSummary* FoundDiag = nullptr;
		for (const FAngelscriptCompileTraceDiagnosticSummary& Diag : Summary.Diagnostics)
		{
			if (!Diag.bIsError && !Diag.bIsInfo && Diag.Message.Contains(ExpectedWarningFragment))
			{
				FoundDiag = &Diag;
				break;
			}
		}

		bResult &= Test.TestNotNull(
			FString::Printf(TEXT("[Warning] %s — expected warning: '%s'"), Description, ExpectedWarningFragment),
			FoundDiag);

		if (!bResult)
		{
			ReportCompileDiagnostics(Test, Description, Source, Summary);
		}

		return bResult;
	}

	// -------------------------------------------------------------------------
	// Positive with preprocessor: Verify source compiles with preprocessor.
	// -------------------------------------------------------------------------
	inline bool AssertCompilesWithPreprocessor(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const TCHAR* SectionName,
		const TCHAR* Source,
		const TCHAR* Description)
	{
		FAngelscriptCompileTraceSummary Summary;
		const FString Filename = MakeSyntaxFilename(SectionName);
		CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			FName(SectionName),
			Filename,
			FString(Source),
			true,
			Summary,
			false);

		const bool bPassed = Test.TestTrue(
			FString::Printf(TEXT("[Positive+PP] %s — should compile with preprocessor"), Description),
			Summary.bCompileSucceeded);

		if (!bPassed)
		{
			ReportCompileDiagnostics(Test, Description, Source, Summary);
		}

		return bPassed;
	}

} // namespace SyntaxTestHelpers

#endif // WITH_DEV_AUTOMATION_TESTS
