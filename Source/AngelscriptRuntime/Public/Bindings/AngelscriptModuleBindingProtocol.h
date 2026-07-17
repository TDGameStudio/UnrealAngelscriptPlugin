#pragma once

#include "CoreTypes.h"
#include "Features/IModularFeatures.h"
#include "UObject/NameTypes.h"

class UObject;

// Flags:
// bit0 Static, bit1 Const, bit2 WorldContext, bit3 HasOutParams, bit4 ReturnByRef.
// Higher bits are reserved and require a layout-version bump before use.
struct FAngelscriptModuleBindingCallFrame
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

struct FAngelscriptModuleBinding
{
	const TCHAR* ClassName;
	const TCHAR* FunctionName;
	void (*Thunk)(UObject* Self, FAngelscriptModuleBindingCallFrame* Frame);
	uint16 ArgCount;
	uint16 RetSize;
	uint32 Flags;
};

struct FAngelscriptModuleBindingFeatureView
{
	const FAngelscriptModuleBinding* Table;
	int32 Count;
	const TCHAR* ModuleName;
	uint32 LayoutVersion;
};

namespace FAngelscriptModuleBindingProtocol
{
	static constexpr uint32 LayoutVersionExpected = 0xA5C0DE02u;
	static constexpr uint32 FlagStatic = 1u << 0;
	static constexpr uint32 FlagConst = 1u << 1;
	static constexpr uint32 FlagWorldContext = 1u << 2;
	static constexpr uint32 FlagHasOutParams = 1u << 3;
	static constexpr uint32 FlagReturnByRef = 1u << 4;

	inline FName FeatureName()
	{
		return FName(TEXT("AngelscriptModuleBindingFeature"));
	}
}

static_assert(sizeof(FAngelscriptModuleBindingCallFrame) == 48, "FAngelscriptModuleBindingCallFrame ABI layout changed; bump module-binding-layout-version.txt.");
static_assert(sizeof(FAngelscriptModuleBinding) == 32, "FAngelscriptModuleBinding ABI layout changed; bump module-binding-layout-version.txt.");
static_assert(sizeof(FAngelscriptModuleBindingFeatureView) == 32, "FAngelscriptModuleBindingFeatureView ABI layout changed; bump module-binding-layout-version.txt.");
