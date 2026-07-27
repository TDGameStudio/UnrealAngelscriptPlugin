#include "AngelscriptTestMacros.h"
#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_builder.h"
#include "source/as_module.h"
#include "source/as_parser.h"
#include "source/as_scriptcode.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptnode.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FParserSourceRecoveryDepthTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.ParserCartesianDepth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(SourcePositionsAndParserRecoveryRemainStable)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("FRONTEND-SOURCE-POSITIONS-RECOVERY",
			ENativeEvidence::Compile | ENativeEvidence::Metadata | ENativeEvidence::Diagnostic | ENativeEvidence::Cleanup);

		FString Source;
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("// skipped"));
		AppendGeneratedAsLine(Source, TEXT("int Value ="));
		AppendGeneratedAsLine(Source, TEXT("\t1 +"));
		AppendGeneratedAsLine(Source, TEXT("\t2;"));
		const FString LineFeed = FString::Chr(10);
		const FString CarriageReturnLineFeed = FString::Chr(13) + LineFeed;
		Source.ReplaceInline(*LineFeed, *CarriageReturnLineFeed);
		const FString SourceId = TEXT("FRONTEND-SOURCE-POSITIONS-RECOVERY");
		const FString ModuleName = TEXT("ParserSourcePositionsRecovery");
		PrintGeneratedAsSource(*TestRunner, SourceId, ModuleName, Source);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Source-position products should use the case-owned raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		asCModule* Module = CreateSdkModule(ScriptEngine, TCHAR_TO_UTF8(*ModuleName));
		ASSERT_THAT(IsNotNull(Module, TEXT("Source-position source should create a parser module")));
		if (Module == nullptr)
		{
			return;
		}
		{
			asCBuilder Builder(ScriptEngine, Module);
			const FTCHARToUTF8 SourceUtf8(*Source);
			asCScriptCode Code;
			Code.SetCode(TCHAR_TO_UTF8(*ModuleName), SourceUtf8.Get(), true);
			FParserAccessor Parser(&Builder);
			const int ParseResult = Parser.ParseScript(&Code);
			ASSERT_THAT(AreEqual(0, ParseResult, TEXT("CRLF source should parse successfully")));
			asCScriptNode* Root = Parser.GetScriptNode();
			ASSERT_THAT(IsNotNull(Root, TEXT("CRLF source should expose a root")));
			if (Root != nullptr)
			{
				const asCScriptNode* Declaration = FindFirstNodeOfType(Root, snDeclaration);
				ASSERT_THAT(IsNotNull(Declaration, TEXT("CRLF source should expose its declaration node")));
				if (Declaration != nullptr)
				{
					int Row = 0;
					int Column = 0;
					Code.ConvertPosToRowCol(Declaration->tokenPos, &Row, &Column);
					ASSERT_THAT(AreEqual(3, Row, TEXT("Declaration should remain on source line three after CRLF/comment input")));
					ASSERT_THAT(AreEqual(1, Column, TEXT("Declaration should start at column one")));
					ASSERT_THAT(IsTrue(Declaration->tokenLength > 6, TEXT("Declaration range should cover the multiline initializer")));
				}
			}
		}
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->DiscardModule(ModuleNameUtf8.Get()),
			TEXT("Source-position parser module should discard after its parser tree is released")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
			TEXT("Source-position parser module should leave no name-visible state")));

		const FString InvalidModuleName = TEXT("ParserRecoveryAfterInvalidSource");
		asCModule* InvalidModule = CreateSdkModule(ScriptEngine, TCHAR_TO_UTF8(*InvalidModuleName));
		ASSERT_THAT(IsNotNull(InvalidModule, TEXT("Recovery source should create a parser module")));
		if (InvalidModule == nullptr)
		{
			return;
		}
		{
			asCBuilder RecoveryBuilder(ScriptEngine, InvalidModule);
			RecoveryBuilder.silent = true;
			FParserAccessor RecoveryParser(&RecoveryBuilder);
			asCScriptCode InvalidCode;
			InvalidCode.SetCode("ParserRecoveryInvalid", "void Broken( { return; }", true);
			const int InvalidResult = RecoveryParser.ParseScriptSnippetWithoutImplicitReset(&InvalidCode);
			ASSERT_THAT(IsTrue(InvalidResult < 0, TEXT("Recovery probe should reject malformed source")));
			RecoveryParser.ResetParser();
			asCScriptCode ValidCode;
			ValidCode.SetCode("ParserRecoveryValid", "int Recovered = 7;", true);
			const int ValidResult = RecoveryParser.ParseScriptSnippetWithoutImplicitReset(&ValidCode);
			ASSERT_THAT(AreEqual(0, ValidResult, TEXT("Recovery probe should parse valid source after reset")));
			ASSERT_THAT(IsNotNull(RecoveryParser.GetScriptNode(), TEXT("Recovery probe should publish a root after reset")));
		}
		const FTCHARToUTF8 InvalidModuleNameUtf8(*InvalidModuleName);
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->DiscardModule(InvalidModuleNameUtf8.Get()),
			TEXT("Recovery parser module should discard after malformed-to-valid reuse")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(InvalidModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
			TEXT("Recovery parser module should leave no name-visible state")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
