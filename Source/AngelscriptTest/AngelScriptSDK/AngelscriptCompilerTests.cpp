#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestEngineHelper.h"
#include "CQTest.h"
#include "Misc/Paths.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptfunction.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_AngelScriptSDK_AngelscriptCompilerTests_Private
{
	const FAngelscriptCompileTraceDiagnosticSummary* FindErrorDiagnosticContaining(
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

	bool FindLastBytecodeOpcode(const asDWORD* Bytecode, asUINT BytecodeLength, asBYTE& OutOpcode)
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
}


TEST_CLASS_WITH_FLAGS(FAngelscriptCompilerTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(BytecodeGeneration)
	{
		using namespace AngelscriptTest_AngelScriptSDK_AngelscriptCompilerTests_Private;
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
		TestRunner->TestNotNull(TEXT("Compiled function should expose a bytecode buffer"), Bytecode);
		TestRunner->TestTrue(TEXT("Compiled function should emit at least one bytecode instruction"), BytecodeLength > 0);
		}
	}

	TEST_METHOD(BytecodeExecutionAndRetBoundary)
	{
		using namespace AngelscriptTest_AngelScriptSDK_AngelscriptCompilerTests_Private;
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
		if (!TestRunner->TestNotNull(TEXT("Compiler bytecode boundary test should create a script context"), Context))
		{
			return;
		}

		const int PrepareResult = Context->Prepare(Function);
		const int SetArgResult = PrepareResult == asSUCCESS ? Context->SetArgDWord(0, 1) : PrepareResult;
		const int ExecuteResult = SetArgResult == asSUCCESS ? Context->Execute() : SetArgResult;
		const int32 Result = ExecuteResult == asEXECUTION_FINISHED ? static_cast<int32>(Context->GetReturnDWord()) : 0;
		Context->Release();

		if (!TestRunner->TestEqual(TEXT("Compiler bytecode boundary test should prepare successfully"), PrepareResult, asSUCCESS))
		{
			return;
		}
		if (!TestRunner->TestEqual(TEXT("Compiler bytecode boundary test should accept the integer argument"), SetArgResult, asSUCCESS))
		{
			return;
		}
		if (!TestRunner->TestEqual(TEXT("Compiler bytecode boundary test should execute successfully"), ExecuteResult, asEXECUTION_FINISHED))
		{
			return;
		}
		if (!TestRunner->TestEqual(TEXT("Compiler bytecode boundary test should execute the compiled arithmetic function"), Result, 3))
		{
			return;
		}

		asUINT BytecodeLength = 0;
		asDWORD* Bytecode = Function->GetByteCode(&BytecodeLength);
		if (!TestRunner->TestNotNull(TEXT("Compiler bytecode boundary test should expose a bytecode buffer"), Bytecode))
		{
			return;
		}
		if (!TestRunner->TestTrue(TEXT("Compiler bytecode boundary test should emit more than one dword"), BytecodeLength > 1))
		{
			return;
		}

		const asBYTE FirstOpcode = *reinterpret_cast<const asBYTE*>(&Bytecode[0]);
		if (!TestRunner->TestNotEqual(TEXT("Compiler bytecode boundary test should not begin with RET"), static_cast<int32>(FirstOpcode), static_cast<int32>(asBC_RET)))
		{
			return;
		}

		asBYTE LastOpcode = 0;
		if (!TestRunner->TestTrue(TEXT("Compiler bytecode boundary test should walk the bytecode to a valid end boundary"), FindLastBytecodeOpcode(Bytecode, BytecodeLength, LastOpcode)))
		{
			return;
		}
		if (!TestRunner->TestEqual(TEXT("Compiler bytecode boundary test should end with RET"), static_cast<int32>(LastOpcode), static_cast<int32>(asBC_RET)))
		{
			return;
		}
		}
	}

	TEST_METHOD(VariableScopes)
	{
		using namespace AngelscriptTest_AngelScriptSDK_AngelscriptCompilerTests_Private;
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

		TestRunner->TestTrue(TEXT("Compiled function should report local variables for scoped declarations"), Function->GetVarCount() >= 2);

		const char* FirstVarName = nullptr;
		Function->GetVar(0, &FirstVarName, nullptr);
		TestRunner->TestNotNull(TEXT("Compiler should record the first local variable name"), FirstVarName);
		}
	}

	TEST_METHOD(OutOfScopeUseRejected)
	{
		using namespace AngelscriptTest_AngelScriptSDK_AngelscriptCompilerTests_Private;
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

		TestRunner->TestFalse(
			TEXT("Compiler.VariableScopes.OutOfScopeUseRejected should reject out-of-scope locals"),
			bCompiled);
		TestRunner->TestFalse(
			TEXT("Compiler.VariableScopes.OutOfScopeUseRejected should report bCompileSucceeded=false"),
			Summary.bCompileSucceeded);
		TestRunner->TestEqual(
			TEXT("Compiler.VariableScopes.OutOfScopeUseRejected should surface ECompileResult::Error"),
			Summary.CompileResult,
			ECompileResult::Error);
		TestRunner->TestTrue(
			TEXT("Compiler.VariableScopes.OutOfScopeUseRejected should capture at least one diagnostic"),
			Summary.Diagnostics.Num() > 0);
		TestRunner->TestNotNull(
			TEXT("Compiler.VariableScopes.OutOfScopeUseRejected should surface a scope diagnostic"),
			Diagnostic);
		if (Diagnostic != nullptr)
		{
			TestRunner->TestTrue(
				TEXT("Compiler.VariableScopes.OutOfScopeUseRejected should keep the missing variable name in the diagnostic"),
				Diagnostic->Message.Contains(TEXT("Inner")));
			TestRunner->TestTrue(
				TEXT("Compiler.VariableScopes.OutOfScopeUseRejected should report a non-zero row"),
				Diagnostic->Row > 0);
			TestRunner->TestTrue(
				TEXT("Compiler.VariableScopes.OutOfScopeUseRejected should report a non-zero column"),
				Diagnostic->Column > 0);
		}
		TestRunner->TestTrue(
			TEXT("Compiler.VariableScopes.OutOfScopeUseRejected should not leave a compiled module behind"),
			!Engine.GetModuleByModuleName(ModuleName.ToString()).IsValid());

		}
	}

	TEST_METHOD(FunctionCalls)
	{
		using namespace AngelscriptTest_AngelScriptSDK_AngelscriptCompilerTests_Private;
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

		TestRunner->TestEqual(TEXT("Compiler should generate callable bytecode for function invocations"), Result, 12);
		}
	}

	TEST_METHOD(TypeConversions)
	{
		using namespace AngelscriptTest_AngelScriptSDK_AngelscriptCompilerTests_Private;
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
		if (!TestRunner->TestNotNull(TEXT("Compiler conversion test should create a script context"), Context))
		{
			return;
		}

		const int PrepareResult = Context->Prepare(Function);
		const int ExecuteResult = PrepareResult == asSUCCESS ? Context->Execute() : PrepareResult;
		const float Result = Context->GetReturnFloat();
		Context->Release();

		TestRunner->TestEqual(TEXT("Compiler conversion test should prepare successfully"), PrepareResult, asSUCCESS);
		TestRunner->TestEqual(TEXT("Compiler conversion test should execute successfully"), ExecuteResult, asEXECUTION_FINISHED);
		TestRunner->TestTrue(TEXT("Compiler should emit a numeric conversion that preserves the value"), FMath::IsNearlyEqual(Result, 7.0f));
		}
	}

	TEST_METHOD(NegativeAndFloat64Matrix)
	{
		using namespace AngelscriptTest_AngelScriptSDK_AngelscriptCompilerTests_Private;
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

		if (!TestRunner->TestEqual(
			TEXT("Compiler type conversion matrix should truncate both float32 negatives and float64 positives toward zero"),
			Result,
			709))
		{
			return;
		}

		asUINT BytecodeLength = 0;
		asDWORD* Bytecode = Function->GetByteCode(&BytecodeLength);
		if (!TestRunner->TestNotNull(TEXT("Compiler type conversion matrix should expose generated bytecode"), Bytecode))
		{
			return;
		}
		if (!TestRunner->TestTrue(TEXT("Compiler type conversion matrix should emit at least one bytecode instruction"), BytecodeLength > 0))
		{
			return;
		}

		}
	}
};

#endif
