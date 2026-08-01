#pragma once

class asIScriptGeneric;

namespace AngelscriptStandalone
{
	inline constexpr const char CompileOnlyTrapMessage[] =
		"UE validation imports are compile-only and cannot execute";

	void CompileOnlyTrap(asIScriptGeneric* Generic);
}
