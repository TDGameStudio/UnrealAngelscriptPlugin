#include "Compiler/AngelscriptStandaloneFrontend.h"

#include "Compiler/Frontend/AngelscriptStandaloneFrontendSession.h"

namespace AngelscriptStandalone
{
	namespace
	{
		EDiagnosticSeverity ConvertSeverity(
			Frontend::EDiagnosticSeverity Severity)
		{
			switch (Severity)
			{
			case Frontend::EDiagnosticSeverity::Info:
				return EDiagnosticSeverity::Info;
			case Frontend::EDiagnosticSeverity::Warning:
				return EDiagnosticSeverity::Warning;
			case Frontend::EDiagnosticSeverity::Error:
			default:
				return EDiagnosticSeverity::Error;
			}
		}
	}

	FFrontendResult ProcessNativeFrontend(const FResolvedSource& Source)
	{
		using namespace Frontend;

		FPreprocessConfig Config;
		Config.Flags.emplace("EDITOR", false);
		Config.Flags.emplace("EDITORONLY_DATA", false);
		Config.Flags.emplace("COOK_COMMANDLET", false);
		Config.Flags.emplace("RELEASE", false);
		Config.Flags.emplace("TEST", true);
		Config.Flags.emplace("WITH_SERVER_CODE", false);

		const FPreprocessResult LanguageResult = PreprocessSource(
			{Source.LogicalPath, Source.Contents},
			Config);

		FFrontendResult Result;
		Result.bSuccess = LanguageResult.bSuccess;
		Result.LogicalPath = LanguageResult.LogicalPath;
		Result.ModuleName = LanguageResult.ModuleName;
		Result.ModuleId = LanguageResult.ModuleId;
		Result.ProcessedSource = LanguageResult.ProcessedSource;

		const FSourceText SourceText(Source.Contents);
		for (const AngelscriptStandalone::Frontend::FDiagnostic& LanguageDiagnostic
			: LanguageResult.Diagnostics)
		{
			const FSourceLocation Location =
				SourceText.GetLocation(LanguageDiagnostic.Span.Begin);
			AngelscriptStandalone::FDiagnostic& Diagnostic =
				Result.Diagnostics.emplace_back();
			Diagnostic.Code = LanguageDiagnostic.Code;
			Diagnostic.Section = LanguageDiagnostic.LogicalPath;
			Diagnostic.Row = static_cast<int>(Location.Line);
			Diagnostic.Column = static_cast<int>(Location.Column);
			Diagnostic.Severity = ConvertSeverity(LanguageDiagnostic.Severity);
			Diagnostic.Message = LanguageDiagnostic.Message;
		}
		return Result;
	}
}
