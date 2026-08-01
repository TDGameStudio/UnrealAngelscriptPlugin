#pragma once

#include "Compiler/AngelscriptStandaloneUECompiler.h"

#include <filesystem>
#include <string>

namespace AngelscriptStandalone
{
	struct FValidationArtifactWriteRequest
	{
		std::filesystem::path OutputDirectory;
		const FUECompileResult* CompileResult = nullptr;
	};

	struct FValidationArtifactWriteResult
	{
		bool bSuccess = false;
		std::string Error;
	};

	FValidationArtifactWriteResult WriteValidationArtifacts(
		const FValidationArtifactWriteRequest& Request);
}
