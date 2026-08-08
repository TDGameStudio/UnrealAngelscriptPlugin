#pragma once
#include "CoreMinimal.h"
#include "StaticJITConfig.h"

#if AS_CAN_GENERATE_JIT

struct FAngelscriptEngine;
struct FNativeFunctionContext;
class UFunction;

struct FNativeFunctionCall
{
	FString CallCode;
	FString ReturnValue;
	FString Header;
	bool bHandledReturnValue = false;
};

enum class EScriptFunctionCallMethod : uint8
{
	NativeCall,
	CustomCall,
	PointerCall,
};

#if WITH_DEV_AUTOMATION_TESTS
enum class EAngelscriptNativeFormKind : uint8
{
	Unknown,
	Constructor,
	Destructor,
	Assignment,
	UObjectCast,
	Method,
	Function,
	FunctionHeader,
	UFunction,
	TArrayIndex,
	TArrayIteratorCreate,
	TArrayIteratorProceed,
	TemplateInstantiatedCall,
	DelegateExecute,
	MulticastExecute,
	EventFunctionExecute,
	PushArgument,
	PushArgumentRef,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptNativeFormDebugInfo
{
	EAngelscriptNativeFormKind Kind = EAngelscriptNativeFormKind::Unknown;
	FString Name;
	FString CustomForm;
	FString TargetType;
	FString Header;
	const UFunction* UnrealFunction = nullptr;
	bool bTrivial = false;
	bool bGuaranteed = false;
	bool bNeedsCompare = false;
	bool bNeedsCopy = false;
};
#endif

struct ANGELSCRIPTRUNTIME_API FScriptFunctionNativeForm
{
	virtual ~FScriptFunctionNativeForm() {}
	virtual FNativeFunctionCall GenerateCall(FNativeFunctionContext& Context) const { return FNativeFunctionCall{}; };
	virtual bool ShouldIgnoreObjectArgument() const { return false; }
	virtual bool IsTrivialFunction(EScriptFunctionCallMethod Method) const { return false; }
	virtual bool CanSkipObjectNullCheck(EScriptFunctionCallMethod Method) const { return false; }
	virtual bool CanSkipInformSystemFunction() const { return false; }
	virtual bool CanSkipScriptFunctionLookup(FNativeFunctionContext& Context) const { return false; }
	virtual bool CanCallNative(const FNativeFunctionContext& Context) const { return true; }

	virtual bool CanCallCustom(const FNativeFunctionContext& Context) const { return false; }
	virtual bool ShouldCustomLookupScriptFunction(const FNativeFunctionContext& Context) const { return false; }
	virtual FNativeFunctionCall GenerateCustomCall(FNativeFunctionContext& Context, struct FStaticJITContext& JITContext) const { return FNativeFunctionCall{}; }
#if WITH_DEV_AUTOMATION_TESTS
	virtual FAngelscriptNativeFormDebugInfo GetDebugInfoForTesting() const = 0;
#endif

	static FScriptFunctionNativeForm* GetNativeForm(class asIScriptFunction* ScriptFunction);

	// Diagnostic — returns the number of native-form entries currently owned by
	// live engine states. Used by Memory.BindFreeEvidence regression tests to
	// assert the tables do not accumulate across engine cycles. Always returns 0
	// outside JIT-capable builds.
	static int32 NumNativeForms();
	static void BindNativeConstructor(FAngelscriptEngine& Engine, asIScriptFunction* ScriptFunction, const ANSICHAR* Name, bool bTrivial, const ANSICHAR* CustomForm = nullptr);
	static void BindNativeDestructor(FAngelscriptEngine& Engine, asIScriptFunction* ScriptFunction, const ANSICHAR* Name, bool bTrivial);
	static void BindNativeAssignment(FAngelscriptEngine& Engine, asIScriptFunction* ScriptFunction, const ANSICHAR* Name, bool bTrivial);
	static void BindNativeUObjectCast(FAngelscriptEngine& Engine, asIScriptFunction* ScriptFunction, const FString& TargetType, bool bGuaranteed);
	static void BindNativeMethod(FAngelscriptEngine& Engine, asIScriptFunction* ScriptFunction, const ANSICHAR* Name, bool bTrivial);
	static void BindNativeFunction(FAngelscriptEngine& Engine, asIScriptFunction* ScriptFunction, const ANSICHAR* Name, bool bTrivial);
	static void BindNativeFunctionHeader(FAngelscriptEngine& Engine, asIScriptFunction* ScriptFunction, const ANSICHAR* Name, bool bTrivial, const ANSICHAR* Header);
	static void BindUFunction(FAngelscriptEngine& Engine, asIScriptFunction* ScriptFunction, UFunction* Function, const FString& Name, bool bTrivial);
	static void BindTArrayIteratorCreate(FAngelscriptEngine& Engine, asIScriptFunction* ScriptFunction);
	static void BindTArrayIteratorProceed(FAngelscriptEngine& Engine, asIScriptFunction* ScriptFunction);
	static void BindTArrayIndex(FAngelscriptEngine& Engine, asIScriptFunction* ScriptFunction);
	static void BindTemplateInstantiatedCall(FAngelscriptEngine& Engine, asIScriptFunction* ScriptFunction, const ANSICHAR* Name, bool bTrivial, bool bNeedsCompare, bool bNeedsCopy);
	static void BindDelegateExecute(FAngelscriptEngine& Engine, asIScriptFunction* ScriptFunction);
	static void BindMulticastExecute(FAngelscriptEngine& Engine, asIScriptFunction* ScriptFunction);
	static void BindEventFunctionExecute(FAngelscriptEngine& Engine, asIScriptFunction* ScriptFunction);
	static void BindPushArg(FAngelscriptEngine& Engine, asIScriptFunction* ScriptFunction);
	static void BindPushArgRef(FAngelscriptEngine& Engine, asIScriptFunction* ScriptFunction);

	static const ANSICHAR* AllocateAnsiTypeName(const FString& TypeName);
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptNativeFormState
{
	~FAngelscriptNativeFormState();
	TMap<asIScriptFunction*, FScriptFunctionNativeForm*> Forms;
};

#endif // AS_CAN_GENERATE_JIT
