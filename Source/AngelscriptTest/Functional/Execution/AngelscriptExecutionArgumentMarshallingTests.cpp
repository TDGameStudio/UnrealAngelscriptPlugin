#include "CQTest.h"

#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"

// Test Layer: Runtime Integration
#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(
	FAngelscriptExecutionArgumentMarshallingTests,
	"Angelscript.TestModule.Functional.Execute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static constexpr ANSICHAR ModuleName[] = "ASExecutionArgumentSlotOrderMatrix";
	inline static constexpr ANSICHAR RefAddressModuleName[] = "ASExecutionRefAddressRoundTrip";

	static FString MakeArgumentMatrixScript(bool bFloatUsesFloat64)
	{
		return FString::Printf(
			TEXT("int Encode2(int A, int B) { return A * 100 + B; }\n")
			TEXT("int Encode4(int A, int B, int C, int D) { return A * 1000 + B * 100 + C * 10 + D; }\n")
			TEXT("int EncodeMixed(int A, %s B, int C) { return A * 1000 + int(B * 10) + C; }\n"),
			bFloatUsesFloat64 ? TEXT("double") : TEXT("float"));
	}

	static FString GetMixedDeclaration(bool bFloatUsesFloat64)
	{
		return bFloatUsesFloat64
			? TEXT("int EncodeMixed(int, double, int)")
			: TEXT("int EncodeMixed(int, float, int)");
	}

	static asQWORD EncodeDoubleArgument(double Value)
	{
		asQWORD EncodedValue = 0;
		FMemory::Memcpy(&EncodedValue, &Value, sizeof(Value));
		return EncodedValue;
	}

	static bool ExecuteArgumentCase(
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

	static bool RunArgumentSlotOrderMatrix(FAutomationTestBase& Test)
	{
		bool bPassed = false;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		do
		{
			asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
			if (!Test.TestNotNull(TEXT("Execution.ArgumentSlotOrderMatrix should expose the script engine"), ScriptEngine))
			{
				break;
			}

			const bool bFloatUsesFloat64 = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
			asIScriptModule* Module = BuildModule(
				Test,
				Engine,
				ModuleName,
				MakeArgumentMatrixScript(bFloatUsesFloat64));
			if (Module == nullptr)
			{
				break;
			}

			if (!ExecuteArgumentCase(
				Test,
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
				Test,
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
				Test,
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

	static bool RunRefAddressRoundTrip(FAutomationTestBase& Test)
	{
		bool bPassed = false;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		asIScriptModule* Module = BuildModule(
			Test,
			Engine,
			RefAddressModuleName,
			TEXT("int UseRefs(const int&in Input, int&out Output) { Output = Input + 5; return Output * 2; }"));
		if (Module == nullptr)
		{
			return false;
		}

		asIScriptFunction* Function = GetFunctionByDecl(Test, *Module, TEXT("int UseRefs(const int&in, int&out)"));
		if (Function == nullptr)
		{
			return false;
		}

		asIScriptContext* Context = Engine.CreateContext();
		if (!Test.TestNotNull(TEXT("Execution.RefAddressRoundTrip should create a context"), Context))
		{
			return false;
		}

		const int PrepareResult = Context->Prepare(Function);
		if (!Test.TestEqual(TEXT("Execution.RefAddressRoundTrip should prepare the entry point"), PrepareResult, static_cast<int32>(asSUCCESS)))
		{
			Context->Release();
			return false;
		}

		int32 Input = 10;
		int32 Output = -1;
		const int SetInputResult = Context->SetArgAddress(0, &Input);
		if (!Test.TestEqual(TEXT("Execution.RefAddressRoundTrip should bind the const ref input address"), SetInputResult, static_cast<int32>(asSUCCESS)))
		{
			Context->Release();
			return false;
		}

		const int SetOutputResult = Context->SetArgAddress(1, &Output);
		if (!Test.TestEqual(TEXT("Execution.RefAddressRoundTrip should bind the out ref output address"), SetOutputResult, static_cast<int32>(asSUCCESS)))
		{
			Context->Release();
			return false;
		}

		const int ExecuteResult = Context->Execute();
		if (!Test.TestEqual(TEXT("Execution.RefAddressRoundTrip should execute successfully"), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED)))
		{
			Context->Release();
			return false;
		}

		const bool bReturnMatched = Test.TestEqual(
			TEXT("Execution.RefAddressRoundTrip should return the doubled out value"),
			static_cast<int32>(Context->GetReturnDWord()),
			30);
		const bool bOutputMatched = Test.TestEqual(
			TEXT("Execution.RefAddressRoundTrip should write the expected out ref value"),
			Output,
			15);
		const bool bInputMatched = Test.TestEqual(
			TEXT("Execution.RefAddressRoundTrip should keep the const ref input unchanged"),
			Input,
			10);

		bPassed = bReturnMatched && bOutputMatched && bInputMatched;
		Context->Release();
		}
		return bPassed;
	}

public:
	TEST_METHOD(ArgumentSlotOrderMatrix)
	{
		ASSERT_THAT(IsTrue(RunArgumentSlotOrderMatrix(*TestRunner)));
	}

	TEST_METHOD(RefAddressRoundTrip)
	{
		ASSERT_THAT(IsTrue(RunRefAddressRoundTrip(*TestRunner)));
	}
};

TEST_CLASS_WITH_FLAGS(
	FAngelscriptExecutionOneArgArgumentMarshallingTests,
	"Angelscript.TestModule.Functional.Execute.OneArg",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static constexpr ANSICHAR OneArgModuleName[] = "ASExecutionOneArgNegativeAndZero";

	struct FOneArgCase
	{
		const TCHAR* Name = TEXT("");
		int32 InputValue = 0;
		int32 ExpectedReturnValue = 0;
	};

	static bool ExecuteOneArgCase(
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

	static bool RunOneArgNegativeAndZero(FAutomationTestBase& Test)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		asIScriptModule* Module = BuildModule(
			Test,
			Engine,
			OneArgModuleName,
			TEXT("int Test(int Value) { return Value * 2; }"));
		if (Module == nullptr)
		{
			return false;
		}

		asIScriptFunction* Function = GetFunctionByDecl(Test, *Module, TEXT("int Test(int)"));
		if (Function == nullptr)
		{
			return false;
		}

		asIScriptContext* Context = Engine.CreateContext();
		if (!Test.TestNotNull(TEXT("Execution.OneArg.NegativeAndZero should create a reusable context"), Context))
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
			if (!ExecuteOneArgCase(Test, *Context, *Function, OneArgCase))
			{
				Context->Release();
				return false;
			}
		}

		Context->Release();
		}
		return true;
	}

public:
	TEST_METHOD(NegativeAndZero)
	{
		ASSERT_THAT(IsTrue(RunOneArgNegativeAndZero(*TestRunner)));
	}
};

TEST_CLASS_WITH_FLAGS(
	FAngelscriptExecutionDoubleArgArgumentMarshallingTests,
	"Angelscript.TestModule.Functional.Execute.DoubleArg",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static constexpr ANSICHAR DoubleArgModuleName[] = "ASExecutionDoubleArgDirectApiRoundTrip";

	static bool RunDoubleArgDirectApiRoundTrip(FAutomationTestBase& Test)
	{
		bool bPassed = false;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
		if (!Test.TestNotNull(TEXT("Execution.DoubleArg.DirectApiRoundTrip should expose the script engine"), ScriptEngine))
		{
			return false;
		}

		if (!Test.TestTrue(TEXT("Execution.DoubleArg.DirectApiRoundTrip should enable the double type"), ScriptEngine->GetEngineProperty(asEP_ALLOW_DOUBLE_TYPE) != 0))
		{
			return false;
		}

		asIScriptModule* Module = BuildModule(
			Test,
			Engine,
			DoubleArgModuleName,
			TEXT("double Test(double Value) { return Value * 1.5 + 0.25; }"));
		if (Module == nullptr)
		{
			return false;
		}

		asIScriptFunction* Function = GetFunctionByDecl(Test, *Module, TEXT("double Test(double)"));
		if (Function == nullptr)
		{
			return false;
		}

		asIScriptContext* Context = Engine.CreateContext();
		if (!Test.TestNotNull(TEXT("Execution.DoubleArg.DirectApiRoundTrip should create a context"), Context))
		{
			return false;
		}

		const int PrepareResult = Context->Prepare(Function);
		if (!Test.TestEqual(TEXT("Execution.DoubleArg.DirectApiRoundTrip should prepare the entry point"), PrepareResult, static_cast<int32>(asSUCCESS)))
		{
			Context->Release();
			return false;
		}

		const int SetArgResult = Context->SetArgDouble(0, 20.5);
		if (!Test.TestEqual(TEXT("Execution.DoubleArg.DirectApiRoundTrip should bind the double argument through SetArgDouble"), SetArgResult, static_cast<int32>(asSUCCESS)))
		{
			Context->Release();
			return false;
		}

		const int ExecuteResult = Context->Execute();
		if (!Test.TestEqual(TEXT("Execution.DoubleArg.DirectApiRoundTrip should execute successfully"), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED)))
		{
			Context->Release();
			return false;
		}

		bPassed = Test.TestEqual(
			TEXT("Execution.DoubleArg.DirectApiRoundTrip should preserve the double return value through GetReturnDouble"),
			Context->GetReturnDouble(),
			31.0,
			0.0001);
		Context->Release();
		}
		return bPassed;
	}

public:
	TEST_METHOD(DirectApiRoundTrip)
	{
		ASSERT_THAT(IsTrue(RunDoubleArgDirectApiRoundTrip(*TestRunner)));
	}
};

#endif
