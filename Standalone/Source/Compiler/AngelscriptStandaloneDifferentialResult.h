#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace AngelscriptStandalone
{
	struct FDifferentialDiagnostic
	{
		std::string Code;
		std::string Category;
		std::string LogicalPath;
		std::size_t Begin = 0;
		std::size_t End = 0;
		std::string Severity;

		bool operator==(const FDifferentialDiagnostic&) const = default;
	};

	struct FDifferentialResource
	{
		std::string ContextId;
		std::string State;
		std::string OriginalPath;
		std::string NormalizedPath;
		std::string FinalPath;
		std::string RequestedStableTypeId;
		std::string ResolvedStableTypeId;
		std::string DiagnosticId;

		bool operator==(const FDifferentialResource&) const = default;
	};

	// Host-normalized evidence only. Runtime IDs, addresses, bytecode bytes,
	// prose, elapsed time, and machine paths intentionally have no field.
	struct FDifferentialResult
	{
		std::string Schema =
			"angelscript-standalone-differential-result/1.0";
		std::string CaseId;
		std::string Host;
		std::string Profile;
		std::string CompileStatus;
		std::string Classification;
		std::vector<FDifferentialDiagnostic> Diagnostics;
		std::vector<std::string> ResolvedStableSymbolIds;
		std::vector<std::string> PortableClassRecords;
		std::vector<FDifferentialResource> Resources;
		bool bByteCodeCompleted = false;
		std::optional<int> ScriptResult;
	};

	struct FDifferentialComparison
	{
		bool bEquivalent = false;
		std::string Difference;
	};

	FDifferentialResult NormalizeDifferentialResult(
		FDifferentialResult Result);

	FDifferentialComparison CompareDifferentialResults(
		const FDifferentialResult& Expected,
		const FDifferentialResult& Actual);

	std::string SerializeDifferentialResult(
		const FDifferentialResult& Result);
}
