#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace AngelscriptStandalone::Frontend
{
	using FByteOffset = std::size_t;

	struct FPathResult
	{
		bool bSuccess = false;
		std::string Value;
		std::string Error;
	};

	struct FSourceLocation
	{
		FByteOffset ByteOffset = 0;
		std::size_t Line = 1;
		std::size_t Column = 1;
	};

	struct FSourceSpan
	{
		FByteOffset Begin = 0;
		FByteOffset End = 0;
	};

	struct FSourceInput
	{
		std::string LogicalPath;
		std::string Contents;
	};

	class FSourceText
	{
	public:
		explicit FSourceText(std::string_view Text);

		FSourceLocation GetLocation(FByteOffset ByteOffset) const;
		FByteOffset GetByteOffset(std::size_t Line, std::size_t Column) const;
		std::string_view GetText() const;

	private:
		std::string Text;
		std::vector<FByteOffset> LineStarts;
	};

	bool IsValidUtf8(std::string_view Text);
	FPathResult NormalizeLogicalPath(std::string_view Path);
	std::string ModuleNameFromLogicalPath(std::string_view LogicalPath);
	std::string MakeStableModuleId(std::string_view LogicalPath);
}
