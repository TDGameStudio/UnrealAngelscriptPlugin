// Copyright Hazelight + AngelscriptProject contributors. Apache-2.0.
//
// Per-engine owner of FBlueprintEventSignature instances that are attached to
// asCScriptFunction via SetUserData(...). The AS 2.33 fork used by this project
// only stores a single void* user data slot and never invokes a cleanup callback
// when a script function is destroyed, so any heap-allocated signature must be
// tracked separately and released after the script engine has shut down.
//
// The struct FBlueprintEventSignature stays private to Bind_BlueprintEvent.cpp;
// the registry only sees opaque void* + a centralized deleter implemented in
// that same translation unit (DropOwnedSignature), where the full type is
// visible. This keeps the registry header dependency-free.
//
// Lifecycle:
//   1. FAngelscriptEngine::Initialize* allocates the registry directly as a
//      `TUniquePtr<FBlueprintEventSignatureRegistry> BlueprintEventSignatureRegistry`
//      member of the engine.
//   2. Bind_BlueprintEvent.cpp creates a signature with `new FBlueprintEventSignature`,
//      hands it to AS via SetUserData (through OnBind), and immediately transfers
//      ownership to the registry via AddOwnership().
//   3. FAngelscriptEngine::Shutdown() calls Reset() on the registry *after*
//      ScriptEngine->ShutDownAndRelease() has destroyed every asCScriptFunction
//      that referenced the signature.
//
// See Documents/Guides/ASBindFreeCompletenessVerification.md (Phase 2) for the
// LLM evidence that motivated this fix.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Templates/UniquePtr.h"

namespace BlueprintEventSignatureRegistryInternal
{
	// Implemented in Bind_BlueprintEvent.cpp where FBlueprintEventSignature
	// is a complete type. The registry calls this for every owned pointer
	// from Reset() and from its destructor.
	void DropOwnedSignature(void* Signature);
}

class ANGELSCRIPTRUNTIME_API FBlueprintEventSignatureRegistry
{
public:
	FBlueprintEventSignatureRegistry() = default;
	~FBlueprintEventSignatureRegistry();

	FBlueprintEventSignatureRegistry(const FBlueprintEventSignatureRegistry&) = delete;
	FBlueprintEventSignatureRegistry& operator=(const FBlueprintEventSignatureRegistry&) = delete;
	FBlueprintEventSignatureRegistry(FBlueprintEventSignatureRegistry&&) = delete;
	FBlueprintEventSignatureRegistry& operator=(FBlueprintEventSignatureRegistry&&) = delete;

	// Take ownership of a heap-allocated FBlueprintEventSignature. Thread-safe.
	// Passing nullptr is a no-op (defensive: avoids tracking a null pointer if
	// an upstream allocation path ever returns null).
	void AddOwnership(void* Signature);

	// Delete every owned signature using the centralized deleter and clear the
	// list. Must be called only after the asIScriptEngine that referenced these
	// signatures via SetUserData has been ShutDownAndRelease()'d so we do not
	// observe a dangling userData read during teardown.
	void Reset();

	// Diagnostic: number of signatures currently owned by this registry.
	int32 Num() const;

private:
	mutable FCriticalSection CriticalSection;
	TArray<void*> Owned;
};
