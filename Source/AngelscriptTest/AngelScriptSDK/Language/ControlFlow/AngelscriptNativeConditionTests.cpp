#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

using AngelscriptNativeTestSupport::AppendGeneratedAsLine;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FConditionTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.ControlFlow.Conditions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FStatementCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FConditionCase
	{
		const ANSICHAR* CatalogName;
		bool bInvalid;
	};

	inline static constexpr FStatementCase StatementCases[] =
	{
		{ "if" },
		{ "while" },
		{ "do_while" },
		{ "for" },
	};

	inline static constexpr FConditionCase ConditionCases[] =
	{
		{ "bool_literal", false },
		{ "variable", false },
		{ "comparison", false },
		{ "logical", false },
		{ "side_effect_call", false },
		{ "overloaded_conversion", false },
		{ "invalid_type", true },
	};


	static FString BuildConditionExpression(const FConditionCase& ConditionCase, const bool bTruth)
	{
		const TCHAR* const Literal = bTruth ? TEXT("true") : TEXT("false");
		if (FCStringAnsi::Strcmp(ConditionCase.CatalogName, "bool_literal") == 0)
		{
			return Literal;
		}
		if (FCStringAnsi::Strcmp(ConditionCase.CatalogName, "variable") == 0)
		{
			return TEXT("ConditionValue");
		}
		if (FCStringAnsi::Strcmp(ConditionCase.CatalogName, "comparison") == 0)
		{
			return bTruth ? TEXT("2 > 1") : TEXT("2 < 1");
		}
		if (FCStringAnsi::Strcmp(ConditionCase.CatalogName, "logical") == 0)
		{
			return bTruth ? TEXT("ConditionValue && true") : TEXT("ConditionValue && false");
		}
		if (FCStringAnsi::Strcmp(ConditionCase.CatalogName, "side_effect_call") == 0)
		{
			return TEXT("EvaluateCondition(ConditionValue, EvaluationCount)");
		}
		if (FCStringAnsi::Strcmp(ConditionCase.CatalogName, "overloaded_conversion") == 0)
		{
			return TEXT("FConditionValue(ConditionValue)");
		}
		return TEXT("FInvalidCondition()");
	}

	static FString BuildConditionSource(
		const FStatementCase& StatementCase,
		const FConditionCase& ConditionCase,
		const bool bTruth)
	{
		FString Source;
		AppendGeneratedAsLine(Source, TEXT("struct FConditionValue"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tbool Value;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFConditionValue(bool InValue)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tValue = InValue;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tbool opImplConv() const"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn Value;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("class FInvalidCondition"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("bool EvaluateCondition(bool InValue, int&inout EvaluationCount)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\t++EvaluationCount;"));
		AppendGeneratedAsLine(Source, TEXT("\treturn InValue;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int Entry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, bTruth ? TEXT("\tbool ConditionValue = true;") : TEXT("\tbool ConditionValue = false;"));
		AppendGeneratedAsLine(Source, TEXT("\tint Trace = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tint Guard = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tint EvaluationCount = 0;"));

		const FString Condition = BuildConditionExpression(ConditionCase, bTruth);
		if (FCStringAnsi::Strcmp(StatementCase.CatalogName, "if") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("\tif (") + Condition + TEXT(")"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tTrace = 1;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else if (FCStringAnsi::Strcmp(StatementCase.CatalogName, "while") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("\twhile ((") + Condition + TEXT(") && Guard < 1)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\t++Trace;"));
			AppendGeneratedAsLine(Source, TEXT("\t\t++Guard;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else if (FCStringAnsi::Strcmp(StatementCase.CatalogName, "do_while") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("\tdo"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\t++Trace;"));
			AppendGeneratedAsLine(Source, TEXT("\t\t++Guard;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\twhile ((") + Condition + TEXT(") && Guard < 1);"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\tfor (; (") + Condition + TEXT(") && Guard < 1; ++Guard)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\t++Trace;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}

		AppendGeneratedAsLine(Source, TEXT("\treturn Trace * 10 + EvaluationCount;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static int32 ExpectedResult(
		const FStatementCase& StatementCase,
		const FConditionCase& ConditionCase,
		const bool bTruth)
	{
		const int32 Trace = FCStringAnsi::Strcmp(StatementCase.CatalogName, "do_while") == 0 || bTruth
			? 1
			: 0;
		if (FCStringAnsi::Strcmp(ConditionCase.CatalogName, "side_effect_call") != 0)
		{
			return Trace * 10;
		}

		const bool bRepeatedCondition =
			FCStringAnsi::Strcmp(StatementCase.CatalogName, "while") == 0
			|| FCStringAnsi::Strcmp(StatementCase.CatalogName, "for") == 0;
		const int32 EvaluationCount = bTruth && bRepeatedCondition ? 2 : 1;
		return Trace * 10 + EvaluationCount;
	}

public:
	TEST_METHOD(StatementsByConditionAndTruth)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-CF-CONDITION",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Runtime);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Condition product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		for (const FStatementCase& StatementCase : StatementCases)
		{
			for (const FConditionCase& ConditionCase : ConditionCases)
			{
				for (const bool bTruth : { false, true })
				{
					const FNativeCaseContext Case(MakeNativeCaseId(
						"LANG-CF-CONDITION",
						{ ANSI_TO_TCHAR(StatementCase.CatalogName), ANSI_TO_TCHAR(ConditionCase.CatalogName), bTruth ? TEXT("true") : TEXT("false") }));
					const FString ModuleName = TEXT("Condition_") + Case.GetId().RightChop(18).Replace(TEXT("-"), TEXT("_"));
					const FString Source = BuildConditionSource(StatementCase, ConditionCase, bTruth);
					PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);
					const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
					const FTCHARToUTF8 SourceUtf8(*Source);
					Engine.ResetMessages();
					asIScriptModule* Module = nullptr;
					const int BuildResult = CompileNativeModule(ScriptEngine, ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
					if (ConditionCase.bInvalid)
					{
						ASSERT_THAT(IsTrue(BuildResult < 0, *Case.Describe(TEXT("invalid condition type should be rejected"))));
						const FString MessagesText = Engine.GetMessagesText();
						const bool bHasBooleanDiagnostic = MessagesText.Contains(TEXT("Expression must be of boolean type"))
							|| MessagesText.Contains(TEXT("No conversion from 'FInvalidCondition' to 'bool' available."));
						ASSERT_THAT(IsTrue(bHasBooleanDiagnostic,
							*FString::Printf(TEXT("%s invalid condition diagnostic should preserve the fork's boolean-type diagnostic. Messages={%s}"),
								*Case.GetId(), *MessagesText)));
					}
					else
					{
						ASSERT_THAT(AreEqual(asSUCCESS, BuildResult, *Case.Describe(TEXT("condition cell should compile"))));
						asIScriptFunction* const Entry = GetNativeFunctionByExactDecl(Module, "int Entry()");
						ASSERT_THAT(IsNotNull(Entry, *Case.Describe(TEXT("condition cell should publish exact Entry declaration"))));
						if (Entry != nullptr)
						{
							asIScriptContext* const Context = ScriptEngine->CreateContext();
							ASSERT_THAT(IsNotNull(Context, *Case.Describe(TEXT("condition cell should create a context"))));
							if (Context != nullptr)
							{
								ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, PrepareAndExecute(Context, Entry),
									*Case.Describe(TEXT("condition cell should finish"))));
								ASSERT_THAT(AreEqual(ExpectedResult(StatementCase, ConditionCase, bTruth), static_cast<int32>(Context->GetReturnDWord()),
									*Case.Describe(TEXT("condition should select the expected branch count"))));
								Context->Release();
							}
						}
					}
					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
						*Case.Describe(TEXT("condition cell should discard its module"))));
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
