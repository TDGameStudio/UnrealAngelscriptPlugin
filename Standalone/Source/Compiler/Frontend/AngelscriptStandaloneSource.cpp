#include "Compiler/Frontend/AngelscriptStandaloneSource.h"

#include "Support/AngelscriptStandaloneHash.h"

#include <algorithm>

namespace AngelscriptStandalone::Frontend
{
	namespace
	{
		bool IsContinuationByte(unsigned char Byte)
		{
			return (Byte & 0xc0u) == 0x80u;
		}

		std::size_t Utf8SequenceLength(unsigned char Lead)
		{
			if (Lead < 0x80u)
			{
				return 1;
			}
			if ((Lead & 0xe0u) == 0xc0u)
			{
				return 2;
			}
			if ((Lead & 0xf0u) == 0xe0u)
			{
				return 3;
			}
			if ((Lead & 0xf8u) == 0xf0u)
			{
				return 4;
			}
			return 0;
		}
	}

	bool IsValidUtf8(std::string_view Text)
	{
		for (std::size_t Offset = 0; Offset < Text.size();)
		{
			const unsigned char Lead = static_cast<unsigned char>(Text[Offset]);
			const std::size_t Length = Utf8SequenceLength(Lead);
			if (Length == 0 || Offset + Length > Text.size())
			{
				return false;
			}
			for (std::size_t Index = 1; Index < Length; ++Index)
			{
				if (!IsContinuationByte(static_cast<unsigned char>(Text[Offset + Index])))
				{
					return false;
				}
			}
			if ((Length == 2 && Lead < 0xc2u)
				|| (Length == 3
					&& ((Lead == 0xe0u
							&& static_cast<unsigned char>(Text[Offset + 1]) < 0xa0u)
						|| (Lead == 0xedu
							&& static_cast<unsigned char>(Text[Offset + 1]) >= 0xa0u)))
				|| (Length == 4
					&& ((Lead == 0xf0u
							&& static_cast<unsigned char>(Text[Offset + 1]) < 0x90u)
						|| Lead > 0xf4u
						|| (Lead == 0xf4u
							&& static_cast<unsigned char>(Text[Offset + 1]) >= 0x90u))))
			{
				return false;
			}
			Offset += Length;
		}
		return true;
	}

	FPathResult NormalizeLogicalPath(std::string_view Path)
	{
		FPathResult Result;
		if (Path.empty())
		{
			Result.Error = "logical path is empty";
			return Result;
		}
		if (!IsValidUtf8(Path))
		{
			Result.Error = "logical path is not valid UTF-8";
			return Result;
		}

		std::string Slashed(Path);
		std::replace(Slashed.begin(), Slashed.end(), '\\', '/');
		if (Slashed.starts_with('/') || Slashed.find(':') != std::string::npos)
		{
			Result.Error = "logical path must be relative";
			return Result;
		}

		std::vector<std::string_view> Components;
		std::size_t Offset = 0;
		while (Offset <= Slashed.size())
		{
			const std::size_t End = Slashed.find('/', Offset);
			const std::string_view Component(
				Slashed.data() + Offset,
				(End == std::string::npos ? Slashed.size() : End) - Offset);
			if (!Component.empty() && Component != ".")
			{
				if (Component == "..")
				{
					if (Components.empty())
					{
						Result.Error = "logical path escapes its source root";
						return Result;
					}
					Components.pop_back();
				}
				else
				{
					Components.push_back(Component);
				}
			}
			if (End == std::string::npos)
			{
				break;
			}
			Offset = End + 1;
		}
		if (Components.empty())
		{
			Result.Error = "logical path resolves to an empty path";
			return Result;
		}

		for (std::size_t Index = 0; Index < Components.size(); ++Index)
		{
			if (Index != 0)
			{
				Result.Value.push_back('/');
			}
			Result.Value.append(Components[Index]);
		}
		Result.bSuccess = true;
		return Result;
	}

	std::string ModuleNameFromLogicalPath(std::string_view LogicalPath)
	{
		const FPathResult Normalized = NormalizeLogicalPath(LogicalPath);
		if (!Normalized.bSuccess)
		{
			return {};
		}
		std::string ModuleName = Normalized.Value;
		if (ModuleName.ends_with(".as"))
		{
			ModuleName.resize(ModuleName.size() - 3);
		}
		std::replace(ModuleName.begin(), ModuleName.end(), '/', '.');
		return ModuleName;
	}

	std::string MakeStableModuleId(std::string_view LogicalPath)
	{
		const std::string ModuleName = ModuleNameFromLogicalPath(LogicalPath);
		return ModuleName.empty()
			? std::string()
			: "module-" + Sha256("module-v1\n" + ModuleName).substr(0, 24);
	}

	FSourceText::FSourceText(std::string_view InText)
		: Text(InText)
	{
		LineStarts.push_back(0);
		for (std::size_t Offset = 0; Offset < Text.size(); ++Offset)
		{
			if (Text[Offset] == '\n')
			{
				LineStarts.push_back(Offset + 1);
			}
		}
	}

	FSourceLocation FSourceText::GetLocation(FByteOffset InByteOffset) const
	{
		const FByteOffset ByteOffset = std::min(InByteOffset, Text.size());
		const auto LineIterator = std::upper_bound(
			LineStarts.begin(),
			LineStarts.end(),
			ByteOffset);
		const std::size_t LineIndex =
			LineIterator == LineStarts.begin()
				? 0
				: static_cast<std::size_t>(LineIterator - LineStarts.begin() - 1);
		std::size_t Column = 1;
		for (FByteOffset Offset = LineStarts[LineIndex]; Offset < ByteOffset; ++Offset)
		{
			if (!IsContinuationByte(static_cast<unsigned char>(Text[Offset])))
			{
				++Column;
			}
		}
		return {ByteOffset, LineIndex + 1, Column};
	}

	FByteOffset FSourceText::GetByteOffset(std::size_t Line, std::size_t Column) const
	{
		if (Line == 0 || Column == 0 || Line > LineStarts.size())
		{
			return Text.size();
		}
		FByteOffset Offset = LineStarts[Line - 1];
		const FByteOffset LineEnd =
			Line < LineStarts.size() ? LineStarts[Line] - 1 : Text.size();
		for (std::size_t CurrentColumn = 1;
			CurrentColumn < Column && Offset < LineEnd;
			++CurrentColumn)
		{
			const std::size_t Length =
				Utf8SequenceLength(static_cast<unsigned char>(Text[Offset]));
			Offset += Length == 0 ? 1 : Length;
		}
		return std::min(Offset, LineEnd);
	}

	std::string_view FSourceText::GetText() const
	{
		return Text;
	}
}
