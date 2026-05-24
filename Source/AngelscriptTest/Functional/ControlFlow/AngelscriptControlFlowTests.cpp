#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestLegacyHelpers.h"
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


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptControlFlowForLoopTest,
	"Angelscript.TestModule.Functional.ControlFlow.ForLoop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptControlFlowForLoopDecrementAndZeroIterationTest,
	"Angelscript.TestModule.Functional.ControlFlow.ForLoop.DecrementAndZeroIteration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptControlFlowForLoopTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptControlFlowTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine, "ASControlFlowForLoop",
		TEXT("int Run() { int Sum = 0; for (int Index = 0; Index < 5; ++Index) { Sum += Index; } return Sum; }"),
		TEXT("int Run()"), Result);

	TestEqual(TEXT("For-loop control flow should sum the expected values"), Result, 10);

	}
	return true;
}

bool FAngelscriptControlFlowForLoopDecrementAndZeroIterationTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptControlFlowTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine, "ASControlFlowForLoopDecrementZeroIteration",
		TEXT("int Desc() { int Encoded = 0; for (int Index = 3; Index >= 0; --Index) { Encoded = Encoded * 10 + Index; } return Encoded; } int ZeroLoopHits() { int Hits = 0; for (int Index = 5; Index < 5; ++Index) { ++Hits; } return Hits; } int Run() { return Desc() * 10 + ZeroLoopHits(); }"),
		TEXT("int Run()"), Result);

	TestEqual(TEXT("For-loop control flow should preserve decrement updates and zero-iteration short-circuit"), Result, 32100);

	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptControlFlowSwitchTest,
	"Angelscript.TestModule.Functional.ControlFlow.SwitchAndConditional",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptControlFlowSwitchMatrixTest,
	"Angelscript.TestModule.Functional.ControlFlow.SwitchDefaultAndConditionalMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptControlFlowForeachBreakContinueNestedTest,
	"Angelscript.TestModule.Functional.ControlFlow.ForeachBreakContinueNested",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptControlFlowWhileBreakContinueNestedTest,
	"Angelscript.TestModule.Functional.ControlFlow.WhileBreakContinueNested",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptControlFlowForeachBreakContinueNestedTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptControlFlowTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine, "ASControlFlowForeachBreakContinue",
		TEXT("int Run() { TArray<int> Values; Values.Add(1); Values.Add(2); Values.Add(3); Values.Add(4); Values.Add(5); int Count = 0; int Sum = 0; foreach (int Value : Values) { if (Value == 2) continue; ++Count; Sum += Value; if (Value == 4) break; } return Count * 10 + Sum; }"),
		TEXT("int Run()"), Result);

	TestEqual(TEXT("Foreach control flow should preserve continue skip, break exit, and accumulated state"), Result, 38);

	}
	return true;
}

bool FAngelscriptControlFlowWhileBreakContinueNestedTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptControlFlowTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine, "ASControlFlowWhileBreakContinue",
		TEXT("int Run() { int Index = 0; int Hits = 0; int Sum = 0; while (Index < 6) { ++Index; if ((Index % 2) == 0) { continue; } else { Sum += Index; ++Hits; } if (Index >= 5) { break; } } return Hits * 100 + Sum; }"),
		TEXT("int Run()"), Result);

	TestEqual(TEXT("While control flow should preserve continue skip, break exit, and nested if/else accumulation"), Result, 309);

	}
	return true;
}

bool FAngelscriptControlFlowSwitchTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptControlFlowTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine, "ASControlFlowSwitch",
		TEXT("int Pick(int Value) { switch (Value) { case 0: return 2; case 1: return 4; default: return 6; } } int Run() { int Base = Pick(1); return Base > 3 ? Base + 3 : Base - 1; }"),
		TEXT("int Run()"), Result);

	TestEqual(TEXT("Switch/conditional flow should return the expected branch result"), Result, 7);

	}
	return true;
}

bool FAngelscriptControlFlowSwitchMatrixTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptControlFlowTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine, "ASControlFlowSwitchMatrix",
		TEXT("int Pick(int Value) { switch (Value) { case 0: return 2; case 1: return 4; default: return 6; } } int Conditional(int Base) { return Base > 3 ? Base + 3 : Base - 1; } int Run() { return Pick(0) * 10000 + Pick(1) * 1000 + Pick(9) * 100 + Conditional(4) * 10 + Conditional(2); }"),
		TEXT("int Run()"), Result);

	TestEqual(TEXT("Switch default and conditional matrix should return the expected branch summary"), Result, 24671);

	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptControlFlowConditionTest,
	"Angelscript.TestModule.Functional.ControlFlow.Condition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptControlFlowIfElseStatementMatrixTest,
	"Angelscript.TestModule.Functional.ControlFlow.IfElseStatementMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptControlFlowInvalidBreakContinueDiagnosticsTest,
	"Angelscript.TestModule.Functional.ControlFlow.InvalidBreakContinueDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptControlFlowConditionTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptControlFlowTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine, "ASControlFlowCondition",
		TEXT("int Evaluate(int Value) { return (Value > 0) ? ((Value > 10) ? 2 : 1) : 0; } int Run() { return Evaluate(15) * 100 + Evaluate(5) * 10 + Evaluate(-3); }"),
		TEXT("int Run()"), Result);

	TestEqual(TEXT("Condition control flow should preserve nested ternary branches"), Result, 210);

	}
	return true;
}

bool FAngelscriptControlFlowIfElseStatementMatrixTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptControlFlowTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine, "ASControlFlowIfElseStatementMatrix",
		TEXT("int Evaluate(int Value) { int TrueHits = 0; int FalseHits = 0; if (Value > 0) { int Local = Value + 1; TrueHits = Local; } else { int Local = -Value + 2; FalseHits = Local; } return TrueHits * 100 + FalseHits; } int Run() { return Evaluate(3) * 10 + Evaluate(-2); }"),
		TEXT("int Run()"), Result);

	TestEqual(TEXT("Statement-level if/else control flow should preserve both branch-local writes"), Result, 4004);

	}
	return true;
}

bool FAngelscriptControlFlowInvalidBreakContinueDiagnosticsTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptControlFlowTests_Private;
	bool bPassed = true;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};

	auto VerifyInvalidControlFlow = [this, &Engine, &bPassed](
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

		bPassed &= TestFalse(
			*FString::Printf(TEXT("Invalid control-flow test should reject %s outside a loop"), *ControlFlowLabel),
			bCompiled);
		bPassed &= TestFalse(
			*FString::Printf(TEXT("Invalid control-flow test should report bCompileSucceeded=false for %s"), *ControlFlowLabel),
			Summary.bCompileSucceeded);
		bPassed &= TestEqual(
			*FString::Printf(TEXT("Invalid control-flow test should report ECompileResult::Error for %s"), *ControlFlowLabel),
			Summary.CompileResult,
			ECompileResult::Error);
		bPassed &= TestTrue(
			*FString::Printf(TEXT("Invalid control-flow test should collect diagnostics for %s"), *ControlFlowLabel),
			Summary.Diagnostics.Num() > 0);
		bPassed &= TestNotNull(
			*FString::Printf(TEXT("Invalid control-flow test should surface an error diagnostic containing %s"), *ExpectedMessage),
			Diagnostic);
		if (Diagnostic != nullptr)
		{
			bPassed &= TestTrue(
				*FString::Printf(TEXT("Invalid control-flow test should report a non-zero diagnostic row for %s"), *ControlFlowLabel),
				Diagnostic->Row > 0);
			bPassed &= TestTrue(
				*FString::Printf(TEXT("Invalid control-flow test should report a non-zero diagnostic column for %s"), *ControlFlowLabel),
				Diagnostic->Column > 0);
		}
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
	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptControlFlowNeverVisitedTest,
	"Angelscript.TestModule.Functional.ControlFlow.NeverVisited",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptControlFlowNeverVisitedTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptControlFlowTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};

	asIScriptModule* Module = nullptr;
	ASTEST_BUILD_MODULE(Engine, "ASControlFlowNeverVisited",
		TEXT("void Run(bool bCondition) { if (bCondition) { return; } int Value = 42; }"),
		Module);

	if (GetFunctionByDecl(*this, *Module, TEXT("void Run(bool)")) == nullptr)
	{
		return false;
	}

	TestTrue(TEXT("NeverVisited should compile code with a potentially unreachable block"), true);

	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptControlFlowNotInitializedTest,
	"Angelscript.TestModule.Functional.ControlFlow.NotInitialized",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptControlFlowNotInitializedBranchDefiniteAssignmentMatrixTest,
	"Angelscript.TestModule.Functional.ControlFlow.NotInitialized.BranchDefiniteAssignmentMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptControlFlowNotInitializedTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptControlFlowTests_Private;
	bool bFoundUninitializedWarning = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};

	asIScriptModule* Module = nullptr;
	ASTEST_BUILD_MODULE(Engine, "ASControlFlowNotInitialized",
		TEXT("int Run() { int Value; return Value; }"),
		Module);

	if (GetFunctionByDecl(*this, *Module, TEXT("int Run()")) == nullptr)
	{
		return false;
	}

	bFoundUninitializedWarning = ContainsWarningDiagnostic(Engine, TEXT("may not be initialized"));

	TestTrue(TEXT("NotInitialized should preserve the compiler warning for reading an uninitialized variable"), bFoundUninitializedWarning);

	}
	return bFoundUninitializedWarning;
}

bool FAngelscriptControlFlowNotInitializedBranchDefiniteAssignmentMatrixTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptControlFlowTests_Private;
	bool bPassed = true;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

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

	bPassed &= TestTrue(
		TEXT("Branch definite-assignment partial test case should still compile"),
		bPartialCompiled);
	bPassed &= TestTrue(
		TEXT("Branch definite-assignment partial test case should report bCompileSucceeded=true"),
		PartialSummary.bCompileSucceeded);
	bPassed &= TestTrue(
		TEXT("Branch definite-assignment partial test case should stay on a handled compile path"),
		PartialSummary.CompileResult == ECompileResult::FullyHandled
			|| PartialSummary.CompileResult == ECompileResult::PartiallyHandled);
	bPassed &= TestTrue(
		TEXT("Branch definite-assignment partial test case should capture at least one warning diagnostic for Value"),
		PartialWarning != nullptr);
	if (PartialWarning != nullptr)
	{
		bPassed &= TestTrue(
			TEXT("Branch definite-assignment partial warning should report a non-zero row"),
			PartialWarning->Row > 0);
		bPassed &= TestTrue(
			TEXT("Branch definite-assignment partial warning should report a non-zero column"),
			PartialWarning->Column > 0);
	}

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

	bPassed &= TestTrue(
		TEXT("Branch definite-assignment full test case should compile"),
		bFullCompiled);
	bPassed &= TestTrue(
		TEXT("Branch definite-assignment full test case should report bCompileSucceeded=true"),
		FullSummary.bCompileSucceeded);
	bPassed &= TestTrue(
		TEXT("Branch definite-assignment full test case should stay on a handled compile path"),
		FullSummary.CompileResult == ECompileResult::FullyHandled
			|| FullSummary.CompileResult == ECompileResult::PartiallyHandled);
	bPassed &= TestTrue(
		TEXT("Branch definite-assignment full test case should not emit an uninitialized warning for Value"),
		FullWarning == nullptr);

	int32 SafeTrueResult = 0;
	int32 SafeFalseResult = 0;
	bPassed &= ExecuteIntFunction(&Engine, FullModuleName, TEXT("int RunSafeTrue()"), SafeTrueResult);
	bPassed &= ExecuteIntFunction(&Engine, FullModuleName, TEXT("int RunSafeFalse()"), SafeFalseResult);
	bPassed &= TestEqual(
		TEXT("Branch definite-assignment full test case should execute the true wrapper result"),
		SafeTrueResult,
		7);
	bPassed &= TestEqual(
		TEXT("Branch definite-assignment full test case should execute the false wrapper result"),
		SafeFalseResult,
		9);

	}
	return bPassed;
}

#endif
