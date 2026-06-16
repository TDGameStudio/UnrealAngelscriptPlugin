#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"
#include "../../AngelscriptRuntime/Core/FunctionCallers.h"

#include "CQTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FConstRefQualifiedProbe
	{
		int32 Base = 11;

		int32 ReadPlus(int32 Delta) const&
		{
			return Base + Delta;
		}
	};

	template <typename MethodType>
	ASAutoCaller::TMethodPtr MakeErasedMethodPointer(MethodType Method)
	{
		const FGenericFuncPtr GenericMethod = MakeAutoMethodPtr(Method);
		ASAutoCaller::TMethodPtr ErasedMethod = nullptr;
		FMemory::Memcpy(&ErasedMethod, GenericMethod.ptr.dummy, sizeof(ErasedMethod));
		return ErasedMethod;
	}
}


TEST_CLASS_WITH_FLAGS(FAngelscriptFunctionCallerErasureTests,
	"Angelscript.TestModule.AngelScriptSDK.FunctionCallers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ConstRefQualifiedMethodCaller)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		using FProbeMethod = int32 (FConstRefQualifiedProbe::*)(int32) const&;
		const FProbeMethod Method = &FConstRefQualifiedProbe::ReadPlus;

		FGenericFuncPtr GenericMethod = MakeAutoMethodPtr(Method);
		const ASAutoCaller::FunctionCaller Caller = ASAutoCaller::MakeFunctionCaller(Method);
		ASAutoCaller::TMethodPtr ErasedMethod = MakeErasedMethodPointer(Method);

		TestRunner->TestTrue(TEXT("const& qualified method caller should produce a bound generic method pointer"), GenericMethod.IsBound());
		TestRunner->TestEqual(TEXT("const& qualified method caller should encode the method pointer as a class method"), static_cast<int32>(GenericMethod.flag), 3);
		TestRunner->TestTrue(TEXT("const& qualified method caller should produce a bound auto caller"), Caller.IsBound());
		TestRunner->TestEqual(TEXT("const& qualified method caller should select the method caller path"), Caller.type, 2);

		FConstRefQualifiedProbe Probe;
		int32 Delta = 5;
		int32 Result = 0;
		void* Arguments[] =
		{
			&Probe,
			&Delta,
		};

		Caller.MethodPtr(ErasedMethod, Arguments, &Result);

		TestRunner->TestEqual(TEXT("const& qualified method caller should preserve the erased return value"), Result, 16);
		TestRunner->TestEqual(TEXT("const& qualified method caller should not mutate the probe object"), Probe.Base, 11);

		}
	}
};

#endif
