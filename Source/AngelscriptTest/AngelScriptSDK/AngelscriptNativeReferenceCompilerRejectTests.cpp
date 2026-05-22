#include "AngelscriptNativeTestSupport.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_AngelScriptSDK_ReferenceCompilerReject_Private
{
	bool ContainsError(const AngelscriptNativeTestSupport::FNativeMessageCollector& Messages, const TCHAR* Needle)
	{
		for (const AngelscriptNativeTestSupport::FNativeMessageEntry& Entry : Messages.Entries)
		{
			if (Entry.Type == asMSGTYPE_ERROR && Entry.Message.Contains(Needle))
			{
				return true;
			}
		}

		return false;
	}

	int CompileSnippet(const char* ModuleName, const char* Source, AngelscriptNativeTestSupport::FNativeMessageCollector& Messages)
	{
		asIScriptEngine* ScriptEngine = AngelscriptNativeTestSupport::CreateNativeEngine(&Messages);
		if (ScriptEngine == nullptr)
		{
			return asERROR;
		}

		ON_SCOPE_EXIT
		{
			AngelscriptNativeTestSupport::DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = nullptr;
		return AngelscriptNativeTestSupport::CompileNativeModule(ScriptEngine, ModuleName, Source, Module);
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptNativeReferenceCompilerRejectTests,
	"Angelscript.TestModule.AngelScriptSDK.Reference.CompilerReject",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(InvalidConstObjectAssignmentReportsTypeMismatch)
	{
		using namespace AngelscriptTest_AngelScriptSDK_ReferenceCompilerReject_Private;
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		const int CompileResult = CompileSnippet("ReferenceCompilerConstObjectAssignment", R"(
class Foo
{
}

void Main()
{
	const Foo F = 10;
}
)",
			Messages);

		TestRunner->TestTrue(TEXT("Reference invalid const object assignment should fail to build"), CompileResult < 0);
		TestRunner->TestTrue(TEXT("Reference invalid const object assignment should report the target type or conversion"), ContainsError(Messages, TEXT("Foo")) || ContainsError(Messages, TEXT("convert")));
	}

	TEST_METHOD(OutOfScopeLocalReferenceReportsIdentifier)
	{
		using namespace AngelscriptTest_AngelScriptSDK_ReferenceCompilerReject_Private;
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		const int CompileResult = CompileSnippet("ReferenceCompilerOutOfScopeLocal", R"(
int Entry()
{
	{
		int Inner = 2;
	}
	return Inner;
}
)",
			Messages);

		TestRunner->TestTrue(TEXT("Reference out-of-scope local should fail to build"), CompileResult < 0);
		TestRunner->TestTrue(TEXT("Reference out-of-scope local should keep the missing identifier name"), ContainsError(Messages, TEXT("Inner")) || ContainsError(Messages, TEXT("not declared")));
	}

	TEST_METHOD(UnknownFunctionCallReportsMissingSymbol)
	{
		using namespace AngelscriptTest_AngelScriptSDK_ReferenceCompilerReject_Private;
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		const int CompileResult = CompileSnippet("ReferenceCompilerUnknownCall", R"(
void Entry()
{
	MissingCall(1, 2);
}
)",
			Messages);

		TestRunner->TestTrue(TEXT("Reference unknown function call should fail to build"), CompileResult < 0);
		TestRunner->TestTrue(TEXT("Reference unknown function call should keep the missing symbol name"), ContainsError(Messages, TEXT("MissingCall")) || ContainsError(Messages, TEXT("No matching symbol")));
	}

	TEST_METHOD(ReturnObjectFromIntFunctionIsRejected)
	{
		using namespace AngelscriptTest_AngelScriptSDK_ReferenceCompilerReject_Private;
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		const int CompileResult = CompileSnippet("ReferenceCompilerReturnObjectAsInt", R"(
class Foo
{
}

int Entry()
{
	Foo F;
	return F;
}
)",
			Messages);

		TestRunner->TestTrue(TEXT("Reference object returned from int function should fail to build"), CompileResult < 0);
		TestRunner->TestTrue(TEXT("Reference object returned from int function should report type mismatch context"), ContainsError(Messages, TEXT("Foo")) || ContainsError(Messages, TEXT("int")) || ContainsError(Messages, TEXT("convert")));
	}

	TEST_METHOD(LongIdentifierAssignmentReportsDiagnosticWithoutCrash)
	{
		using namespace AngelscriptTest_AngelScriptSDK_ReferenceCompilerReject_Private;
		const FString LongIdentifier = FString::ChrN(400, TEXT('a'));
		const FString Source = FString::Printf(TEXT(R"(
void Entry()
{
	%s = 1;
}
)"),
			*LongIdentifier);
		const FTCHARToUTF8 SourceUtf8(*Source);

		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		const int CompileResult = CompileSnippet("ReferenceCompilerLongIdentifier", SourceUtf8.Get(), Messages);

		TestRunner->TestTrue(TEXT("Reference long-token assignment should fail to build without crashing"), CompileResult < 0);
		TestRunner->TestTrue(TEXT("Reference long-token assignment should produce a useful missing-symbol diagnostic"), ContainsError(Messages, TEXT("No matching symbol")) || ContainsError(Messages, TEXT("not declared")) || ContainsError(Messages, *LongIdentifier.Left(32)));
	}
};

#endif
