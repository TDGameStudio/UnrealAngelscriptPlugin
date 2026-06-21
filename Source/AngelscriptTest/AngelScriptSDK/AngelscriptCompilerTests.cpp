#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestEngineHelper.h"
#include "CQTest.h"
#include "Misc/Paths.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptfunction.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCompilerTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static const FAngelscriptCompileTraceDiagnosticSummary* FindErrorDiagnosticContaining(
		const TArray<FAngelscriptCompileTraceDiagnosticSummary>& Diagnostics,
		const FString& Needle)
	{
		for (const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic : Diagnostics)
		{
			if (Diagnostic.bIsError && Diagnostic.Message.Contains(Needle))
			{
				return &Diagnostic;
			}
		}

		return nullptr;
	}

	static bool FindLastBytecodeOpcode(const asDWORD* Bytecode, asUINT BytecodeLength, asBYTE& OutOpcode)
	{
		if (Bytecode == nullptr || BytecodeLength == 0)
		{
			return false;
		}

		asUINT Cursor = 0;
		asBYTE LastOpcode = 0;
		while (Cursor < BytecodeLength)
		{
			const asBYTE Opcode = *reinterpret_cast<const asBYTE*>(&Bytecode[Cursor]);
			const int InstructionSize = asBCTypeSize[asBCInfo[Opcode].type];
			if (InstructionSize <= 0 || Cursor + static_cast<asUINT>(InstructionSize) > BytecodeLength)
			{
				return false;
			}

			LastOpcode = Opcode;
			Cursor += static_cast<asUINT>(InstructionSize);
		}

		OutOpcode = LastOpcode;
		return true;
	}

public:
	TEST_METHOD(BytecodeGeneration)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		asIScriptModule* Module = BuildModule(
			*TestRunner,
			Engine,
			"CompilerBytecodeGeneration",
			TEXT("int Entry() { int A = 1; int B = 2; return A + B; }"));
		if (Module == nullptr)
		{
			return;
		}

		asIScriptFunction* Function = GetFunctionByDecl(*TestRunner, *Module, TEXT("int Entry()"));
		if (Function == nullptr)
		{
			return;
		}

		asUINT BytecodeLength = 0;
		asDWORD* Bytecode = Function->GetByteCode(&BytecodeLength);
		ASSERT_THAT(IsNotNull(Bytecode, TEXT("Compiled function should expose a bytecode buffer")));
		ASSERT_THAT(IsTrue(BytecodeLength > 0, TEXT("Compiled function should emit at least one bytecode instruction")));
		}
	}

	TEST_METHOD(BytecodeExecutionAndRetBoundary)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		asIScriptModule* Module = BuildModule(
			*TestRunner,
			Engine,
			"CompilerBytecodeExecutionAndRetBoundary",
			TEXT("int Entry(int A) { int B = 2; return A + B; }"));
		if (Module == nullptr)
		{
			return;
		}

		asIScriptFunction* Function = GetFunctionByDecl(*TestRunner, *Module, TEXT("int Entry(int)"));
		if (Function == nullptr)
		{
			return;
		}

		asIScriptContext* Context = Engine.CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("Compiler bytecode boundary test should create a script context")));

		const int PrepareResult = Context->Prepare(Function);
		const int SetArgResult = PrepareResult == asSUCCESS ? Context->SetArgDWord(0, 1) : PrepareResult;
		const int ExecuteResult = SetArgResult == asSUCCESS ? Context->Execute() : SetArgResult;
		const int32 Result = ExecuteResult == asEXECUTION_FINISHED ? static_cast<int32>(Context->GetReturnDWord()) : 0;
		Context->Release();

		ASSERT_THAT(AreEqual(asSUCCESS, PrepareResult, TEXT("Compiler bytecode boundary test should prepare successfully")));
		ASSERT_THAT(AreEqual(asSUCCESS, SetArgResult, TEXT("Compiler bytecode boundary test should accept the integer argument")));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, ExecuteResult, TEXT("Compiler bytecode boundary test should execute successfully")));
		ASSERT_THAT(AreEqual(3, Result, TEXT("Compiler bytecode boundary test should execute the compiled arithmetic function")));

		asUINT BytecodeLength = 0;
		asDWORD* Bytecode = Function->GetByteCode(&BytecodeLength);
		ASSERT_THAT(IsNotNull(Bytecode, TEXT("Compiler bytecode boundary test should expose a bytecode buffer")));
		ASSERT_THAT(IsTrue(BytecodeLength > 1, TEXT("Compiler bytecode boundary test should emit more than one dword")));

		const asBYTE FirstOpcode = *reinterpret_cast<const asBYTE*>(&Bytecode[0]);
		ASSERT_THAT(AreNotEqual(static_cast<int32>(asBC_RET), static_cast<int32>(FirstOpcode), TEXT("Compiler bytecode boundary test should not begin with RET")));

		asBYTE LastOpcode = 0;
		ASSERT_THAT(IsTrue(FindLastBytecodeOpcode(Bytecode, BytecodeLength, LastOpcode), TEXT("Compiler bytecode boundary test should walk the bytecode to a valid end boundary")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asBC_RET), static_cast<int32>(LastOpcode), TEXT("Compiler bytecode boundary test should end with RET")));
		}
	}

	TEST_METHOD(VariableScopes)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		asIScriptModule* Module = BuildModule(
			*TestRunner,
			Engine,
			"CompilerVariableScopes",
			TEXT("int Entry() { int Outer = 1; { int Inner = 2; Outer += Inner; } return Outer; }"));
		if (Module == nullptr)
		{
			return;
		}

		asIScriptFunction* Function = GetFunctionByDecl(*TestRunner, *Module, TEXT("int Entry()"));
		if (Function == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(Function->GetVarCount() >= 2, TEXT("Compiled function should report local variables for scoped declarations")));

		const char* FirstVarName = nullptr;
		Function->GetVar(0, &FirstVarName, nullptr);
		ASSERT_THAT(IsNotNull(FirstVarName, TEXT("Compiler should record the first local variable name")));
		}
	}

	TEST_METHOD(OutOfScopeUseRejected)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		const FName ModuleName(TEXT("CompilerVariableScopesOutOfScope"));
		const FString ScriptFilename = FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("NegativeCompileIsolation"),
			TEXT("CompilerVariableScopesOutOfScope.as"));
		const FString ScriptSource = TEXT(R"AS(
int Entry()
{
	{
		int Inner = 2;
	}
	return Inner;
}
)AS");

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			ModuleName,
			ScriptFilename,
			ScriptSource,
			false,
			Summary,
			true);
		const FAngelscriptCompileTraceDiagnosticSummary* Diagnostic =
			FindErrorDiagnosticContaining(Summary.Diagnostics, TEXT("is not declared"));

		ASSERT_THAT(IsFalse(bCompiled, TEXT("Compiler.VariableScopes.OutOfScopeUseRejected should reject out-of-scope locals")));
		ASSERT_THAT(IsFalse(Summary.bCompileSucceeded, TEXT("Compiler.VariableScopes.OutOfScopeUseRejected should report bCompileSucceeded=false")));
		ASSERT_THAT(AreEqual(ECompileResult::Error, Summary.CompileResult, TEXT("Compiler.VariableScopes.OutOfScopeUseRejected should surface ECompileResult::Error")));
		ASSERT_THAT(IsTrue(Summary.Diagnostics.Num() > 0, TEXT("Compiler.VariableScopes.OutOfScopeUseRejected should capture at least one diagnostic")));
		ASSERT_THAT(IsNotNull(Diagnostic, TEXT("Compiler.VariableScopes.OutOfScopeUseRejected should surface a scope diagnostic")));
		if (Diagnostic != nullptr)
		{
			ASSERT_THAT(IsTrue(Diagnostic->Message.Contains(TEXT("Inner")), TEXT("Compiler.VariableScopes.OutOfScopeUseRejected should keep the missing variable name in the diagnostic")));
			ASSERT_THAT(IsTrue(Diagnostic->Row > 0, TEXT("Compiler.VariableScopes.OutOfScopeUseRejected should report a non-zero row")));
			ASSERT_THAT(IsTrue(Diagnostic->Column > 0, TEXT("Compiler.VariableScopes.OutOfScopeUseRejected should report a non-zero column")));
		}
		ASSERT_THAT(IsTrue(!Engine.GetModuleByModuleName(ModuleName.ToString()).IsValid(), TEXT("Compiler.VariableScopes.OutOfScopeUseRejected should not leave a compiled module behind")));

		}
	}

	TEST_METHOD(FunctionCalls)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		asIScriptModule* Module = BuildModule(
			*TestRunner,
			Engine,
			"CompilerFunctionCalls",
			TEXT("int Add(int A, int B) { return A + B; } int Entry() { return Add(7, 5); }"));
		if (Module == nullptr)
		{
			return;
		}

		asIScriptFunction* Function = GetFunctionByDecl(*TestRunner, *Module, TEXT("int Entry()"));
		if (Function == nullptr)
		{
			return;
		}

		int32 Result = 0;
		if (!ExecuteIntFunction(*TestRunner, Engine, *Function, Result))
		{
			return;
		}

		ASSERT_THAT(AreEqual(12, Result, TEXT("Compiler should generate callable bytecode for function invocations")));
		}
	}

	TEST_METHOD(TypeConversions)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		asIScriptModule* Module = BuildModule(
			*TestRunner,
			Engine,
			"CompilerTypeConversions",
			TEXT("float32 Entry() { int Value = 7; return float32(Value); }"));
		if (Module == nullptr)
		{
			return;
		}

		asIScriptFunction* Function = GetFunctionByDecl(*TestRunner, *Module, TEXT("float32 Entry()"));
		if (Function == nullptr)
		{
			return;
		}

		asIScriptContext* Context = Engine.CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("Compiler conversion test should create a script context")));

		const int PrepareResult = Context->Prepare(Function);
		const int ExecuteResult = PrepareResult == asSUCCESS ? Context->Execute() : PrepareResult;
		const float Result = Context->GetReturnFloat();
		Context->Release();

		ASSERT_THAT(AreEqual(asSUCCESS, PrepareResult, TEXT("Compiler conversion test should prepare successfully")));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, ExecuteResult, TEXT("Compiler conversion test should execute successfully")));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Result, 7.0f), TEXT("Compiler should emit a numeric conversion that preserves the value")));
		}
	}

	TEST_METHOD(NegativeAndFloat64Matrix)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		asIScriptModule* Module = BuildModule(
			*TestRunner,
			Engine,
			"CompilerTypeConversionsNegativeAndFloat64Matrix",
			TEXT("int Entry() { float32 A = -3.75f; float64 B = 9.25; int FromA = int(A); int FromB = int(B); return (FromA + 10) * 100 + FromB; }"));
		if (Module == nullptr)
		{
			return;
		}

		asIScriptFunction* Function = GetFunctionByDecl(*TestRunner, *Module, TEXT("int Entry()"));
		if (Function == nullptr)
		{
			return;
		}

		int32 Result = 0;
		if (!ExecuteIntFunction(*TestRunner, Engine, *Function, Result))
		{
			return;
		}

		ASSERT_THAT(AreEqual(709, Result, TEXT("Compiler type conversion matrix should truncate both float32 negatives and float64 positives toward zero")));

		asUINT BytecodeLength = 0;
		asDWORD* Bytecode = Function->GetByteCode(&BytecodeLength);
		ASSERT_THAT(IsNotNull(Bytecode, TEXT("Compiler type conversion matrix should expose generated bytecode")));
		ASSERT_THAT(IsTrue(BytecodeLength > 0, TEXT("Compiler type conversion matrix should emit at least one bytecode instruction")));

		}
	}
};

#endif
