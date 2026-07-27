#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeCaseTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_module.h"
#include "source/as_parser.h"
#include "source/as_scriptcode.h"
#include "source/as_scriptengine.h"
#include "source/as_tokendef.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptParserInternalPrivate
{
	struct FParserAccessor : asCParser
	{
		explicit FParserAccessor(asCBuilder* InBuilder)
			: asCParser(InBuilder)
		{
		}

		using asCParser::IdentifierIs;
		using asCParser::IsAssignOperator;
		using asCParser::IsConstant;
		using asCParser::IsDataType;
		using asCParser::IsOperator;
		using asCParser::IsPostOperator;
		using asCParser::IsPreOperator;
		using asCParser::IsRealType;
	};

	inline FString MakeReviewSource(const FString& CaseId, const FString& PredicateName, const FString& TokenName)
	{
		FString Source;
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(Source, TEXT("void ParserInternalCase()"));
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(Source, TEXT("{"));
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(
			Source,
			FString::Printf(TEXT("\t// case: %s"), *CaseId));
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(
			Source,
			FString::Printf(TEXT("\t// predicate: %s, token: %s"), *PredicateName, *TokenName));
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	inline bool IsRealTypeToken(const eTokenType Token)
	{
		switch (Token)
		{
		case ttVoid:
		case ttInt:
		case ttFloat32:
		case ttFloat64:
		case ttBool:
			return true;
		default:
			return false;
		}
	}

	inline bool IsConstantToken(const eTokenType Token)
	{
		switch (Token)
		{
		case ttIntConstant:
		case ttFloat32Constant:
		case ttFloat64Constant:
		case ttStringConstant:
		case ttBitsConstant:
		case ttTrue:
		case ttFalse:
			return true;
		default:
			return false;
		}
	}

	inline bool IsOperatorToken(const eTokenType Token)
	{
		switch (Token)
		{
		case ttPlus:
		case ttMinus:
		case ttStar:
		case ttSlash:
		case ttPercent:
		case ttStarStar:
		case ttAnd:
		case ttOr:
		case ttXor:
		case ttEqual:
		case ttNotEqual:
		case ttLessThan:
		case ttLessThanOrEqual:
		case ttGreaterThan:
		case ttGreaterThanOrEqual:
		case ttAmp:
		case ttBitOr:
		case ttBitXor:
		case ttBitShiftLeft:
		case ttBitShiftRight:
		case ttBitShiftRightArith:
		case ttIs:
		case ttNotIs:
			return true;
		default:
			return false;
		}
	}

	inline bool IsPreOperatorToken(const eTokenType Token)
	{
		return Token == ttPlus || Token == ttMinus || Token == ttInc || Token == ttDec;
	}

	inline bool IsPostOperatorToken(const eTokenType Token)
	{
		return Token == ttInc
			|| Token == ttDec
			|| Token == ttDot
			|| Token == ttOpenBracket
			|| Token == ttOpenParanthesis;
	}

	inline bool IsAssignOperatorToken(const eTokenType Token)
	{
		return Token == ttAssignment || Token == ttAddAssign;
	}
}

TEST_CLASS_WITH_FLAGS(FParserInternalTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.ParserInternal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(TokenPredicatesByPredicateAndToken)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("FRONTEND-PARSER-INTERNAL-PREDICATES",
			ENativeEvidence::Metadata
			| ENativeEvidence::Isolation);

		enum class EPredicate : uint8
		{
			RealType,
			DataType,
			Constant,
			Operator,
			PreOperator,
			PostOperator,
			AssignOperator,
		};

		struct FPredicateCase
		{
			const TCHAR* Id;
			const TCHAR* DisplayName;
			EPredicate Predicate;
		};
		const FPredicateCase Predicates[] =
		{
			{ TEXT("real_type"), TEXT("IsRealType"), EPredicate::RealType },
			{ TEXT("data_type"), TEXT("IsDataType"), EPredicate::DataType },
			{ TEXT("constant"), TEXT("IsConstant"), EPredicate::Constant },
			{ TEXT("operator"), TEXT("IsOperator"), EPredicate::Operator },
			{ TEXT("pre_operator"), TEXT("IsPreOperator"), EPredicate::PreOperator },
			{ TEXT("post_operator"), TEXT("IsPostOperator"), EPredicate::PostOperator },
			{ TEXT("assign_operator"), TEXT("IsAssignOperator"), EPredicate::AssignOperator },
		};

		struct FTokenCase
		{
			const TCHAR* Id;
			const TCHAR* DisplayName;
			eTokenType Token;
		};
		const FTokenCase Tokens[] =
		{
			{ TEXT("void"), TEXT("ttVoid"), ttVoid },
			{ TEXT("int"), TEXT("ttInt"), ttInt },
			{ TEXT("float64"), TEXT("ttFloat64"), ttFloat64 },
			{ TEXT("identifier"), TEXT("ttIdentifier"), ttIdentifier },
			{ TEXT("int_constant"), TEXT("ttIntConstant"), ttIntConstant },
			{ TEXT("string_constant"), TEXT("ttStringConstant"), ttStringConstant },
			{ TEXT("plus"), TEXT("ttPlus"), ttPlus },
			{ TEXT("minus"), TEXT("ttMinus"), ttMinus },
			{ TEXT("inc"), TEXT("ttInc"), ttInc },
			{ TEXT("dec"), TEXT("ttDec"), ttDec },
			{ TEXT("assign"), TEXT("ttAssignment"), ttAssignment },
			{ TEXT("add_assign"), TEXT("ttAddAssign"), ttAddAssign },
			{ TEXT("statement_end"), TEXT("ttEndStatement"), ttEndStatement },
			{ TEXT("open_parenthesis"), TEXT("ttOpenParanthesis"), ttOpenParanthesis },
		};

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Parser internal predicates require a raw engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const char* const ModuleName = "ParserInternalPredicates";
		FScopedNativeModuleName ModuleScope(Engine, ModuleName);
		asCModule* const Module = static_cast<asCModule*>(ScriptEngine->GetModule(ModuleName, asGM_ALWAYS_CREATE));
		ASSERT_THAT(IsNotNull(Module, TEXT("Parser internal predicates should create a backing module")));
		if (Module == nullptr)
		{
			return;
		}

		asCBuilder Builder(static_cast<asCScriptEngine*>(ScriptEngine), Module);
		AngelscriptParserInternalPrivate::FParserAccessor Parser(&Builder);
		int32 ObservedCaseCount = 0;
		for (const FPredicateCase& Predicate : Predicates)
		{
			for (const FTokenCase& Token : Tokens)
			{
				const FString CaseId = MakeNativeCaseId(
					"FRONTEND-PARSER-INTERNAL-PREDICATES",
					{ Predicate.Id, Token.Id });
				PrintGeneratedAsSource(
					*TestRunner,
					CaseId,
					FString::Printf(TEXT("ParserPredicate_%s_%s"), Predicate.Id, Token.Id),
					AngelscriptParserInternalPrivate::MakeReviewSource(CaseId, Predicate.DisplayName, Token.DisplayName));

				sToken ScriptToken;
				ScriptToken.type = Token.Token;
				ScriptToken.pos = 0;
				ScriptToken.length = 0;
				bool bObserved = false;
				switch (Predicate.Predicate)
				{
				case EPredicate::RealType:
					bObserved = Parser.IsRealType(Token.Token);
					break;
				case EPredicate::DataType:
					bObserved = Parser.IsDataType(ScriptToken);
					break;
				case EPredicate::Constant:
					bObserved = Parser.IsConstant(Token.Token);
					break;
				case EPredicate::Operator:
					bObserved = Parser.IsOperator(Token.Token);
					break;
				case EPredicate::PreOperator:
					bObserved = Parser.IsPreOperator(Token.Token);
					break;
				case EPredicate::PostOperator:
					bObserved = Parser.IsPostOperator(Token.Token);
					break;
				case EPredicate::AssignOperator:
					bObserved = Parser.IsAssignOperator(Token.Token);
					break;
				}

				bool bExpected = false;
				switch (Predicate.Predicate)
				{
				case EPredicate::RealType:
					bExpected = AngelscriptParserInternalPrivate::IsRealTypeToken(Token.Token);
					break;
				case EPredicate::DataType:
					bExpected = AngelscriptParserInternalPrivate::IsRealTypeToken(Token.Token) || Token.Token == ttIdentifier;
					break;
				case EPredicate::Constant:
					bExpected = AngelscriptParserInternalPrivate::IsConstantToken(Token.Token);
					break;
				case EPredicate::Operator:
					bExpected = AngelscriptParserInternalPrivate::IsOperatorToken(Token.Token);
					break;
				case EPredicate::PreOperator:
					bExpected = AngelscriptParserInternalPrivate::IsPreOperatorToken(Token.Token);
					break;
				case EPredicate::PostOperator:
					bExpected = AngelscriptParserInternalPrivate::IsPostOperatorToken(Token.Token);
					break;
				case EPredicate::AssignOperator:
					bExpected = AngelscriptParserInternalPrivate::IsAssignOperatorToken(Token.Token);
					break;
				}

				ASSERT_THAT(AreEqual(bExpected, bObserved,
					FString::Printf(TEXT("%s should match the independent predicate oracle"), *CaseId)));
				++ObservedCaseCount;
			}
		}

		ASSERT_THAT(AreEqual(98, ObservedCaseCount,
			TEXT("The parser predicate product should execute every predicate/token pair")));
	}

	TEST_METHOD(IdentifierPredicateByCandidateAndProbe)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("FRONTEND-PARSER-INTERNAL-IDENTIFIER",
			ENativeEvidence::Compile
			| ENativeEvidence::Metadata
			| ENativeEvidence::Isolation);

		struct FCandidateCase
		{
			const TCHAR* Id;
			const char* Source;
			const char* Identifier;
			int32 Position;
			int32 Length;
			bool bTokenIsIdentifier;
		};
		const FCandidateCase Candidates[] =
		{
			{ TEXT("exact_identifier"), "int Value = 1;", "Value", 4, 5, true },
			{ TEXT("suffixed_identifier"), "int Value2 = 1;", "Value2", 4, 6, true },
			{ TEXT("keyword_token"), "int Value = 1;", "int", 0, 3, false },
		};
		const bool ProbeMatches[] = { true, false };

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Identifier predicate requires a raw engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const char* const ModuleName = "ParserInternalIdentifier";
		FScopedNativeModuleName ModuleScope(Engine, ModuleName);
		asCModule* const Module = static_cast<asCModule*>(ScriptEngine->GetModule(ModuleName, asGM_ALWAYS_CREATE));
		ASSERT_THAT(IsNotNull(Module, TEXT("Identifier predicate should create a backing module")));
		if (Module == nullptr)
		{
			return;
		}

		asCBuilder Builder(static_cast<asCScriptEngine*>(ScriptEngine), Module);
		AngelscriptParserInternalPrivate::FParserAccessor Parser(&Builder);
		asCScriptCode Code;
		int32 ObservedCaseCount = 0;
		for (const FCandidateCase& Candidate : Candidates)
		{
			for (const bool bProbeMatches : ProbeMatches)
			{
				const FString ProbeId = bProbeMatches ? TEXT("same") : TEXT("different");
				const FString CaseId = MakeNativeCaseId(
					"FRONTEND-PARSER-INTERNAL-IDENTIFIER",
					{ Candidate.Id, *ProbeId });
				const FString SourceText = UTF8_TO_TCHAR(Candidate.Source);
				PrintGeneratedAsSource(
					*TestRunner,
					CaseId,
					FString::Printf(TEXT("ParserIdentifier_%s_%s"), Candidate.Id, *ProbeId),
					SourceText);

				ASSERT_THAT(AreEqual(0, Code.SetCode("ParserInternalIdentifier", Candidate.Source, true),
					FString::Printf(TEXT("%s should initialize its source code"), *CaseId)));
				ASSERT_THAT(AreEqual(0, Parser.ParseScript(&Code),
					FString::Printf(TEXT("%s should parse its identifier source"), *CaseId)));

				sToken Token;
				Token.type = Candidate.bTokenIsIdentifier ? ttIdentifier : ttInt;
				Token.pos = static_cast<size_t>(Candidate.Position);
				Token.length = static_cast<size_t>(Candidate.Length);
				const char* const Probe = bProbeMatches ? Candidate.Identifier : "Other";
				const bool bExpected = Candidate.bTokenIsIdentifier && bProbeMatches;
				ASSERT_THAT(AreEqual(bProbeMatches, Code.TokenEquals(Candidate.Position, Candidate.Length, Probe),
					FString::Printf(TEXT("%s should compare the exact source span through TokenEquals"), *CaseId)));
				ASSERT_THAT(AreEqual(bExpected, Parser.IdentifierIs(Token, Probe),
					FString::Printf(TEXT("%s should honor token kind and exact source spelling"), *CaseId)));
				++ObservedCaseCount;
			}
		}

		ASSERT_THAT(AreEqual(6, ObservedCaseCount,
			TEXT("The identifier predicate product should execute every candidate/probe pair")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
