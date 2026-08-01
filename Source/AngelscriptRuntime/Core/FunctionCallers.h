#pragma once
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "UObject/Object.h"
#include "UObject/Script.h"

// HAZE FIX(LV): Expose type-erased method pointers to native C++ methods
/*
* These are type-erasure templates for method pointer-to-member function pointers.
* This code is literally copied in from angelscript.h and renamed so unreal can
* generate the code needed to save these, so they can be bound to angelscript.
*/
class FUnknownClass;
typedef void (FUnknownClass::* FTypeErasedMethodPtr)();

typedef void (*FTypeErasedFuncPtr)();

struct FGenericFuncPtr
{
	FORCEINLINE static FGenericFuncPtr Make(uint8 f = 0)
	{
		FGenericFuncPtr Ptr;
		Ptr.Init(f);
		return Ptr;
	}

	FORCEINLINE static FGenericFuncPtr Make(FTypeErasedFuncPtr func)
	{
		FGenericFuncPtr Ptr;
		Ptr.Init(2);
		Ptr.ptr.f.func = func;
		return Ptr;
	}

	FORCEINLINE void Init(uint8 f = 0)
	{
		for (size_t n = 0; n < sizeof(ptr.dummy); n++)
			ptr.dummy[n] = 0;
		flag = f;
	}

	FORCEINLINE void CopyMethodPtr(const void* mthdPtr, size_t size)
	{
		for (size_t n = 0; n < size; n++)
			ptr.dummy[n] = reinterpret_cast<const char*>(mthdPtr)[n];
	}

	FORCEINLINE bool IsBound() const
	{
		return flag != 0;
	}

	union
	{
		// The largest known method point is 20 bytes (MSVC 64bit),
		// but with 8byte alignment this becomes 24 bytes. So we need
		// to be able to store at least that much.
		struct { FTypeErasedFuncPtr func; char dummy[25 - sizeof(FTypeErasedFuncPtr)]; } f;
		struct { FTypeErasedMethodPtr   mthd; char dummy[25 - sizeof(FTypeErasedMethodPtr)]; } m;
		char dummy[25];
	} ptr;
	uint8 flag; // 1 = generic, 2 = global func, 3 = method
};

#define ERASE_ARGUMENT_PACK(...) __VA_ARGS__
#define ERASE_NO_FUNCTION() FGenericFuncPtr{}, ASAutoCaller::FunctionCaller{}
#define ERASE_METHOD_PTR(c,m,p,r) FMethodPtrHelper<sizeof(void (c::*)())>::Convert(static_cast<r(c::*)p>(&c::m)), ASAutoCaller::MakeFunctionCaller(static_cast<r(c::*)p>(&c::m))
#define ERASE_FUNCTION_PTR(f,p,r) FGenericFuncPtr::Make(reinterpret_cast<FTypeErasedFuncPtr>(size_t(static_cast<r(*)p>(f)))), ASAutoCaller::MakeFunctionCaller(static_cast<r(*)p>(f))
#define ERASE_AUTO_METHOD_PTR(c,m) MakeAutoMethodPtr(&c::m), ASAutoCaller::MakeFunctionCaller(&c::m)
#define ERASE_AUTO_FUNCTION_PTR(f) MakeAutoFunctionPtr(&f), ASAutoCaller::MakeFunctionCaller(&f)

// Declare a dummy class so that we can determine the size of a simple method pointer
class FDummyClass {};
typedef void (FDummyClass::* FDummyMethodPtr)();
const int DummyMethodPtrSize = sizeof(FDummyMethodPtr);

// Define template
template <int N>
struct FMethodPtrHelper
{
	template<class M>
	FORCEINLINE static FGenericFuncPtr Convert(M Mthd)
	{
		// This version of the function should never be executed, nor compiled,
		// as it would mean that the size of the method pointer cannot be determined.

		int ERROR_UnsupportedMethodPtr[N - 100];

		FGenericFuncPtr p;
		p.Init(0);
		return p;
	}
};

template <typename MethodType>
FORCEINLINE FGenericFuncPtr MakeAutoMethodPtr(MethodType Method)
{
	return FMethodPtrHelper<sizeof(MethodType)>::Convert(Method);
}

template <typename FunctionType>
FORCEINLINE FGenericFuncPtr MakeAutoFunctionPtr(FunctionType Function)
{
	return FGenericFuncPtr::Make(reinterpret_cast<FTypeErasedFuncPtr>(size_t(Function)));
}

// Template specialization
template <>
struct FMethodPtrHelper<DummyMethodPtrSize>
{
	template<class M>
	FORCEINLINE static FGenericFuncPtr Convert(M Mthd)
	{
		// Mark this as a class method
		FGenericFuncPtr p;
		p.Init(3);
		p.CopyMethodPtr(&Mthd, DummyMethodPtrSize);
		return p;
	}
};

#if defined(_MSC_VER)
template <>
struct FMethodPtrHelper<DummyMethodPtrSize + 1 * sizeof(int)>
{
	template <class M>
	FORCEINLINE static FGenericFuncPtr Convert(M Mthd)
	{
		// Mark this as a class method
		FGenericFuncPtr p;
		p.Init(3);
		p.CopyMethodPtr(&Mthd, DummyMethodPtrSize + sizeof(int));
		return p;
	}
};

template <>
struct FMethodPtrHelper<DummyMethodPtrSize + 2 * sizeof(int)>
{
	template <class M>
	FORCEINLINE static FGenericFuncPtr Convert(M Mthd)
	{
		// On 32bit platforms with is where a class with virtual inheritance falls.
		// On 64bit platforms we can also fall here if 8byte data alignments is used.

		// Mark this as a class method
		FGenericFuncPtr p;
		p.Init(3);
		p.CopyMethodPtr(&Mthd, DummyMethodPtrSize + 2 * sizeof(int));

		return p;
	}
};

template <>
struct FMethodPtrHelper<DummyMethodPtrSize + 3 * sizeof(int)>
{
	template <class M>
	FORCEINLINE static FGenericFuncPtr Convert(M Mthd)
	{
		// Mark this as a class method
		FGenericFuncPtr p;
		p.Init(3);
		p.CopyMethodPtr(&Mthd, DummyMethodPtrSize + 3 * sizeof(int));
		return p;
	}
};

template <>
struct FMethodPtrHelper<DummyMethodPtrSize + 4 * sizeof(int)>
{
	template <class M>
	FORCEINLINE static FGenericFuncPtr Convert(M Mthd)
	{
		// On 64bit platforms with 8byte data alignment
		// the unknown class method pointers will come here.

		// Mark this as a class method
		FGenericFuncPtr p;
		p.Init(3);
		p.CopyMethodPtr(&Mthd, DummyMethodPtrSize + 4 * sizeof(int));
		return p;
	}
};

#endif

namespace ASAutoCaller
{
	class FFakeObject;
	using TFunctionPtr = void(*)();
	using TMethodPtr = void(FFakeObject::*)();

	struct FunctionCaller
	{
		using FunctionCallerPtr = void(*)(TFunctionPtr Method, void** Parameters, void* ReturnValue);
		using MethodCallerPtr = void(*)(TMethodPtr Function, void** Parameters, void* ReturnValue);

		union
		{
			MethodCallerPtr MethodPtr;
			FunctionCallerPtr FuncPtr;
		};
		int type;

		static FunctionCaller Make()
		{
			FunctionCaller Caller;
			Caller.FuncPtr = nullptr;
			Caller.type = 0;
			return Caller;
		}

		static FunctionCaller Make(FunctionCallerPtr InFunctionCaller)
		{
			FunctionCaller Caller;
			Caller.FuncPtr = InFunctionCaller;
			Caller.type = 1;
			return Caller;
		}

		static FunctionCaller Make(MethodCallerPtr InMethodCaller)
		{
			FunctionCaller Caller;
			Caller.MethodPtr = InMethodCaller;
			Caller.type = 2;
			return Caller;
		}

		bool IsBound() const { return MethodPtr != nullptr; }
	};

	template <typename T> struct TReferenceToPtr { typedef T Type; };
	template <typename T> struct TReferenceToPtr<T& > { typedef T* Type; };
	template <typename T> struct TReferenceToPtr<T&&> { typedef T* Type; };

	template<typename T>
	FORCEINLINE typename TEnableIf<TIsReferenceType<T>::Value, typename TRemoveReference<T>::Type*>::Type GetAddressIfReference(const T& Value)
	{
		return (typename TRemoveReference<T>::Type*) & Value;
	}

	template<typename T>
	FORCEINLINE typename TEnableIf<!TIsReferenceType<T>::Value, const T&>::Type GetAddressIfReference(const T& Value)
	{
		return Value;
	}

	template<typename T>
	FORCEINLINE typename TEnableIf<TIsPointer<T>::Value, T>::Type PassArgument(void* Value)
	{
		return *(T*)Value;
	}

	template<typename T>
	FORCEINLINE typename TEnableIf<!TIsPointer<T>::Value, typename TRemoveReference<T>::Type&>::Type PassArgument(void* Value)
	{
		return **(typename TRemoveReference<T>::Type**)Value;
	}

	template<typename ReturnType, typename... ParamTypes, int... TIndices>
	FORCEINLINE void IndexedFunctionCaller(TFunctionPtr FunctionPtr, void** Arguments, void* ReturnValue, TIntegerSequence<int, TIndices...>)
	{
		union
		{
			TFunctionPtr Function;
			ReturnType(*Casted)(ParamTypes...);
		} CastedFunctionPtr;
		CastedFunctionPtr.Casted = nullptr;
		CastedFunctionPtr.Function = FunctionPtr;
		new(ReturnValue) (typename TReferenceToPtr<ReturnType>::Type)(
			GetAddressIfReference<ReturnType>(
				(CastedFunctionPtr.Casted)(
					(PassArgument<ParamTypes>(Arguments + TIndices))...
					)
			)
			);
	}

	template<typename ReturnType, typename... ParamTypes>
	typename TEnableIf<!std::is_void<ReturnType>::value>::Type RedirectFunctionCaller(TFunctionPtr FunctionPtr, void** Arguments, void* ReturnValue)
	{
		IndexedFunctionCaller<ReturnType, ParamTypes...>(FunctionPtr, Arguments, ReturnValue, TMakeIntegerSequence<int, sizeof...(ParamTypes)>());
	}

	template<typename... ParamTypes, int... TIndices>
	FORCEINLINE void IndexedFunctionCallerVoid(TFunctionPtr FunctionPtr, void** Arguments, TIntegerSequence<int, TIndices...>)
	{
		union
		{
			TFunctionPtr Function;
			void(*Casted)(ParamTypes...);
		} CastedFunctionPtr;
		CastedFunctionPtr.Casted = nullptr;
		CastedFunctionPtr.Function = FunctionPtr;
		(CastedFunctionPtr.Casted)(
			(PassArgument<ParamTypes>(Arguments + TIndices))...
			);
	}

	template<typename ReturnType, typename... ParamTypes>
	typename TEnableIf<std::is_void<ReturnType>::value>::Type RedirectFunctionCaller(TFunctionPtr FunctionPtr, void** Arguments, void* ReturnValue)
	{
		IndexedFunctionCallerVoid<ParamTypes...>(FunctionPtr, Arguments, TMakeIntegerSequence<int, sizeof...(ParamTypes)>());
	}

	template<typename ReturnType, typename ObjectType, typename... ParamTypes, int... TIndices>
	FORCEINLINE void IndexedMethodCaller(TMethodPtr MethodPtr, void** Arguments, void* ReturnValue, TIntegerSequence<int, TIndices...>)
	{
		union
		{
			TMethodPtr Method;
			ReturnType(ObjectType::* Casted)(ParamTypes...);
		} CastedMethodPtr;
		CastedMethodPtr.Casted = nullptr;
		CastedMethodPtr.Method = MethodPtr;
		new(ReturnValue) (typename TReferenceToPtr<ReturnType>::Type)(
			GetAddressIfReference<ReturnType>(
				(((ObjectType*)Arguments[0])->*CastedMethodPtr.Casted)
				(
					(PassArgument<ParamTypes>(Arguments + TIndices + 1))...
					)
			)
			);
	}

	template<typename ReturnType, typename ObjectType, typename... ParamTypes>
	typename TEnableIf<!std::is_void<ReturnType>::value>::Type RedirectMethodCaller(TMethodPtr MethodPtr, void** Arguments, void* ReturnValue)
	{
		IndexedMethodCaller<ReturnType, ObjectType, ParamTypes...>(MethodPtr, Arguments, ReturnValue, TMakeIntegerSequence<int, sizeof...(ParamTypes)>());
	}

	template<typename ObjectType, typename... ParamTypes, int... TIndices>
	FORCEINLINE void IndexedMethodCallerVoid(TMethodPtr MethodPtr, void** Arguments, TIntegerSequence<int, TIndices...>)
	{
		union
		{
			TMethodPtr Method;
			void(ObjectType::* Casted)(ParamTypes...);
		} CastedMethodPtr;
		CastedMethodPtr.Casted = nullptr;
		CastedMethodPtr.Method = MethodPtr;
		(((ObjectType*)Arguments[0])->*CastedMethodPtr.Casted)(
			(PassArgument<ParamTypes>(Arguments + TIndices + 1))...
			);
	}

	template<typename ReturnType, typename ObjectType, typename... ParamTypes>
	typename TEnableIf<std::is_void<ReturnType>::value>::Type RedirectMethodCaller(TMethodPtr MethodPtr, void** Arguments, void* ReturnValue)
	{
		IndexedMethodCallerVoid<ObjectType, ParamTypes...>(MethodPtr, Arguments, TMakeIntegerSequence<int, sizeof...(ParamTypes)>());
	}

	template<typename ReturnType, typename... ParamTypes>
	FunctionCaller MakeFunctionCaller(ReturnType(*FunctionPtr)(ParamTypes...))
	{
		return FunctionCaller::Make(&RedirectFunctionCaller<ReturnType, ParamTypes...>);
	}

	template<typename ReturnType, typename... ParamTypes, typename ObjectType>
	FunctionCaller MakeFunctionCaller(ReturnType(ObjectType::* FunctionPtr)(ParamTypes...))
	{
		return FunctionCaller::Make(&RedirectMethodCaller<ReturnType, ObjectType, ParamTypes...>);
	}

	template<typename ReturnType, typename... ParamTypes, typename ObjectType>
	FunctionCaller MakeFunctionCaller(ReturnType(ObjectType::* FunctionPtr)(ParamTypes...) const)
	{
		return FunctionCaller::Make(&RedirectMethodCaller<ReturnType, ObjectType, ParamTypes...>);
	}

	template<typename ReturnType, typename... ParamTypes, typename ObjectType>
	FunctionCaller MakeFunctionCaller(ReturnType(ObjectType::* FunctionPtr)(ParamTypes...) const&)
	{
		return FunctionCaller::Make(&RedirectMethodCaller<ReturnType, ObjectType, ParamTypes...>);
	}
}
// END HAZE FIX
enum class EAngelscriptFunctionBindingOrigin : uint8
{
	Unknown,
	Manual,
	Generated,
	NativeModule,
	Reflective,
};

struct FAngelscriptFunctionBinding
{
	FGenericFuncPtr FunctionPointer;
	ASAutoCaller::FunctionCaller FunctionCaller;
	void* UserData = nullptr;
	bool bReflectiveFallbackBound = false;
	bool bUsesGenericCall = false;
	EAngelscriptFunctionBindingOrigin Origin =
		EAngelscriptFunctionBindingOrigin::Unknown;
};
