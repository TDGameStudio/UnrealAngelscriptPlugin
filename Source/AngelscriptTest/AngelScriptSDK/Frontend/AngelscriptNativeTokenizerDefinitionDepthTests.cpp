#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_tokendef.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTokenizerDefinitionDepthTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.Tokenizer.DeepCoverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(TokenDefinitionsRemainAvailableForPublishedKinds)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("FRONTEND-TOKEN-DEFINITIONS",
			ENativeEvidence::Metadata
			| ENativeEvidence::Diagnostic);

		FString GeneratedSource;
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(GeneratedSource, TEXT("int GeneratedTokenDefinitionCorpus()"));
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(GeneratedSource, TEXT("{"));
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(GeneratedSource, TEXT("\treturn 20;"));
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(GeneratedSource, TEXT("}"));
		const eTokenType TokenKinds[] =
		{
			ttIdentifier,
			ttIntConstant,
			ttFloat32Constant,
			ttFloat64Constant,
			ttStringConstant,
			ttMultilineStringConstant,
			ttHeredocStringConstant,
			ttNonTerminatedStringConstant,
			ttBitsConstant,
			ttPlus,
			ttScope,
			ttShiftRightAAssign,
			ttStartStatementBlock,
			ttCloseBracket,
			ttQuestion,
			ttColon,
			ttClass,
			ttFloat64,
			ttReturn,
			ttUnresolvedObject,
		};

		for (const eTokenType TokenKind : TokenKinds)
		{
			const char* Definition = asCTokenizer::GetDefinition(static_cast<int>(TokenKind));
			AngelscriptNativeTestSupport::AppendGeneratedAsLine(
				GeneratedSource,
				FString::Printf(
					TEXT("\t// TokenKind=%d Definition=%hs"),
					static_cast<int32>(TokenKind),
					Definition != nullptr ? Definition : "<null>"));
			ASSERT_THAT(IsTrue(Definition != nullptr && Definition[0] != '\0',
				FString::Printf(TEXT("Published token kind %d should have a diagnostic definition"), static_cast<int32>(TokenKind))));
		}

		AngelscriptNativeTestSupport::PrintGeneratedAsSource(
			*TestRunner,
			TEXT("FRONTEND-TOKEN-DEFINITIONS"),
			TEXT("AS_SDK_FrontendTokenizerDeep_Definitions"),
			GeneratedSource);
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
