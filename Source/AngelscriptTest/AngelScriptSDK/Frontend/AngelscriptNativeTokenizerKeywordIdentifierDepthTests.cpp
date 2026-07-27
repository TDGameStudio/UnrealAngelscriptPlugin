#include "../Support/AngelscriptNativeTokenizerTestSupport.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTokenizerKeywordIdentifierDepthTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.Tokenizer.DeepCoverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(TokenTaxonomyCoversActiveKeywordFamilies)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("FRONTEND-TOKEN-TAXONOMY-ACTIVE-KEYWORDS",
			ENativeEvidence::Metadata
			| ENativeEvidence::Diagnostic);

		// The source is intentionally emitted as a reviewable fixture even though
		// the assertion below calls the raw tokenizer directly for each token.
		FString GeneratedSource;
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(GeneratedSource, TEXT("int GeneratedKeywordCorpus()"));
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(GeneratedSource, TEXT("{"));
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(GeneratedSource, TEXT("\tint Result = 0;"));

		const FTokenCase Cases[] =
		{
			{ "access", ttAccess, 6, TEXT("access keyword") },
			{ "auto", ttAuto, 4, TEXT("auto keyword") },
			{ "bool", ttBool, 4, TEXT("bool keyword") },
			{ "break", ttBreak, 5, TEXT("break keyword") },
			{ "case", ttCase, 4, TEXT("case keyword") },
			{ "Cast", ttCast, 4, TEXT("Cast keyword") },
			{ "class", ttClass, 5, TEXT("class keyword") },
			{ "const", ttConst, 5, TEXT("const keyword") },
			{ "continue", ttContinue, 8, TEXT("continue keyword") },
			{ "default", ttDefault, 7, TEXT("default keyword") },
			{ "do", ttDo, 2, TEXT("do keyword") },
			{ "double", ttDouble, 6, TEXT("double keyword") },
			{ "else", ttElse, 4, TEXT("else keyword") },
			{ "false", ttFalse, 5, TEXT("false keyword") },
			{ "fallthrough", ttFallthrough, 11, TEXT("fallthrough keyword") },
			{ "float", ttFloat, 5, TEXT("float keyword") },
			{ "float32", ttFloat32, 7, TEXT("float32 keyword") },
			{ "float64", ttFloat64, 7, TEXT("float64 keyword") },
			{ "for", ttFor, 3, TEXT("for keyword") },
			{ "foreach", ttForeach, 7, TEXT("foreach keyword") },
			{ "funcdef", ttIdentifier, 7, TEXT("funcdef is a fork-rejected keyword spelling"), true, true },
			{ "if", ttIf, 2, TEXT("if keyword") },
			{ "import", ttImport, 6, TEXT("import keyword") },
			{ "in", ttIn, 2, TEXT("in keyword") },
			{ "inout", ttInOut, 5, TEXT("inout keyword") },
			{ "interface", ttIdentifier, 9, TEXT("interface is a fork-rejected keyword spelling"), true, true },
			{ "is", ttIdentifier, 2, TEXT("is is a fork-rejected keyword spelling"), true, true },
			{ "int", ttInt, 3, TEXT("int keyword") },
			{ "int8", ttInt8, 4, TEXT("int8 keyword") },
			{ "int16", ttInt16, 5, TEXT("int16 keyword") },
			{ "int32", ttInt, 5, TEXT("int32 compatibility spelling") },
			{ "int64", ttInt64, 5, TEXT("int64 keyword") },
			{ "local", ttLocal, 5, TEXT("local keyword") },
			{ "mixin", ttMixin, 5, TEXT("mixin keyword") },
			{ "namespace", ttNamespace, 9, TEXT("namespace keyword") },
			{ "null", ttIdentifier, 4, TEXT("null is a fork-rejected keyword spelling"), true, true },
			{ "nullptr", ttNull, 7, TEXT("nullptr keyword") },
			{ "not", ttIdentifier, 3, TEXT("not is a fork-rejected keyword spelling"), true, true },
			{ "and", ttIdentifier, 3, TEXT("and is a fork-rejected keyword spelling"), true, true },
			{ "or", ttIdentifier, 2, TEXT("or is a fork-rejected keyword spelling"), true, true },
			{ "xor", ttIdentifier, 3, TEXT("xor is a fork-rejected keyword spelling"), true, true },
			{ "!is", ttNot, 1, TEXT("!is is a fork-rejected operator spelling and keeps the ! token"), false, true },
			{ "out", ttOut, 3, TEXT("out keyword") },
			{ "private", ttPrivate, 7, TEXT("private keyword") },
			{ "protected", ttProtected, 9, TEXT("protected keyword") },
			{ "return", ttReturn, 6, TEXT("return keyword") },
			{ "struct", ttStruct, 6, TEXT("struct keyword") },
			{ "switch", ttSwitch, 6, TEXT("switch keyword") },
			{ "typedef", ttIdentifier, 7, TEXT("typedef is a fork-rejected keyword spelling"), true, true },
			{ "true", ttTrue, 4, TEXT("true keyword") },
			{ "uint", ttUInt, 4, TEXT("uint keyword") },
			{ "uint8", ttUInt8, 5, TEXT("uint8 keyword") },
			{ "uint16", ttUInt16, 6, TEXT("uint16 keyword") },
			{ "uint32", ttUInt, 6, TEXT("uint32 compatibility spelling") },
			{ "uint64", ttUInt64, 6, TEXT("uint64 keyword") },
			{ "unresolved_object", ttUnresolvedObject, 17, TEXT("unresolved_object keyword") },
			{ "enum", ttEnum, 4, TEXT("enum keyword") },
			{ "void", ttVoid, 4, TEXT("void keyword") },
			{ "while", ttWhile, 5, TEXT("while keyword") },
		};

		for (const FTokenCase& Case : Cases)
		{
			AppendCommentedCase(GeneratedSource,
				Case.bForkRejected ? TEXT("fork-rejected keyword spelling") : TEXT("active keyword"),
				Case.Input);
			const FTokenObservation Observation = ReadToken(Case.Input);
			ASSERT_THAT(AreEqual(static_cast<int32>(Case.ExpectedType), static_cast<int32>(Observation.Type),
				FString::Printf(TEXT("%s should retain its expected fork token kind"), Case.Description)));
			ASSERT_THAT(AreEqual(Case.ExpectedLength, Observation.Length,
				FString::Printf(TEXT("%s should consume exactly its spelling"), Case.Description)));

			if (Case.bIdentifierBoundary)
			{
				const char* SuffixInput = "_suffix";
				std::string KeywordWithSuffix(Case.Input);
				KeywordWithSuffix += SuffixInput;
				const FTokenObservation SuffixObservation = ReadToken(KeywordWithSuffix.c_str());
				ASSERT_THAT(AreEqual(static_cast<int32>(ttIdentifier), static_cast<int32>(SuffixObservation.Type),
					FString::Printf(TEXT("%s followed by identifier characters must not split into a keyword"), Case.Description)));
				ASSERT_THAT(AreEqual(static_cast<int32>(KeywordWithSuffix.size()), SuffixObservation.Length,
					FString::Printf(TEXT("%s suffix identifier should be consumed as one token"), Case.Description)));
			}
		}

		// `!is` is intentionally two tokens in this fork. Exercise the suffix
		// boundary as a two-token recovery path instead of pretending it is one
		// keyword token.
		const char NotIsWithSuffix[] = "!is_suffix";
		AppendCommentedCase(GeneratedSource, TEXT("fork-rejected !is suffix input"), NotIsWithSuffix);
		const FTokenObservation NotIsObservation = ReadToken(NotIsWithSuffix);
		ASSERT_THAT(AreEqual(static_cast<int32>(ttNot), static_cast<int32>(NotIsObservation.Type),
			TEXT("!is suffix input should retain the leading ! token")));
		ASSERT_THAT(AreEqual(1, NotIsObservation.Length,
			TEXT("!is suffix input should consume only the leading ! token")));
		const FTokenObservation NotIsSuffixObservation = ReadToken(NotIsWithSuffix + NotIsObservation.Length);
		ASSERT_THAT(AreEqual(static_cast<int32>(ttIdentifier), static_cast<int32>(NotIsSuffixObservation.Type),
			TEXT("!is suffix input should expose is_suffix as the recovery identifier")));
		ASSERT_THAT(AreEqual(9, NotIsSuffixObservation.Length,
			TEXT("!is suffix recovery identifier should consume its complete spelling")));

		AngelscriptNativeTestSupport::AppendGeneratedAsLine(GeneratedSource, TEXT("\treturn Result;"));
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(GeneratedSource, TEXT("}"));
		AngelscriptNativeTestSupport::PrintGeneratedAsSource(
			*TestRunner,
			TEXT("FRONTEND-TOKEN-TAXONOMY-ACTIVE-KEYWORDS"),
			TEXT("AS_SDK_FrontendTokenizerDeep_Keywords"),
			GeneratedSource);
	}

	TEST_METHOD(ContextualWordsRemainIdentifierTokens)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("FRONTEND-TOKEN-CONTEXTUAL-IDENTIFIER-WORDS",
			ENativeEvidence::Metadata
			| ENativeEvidence::Diagnostic);

		const FTokenCase ContextualWords[] =
		{
			{ "this", ttIdentifier, 4, TEXT("this is contextual parser spelling") },
			{ "from", ttIdentifier, 4, TEXT("from is contextual parser spelling") },
			{ "super", ttIdentifier, 5, TEXT("super is contextual parser spelling") },
		};

		FString GeneratedSource;
		AppendGeneratedAsLine(GeneratedSource, TEXT("int GeneratedContextualWordCell()"));
		AppendGeneratedAsLine(GeneratedSource, TEXT("{"));
		AppendGeneratedAsLine(GeneratedSource, TEXT("\treturn 0;"));
		int32 ObservedCaseCount = 0;

		for (const FTokenCase& Case : ContextualWords)
		{
			for (const char* Suffix : { "", "_suffix" })
			{
				std::string Input(Case.Input);
				Input += Suffix;
				const FString CaseId = MakeNativeCaseId(
					"FRONTEND-TOKEN-CONTEXTUAL-IDENTIFIER-WORDS",
					{ ANSI_TO_TCHAR(Case.Input), ANSI_TO_TCHAR(*Suffix == '\0' ? "bare" : "identifier_suffix") });
				AppendCommentedCase(GeneratedSource, TEXT("contextual identifier input"), Input.c_str());
				AppendGeneratedAsLine(GeneratedSource,
					FString::Printf(TEXT("\t// CaseId: %s"), *CaseId));
				const FTokenObservation Observation = ReadToken(Input.c_str());
				ASSERT_THAT(AreEqual(static_cast<int32>(ttIdentifier), static_cast<int32>(Observation.Type),
					FString::Printf(TEXT("%s should remain an identifier in %s"), Case.Description, *CaseId)));
				ASSERT_THAT(AreEqual(static_cast<int32>(Input.size()), Observation.Length,
					FString::Printf(TEXT("%s should consume the contextual spelling in %s"), Case.Description, *CaseId)));
				++ObservedCaseCount;
			}
		}

		ASSERT_THAT(AreEqual(6, ObservedCaseCount,
			TEXT("Contextual parser words × boundary should execute every cell")));
		AppendGeneratedAsLine(GeneratedSource, TEXT("}"));
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("FRONTEND-TOKEN-CONTEXTUAL-IDENTIFIER-WORDS"),
			TEXT("AS_SDK_FrontendTokenizerDeep_ContextualWords"),
			GeneratedSource);
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
