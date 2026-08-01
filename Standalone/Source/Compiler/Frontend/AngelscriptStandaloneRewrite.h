#pragma once

#include "Compiler/Frontend/AngelscriptStandaloneSource.h"

#include <string>
#include <string_view>
#include <vector>

namespace AngelscriptStandalone::Frontend
{
	struct FRewriteEdit
	{
		FByteOffset Begin = 0;
		FByteOffset End = 0;
		std::string Replacement;
	};

	struct FRewriteOperationResult
	{
		bool bSuccess = false;
		std::string Error;
	};

	struct FSourceMapSegment
	{
		FSourceSpan Processed;
		FSourceSpan Original;
	};

	class FSourceMap
	{
	public:
		FByteOffset MapProcessedToOriginal(FByteOffset ProcessedOffset) const;

	private:
		friend class FRewritePlan;
		std::vector<FSourceMapSegment> Segments;
	};

	struct FRewriteResult
	{
		bool bSuccess = false;
		std::string Text;
		std::string Error;
		FSourceMap SourceMap;
	};

	class FRewritePlan
	{
	public:
		FRewriteOperationResult Add(FRewriteEdit Edit);
		FRewriteResult Apply(std::string_view Source) const;
		const std::vector<FRewriteEdit>& GetEdits() const;

	private:
		std::vector<FRewriteEdit> Edits;
	};

	FRewriteResult RewriteRangeBasedFor(
		std::string_view Source);

	FRewriteResult RewriteNameLiterals(
		std::string_view Source);
}
