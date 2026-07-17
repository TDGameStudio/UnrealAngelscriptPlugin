#pragma once

#include "CoreTypes.h"
#include "Features/IModularFeatures.h"
#include "UObject/NameTypes.h"

class UObject;

// Flags:
// bit0 Static, bit1 Const, bit2 WorldContext, bit3 HasOutParams, bit4 ReturnByRef.
// Higher bits are reserved and require a layout-version bump before use.
struct FAngelscriptCrossModuleCallFrame
{
	void** ArgSlots;
	uint16 ArgCount;
	uint16 Reserved0;
	void* ReturnSlot;
	UObject* ScriptSelf;
	UObject* WorldContext;
	uint32 Flags;
	uint32 Reserved1;
};

struct FAngelscriptCrossModuleBinding
{
	const TCHAR* ClassName;
	const TCHAR* FunctionName;
	void (*Thunk)(UObject* Self, FAngelscriptCrossModuleCallFrame* Frame);
	uint16 ArgCount;
	uint16 RetSize;
	uint32 Flags;
};

struct FAngelscriptCrossModuleBindingFeatureReader
{
	const FAngelscriptCrossModuleBinding* Table;
	int32 Count;
	const TCHAR* ModuleName;
	uint32 LayoutVersion;
};

namespace FAngelscriptCrossModuleFunctionBindings
{
	static constexpr uint32 LayoutVersionExpected = 0xA5C0DE02u;
	static constexpr uint32 FlagStatic = 1u << 0;
	static constexpr uint32 FlagConst = 1u << 1;
	static constexpr uint32 FlagWorldContext = 1u << 2;
	static constexpr uint32 FlagHasOutParams = 1u << 3;
	static constexpr uint32 FlagReturnByRef = 1u << 4;

	inline FName FeatureName()
	{
		return FName(TEXT("AngelscriptCrossModuleFunctionBindings"));
	}
}

static_assert(sizeof(FAngelscriptCrossModuleCallFrame) == 48, "FAngelscriptCrossModuleCallFrame ABI layout changed; bump cross-module-layout-version.txt.");
static_assert(sizeof(FAngelscriptCrossModuleBinding) == 32, "FAngelscriptCrossModuleBinding ABI layout changed; bump cross-module-layout-version.txt.");
static_assert(sizeof(FAngelscriptCrossModuleBindingFeatureReader) == 32, "FAngelscriptCrossModuleBindingFeatureReader ABI layout changed; bump cross-module-layout-version.txt.");
