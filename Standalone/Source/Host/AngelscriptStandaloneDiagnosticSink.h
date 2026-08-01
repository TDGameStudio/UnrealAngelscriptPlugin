#pragma once

#include "angelscript.h"

#include <map>
#include <string>
#include <vector>

namespace AngelscriptStandalone
{
	enum class EDiagnosticSeverity
	{
		Info,
		Warning,
		Error,
	};

	struct FDiagnostic
	{
		std::string Code;
		std::string Section;
		int Row = 0;
		int Column = 0;
		EDiagnosticSeverity Severity = EDiagnosticSeverity::Info;
		std::string Message;
		std::map<std::string, std::string> Evidence;
	};

	class FDiagnosticSink
	{
	public:
		static void MessageCallback(const asSMessageInfo* Message, void* UserData);

		void Add(FDiagnostic Diagnostic);
		const std::vector<FDiagnostic>& GetDiagnostics() const;
		int Count(EDiagnosticSeverity Severity) const;

	private:
		std::vector<FDiagnostic> Diagnostics;
	};

	const char* ToString(EDiagnosticSeverity Severity);
}
