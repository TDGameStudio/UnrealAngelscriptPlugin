#pragma once

#include "CoreMinimal.h"

class UFunction;
class UObject;
class asIScriptGeneric;
struct FAngelscriptBinds;
struct FAngelscriptBoundFunction;
struct FAngelscriptType;
struct FAngelscriptFunctionBinding;
struct FAngelscriptFunctionSignature;

// Result of evaluating whether a UFunction is eligible for reflection-based fallback binding.
enum class EReflectionFallbackResult : uint8
{
	// Function passed all checks and can use reflection fallback.
	Success,
	// Function pointer is null.
	NullFunction,
	// Function has no valid owning UClass.
	MissingOwningClass,
	// Function belongs to an interface class, which is not supported.
	InterfaceClass,
	// Function uses a custom thunk (DECLARE_FUNCTION), incompatible with generic reflection invoke.
	CustomThunk,
	// Function has more parameters than the reflection fallback can handle.
	TooManyArguments,
};

ANGELSCRIPTRUNTIME_API EReflectionFallbackResult EvaluateReflectionFallback(const UFunction* Function);
ANGELSCRIPTRUNTIME_API bool ShouldBindBlueprintCallableReflectionFallback(const UFunction* Function);
ANGELSCRIPTRUNTIME_API bool InvokeReflectionFallbackFromGenericCall(
	asIScriptGeneric* Generic,
	UObject* TargetObject,
	UFunction* Function,
	bool bInjectMixinObject = false);
bool IsScriptDeclarationAlreadyBound(
	FAngelscriptBinds& Binds,
	TSharedRef<FAngelscriptType> InType,
	FAngelscriptFunctionSignature& Signature);

// RAII guard that enables TLS caches used by BlueprintType ReflectionBindings:
//   - global declaration cache for IsScriptDeclarationAlreadyBound (Opt 1)
//   - per-class function-name cache for GetScriptNameForFunction prefix conflict detection (Opt 3)
// Outside of the guard scope, cached lookups fall back to the original linear / FindFunctionByName path.
struct ANGELSCRIPTRUNTIME_API FScopedBindCaches
{
	FScopedBindCaches();
	~FScopedBindCaches();

	FScopedBindCaches(const FScopedBindCaches&) = delete;
	FScopedBindCaches& operator=(const FScopedBindCaches&) = delete;
};

// Called by global-function registration paths to keep the TLS declaration cache warm.
// Safe to call when no FScopedBindCaches is active (becomes a no-op).
ANGELSCRIPTRUNTIME_API void AngelscriptBindCaches_NotifyGlobalFunctionRegistered(const char* Namespace, const char* Name, const char* Declaration);

// Called by GetScriptNameForFunction as a fast path replacement for OwningClass->FindFunctionByName during Phase 2.
// Returns true when Phase 2 caches are active and populated; sets bOutExists accordingly.
// When false, callers must fall back to FindFunctionByName.
ANGELSCRIPTRUNTIME_API bool AngelscriptBindCaches_TryHasFunctionName(class UClass* OwningClass, FName FunctionName, bool& bOutExists);

ANGELSCRIPTRUNTIME_API bool BindBlueprintCallableReflectionFallback(
	FAngelscriptBinds& Binds,
	TSharedRef<FAngelscriptType> InType,
	UFunction* Function,
	FAngelscriptFunctionSignature& Signature,
	FAngelscriptFunctionBinding& Binding,
	FAngelscriptBoundFunction& OutPrimaryBinding);
