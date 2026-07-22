#include "AngelscriptTestUtilities.h"

// Core erased-function-caller coverage.
#include "AngelscriptTestMacros.h"
#include "../../AngelscriptRuntime/Core/FunctionCallers.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FFunctionCallerErasureTests,
	"Angelscript.TestModule.Engine.FunctionCallers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FConstRefQualifiedProbe
	{
		int32 Base = 11;

		int32 ReadPlus(int32 Delta) const&
		{
			return Base + Delta;
		}
	};

	template <typename MethodType>
	static ASAutoCaller::TMethodPtr MakeErasedMethodPointer(MethodType Method)
	{
		const FGenericFuncPtr GenericMethod = MakeAutoMethodPtr(Method);
		ASAutoCaller::TMethodPtr ErasedMethod = nullptr;
		FMemory::Memcpy(&ErasedMethod, GenericMethod.ptr.dummy, sizeof(ErasedMethod));
		return ErasedMethod;
	}

public:
	TEST_METHOD(ConstRefQualifiedMethodCaller)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		using FProbeMethod = int32 (FConstRefQualifiedProbe::*)(int32) const&;
		const FProbeMethod Method = &FConstRefQualifiedProbe::ReadPlus;

		FGenericFuncPtr GenericMethod = MakeAutoMethodPtr(Method);
		const ASAutoCaller::FunctionCaller Caller = ASAutoCaller::MakeFunctionCaller(Method);
		ASAutoCaller::TMethodPtr ErasedMethod = MakeErasedMethodPointer(Method);

		ASSERT_THAT(IsTrue(GenericMethod.IsBound(),
			TEXT("const& qualified method caller should produce a bound generic method pointer")));
		ASSERT_THAT(AreEqual(3, static_cast<int32>(GenericMethod.flag),
			TEXT("const& qualified method caller should encode the method pointer as a class method")));
		ASSERT_THAT(IsTrue(Caller.IsBound(),
			TEXT("const& qualified method caller should produce a bound auto caller")));
		ASSERT_THAT(AreEqual(2, Caller.type,
			TEXT("const& qualified method caller should select the method caller path")));

		FConstRefQualifiedProbe Probe;
		int32 Delta = 5;
		int32 Result = 0;
		void* Arguments[] =
		{
			&Probe,
			&Delta,
		};

		Caller.MethodPtr(ErasedMethod, Arguments, &Result);

		ASSERT_THAT(AreEqual(16, Result,
			TEXT("const& qualified method caller should preserve the erased return value")));
		ASSERT_THAT(AreEqual(11, Probe.Base,
			TEXT("const& qualified method caller should not mutate the probe object")));

		}
	}
};

#endif
