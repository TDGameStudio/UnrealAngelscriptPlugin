#pragma once

#include "CoreMinimal.h"
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
	TArray<FString> BindModuleNames;
	TMap<UClass*, TSet<FString>> SkipBinds;
	TSet<TTuple<FName, FName>> SkipBindNames;
	TSet<FName> SkipBindClasses;
	TMap<int32, FAngelscriptRegisteredFunctionProvenance> FunctionProvenance;
	TMap<const asIScriptFunction*, FAngelscriptRegisteredFunctionProvenance>
		FunctionProvenanceByPointer;
	FName ActiveBindProvider;
	int32 PreviouslyBoundFunction = -1;
	int32 PreviouslyBoundGlobalProperty = -1;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptBinds
{
	/* 
	* Class-specific binding.
	*/
	static FAngelscriptBinds ReferenceClass(FBindString Name, UClass* UnrealClass);
	static FAngelscriptBinds ExistingClass(FBindString Name);

	template<typename T>
	static FAngelscriptBinds ValueClass(FBindString Name, FBindFlags Flags)
	{
		if (Flags.Alignment == -1)
			Flags.Alignment = alignof(T);
		Flags.ExtraFlags |= asGetTypeTraits<T>();
		return ValueClass(Name, Flags, sizeof(T));
	}

	static FAngelscriptBinds ValueClass(FBindString Name, UScriptStruct* StructType, FBindFlags Flags)
	{
		int Size;
		auto* Ops = StructType->GetCppStructOps();
		if (Ops != nullptr)
			Size = Ops->GetSize();
		else
			Size = StructType->GetPropertiesSize();
		if (Flags.Alignment == -1)
			Flags.Alignment = StructType->GetMinAlignment();
		return ValueClass(Name, Flags, Size);
	}

	static FAngelscriptBinds ValueClass(FBindString Name, SIZE_T Size, FBindFlags Flags)
	{
		Flags.ExtraFlags |= asOBJ_APP_CLASS_CDAK;
		return ValueClass(Name, Flags, Size);
	}

	template<typename C, typename Value, typename... Args>
	inline void Method(FBindString Signature, Value(C::*Fun)(Args...), void* UserData = nullptr)
	{
		return BindMethod(Signature, asSMethodPtr<sizeof(void(C::*)())>::Convert((void(C::*)())(Fun)), ASAutoCaller::MakeFunctionCaller(Fun), UserData);
	}

	template<typename C, typename Value, typename... Args>
	inline void Method(FBindString Signature, Value(C::*Fun)(Args...)const, void* UserData = nullptr)
	{
		return BindMethod(Signature, asSMethodPtr<sizeof(void(C::*)())>::Convert((void(C::*)())(Fun)), ASAutoCaller::MakeFunctionCaller(Fun), UserData);
	}

	template<typename C, typename Value, typename... Args>
	inline void Method(FBindString Signature, Value(C::*Fun)(Args...)const&, void* UserData = nullptr)
	{
		return BindMethod(Signature, asSMethodPtr<sizeof(void(C::*)())>::Convert((void(C::*)())(Fun)), ASAutoCaller::MakeFunctionCaller(Fun), UserData);
	}

	template<typename Value, typename... Args>
	inline int Method(FBindString Signature, Value(CDECL *fun)(Args...), void* UserData = nullptr)
	{
		return BindExternMethod(Signature, asFUNCTION(fun), ASAutoCaller::MakeFunctionCaller(fun), UserData);
	}

	template<typename C, typename Value, typename... Args>
	inline void Method(FBindString Signature, Value(C::*Fun)(Args...)const, FBindString MethodClass, FBindString MethodName, bool bTrivial, void* UserData = nullptr)
	{
		BindMethod(Signature, asSMethodPtr<sizeof(void(C::*)())>::Convert((void(C::*)())(Fun)), ASAutoCaller::MakeFunctionCaller(Fun), UserData);
		SCRIPT_NATIVE_METHOD(*this, MethodName.ToCString_EnsureConstant(), bTrivial);
	}

	template<typename C, typename Value, typename... Args>
	inline void Method(FBindString Signature, Value(C::*Fun)(Args...), FBindString MethodClass, FBindString MethodName, bool bTrivial, void* UserData = nullptr)
	{
		BindMethod(Signature, asSMethodPtr<sizeof(void(C::*)())>::Convert((void(C::*)())(Fun)), ASAutoCaller::MakeFunctionCaller(Fun), UserData);
		SCRIPT_NATIVE_METHOD(*this, MethodName.ToCString_EnsureConstant(), bTrivial);
	}

	template<typename C, typename Value, typename... Args>
	inline void Method(FBindString Signature, Value(C::*Fun)(Args...)const&, FBindString MethodClass, FBindString MethodName, bool bTrivial, void* UserData = nullptr)
	{
		BindMethod(Signature, asSMethodPtr<sizeof(void(C::*)())>::Convert((void(C::*)())(Fun)), ASAutoCaller::MakeFunctionCaller(Fun), UserData);
		SCRIPT_NATIVE_METHOD(*this, MethodName.ToCString_EnsureConstant(), bTrivial);
	}

	template<typename Value, typename... Args>
	inline int Method(FBindString Signature, Value(CDECL *fun)(Args...), FBindString FuncName, bool bTrivial, void* UserData = nullptr)
	{
		int Result = BindExternMethod(Signature, asFUNCTION(fun), ASAutoCaller::MakeFunctionCaller(fun), UserData);
		SCRIPT_NATIVE_FUNCTION(FuncName.ToCString_EnsureConstant(), bTrivial);
		return Result;
	}

	template<typename Value, typename... Args>
	inline int Method(FBindString Signature, Value(CDECL *fun)(Args...), FBindString FuncName, bool bTrivial, const FAngelscriptType::FBindParams& BindParams, void* UserData = nullptr)
	{
		int Result = BindExternMethod(Signature, asFUNCTION(fun), BindParams, ASAutoCaller::MakeFunctionCaller(fun), UserData);
		SCRIPT_NATIVE_FUNCTION(FuncName.ToCString_EnsureConstant(), bTrivial);
		return Result;
	}

	template<typename Value, typename... Args>
	inline int Method(FBindString Signature, Value(CDECL *fun)(Args...), const FAngelscriptType::FBindParams& BindParams, void* UserData = nullptr)
	{
		int Result = BindExternMethod(Signature, asFUNCTION(fun), BindParams, ASAutoCaller::MakeFunctionCaller(fun), UserData);
		return Result;
	}

	template<typename T>
	inline int Method(FBindString Signature, T Function, void* UserData = nullptr)
	{
		auto FunctionPointer = (typename TLambdaFuncPtr<T>::Type)Function;
		return BindExternMethod(Signature, asFUNCTION(FunctionPointer), ASAutoCaller::MakeFunctionCaller(FunctionPointer), UserData);
	}

	template<typename T>
	inline int Method(FBindString Signature, T Function, const FAngelscriptType::FBindParams& BindParams, void* UserData = nullptr)
	{
		auto FunctionPointer = (typename TLambdaFuncPtr<T>::Type)Function;
		return BindExternMethod(Signature, asFUNCTION(FunctionPointer), BindParams, ASAutoCaller::MakeFunctionCaller(FunctionPointer), UserData);
	}

	void GenericMethod(FBindString Signature, void(CDECL *Fun)(asIScriptGeneric*), void* UserData);

	template<typename T>
	inline void GenericMethod(FBindString Signature, T Function, void* UserData)
	{
		return GenericMethod(Signature, (typename TLambdaFuncPtr<T>::Type)Function, UserData);
	}

	template<typename C, typename T>
	inline void Property(FBindString Signature, T C::*Ptr)
	{
		return BindProperty(Signature, (size_t)&(((C*)nullptr)->*Ptr));
	}

	inline void Property(FBindString Signature, size_t Offset)
	{
		return BindProperty(Signature, Offset);
	}

	inline void Property(FBindString Signature, size_t Offset, const FAngelscriptType::FBindParams& BindParams)
	{
		return BindProperty(Signature, Offset, BindParams);
	}

	template<typename Value, typename... Args>
	inline void Factory(FBindString Signature, Value(CDECL *Fun)(Args...), void* UserData = nullptr)
	{
		return BindStaticBehaviour(asBEHAVE_FACTORY, Signature, asFUNCTION(Fun), ASAutoCaller::MakeFunctionCaller(Fun), UserData);
	}

	template<typename Value, typename... Args>
	inline void Destructor(FBindString Signature, Value(CDECL *Fun)(Args...), void* UserData = nullptr)
	{
		BindExternBehaviour(asBEHAVE_DESTRUCT, Signature, asFUNCTION(Fun), ASAutoCaller::MakeFunctionCaller(Fun), UserData);
	}

	template<typename Value, typename... Args>
	inline void Destructor(FBindString Signature, Value(CDECL *Fun)(Args...), FBindString FuncName, bool bTrivial, void* UserData = nullptr)
	{
		BindExternBehaviour(asBEHAVE_DESTRUCT, Signature, asFUNCTION(Fun), ASAutoCaller::MakeFunctionCaller(Fun), UserData);
		SCRIPT_NATIVE_FUNCTION(FuncName.ToCString_EnsureConstant(), bTrivial);
	}

	template<typename T>
	inline void Destructor(FBindString Signature, T Function, void* UserData = nullptr)
	{
		Destructor(Signature, (typename TLambdaFuncPtr<T>::Type)Function, UserData);
	}

	template<typename T>
	inline void Factory(FBindString Signature, T Function, void* UserData = nullptr)
	{
		return Factory(Signature, (typename TLambdaFuncPtr<T>::Type)Function, UserData);
	}

	template<typename Value, typename... Args>
	inline void Constructor(FBindString Signature, Value(CDECL *Fun)(Args...), void* UserData = nullptr)
	{
		return BindExternBehaviour(asBEHAVE_CONSTRUCT, Signature, asFUNCTION(Fun), ASAutoCaller::MakeFunctionCaller(Fun), UserData);
	}

	template<typename Value, typename... Args>
	inline void Constructor(FBindString Signature, Value(CDECL *Fun)(Args...), FBindString FuncName, bool bTrivial, void* UserData = nullptr)
	{
		BindExternBehaviour(asBEHAVE_CONSTRUCT, Signature, asFUNCTION(Fun), ASAutoCaller::MakeFunctionCaller(Fun), UserData);
		SCRIPT_NATIVE_FUNCTION(FuncName.ToCString_EnsureConstant(), bTrivial);
	}

	template<typename T>
	inline void Constructor(FBindString Signature, T Function, void* UserData = nullptr)
	{
		return Constructor(Signature, (typename TLambdaFuncPtr<T>::Type)Function, UserData);
	}

	template<typename Value, typename... Args>
	inline void ImplicitConstructor(FBindString Signature, Value(CDECL *Fun)(Args...), void* UserData = nullptr)
	{
		BindExternBehaviour(asBEHAVE_CONSTRUCT, Signature, asFUNCTION(Fun), ASAutoCaller::MakeFunctionCaller(Fun), UserData);
		MarkAsImplicitConstructor();
	}

	template<typename Value, typename... Args>
	inline void ImplicitConstructor(FBindString Signature, Value(CDECL *Fun)(Args...), FBindString FuncName, bool bTrivial, void* UserData = nullptr)
	{
		BindExternBehaviour(asBEHAVE_CONSTRUCT, Signature, asFUNCTION(Fun), ASAutoCaller::MakeFunctionCaller(Fun), UserData);
		SCRIPT_NATIVE_FUNCTION(FuncName.ToCString_EnsureConstant(), bTrivial);
		MarkAsImplicitConstructor();
	}

	template<typename T>
	inline void ImplicitConstructor(FBindString Signature, T Function, void* UserData = nullptr)
	{
		ImplicitConstructor(Signature, (typename TLambdaFuncPtr<T>::Type)Function, UserData);
	}

	template<typename Value, typename... Args>
	inline void TemplateCallback(FBindString Signature, Value(CDECL *Fun)(Args...))
	{
		return BindStaticBehaviour(asBEHAVE_TEMPLATE_CALLBACK, Signature, asFUNCTION(Fun), ASAutoCaller::MakeFunctionCaller(Fun));
	}

	template<typename T>
	inline void TemplateCallback(FBindString Signature, T Function)
	{
		return TemplateCallback(Signature, (typename TLambdaFuncPtr<T>::Type)Function);
	}

	/* 
	* Global binding
	*/
	template<typename Value, typename... Args>
	static inline int BindGlobalFunction(FBindString Signature, Value(*Fun)(Args...), void* UserData = nullptr)
	{
		return BindGlobalFunction(Signature, asFUNCTION(Fun), ASAutoCaller::MakeFunctionCaller(Fun), UserData);
	}

	template<typename Value, typename... Args>
	static inline int BindGlobalFunction(FBindString Signature, Value(*Fun)(Args...), FBindString FuncName, bool bTrivial, void* UserData = nullptr)
	{
		return BindGlobalFunction(Signature, asFUNCTION(Fun), FuncName, bTrivial, ASAutoCaller::MakeFunctionCaller(Fun), UserData);
	}

	template<typename T>
	static inline int BindGlobalFunction(FBindString Signature, T Function, void* UserData = nullptr)
	{
		auto FuncPtr = (typename TLambdaFuncPtr<T>::Type)Function;
		return BindGlobalFunction(Signature, asFUNCTION(FuncPtr), ASAutoCaller::MakeFunctionCaller(FuncPtr), UserData);
	}

	template<typename T>
	static inline int BindGlobalGenericFunction(FBindString Signature, T Function, void* UserData = nullptr)
	{
		return BindGlobalGenericFunction(Signature, (typename TLambdaFuncPtr<T>::Type)Function, UserData);
	}

	/**
	 * Enum Binding
	 */
	class ANGELSCRIPTRUNTIME_API FEnumBind
	{
	public:
		FEnumBind(FBindString Name);
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
	};

	inline static FEnumBind Enum(FBindString Name)
	{
		return FEnumBind(Name);
	}

	/* Register functions to be called at bind-time. */
	enum class EOrder : int32
	{
		Early = -100,
		Normal = 0,
		Late = 100,
	};

	struct FBindInfo
	{
		FName BindName;
		int32 BindOrder = 0;
		bool bEnabled = true;
	};

	struct ANGELSCRIPTRUNTIME_API FBind
	{
		FBind(FName BindName, int32 BindOrder, TFunction<void()> Function)
		{
			FAngelscriptBinds::RegisterBinds(BindName, BindOrder, MoveTemp(Function));
		}

		FBind(FName BindName, EOrder BindOrder, TFunction<void()> Function)
		{
			FAngelscriptBinds::RegisterBinds(BindName, (int32)BindOrder, MoveTemp(Function));
		}

		FBind(FName BindName, TFunction<void()> Function)
		{
			FAngelscriptBinds::RegisterBinds(BindName, 0, MoveTemp(Function));
		}

		FBind(int32 BindOrder, TFunction<void()> Function, const ANSICHAR* CallerFile = __builtin_FILE())
		{
			FAngelscriptBinds::RegisterBinds(BindOrder, MoveTemp(Function), CallerFile);
		}

		FBind(EOrder BindOrder, TFunction<void()> Function, const ANSICHAR* CallerFile = __builtin_FILE())
		{
			FAngelscriptBinds::RegisterBinds((int32)BindOrder, MoveTemp(Function), CallerFile);
		}

		FBind(TFunction<void()> Function, const ANSICHAR* CallerFile = __builtin_FILE())
		{
			FAngelscriptBinds::RegisterBinds(0, MoveTemp(Function), CallerFile);
		}
	};

	static void RegisterBinds(FName BindName, int32 BindOrder, TFunction<void()> Function);
	static void RegisterBinds(int32 BindOrder, TFunction<void()> Function, const ANSICHAR* CallerFile = nullptr);
	static TArray<FName> GetAllRegisteredBindNames();
	static TArray<FBindInfo> GetBindInfoList(const TSet<FName>& DisabledBindNames = TSet<FName>());
	static void ResetBindState();
	static bool ShouldSkipBlueprintCallableFunction(const UFunction* Function);
	static TMap<FString, TArray<TObjectPtr<UClass>>>& GetRuntimeClassDB();
#if WITH_EDITOR
	static TMap<FString, TArray<TObjectPtr<UClass>>>& GetEditorClassDB();
#endif
	static TMap<UClass*, TMap<FString, FAngelscriptFunctionBinding>>& GetClassFunctionBindings();
	static TArray<FString>& GetBindModuleNames();
	static TMap<UClass*, TSet<FString>>& GetSkipBinds();
	static TSet<TTuple<FName, FName>>& GetSkipBindNames();
	static TSet<FName>& GetSkipBindClasses();
	static const FAngelscriptRegisteredFunctionProvenance*
		FindFunctionProvenance(int32 FunctionId);
	static const FAngelscriptRegisteredFunctionProvenance*
		FindFunctionProvenance(const asIScriptFunction& Function);

	struct ANGELSCRIPTRUNTIME_API FNamespace
	{
		FBindString PrevNamespace;

		FNamespace(FBindString Name);
		~FNamespace();
	};

	//WILL-EDIT ================================================================

	static void RegisterFunctionBinding(UClass* Class, const FString& Name, const FAngelscriptFunctionBinding& Binding)
	{
		if (Class == nullptr)
		{
			return;
		}

		auto& ClassFunctionBindings = GetClassFunctionBindings();
		TMap<FString, FAngelscriptFunctionBinding>& FunctionMap = ClassFunctionBindings.FindOrAdd(Class);
		if (FAngelscriptFunctionBinding* ExistingBinding = FunctionMap.Find(Name))
		{
			if (!ExistingBinding->FunctionPointer.IsBound() && !ExistingBinding->bReflectiveFallbackBound && Binding.FunctionPointer.IsBound())
			{
				*ExistingBinding = Binding;
			}
			return;
		}

		FunctionMap.Add(Name, Binding);
	}

	static void RegisterGeneratedFunctionBinding(
		UClass* Class,
		const FString& Name,
		FAngelscriptFunctionBinding Binding)
	{
		Binding.Origin = EAngelscriptFunctionBindingOrigin::Generated;
		RegisterFunctionBinding(Class, Name, Binding);
	}

	static void SkipFunctionEntry(UClass* Class, FString Name)
	{
		auto& SkipBinds = GetSkipBinds();
		if (SkipBinds.Contains(Class))
		{
			if (!SkipBinds[Class].Contains(Name))
			{
				SkipBinds[Class].Add(Name);
			}
		}

		else
		{
			SkipBinds.Add(Class, TSet<FString>()).Add(Name);
		}
	}

	static void AddSkipClass(FName ClassName)
	{
		auto& SkipBindClasses = GetSkipBindClasses();
		if (!SkipBindClasses.Contains(ClassName))
			SkipBindClasses.Add(ClassName);
	}

	//static void AddSkipEntry(FString ClassName, FString FunctionName)
	static void AddSkipEntry(FName ClassName, FName FunctionName)
	{
		auto& SkipBindNames = GetSkipBindNames();
		//TTuple<FString, FString> tuple = TTuple<FString, FString>(ClassName, FunctionName);
		TTuple<FName, FName> tuple = TTuple<FName, FName>(ClassName, FunctionName);
		if (!SkipBindNames.Contains(tuple))
		{
			SkipBindNames.Add(tuple);
		}
	}

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
	static void SaveBindModules(FString Path)
	{
		auto& BindModuleNames = GetBindModuleNames();
		//TArray<uint8> Data;
		//FMemoryWriter Writer(Data);
		//Writer << BindModuleNames;
		//FFileHelper::SaveArrayToFile(Data, *Path);
		FFileHelper::SaveStringArrayToFile(BindModuleNames, *Path);
	}

	static void LoadBindModules(FString Path)
	{
		auto& BindModuleNames = GetBindModuleNames();
		//TArray<uint8> Data;
		//FFileHelper::LoadFileToArray(Data, *Path);
		//FMemoryReader Reader(Data);
		//Reader << BindModuleNames;
		FFileHelper::LoadFileToStringArray(BindModuleNames, *Path);
	}

	//END WILL-EDIT =============================================================

	static int BindGlobalFunction(FBindString Signature, asSFuncPtr Function, ASAutoCaller::FunctionCaller Caller, void* UserData = nullptr);
	static int BindGlobalFunction(FBindString Signature, asSFuncPtr Function, FBindString FuncName, bool bTrivial, ASAutoCaller::FunctionCaller Caller, void* UserData = nullptr);
	static int BindGlobalFunctionDirect(FBindString Signature, asSFuncPtr Function, asECallConvTypes CallConv, ASAutoCaller::FunctionCaller Caller, void* UserData = nullptr);
	static int BindGlobalGenericFunction(FBindString Signature, void(CDECL* Function)(asIScriptGeneric*), void* UserData = nullptr);
	static void BindGlobalVariable(FBindString Signature, const void* Address);
	static int BindMethodDirect(FBindString ClassName, FBindString Signature, asSFuncPtr Function, asECallConvTypes CallConv, ASAutoCaller::FunctionCaller Caller, void* UserData = nullptr);

	static int CompileOutInTest(int FunctionId);
	static int CompileOutIfNoLog(int FunctionId);
	static int CompileOutAsEnsure(int FunctionId);
	static int CompileOutAsCheck(int FunctionId);
	static int CompileReplaceWithFirstArgInTest(int FunctionId);

	static void CompileOutPreviousBind();
	static void CompileOutPreviousBindAsMethodChain();

	asITypeInfo* GetTypeInfo();
	bool HasMethod(const FString& MethodName);
	bool HasGetter(const FString& PropertyName);
	bool HasSetter(const FString& PropertyName);

	static asIScriptFunction* GetPreviousBind();
	static int32 GetPreviousFunctionId() { return GetPreviouslyBoundFunctionRef(); }
	static int32 GetPreviousGlobalVariableId() { return GetPreviouslyBoundGlobalPropertyRef(); }
	static void DeprecatePreviousBind(const ANSICHAR* DeprecationMessage);
	static void SetPreviousBindIsGeneratedAccessor(bool bIsAccessor);
	static void SetPreviousBindIsEditorOnly(bool bEditorOnly);
	static void SetPreviousBindRequiresWorldContext(bool bRequiresWorldContext);
	static void SetPreviousBindIsCallable(bool bIsCallable);
	static void SetPreviousBindNoDiscard(bool bNoDiscard);
	static void SetPreviousBindForceConstArgumentExpressions(bool bForceConst);
	static void PreviousBindPassScriptFunctionAsFirstParam();
	static void PreviousBindPassScriptObjectTypeAsFirstParam();
	static void SetPreviousBindArgumentDeterminesOutputType(int ArgumentIndex);

	template<typename T>
	static void SetPreviousBoundGlobalVariablePureConstant(T ConstantValue)
	{
		asQWORD RawValue = 0;
		memcpy(&RawValue, &ConstantValue, sizeof(T));
		SetPreviousBoundPropertyPureConstant(RawValue);
	}

private:
	static int32& GetPreviouslyBoundFunctionRef();
	static int32& GetPreviouslyBoundGlobalPropertyRef();
	static FAngelscriptBinds ValueClass(FBindString Name, FBindFlags Flags, int32 Size);

	explicit FAngelscriptBinds(FBindString Name, asQWORD Flags, int32 Size);
	explicit FAngelscriptBinds(FBindString Name);

	FBindString ClassName;
	asITypeInfo* ScriptType = nullptr;

	void BindBehaviour(asEBehaviours Beh, FBindString Signature, asSFuncPtr Ptr, ASAutoCaller::FunctionCaller Caller);
	void BindExternBehaviour(asEBehaviours Beh, FBindString Signature, asSFuncPtr Ptr, ASAutoCaller::FunctionCaller Caller, void* UserData = nullptr);
	void BindStaticBehaviour(asEBehaviours Beh, FBindString Signature, asSFuncPtr Ptr, ASAutoCaller::FunctionCaller Caller, void* UserData = nullptr);
	void BindMethod(FBindString Signature, asSFuncPtr Ptr, ASAutoCaller::FunctionCaller Caller, void* UserData = nullptr);
	int BindExternMethod(FBindString Signature, asSFuncPtr Ptr, ASAutoCaller::FunctionCaller Caller, void* UserData = nullptr);
	int BindExternMethod(FBindString Signature, asSFuncPtr Ptr, const FAngelscriptType::FBindParams& BindParams, ASAutoCaller::FunctionCaller Caller, void* UserData = nullptr);
	void BindProperty(FBindString Signature, size_t Offset);
	void BindProperty(FBindString Signature, size_t Offset, const FAngelscriptType::FBindParams& BindParams);

	void MarkAsImplicitConstructor();

	static void OnBind(int FunctionId, void* UserData, const FAngelscriptType::FBindParams* BindParams);

	static void SetPreviousBoundPropertyPureConstant(asQWORD ConstantValue);

	friend struct FAngelscriptEngine;
	friend struct FAngelscriptBindConfigTestAccess;
	static void CallBinds();
	static void CallBinds(const TSet<FName>& DisabledBindNames);
};

// Re-enable any warnings we disabled
#ifdef _MSC_VER
#pragma warning(pop)
#endif
