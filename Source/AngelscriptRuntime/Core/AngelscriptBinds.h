#pragma once

#include "CoreMinimal.h"
#include "Misc/FileHelper.h"
#include "UObject/Class.h"

// Unfortunately we need Angelscript here, since we need access
// to asSMethodPtr to make the template magic work.
// Dependency inversion on the bind event means we 
// don't really include this file outside the Binds folder
// though, thankfully.
#include "AngelscriptInclude.h"
#include "AngelscriptBindDatabase.h"
#include "AngelscriptBindString.h"
#include "AngelscriptType.h"
#include "StaticJIT/StaticJITBinds.h"
#include "FunctionCallers.h"

// Need to disable some casting warnings. Trust us MSVC we know what we're doing
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4191)
#endif

#ifdef _MSC_VER
#define CDECL __cdecl
#else
#define CDECL
#endif

#ifndef AS_FORCE_LINK
#if defined(__GNUC__) || defined(__clang__)
#define AS_FORCE_LINK [[gnu::used, gnu::retain]]
#else
#define AS_FORCE_LINK
#endif
#endif

#if AS_CAN_GENERATE_JIT
#define METHODPR(Ret,Cls,Name,Args) ((Ret(Cls::*)Args)&Cls::Name), #Cls, #Name, false
#define METHODPR_TRIVIAL(Ret,Cls,Name,Args) ((Ret(Cls::*)Args)&Cls::Name), #Cls, #Name, true
#define METHOD(Cls,Name) (&Cls::Name), #Cls, #Name, false
#define METHOD_TRIVIAL(Cls,Name) (&Cls::Name), #Cls, #Name, true
#define FUNCPR(Ret,Name,Args) ((Ret(*)Args)&Name), #Name, false
#define FUNCPR_TRIVIAL(Ret,Name,Args) ((Ret(*)Args)&Name), #Name, true
#define FUNC(Name) (&Name), #Name, false
#define FUNC_TRIVIAL(Name) (&Name), #Name, true
#define FUNC_CUSTOMNATIVE(Name, CustomNative) (&Name), #CustomNative, false
#define FUNC_TRIVIAL_CUSTOMNATIVE(Name, CustomNative) (&Name), #CustomNative, true
#else
#define METHODPR(Ret,Cls,Name,Args) ((Ret(Cls::*)Args)&Cls::Name)
#define METHODPR_TRIVIAL(Ret,Cls,Name,Args) ((Ret(Cls::*)Args)&Cls::Name)
#define METHOD(Cls,Name) (&Cls::Name)
#define METHOD_TRIVIAL(Cls,Name) (&Cls::Name)
#define FUNCPR(Ret,Name,Args) ((Ret(*)Args)&Name)
#define FUNCPR_TRIVIAL(Ret,Name,Args) ((Ret(*)Args)&Name)
#define FUNC(Name) (&Name)
#define FUNC_TRIVIAL(Name) (&Name)
#define FUNC_CUSTOMNATIVE(Name, CustomNative) (&Name)
#define FUNC_TRIVIAL_CUSTOMNATIVE(Name, CustomNative) (&Name)
#endif

struct FAngelscriptBinds;
struct FAngelscriptEngine;
struct FToStringType;
class FBlueprintEventSignatureRegistry;

enum class EAngelscriptBindPhase : uint8
{
	TypeDeclarations,
	TypeInfrastructure,
	ExplicitBindings,
	GeneratedBindings,
	ReflectionBindings,
	PostReflectionBindings,
	Finalization,
};

using FAngelscriptBindCallback = void (*)(FAngelscriptBinds&);

namespace UE::Angelscript::Private
{
	class FAngelscriptBindCollection;
}

struct FAngelscriptBindMetadata
{
	FName OwnerModule;
	FName BindName;
	EAngelscriptBindPhase Phase = EAngelscriptBindPhase::ExplicitBindings;
	FString SourceFile;
	int32 SourceLine = 0;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptBind
{
	FAngelscriptBind(
		FName BindName,
		EAngelscriptBindPhase Phase,
		FAngelscriptBindCallback Callback,
		const ANSICHAR* OwnerModule = UE_MODULE_NAME,
		const ANSICHAR* SourceFile = __builtin_FILE(),
		int32 SourceLine = __builtin_LINE());

	static bool FinalizeRegisteredBinds(FString& OutDiagnostic);
	static bool PrepareForEngineInitialization(FString& OutDiagnostic);
	static bool ExecuteRegisteredBinds(FAngelscriptBinds& Binds, FString& OutDiagnostic);
	static bool ExecuteRegisteredBindPhases(
		FAngelscriptBinds& Binds,
		EAngelscriptBindPhase FirstPhase,
		EAngelscriptBindPhase LastPhase,
		FString& OutDiagnostic);
	static TArray<FAngelscriptBindMetadata> GetRegisteredBindMetadata();

#if WITH_DEV_AUTOMATION_TESTS
	FAngelscriptBind(
		UE::Angelscript::Private::FAngelscriptBindCollection& Collection,
		FName BindName,
		EAngelscriptBindPhase Phase,
		FAngelscriptBindCallback Callback,
		const ANSICHAR* OwnerModule = UE_MODULE_NAME,
		const ANSICHAR* SourceFile = __builtin_FILE(),
		int32 SourceLine = __builtin_LINE());

	static void ResetPrepareInvocationCountForTesting();
	static int32 GetPrepareInvocationCountForTesting();
	static const void* GetRegisteredCollectionIdentityForTesting();
	static int32 GetRegisteredBindCountForTesting();
	static bool IsRegisteredCollectionSealedForTesting();
#endif
};



/* Template metamagic for determining function pointer type from lambda. */
template<class> struct TRemoveFuncConst;
template<class Value, class... Args>
struct TRemoveFuncConst<Value(Args...)>
{
	using Type = Value(CDECL *)(Args...);
};

template<class Value, class... Args>
struct TRemoveFuncConst<Value(Args...) const>
{
	using Type = Value(CDECL *)(Args...);
};

template< class > struct TRemoveMethodPtr;
template< class C, class T > struct TRemoveMethodPtr< T C::* >
{
	typedef typename TRemoveFuncConst<T>::Type Type;
};

template< class T > struct TLambdaFuncPtr
{
	typedef typename TRemoveMethodPtr< decltype(&T::operator()) >::Type Type;
};


struct FBindFlags
{
	bool bPOD = false;
	bool bTemplate = false;
	FBindString TemplateType;
	int Alignment = -1;
	asQWORD ExtraFlags = 0;
};

struct FSystemFunctionArgument
{
	FBindString Name;
	FBindString DefaultValue;
	FAngelscriptTypeUsage Type;
	bool bReference = false;
	bool bConst = false;
};

struct FSystemFunctionBind
{
	FBindString Name;
	FAngelscriptTypeUsage ReturnType;
	TArray<FSystemFunctionArgument, TInlineAllocator<4>> Arguments;
	bool bConst = false;
	FBindString Namespace;
	TSharedPtr<FAngelscriptType> ObjectType;
};

struct FAngelscriptRegisteredFunctionProvenance
{
	EAngelscriptFunctionBindingOrigin Origin =
		EAngelscriptFunctionBindingOrigin::Unknown;
	FName Provider;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptBindState
{
	TMap<UClass*, TMap<FString, FAngelscriptFunctionBinding>> ClassFunctionBindings;
	TMap<FString, TArray<TObjectPtr<UClass>>> RuntimeClassDB;
#if WITH_EDITOR
	TMap<FString, TArray<TObjectPtr<UClass>>> EditorClassDB;
#endif
	TArray<TObjectPtr<UClass>> BlueprintTypeClassSnapshot;
	bool bBlueprintTypeClassSnapshotCaptured = false;
	TArray<TObjectPtr<UScriptStruct>> UStructTypeSnapshot;
	bool bUStructTypeSnapshotCaptured = false;
	TMap<UClass*, TSet<FString>> SkipBinds;
	TSet<TTuple<FName, FName>> SkipBindNames;
	TSet<FName> SkipBindClasses;
	TMap<int32, FAngelscriptRegisteredFunctionProvenance> FunctionProvenance;
	TMap<const asIScriptFunction*, FAngelscriptRegisteredFunctionProvenance>
		FunctionProvenanceByPointer;
	FName ActiveBindOwnerModule;
	FName ActiveBindProvider;
	EAngelscriptBindPhase ActiveBindPhase = EAngelscriptBindPhase::TypeDeclarations;
	const ANSICHAR* ActiveBindSourceFile = nullptr;
	int32 ActiveBindSourceLine = 0;
	bool bDirectBindFailed = false;
	FString DirectBindFailureDiagnostic;
#if WITH_DEV_AUTOMATION_TESTS
	int32 DirectCallbackExecutionCountForTesting = 0;
#endif
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptBoundFunction
{
	FAngelscriptBoundFunction() = default;
	FAngelscriptBoundFunction(FAngelscriptEngine* InTargetEngine, int32 InFunctionId)
		: TargetEngine(InTargetEngine)
		, FunctionId(InFunctionId)
	{
	}

	bool IsValid() const;
	FAngelscriptEngine& GetTargetEngine() const;
	asIScriptFunction* GetFunction() const;
	int32 GetFunctionId() const { return FunctionId; }
	operator int32() const { return FunctionId; }

	FAngelscriptBoundFunction& EditorOnly(bool bEditorOnly = true);
	FAngelscriptBoundFunction& Deprecated(const ANSICHAR* DeprecationMessage);
	FAngelscriptBoundFunction& PropertyAccessor(bool bPropertyAccessor = true);
	FAngelscriptBoundFunction& GeneratedAccessor(bool bGeneratedAccessor = true);
	FAngelscriptBoundFunction& NoDiscard(bool bNoDiscard = true);
	FAngelscriptBoundFunction& WorldContext(bool bRequiresWorldContext = true);
	FAngelscriptBoundFunction& Callable(bool bCallable = true);
	FAngelscriptBoundFunction& ImplicitConstructor(bool bImplicitConstructor = true);
	FAngelscriptBoundFunction& ForceConstArgumentExpressions(bool bForceConst = true);
	FAngelscriptBoundFunction& DeterminesOutputType(int32 ArgumentIndex);
	FAngelscriptBoundFunction& PassScriptFunctionAsFirstParam();
	FAngelscriptBoundFunction& PassScriptObjectTypeAsFirstParam();
	FAngelscriptBoundFunction& Documentation(FStringView Documentation, FStringView Category = FStringView(), UFunction* UnrealFunction = nullptr);
	FAngelscriptBoundFunction& NativeConstructor(const ANSICHAR* Name, bool bTrivial = false, const ANSICHAR* CustomForm = nullptr);
	FAngelscriptBoundFunction& NativeDestructor(const ANSICHAR* Name, bool bTrivial = false);
	FAngelscriptBoundFunction& NativeAssignment(const ANSICHAR* Name, bool bTrivial = false);
	FAngelscriptBoundFunction& NativeUObjectCast(const FString& TargetType, bool bGuaranteed);
	FAngelscriptBoundFunction& NativeMethod(const ANSICHAR* Name, bool bTrivial = false);
	FAngelscriptBoundFunction& NativeFunction(const ANSICHAR* Name, bool bTrivial = false);
	FAngelscriptBoundFunction& NativeFunctionHeader(const ANSICHAR* Name, const ANSICHAR* Header, bool bTrivial = false);
	FAngelscriptBoundFunction& NativeUFunction(UFunction* Function, const FString& Name, bool bTrivial = false);
	FAngelscriptBoundFunction& NativeTArrayIndex();
	FAngelscriptBoundFunction& NativeTArrayIteratorCreate();
	FAngelscriptBoundFunction& NativeTArrayIteratorProceed();
	FAngelscriptBoundFunction& NativeTemplateInstantiatedCall(const ANSICHAR* Name, bool bTrivial = false, bool bNeedsCompare = false, bool bNeedsCopy = false);
	FAngelscriptBoundFunction& NativeDelegateExecute();
	FAngelscriptBoundFunction& NativeMulticastExecute();
	FAngelscriptBoundFunction& NativeEventFunctionExecute();
	FAngelscriptBoundFunction& NativePushArgument();
	FAngelscriptBoundFunction& NativePushArgumentRef();
	FAngelscriptBoundFunction& CompileOutEntirely();
	FAngelscriptBoundFunction& CompileOutAsMethodChain();
	FAngelscriptBoundFunction& CompileOutInTest();
	FAngelscriptBoundFunction& CompileOutIfNoLog();
	FAngelscriptBoundFunction& CompileOutAsEnsure();
	FAngelscriptBoundFunction& CompileOutAsCheck();
	FAngelscriptBoundFunction& ReplaceWithFirstArgInTest();

private:
	FAngelscriptEngine* TargetEngine = nullptr;
	int32 FunctionId = asERROR;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptBoundProperty
{
	FAngelscriptBoundProperty() = default;
	FAngelscriptBoundProperty(FAngelscriptEngine* InTargetEngine, int32 InPropertyId, bool bInGlobal)
		: TargetEngine(InTargetEngine)
		, PropertyId(InPropertyId)
		, bGlobal(bInGlobal)
	{
	}

	bool IsValid() const;
	FAngelscriptEngine& GetTargetEngine() const;
	int32 GetPropertyId() const { return PropertyId; }
	operator int32() const { return PropertyId; }

	FAngelscriptBoundProperty& PureConstant(asQWORD ConstantValue);

private:
	FAngelscriptEngine* TargetEngine = nullptr;
	int32 PropertyId = asERROR;
	bool bGlobal = false;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptBinds
{
	explicit FAngelscriptBinds(FAngelscriptEngine& InTargetEngine);

	FAngelscriptEngine& GetTargetEngine() const;
	asIScriptEngine& GetTargetScriptEngine() const;
	FAngelscriptBindState& GetTargetBindState() const;
	FAngelscriptTypeDatabase& GetTargetTypeDatabase() const;
	FAngelscriptBindDatabase& GetTargetBindDatabase() const;
	TArray<FToStringType>& GetTargetToStringList() const;
	FBlueprintEventSignatureRegistry& GetTargetBlueprintEventSignatureRegistry() const;
	void RegisterTypeForTarget(TSharedRef<FAngelscriptType> Type) const;
	void RegisterTypeFinderForTarget(FAngelscriptType::FTypeFinder Finder) const;
	void RegisterFunctionBindingForTarget(UClass* Class, const FString& Name, const FAngelscriptFunctionBinding& Binding) const;
	void RegisterGeneratedFunctionBindingForTarget(
		UClass* Class,
		const FString& Name,
		FAngelscriptFunctionBinding Binding) const;
	bool HasRegistrationFailure() const;
	const FString& GetRegistrationFailureDiagnostic() const;

	template<typename T>
	FAngelscriptBinds ValueClassForTarget(FBindString Name, FBindFlags Flags)
	{
		if (Flags.Alignment == -1)
		{
			Flags.Alignment = alignof(T);
		}
		Flags.ExtraFlags |= asGetTypeTraits<T>();
		return ValueClassForTarget(Name, Flags, sizeof(T));
	}

	FAngelscriptBinds ValueClassForTarget(FBindString Name, UScriptStruct* StructType, FBindFlags Flags)
	{
		int32 Size;
		UScriptStruct::ICppStructOps* Ops = StructType->GetCppStructOps();
		if (Ops != nullptr)
		{
			Size = Ops->GetSize();
		}
		else
		{
			Size = StructType->GetPropertiesSize();
		}
		if (Flags.Alignment == -1)
		{
			Flags.Alignment = StructType->GetMinAlignment();
		}
		return ValueClassForTarget(Name, Flags, Size);
	}

	FAngelscriptBinds ValueClassForTarget(FBindString Name, SIZE_T Size, FBindFlags Flags)
	{
		Flags.ExtraFlags |= asOBJ_APP_CLASS_CDAK;
		return ValueClassForTarget(Name, Flags, static_cast<int32>(Size));
	}

	template<typename Value, typename... Args>
	FAngelscriptBoundFunction BindGlobalFunctionForTarget(FBindString Signature, Value (*Function)(Args...), void* UserData = nullptr)
	{
		return BindGlobalFunctionForTarget(Signature, asFUNCTION(Function), ASAutoCaller::MakeFunctionCaller(Function), UserData);
	}

	template<typename T>
	FAngelscriptBoundFunction BindGlobalFunctionForTarget(FBindString Signature, T Function, void* UserData = nullptr)
	{
		auto FunctionPointer = (typename TLambdaFuncPtr<T>::Type)Function;
		return BindGlobalFunctionForTarget(
			Signature,
			asFUNCTION(FunctionPointer),
			ASAutoCaller::MakeFunctionCaller(FunctionPointer),
			UserData);
	}

	template<typename Value, typename... Args>
	FAngelscriptBoundFunction BindGlobalFunctionForTarget(
		FBindString Signature,
		Value (*Function)(Args...),
		FBindString FunctionName,
		bool bTrivial,
		void* UserData = nullptr)
	{
		FAngelscriptBoundFunction Result = BindGlobalFunctionForTarget(
			Signature,
			asFUNCTION(Function),
			ASAutoCaller::MakeFunctionCaller(Function),
			UserData);
		return Result.NativeFunction(FunctionName.ToCString_EnsureConstant(), bTrivial);
	}

	FAngelscriptBoundFunction BindGlobalGenericFunctionForTarget(
		FBindString Signature,
		void(CDECL* Function)(asIScriptGeneric*),
		void* UserData = nullptr);
	FAngelscriptBoundFunction BindGlobalFunctionDirectForTarget(
		FBindString Signature,
		asSFuncPtr Function,
		asECallConvTypes CallConv,
		ASAutoCaller::FunctionCaller Caller,
		void* UserData = nullptr);
	FAngelscriptBoundFunction BindMethodDirectForTarget(
		FBindString ObjectTypeName,
		FBindString Signature,
		asSFuncPtr Function,
		asECallConvTypes CallConv,
		ASAutoCaller::FunctionCaller Caller,
		void* UserData = nullptr);

	FAngelscriptBoundProperty BindGlobalVariableForTarget(FBindString Signature, const void* Address);

	/*
	* Class-specific binding.
	*/
	FAngelscriptBinds ReferenceClassForTarget(FBindString Name, UClass* UnrealClass) const;
	FAngelscriptBinds ExistingClassForTarget(FBindString Name) const;

	template<typename C, typename Value, typename... Args>
	inline FAngelscriptBoundFunction Method(FBindString Signature, Value(C::*Fun)(Args...), void* UserData = nullptr)
	{
		return BindMethod(Signature, asSMethodPtr<sizeof(void(C::*)())>::Convert((void(C::*)())(Fun)), ASAutoCaller::MakeFunctionCaller(Fun), UserData);
	}

	template<typename C, typename Value, typename... Args>
	inline FAngelscriptBoundFunction Method(FBindString Signature, Value(C::*Fun)(Args...)const, void* UserData = nullptr)
	{
		return BindMethod(Signature, asSMethodPtr<sizeof(void(C::*)())>::Convert((void(C::*)())(Fun)), ASAutoCaller::MakeFunctionCaller(Fun), UserData);
	}

	template<typename C, typename Value, typename... Args>
	inline FAngelscriptBoundFunction Method(FBindString Signature, Value(C::*Fun)(Args...)const&, void* UserData = nullptr)
	{
		return BindMethod(Signature, asSMethodPtr<sizeof(void(C::*)())>::Convert((void(C::*)())(Fun)), ASAutoCaller::MakeFunctionCaller(Fun), UserData);
	}

	template<typename Value, typename... Args>
	inline FAngelscriptBoundFunction Method(FBindString Signature, Value(CDECL *fun)(Args...), void* UserData = nullptr)
	{
		return BindExternMethod(Signature, asFUNCTION(fun), ASAutoCaller::MakeFunctionCaller(fun), UserData);
	}

	template<typename C, typename Value, typename... Args>
	inline FAngelscriptBoundFunction Method(FBindString Signature, Value(C::*Fun)(Args...)const, FBindString MethodClass, FBindString MethodName, bool bTrivial, void* UserData = nullptr)
	{
		FAngelscriptBoundFunction Result = BindMethod(Signature, asSMethodPtr<sizeof(void(C::*)())>::Convert((void(C::*)())(Fun)), ASAutoCaller::MakeFunctionCaller(Fun), UserData);
		return Result.NativeMethod(MethodName.ToCString_EnsureConstant(), bTrivial);
	}

	template<typename C, typename Value, typename... Args>
	inline FAngelscriptBoundFunction Method(FBindString Signature, Value(C::*Fun)(Args...), FBindString MethodClass, FBindString MethodName, bool bTrivial, void* UserData = nullptr)
	{
		FAngelscriptBoundFunction Result = BindMethod(Signature, asSMethodPtr<sizeof(void(C::*)())>::Convert((void(C::*)())(Fun)), ASAutoCaller::MakeFunctionCaller(Fun), UserData);
		return Result.NativeMethod(MethodName.ToCString_EnsureConstant(), bTrivial);
	}

	template<typename C, typename Value, typename... Args>
	inline FAngelscriptBoundFunction Method(FBindString Signature, Value(C::*Fun)(Args...)const&, FBindString MethodClass, FBindString MethodName, bool bTrivial, void* UserData = nullptr)
	{
		FAngelscriptBoundFunction Result = BindMethod(Signature, asSMethodPtr<sizeof(void(C::*)())>::Convert((void(C::*)())(Fun)), ASAutoCaller::MakeFunctionCaller(Fun), UserData);
		return Result.NativeMethod(MethodName.ToCString_EnsureConstant(), bTrivial);
	}

	template<typename Value, typename... Args>
	inline FAngelscriptBoundFunction Method(FBindString Signature, Value(CDECL *fun)(Args...), FBindString FuncName, bool bTrivial, void* UserData = nullptr)
	{
		FAngelscriptBoundFunction Result = BindExternMethod(Signature, asFUNCTION(fun), ASAutoCaller::MakeFunctionCaller(fun), UserData);
		return Result.NativeFunction(FuncName.ToCString_EnsureConstant(), bTrivial);
	}

	template<typename Value, typename... Args>
	inline FAngelscriptBoundFunction Method(FBindString Signature, Value(CDECL *fun)(Args...), FBindString FuncName, bool bTrivial, const FAngelscriptType::FBindParams& BindParams, void* UserData = nullptr)
	{
		FAngelscriptBoundFunction Result = BindExternMethod(Signature, asFUNCTION(fun), BindParams, ASAutoCaller::MakeFunctionCaller(fun), UserData);
		return Result.NativeFunction(FuncName.ToCString_EnsureConstant(), bTrivial);
	}

	template<typename Value, typename... Args>
	inline FAngelscriptBoundFunction Method(FBindString Signature, Value(CDECL *fun)(Args...), const FAngelscriptType::FBindParams& BindParams, void* UserData = nullptr)
	{
		return BindExternMethod(Signature, asFUNCTION(fun), BindParams, ASAutoCaller::MakeFunctionCaller(fun), UserData);
	}

	template<typename T>
	inline FAngelscriptBoundFunction Method(FBindString Signature, T Function, void* UserData = nullptr)
	{
		auto FunctionPointer = (typename TLambdaFuncPtr<T>::Type)Function;
		return BindExternMethod(Signature, asFUNCTION(FunctionPointer), ASAutoCaller::MakeFunctionCaller(FunctionPointer), UserData);
	}

	template<typename T>
	inline FAngelscriptBoundFunction Method(FBindString Signature, T Function, const FAngelscriptType::FBindParams& BindParams, void* UserData = nullptr)
	{
		auto FunctionPointer = (typename TLambdaFuncPtr<T>::Type)Function;
		return BindExternMethod(Signature, asFUNCTION(FunctionPointer), BindParams, ASAutoCaller::MakeFunctionCaller(FunctionPointer), UserData);
	}

	FAngelscriptBoundFunction GenericMethod(FBindString Signature, void(CDECL *Fun)(asIScriptGeneric*), void* UserData);

	template<typename T>
	inline FAngelscriptBoundFunction GenericMethod(FBindString Signature, T Function, void* UserData)
	{
		return GenericMethod(Signature, (typename TLambdaFuncPtr<T>::Type)Function, UserData);
	}

	template<typename C, typename T>
	inline FAngelscriptBoundProperty Property(FBindString Signature, T C::*Ptr)
	{
		return BindProperty(Signature, (size_t)&(((C*)nullptr)->*Ptr));
	}

	inline FAngelscriptBoundProperty Property(FBindString Signature, size_t Offset)
	{
		return BindProperty(Signature, Offset);
	}

	inline FAngelscriptBoundProperty Property(FBindString Signature, size_t Offset, const FAngelscriptType::FBindParams& BindParams)
	{
		return BindProperty(Signature, Offset, BindParams);
	}

	template<typename Value, typename... Args>
	inline FAngelscriptBoundFunction Factory(FBindString Signature, Value(CDECL *Fun)(Args...), void* UserData = nullptr)
	{
		return BindStaticBehaviour(asBEHAVE_FACTORY, Signature, asFUNCTION(Fun), ASAutoCaller::MakeFunctionCaller(Fun), UserData);
	}

	template<typename Value, typename... Args>
	inline FAngelscriptBoundFunction Destructor(FBindString Signature, Value(CDECL *Fun)(Args...), void* UserData = nullptr)
	{
		return BindExternBehaviour(asBEHAVE_DESTRUCT, Signature, asFUNCTION(Fun), ASAutoCaller::MakeFunctionCaller(Fun), UserData);
	}

	template<typename Value, typename... Args>
	inline FAngelscriptBoundFunction Destructor(FBindString Signature, Value(CDECL *Fun)(Args...), FBindString FuncName, bool bTrivial, void* UserData = nullptr)
	{
		FAngelscriptBoundFunction Result = BindExternBehaviour(asBEHAVE_DESTRUCT, Signature, asFUNCTION(Fun), ASAutoCaller::MakeFunctionCaller(Fun), UserData);
		return Result.NativeDestructor(FuncName.ToCString_EnsureConstant(), bTrivial);
	}

	template<typename T>
	inline FAngelscriptBoundFunction Destructor(FBindString Signature, T Function, void* UserData = nullptr)
	{
		return Destructor(Signature, (typename TLambdaFuncPtr<T>::Type)Function, UserData);
	}

	template<typename T>
	inline FAngelscriptBoundFunction Factory(FBindString Signature, T Function, void* UserData = nullptr)
	{
		return Factory(Signature, (typename TLambdaFuncPtr<T>::Type)Function, UserData);
	}

	template<typename Value, typename... Args>
	inline FAngelscriptBoundFunction Constructor(FBindString Signature, Value(CDECL *Fun)(Args...), void* UserData = nullptr)
	{
		return BindExternBehaviour(asBEHAVE_CONSTRUCT, Signature, asFUNCTION(Fun), ASAutoCaller::MakeFunctionCaller(Fun), UserData);
	}

	template<typename Value, typename... Args>
	inline FAngelscriptBoundFunction Constructor(FBindString Signature, Value(CDECL *Fun)(Args...), FBindString FuncName, bool bTrivial, void* UserData = nullptr)
	{
		FAngelscriptBoundFunction Result = BindExternBehaviour(asBEHAVE_CONSTRUCT, Signature, asFUNCTION(Fun), ASAutoCaller::MakeFunctionCaller(Fun), UserData);
		return Result.NativeConstructor(FuncName.ToCString_EnsureConstant(), bTrivial);
	}

	template<typename T>
	inline FAngelscriptBoundFunction Constructor(FBindString Signature, T Function, void* UserData = nullptr)
	{
		return Constructor(Signature, (typename TLambdaFuncPtr<T>::Type)Function, UserData);
	}

	template<typename Value, typename... Args>
	inline FAngelscriptBoundFunction ImplicitConstructor(FBindString Signature, Value(CDECL *Fun)(Args...), void* UserData = nullptr)
	{
		return BindExternBehaviour(asBEHAVE_CONSTRUCT, Signature, asFUNCTION(Fun), ASAutoCaller::MakeFunctionCaller(Fun), UserData).ImplicitConstructor();
	}

	template<typename Value, typename... Args>
	inline FAngelscriptBoundFunction ImplicitConstructor(FBindString Signature, Value(CDECL *Fun)(Args...), FBindString FuncName, bool bTrivial, void* UserData = nullptr)
	{
		FAngelscriptBoundFunction Result = BindExternBehaviour(asBEHAVE_CONSTRUCT, Signature, asFUNCTION(Fun), ASAutoCaller::MakeFunctionCaller(Fun), UserData);
		return Result.NativeConstructor(FuncName.ToCString_EnsureConstant(), bTrivial).ImplicitConstructor();
	}

	template<typename T>
	inline FAngelscriptBoundFunction ImplicitConstructor(FBindString Signature, T Function, void* UserData = nullptr)
	{
		return ImplicitConstructor(Signature, (typename TLambdaFuncPtr<T>::Type)Function, UserData);
	}

	template<typename Value, typename... Args>
	inline FAngelscriptBoundFunction TemplateCallback(FBindString Signature, Value(CDECL *Fun)(Args...))
	{
		return BindStaticBehaviour(asBEHAVE_TEMPLATE_CALLBACK, Signature, asFUNCTION(Fun), ASAutoCaller::MakeFunctionCaller(Fun));
	}

	template<typename T>
	inline FAngelscriptBoundFunction TemplateCallback(FBindString Signature, T Function)
	{
		return TemplateCallback(Signature, (typename TLambdaFuncPtr<T>::Type)Function);
	}

	/**
	 * Enum Binding
	 */
	class ANGELSCRIPTRUNTIME_API FEnumBind
	{
	public:
		FEnumBind(FAngelscriptBinds& InTargetBinds, FBindString Name);
		FBindString EnumName;
		int32 TypeId;

		asITypeInfo* GetTypeInfo();

		struct ANGELSCRIPTRUNTIME_API FEnumElement
		{
			FEnumBind* Bind;
			FBindString Name;

			void operator=(int32 Value);

			template<typename E>
			void operator=(E Value)
			{
				*this = (int32)Value;
			}
		};

		FEnumElement operator[](FBindString Name)
		{
			return FEnumElement{this, Name};
		}

	private:
		FAngelscriptBinds* TargetBinds = nullptr;
		asIScriptEngine& ResolveScriptEngine() const;
	};

	FEnumBind EnumForTarget(FBindString Name)
	{
		return FEnumBind(*this, Name);
	}

	static bool ShouldSkipBlueprintCallableFunction(const UFunction* Function);
	static TMap<FString, TArray<TObjectPtr<UClass>>>& GetRuntimeClassDB();
#if WITH_EDITOR
	static TMap<FString, TArray<TObjectPtr<UClass>>>& GetEditorClassDB();
#endif
	static TMap<UClass*, TMap<FString, FAngelscriptFunctionBinding>>& GetClassFunctionBindings();
	static TMap<UClass*, TSet<FString>>& GetSkipBinds();
	static TSet<TTuple<FName, FName>>& GetSkipBindNames();
	static TSet<FName>& GetSkipBindClasses();
	static const FAngelscriptRegisteredFunctionProvenance*
		FindFunctionProvenance(int32 FunctionId);
	static const FAngelscriptRegisteredFunctionProvenance*
		FindFunctionProvenance(const asIScriptFunction& Function);

	struct ANGELSCRIPTRUNTIME_API FNamespace
	{
		FAngelscriptEngine* TargetEngine = nullptr;
		FBindString PrevNamespace;

		FNamespace(FAngelscriptEngine& InTargetEngine, FBindString Name);
		~FNamespace();
	};

	//WILL-EDIT ================================================================

	static bool CheckForSkipClass(FName ClassName)
	{
		auto& SkipBindClasses = GetSkipBindClasses();
		if (SkipBindClasses.Contains(ClassName))
			return true;
		return false;
	}

	//static bool CheckForSkipEntry(FString ClassName, FString FunctionName)
	static bool CheckForSkipEntry(FName ClassName, FName FunctionName)
	{
		auto& SkipBindNames = GetSkipBindNames();
		//TTuple<FString, FString> tuple = TTuple<FString, FString>(ClassName, FunctionName);
		TTuple<FName, FName> tuple = TTuple<FName, FName>(ClassName, FunctionName);
		
		if (SkipBindNames.Contains(tuple))
			return true;

		return false;
	}

	static bool CheckForSkip(UClass* Class, UFunction* Function)
	{		
		auto& SkipBinds = GetSkipBinds();
		if (SkipBinds.Contains(Class))
		{
			if (SkipBinds[Class].Contains(Function->GetName()))
				return true;
		}

		return false;
	}

	//TO-DO make sure the binds are written to base directory not inside another module
	static void SaveBindModules(const FString& Path, const TArray<FString>& BindModuleNames)
	{
		FFileHelper::SaveStringArrayToFile(BindModuleNames, *Path);
	}

	static void LoadBindModules(const FString& Path, TArray<FString>& BindModuleNames)
	{
		FFileHelper::LoadFileToStringArray(BindModuleNames, *Path);
	}

	//END WILL-EDIT =============================================================

	asITypeInfo* GetTypeInfo();
	bool HasMethod(const FString& MethodName);
	bool HasGetter(const FString& PropertyName);
	bool HasSetter(const FString& PropertyName);

private:
	FAngelscriptBinds ValueClassForTarget(FBindString Name, FBindFlags Flags, int32 Size);

	FAngelscriptBinds(FAngelscriptEngine& InTargetEngine, FBindString Name, asQWORD Flags, int32 Size);
	FAngelscriptBinds(FAngelscriptEngine& InTargetEngine, FBindString Name);

	FAngelscriptEngine* TargetEngine = nullptr;
	FBindString ClassName;
	asITypeInfo* ScriptType = nullptr;

	FAngelscriptEngine& ResolveTargetEngine() const;
	void RecordRegistrationFailure(const TCHAR* Operation, FBindString Declaration, int32 Result);
	FAngelscriptBoundFunction OnBindForTarget(int32 FunctionId, void* UserData, const FAngelscriptType::FBindParams* BindParams, const TCHAR* Operation, FBindString Declaration);
	FAngelscriptBoundFunction BindGlobalFunctionForTarget(FBindString Signature, asSFuncPtr Function, ASAutoCaller::FunctionCaller Caller, void* UserData);

	FAngelscriptBoundFunction BindBehaviour(asEBehaviours Beh, FBindString Signature, asSFuncPtr Ptr, ASAutoCaller::FunctionCaller Caller);
	FAngelscriptBoundFunction BindExternBehaviour(asEBehaviours Beh, FBindString Signature, asSFuncPtr Ptr, ASAutoCaller::FunctionCaller Caller, void* UserData = nullptr);
	FAngelscriptBoundFunction BindStaticBehaviour(asEBehaviours Beh, FBindString Signature, asSFuncPtr Ptr, ASAutoCaller::FunctionCaller Caller, void* UserData = nullptr);
	FAngelscriptBoundFunction BindMethod(FBindString Signature, asSFuncPtr Ptr, ASAutoCaller::FunctionCaller Caller, void* UserData = nullptr);
	FAngelscriptBoundFunction BindExternMethod(FBindString Signature, asSFuncPtr Ptr, ASAutoCaller::FunctionCaller Caller, void* UserData = nullptr);
	FAngelscriptBoundFunction BindExternMethod(FBindString Signature, asSFuncPtr Ptr, const FAngelscriptType::FBindParams& BindParams, ASAutoCaller::FunctionCaller Caller, void* UserData = nullptr);
	FAngelscriptBoundProperty BindProperty(FBindString Signature, size_t Offset);
	FAngelscriptBoundProperty BindProperty(FBindString Signature, size_t Offset, const FAngelscriptType::FBindParams& BindParams);

	friend struct FAngelscriptEngine;
};

// Re-enable any warnings we disabled
#ifdef _MSC_VER
#pragma warning(pop)
#endif
