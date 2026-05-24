#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestMacros.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptExecutionBasicTest,
	"Angelscript.TestModule.Functional.Execute.Basic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptExecutionBasicTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASExecutionBasic",
		TEXT("void TestVoid() {} int TestValue() { return 42; }"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* VoidFunction = GetFunctionByDecl(*this, *Module, TEXT("void TestVoid()"));
	if (VoidFunction == nullptr)
	{
		return false;
	}

	asIScriptContext* Context = Engine.CreateContext();
	if (!TestNotNull(TEXT("Execution.Basic should create a context for the void function"), Context))
	{
		return false;
	}

	const int PrepareVoidResult = Context->Prepare(VoidFunction);
	const int ExecuteVoidResult = PrepareVoidResult == asSUCCESS ? Context->Execute() : PrepareVoidResult;
	TestEqual(TEXT("Execution.Basic should prepare the void function"), PrepareVoidResult, static_cast<int32>(asSUCCESS));
	TestEqual(TEXT("Execution.Basic should execute the void function"), ExecuteVoidResult, static_cast<int32>(asEXECUTION_FINISHED));
	Context->Release();

	asIScriptFunction* ValueFunction = GetFunctionByDecl(*this, *Module, TEXT("int TestValue()"));
	if (ValueFunction == nullptr)
	{
		return false;
	}

	int32 Result = 0;
	if (!ExecuteIntFunction(*this, Engine, *ValueFunction, Result))
	{
		return false;
	}

	TestEqual(TEXT("Execution.Basic should return 42 from the value function"), Result, 42);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptExecutionOneArgTest,
	"Angelscript.TestModule.Functional.Execute.OneArg",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptExecutionOneArgTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASExecutionOneArg",
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
	if (!TestNotNull(TEXT("Execution.OneArg should create a context"), Context))
	{
		return false;
	}

	const int PrepareResult = Context->Prepare(Function);
	if (!TestEqual(TEXT("Execution.OneArg should prepare the entry point"), PrepareResult, static_cast<int32>(asSUCCESS)))
	{
		Context->Release();
		return false;
	}

	Context->SetArgDWord(0, 21);
	const int ExecuteResult = Context->Execute();
	if (!TestEqual(TEXT("Execution.OneArg should execute the entry point"), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED)))
	{
		Context->Release();
		return false;
	}

	TestEqual(TEXT("Execution.OneArg should double the provided value"), static_cast<int32>(Context->GetReturnDWord()), 42);
	Context->Release();
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptExecutionTwoArgsTest,
	"Angelscript.TestModule.Functional.Execute.TwoArgs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptExecutionTwoArgsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASExecutionTwoArgs",
		TEXT("int Test(int A, int B) { return A + B; }"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Test(int, int)"));
	if (Function == nullptr)
	{
		return false;
	}

	asIScriptContext* Context = Engine.CreateContext();
	if (!TestNotNull(TEXT("Execution.TwoArgs should create a context"), Context))
	{
		return false;
	}

	const int PrepareResult = Context->Prepare(Function);
	if (!TestEqual(TEXT("Execution.TwoArgs should prepare the entry point"), PrepareResult, static_cast<int32>(asSUCCESS)))
	{
		Context->Release();
		return false;
	}

	Context->SetArgDWord(0, 20);
	Context->SetArgDWord(1, 22);
	const int ExecuteResult = Context->Execute();
	if (!TestEqual(TEXT("Execution.TwoArgs should execute the entry point"), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED)))
	{
		Context->Release();
		return false;
	}

	TestEqual(TEXT("Execution.TwoArgs should sum both arguments"), static_cast<int32>(Context->GetReturnDWord()), 42);
	Context->Release();
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptExecutionFourArgsTest,
	"Angelscript.TestModule.Functional.Execute.FourArgs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptExecutionFourArgsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASExecutionFourArgs",
		TEXT("int Test(int A, int B, int C, int D) { return A + B + C + D; }"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Test(int, int, int, int)"));
	if (Function == nullptr)
	{
		return false;
	}

	asIScriptContext* Context = Engine.CreateContext();
	if (!TestNotNull(TEXT("Execution.FourArgs should create a context"), Context))
	{
		return false;
	}

	const int PrepareResult = Context->Prepare(Function);
	if (!TestEqual(TEXT("Execution.FourArgs should prepare the entry point"), PrepareResult, static_cast<int32>(asSUCCESS)))
	{
		Context->Release();
		return false;
	}

	Context->SetArgDWord(0, 10);
	Context->SetArgDWord(1, 10);
	Context->SetArgDWord(2, 10);
	Context->SetArgDWord(3, 12);
	const int ExecuteResult = Context->Execute();
	if (!TestEqual(TEXT("Execution.FourArgs should execute the entry point"), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED)))
	{
		Context->Release();
		return false;
	}

	TestEqual(TEXT("Execution.FourArgs should sum all arguments"), static_cast<int32>(Context->GetReturnDWord()), 42);
	Context->Release();
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptExecutionMixedArgsTest,
	"Angelscript.TestModule.Functional.Execute.MixedArgs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptExecutionInt64QWordRoundTripTest,
	"Angelscript.TestModule.Functional.Execute.Int64QWordRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptExecutionMixedArgsTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
	if (!TestNotNull(TEXT("Execution.MixedArgs should expose the script engine"), ScriptEngine))
	{
		return false;
	}

	const bool bFloatUsesFloat64 = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
	const FString Script = bFloatUsesFloat64
		? TEXT("double Test(int A, double B, int C) { return A + B + C; }")
		: TEXT("float Test(int A, float B, int C) { return A + B + C; }");
	const FString Declaration = bFloatUsesFloat64
		? TEXT("double Test(int, double, int)")
		: TEXT("float Test(int, float, int)");

	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASExecutionMixedArgs",
		Script);
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, Declaration);
	if (Function == nullptr)
	{
		return false;
	}

	asIScriptContext* Context = Engine.CreateContext();
	if (!TestNotNull(TEXT("Execution.MixedArgs should create a context"), Context))
	{
		return false;
	}

	const int PrepareResult = Context->Prepare(Function);
	if (!TestEqual(TEXT("Execution.MixedArgs should prepare the entry point"), PrepareResult, static_cast<int32>(asSUCCESS)))
	{
		Context->Release();
		return false;
	}

	Context->SetArgDWord(0, 10);
	if (bFloatUsesFloat64)
	{
		const double DoubleArg = 20.5;
		asQWORD EncodedDoubleArg = 0;
		FMemory::Memcpy(&EncodedDoubleArg, &DoubleArg, sizeof(DoubleArg));
		Context->SetArgQWord(1, EncodedDoubleArg);
	}
	else
	{
		Context->SetArgFloat(1, 20.5f);
	}
	Context->SetArgDWord(2, 12);
	const int ExecuteResult = Context->Execute();
	if (!TestEqual(TEXT("Execution.MixedArgs should execute the entry point"), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED)))
	{
		Context->Release();
		return false;
	}

	if (bFloatUsesFloat64)
	{
		const asQWORD EncodedReturnValue = Context->GetReturnQWord();
		double ReturnValue = 0.0;
		FMemory::Memcpy(&ReturnValue, &EncodedReturnValue, sizeof(ReturnValue));
		TestTrue(TEXT("Execution.MixedArgs should preserve the mixed-argument result in float64 mode"), FMath::IsNearlyEqual(ReturnValue, 42.5, 0.001));
	}
	else
	{
		TestEqual(TEXT("Execution.MixedArgs should preserve the mixed-argument result"), Context->GetReturnFloat(), 42.5f, 0.001f);
	}
	Context->Release();
	}

	return true;
}

bool FAngelscriptExecutionInt64QWordRoundTripTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASExecutionInt64QWordRoundTrip",
		TEXT("int64 AddFive(int64 Value) { return Value + 5; } int64 Negate(int64 Value) { return -Value; }"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* AddFiveFunction = GetFunctionByDecl(*this, *Module, TEXT("int64 AddFive(int64)"));
	if (AddFiveFunction == nullptr)
	{
		return false;
	}

	asIScriptFunction* NegateFunction = GetFunctionByDecl(*this, *Module, TEXT("int64 Negate(int64)"));
	if (NegateFunction == nullptr)
	{
		return false;
	}

	auto EncodeInt64 = [](int64 Value) -> asQWORD
	{
		asQWORD EncodedValue = 0;
		FMemory::Memcpy(&EncodedValue, &Value, sizeof(Value));
		return EncodedValue;
	};

	auto DecodeInt64 = [](asQWORD EncodedValue) -> int64
	{
		int64 DecodedValue = 0;
		FMemory::Memcpy(&DecodedValue, &EncodedValue, sizeof(DecodedValue));
		return DecodedValue;
	};

	auto ExecuteInt64Case = [this, &Engine, &EncodeInt64, &DecodeInt64](const FString& CaseName, asIScriptFunction& Function, int64 ArgumentValue, int64 ExpectedValue) -> bool
	{
		asIScriptContext* Context = Engine.CreateContext();
		if (!TestNotNull(*FString::Printf(TEXT("%s should create a context"), *CaseName), Context))
		{
			return false;
		}

		const int PrepareResult = Context->Prepare(&Function);
		if (!TestEqual(*FString::Printf(TEXT("%s should prepare the function"), *CaseName), PrepareResult, static_cast<int32>(asSUCCESS)))
		{
			Context->Release();
			return false;
		}

		Context->SetArgQWord(0, EncodeInt64(ArgumentValue));
		const int ExecuteResult = Context->Execute();
		if (!TestEqual(*FString::Printf(TEXT("%s should execute successfully"), *CaseName), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED)))
		{
			Context->Release();
			return false;
		}

		const int64 ReturnValue = DecodeInt64(Context->GetReturnQWord());
		const bool bMatched = TestEqual(*FString::Printf(TEXT("%s should preserve the expected int64 result"), *CaseName), ReturnValue, ExpectedValue);
		Context->Release();
		return bMatched;
	};

	if (!ExecuteInt64Case(TEXT("Execution.Int64QWordRoundTrip.AddFive"), *AddFiveFunction, static_cast<int64>(1099511627776LL), static_cast<int64>(1099511627781LL)))
	{
		return false;
	}

	if (!ExecuteInt64Case(TEXT("Execution.Int64QWordRoundTrip.Negate"), *NegateFunction, static_cast<int64>(-7), static_cast<int64>(7)))
	{
		return false;
	}

	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptExecutionContextTest,
	"Angelscript.TestModule.Functional.Execute.Context",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptExecutionContextTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	asIScriptContext* ContextA = Engine.CreateContext();
	if (!TestNotNull(TEXT("Execution.Context should create the first context"), ContextA))
	{
		return false;
	}

	asIScriptContext* ContextB = Engine.CreateContext();
	if (!TestNotNull(TEXT("Execution.Context should create the second context"), ContextB))
	{
		ContextA->Release();
		return false;
	}

	TestNotEqual(TEXT("Execution.Context should return distinct contexts"), ContextA, ContextB);
	ContextB->Release();

	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASExecutionContext",
		TEXT("void Test() {}"));
	if (Module == nullptr)
	{
		ContextA->Release();
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("void Test()"));
	if (Function == nullptr)
	{
		ContextA->Release();
		return false;
	}

	TestEqual(TEXT("Execution.Context should start uninitialized"), static_cast<int32>(ContextA->GetState()), static_cast<int32>(asEXECUTION_UNINITIALIZED));

	const int PrepareResult = ContextA->Prepare(Function);
	if (!TestEqual(TEXT("Execution.Context should prepare the function"), PrepareResult, static_cast<int32>(asSUCCESS)))
	{
		ContextA->Release();
		return false;
	}

	TestEqual(TEXT("Execution.Context should report a prepared state after prepare"), static_cast<int32>(ContextA->GetState()), static_cast<int32>(asEXECUTION_PREPARED));

	const int ExecuteResult = ContextA->Execute();
	if (!TestEqual(TEXT("Execution.Context should execute the function"), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED)))
	{
		ContextA->Release();
		return false;
	}

	TestEqual(TEXT("Execution.Context should report a finished state after execute"), static_cast<int32>(ContextA->GetState()), static_cast<int32>(asEXECUTION_FINISHED));
	ContextA->Release();
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptExecutionNestedTest,
	"Angelscript.TestModule.Functional.Execute.Nested",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptExecutionNestedTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASExecutionNested",
		TEXT("int Outer(int Value) { return Inner(Value) + 1; } int Inner(int Value) { return Value * 2; }"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Outer(int)"));
	if (Function == nullptr)
	{
		return false;
	}

	asIScriptContext* Context = Engine.CreateContext();
	if (!TestNotNull(TEXT("Execution.Nested should create a context"), Context))
	{
		return false;
	}

	const int PrepareResult = Context->Prepare(Function);
	if (!TestEqual(TEXT("Execution.Nested should prepare the entry point"), PrepareResult, static_cast<int32>(asSUCCESS)))
	{
		Context->Release();
		return false;
	}

	Context->SetArgDWord(0, 20);
	const int ExecuteResult = Context->Execute();
	if (!TestEqual(TEXT("Execution.Nested should execute the entry point"), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED)))
	{
		Context->Release();
		return false;
	}

	TestEqual(TEXT("Execution.Nested should evaluate nested calls in order"), static_cast<int32>(Context->GetReturnDWord()), 41);
	Context->Release();
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptExecutionDiscardTest,
	"Angelscript.TestModule.Functional.Execute.Discard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptExecutionDiscardTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASExecutionDiscard",
		TEXT("void Test() {}"));
	if (Module == nullptr)
	{
		return false;
	}

	TestTrue(TEXT("Execution.Discard should track the compiled module before discard"), Engine.GetModuleByModuleName(TEXT("ASExecutionDiscard")).IsValid());

	if (!TestTrue(TEXT("Execution.Discard should discard the tracked module"), Engine.DiscardModule(TEXT("ASExecutionDiscard"))))
	{
		return false;
	}

	TestTrue(TEXT("Execution.Discard should remove the tracked module after discard"), !Engine.GetModuleByModuleName(TEXT("ASExecutionDiscard")).IsValid());
	TestFalse(TEXT("Execution.Discard should fail when discarding the same module twice"), Engine.DiscardModule(TEXT("ASExecutionDiscard")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptExecutionScriptTest,
	"Angelscript.TestModule.Functional.Execute.Script",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptExecutionScriptTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASExecutionScript",
		TEXT("int Calculate(int Start, int End) { int Result = 0; for (int Index = Start; Index <= End; ++Index) { Result += Index; } return Result; }"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Calculate(int, int)"));
	if (Function == nullptr)
	{
		return false;
	}

	asIScriptContext* Context = Engine.CreateContext();
	if (!TestNotNull(TEXT("Execution.Script should create a context"), Context))
	{
		return false;
	}

	const int PrepareResult = Context->Prepare(Function);
	if (!TestEqual(TEXT("Execution.Script should prepare the entry point"), PrepareResult, static_cast<int32>(asSUCCESS)))
	{
		Context->Release();
		return false;
	}

	Context->SetArgDWord(0, 1);
	Context->SetArgDWord(1, 10);
	const int ExecuteResult = Context->Execute();
	if (!TestEqual(TEXT("Execution.Script should execute the entry point"), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED)))
	{
		Context->Release();
		return false;
	}

	TestEqual(TEXT("Execution.Script should sum the range inclusively"), static_cast<int32>(Context->GetReturnDWord()), 55);
	Context->Release();
	}

	return true;
}

#endif
