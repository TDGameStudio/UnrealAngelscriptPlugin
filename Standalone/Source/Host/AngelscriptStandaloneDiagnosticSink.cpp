#include "Host/AngelscriptStandaloneDiagnosticSink.h"

#include <utility>

namespace AngelscriptStandalone
{
	void FDiagnosticSink::MessageCallback(const asSMessageInfo* Message, void* UserData)
	{
		if (Message == nullptr || UserData == nullptr)
		{
			return;
		}

		FDiagnostic Diagnostic;
		Diagnostic.Code = "AS-COMPILER";
		Diagnostic.Section = Message->section != nullptr ? Message->section : "";
		Diagnostic.Row = Message->row;
		Diagnostic.Column = Message->col;
		Diagnostic.Message = Message->message != nullptr ? Message->message : "";
		switch (Message->type)
		{
		case asMSGTYPE_ERROR:
			Diagnostic.Severity = EDiagnosticSeverity::Error;
			break;
		case asMSGTYPE_WARNING:
			Diagnostic.Severity = EDiagnosticSeverity::Warning;
			break;
		default:
			Diagnostic.Severity = EDiagnosticSeverity::Info;
			break;
		}
		static_cast<FDiagnosticSink*>(UserData)->Add(std::move(Diagnostic));
	}

	void FDiagnosticSink::Add(FDiagnostic Diagnostic)
	{
		Diagnostics.push_back(std::move(Diagnostic));
	}

	const std::vector<FDiagnostic>& FDiagnosticSink::GetDiagnostics() const
	{
		return Diagnostics;
	}

	int FDiagnosticSink::Count(EDiagnosticSeverity Severity) const
	{
		int Result = 0;
		for (const FDiagnostic& Diagnostic : Diagnostics)
		{
			Result += Diagnostic.Severity == Severity ? 1 : 0;
		}
		return Result;
	}

	const char* ToString(EDiagnosticSeverity Severity)
	{
		switch (Severity)
		{
		case EDiagnosticSeverity::Error:
			return "error";
		case EDiagnosticSeverity::Warning:
			return "warning";
		default:
			return "info";
		}
	}
}
