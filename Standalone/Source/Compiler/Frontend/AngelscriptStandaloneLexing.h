#pragma once

#include "Compiler/Frontend/AngelscriptStandaloneSource.h"

#include <string>
#include <string_view>
#include <vector>

namespace AngelscriptStandalone::Frontend
{
	enum class ELexicalRangeKind
	{
		Code,
		String,
		LineComment,
		BlockComment,
	};

	struct FLexicalRange
	{
		ELexicalRangeKind Kind = ELexicalRangeKind::Code;
		FSourceSpan Span;
		bool bTerminated = true;
	};

	struct FDelimiterMatch
	{
		bool bSuccess = false;
		FByteOffset CloseOffset = 0;
		std::string Error;
	};

	std::vector<FLexicalRange> ScanLexicalRanges(
		std::string_view Text);
	std::string BlankCommentsPreservingLayout(
		std::string_view Text);
	FDelimiterMatch FindMatchingDelimiter(
		std::string_view Text,
		FByteOffset OpenOffset,
		char OpenDelimiter,
		char CloseDelimiter);
}
