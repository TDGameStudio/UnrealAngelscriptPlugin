#pragma once

#include "CoreMinimal.h"
#include "Binds/Helper_FunctionSignature.h"

class UFunction;
struct FAngelscriptFunctionBinding;

// Per-UFunction prepared data for the Bind_Defaults Late+100 Phase 2 split.
//
// Phase 2A (prepare) populates this struct from a single UClass + UFunction:
//  - InitFromFunction reads UE reflection (read-only) and builds the AS declaration
//    string. This is the bulk of the per-function CPU cost (~60% of Phase 2).
//  - CachedBinding comes from FAngelscriptBinds::GetClassFunctionBindings() lookup so Phase 2B
//    does not have to redo it.
// Phase 2B (commit) consumes Prep.Signature and Prep.CachedBinding and only invokes
// AS Engine register methods, which must remain serial.
//
// Lifetime: a TArray<FUFunctionBindPrep> lives inside the Bind_Defaults FBindOrder
// for the duration of the Late+100 lambda only.
struct FUFunctionBindPrep
{
	enum class EKind : uint8
	{
		Skip = 0,
		Callable,
		Event,
	};

	UFunction* Function = nullptr;
	EKind Kind = EKind::Skip;
	FAngelscriptFunctionSignature Signature;
	FAngelscriptFunctionBinding* CachedBinding = nullptr;
};
