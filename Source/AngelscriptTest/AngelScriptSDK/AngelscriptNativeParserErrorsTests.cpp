#include "AngelscriptNativeTestSupport.h"
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

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;

namespace
{
	int ParseScriptWithResult(asCScriptEngine* ScriptEngine, const char* ModuleName, const char* Source)
	{
		asCModule* Module = CreateSdkModule(ScriptEngine, ModuleName);
		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;

		asCScriptCode Code;
		Code.SetCode(ModuleName, Source, true);

		FParserAccessor Parser(&Builder);
		return Parser.ParseScript(&Code);
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptNativeParserErrorsTests,
	"Angelscript.TestModule.AngelScriptSDK.Parser.Errors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(MissingSemicolonRecovers)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("Parser error test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		const int ParseResult = ParseScriptWithResult(BareEngine, "ParserErrorMissingSemicolon", "int A = 1 int B = 2;");
		TestRunner->TestEqual(TEXT("Parser currently accepts adjacent declarations without an explicit semicolon error in this recovery path"), ParseResult, 0);
	}

	TEST_METHOD(UnbalancedBracesError)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("Parser error test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		const int ParseResult = ParseScriptWithResult(BareEngine, "ParserErrorUnbalancedBraces", "class FBroken { void Run() { }");
		TestRunner->TestTrue(TEXT("Unbalanced braces should fail parser validation"), ParseResult < 0);
	}

	TEST_METHOD(UnclosedStringInDeclaration)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("Parser error test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		const int ParseResult = ParseScriptWithResult(BareEngine, "ParserErrorUnclosedString", "const string Name = \"unterminated;");
		TestRunner->TestTrue(TEXT("Unclosed string should fail parser validation"), ParseResult < 0);
	}

	TEST_METHOD(BadOperatorSequenceError)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("Parser error test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		const int ParseResult = ParseScriptWithResult(BareEngine, "ParserErrorBadOperatorSequence", "int Read() { return 1 + * 2; }");
		TestRunner->TestEqual(TEXT("Parser currently accepts this operator sequence at syntax-tree construction time"), ParseResult, 0);
	}

	TEST_METHOD(BadParameterListError)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("Parser error test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		const int ParseResult = ParseScriptWithResult(BareEngine, "ParserErrorBadParameterList", "void Bad(int A,, int B) { }");
		TestRunner->TestTrue(TEXT("Bad parameter list should fail parser validation"), ParseResult < 0);
	}

	TEST_METHOD(ResetClearsErrorState)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("Parser error test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		asCModule* Module = CreateSdkModule(BareEngine, "ParserErrorReset");
		if (!TestRunner->TestNotNull(TEXT("Parser reset test should create a module"), Module))
		{
			return;
		}

		asCBuilder Builder(BareEngine, Module);
		Builder.silent = true;
		FParserAccessor Parser(&Builder);

		asCScriptCode InvalidCode;
		InvalidCode.SetCode("ParserErrorResetInvalid", "void Broken( { }", true);
		TestRunner->TestTrue(TEXT("Invalid parser pass should fail"), Parser.ParseScript(&InvalidCode) < 0);

		Parser.ResetParser();

		asCScriptCode ValidCode;
		ValidCode.SetCode("ParserErrorResetValid", "void Fixed() { }", true);
		TestRunner->TestEqual(TEXT("Parser should accept valid input after explicit Reset"), Parser.ParseScript(&ValidCode), 0);
	}

	TEST_METHOD(MultipleErrorsAccumulated)
	{
		asCScriptEngine* BareEngine = AngelscriptNativeTestSupport::CreateBareSdkEngine(&*TestRunner);
		if (!TestRunner->TestNotNull(TEXT("Parser error test should create a bare engine"), BareEngine))
		{
			return;
		}
		ON_SCOPE_EXIT { BareEngine->ShutDownAndRelease(); };

		const int ParseResult = ParseScriptWithResult(BareEngine, "ParserErrorMultiple", "void Bad( { }\nclass Broken { int ; }\nint A = ;");
		TestRunner->TestTrue(TEXT("Multiple malformed declarations should fail parser validation"), ParseResult < 0);
	}
};

#endif
