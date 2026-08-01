#pragma once

#include "angelscript.h"

#include <functional>
#include <string>

namespace AngelscriptStandalone
{
	struct FStandardLibraryOptions
	{
		std::function<void(const std::string&)> Print;
	};

	bool RegisterNativeStandardLibrary(
		asIScriptEngine* Engine,
		const FStandardLibraryOptions& Options,
		std::string& OutError);
}
