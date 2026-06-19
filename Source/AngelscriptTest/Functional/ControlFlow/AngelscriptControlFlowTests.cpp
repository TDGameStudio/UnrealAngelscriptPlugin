#include "CQTest.h"
#include "AngelscriptBindingsAssertions.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_Angelscript_AngelscriptControlFlowTests_Private
{
	bool ContainsWarningDiagnostic(const FAngelscriptEngine& Engine, const FString& Needle)
	{
		for (const TPair<FString, FAngelscriptEngine::FDiagnostics>& Pair : Engine.Diagnostics)
		{
			for (const FAngelscriptEngine::FDiagnostic& Diagnostic : Pair.Value.Diagnostics)
			{
				if (!Diagnostic.bIsError && Diagnostic.Message.Contains(Needle))
				{
					return true;
				}
			}
		}

		return false;
	}

	const FAngelscriptCompileTraceDiagnosticSummary* FindErrorDiagnostic(
		const FAngelscriptCompileTraceSummary& Summary,
		const FString& Needle)
	{
		for (const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic : Summary.Diagnostics)
		{
			if (Diagnostic.bIsError && Diagnostic.Message.Contains(Needle))
			{
				return &Diagnostic;
			}
		}

		return nullptr;
	}

	const FAngelscriptCompileTraceDiagnosticSummary* FindWarningDiagnostic(
		const FAngelscriptCompileTraceSummary& Summary,
		const FString& MessageFragment,
		const FString& DetailFragment)
	{
		for (const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic : Summary.Diagnostics)
		{
			if (!Diagnostic.bIsError
				&& Diagnostic.Message.Contains(MessageFragment)
				&& Diagnostic.Message.Contains(DetailFragment))
			{
				return &Diagnostic;
			}
		}

		return nullptr;
	}
}


TEST_CLASS_WITH_FLAGS(
	FAngelscriptControlFlowTests,
	"Angelscript.TestModule.Functional.ControlFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE_FULL();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	TEST_METHOD(ForLoop)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASControlFlowForLoop"),
			TEXT("int Run() { int Sum = 0; for (int Index = 0; Index < 5; ++Index) { Sum += Index; } return Sum; }"));
		ASSERT_THAT(IsTrue(Module.IsValid()));

		ExpectGlobalInt(*TestRunner, Engine, Module.GetModule(), TEXT("int Run()"), TEXT("For-loop control flow should sum the expected values"), 10);
	}

	TEST_METHOD(ForLoop_DecrementAndZeroIteration)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASControlFlowForLoopDecrementZeroIteration"),
			TEXT("int Desc() { int Encoded = 0; for (int Index = 3; Index >= 0; --Index) { Encoded = Encoded * 10 + Index; } return Encoded; } int ZeroLoopHits() { int Hits = 0; for (int Index = 5; Index < 5; ++Index) { ++Hits; } return Hits; } int Run() { return Desc() * 10 + ZeroLoopHits(); }"));
		ASSERT_THAT(IsTrue(Module.IsValid()));

		ExpectGlobalInt(*TestRunner, Engine, Module.GetModule(), TEXT("int Run()"), TEXT("For-loop control flow should preserve decrement updates and zero-iteration short-circuit"), 32100);
	}

	TEST_METHOD(ForeachBreakContinueNested)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASControlFlowForeachBreakContinue"),
			TEXT("int Run() { TArray<int> Values; Values.Add(1); Values.Add(2); Values.Add(3); Values.Add(4); Values.Add(5); int Count = 0; int Sum = 0; foreach (int Value : Values) { if (Value == 2) continue; ++Count; Sum += Value; if (Value == 4) break; } return Count * 10 + Sum; }"));
		ASSERT_THAT(IsTrue(Module.IsValid()));

		ExpectGlobalInt(*TestRunner, Engine, Module.GetModule(), TEXT("int Run()"), TEXT("Foreach control flow should preserve continue skip, break exit, and accumulated state"), 38);
	}

	TEST_METHOD(WhileBreakContinueNested)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASControlFlowWhileBreakContinue"),
			TEXT("int Run() { int Index = 0; int Hits = 0; int Sum = 0; while (Index < 6) { ++Index; if ((Index % 2) == 0) { continue; } else { Sum += Index; ++Hits; } if (Index >= 5) { break; } } return Hits * 100 + Sum; }"));
		ASSERT_THAT(IsTrue(Module.IsValid()));

		ExpectGlobalInt(*TestRunner, Engine, Module.GetModule(), TEXT("int Run()"), TEXT("While control flow should preserve continue skip, break exit, and nested if/else accumulation"), 309);
	}

	TEST_METHOD(SwitchAndConditional)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASControlFlowSwitch"),
			TEXT("int Pick(int Value) { switch (Value) { case 0: return 2; case 1: return 4; default: return 6; } } int Run() { int Base = Pick(1); return Base > 3 ? Base + 3 : Base - 1; }"));
		ASSERT_THAT(IsTrue(Module.IsValid()));

		ExpectGlobalInt(*TestRunner, Engine, Module.GetModule(), TEXT("int Run()"), TEXT("Switch/conditional flow should return the expected branch result"), 7);
	}

	TEST_METHOD(SwitchDefaultAndConditionalMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASControlFlowSwitchMatrix"),
			TEXT("int Pick(int Value) { switch (Value) { case 0: return 2; case 1: return 4; default: return 6; } } int Conditional(int Base) { return Base > 3 ? Base + 3 : Base - 1; } int Run() { return Pick(0) * 10000 + Pick(1) * 1000 + Pick(9) * 100 + Conditional(4) * 10 + Conditional(2); }"));
		ASSERT_THAT(IsTrue(Module.IsValid()));

		ExpectGlobalInt(*TestRunner, Engine, Module.GetModule(), TEXT("int Run()"), TEXT("Switch default and conditional matrix should return the expected branch summary"), 24671);
	}

	TEST_METHOD(Condition)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASControlFlowCondition"),
			TEXT("int Evaluate(int Value) { return (Value > 0) ? ((Value > 10) ? 2 : 1) : 0; } int Run() { return Evaluate(15) * 100 + Evaluate(5) * 10 + Evaluate(-3); }"));
		ASSERT_THAT(IsTrue(Module.IsValid()));

		ExpectGlobalInt(*TestRunner, Engine, Module.GetModule(), TEXT("int Run()"), TEXT("Condition control flow should preserve nested ternary branches"), 210);
	}

	TEST_METHOD(IfElseStatementMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASControlFlowIfElseStatementMatrix"),
			TEXT("int Evaluate(int Value) { int TrueHits = 0; int FalseHits = 0; if (Value > 0) { int Local = Value + 1; TrueHits = Local; } else { int Local = -Value + 2; FalseHits = Local; } return TrueHits * 100 + FalseHits; } int Run() { return Evaluate(3) * 10 + Evaluate(-2); }"));
		ASSERT_THAT(IsTrue(Module.IsValid()));

		ExpectGlobalInt(*TestRunner, Engine, Module.GetModule(), TEXT("int Run()"), TEXT("Statement-level if/else control flow should preserve both branch-local writes"), 4004);
	}

	TEST_METHOD(InvalidBreakContinueDiagnostics)
	{
		using namespace AngelscriptTest_Angelscript_AngelscriptControlFlowTests_Private;
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};

		auto VerifyInvalidControlFlow = [this, &Engine](
			const FName ModuleName,
			const FString& ScriptFilename,
			const FString& ScriptSource,
			const FString& ControlFlowLabel,
			const FString& ExpectedMessage)
		{
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
			const FAngelscriptCompileTraceDiagnosticSummary* Diagnostic = FindErrorDiagnostic(Summary, ExpectedMessage);

			ASSERT_THAT(IsFalse(bCompiled, *FString::Printf(TEXT("Invalid control-flow test should reject %s outside a loop"), *ControlFlowLabel)));
			ASSERT_THAT(IsFalse(Summary.bCompileSucceeded, *FString::Printf(TEXT("Invalid control-flow test should report bCompileSucceeded=false for %s"), *ControlFlowLabel)));
			ASSERT_THAT(AreEqual(ECompileResult::Error, Summary.CompileResult, *FString::Printf(TEXT("Invalid control-flow test should report ECompileResult::Error for %s"), *ControlFlowLabel)));
			ASSERT_THAT(IsTrue(Summary.Diagnostics.Num() > 0, *FString::Printf(TEXT("Invalid control-flow test should collect diagnostics for %s"), *ControlFlowLabel)));
			ASSERT_THAT(IsNotNull(Diagnostic, *FString::Printf(TEXT("Invalid control-flow test should surface an error diagnostic containing %s"), *ExpectedMessage)));
			ASSERT_THAT(IsTrue(Diagnostic->Row > 0, *FString::Printf(TEXT("Invalid control-flow test should report a non-zero diagnostic row for %s"), *ControlFlowLabel)));
			ASSERT_THAT(IsTrue(Diagnostic->Column > 0, *FString::Printf(TEXT("Invalid control-flow test should report a non-zero diagnostic column for %s"), *ControlFlowLabel)));
		};

		VerifyInvalidControlFlow(
			TEXT("ASControlFlowInvalidBreak"),
			FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NegativeCompileIsolation"), TEXT("ASControlFlowInvalidBreak.as")),
			TEXT("void Run() { break; }"),
			TEXT("break"),
			TEXT("Invalid 'break'"));

		VerifyInvalidControlFlow(
			TEXT("ASControlFlowInvalidContinue"),
			FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NegativeCompileIsolation"), TEXT("ASControlFlowInvalidContinue.as")),
			TEXT("void Run() { continue; }"),
			TEXT("continue"),
			TEXT("Invalid 'continue'"));

	}

	TEST_METHOD(NeverVisited)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASControlFlowNeverVisited"),
			TEXT("void Run(bool bCondition) { if (bCondition) { return; } int Value = 42; }"));
		ASSERT_THAT(IsTrue(Module.IsValid()));

		ASSERT_THAT(IsNotNull(GetFunctionByDecl(*TestRunner, Module.GetModule(), TEXT("void Run(bool)"))));

		ASSERT_THAT(IsTrue(true, TEXT("NeverVisited should compile code with a potentially unreachable block")));
	}

	TEST_METHOD(NotInitialized)
	{
		using namespace AngelscriptTest_Angelscript_AngelscriptControlFlowTests_Private;
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASControlFlowNotInitialized"),
			TEXT("int Run() { int Value; return Value; }"));
		ASSERT_THAT(IsTrue(Module.IsValid()));

		ASSERT_THAT(IsNotNull(GetFunctionByDecl(*TestRunner, Module.GetModule(), TEXT("int Run()"))));

		const bool bFoundUninitializedWarning = ContainsWarningDiagnostic(Engine, TEXT("may not be initialized"));
		ASSERT_THAT(IsTrue(bFoundUninitializedWarning, TEXT("NotInitialized should preserve the compiler warning for reading an uninitialized variable")));
	}

	TEST_METHOD(NotInitialized_BranchDefiniteAssignmentMatrix)
	{
		using namespace AngelscriptTest_Angelscript_AngelscriptControlFlowTests_Private;
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

	const FName PartialModuleName(TEXT("ASControlFlowNotInitializedBranchPartial"));
	const FName FullModuleName(TEXT("ASControlFlowNotInitializedBranchFull"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*PartialModuleName.ToString());
		Engine.DiscardModule(*FullModuleName.ToString());
	};

	const FString PartialFilename = FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("NegativeCompileIsolation"),
		TEXT("ASControlFlowNotInitializedBranchPartial.as"));
	const FString FullFilename = FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("NegativeCompileIsolation"),
		TEXT("ASControlFlowNotInitializedBranchFull.as"));

	FAngelscriptCompileTraceSummary PartialSummary;
	const bool bPartialCompiled = CompileModuleWithSummary(
		&Engine,
		ECompileType::SoftReloadOnly,
		PartialModuleName,
		PartialFilename,
		TEXT(R"AS(
int RunPartial(bool bFlag)
{
	int Value;
	if (bFlag)
	{
		Value = 7;
	}
	return Value;
}
)AS"),
		false,
		PartialSummary,
		true);
	const FAngelscriptCompileTraceDiagnosticSummary* PartialWarning = FindWarningDiagnostic(
		PartialSummary,
		TEXT("may not be initialized"),
		TEXT("Value"));

	ASSERT_THAT(IsTrue(bPartialCompiled, TEXT("Branch definite-assignment partial test case should still compile")));
	ASSERT_THAT(IsTrue(PartialSummary.bCompileSucceeded, TEXT("Branch definite-assignment partial test case should report bCompileSucceeded=true")));
	ASSERT_THAT(IsTrue(
		PartialSummary.CompileResult == ECompileResult::FullyHandled
			|| PartialSummary.CompileResult == ECompileResult::PartiallyHandled,
		TEXT("Branch definite-assignment partial test case should stay on a handled compile path")));
	ASSERT_THAT(IsNotNull(PartialWarning, TEXT("Branch definite-assignment partial test case should capture at least one warning diagnostic for Value")));
	ASSERT_THAT(IsTrue(PartialWarning->Row > 0, TEXT("Branch definite-assignment partial warning should report a non-zero row")));
	ASSERT_THAT(IsTrue(PartialWarning->Column > 0, TEXT("Branch definite-assignment partial warning should report a non-zero column")));

	FAngelscriptCompileTraceSummary FullSummary;
	const bool bFullCompiled = CompileModuleWithSummary(
		&Engine,
		ECompileType::SoftReloadOnly,
		FullModuleName,
		FullFilename,
		TEXT(R"AS(
int Compute(bool bFlag)
{
	int Value;
	if (bFlag)
	{
		Value = 7;
	}
	else
	{
		Value = 9;
	}
	return Value;
}

int RunSafeTrue()
{
	return Compute(true);
}

int RunSafeFalse()
{
	return Compute(false);
}
)AS"),
		false,
		FullSummary,
		true);
	const FAngelscriptCompileTraceDiagnosticSummary* FullWarning = FindWarningDiagnostic(
		FullSummary,
		TEXT("may not be initialized"),
		TEXT("Value"));

	ASSERT_THAT(IsTrue(bFullCompiled, TEXT("Branch definite-assignment full test case should compile")));
	ASSERT_THAT(IsTrue(FullSummary.bCompileSucceeded, TEXT("Branch definite-assignment full test case should report bCompileSucceeded=true")));
	ASSERT_THAT(IsTrue(
		FullSummary.CompileResult == ECompileResult::FullyHandled
			|| FullSummary.CompileResult == ECompileResult::PartiallyHandled,
		TEXT("Branch definite-assignment full test case should stay on a handled compile path")));
	ASSERT_THAT(IsNull(FullWarning, TEXT("Branch definite-assignment full test case should not emit an uninitialized warning for Value")));

	int32 SafeTrueResult = 0;
	int32 SafeFalseResult = 0;
	ASSERT_THAT(IsTrue(ExecuteIntFunction(&Engine, FullModuleName, TEXT("int RunSafeTrue()"), SafeTrueResult)));
	ASSERT_THAT(IsTrue(ExecuteIntFunction(&Engine, FullModuleName, TEXT("int RunSafeFalse()"), SafeFalseResult)));
	ASSERT_THAT(AreEqual(7, SafeTrueResult, TEXT("Branch definite-assignment full test case should execute the true wrapper result")));
	ASSERT_THAT(AreEqual(9, SafeFalseResult, TEXT("Branch definite-assignment full test case should execute the false wrapper result")));

	}
};

#endif
