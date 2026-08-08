#pragma once

#include "CoreMinimal.h"
#include "AngelscriptEngine.h"
#include "AngelscriptType.h"
#include "Helper_CppType.h"

#include "StartAngelscriptHeaders.h"
//#include "as_context.h"
//#include "as_scriptfunction.h"
//#include "as_scriptengine.h"
//#include "as_objecttype.h"
#include "source/as_context.h"
#include "source/as_scriptfunction.h"
#include "source/as_scriptengine.h"
#include "source/as_objecttype.h"
#include "EndAngelscriptHeaders.h"

class FAngelscriptBindDatabase;

struct ANGELSCRIPTRUNTIME_API FAngelscriptDelegateOperations
{
	static void Construct(FScriptDelegate* Delegate)
	{
		new(Delegate) FScriptDelegate();
	}

	static void Destruct(FScriptDelegate* Delegate)
	{
		Delegate->~FScriptDelegate();
	}

	static void CopyConstruct(FScriptDelegate* Delegate, FScriptDelegate& Other)
	{
		new(Delegate) FScriptDelegate(Other);
	}

	static FScriptDelegate& Assign(FScriptDelegate* Delegate, FScriptDelegate& Other)
	{
		*Delegate = Other;
		return *Delegate;
	}

	static bool IsBound(FScriptDelegate* Delegate)
	{
		return Delegate->IsBound();
	}

	static UObject* GetUObject(FScriptDelegate* Delegate)
	{
		return Delegate->GetUObject();
	}

	static FName GetFunctionName(FScriptDelegate* Delegate)
	{
		return Delegate->GetFunctionName();
	}

	static void Clear(FScriptDelegate* Delegate)
	{
		return Delegate->Clear();
	}

	static void ConstructFromFunction(FScriptDelegate* Delegate, asCScriptFunction* Function, UObject* Object, const FName& FunctionName);
	static void ConstructFromFunction_Signature(FScriptDelegate* Delegate, UObject* Object, const FName& FunctionName, UDelegateFunction* Signature);
	static void BindUFunction(FScriptDelegate* Delegate, asCScriptFunction* Function, UObject* Object, const FName& FunctionName);
	static void BindUFunction_Signature(FScriptDelegate* Delegate, UObject* Object, const FName& FunctionName, UDelegateFunction* Signature);

	static UDelegateFunction* GetDelegateSignature(void* Ptr, int TypeId)
	{
		asCObjectType* Type = (asCObjectType*)FAngelscriptEngine::Get().Engine->GetTypeInfoById(TypeId);
		checkSlow(Type);
		checkSlow(Type->GetFlags() & asOBJ_VALUE);
		return (UDelegateFunction*)Type->plainUserData;
	}
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptMulticastDelegateOperations
{
	static void Construct(FMulticastScriptDelegate* Delegate)
	{
		new(Delegate) FMulticastScriptDelegate();
	}

	static void Destruct(FMulticastScriptDelegate* Delegate)
	{
		Delegate->~FMulticastScriptDelegate();
	}

	static void CopyConstruct(FMulticastScriptDelegate* Delegate, FMulticastScriptDelegate& Other)
	{
		new(Delegate) FMulticastScriptDelegate(Other);
	}

	static FMulticastScriptDelegate& Assign(FMulticastScriptDelegate* Delegate, FMulticastScriptDelegate& Other)
	{
		*Delegate = Other;
		return *Delegate;
	}

	static bool IsBound(FMulticastScriptDelegate* Delegate)
	{
		return Delegate->IsBound();
	}

	static void Clear(FMulticastScriptDelegate* Delegate)
	{
		return Delegate->Clear();
	}

	static void AddUFunction(FMulticastScriptDelegate* Delegate, asCScriptFunction* Function, UObject* Object, const FName& FunctionName);
	static void AddUFunction_Signature(FMulticastScriptDelegate* Delegate, UObject* Object, const FName& FunctionName, UDelegateFunction* Signature);

	static void Unbind(FMulticastScriptDelegate* Delegate, UObject* Object, const FName& InFunctionName)
	{
		Delegate->Remove(Object, InFunctionName);
	}

	static void UnbindObject(FMulticastScriptDelegate* Delegate, UObject* Object)
	{
		Delegate->RemoveAll(Object);
	}
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptSparseDelegateOperations
{
	static void Construct(FSparseDelegate* Delegate);
	static void Destruct(FSparseDelegate* Delegate);
	static bool IsBound(FSparseDelegate* Delegate);
	static void Clear(FSparseDelegate* Delegate, asCScriptFunction* ScriptFunction);
	static void AddUFunction(FSparseDelegate* Delegate, asCScriptFunction* ScriptFunction, UObject* Object, const FName& FunctionName);
	static void Unbind(FSparseDelegate* Delegate, asCScriptFunction* ScriptFunction, UObject* Object, const FName& FunctionName);
	static void UnbindObject(FSparseDelegate* Delegate, asCScriptFunction* ScriptFunction, UObject* Object, const FName& FunctionName);
};

struct FScriptDelegateType : TAngelscriptCppType<FScriptDelegate>
{
	FString Name;
	UDelegateFunction* Function;
	const FAngelscriptBindDatabase* BindDatabase;

	FScriptDelegateType(
		const FString& InName,
		UDelegateFunction* InFunction,
		const FAngelscriptBindDatabase& InBindDatabase);

	explicit FScriptDelegateType(const FAngelscriptBindDatabase& InBindDatabase);

	UDelegateFunction* GetSignature(const FAngelscriptTypeUsage& Usage) const;

	UDelegateFunction* GetSignatureMaybeTagged(const FAngelscriptTypeUsage& Usage) const;

	bool IsTypeEquivalent(const FAngelscriptTypeUsage& Usage, const FAngelscriptTypeUsage& Other) const override;

	void* GetData() const override;

	FString GetAngelscriptTypeName() const override;

	bool CanCreateProperty(const FAngelscriptTypeUsage& Usage) const override;

	FProperty* CreateProperty(const FAngelscriptTypeUsage& Usage, const FPropertyParams& Params) const override;

	bool CanBeArgument(const FAngelscriptTypeUsage& Usage) const override;

	void SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FArgData& Data) const override;

	bool CanBeReturned(const FAngelscriptTypeUsage& Usage) const override;

	void GetReturnValue(const FAngelscriptTypeUsage& Usage, class asIScriptContext* Context, void* Destination) const override;

	bool CanQueryPropertyType() const override;

	bool MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const override;

	bool DefaultValue_AngelscriptFallback(const FAngelscriptTypeUsage& Usage, FString& OutAngelscriptValue) const;

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;

	bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const override;

	bool GetDebuggerScope(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerScope& Scope) const override;

	bool GetDebuggerMember(const FAngelscriptTypeUsage& Usage, void* Address, const FString& Member, struct FDebuggerValue& Value) const override;
};

struct FMulticastScriptDelegateType : TAngelscriptCppType<FMulticastScriptDelegate>
{
	FString Name;
	UDelegateFunction* Function;
	const FAngelscriptBindDatabase* BindDatabase;

	FMulticastScriptDelegateType(
		const FString& InName,
		UDelegateFunction* InFunction,
		const FAngelscriptBindDatabase& InBindDatabase);

	explicit FMulticastScriptDelegateType(const FAngelscriptBindDatabase& InBindDatabase);

	UDelegateFunction* GetSignature(const FAngelscriptTypeUsage& Usage) const;

	UDelegateFunction* GetSignatureMaybeTagged(const FAngelscriptTypeUsage& Usage) const;

	bool IsTypeEquivalent(const FAngelscriptTypeUsage& Usage, const FAngelscriptTypeUsage& Other) const override;

	void* GetData() const override;

	FString GetAngelscriptTypeName() const override;

	bool CanCreateProperty(const FAngelscriptTypeUsage& Usage) const override;

	FProperty* CreateProperty(const FAngelscriptTypeUsage& Usage, const FPropertyParams& Params) const override;

	bool CanBeArgument(const FAngelscriptTypeUsage& Usage) const override;

	void SetArgument(const FAngelscriptTypeUsage& Usage, int32 ArgumentIndex, class asIScriptContext* Context, struct FFrame& Stack, const FArgData& Data) const override;

	bool CanBeReturned(const FAngelscriptTypeUsage& Usage) const override;

	void GetReturnValue(const FAngelscriptTypeUsage& Usage, class asIScriptContext* Context, void* Destination) const override;

	bool CanQueryPropertyType() const override;

	bool MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const override;

	bool DefaultValue_AngelscriptFallback(const FAngelscriptTypeUsage& Usage, FString& OutAngelscriptValue) const;

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;

	bool GetDebuggerValue(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerValue& Value) const override;

	struct FMulticastScriptDelegateBinding
	{
		UObject* Object;
		FString FunctionName;
	};

	bool GetBindings(const FMulticastScriptDelegate& Delegate, TArray<FMulticastScriptDelegateBinding>& OutBindings) const;

	bool GetDebuggerScope(const FAngelscriptTypeUsage& Usage, void* Address, struct FDebuggerScope& Scope) const override;

	bool GetDebuggerMember(const FAngelscriptTypeUsage& Usage, void* Address, const FString& Member, struct FDebuggerValue& Value) const override;
};

struct FScriptSparseDelegateType : public FAngelscriptType
{
	FString Name;
	USparseDelegateFunction* Function;

	FScriptSparseDelegateType(const FString& InName, USparseDelegateFunction* InFunction);

	FScriptSparseDelegateType();

	USparseDelegateFunction* GetSignature(const FAngelscriptTypeUsage& Usage) const;

	void* GetData() const override;

	FString GetAngelscriptTypeName() const override;

	bool CanCreateProperty(const FAngelscriptTypeUsage& Usage) const override;

	FProperty* CreateProperty(const FAngelscriptTypeUsage& Usage, const FPropertyParams& Params) const override;

	bool CanBeArgument(const FAngelscriptTypeUsage& Usage) const override;

	bool CanBeReturned(const FAngelscriptTypeUsage& Usage) const override;

	bool CanQueryPropertyType() const override;

	bool MatchesProperty(const FAngelscriptTypeUsage& Usage, const FProperty* Property, EPropertyMatchType MatchType) const override;

	bool CanCopy(const FAngelscriptTypeUsage& Usage) const override;
	bool CanCompare(const FAngelscriptTypeUsage& Usage) const override;

	bool CanConstruct(const FAngelscriptTypeUsage& Usage) const override;
	bool NeedConstruct(const FAngelscriptTypeUsage& Usage) const override;

	bool CanDestruct(const FAngelscriptTypeUsage& Usage) const override;
	bool NeedDestruct(const FAngelscriptTypeUsage& Usage) const override;

	int32 GetValueSize(const FAngelscriptTypeUsage& Usage) const override;

	int32 GetValueAlignment(const FAngelscriptTypeUsage& Usage) const;
};
