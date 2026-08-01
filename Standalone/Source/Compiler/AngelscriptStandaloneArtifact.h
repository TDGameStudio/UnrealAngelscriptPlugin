#pragma once

#include "Compiler/AngelscriptStandaloneNativeCompiler.h"
#include "Runtime/AngelscriptStandaloneRunner.h"

#include <filesystem>
#include <string>

namespace AngelscriptStandalone
{
	struct FArtifactWriteRequest
	{
		std::filesystem::path OutputDirectory;
		const FCompileResult* CompileResult = nullptr;
	};

	struct FArtifactWriteResult
	{
		bool bSuccess = false;
		std::string Error;
	};

	struct FRunArtifactWriteRequest
	{
		std::filesystem::path OutputDirectory;
		const FRunResult* RunResult = nullptr;
	};

	FArtifactWriteResult WriteCompileArtifacts(const FArtifactWriteRequest& Request);
	FArtifactWriteResult WriteRunArtifacts(const FRunArtifactWriteRequest& Request);
}
