#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

using AngelscriptNativeTestSupport::AppendGeneratedAsLine;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FStatementTransferTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.ControlFlow.StatementTransfers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FNamedCase
	{
		const ANSICHAR* CatalogName;
		int32 Weight = 0;
	};

	struct FStatementTransferExpectation
	{
		int32 ReturnValue = 0;
		bool bThrows = false;
	};

	inline static constexpr FNamedCase StatementCases[] =
	{
		{ "if", 1 }, { "if_else", 2 }, { "else_if", 3 }, { "while", 4 }, { "do_while", 5 }, { "for", 6 }, { "switch", 7 }, { "nested_block", 8 },
	};
	inline static constexpr FNamedCase CountCases[] =
	{
		{ "zero", 0 }, { "one", 1 }, { "two", 2 }, { "many", 4 },
	};
	inline static constexpr FNamedCase TransferCases[] =
	{
		{ "none" }, { "break_loop" }, { "break_switch" }, { "continue" }, { "early_return" }, { "nested_return" }, { "fallthrough" }, { "exception" },
	};
	inline static constexpr FNamedCase NestingCases[] =
	{
		{ "none", 0 }, { "same_kind", 10 }, { "mixed_loop", 20 }, { "loop_switch", 30 }, { "branch_loop", 40 }, { "three_level", 50 },
	};


	static bool IsCase(const FNamedCase& Case, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(Case.CatalogName, Name) == 0;
	}

	static void AppendStatementBody(FString& Source, const FNamedCase& StatementCase, const FString& Indent)
	{
		if (IsCase(StatementCase, "if"))
		{
			AppendGeneratedAsLine(Source, Indent + TEXT("if (Index >= 0)"));
			AppendGeneratedAsLine(Source, Indent + TEXT("{"));
			AppendGeneratedAsLine(Source, Indent + FString::Printf(TEXT("\tTrace += %d;"), StatementCase.Weight));
			AppendGeneratedAsLine(Source, Indent + TEXT("}"));
		}
		else if (IsCase(StatementCase, "if_else"))
		{
			AppendGeneratedAsLine(Source, Indent + TEXT("if ((Index & 1) == 0)"));
			AppendGeneratedAsLine(Source, Indent + TEXT("{"));
			AppendGeneratedAsLine(Source, Indent + FString::Printf(TEXT("\tTrace += %d;"), StatementCase.Weight));
			AppendGeneratedAsLine(Source, Indent + TEXT("}"));
			AppendGeneratedAsLine(Source, Indent + TEXT("else"));
			AppendGeneratedAsLine(Source, Indent + TEXT("{"));
			AppendGeneratedAsLine(Source, Indent + FString::Printf(TEXT("\tTrace += %d;"), StatementCase.Weight));
			AppendGeneratedAsLine(Source, Indent + TEXT("}"));
		}
		else if (IsCase(StatementCase, "else_if"))
		{
			AppendGeneratedAsLine(Source, Indent + TEXT("if (Index < 0)"));
			AppendGeneratedAsLine(Source, Indent + TEXT("{"));
			AppendGeneratedAsLine(Source, Indent + TEXT("\tTrace += 1000;"));
			AppendGeneratedAsLine(Source, Indent + TEXT("}"));
			AppendGeneratedAsLine(Source, Indent + TEXT("else if (Index >= 0)"));
			AppendGeneratedAsLine(Source, Indent + TEXT("{"));
			AppendGeneratedAsLine(Source, Indent + FString::Printf(TEXT("\tTrace += %d;"), StatementCase.Weight));
			AppendGeneratedAsLine(Source, Indent + TEXT("}"));
			AppendGeneratedAsLine(Source, Indent + TEXT("else"));
			AppendGeneratedAsLine(Source, Indent + TEXT("{"));
			AppendGeneratedAsLine(Source, Indent + TEXT("\tTrace += 2000;"));
			AppendGeneratedAsLine(Source, Indent + TEXT("}"));
		}
		else if (IsCase(StatementCase, "while"))
		{
			AppendGeneratedAsLine(Source, Indent + TEXT("int Once = 0;"));
			AppendGeneratedAsLine(Source, Indent + TEXT("while (Once < 1)"));
			AppendGeneratedAsLine(Source, Indent + TEXT("{"));
			AppendGeneratedAsLine(Source, Indent + TEXT("\t++Once;"));
			AppendGeneratedAsLine(Source, Indent + FString::Printf(TEXT("\tTrace += %d;"), StatementCase.Weight));
			AppendGeneratedAsLine(Source, Indent + TEXT("}"));
		}
		else if (IsCase(StatementCase, "do_while"))
		{
			AppendGeneratedAsLine(Source, Indent + TEXT("int Once = 0;"));
			AppendGeneratedAsLine(Source, Indent + TEXT("do"));
			AppendGeneratedAsLine(Source, Indent + TEXT("{"));
			AppendGeneratedAsLine(Source, Indent + TEXT("\t++Once;"));
			AppendGeneratedAsLine(Source, Indent + FString::Printf(TEXT("\tTrace += %d;"), StatementCase.Weight));
			AppendGeneratedAsLine(Source, Indent + TEXT("}"));
			AppendGeneratedAsLine(Source, Indent + TEXT("while (Once < 1);"));
		}
		else if (IsCase(StatementCase, "for"))
		{
			AppendGeneratedAsLine(Source, Indent + TEXT("for (int Once = 0; Once < 1; ++Once)"));
			AppendGeneratedAsLine(Source, Indent + TEXT("{"));
			AppendGeneratedAsLine(Source, Indent + FString::Printf(TEXT("\tTrace += %d;"), StatementCase.Weight));
			AppendGeneratedAsLine(Source, Indent + TEXT("}"));
		}
		else if (IsCase(StatementCase, "switch"))
		{
			AppendGeneratedAsLine(Source, Indent + TEXT("switch (Index & 1)"));
			AppendGeneratedAsLine(Source, Indent + TEXT("{"));
			AppendGeneratedAsLine(Source, Indent + TEXT("case 0:"));
			AppendGeneratedAsLine(Source, Indent + FString::Printf(TEXT("\tTrace += %d;"), StatementCase.Weight));
			AppendGeneratedAsLine(Source, Indent + TEXT("\tbreak;"));
			AppendGeneratedAsLine(Source, Indent + TEXT("default:"));
			AppendGeneratedAsLine(Source, Indent + FString::Printf(TEXT("\tTrace += %d;"), StatementCase.Weight));
			AppendGeneratedAsLine(Source, Indent + TEXT("\tbreak;"));
			AppendGeneratedAsLine(Source, Indent + TEXT("}"));
		}
		else
		{
			AppendGeneratedAsLine(Source, Indent + TEXT("{"));
			AppendGeneratedAsLine(Source, Indent + TEXT("\t{"));
			AppendGeneratedAsLine(Source, Indent + FString::Printf(TEXT("\t\tTrace += %d;"), StatementCase.Weight));
			AppendGeneratedAsLine(Source, Indent + TEXT("\t}"));
			AppendGeneratedAsLine(Source, Indent + TEXT("}"));
		}
	}

	static void AppendNestedStatement(
		FString& Source,
		const FNamedCase& StatementCase,
		const FNamedCase& NestingCase)
	{
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t\tTrace += %d;"), NestingCase.Weight));
		if (IsCase(NestingCase, "none"))
		{
			AppendStatementBody(Source, StatementCase, TEXT("\t\t"));
		}
		else if (IsCase(NestingCase, "same_kind"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tfor (int SameKind = 0; SameKind < 1; ++SameKind)"));
			AppendGeneratedAsLine(Source, TEXT("\t\t{"));
			AppendStatementBody(Source, StatementCase, TEXT("\t\t\t"));
			AppendGeneratedAsLine(Source, TEXT("\t\t}"));
		}
		else if (IsCase(NestingCase, "mixed_loop"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tint MixedLoop = 0;"));
			AppendGeneratedAsLine(Source, TEXT("\t\twhile (MixedLoop < 1)"));
			AppendGeneratedAsLine(Source, TEXT("\t\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\t++MixedLoop;"));
			AppendStatementBody(Source, StatementCase, TEXT("\t\t\t"));
			AppendGeneratedAsLine(Source, TEXT("\t\t}"));
		}
		else if (IsCase(NestingCase, "loop_switch"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tswitch (0)"));
			AppendGeneratedAsLine(Source, TEXT("\t\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tcase 0:"));
			// The current fork rejects declarations directly under a case label;
			// scope every generated case body so loop/initializer statements remain legal.
			AppendGeneratedAsLine(Source, TEXT("\t\t{"));
			AppendStatementBody(Source, StatementCase, TEXT("\t\t\t"));
			AppendGeneratedAsLine(Source, TEXT("\t\t}"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\tbreak;"));
			AppendGeneratedAsLine(Source, TEXT("\t\tdefault:"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\tbreak;"));
			AppendGeneratedAsLine(Source, TEXT("\t\t}"));
		}
		else if (IsCase(NestingCase, "branch_loop"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tif (Index >= 0)"));
			AppendGeneratedAsLine(Source, TEXT("\t\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\tfor (int BranchLoop = 0; BranchLoop < 1; ++BranchLoop)"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\t{"));
			AppendStatementBody(Source, StatementCase, TEXT("\t\t\t\t"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\t}"));
			AppendGeneratedAsLine(Source, TEXT("\t\t}"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tif (Index >= 0)"));
			AppendGeneratedAsLine(Source, TEXT("\t\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\tswitch (0)"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\tcase 0:"));
			// A declaration directly after a case label is rejected by the fork;
			// scope the case body while preserving the same switch/loop nesting.
			AppendGeneratedAsLine(Source, TEXT("\t\t\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\t\tint ThreeLevel = 0;"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\t\twhile (ThreeLevel < 1)"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\t\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\t\t\t++ThreeLevel;"));
			AppendStatementBody(Source, StatementCase, TEXT("\t\t\t\t\t"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\t\t}"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\t\tbreak;"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\t}"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\tdefault:"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\t\tbreak;"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\t}"));
			AppendGeneratedAsLine(Source, TEXT("\t\t}"));
		}
	}

	static void AppendTransfer(FString& Source, const FNamedCase& TransferCase)
	{
		if (IsCase(TransferCase, "break_loop"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 10;"));
			AppendGeneratedAsLine(Source, TEXT("\t\tbreak;"));
		}
		else if (IsCase(TransferCase, "break_switch"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tswitch (0)"));
			AppendGeneratedAsLine(Source, TEXT("\t\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tcase 0:"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\tTrace += 10;"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\tbreak;"));
			AppendGeneratedAsLine(Source, TEXT("\t\tdefault:"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\tTrace += 1000;"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\tbreak;"));
			AppendGeneratedAsLine(Source, TEXT("\t\t}"));
			AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 1;"));
		}
		else if (IsCase(TransferCase, "continue"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 10;"));
			AppendGeneratedAsLine(Source, TEXT("\t\tcontinue;"));
		}
		else if (IsCase(TransferCase, "early_return"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Trace + 20;"));
		}
		else if (IsCase(TransferCase, "nested_return"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tif (Trace >= 0)"));
			AppendGeneratedAsLine(Source, TEXT("\t\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\treturn Trace + 30;"));
			AppendGeneratedAsLine(Source, TEXT("\t\t}"));
		}
		else if (IsCase(TransferCase, "fallthrough"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tswitch (0)"));
			AppendGeneratedAsLine(Source, TEXT("\t\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tcase 0:"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\tTrace += 10;"));
			AppendGeneratedAsLine(Source, TEXT("\t\tcase 1:"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\tTrace += 1;"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\tbreak;"));
			AppendGeneratedAsLine(Source, TEXT("\t\tdefault:"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\tbreak;"));
			AppendGeneratedAsLine(Source, TEXT("\t\t}"));
		}
		else if (IsCase(TransferCase, "exception"))
		{
			// The current fork does not recognize string literals in throw(); use a
			// runtime divide-by-zero fault so the transfer path remains executable.
			AppendGeneratedAsLine(Source, TEXT("\t\tint ExceptionDivisor = 0;"));
			AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 1 / ExceptionDivisor;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 1;"));
		}
	}

	static FString BuildStatementTransferSource(
		const FNamedCase& StatementCase,
		const FNamedCase& CountCase,
		const FNamedCase& TransferCase,
		const FNamedCase& NestingCase)
	{
		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int Entry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Trace = 0;"));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tint Limit = %d;"), CountCase.Weight));
		AppendGeneratedAsLine(Source, TEXT("\tfor (int Index = 0; Index < Limit; ++Index)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendNestedStatement(Source, StatementCase, NestingCase);
		AppendTransfer(Source, TransferCase);
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Trace;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static FStatementTransferExpectation ExpectedResult(
		const FNamedCase& StatementCase,
		const FNamedCase& CountCase,
		const FNamedCase& TransferCase,
		const FNamedCase& NestingCase)
	{
		FStatementTransferExpectation Result;
		const int32 PerIteration = StatementCase.Weight + NestingCase.Weight;
		if (CountCase.Weight == 0)
		{
			return Result;
		}

		if (IsCase(TransferCase, "exception"))
		{
			Result.bThrows = true;
			return Result;
		}
		if (IsCase(TransferCase, "break_loop"))
		{
			Result.ReturnValue = PerIteration + 10;
		}
		else if (IsCase(TransferCase, "early_return"))
		{
			Result.ReturnValue = PerIteration + 20;
		}
		else if (IsCase(TransferCase, "nested_return"))
		{
			Result.ReturnValue = PerIteration + 30;
		}
		else if (IsCase(TransferCase, "break_switch") || IsCase(TransferCase, "fallthrough"))
		{
			Result.ReturnValue = CountCase.Weight * (PerIteration + 11);
		}
		else if (IsCase(TransferCase, "continue"))
		{
			Result.ReturnValue = CountCase.Weight * (PerIteration + 10);
		}
		else
		{
			Result.ReturnValue = CountCase.Weight * (PerIteration + 1);
		}
		return Result;
	}

public:
	TEST_METHOD(StatementsByCountTransferAndNesting)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-CF-STATEMENT-COUNT-TRANSFER",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Bytecode);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Statement transfer product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		for (const FNamedCase& StatementCase : StatementCases)
		{
			for (const FNamedCase& CountCase : CountCases)
			{
				for (const FNamedCase& TransferCase : TransferCases)
				{
					for (const FNamedCase& NestingCase : NestingCases)
					{
						const FNativeCaseContext Case(MakeNativeCaseId("LANG-CF-STATEMENT-COUNT-TRANSFER",
							{ ANSI_TO_TCHAR(StatementCase.CatalogName), ANSI_TO_TCHAR(CountCase.CatalogName), ANSI_TO_TCHAR(TransferCase.CatalogName), ANSI_TO_TCHAR(NestingCase.CatalogName) }));
						const FString ModuleName = TEXT("StatementTransfer_") + Case.GetId().RightChop(31).Replace(TEXT("-"), TEXT("_"));
						const FString Source = BuildStatementTransferSource(StatementCase, CountCase, TransferCase, NestingCase);
						const FStatementTransferExpectation Expected = ExpectedResult(StatementCase, CountCase, TransferCase, NestingCase);
						PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);
						const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
						const FTCHARToUTF8 SourceUtf8(*Source);
						Engine.ResetMessages();
						asIScriptModule* Module = nullptr;
						const int BuildResult = CompileNativeModule(ScriptEngine, ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
						ASSERT_THAT(AreEqual(asSUCCESS, BuildResult,
							*FString::Printf(TEXT("%s statement transfer cell should compile. BuildResult=%d Messages={%s}"),
								*Case.GetId(), BuildResult, *Engine.GetMessagesText())));
						asIScriptFunction* const Entry = GetNativeFunctionByExactDecl(Module, "int Entry()");
						ASSERT_THAT(IsNotNull(Entry, *Case.Describe(TEXT("statement transfer should publish exact Entry declaration"))));
						if (Entry != nullptr)
						{
							asIScriptContext* const Context = ScriptEngine->CreateContext();
							ASSERT_THAT(IsNotNull(Context, *Case.Describe(TEXT("statement transfer should create a context"))));
							if (Context != nullptr)
							{
								const int ExecuteResult = PrepareAndExecute(Context, Entry);
								ASSERT_THAT(AreEqual(Expected.bThrows ? asEXECUTION_EXCEPTION : asEXECUTION_FINISHED, ExecuteResult,
									*Case.Describe(TEXT("statement transfer execution state should match the selected transfer"))));
								if (Expected.bThrows)
								{
									ASSERT_THAT(AreEqual(FString(TEXT("Divide by zero")), FString(UTF8_TO_TCHAR(Context->GetExceptionString())),
										*Case.Describe(TEXT("statement transfer should preserve the fork's exact divide-by-zero exception text"))));
								}
								else
								{
									ASSERT_THAT(AreEqual(Expected.ReturnValue, static_cast<int32>(Context->GetReturnDWord()),
										*Case.Describe(TEXT("statement transfer should preserve the exact selected trace"))));
								}
								asUINT BytecodeLength = 0;
								Entry->GetByteCode(&BytecodeLength);
								ASSERT_THAT(IsTrue(BytecodeLength > 0,
									*Case.Describe(TEXT("statement transfer entry should retain bytecode"))));
								Context->Release();
							}
						}
						ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
						ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
							*Case.Describe(TEXT("statement transfer cell should discard its module"))));
					}
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
