#pragma once
#include "CoreMinimal.h"

#include <cmath>
#include <type_traits>

#include "Kismet/KismetMathLibrary.h"
#include "AngelscriptEngine.h"
#include "StaticJIT/StaticJITHelperFunctions.h"
#include "StaticJIT/StaticJITConfig.h"

#include "StartAngelscriptHeaders.h"
#include "AngelscriptInclude.h"
//#include "as_scriptfunction.h"
//#include "as_callfunc.h"
//#include "as_typeinfo.h"
//#include "as_context.h"
//#include "as_scriptengine.h"
//#include "as_scriptobject.h"
#include "source/as_scriptfunction.h"
#include "source/as_callfunc.h"
#include "source/as_typeinfo.h"
#include "source/as_context.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptobject.h"
#include "EndAngelscriptHeaders.h"

#if defined(_MSC_VER)
#define SCRIPT_ASSUME_NOT_NULL(V) __assume(V != 0);
#define SCRIPT_ASSUME(V) __assume(V);
#elif defined(__GNUC__) || defined(__clang__)
#define SCRIPT_ASSUME_NOT_NULL(V) __builtin_assume(V != 0);
#define SCRIPT_ASSUME(V) __builtin_assume(V);
#else
#define SCRIPT_ASSUME_NOT_NULL(V) 
#define SCRIPT_ASSUME(V) 
#endif

#include "Binds/Bind_TArray_Functions.h"

#ifndef AS_FORCE_LINK
#if defined(__GNUC__) || defined(__clang__)
#define AS_FORCE_LINK [[gnu::used, gnu::retain]]
#else
#define AS_FORCE_LINK
#endif
#endif

#define AS_JIT_DEBUG_CALLSTACKS (!UE_BUILD_SHIPPING)

#ifdef _MSC_VER
// Disable some warnings we dgaf about in static jit
#pragma warning(disable : 4101)
#pragma warning(disable : 4102)
#pragma warning(disable : 4191)
#pragma warning(disable : 4996)
#pragma warning(disable : 4883)
#pragma warning(disable : 4702)
#endif

#ifdef __clang__
#pragma clang diagnostic ignored "-Wself-assign"
#endif

enum class EJITException : uint8
{
	NullPointer,
	Div0,
	Overflow,
	UnboundFunction,
	OutOfBounds,
};

struct ANGELSCRIPTRUNTIME_API FStaticJITCompiledInfo
{
	FGuid PrecompiledDataGuid;

	FStaticJITCompiledInfo(FGuid Guid);
	static const FStaticJITCompiledInfo* Get();
};

struct ANGELSCRIPTRUNTIME_API FStaticJITFunction
{
	FStaticJITFunction(uint32 FunctionId,
		asJITFunction VMEntry,
		asJITFunction_ParmsEntry ParmsEntry,
		asJITFunction_Raw RawFunction
	);

	FORCENOINLINE static void ScriptFinishConstruct(FScriptExecution& Execution, asIScriptObject* Object, asITypeInfo* TypeInfo);
	FORCENOINLINE static void ScriptCallNative(FScriptExecution& Execution, asCScriptFunction* Function, asBYTE* l_sp, asQWORD* valueRegister, void** objectRegister);
	FORCENOINLINE static void SetException(FScriptExecution& Execution, EJITException Exception);
	FORCENOINLINE static void SetNullPointerException(FScriptExecution& Execution);
	FORCENOINLINE static void SetDivByZeroException(FScriptExecution& Execution);
	FORCENOINLINE static void SetOverflowException(FScriptExecution& Execution);
	FORCENOINLINE static void SetUnboundFunctionException(FScriptExecution& Execution);
	FORCENOINLINE static void SetOutOfBoundsException(FScriptExecution& Execution);
	FORCENOINLINE static void SetSwitchValueInvalidException(FScriptExecution& Execution);
	FORCENOINLINE static void SetUnknownException(FScriptExecution& Execution);
};

struct FAngelscriptPrecompiledData;

#if AS_WITH_STATIC_JIT_DIAGNOSTICS
struct FStaticJITDiagnostics;

struct ANGELSCRIPTRUNTIME_API FStaticJITDiagnosticEntryMarkers
{
	static void MarkEntry(uint32 FunctionId);
};
#endif

struct ANGELSCRIPTRUNTIME_API FJitRef_Function
{
	// Stable precompiled reference used to resolve Pointer for the current engine/module lifetime.
	uint64 Reference;
	void* Pointer;
	FJitRef_Function(uint64 FunctionRef);
	FORCEINLINE asCScriptFunction* Get() const { return (asCScriptFunction*)Pointer;  }
};

struct ANGELSCRIPTRUNTIME_API FJitRef_SystemFunctionPointer
{
	// Stable precompiled function reference used to refresh the native function pointer.
	uint64 Reference;
	union
	{
		asFUNCTION_t Func;
		asMETHOD_t Method;
		void* Pointer;
	};

	FJitRef_SystemFunctionPointer(uint64 FunctionRef);
	FORCEINLINE asFUNCTION_t GetFunction() const { return Func;  }
	FORCEINLINE asMETHOD_t GetMethod() const { return Method;  }
};

struct ANGELSCRIPTRUNTIME_API FJitRef_Type
{
	// Stable precompiled reference used to resolve Pointer for the current engine/module lifetime.
	uint64 Reference;
	void* Pointer;
	FJitRef_Type(uint64 TypeRef);
	FORCEINLINE asCTypeInfo* Get() const { return (asCTypeInfo*)Pointer;  }
	FORCEINLINE asCObjectType* GetObjectType() const { return (asCObjectType*)Pointer;  }
};

struct ANGELSCRIPTRUNTIME_API FJitRef_GlobalVar
{
	// Stable precompiled reference used to resolve Pointer for the current engine/module lifetime.
	uint64 Reference;
	void* Pointer;
	FJitRef_GlobalVar(uint64 GlobalRef);
	FORCEINLINE void* Get() const { return Pointer;  }
};

struct ANGELSCRIPTRUNTIME_API FJitRef_PropertyOffset
{
	uint32 Offset;

	FJitRef_PropertyOffset(uint64 PropertyRef);
	FORCEINLINE uint32 Get() const { return Offset;  }
};

struct ANGELSCRIPTRUNTIME_API FJitVerifyPropertyOffset
{
#if AS_JIT_VERIFY_PROPERTY_OFFSETS
	FJitVerifyPropertyOffset(uint64 PropertyRef, SIZE_T ComputedOffset);
#else
	FJitVerifyPropertyOffset(uint64 PropertyRef, SIZE_T ComputedOffset) {}
#endif
};

struct ANGELSCRIPTRUNTIME_API FJitVerifyTypeSize
{
#if AS_JIT_VERIFY_PROPERTY_OFFSETS
	FJitVerifyTypeSize(uint64 TypeRef, SIZE_T ComputedSize, SIZE_T ComputedAlignment);
#else
	FJitVerifyTypeSize(uint64 TypeRef, SIZE_T ComputedSize, SIZE_T ComputedAlignment) {}
#endif
};

struct FUnknownValueType
{
	union
	{
		float FloatValue;
		asDWORD DWordValue;
		asBYTE ByteValue;
		void* PtrValue;
	};
};

struct FScopeInformSystemFunction
{
	asIScriptFunction* PrevActiveFunction;
	asCThreadLocalData* TLD;

	FORCEINLINE FScopeInformSystemFunction(FScriptExecution& Execution, asIScriptFunction* ScriptFunc)
	{
		TLD = Execution.tld;
		PrevActiveFunction = TLD->activeFunction;
		TLD->activeFunction = ScriptFunc;
	}

	FORCEINLINE ~FScopeInformSystemFunction()
	{
		TLD->activeFunction = PrevActiveFunction;
	}
};

#if AS_JIT_DEBUG_CALLSTACKS
struct FScopeJITDebugCallstack
{
	const char* Filename;
	const char* FunctionName;
	void* ThisObject;
	int32 LineNumber;
	FScopeJITDebugCallstack* PrevFrame;
	FScriptExecution& Execution;

	FORCEINLINE FScopeJITDebugCallstack(FScriptExecution& InExecution, const char* InFilename, const char* InFunctionName, int32 InLineNumber, void* InThisObject)
		: Execution(InExecution)
	{
		Filename = InFilename;
		FunctionName = InFunctionName;
		LineNumber = InLineNumber;
		PrevFrame = (FScopeJITDebugCallstack*)Execution.debugCallStack;
		Execution.debugCallStack = this;
		ThisObject = InThisObject;
	}

	FORCEINLINE ~FScopeJITDebugCallstack()
	{
		Execution.debugCallStack = PrevFrame;
	}
};
#endif

template<typename T>
struct TAutoDereference
{
	T* Ptr;

	TAutoDereference(T* InPtr)
		: Ptr(InPtr)
	{}

	TAutoDereference(void* InPtr)
		: Ptr((T*)InPtr)
	{}

	TAutoDereference(const T& InPtr)
		: Ptr((T*)&InPtr)
	{}

	TAutoDereference(T& InPtr)
		: Ptr(&InPtr)
	{}

	FORCEINLINE operator T&()
	{
		return *Ptr;
	}

	FORCEINLINE operator T*()
	{
		return Ptr;
	}

	FORCEINLINE operator void*()
	{
		return Ptr;
	}
};

template<typename OutT, typename InT>
FORCEINLINE OutT value_as(InT InValue)
{
	if constexpr (std::is_same_v<OutT, InT>)
	{
		return InValue;
	}
	else if constexpr (sizeof(InT) < sizeof(OutT))
	{
		OutT ExtendedValue = {};
		memcpy(&ExtendedValue, &InValue, sizeof(InT));
		return ExtendedValue;
	}
	else
	{
		OutT OutValue;
		memcpy(&OutValue, &InValue, sizeof(OutT));
		return OutValue;
	}
}

template<typename OutT>
FORCEINLINE OutT value_read(void* InValue)
{
	OutT OutValue;
	memcpy(&OutValue, InValue, sizeof(OutT));
	return OutValue;
}

template<typename OutT, typename InT>
FORCEINLINE void value_assign_safe(void* OutValue, InT InValue)
{
	if constexpr (std::is_same_v<OutT, InT>)
	{
		*(OutT*)OutValue = InValue;
	}
	else if constexpr (sizeof(InT) < sizeof(OutT))
	{
		OutT ExtendedValue = {};
		memcpy(&ExtendedValue, &InValue, sizeof(InT));
		memcpy(OutValue, &ExtendedValue, sizeof(OutT));
	}
	else
	{
		memcpy(OutValue, &InValue, sizeof(OutT));
	}
}

// This function is FORCENOINLINE to work around an MSVC
// compiler bug where the optimizer would confuse signed and unsigned integers,
// so a integer -1 could get converted to a double of value UINT_MAX.
template<typename OutT, typename InT>
FORCENOINLINE OutT ConvertPrimitiveValue(InT InValue)
{
	return (OutT)(InValue);
}

// Proxy struct for script structs that are fully POD, allows better code generation
template<int Size, int Alignment> 
struct alignas(Alignment) TScriptPODStruct
{
	uint8 Data[Size];
};

template<int Alignment>
struct alignas(Alignment) TScriptPODEmptyStruct
{
};

constexpr SIZE_T AlignmentMax(SIZE_T A, SIZE_T B)
{
	return A >= B ? A : B;
}

#define SCRIPT_CALLSTACK_FRAME_SIZE 10

#define SCRIPT_ENGINE (FAngelscriptEngine::Get().Engine)
#define SCRIPT_TLD (Execution.tld)

#if AS_JIT_DEBUG_CALLSTACKS
#define SCRIPT_DEBUG_CALLSTACK_FRAME(FunctionName, LineNumber) FScopeJITDebugCallstack DebugCallstack(Execution, SCRIPT_DEBUG_FILENAME, FunctionName, LineNumber, nullptr);
#define SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT(FunctionName, LineNumber) FScopeJITDebugCallstack DebugCallstack(Execution, SCRIPT_DEBUG_FILENAME, FunctionName, LineNumber, l_This);
#define SCRIPT_DEBUG_CALLSTACK_LINE(InLineNumber) DebugCallstack.LineNumber = InLineNumber;
#else
#define SCRIPT_DEBUG_CALLSTACK_FRAME(FunctionName, LineNumber) 
#define SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT(FunctionName, LineNumber) 
#define SCRIPT_DEBUG_CALLSTACK_LINE(LineNumber) 
#endif

#define SCRIPT_NULL_POINTER_EXCEPTION() \
	FStaticJITFunction::SetNullPointerException(Execution);

#define SCRIPT_CALL_NATIVE(Function, StackPosition) \
	FStaticJITFunction::ScriptCallNative(Execution, Function, &l_stack[StackPosition], &l_valueRegister, &l_objectRegister);

#define SCRIPT_FINISH_CONSTRUCT(Object, TypeInfo) \
	FStaticJITFunction::ScriptFinishConstruct(Execution, Object, TypeInfo);

#define SCRIPT_DIV0_EXCEPTION() \
	FStaticJITFunction::SetDivByZeroException(Execution);

#define SCRIPT_OVERFLOW_EXCEPTION() \
	FStaticJITFunction::SetOverflowException(Execution);

#define SCRIPT_UNBOUND_EXCEPTION() \
	FStaticJITFunction::SetUnboundFunctionException(Execution);

#define SCRIPT_OUT_OF_BOUNDS_EXCEPTION() \
	FStaticJITFunction::SetOutOFBoundsException(Execution);

#define SCRIPT_SWITCH_VALUE_INVALID_EXCEPTION() \
	FStaticJITFunction::SetSwitchValueInvalidException(Execution);

#define SCRIPT_UNKNOWN_EXCEPTION() \
	FStaticJITFunction::SetUnknownException(Execution);

#define SCRIPT_ASSUME_NO_EXCEPTION() SCRIPT_ASSUME(Execution.bExceptionThrown == false)
