#pragma once
#include "CoreMinimal.h"
#include "StaticJITConfig.h"

#if AS_CAN_GENERATE_JIT

struct FAngelscriptBinds;
struct FNativeFunctionContext;

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

	static FScriptFunctionNativeForm* GetNativeForm(class asIScriptFunction* ScriptFunction);
	static void ReleaseAllNativeForms();

	static void BindNativeConstructor(FAngelscriptBinds& Binds, const ANSICHAR* Name, bool bTrivial, const ANSICHAR* CustomForm = nullptr);
	static void BindNativeDestructor(FAngelscriptBinds& Binds, const ANSICHAR* Name, bool bTrivial);
	static void BindNativeUObjectCast(FAngelscriptBinds& Binds, const FString& TargetType, bool bGuaranteed);
	static void BindNativeAssignment(FAngelscriptBinds& Binds, const ANSICHAR* Name, bool bTrivial);
	static void BindNativeMethod(FAngelscriptBinds& Binds, const ANSICHAR* Name, bool bTrivial);
	static void BindNativeFunction(const ANSICHAR* Name, bool bTrivial);
	static void BindNativeFunctionHeader(const ANSICHAR* Name, bool bTrivial, const ANSICHAR* Header);
	static void BindUFunction(class UFunction* Function, const FString& Name, bool bTrivial);

	static void BindTArrayIteratorCreate(FAngelscriptBinds& Binds);
	static void BindTArrayIteratorProceed(FAngelscriptBinds& Binds);
	static void BindTArrayIndex(FAngelscriptBinds& Binds);

	static void BindTemplateInstantiatedCall(FAngelscriptBinds& Binds, const ANSICHAR* Name, bool bTrivial, bool bNeedsCompare, bool bNeedsCopy);

	static void BindDelegateExecute();
	static void BindMulticastExecute();
	static void BindEventFunctionExecute();
	static void BindPushArg();
	static void BindPushArgRef();

	static const ANSICHAR* AllocateAnsiTypeName(const FString& TypeName);
};

#define SCRIPT_NATIVE_CONSTRUCTOR(Binds, Name) FScriptFunctionNativeForm::BindNativeConstructor(Binds, Name, false);
#define SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(Binds, Name) FScriptFunctionNativeForm::BindNativeConstructor(Binds, Name, true);
#define SCRIPT_TRIVIAL_NATIVE_DESTRUCTOR(Binds, Name) FScriptFunctionNativeForm::BindNativeDestructor(Binds, Name, true);
#define SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR_CUSTOMFORM(Binds, Name, CustomForm) FScriptFunctionNativeForm::BindNativeConstructor(Binds, Name, true, CustomForm);
#define SCRIPT_TRIVIAL_NATIVE_ASSIGNMENT(Binds, Name) FScriptFunctionNativeForm::BindNativeAssignment(Binds, Name, true);
#define SCRIPT_TRIVIAL_NATIVE_UOBJECT_CAST(Binds, TargetType, Guaranteed) FScriptFunctionNativeForm::BindNativeUObjectCast(Binds, TargetType, Guaranteed);
#define SCRIPT_NATIVE_METHOD(Binds, Name, Trivial) FScriptFunctionNativeForm::BindNativeMethod(Binds, Name, Trivial);
#define SCRIPT_NATIVE_FUNCTION(Name, Trivial) FScriptFunctionNativeForm::BindNativeFunction(Name, Trivial);
#define SCRIPT_NATIVE_FUNCTION_HEADER(Name, Trivial, Header) FScriptFunctionNativeForm::BindNativeFunctionHeader(Name, Trivial, Header);
#define SCRIPT_NATIVE_UFUNCTION(UnrealFunction, Name, Trivial) FScriptFunctionNativeForm::BindUFunction(UnrealFunction, Name, Trivial)
#define SCRIPT_NATIVE_TARRAY_INDEX(Binds) FScriptFunctionNativeForm::BindTArrayIndex(Binds)
#define SCRIPT_NATIVE_TARRAY_ITERATOR_CREATE(Binds) FScriptFunctionNativeForm::BindTArrayIteratorCreate(Binds)
#define SCRIPT_NATIVE_TARRAY_ITERATOR_PROCEED(Binds) FScriptFunctionNativeForm::BindTArrayIteratorProceed(Binds)
#define SCRIPT_NATIVE_DELEGATE_EXECUTE() FScriptFunctionNativeForm::BindDelegateExecute()
#define SCRIPT_NATIVE_MULTICAST_EXECUTE() FScriptFunctionNativeForm::BindMulticastExecute()
#define SCRIPT_NATIVE_PUSH_ARG() FScriptFunctionNativeForm::BindPushArg()
#define SCRIPT_NATIVE_PUSH_ARG_REF() FScriptFunctionNativeForm::BindPushArgRef()
#define SCRIPT_NATIVE_EVENT_FUNCTION_EXECUTE() FScriptFunctionNativeForm::BindEventFunctionExecute()
#define SCRIPT_NATIVE_TEMPLATED_CALL(Binds, Name, Trivial) FScriptFunctionNativeForm::BindTemplateInstantiatedCall(Binds, Name, Trivial, false, false)
#define SCRIPT_NATIVE_TEMPLATED_CALL_NEEDSCOMPARE(Binds, Name, Trivial) FScriptFunctionNativeForm::BindTemplateInstantiatedCall(Binds, Name, Trivial, true, false)
#define SCRIPT_NATIVE_TEMPLATED_CALL_NEEDSCOPY(Binds, Name, Trivial) FScriptFunctionNativeForm::BindTemplateInstantiatedCall(Binds, Name, Trivial, false, true)
#define SCRIPT_NATIVE_TEMPLATED_CALL_NEEDSCOPY_NEEDSCOMPARE(Binds, Name, Trivial) FScriptFunctionNativeForm::BindTemplateInstantiatedCall(Binds, Name, Trivial, true, true)

#else // AS_CAN_GENERATE_JIT

#define SCRIPT_NATIVE_CONSTRUCTOR(Binds, Name)
#define SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(Binds, Name)
#define SCRIPT_TRIVIAL_NATIVE_DESTRUCTOR(Binds, Name)
#define SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR_CUSTOMFORM(Binds, Name, CustomForm)
#define SCRIPT_TRIVIAL_NATIVE_ASSIGNMENT(Binds, Name)
#define SCRIPT_TRIVIAL_NATIVE_UOBJECT_CAST(Binds, TargetType, Guaranteed)
#define SCRIPT_NATIVE_METHOD(Binds, Name, Trivial)
#define SCRIPT_NATIVE_FUNCTION(Name, Trivial) 
#define SCRIPT_NATIVE_FUNCTION_HEADER(Name, Trivial, Header) 
#define SCRIPT_NATIVE_UFUNCTION(UnrealFunction)
#define SCRIPT_NATIVE_TARRAY_INDEX(Binds) 
#define SCRIPT_NATIVE_TARRAY_ITERATOR_CREATE(Binds) 
#define SCRIPT_NATIVE_TARRAY_ITERATOR_PROCEED(Binds) 
#define SCRIPT_NATIVE_TEMPLATED_CALL(Binds, Name, Trivial) 
#define SCRIPT_NATIVE_TEMPLATED_CALL_NEEDSCOMPARE(Binds, Name, Trivial) 
#define SCRIPT_NATIVE_TEMPLATED_CALL_NEEDSCOPY(Binds, Name, Trivial) 
#define SCRIPT_NATIVE_TEMPLATED_CALL_NEEDSCOPY_NEEDSCOMPARE(Binds, Name, Trivial) 

#define SCRIPT_NATIVE_DELEGATE_EXECUTE() 
#define SCRIPT_NATIVE_MULTICAST_EXECUTE() 
#define SCRIPT_NATIVE_PUSH_ARG() 
#define SCRIPT_NATIVE_PUSH_ARG_REF() 
#define SCRIPT_NATIVE_EVENT_FUNCTION_EXECUTE() 

#endif // AS_CAN_GENERATE_JIT