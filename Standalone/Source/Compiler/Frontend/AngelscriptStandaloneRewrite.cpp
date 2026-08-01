#include "Compiler/Frontend/AngelscriptStandaloneRewrite.h"

#include "Compiler/Frontend/AngelscriptStandaloneLexing.h"

#include <algorithm>
#include <cctype>

namespace AngelscriptStandalone::Frontend
{
	FByteOffset FSourceMap::MapProcessedToOriginal(FByteOffset ProcessedOffset) const
	{
		for (const FSourceMapSegment& Segment : Segments)
		{
			if (ProcessedOffset >= Segment.Processed.Begin
				&& ProcessedOffset <= Segment.Processed.End)
			{
				const FByteOffset ProcessedLength =
					Segment.Processed.End - Segment.Processed.Begin;
				const FByteOffset OriginalLength =
					Segment.Original.End - Segment.Original.Begin;
				if (ProcessedLength == 0)
				{
					return Segment.Original.Begin;
				}
				const FByteOffset Relative = ProcessedOffset - Segment.Processed.Begin;
				return Segment.Original.Begin
					+ std::min(Relative, OriginalLength);
			}
		}
		return ProcessedOffset;
	}

	FRewriteOperationResult FRewritePlan::Add(FRewriteEdit Edit)
	{
		if (Edit.End < Edit.Begin)
		{
			return {false, "rewrite end precedes its beginning"};
		}
		auto Position = std::lower_bound(
			Edits.begin(),
			Edits.end(),
			Edit.Begin,
			[](const FRewriteEdit& Existing, FByteOffset Begin)
			{
				return Existing.Begin < Begin;
			});
		if ((Position != Edits.begin() && (Position - 1)->End > Edit.Begin)
			|| (Position != Edits.end() && Position->Begin < Edit.End))
		{
			return {false, "rewrite overlaps an existing edit"};
		}
		Edits.insert(Position, std::move(Edit));
		return {true, {}};
	}

	FRewriteResult FRewritePlan::Apply(std::string_view Source) const
	{
		FRewriteResult Result;
		FByteOffset OriginalOffset = 0;
		FByteOffset ProcessedOffset = 0;
		for (const FRewriteEdit& Edit : Edits)
		{
			if (Edit.End > Source.size())
			{
				Result.Error = "rewrite extends beyond the source";
				return Result;
			}
			if (Edit.Begin > OriginalOffset)
			{
				const FByteOffset Length = Edit.Begin - OriginalOffset;
				Result.Text.append(Source.substr(OriginalOffset, Length));
				Result.SourceMap.Segments.push_back({
					{ProcessedOffset, ProcessedOffset + Length},
					{OriginalOffset, Edit.Begin},
				});
				ProcessedOffset += Length;
			}
			Result.Text.append(Edit.Replacement);
			Result.SourceMap.Segments.push_back({
				{ProcessedOffset, ProcessedOffset + Edit.Replacement.size()},
				{Edit.Begin, Edit.End},
			});
			ProcessedOffset += Edit.Replacement.size();
			OriginalOffset = Edit.End;
		}
		if (OriginalOffset < Source.size())
		{
			const FByteOffset Length = Source.size() - OriginalOffset;
			Result.Text.append(Source.substr(OriginalOffset));
			Result.SourceMap.Segments.push_back({
				{ProcessedOffset, ProcessedOffset + Length},
				{OriginalOffset, Source.size()},
			});
		}
		Result.bSuccess = true;
		return Result;
	}

	const std::vector<FRewriteEdit>& FRewritePlan::GetEdits() const
	{
		return Edits;
	}

	FRewriteResult RewriteRangeBasedFor(const std::string_view Source)
	{
		const std::vector<FLexicalRange> Ranges =
			ScanLexicalRanges(Source);
		auto IsCodeOffset = [&Ranges](const FByteOffset Offset)
		{
			const auto Iterator = std::lower_bound(
				Ranges.begin(),
				Ranges.end(),
				Offset,
				[](const FLexicalRange& Range, const FByteOffset Value)
				{
					return Range.Span.End <= Value;
				});
			return Iterator != Ranges.end()
				&& Iterator->Span.Begin <= Offset
				&& Offset < Iterator->Span.End
				&& Iterator->Kind == ELexicalRangeKind::Code;
		};
		auto IsIdentifierCharacter = [](const char Character)
		{
			const unsigned char Value =
				static_cast<unsigned char>(Character);
			return std::isalnum(Value) != 0 || Character == '_';
		};

		FRewritePlan Plan;
		for (FByteOffset Offset = 0; Offset + 3 <= Source.size();)
		{
			const FByteOffset Candidate = Source.find("for", Offset);
			if (Candidate == std::string_view::npos)
			{
				break;
			}
			Offset = Candidate + 3;
			if (!IsCodeOffset(Candidate)
				|| (Candidate > 0
					&& IsIdentifierCharacter(Source[Candidate - 1]))
				|| (Candidate + 3 < Source.size()
					&& IsIdentifierCharacter(Source[Candidate + 3])))
			{
				continue;
			}

			FByteOffset Open = Candidate + 3;
			while (Open < Source.size()
				&& std::isspace(
					static_cast<unsigned char>(Source[Open])) != 0)
			{
				++Open;
			}
			if (Open >= Source.size()
				|| Source[Open] != '('
				|| !IsCodeOffset(Open))
			{
				continue;
			}
			const FDelimiterMatch Delimiter =
				FindMatchingDelimiter(Source, Open, '(', ')');
			if (!Delimiter.bSuccess)
			{
				continue;
			}

			int ParenthesisDepth = 0;
			int BracketDepth = 0;
			int BraceDepth = 0;
			FByteOffset TopLevelColon = std::string_view::npos;
			bool bHasTopLevelSemicolon = false;
			for (FByteOffset HeaderOffset = Open + 1;
				HeaderOffset < Delimiter.CloseOffset;
				++HeaderOffset)
			{
				if (!IsCodeOffset(HeaderOffset))
				{
					continue;
				}
				const char Character = Source[HeaderOffset];
				switch (Character)
				{
				case '(':
					++ParenthesisDepth;
					break;
				case ')':
					ParenthesisDepth = std::max(0, ParenthesisDepth - 1);
					break;
				case '[':
					++BracketDepth;
					break;
				case ']':
					BracketDepth = std::max(0, BracketDepth - 1);
					break;
				case '{':
					++BraceDepth;
					break;
				case '}':
					BraceDepth = std::max(0, BraceDepth - 1);
					break;
				case ';':
					if (ParenthesisDepth == 0
						&& BracketDepth == 0
						&& BraceDepth == 0)
					{
						bHasTopLevelSemicolon = true;
					}
					break;
				case ':':
					if (ParenthesisDepth == 0
						&& BracketDepth == 0
						&& BraceDepth == 0
						&& (HeaderOffset == Open + 1
							|| Source[HeaderOffset - 1] != ':')
						&& (HeaderOffset + 1
								>= Delimiter.CloseOffset
							|| Source[HeaderOffset + 1] != ':'))
					{
						if (TopLevelColon == std::string_view::npos)
						{
							TopLevelColon = HeaderOffset;
						}
					}
					break;
				default:
					break;
				}
			}
			if (TopLevelColon != std::string_view::npos
				&& !bHasTopLevelSemicolon)
			{
				auto Trim = [](std::string_view Text)
				{
					while (!Text.empty()
						&& std::isspace(static_cast<unsigned char>(
							Text.front())) != 0)
					{
						Text.remove_prefix(1);
					}
					while (!Text.empty()
						&& std::isspace(static_cast<unsigned char>(
							Text.back())) != 0)
					{
						Text.remove_suffix(1);
					}
					return Text;
				};
				const std::string_view Declaration = Trim(
					Source.substr(
						Open + 1,
						TopLevelColon - Open - 1));
				const std::string_view Container = Trim(
					Source.substr(
						TopLevelColon + 1,
						Delimiter.CloseOffset - TopLevelColon - 1));
				FByteOffset NameBegin = Declaration.size();
				while (NameBegin > 0
					&& IsIdentifierCharacter(
						Declaration[NameBegin - 1]))
				{
					--NameBegin;
				}
				const std::string_view Type = Trim(
					Declaration.substr(0, NameBegin));
				const std::string_view Name = Trim(
					Declaration.substr(NameBegin));
				if (Type.empty() || Name.empty() || Container.empty())
				{
					Offset = Delimiter.CloseOffset + 1;
					continue;
				}

				// Keep the generated surface byte-for-byte compatible with the
				// retained UE preprocessor contract. The iterator lives inside
				// the generated for statement, so the historical local name is
				// safely reusable for every lowered loop.
				const std::string IteratorName = "_Iterator";
				const std::string LoweredHeader =
					"for (auto " + IteratorName
					+ " = " + std::string(Container)
					+ ".Iterator();" + IteratorName
					+ ".CanProceed; )";
				const std::string ElementInitialization =
					std::string(Type)
					+ " __auto_constref_type "
					+ std::string(Name) + " = "
					+ IteratorName + ".Proceed();";

				FByteOffset BodyStart = Delimiter.CloseOffset + 1;
				while (BodyStart < Source.size()
					&& std::isspace(static_cast<unsigned char>(
						Source[BodyStart])) != 0)
				{
					++BodyStart;
				}
				FRewriteOperationResult Added;
				if (BodyStart < Source.size()
					&& Source[BodyStart] == '{'
					&& IsCodeOffset(BodyStart))
				{
					Added = Plan.Add({
						Candidate,
						Delimiter.CloseOffset + 1,
						LoweredHeader,
					});
					if (Added.bSuccess)
					{
						Added = Plan.Add({
							BodyStart + 1,
							BodyStart + 1,
							" " + ElementInitialization,
						});
					}
				}
				else
				{
					FByteOffset StatementEnd = BodyStart;
					int StatementParentheses = 0;
					int StatementBrackets = 0;
					for (; StatementEnd < Source.size(); ++StatementEnd)
					{
						if (!IsCodeOffset(StatementEnd))
						{
							continue;
						}
						const char Character = Source[StatementEnd];
						if (Character == '(')
						{
							++StatementParentheses;
						}
						else if (Character == ')')
						{
							StatementParentheses = std::max(
								0,
								StatementParentheses - 1);
						}
						else if (Character == '[')
						{
							++StatementBrackets;
						}
						else if (Character == ']')
						{
							StatementBrackets = std::max(
								0,
								StatementBrackets - 1);
						}
						else if (Character == ';'
							&& StatementParentheses == 0
							&& StatementBrackets == 0)
						{
							++StatementEnd;
							break;
						}
					}
					if (StatementEnd > BodyStart
						&& StatementEnd <= Source.size())
					{
						Added = Plan.Add({
							Candidate,
							StatementEnd,
							LoweredHeader + " { "
								+ ElementInitialization + " "
								+ std::string(Source.substr(
									BodyStart,
									StatementEnd - BodyStart))
								+ " }",
						});
						Offset = StatementEnd;
					}
				}
				if (!Added.bSuccess && !Added.Error.empty())
				{
					return {false, {}, Added.Error, {}};
				}
			}
			Offset = Delimiter.CloseOffset + 1;
		}
		return Plan.Apply(Source);
	}

	FRewriteResult RewriteNameLiterals(const std::string_view Source)
	{
		const std::vector<FLexicalRange> Ranges =
			ScanLexicalRanges(Source);
		FRewritePlan Plan;
		for (const FLexicalRange& Range : Ranges)
		{
			if (Range.Kind != ELexicalRangeKind::String
				|| Range.Span.Begin == 0
				|| Source[Range.Span.Begin - 1] != 'n')
			{
				continue;
			}
			const FByteOffset Prefix = Range.Span.Begin - 1;
			if (Prefix > 0)
			{
				const unsigned char Previous =
					static_cast<unsigned char>(Source[Prefix - 1]);
				if (std::isalnum(Previous) != 0
					|| Source[Prefix - 1] == '_')
				{
					continue;
				}
			}
			const FRewriteOperationResult Added = Plan.Add({
				Prefix,
				Range.Span.End,
				"FName("
					+ std::string(Source.substr(
						Range.Span.Begin,
						Range.Span.End - Range.Span.Begin))
					+ ")",
			});
			if (!Added.bSuccess)
			{
				return {false, {}, Added.Error, {}};
			}
		}
		return Plan.Apply(Source);
	}
}
