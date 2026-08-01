#include "Resources/AngelscriptAssetPath.h"

#include <algorithm>
#include <cctype>

namespace AngelscriptStandalone
{
	namespace
	{
		std::string_view Trim(std::string_view Text)
		{
			while (!Text.empty()
				&& std::isspace(
					static_cast<unsigned char>(Text.front())) != 0)
			{
				Text.remove_prefix(1);
			}
			while (!Text.empty()
				&& std::isspace(
					static_cast<unsigned char>(Text.back())) != 0)
			{
				Text.remove_suffix(1);
			}
			return Text;
		}
	}

	FAssetPathResult NormalizeAssetPath(const std::string_view Path)
	{
		FAssetPathResult Result;
		Result.Original = std::string(Path);
		std::string_view Value = Trim(Path);
		if (Value.size() >= 2
			&& ((Value.front() == '"' && Value.back() == '"')
				|| (Value.front() == '\''
					&& Value.back() == '\'')))
		{
			Value.remove_prefix(1);
			Value.remove_suffix(1);
		}
		const std::size_t LiteralQuote = Value.find('\'');
		if (LiteralQuote != std::string_view::npos
			&& Value.ends_with('\''))
		{
			Value = Value.substr(
				LiteralQuote + 1,
				Value.size() - LiteralQuote - 2);
		}
		if (Value.empty() || Value.front() != '/')
		{
			Result.Error =
				"asset path must begin with a mounted root";
			return Result;
		}
		if (Value.find('\\') != std::string_view::npos
			|| Value.find("..") != std::string_view::npos
			|| Value.find("//") != std::string_view::npos
			|| Value.find_first_of("\r\n\t") != std::string_view::npos)
		{
			Result.Error =
				"asset path contains a non-canonical component";
			return Result;
		}
		for (const char Character : Value)
		{
			if (std::isspace(
					static_cast<unsigned char>(Character)) != 0)
			{
				Result.Error =
					"asset path contains whitespace";
				return Result;
			}
		}
		const std::size_t MountEnd = Value.find('/', 1);
		const std::size_t Dot = Value.find('.');
		const std::size_t EffectiveMountEnd =
			MountEnd == std::string_view::npos
				? (Dot == std::string_view::npos
					? Value.size()
					: Dot)
				: MountEnd;
		if (EffectiveMountEnd <= 1)
		{
			Result.Error = "asset mount point is empty";
			return Result;
		}
		Result.MountPoint =
			std::string(Value.substr(0, EffectiveMountEnd));
		Result.Normalized = std::string(Value);
		if (Result.MountPoint == "/Script")
		{
			Result.Kind = EAssetPathKind::ScriptObject;
		}
		else if (Dot == std::string_view::npos)
		{
			Result.Kind = EAssetPathKind::Package;
		}
		else if (Value.ends_with("_C"))
		{
			Result.Kind = EAssetPathKind::GeneratedClass;
		}
		else
		{
			Result.Kind = EAssetPathKind::Object;
		}
		Result.bChanged = Result.Original != Result.Normalized;
		Result.bSuccess = true;
		return Result;
	}
}
