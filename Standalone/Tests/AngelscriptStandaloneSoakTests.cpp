#include "Compiler/AngelscriptStandaloneArtifact.h"
#include "Runtime/AngelscriptStandaloneRunner.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	namespace fs = std::filesystem;
	using namespace AngelscriptStandalone;

	bool Require(const bool bCondition, const std::string_view Message)
	{
		if (!bCondition)
		{
			std::cerr << Message << '\n';
		}
		return bCondition;
	}

	std::string ReadText(const fs::path& Path)
	{
		std::ifstream Input(Path, std::ios::binary);
		return std::string(
			std::istreambuf_iterator<char>(Input),
			std::istreambuf_iterator<char>());
	}
}

int main()
{
	const fs::path CorpusRoot =
		fs::path(ANGELSCRIPT_STANDALONE_CORPUS_ROOT) / "Native";
	const fs::path OutputRoot =
		fs::temp_directory_path()
		/ ("angelscript-standalone-soak-"
			+ std::to_string(
				std::chrono::steady_clock::now()
					.time_since_epoch()
					.count()));
	std::error_code Error;
	fs::create_directories(OutputRoot, Error);
	if (!Require(!Error, "could not create soak output root"))
	{
		return 1;
	}

	bool bPassed = true;
	std::string ExpectedInputHash;
	std::string ExpectedProfileHash;
	std::vector<std::uint8_t> ExpectedByteCode;
	std::string ExpectedResultArtifact;
	for (int Iteration = 0; Iteration < 100; ++Iteration)
	{
		FRunRequest Request;
		Request.ScriptRoots = {CorpusRoot};
		Request.Entry = "basic.as";
		Request.TimeoutMilliseconds = 1000;
		Request.MemoryLimitBytes = 16ull * 1024ull * 1024ull;
		const FRunResult Result = RunNativeScript(Request);
		bPassed &= Require(
			Result.ExitCode == 0
				&& Result.ScriptResult.has_value()
				&& *Result.ScriptResult == 17,
			"soak run outcome changed");
		bPassed &= Require(
			Result.AllocatedBytesAfterShutdown == 0,
			"soak run leaked tracked engine allocations");
		bPassed &= Require(
			!Result.bTimedOut && !Result.bMemoryLimitReached,
			"soak run crossed a resource guard");
		if (Iteration == 0)
		{
			ExpectedInputHash = Result.InputHash;
			ExpectedProfileHash = Result.ProfileHash;
			ExpectedByteCode = Result.ByteCode;
		}
		else
		{
			bPassed &= Require(
				Result.InputHash == ExpectedInputHash
					&& Result.ProfileHash == ExpectedProfileHash
					&& Result.ByteCode == ExpectedByteCode,
				"soak run produced nondeterministic identities or bytecode");
		}

		const fs::path ArtifactRoot = OutputRoot / "current";
		const FArtifactWriteResult Write =
			WriteRunArtifacts({ArtifactRoot, &Result});
		bPassed &= Require(
			Write.bSuccess,
			Write.Error.empty()
				? "soak artifact replacement failed"
				: Write.Error);
		const std::string ResultArtifact =
			ReadText(ArtifactRoot / "result.json");
		if (Iteration == 0)
		{
			ExpectedResultArtifact = ResultArtifact;
		}
		else
		{
			bPassed &= Require(
				ResultArtifact == ExpectedResultArtifact,
				"soak result artifact changed across identical runs");
		}
	}

	fs::remove_all(OutputRoot, Error);
	bPassed &= Require(
		!Error && !fs::exists(OutputRoot),
		"soak left locked or temporary artifacts");
	return bPassed ? 0 : 1;
}
