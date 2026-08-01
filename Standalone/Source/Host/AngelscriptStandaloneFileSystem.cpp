#include "Host/AngelscriptStandaloneFileSystem.h"

#include "Compiler/Frontend/AngelscriptStandaloneSource.h"

#include <fstream>
#include <iterator>

namespace AngelscriptStandalone
{
	namespace
	{
		bool IsWithin(const std::filesystem::path& Child, const std::filesystem::path& Parent)
		{
			auto ChildIterator = Child.begin();
			auto ParentIterator = Parent.begin();
			for (; ParentIterator != Parent.end(); ++ParentIterator, ++ChildIterator)
			{
				if (ChildIterator == Child.end() || *ChildIterator != *ParentIterator)
				{
					return false;
				}
			}
			return true;
		}
	}

	std::string NormalizeLogicalPath(const std::filesystem::path& Path)
	{
		const AngelscriptStandalone::Frontend::FPathResult Result =
			AngelscriptStandalone::Frontend::NormalizeLogicalPath(Path.generic_string());
		return Result.bSuccess ? Result.Value : std::string();
	}

	bool IsValidUtf8(const std::string& Text)
	{
		return AngelscriptStandalone::Frontend::IsValidUtf8(Text);
	}

	FSourceResolveResult ResolveAndReadEntry(
		const std::vector<std::filesystem::path>& ScriptRoots,
		const std::filesystem::path& Entry)
	{
		FSourceResolveResult Result;
		if (ScriptRoots.empty())
		{
			Result.Error = "at least one --script-root is required";
			return Result;
		}
		if (Entry.empty())
		{
			Result.Error = "--entry is required";
			return Result;
		}

		for (const std::filesystem::path& InputRoot : ScriptRoots)
		{
			std::error_code Error;
			const std::filesystem::path Root = std::filesystem::weakly_canonical(InputRoot, Error);
			if (Error)
			{
				continue;
			}
			const std::filesystem::path CandidateInput = Entry.is_absolute() ? Entry : Root / Entry;
			const std::filesystem::path Candidate = std::filesystem::weakly_canonical(CandidateInput, Error);
			if (Error || !IsWithin(Candidate, Root) || !std::filesystem::is_regular_file(Candidate, Error))
			{
				continue;
			}

			std::ifstream Input(Candidate, std::ios::binary);
			if (!Input)
			{
				Result.Error = "failed to open entry source";
				return Result;
			}
			std::string Contents(
				(std::istreambuf_iterator<char>(Input)),
				std::istreambuf_iterator<char>());
			if (Contents.starts_with("\xef\xbb\xbf"))
			{
				Contents.erase(0, 3);
			}
			if (!IsValidUtf8(Contents))
			{
				Result.Error = "entry source is not valid UTF-8";
				return Result;
			}

			Result.bSuccess = true;
			Result.Source.AbsolutePath = Candidate;
			Result.Source.LogicalPath = NormalizeLogicalPath(std::filesystem::relative(Candidate, Root));
			Result.Source.Contents = std::move(Contents);
			return Result;
		}

		Result.Error = "entry was not found beneath any script root";
		return Result;
	}
}
