#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestMacros.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_Angelscript_AngelscriptExecutionArgumentMarshallingTests_Private
{
	constexpr ANSICHAR ModuleName[] = "ASExecutionArgumentSlotOrderMatrix";
	constexpr ANSICHAR OneArgModuleName[] = "ASExecutionOneArgNegativeAndZero";
	constexpr ANSICHAR RefAddressModuleName[] = "ASExecutionRefAddressRoundTrip";
	constexpr ANSICHAR DoubleArgModuleName[] = "ASExecutionDoubleArgDirectApiRoundTrip";

	FString MakeArgumentMatrixScript(bool bFloatUsesFloat64)
	{
		return FString::Printf(
			TEXT("int Encode2(int A, int B) { return A * 100 + B; }\n")
			TEXT("int Encode4(int A, int B, int C, int D) { return A * 1000 + B * 100 + C * 10 + D; }\n")
			TEXT("int EncodeMixed(int A, %s B, int C) { return A * 1000 + int(B * 10) + C; }\n"),
			bFloatUsesFloat64 ? TEXT("double") : TEXT("float"));
	}

	FString GetMixedDeclaration(bool bFloatUsesFloat64)
	{
		return bFloatUsesFloat64
			? TEXT("int EncodeMixed(int, double, int)")
			: TEXT("int EncodeMixed(int, float, int)");
	}

	asQWORD EncodeDoubleArgument(double Value)
	{
		asQWORD EncodedValue = 0;
		FMemory::Memcpy(&EncodedValue, &Value, sizeof(Value));
		return EncodedValue;
	}

	bool ExecuteArgumentCase(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		asIScriptModule& Module,
		const FString& Declaration,
		const FString& CaseName,
		TFunctionRef<void(asIScriptContext&)> BindArguments,
		int32 ExpectedReturnValue)
	{
		asIScriptFunction* Function = GetFunctionByDecl(Test, Module, Declaration);
		if (Function == nullptr)
		{
			return false;
		}

		asIScriptContext* Context = Engine.CreateContext();
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s should create a context"), *CaseName), Context))
		{
			return false;
		}

		const int PrepareResult = Context->Prepare(Function);
		if (!Test.TestEqual(*FString::Printf(TEXT("%s should prepare the entry point"), *CaseName), PrepareResult, static_cast<int32>(asSUCCESS)))
		{
			Context->Release();
			return false;
		}

		BindArguments(*Context);

		const int ExecuteResult = Context->Execute();
		if (!Test.TestEqual(*FString::Printf(TEXT("%s should finish execution"), *CaseName), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED)))
		{
			Context->Release();
			return false;
		}

		const bool bMatched = Test.TestEqual(
			*FString::Printf(TEXT("%s should preserve slot order in the encoded return value"), *CaseName),
			static_cast<int32>(Context->GetReturnDWord()),
			ExpectedReturnValue);
		Context->Release();
		return bMatched;
	}

	struct FOneArgCase
	{
		const TCHAR* Name = TEXT("");
		int32 InputValue = 0;
		int32 ExpectedReturnValue = 0;
	};

	bool ExecuteOneArgCase(
		FAutomationTestBase& Test,
		asIScriptContext& Context,
		asIScriptFunction& Function,
		const FOneArgCase& OneArgCase)
	{
		const int PrepareResult = Context.Prepare(&Function);
		if (!Test.TestEqual(*FString::Printf(TEXT("%s should prepare the entry point"), OneArgCase.Name), PrepareResult, static_cast<int32>(asSUCCESS)))
		{
			return false;
		}

		Context.SetArgDWord(0, static_cast<asDWORD>(OneArgCase.InputValue));

		const int ExecuteResult = Context.Execute();
		if (!Test.TestEqual(*FString::Printf(TEXT("%s should execute successfully"), OneArgCase.Name), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED)))
		{
			Context.Unprepare();
			return false;
		}

		const bool bMatched = Test.TestEqual(
			*FString::Printf(TEXT("%s should preserve the signed int return value"), OneArgCase.Name),
			static_cast<int32>(Context.GetReturnDWord()),
			OneArgCase.ExpectedReturnValue);
		Context.Unprepare();
		return bMatched;
	}
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptExecutionArgumentSlotOrderMatrixTest,
	"Angelscript.TestModule.Functional.Execute.ArgumentSlotOrderMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptExecutionArgumentSlotOrderMatrixTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptExecutionArgumentMarshallingTests_Private;
	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	do
	{
		asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
		if (!TestNotNull(TEXT("Execution.ArgumentSlotOrderMatrix should expose the script engine"), ScriptEngine))
		{
			break;
		}

		const bool bFloatUsesFloat64 = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
		asIScriptModule* Module = BuildModule(
			*this,
			Engine,
			ModuleName,
			MakeArgumentMatrixScript(bFloatUsesFloat64));
		if (Module == nullptr)
		{
			break;
		}

		if (!ExecuteArgumentCase(
				*this,
				Engine,
				*Module,
				TEXT("int Encode2(int, int)"),
				TEXT("Execution.ArgumentSlotOrderMatrix.Encode2"),
				[](asIScriptContext& Context)
				{
					Context.SetArgDWord(0, 20);
					Context.SetArgDWord(1, 22);
				},
				2022))
		{
			break;
		}

		if (!ExecuteArgumentCase(
				*this,
				Engine,
				*Module,
				TEXT("int Encode4(int, int, int, int)"),
				TEXT("Execution.ArgumentSlotOrderMatrix.Encode4"),
				[](asIScriptContext& Context)
				{
					Context.SetArgDWord(0, 1);
					Context.SetArgDWord(1, 2);
					Context.SetArgDWord(2, 3);
					Context.SetArgDWord(3, 4);
				},
				1234))
		{
			break;
		}

		const FString MixedDeclaration = GetMixedDeclaration(bFloatUsesFloat64);
		if (!ExecuteArgumentCase(
				*this,
				Engine,
				*Module,
				MixedDeclaration,
				TEXT("Execution.ArgumentSlotOrderMatrix.EncodeMixed"),
				[bFloatUsesFloat64](asIScriptContext& Context)
				{
					Context.SetArgDWord(0, 7);
					if (bFloatUsesFloat64)
					{
						Context.SetArgQWord(1, EncodeDoubleArgument(2.5));
					}
					else
					{
						Context.SetArgFloat(1, 2.5f);
					}
					Context.SetArgDWord(2, 9);
				},
				7034))
		{
			break;
		}

		bPassed = true;
	}
	while (false);

	}
	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptExecutionOneArgNegativeAndZeroTest,
	"Angelscript.TestModule.Functional.Execute.OneArg.NegativeAndZero",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptExecutionRefAddressRoundTripTest,
	"Angelscript.TestModule.Functional.Execute.RefAddressRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptExecutionDoubleArgDirectApiRoundTripTest,
	"Angelscript.TestModule.Functional.Execute.DoubleArg.DirectApiRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptExecutionOneArgNegativeAndZeroTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptExecutionArgumentMarshallingTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		OneArgModuleName,
		TEXT("int Test(int Value) { return Value * 2; }"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Test(int)"));
	if (Function == nullptr)
	{
		return false;
	}

	asIScriptContext* Context = Engine.CreateContext();
	if (!TestNotNull(TEXT("Execution.OneArg.NegativeAndZero should create a reusable context"), Context))
	{
		return false;
	}

	const FOneArgCase Cases[] =
	{
		{ TEXT("Execution.OneArg.NegativeAndZero zero case"), 0, 0 },
		{ TEXT("Execution.OneArg.NegativeAndZero negative case"), -21, -42 },
	};

	for (const FOneArgCase& OneArgCase : Cases)
	{
		if (!ExecuteOneArgCase(*this, *Context, *Function, OneArgCase))
		{
			Context->Release();
			return false;
		}
	}

	Context->Release();
	}
	return true;
}

bool FAngelscriptExecutionRefAddressRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptExecutionArgumentMarshallingTests_Private;
	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		RefAddressModuleName,
		TEXT("int UseRefs(const int&in Input, int&out Output) { Output = Input + 5; return Output * 2; }"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int UseRefs(const int&in, int&out)"));
	if (Function == nullptr)
	{
		return false;
	}

	asIScriptContext* Context = Engine.CreateContext();
	if (!TestNotNull(TEXT("Execution.RefAddressRoundTrip should create a context"), Context))
	{
		return false;
	}

	const int PrepareResult = Context->Prepare(Function);
	if (!TestEqual(TEXT("Execution.RefAddressRoundTrip should prepare the entry point"), PrepareResult, static_cast<int32>(asSUCCESS)))
	{
		Context->Release();
		return false;
	}

	int32 Input = 10;
	int32 Output = -1;
	const int SetInputResult = Context->SetArgAddress(0, &Input);
	if (!TestEqual(TEXT("Execution.RefAddressRoundTrip should bind the const ref input address"), SetInputResult, static_cast<int32>(asSUCCESS)))
	{
		Context->Release();
		return false;
	}

	const int SetOutputResult = Context->SetArgAddress(1, &Output);
	if (!TestEqual(TEXT("Execution.RefAddressRoundTrip should bind the out ref output address"), SetOutputResult, static_cast<int32>(asSUCCESS)))
	{
		Context->Release();
		return false;
	}

	const int ExecuteResult = Context->Execute();
	if (!TestEqual(TEXT("Execution.RefAddressRoundTrip should execute successfully"), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED)))
	{
		Context->Release();
		return false;
	}

	const bool bReturnMatched = TestEqual(
		TEXT("Execution.RefAddressRoundTrip should return the doubled out value"),
		static_cast<int32>(Context->GetReturnDWord()),
		30);
	const bool bOutputMatched = TestEqual(
		TEXT("Execution.RefAddressRoundTrip should write the expected out ref value"),
		Output,
		15);
	const bool bInputMatched = TestEqual(
		TEXT("Execution.RefAddressRoundTrip should keep the const ref input unchanged"),
		Input,
		10);

	bPassed = bReturnMatched && bOutputMatched && bInputMatched;
	Context->Release();
	}
	return bPassed;
}

bool FAngelscriptExecutionDoubleArgDirectApiRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptExecutionArgumentMarshallingTests_Private;
	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
	if (!TestNotNull(TEXT("Execution.DoubleArg.DirectApiRoundTrip should expose the script engine"), ScriptEngine))
	{
		return false;
	}

	if (!TestTrue(TEXT("Execution.DoubleArg.DirectApiRoundTrip should enable the double type"), ScriptEngine->GetEngineProperty(asEP_ALLOW_DOUBLE_TYPE) != 0))
	{
		return false;
	}

	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		DoubleArgModuleName,
		TEXT("double Test(double Value) { return Value * 1.5 + 0.25; }"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("double Test(double)"));
	if (Function == nullptr)
	{
		return false;
	}

	asIScriptContext* Context = Engine.CreateContext();
	if (!TestNotNull(TEXT("Execution.DoubleArg.DirectApiRoundTrip should create a context"), Context))
	{
		return false;
	}

	const int PrepareResult = Context->Prepare(Function);
	if (!TestEqual(TEXT("Execution.DoubleArg.DirectApiRoundTrip should prepare the entry point"), PrepareResult, static_cast<int32>(asSUCCESS)))
	{
		Context->Release();
		return false;
	}

	const int SetArgResult = Context->SetArgDouble(0, 20.5);
	if (!TestEqual(TEXT("Execution.DoubleArg.DirectApiRoundTrip should bind the double argument through SetArgDouble"), SetArgResult, static_cast<int32>(asSUCCESS)))
	{
		Context->Release();
		return false;
	}

	const int ExecuteResult = Context->Execute();
	if (!TestEqual(TEXT("Execution.DoubleArg.DirectApiRoundTrip should execute successfully"), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED)))
	{
		Context->Release();
		return false;
	}

	bPassed = TestEqual(
		TEXT("Execution.DoubleArg.DirectApiRoundTrip should preserve the double return value through GetReturnDouble"),
		Context->GetReturnDouble(),
		31.0,
		0.0001);
	Context->Release();
	}
	return bPassed;
}

#endif
