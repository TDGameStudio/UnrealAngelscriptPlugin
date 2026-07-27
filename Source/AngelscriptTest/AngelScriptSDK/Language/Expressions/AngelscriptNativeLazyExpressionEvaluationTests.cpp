#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"
#include "../../Support/AngelscriptNativeExpressionEvaluationTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"


#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FLazyExpressionEvaluationTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Expressions.Evaluation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext = AngelscriptNativeTestSupport::FNativeCaseContext;

	static void AppendGeneratedAsLine(
		FString& Source,
		const FString& Line = FString())
	{
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(Source, Line);
	}

	struct FFormCase
	{
		const ANSICHAR* CatalogName;
		bool bLogicalAnd;
		bool bLogicalOr;
	};

	struct FSelectorCase
	{
		const ANSICHAR* CatalogName;
		bool Value;
	};

	struct FOperandOutcomeCase
	{
		const ANSICHAR* CatalogName;
		bool bSideEffect;
		bool bException;
	};

	struct FSourceShapeCase
	{
		const ANSICHAR* CatalogName;
	};


	inline static constexpr FFormCase FormCases[] = {
		{"logical_and", true, false},
		{"logical_or", false, true},
		{"conditional", false, false},
	};

	inline static constexpr FSelectorCase SelectorCases[] = {
		{"false", false},
		{"true", true},
	};

	inline static constexpr FOperandOutcomeCase OperandOutcomeCases[] = {
		{"value", false, false},
		{"side_effect", true, false},
		{"exception", false, true},
	};

	inline static constexpr FSourceShapeCase SourceShapeCases[] = {
		{"single_line"},
		{"comments"},
		{"multiline"},
		{"parenthesized"},
	};


	static bool IsSourceShape(const FSourceShapeCase& SourceShapeCase, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(SourceShapeCase.CatalogName, Name) == 0;
	}

	static FString MakeSuffix(const FFormCase& FormCase,
		const FOperandOutcomeCase& OperandOutcomeCase,
		const FSelectorCase& SelectorCase,
		const FSourceShapeCase& SourceShapeCase)
	{
		return FString::Printf(TEXT("%hs_%hs_%hs_%hs"),
			FormCase.CatalogName,
			OperandOutcomeCase.CatalogName,
			SelectorCase.CatalogName,
			SourceShapeCase.CatalogName);
	}

	static bool IsGuardedOperandSelected(
		const FFormCase& FormCase, const FSelectorCase& SelectorCase)
	{
		if (FormCase.bLogicalAnd)
		{
			return SelectorCase.Value;
		}
		if (FormCase.bLogicalOr)
		{
			return !SelectorCase.Value;
		}
		return SelectorCase.Value;
	}

	static FString MakeGuardedExpression(
		const FFormCase& FormCase, const FOperandOutcomeCase& OperandOutcomeCase)
	{
		if (OperandOutcomeCase.bException)
		{
			return FormCase.bLogicalAnd || FormCase.bLogicalOr ? TEXT("RaiseGuardedBool()")
															   : TEXT("RaiseGuardedInt()");
		}
		if (OperandOutcomeCase.bSideEffect)
		{
			return FormCase.bLogicalAnd || FormCase.bLogicalOr
					   ? TEXT("RecordExpressionBool(2, true)")
					   : TEXT("RecordExpressionInt(2, 41)");
		}
		return FormCase.bLogicalAnd || FormCase.bLogicalOr ? TEXT("true") : TEXT("41");
	}

	static FString MakeSelectorExpression(const FSelectorCase& SelectorCase)
	{
		return FString::Printf(
			TEXT("RecordExpressionBool(1, %s)"), SelectorCase.Value ? TEXT("true") : TEXT("false"));
	}

	static FString MakeSingleLineExpression(const FFormCase& FormCase,
		const FOperandOutcomeCase& OperandOutcomeCase,
		const FSelectorCase& SelectorCase,
		const FSourceShapeCase& SourceShapeCase)
	{
		const FString Selector = MakeSelectorExpression(SelectorCase);
		const FString Guarded = MakeGuardedExpression(FormCase, OperandOutcomeCase);
		FString Expression;
		if (FormCase.bLogicalAnd)
		{
			Expression = FString::Printf(TEXT("%s && %s ? 1 : 0"), *Selector, *Guarded);
		}
		else if (FormCase.bLogicalOr)
		{
			Expression = FString::Printf(TEXT("%s || %s ? 1 : 0"), *Selector, *Guarded);
		}
		else
		{
			Expression =
				FString::Printf(TEXT("%s ? %s : RecordExpressionInt(3, 23)"), *Selector, *Guarded);
		}

		if (IsSourceShape(SourceShapeCase, "comments"))
		{
			if (FormCase.bLogicalAnd)
			{
				Expression = FString::Printf(
					TEXT("%s /* selector */ && /* guarded */ %s ? 1 : 0"), *Selector, *Guarded);
			}
			else if (FormCase.bLogicalOr)
			{
				Expression = FString::Printf(
					TEXT("%s /* selector */ || /* guarded */ %s ? 1 : 0"), *Selector, *Guarded);
			}
			else
			{
				Expression = FString::Printf(TEXT("%s /* selector */ ? /* guarded */ %s : /* "
												  "fallback */ RecordExpressionInt(3, 23)"),
					*Selector,
					*Guarded);
			}
		}
		else if (IsSourceShape(SourceShapeCase, "parenthesized"))
		{
			if (FormCase.bLogicalAnd)
			{
				Expression = FString::Printf(TEXT("((%s) && (%s)) ? 1 : 0"), *Selector, *Guarded);
			}
			else if (FormCase.bLogicalOr)
			{
				Expression = FString::Printf(TEXT("((%s) || (%s)) ? 1 : 0"), *Selector, *Guarded);
			}
			else
			{
				Expression = FString::Printf(
					TEXT("(%s) ? (%s) : (RecordExpressionInt(3, 23))"), *Selector, *Guarded);
			}
		}
		return Expression;
	}

	static void AppendExceptionHelpers(
		FString& Source, const FFormCase& FormCase, const FOperandOutcomeCase& OperandOutcomeCase)
	{
		if (!OperandOutcomeCase.bException)
		{
			return;
		}

		if (FormCase.bLogicalAnd || FormCase.bLogicalOr)
		{
			AppendGeneratedAsLine(Source, TEXT("bool RaiseGuardedBool()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tRecordExpressionBool(2, true);"));
			AppendGeneratedAsLine(Source, TEXT("\tint Zero = 0;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 1 / Zero > 0;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("int RaiseGuardedInt()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tRecordExpressionInt(2, 41);"));
			AppendGeneratedAsLine(Source, TEXT("\tint Zero = 0;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 1 / Zero;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		AppendGeneratedAsLine(Source);
	}

	static void AppendMultilineReturn(FString& Source,
		const FFormCase& FormCase,
		const FOperandOutcomeCase& OperandOutcomeCase,
		const FSelectorCase& SelectorCase)
	{
		const FString Selector = MakeSelectorExpression(SelectorCase);
		const FString Guarded = MakeGuardedExpression(FormCase, OperandOutcomeCase);
		AppendGeneratedAsLine(Source, TEXT("\treturn ("));
		if (FormCase.bLogicalAnd)
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t\t%s"), *Selector));
			AppendGeneratedAsLine(Source, TEXT("\t\t&&"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t\t%s"), *Guarded));
			AppendGeneratedAsLine(Source, TEXT("\t) ? 1 : 0;"));
		}
		else if (FormCase.bLogicalOr)
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t\t%s"), *Selector));
			AppendGeneratedAsLine(Source, TEXT("\t\t||"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t\t%s"), *Guarded));
			AppendGeneratedAsLine(Source, TEXT("\t) ? 1 : 0;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t\t%s"), *Selector));
			AppendGeneratedAsLine(Source, TEXT("\t\t?"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t\t%s"), *Guarded));
			AppendGeneratedAsLine(Source, TEXT("\t\t:"));
			AppendGeneratedAsLine(Source, TEXT("\t\tRecordExpressionInt(3, 23)"));
			AppendGeneratedAsLine(Source, TEXT("\t);"));
		}
	}

	static FString BuildLazyEvaluationSource(const FFormCase& FormCase,
		const FOperandOutcomeCase& OperandOutcomeCase,
		const FSelectorCase& SelectorCase,
		const FSourceShapeCase& SourceShapeCase,
		const FString& Suffix)
	{
		FString Source;
		AppendExceptionHelpers(Source, FormCase, OperandOutcomeCase);
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("int Run_%s()"), *Suffix));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue ScopeValue(77);"));
		AppendGeneratedAsLine(Source);
		if (IsSourceShape(SourceShapeCase, "multiline"))
		{
			AppendMultilineReturn(Source, FormCase, OperandOutcomeCase, SelectorCase);
		}
		else
		{
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("\treturn %s;"),
					*MakeSingleLineExpression(
						FormCase, OperandOutcomeCase, SelectorCase, SourceShapeCase)));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int CleanAfterLazyExpression()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 89;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static int32 ExpectedResult(const FFormCase& FormCase, const FSelectorCase& SelectorCase)
	{
		if (FormCase.bLogicalAnd)
		{
			return SelectorCase.Value ? 1 : 0;
		}
		if (FormCase.bLogicalOr)
		{
			return 1;
		}
		return SelectorCase.Value ? 41 : 23;
	}

	static TArray<int32> ExpectedMarkers(const FFormCase& FormCase,
		const FOperandOutcomeCase& OperandOutcomeCase,
		const FSelectorCase& SelectorCase)
	{
		TArray<int32> Markers = {1};
		const bool bGuardedSelected = IsGuardedOperandSelected(FormCase, SelectorCase);
		if (bGuardedSelected && (OperandOutcomeCase.bSideEffect || OperandOutcomeCase.bException))
		{
			Markers.Add(2);
		}
		if (!FormCase.bLogicalAnd && !FormCase.bLogicalOr && !SelectorCase.Value)
		{
			Markers.Add(3);
		}
		return Markers;
	}


	void VerifyMarkers(
		const FNativeCaseContext& Case, const TArray<int32>& Expected, const TArray<int32>& Actual)
	{
		ASSERT_THAT(AreEqual(Expected.Num(),
			Actual.Num(),
			*Case.Describe(TEXT("lazy expression should execute exactly the expected number of "
								"observable operands"))));
		for (int32 Index = 0; Index < FMath::Min(Expected.Num(), Actual.Num()); ++Index)
		{
			ASSERT_THAT(AreEqual(Expected[Index],
				Actual[Index],
				*Case.Describe(
					TEXT("lazy expression should preserve exact operand evaluation order"))));
		}
	}

public:
	TEST_METHOD(LazyFormsBySelectorOutcomeAndShape)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-EXPR-LAZY-EVALUATION",
			ENativeEvidence::Compile | ENativeEvidence::Runtime | ENativeEvidence::Diagnostic |
				ENativeEvidence::Lifecycle | ENativeEvidence::Debug | ENativeEvidence::Cleanup);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine, TEXT("Lazy expression product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FExpressionEvaluationRecorder EvaluationRecorder;
		FNativeLifecycleRecorder Lifecycle;
		EvaluationRecorder.Reset();
		Lifecycle.Reset();
		ASSERT_THAT(IsTrue(RegisterExpressionEvaluationFunctions(*ScriptEngine, EvaluationRecorder),
			TEXT("Lazy expression product should register its observable native operands")));
		ASSERT_THAT(IsTrue(RegisterNativeCaseValue(*ScriptEngine, Lifecycle),
			TEXT("Lazy expression product should register its tracked scope value")));

		for (const FFormCase& FormCase : FormCases)
		{
			for (const FOperandOutcomeCase& OperandOutcomeCase : OperandOutcomeCases)
			{
				for (const FSelectorCase& SelectorCase : SelectorCases)
				{
					for (const FSourceShapeCase& SourceShapeCase : SourceShapeCases)
					{
						EvaluationRecorder.Reset();
						Lifecycle.Reset();
						const FNativeCaseContext Case(MakeNativeCaseId("LANG-EXPR-LAZY-EVALUATION",
							{
								ANSI_TO_TCHAR(FormCase.CatalogName),
								ANSI_TO_TCHAR(OperandOutcomeCase.CatalogName),
								ANSI_TO_TCHAR(SelectorCase.CatalogName),
								ANSI_TO_TCHAR(SourceShapeCase.CatalogName),
							}));
						const FString Suffix =
							MakeSuffix(FormCase, OperandOutcomeCase, SelectorCase, SourceShapeCase);
						const FString ModuleName = TEXT("ExpressionLazy_") + Suffix;
						const FString Source = BuildLazyEvaluationSource(
							FormCase, OperandOutcomeCase, SelectorCase, SourceShapeCase, Suffix);
						PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);
						const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
						const FTCHARToUTF8 SourceUtf8(*Source);
						Engine.ResetMessages();
						asIScriptModule* Module = nullptr;
						const int BuildResult = CompileNativeModule(
							ScriptEngine, ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
						ASSERT_THAT(IsTrue(BuildResult >= 0,
							*Case.Describe(TEXT("lazy expression cell should compile"))));
						ASSERT_THAT(IsNotNull(Module,
							*Case.Describe(TEXT("lazy expression cell should publish a module"))));
						if (BuildResult >= 0 && Module != nullptr)
						{
							const FString EntryDeclaration =
								FString::Printf(TEXT("int Run_%s()"), *Suffix);
							asIScriptFunction* const Entry = GetNativeFunctionByExactDecl(
								Module, TCHAR_TO_ANSI(*EntryDeclaration));
							asIScriptFunction* const Clean = GetNativeFunctionByExactDecl(
								Module, "int CleanAfterLazyExpression()");
							ASSERT_THAT(IsNotNull(Entry,
								*Case.Describe(TEXT(
									"lazy expression entry should resolve by exact declaration"))));
							ASSERT_THAT(IsNotNull(Clean,
								*Case.Describe(TEXT("lazy expression cell should expose its "
													"context-reuse probe"))));
							if (Entry != nullptr && Clean != nullptr)
							{
								asIScriptContext* const Context = ScriptEngine->CreateContext();
								ASSERT_THAT(IsNotNull(Context,
									*Case.Describe(TEXT("lazy expression cell should create an "
														"execution context"))));
								if (Context != nullptr)
								{
									const bool bExpectedException =
										OperandOutcomeCase.bException &&
										IsGuardedOperandSelected(FormCase, SelectorCase);
									const int ExecuteResult = PrepareAndExecute(Context, Entry);
									if (bExpectedException)
									{
										ASSERT_THAT(
											AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION),
												ExecuteResult,
												*Case.Describe(
													TEXT("selected throwing operand should stop "
														 "with an execution exception"))));
										asIScriptFunction* const ExceptionFunction =
											Context->GetExceptionFunction();
										ASSERT_THAT(IsNotNull(ExceptionFunction,
											*Case.Describe(TEXT("selected throwing operand should "
																"expose its helper frame"))));
										if (ExceptionFunction != nullptr)
										{
											ASSERT_THAT(IsTrue(
												FString(UTF8_TO_TCHAR(ExceptionFunction->GetName()))
													.StartsWith(TEXT("RaiseGuarded")),
												*Case.Describe(
													TEXT("exception metadata should identify the "
														 "selected guarded helper"))));
										}
										const char* ExceptionSection = nullptr;
										int ExceptionColumn = INDEX_NONE;
										ASSERT_THAT(IsTrue(
											Context->GetExceptionLineNumber(
												&ExceptionColumn, &ExceptionSection) > 0,
											*Case.Describe(TEXT("selected throwing operand should "
																"expose a source line"))));
										ASSERT_THAT(AreEqual(ModuleName,
											FString(UTF8_TO_TCHAR(ExceptionSection != nullptr
																	  ? ExceptionSection
																	  : "")),
											*Case.Describe(TEXT("throwing operand should retain "
																"its generated module section"))));
										ASSERT_THAT(IsTrue(ExceptionColumn > 0,
											*Case.Describe(TEXT("selected throwing operand should "
																"expose a source column"))));
										ASSERT_THAT(IsTrue(Context->GetCallstackSize() >= 2,
											*Case.Describe(TEXT("throwing helper should retain "
																"helper and entry frames"))));
									}
									else
									{
										ASSERT_THAT(AreEqual(
											static_cast<int32>(asEXECUTION_FINISHED),
											ExecuteResult,
											*Case.Describe(TEXT("non-throwing or skipped throwing "
																"operand should finish"))));
										if (ExecuteResult == asEXECUTION_FINISHED)
										{
											ASSERT_THAT(
												AreEqual(ExpectedResult(FormCase, SelectorCase),
													static_cast<int32>(Context->GetReturnDWord()),
													*Case.Describe(TEXT(
														"lazy expression should return the "
														"independently expected branch result"))));
										}
									}

									VerifyMarkers(Case,
										ExpectedMarkers(FormCase, OperandOutcomeCase, SelectorCase),
										EvaluationRecorder.Markers);
									ASSERT_THAT(AreEqual(asSUCCESS,
										Context->Unprepare(),
										*Case.Describe(
											TEXT("lazy expression context should unprepare after "
												 "finish or exception"))));
									ASSERT_THAT(AreEqual(0,
										Lifecycle.GetLiveObjectCount(),
										*Case.Describe(TEXT("lazy expression cleanup should "
															"release its tracked scope value"))));
									ASSERT_THAT(AreEqual(1,
										Lifecycle.Num(ENativeLifecycleEvent::ValueConstruct),
										*Case.Describe(TEXT("lazy expression entry should "
															"construct one tracked scope value"))));
									ASSERT_THAT(AreEqual(1,
										Lifecycle.Num(ENativeLifecycleEvent::Destruct),
										*Case.Describe(TEXT("lazy expression entry should destroy "
															"its tracked scope value once"))));

									ASSERT_THAT(AreEqual(asSUCCESS,
										Context->Prepare(Clean),
										*Case.Describe(TEXT("lazy expression context should "
															"prepare a clean follow-up"))));
									ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
										Context->Execute(),
										*Case.Describe(TEXT("lazy expression context should "
															"execute cleanly after prior state"))));
									ASSERT_THAT(AreEqual(89,
										static_cast<int32>(Context->GetReturnDWord()),
										*Case.Describe(TEXT("lazy expression context reuse should "
															"not retain stale result state"))));
									Context->Release();
								}
							}
						}

						ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
						ASSERT_THAT(IsNull(
							ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
							*Case.Describe(
								TEXT("lazy expression cell should discard its isolated module"))));
					}
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
