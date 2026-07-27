#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "Support/AngelscriptNativeCaseTestSupport.h"
#include "AngelscriptTestMacros.h"

// Raw SDK variable-scope coverage.
// AngelscriptSDKVariableScopeTests.cpp
// Tests for as_variablescope.cpp - variable scope isolation and shadowing.
// Automation IDs: Angelscript.TestModule.AngelScriptSDK.TypeSystem.VariableScope.*

#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"
#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FVariableScopeTests, "Angelscript.TestModule.AngelScriptSDK.TypeSystem.VariableScope", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ScopesByConstructAndOutcome)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		AS_NATIVE_PRODUCT("TYPE-VARIABLE-SCOPE-BOUNDARIES",
			ENativeEvidence::Compile
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Runtime
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Variable-scope boundary product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const TCHAR* Constructs[] =
		{
			TEXT("block"),
			TEXT("for"),
			TEXT("while"),
			TEXT("if"),
		};
		const TCHAR* Outcomes[] =
		{
			TEXT("inside-valid"),
			TEXT("outside-rejected"),
		};

		for (const TCHAR* Construct : Constructs)
		{
			for (const TCHAR* Outcome : Outcomes)
			{
				const bool bExpectedSuccess =
					FCString::Strcmp(Outcome, TEXT("inside-valid")) == 0;
				const FString CaseId = MakeNativeCaseId(
					"TYPE-VARIABLE-SCOPE-BOUNDARIES",
					{ Construct, Outcome });
				const FString ModuleName =
					TEXT("TypeVariableScope_")
					+ CaseId.Replace(TEXT("-"), TEXT("_"));
				FString Source;
				const TCHAR* EscapedIdentifier = TEXT("");
				int32 ExpectedReturn = INDEX_NONE;

				if (FCString::Strcmp(Construct, TEXT("block")) == 0)
				{
					EscapedIdentifier = TEXT("BlockValue");
					AppendGeneratedAsLine(Source, TEXT("int Entry()"));
					AppendGeneratedAsLine(Source, TEXT("{"));
					AppendGeneratedAsLine(Source, TEXT("\tint Result = 1;"));
					AppendGeneratedAsLine(Source, TEXT("\t{"));
					AppendGeneratedAsLine(Source, TEXT("\t\tint BlockValue = 4;"));
					if (bExpectedSuccess)
					{
						AppendGeneratedAsLine(Source, TEXT("\t\tResult += BlockValue;"));
					}
					AppendGeneratedAsLine(Source, TEXT("\t}"));
					if (!bExpectedSuccess)
					{
						AppendGeneratedAsLine(Source, TEXT("\tResult += BlockValue;"));
					}
					AppendGeneratedAsLine(Source, TEXT("\treturn Result;"));
					AppendGeneratedAsLine(Source, TEXT("}"));
					ExpectedReturn = 5;
				}
				else if (FCString::Strcmp(Construct, TEXT("for")) == 0)
				{
					EscapedIdentifier = TEXT("LoopIndex");
					AppendGeneratedAsLine(Source, TEXT("int Entry()"));
					AppendGeneratedAsLine(Source, TEXT("{"));
					AppendGeneratedAsLine(Source, TEXT("\tint Result = 0;"));
					AppendGeneratedAsLine(Source, TEXT("\tfor (int LoopIndex = 0; LoopIndex < 3; ++LoopIndex)"));
					AppendGeneratedAsLine(Source, TEXT("\t{"));
					AppendGeneratedAsLine(Source, TEXT("\t\tResult += LoopIndex;"));
					AppendGeneratedAsLine(Source, TEXT("\t}"));
					if (!bExpectedSuccess)
					{
						AppendGeneratedAsLine(Source, TEXT("\tResult += LoopIndex;"));
					}
					AppendGeneratedAsLine(Source, TEXT("\treturn Result;"));
					AppendGeneratedAsLine(Source, TEXT("}"));
					ExpectedReturn = 3;
				}
				else if (FCString::Strcmp(Construct, TEXT("while")) == 0)
				{
					EscapedIdentifier = TEXT("Step");
					AppendGeneratedAsLine(Source, TEXT("int Entry()"));
					AppendGeneratedAsLine(Source, TEXT("{"));
					AppendGeneratedAsLine(Source, TEXT("\tint Result = 0;"));
					AppendGeneratedAsLine(Source, TEXT("\tint Index = 0;"));
					AppendGeneratedAsLine(Source, TEXT("\twhile (Index < 3)"));
					AppendGeneratedAsLine(Source, TEXT("\t{"));
					AppendGeneratedAsLine(Source, TEXT("\t\tint Step = Index + 1;"));
					AppendGeneratedAsLine(Source, TEXT("\t\tResult += Step;"));
					AppendGeneratedAsLine(Source, TEXT("\t\t++Index;"));
					AppendGeneratedAsLine(Source, TEXT("\t}"));
					if (!bExpectedSuccess)
					{
						AppendGeneratedAsLine(Source, TEXT("\tResult += Step;"));
					}
					AppendGeneratedAsLine(Source, TEXT("\treturn Result;"));
					AppendGeneratedAsLine(Source, TEXT("}"));
					ExpectedReturn = 6;
				}
				else
				{
					EscapedIdentifier = TEXT("BranchValue");
					AppendGeneratedAsLine(Source, TEXT("int Entry()"));
					AppendGeneratedAsLine(Source, TEXT("{"));
					AppendGeneratedAsLine(Source, TEXT("\tint Result = 1;"));
					AppendGeneratedAsLine(Source, TEXT("\tif (Result == 1)"));
					AppendGeneratedAsLine(Source, TEXT("\t{"));
					AppendGeneratedAsLine(Source, TEXT("\t\tint BranchValue = 8;"));
					if (bExpectedSuccess)
					{
						AppendGeneratedAsLine(Source, TEXT("\t\tResult += BranchValue;"));
					}
					AppendGeneratedAsLine(Source, TEXT("\t}"));
					if (!bExpectedSuccess)
					{
						AppendGeneratedAsLine(Source, TEXT("\tResult += BranchValue;"));
					}
					AppendGeneratedAsLine(Source, TEXT("\treturn Result;"));
					AppendGeneratedAsLine(Source, TEXT("}"));
					ExpectedReturn = 9;
				}

				PrintGeneratedAsSource(
					*TestRunner,
					CaseId,
					ModuleName,
					Source);
				const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
				const FTCHARToUTF8 SourceUtf8(*Source);
				Engine.ResetMessages();
				asIScriptModule* Module = nullptr;
				const int BuildResult = CompileNativeModule(
					ScriptEngine,
					ModuleNameUtf8.Get(),
					SourceUtf8.Get(),
					Module);

				if (!bExpectedSuccess)
				{
					ASSERT_THAT(IsTrue(
						BuildResult < 0,
						*FString::Printf(
							TEXT("%s should reject the escaped %s identifier"),
							*CaseId,
							EscapedIdentifier)));
					ASSERT_THAT(IsTrue(
						Engine.GetMessagesText().Contains(EscapedIdentifier),
						*FString::Printf(
							TEXT("%s diagnostic should name the escaped identifier; messages={%s}"),
							*CaseId,
							*Engine.GetMessagesText())));
					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					continue;
				}

				ASSERT_THAT(AreEqual(
					asSUCCESS,
					BuildResult,
					*FString::Printf(
						TEXT("%s should compile; messages={%s}"),
						*CaseId,
						*Engine.GetMessagesText())));
				if (BuildResult < 0 || Module == nullptr)
				{
					continue;
				}

				FSdkFunctionInvoker Invoker(
					*TestRunner,
					ScriptEngine,
					Module,
					"int Entry()");
				ASSERT_THAT(IsTrue(
					Invoker.IsValid(),
					*FString::Printf(
						TEXT("%s should prepare exact Entry"),
						*CaseId)));
				if (Invoker.IsValid())
				{
					ASSERT_THAT(AreEqual(
						ExpectedReturn,
						Invoker.CallAndReturn<int32>(INDEX_NONE),
						*FString::Printf(
							TEXT("%s should preserve inside-scope runtime value"),
							*CaseId)));
				}
				ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
				ASSERT_THAT(IsNull(
					ScriptEngine->GetModule(
						ModuleNameUtf8.Get(),
						asGM_ONLY_IF_EXISTS),
					*FString::Printf(
						TEXT("%s module should be discarded"),
						*CaseId)));
			}
		}
	}

	TEST_METHOD(VariableScopeIsolation)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"TYPE-VARIABLE-SCOPE-BOUNDARIES supersedes this block-only rejection with block/for/while/if positive and escaped-identifier controls");

		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));

		// Variable declared in inner scope should not be visible in outer scope
		Engine.ResetMessages();
		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "ScopeIso");
		asIScriptModule* M = BuildNativeModule(SE, "ScopeIso", ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				{
					int x = 5;
				}
				return x;
			}
		)AS"));
		ASSERT_THAT(IsNull(M, TEXT("Access to out-of-scope variable should fail compilation")));
	}

	TEST_METHOD(VariableScopeShadowing)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"LANG-VAR-SHADOW and TYPE-VARIABLE-SCOPE-BOUNDARIES supersede this single nested shadow with depth, storage owner, runtime restoration, and boundary evidence");

		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));

		AngelscriptNativeTestSupport::FScopedNativeModule M(*TestRunner, Engine, "ScopeShadow", ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				int x = 10;
				{
					int x = 20;
				}
				return x;
			}
		)AS"));
		if (!M.IsValid())
		{
			return;
		}

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "int Entry()", Result)) return;
		ASSERT_THAT(AreEqual(10, Result, TEXT("Outer x should remain 10 after inner shadow")));
	}

	TEST_METHOD(VariableScopeNestedBlocks)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		AS_NATIVE_NON_PRODUCT(
			"AggregateSupport",
			"LANG-VAR-LIFETIME and TYPE-VARIABLE-SCOPE-BOUNDARIES own block lifetime and escape rejection; this fixed sum remains aggregate compatibility evidence");

		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));

		AngelscriptNativeTestSupport::FScopedNativeModule M(*TestRunner, Engine, "ScopeNested", ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				int sum = 0;
				{
					int a = 1;
					sum += a;
				}
				{
					int b = 2;
					sum += b;
				}
				{
					int c = 3;
					{
						int d = 4;
						sum += d;
					}
					sum += c;
				}
				return sum;
			}
		)AS"));
		if (!M.IsValid())
		{
			return;
		}

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "int Entry()", Result)) return;
		ASSERT_THAT(AreEqual(10, Result, TEXT("sum = 1+2+4+3 = 10")));
	}

	TEST_METHOD(VariableScopeForInitScope)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"LANG-VAR-LOOP-DECL-LIFETIME and TYPE-VARIABLE-SCOPE-BOUNDARIES supersede this one for-init example with loop owner, transfer path, positive runtime, and escaped-index rejection");

		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));

		// Loop counter declared in for-init is scoped to the loop; a same-named
		// outer variable is unaffected, and two sequential loops may reuse the name.
		AngelscriptNativeTestSupport::FScopedNativeModule M(*TestRunner, Engine, "ScopeForInit", ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				int i = 100;
				int sum = 0;
				for (int i = 0; i < 5; i++)
				{
					sum += i;
				}
				// 0+1+2+3+4 = 10
				for (int i = 0; i < 3; i++)
				{
					sum += i;
				}
				// +0+1+2 = 13
				return sum + i;
				// 13 + 100 = 113
			}
		)AS"));
		if (!M.IsValid())
		{
			return;
		}

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "int Entry()", Result)) return;
		ASSERT_THAT(AreEqual(113, Result,
			TEXT("for-init counters stay loop-scoped; outer i preserved (13+100=113)")));
	}

	TEST_METHOD(ForInitLeakRejected)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"TYPE-VARIABLE-SCOPE-BOUNDARIES supersedes this one for-init leak with paired positive and negative block/for/while/if scope controls and named diagnostics");

		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));

		// A for-init counter must not be visible after the loop body.
		Engine.ResetMessages();
		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "ScopeForInitLeak");
		asIScriptModule* M = BuildNativeModule(SE, "ScopeForInitLeak", ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				for (int k = 0; k < 3; k++)
				{
				}
				return k;
			}
		)AS"));
		ASSERT_THAT(IsNull(M, TEXT("Referencing a for-init counter after the loop should fail compilation")));
	}

	TEST_METHOD(VariableScopeDeepShadowing)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"LANG-VAR-SHADOW supersedes this fixed four-level example with generated depth, owner restoration, runtime value, and isolation evidence");

		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));

		// Each nested block may re-shadow the same name; the innermost value is
		// used within its block, and each outer value is restored on block exit.
		AngelscriptNativeTestSupport::FScopedNativeModule M(*TestRunner, Engine, "ScopeDeepShadow", ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				int x = 1;
				int captured = 0;
				{
					int x = 2;
					{
						int x = 3;
						{
							int x = 4;
							captured += x;
							// 4
						}
						captured += x;
						// +3 = 7
					}
					captured += x;
					// +2 = 9
				}
				captured += x;
				// +1 = 10
				return captured;
			}
		)AS"));
		if (!M.IsValid())
		{
			return;
		}

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "int Entry()", Result)) return;
		ASSERT_THAT(AreEqual(10, Result, TEXT("four-level shadow sums 4+3+2+1 = 10")));
	}

	TEST_METHOD(WhileAndIfBlockScope)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		AS_NATIVE_NON_PRODUCT(
			"AggregateSupport",
			"TYPE-VARIABLE-SCOPE-BOUNDARIES independently owns while and if positive/negative scope boundaries; this combined fixed accumulator remains aggregate compatibility evidence");

		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));

		// Variables declared inside while/if bodies are block-scoped; the outer
		// accumulator survives across iterations.
		AngelscriptNativeTestSupport::FScopedNativeModule M(*TestRunner, Engine, "ScopeWhileIf", ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				int sum = 0;
				int i = 0;
				while (i < 4)
				{
					int step = i * 2;
					// block-scoped to the loop body
					sum += step;
					i++;
				}
				if (sum > 0)
				{
					int bonus = 100;
					// block-scoped to the if body
					sum += bonus;
				}
				return sum;
				// (0+2+4+6) + 100 = 112
			}
		)AS"));
		if (!M.IsValid())
		{
			return;
		}

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "int Entry()", Result)) return;
		ASSERT_THAT(AreEqual(112, Result,
			TEXT("while/if block-scoped locals; outer sum = 12+100 = 112")));
	}

	TEST_METHOD(IfBlockLeakRejected)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"TYPE-VARIABLE-SCOPE-BOUNDARIES supersedes this one if-body leak with exact paired scope-boundary evidence across block, for, while, and if constructs");

		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));

		// A variable declared inside an if body must not be visible afterward.
		Engine.ResetMessages();
		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "ScopeIfLeak");
		asIScriptModule* M = BuildNativeModule(SE, "ScopeIfLeak", ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				if (true)
				{
					int inner = 7;
				}
				return inner;
			}
		)AS"));
		ASSERT_THAT(IsNull(M, TEXT("Referencing an if-body local after the block should fail compilation")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
