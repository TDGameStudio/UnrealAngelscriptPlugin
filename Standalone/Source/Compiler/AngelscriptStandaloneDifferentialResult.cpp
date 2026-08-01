#include "Compiler/AngelscriptStandaloneDifferentialResult.h"

#include "Support/AngelscriptStandaloneJson.h"

#include <algorithm>
#include <sstream>
#include <tuple>

namespace AngelscriptStandalone
{
	namespace
	{
		auto DiagnosticKey(const FDifferentialDiagnostic& Value)
		{
			return std::tie(
				Value.LogicalPath,
				Value.Begin,
				Value.End,
				Value.Category,
				Value.Code,
				Value.Severity);
		}

		auto ResourceKey(const FDifferentialResource& Value)
		{
			return std::tie(
				Value.ContextId,
				Value.State,
				Value.NormalizedPath,
				Value.FinalPath,
				Value.RequestedStableTypeId,
				Value.ResolvedStableTypeId,
				Value.DiagnosticId);
		}

		template <typename Value>
		void SortUnique(std::vector<Value>& Values)
		{
			std::sort(Values.begin(), Values.end());
			Values.erase(
				std::unique(Values.begin(), Values.end()),
				Values.end());
		}

		std::string DiagnosticJson(
			const FDifferentialDiagnostic& Value)
		{
			std::ostringstream Json;
			Json
				<< "{\"begin\":" << Value.Begin
				<< ",\"category\":" << EscapeJsonString(Value.Category)
				<< ",\"code\":" << EscapeJsonString(Value.Code)
				<< ",\"end\":" << Value.End
				<< ",\"logicalPath\":"
				<< EscapeJsonString(Value.LogicalPath)
				<< ",\"severity\":" << EscapeJsonString(Value.Severity)
				<< '}';
			return Json.str();
		}

		std::string ResourceJson(
			const FDifferentialResource& Value)
		{
			std::ostringstream Json;
			Json
				<< "{\"contextId\":"
				<< EscapeJsonString(Value.ContextId)
				<< ",\"diagnosticId\":"
				<< EscapeJsonString(Value.DiagnosticId)
				<< ",\"finalPath\":"
				<< EscapeJsonString(Value.FinalPath)
				<< ",\"normalizedPath\":"
				<< EscapeJsonString(Value.NormalizedPath)
				<< ",\"originalPath\":"
				<< EscapeJsonString(Value.OriginalPath)
				<< ",\"requestedStableTypeId\":"
				<< EscapeJsonString(Value.RequestedStableTypeId)
				<< ",\"resolvedStableTypeId\":"
				<< EscapeJsonString(Value.ResolvedStableTypeId)
				<< ",\"state\":" << EscapeJsonString(Value.State)
				<< '}';
			return Json.str();
		}
	}

	FDifferentialResult NormalizeDifferentialResult(
		FDifferentialResult Result)
	{
		std::sort(
			Result.Diagnostics.begin(),
			Result.Diagnostics.end(),
			[](const auto& Left, const auto& Right)
			{
				return DiagnosticKey(Left) < DiagnosticKey(Right);
			});
		Result.Diagnostics.erase(
			std::unique(
				Result.Diagnostics.begin(),
				Result.Diagnostics.end()),
			Result.Diagnostics.end());
		SortUnique(Result.ResolvedStableSymbolIds);
		SortUnique(Result.PortableClassRecords);
		std::sort(
			Result.Resources.begin(),
			Result.Resources.end(),
			[](const auto& Left, const auto& Right)
			{
				return ResourceKey(Left) < ResourceKey(Right);
			});
		Result.Resources.erase(
			std::unique(
				Result.Resources.begin(),
				Result.Resources.end()),
			Result.Resources.end());
		return Result;
	}

	FDifferentialComparison CompareDifferentialResults(
		const FDifferentialResult& ExpectedInput,
		const FDifferentialResult& ActualInput)
	{
		const FDifferentialResult Expected =
			NormalizeDifferentialResult(ExpectedInput);
		const FDifferentialResult Actual =
			NormalizeDifferentialResult(ActualInput);
		if (Expected.Schema != Actual.Schema)
			return {false, "schema differs"};
		if (Expected.CaseId != Actual.CaseId)
			return {false, "case ID differs"};
		if (Expected.Profile != Actual.Profile)
			return {false, "profile differs"};
		if (Expected.CompileStatus != Actual.CompileStatus)
			return {false, "compile status differs"};
		if (Expected.Classification != Actual.Classification)
			return {false, "classification differs"};
		if (Expected.Diagnostics != Actual.Diagnostics)
			return {false, "normalized diagnostics differ"};
		if (Expected.ResolvedStableSymbolIds
			!= Actual.ResolvedStableSymbolIds)
			return {false, "resolved stable symbols differ"};
		if (Expected.PortableClassRecords
			!= Actual.PortableClassRecords)
			return {false, "portable class records differ"};
		if (Expected.Resources != Actual.Resources)
			return {false, "resource records differ"};
		if (Expected.bByteCodeCompleted
			!= Actual.bByteCodeCompleted)
			return {false, "bytecode completion differs"};
		if (Expected.ScriptResult != Actual.ScriptResult)
			return {false, "script result differs"};
		return {true, {}};
	}

	std::string SerializeDifferentialResult(
		const FDifferentialResult& Input)
	{
		const FDifferentialResult Result =
			NormalizeDifferentialResult(Input);
		std::ostringstream Json;
		Json
			<< "{\"bytecodeCompleted\":"
			<< (Result.bByteCodeCompleted ? "true" : "false")
			<< ",\"caseId\":" << EscapeJsonString(Result.CaseId)
			<< ",\"classification\":"
			<< EscapeJsonString(Result.Classification)
			<< ",\"compileStatus\":"
			<< EscapeJsonString(Result.CompileStatus)
			<< ",\"diagnostics\":[";
		for (std::size_t Index = 0;
			Index < Result.Diagnostics.size();
			++Index)
		{
			if (Index != 0)
				Json << ',';
			Json << DiagnosticJson(Result.Diagnostics[Index]);
		}
		Json << "],\"host\":" << EscapeJsonString(Result.Host)
			<< ",\"portableClasses\":[";
		for (std::size_t Index = 0;
			Index < Result.PortableClassRecords.size();
			++Index)
		{
			if (Index != 0)
				Json << ',';
			Json << EscapeJsonString(
				Result.PortableClassRecords[Index]);
		}
		Json << "],\"profile\":" << EscapeJsonString(Result.Profile)
			<< ",\"resolvedStableSymbols\":[";
		for (std::size_t Index = 0;
			Index < Result.ResolvedStableSymbolIds.size();
			++Index)
		{
			if (Index != 0)
				Json << ',';
			Json << EscapeJsonString(
				Result.ResolvedStableSymbolIds[Index]);
		}
		Json << "],\"resources\":[";
		for (std::size_t Index = 0;
			Index < Result.Resources.size();
			++Index)
		{
			if (Index != 0)
				Json << ',';
			Json << ResourceJson(Result.Resources[Index]);
		}
		Json << "],\"schema\":" << EscapeJsonString(Result.Schema)
			<< ",\"scriptResult\":";
		if (Result.ScriptResult.has_value())
			Json << *Result.ScriptResult;
		else
			Json << "null";
		Json << '}';
		return Json.str();
	}
}
