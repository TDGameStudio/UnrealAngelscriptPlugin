#include "Compiler/Frontend/AngelscriptStandaloneLexing.h"

namespace AngelscriptStandalone::Frontend
{
	std::vector<FLexicalRange> ScanLexicalRanges(std::string_view Text)
	{
		std::vector<FLexicalRange> Ranges;
		std::size_t CodeStart = 0;
		auto EmitCode = [&Ranges, &CodeStart](std::size_t End)
		{
			if (End > CodeStart)
			{
				Ranges.push_back({ELexicalRangeKind::Code, {CodeStart, End}, true});
			}
		};

		for (std::size_t Offset = 0; Offset < Text.size();)
		{
			ELexicalRangeKind Kind = ELexicalRangeKind::Code;
			char Quote = '\0';
			if (Text[Offset] == '/' && Offset + 1 < Text.size() && Text[Offset + 1] == '/')
			{
				Kind = ELexicalRangeKind::LineComment;
			}
			else if (Text[Offset] == '/' && Offset + 1 < Text.size() && Text[Offset + 1] == '*')
			{
				Kind = ELexicalRangeKind::BlockComment;
			}
			else if (Text[Offset] == '"' || Text[Offset] == '\'')
			{
				Kind = ELexicalRangeKind::String;
				Quote = Text[Offset];
			}
			else
			{
				++Offset;
				continue;
			}

			EmitCode(Offset);
			const std::size_t RangeStart = Offset;
			bool bTerminated = false;
			if (Kind == ELexicalRangeKind::LineComment)
			{
				Offset += 2;
				while (Offset < Text.size() && Text[Offset] != '\n')
				{
					++Offset;
				}
				bTerminated = true;
			}
			else if (Kind == ELexicalRangeKind::BlockComment)
			{
				Offset += 2;
				while (Offset + 1 < Text.size())
				{
					if (Text[Offset] == '*' && Text[Offset + 1] == '/')
					{
						Offset += 2;
						bTerminated = true;
						break;
					}
					++Offset;
				}
				if (!bTerminated)
				{
					Offset = Text.size();
				}
			}
			else
			{
				++Offset;
				bool bEscaped = false;
				while (Offset < Text.size())
				{
					const char Character = Text[Offset++];
					if (Character == Quote && !bEscaped)
					{
						bTerminated = true;
						break;
					}
					if (Character == '\\' && !bEscaped)
					{
						bEscaped = true;
					}
					else
					{
						bEscaped = false;
					}
				}
			}
			Ranges.push_back({Kind, {RangeStart, Offset}, bTerminated});
			CodeStart = Offset;
		}
		EmitCode(Text.size());
		return Ranges;
	}

	std::string BlankCommentsPreservingLayout(std::string_view Text)
	{
		std::string Result(Text);
		for (const FLexicalRange& Range : ScanLexicalRanges(Text))
		{
			if (Range.Kind != ELexicalRangeKind::LineComment
				&& Range.Kind != ELexicalRangeKind::BlockComment)
			{
				continue;
			}
			for (std::size_t Offset = Range.Span.Begin; Offset < Range.Span.End; ++Offset)
			{
				if (Result[Offset] != '\n' && Result[Offset] != '\r')
				{
					Result[Offset] = ' ';
				}
			}
		}
		return Result;
	}

	FDelimiterMatch FindMatchingDelimiter(
		std::string_view Text,
		FByteOffset OpenOffset,
		char OpenDelimiter,
		char CloseDelimiter)
	{
		FDelimiterMatch Result;
		if (OpenOffset >= Text.size() || Text[OpenOffset] != OpenDelimiter)
		{
			Result.Error = "opening delimiter is not present at the requested offset";
			return Result;
		}

		int Depth = 0;
		for (const FLexicalRange& Range : ScanLexicalRanges(Text))
		{
			if (Range.Kind != ELexicalRangeKind::Code || Range.Span.End <= OpenOffset)
			{
				continue;
			}
			const std::size_t Start = Range.Span.Begin < OpenOffset
				? OpenOffset
				: Range.Span.Begin;
			for (std::size_t Offset = Start; Offset < Range.Span.End; ++Offset)
			{
				if (Text[Offset] == OpenDelimiter)
				{
					++Depth;
				}
				else if (Text[Offset] == CloseDelimiter)
				{
					--Depth;
					if (Depth == 0)
					{
						Result.bSuccess = true;
						Result.CloseOffset = Offset;
						return Result;
					}
				}
			}
		}
		Result.Error = "matching closing delimiter was not found";
		return Result;
	}
}
