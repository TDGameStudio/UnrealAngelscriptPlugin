#include "Core/FunctionCallers.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptFunctionCallersTests,
	"Angelscript.TestModule.Engine.FunctionCallers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
struct FFunctionCallerHarness
{
	int32 Bias = 0;

	int32 AddToBias(int32 Value, int32& InOut)
	{
		InOut += Bias;
		return Bias + Value + InOut;
	}

	const int32& GetBiasRef() const
	{
		return Bias;
	}
};

static int32 GlobalAddAndBump(int32 Value, int32& InOut)
{
	InOut += 6;
	return Value + InOut;
}

static void CopyFromPtr(const int32* InValue, int32& OutValue)
{
	OutValue = InValue != nullptr ? *InValue : -1;
}

static void InvokeCaller(const FFuncEntry& Entry, void** Arguments, void* ReturnValue)
{
	if (Entry.Caller.type == 1)
	{
		Entry.Caller.FuncPtr(
			reinterpret_cast<ASAutoCaller::TFunctionPtr>(Entry.FuncPtr.ptr.f.func),
			Arguments,
			ReturnValue);
		return;
	}

	if (Entry.Caller.type == 2)
	{
		union FMethodPtrBridge
		{
			FTypeErasedMethodPtr Erased;
			ASAutoCaller::TMethodPtr Auto;
		};

		FMethodPtrBridge MethodPtrBridge;
		FMemory::Memzero(MethodPtrBridge);
		MethodPtrBridge.Erased = Entry.FuncPtr.ptr.m.mthd;
		Entry.Caller.MethodPtr(MethodPtrBridge.Auto, Arguments, ReturnValue);
	}
}

public:
	TEST_METHOD(DirectCallersRoundTripValueReferenceAndPointerArguments)
	{
FFuncEntry GlobalEntry = { ERASE_AUTO_FUNCTION_PTR(GlobalAddAndBump) };
		FFuncEntry MethodEntry = { ERASE_AUTO_METHOD_PTR(FFunctionCallerHarness, AddToBias) };
		FFuncEntry ConstMethodEntry = { ERASE_AUTO_METHOD_PTR(FFunctionCallerHarness, GetBiasRef) };
		FFuncEntry PointerEntry = { ERASE_AUTO_FUNCTION_PTR(CopyFromPtr) };

		ASSERT_THAT(IsTrue(GlobalEntry.FuncPtr.IsBound(), TEXT("Function caller round-trip test should bind the global direct-call pointer")));
		ASSERT_THAT(IsTrue(GlobalEntry.Caller.IsBound(), TEXT("Function caller round-trip test should bind the global caller thunk")));
		ASSERT_THAT(AreEqual(1, GlobalEntry.Caller.type, TEXT("Function caller round-trip test should tag the global caller as a function thunk")));
		ASSERT_THAT(IsTrue(MethodEntry.FuncPtr.IsBound(), TEXT("Function caller round-trip test should bind the method direct-call pointer")));
		ASSERT_THAT(IsTrue(MethodEntry.Caller.IsBound(), TEXT("Function caller round-trip test should bind the method caller thunk")));
		ASSERT_THAT(AreEqual(2, MethodEntry.Caller.type, TEXT("Function caller round-trip test should tag the method caller as a method thunk")));
		ASSERT_THAT(IsTrue(ConstMethodEntry.FuncPtr.IsBound(), TEXT("Function caller round-trip test should bind the const method direct-call pointer")));
		ASSERT_THAT(IsTrue(ConstMethodEntry.Caller.IsBound(), TEXT("Function caller round-trip test should bind the const method caller thunk")));
		ASSERT_THAT(AreEqual(2, ConstMethodEntry.Caller.type, TEXT("Function caller round-trip test should keep the const method on the method-thunk path")));
		ASSERT_THAT(IsTrue(PointerEntry.FuncPtr.IsBound(), TEXT("Function caller round-trip test should bind the pointer-argument direct-call pointer")));
		ASSERT_THAT(IsTrue(PointerEntry.Caller.IsBound(), TEXT("Function caller round-trip test should bind the pointer-argument caller thunk")));
		ASSERT_THAT(AreEqual(1, PointerEntry.Caller.type, TEXT("Function caller round-trip test should keep the pointer-argument function on the function-thunk path")));

		int32 GlobalValue = 9;
		int32 GlobalInOut = 4;
		int32 GlobalReturn = 0;
		void* GlobalArgs[] = { &GlobalValue, &GlobalInOut };
		InvokeCaller(GlobalEntry, GlobalArgs, &GlobalReturn);

		ASSERT_THAT(AreEqual(9, GlobalValue, TEXT("Function caller round-trip test should preserve the by-value global input")));
		ASSERT_THAT(AreEqual(10, GlobalInOut, TEXT("Function caller round-trip test should write back the by-reference global argument")));
		ASSERT_THAT(AreEqual(19, GlobalReturn, TEXT("Function caller round-trip test should return the expected global result")));

		FFunctionCallerHarness Harness;
		Harness.Bias = 11;

		int32 MethodValue = 5;
		int32 MethodInOut = 4;
		int32 MethodReturn = 0;
		void* MethodArgs[] = { &Harness, &MethodValue, &MethodInOut };
		InvokeCaller(MethodEntry, MethodArgs, &MethodReturn);

		ASSERT_THAT(AreEqual(5, MethodValue, TEXT("Function caller round-trip test should keep the by-value method input untouched")));
		ASSERT_THAT(AreEqual(15, MethodInOut, TEXT("Function caller round-trip test should route the reference method argument back to the caller")));
		ASSERT_THAT(AreEqual(31, MethodReturn, TEXT("Function caller round-trip test should dispatch methods against the object passed in Args[0]")));

		const int32* BiasRef = nullptr;
		void* ConstMethodArgs[] = { &Harness };
		InvokeCaller(ConstMethodEntry, ConstMethodArgs, &BiasRef);

		ASSERT_THAT(IsNotNull(BiasRef, TEXT("Function caller round-trip test should materialize the const method return reference as a stable pointer")));
		ASSERT_THAT(AreEqual(Harness.Bias, *BiasRef, TEXT("Function caller round-trip test should preserve the const reference value")));
		ASSERT_THAT(IsTrue(BiasRef == &Harness.Bias, TEXT("Function caller round-trip test should return a reference to the object field rather than a copied temporary")));

		int32 PointerSource = 27;
		const int32* PointerInput = &PointerSource;
		int32 PointerOut = 0;
		void* PointerArgs[] = { const_cast<int32*>(PointerInput), &PointerOut };
		InvokeCaller(PointerEntry, PointerArgs, nullptr);

		ASSERT_THAT(AreEqual(27, PointerOut, TEXT("Function caller round-trip test should dereference const pointer parameters and write the result to the referenced output")));
	}
};

#endif
