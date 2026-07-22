#include "../Support/AngelscriptNativeCoreTestSupport.h"

// Parser error recovery coverage.
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_builder.h"
#include "source/as_module.h"
#include "source/as_parser.h"
#include "source/as_scriptcode.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptnode.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FParserErrorsTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.Parser.Errors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static int ParseScriptWithResult(asCScriptEngine* ScriptEngine, const char* ModuleName, const char* Source)
	{
		using namespace AngelscriptNativeTestSupport;

		asCModule* Module = CreateSdkModule(ScriptEngine, ModuleName);
		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;

		asCScriptCode Code;
		Code.SetCode(ModuleName, Source, true);

		AngelscriptNativeTestSupport::FParserAccessor Parser(&Builder);
		return Parser.ParseScript(&Code);
	}

public:
	TEST_METHOD(MissingSemicolonRecovers)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser error test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		const int ParseResult = ParseScriptWithResult(BareEngine, "ParserErrorMissingSemicolon", "int A = 1 int B = 2;");
		ASSERT_THAT(AreEqual(0, ParseResult,
			TEXT("Parser currently accepts adjacent declarations without an explicit semicolon error in this recovery path")));
	}

	TEST_METHOD(UnbalancedBracesError)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser error test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		const int ParseResult = ParseScriptWithResult(BareEngine, "ParserErrorUnbalancedBraces", "class FBroken { void Run() { }");
		ASSERT_THAT(IsTrue(ParseResult < 0, TEXT("Unbalanced braces should fail parser validation")));
	}

	TEST_METHOD(UnclosedStringInDeclaration)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser error test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		const int ParseResult = ParseScriptWithResult(BareEngine, "ParserErrorUnclosedString", "const string Name = \"unterminated;");
		ASSERT_THAT(IsTrue(ParseResult < 0, TEXT("Unclosed string should fail parser validation")));
	}

	TEST_METHOD(BadOperatorSequenceError)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser error test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		const int ParseResult = ParseScriptWithResult(BareEngine, "ParserErrorBadOperatorSequence", "int Read() { return 1 + * 2; }");
		ASSERT_THAT(AreEqual(0, ParseResult,
			TEXT("Parser currently accepts this operator sequence at syntax-tree construction time")));
	}

	TEST_METHOD(BadParameterListError)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser error test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		const int ParseResult = ParseScriptWithResult(BareEngine, "ParserErrorBadParameterList", "void Bad(int A,, int B) { }");
		ASSERT_THAT(IsTrue(ParseResult < 0, TEXT("Bad parameter list should fail parser validation")));
	}

	TEST_METHOD(ResetClearsErrorState)
	{
		using namespace AngelscriptNativeTestSupport;

		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser error test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		asCModule* Module = CreateSdkModule(BareEngine, "ParserErrorReset");
		ASSERT_THAT(IsNotNull(Module, TEXT("Parser reset test should create a module")));

		asCBuilder Builder(BareEngine, Module);
		Builder.silent = true;
		AngelscriptNativeTestSupport::FParserAccessor Parser(&Builder);

		asCScriptCode InvalidCode;
		InvalidCode.SetCode("ParserErrorResetInvalid", "void Broken( { }", true);
		ASSERT_THAT(IsTrue(Parser.ParseScript(&InvalidCode) < 0, TEXT("Invalid parser pass should fail")));

		Parser.ResetParser();

		asCScriptCode ValidCode;
		ValidCode.SetCode("ParserErrorResetValid", "void Fixed() { }", true);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&ValidCode),
			TEXT("Parser should accept valid input after explicit Reset")));
	}

	TEST_METHOD(MultipleErrorsAccumulated)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser error test should create a bare engine")));
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		const int ParseResult = ParseScriptWithResult(BareEngine, "ParserErrorMultiple", "void Bad( { }\nclass Broken { int ; }\nint A = ;");
		ASSERT_THAT(IsTrue(ParseResult < 0, TEXT("Multiple malformed declarations should fail parser validation")));
	}
};

#endif
