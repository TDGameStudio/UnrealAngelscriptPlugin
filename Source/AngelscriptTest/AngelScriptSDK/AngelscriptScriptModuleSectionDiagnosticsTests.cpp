#include "AngelscriptNativeTestSupport.h"

#include "CQTest.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;

TEST_CLASS_WITH_FLAGS(FAngelscriptScriptModuleSectionDiagnosticsTests,
	"Angelscript.TestModule.AngelScriptSDK.ScriptModule.SectionDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	inline static FNativeTestEngine Engine;

	BEFORE_ALL()
	{
		Engine.Create(*TestRunner);
	}

	AFTER_ALL()
	{
		Engine.Destroy();
	}

	BEFORE_EACH()
	{
		Engine.ResetMessages();
	}

	TEST_METHOD(SyntaxErrorReportsSectionAndOffset)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("ScriptModule section diagnostic test should create a standalone SDK engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModuleName ModuleScope(Engine, "ScriptModuleSectionSyntaxError");
		asIScriptModule* Module = ScriptEngine->GetModule(ModuleScope.Get(), asGM_ALWAYS_CREATE);
		if (!TestRunner->TestNotNull(TEXT("ScriptModule section diagnostic test should create a module"), Module))
		{
			return;
		}

		const char* Source = R"(

int Broken(
{
	return 1;
}
)";
		const int AddResult = Module->AddScriptSection("ScriptModuleSectionSyntaxError_A", Source, std::strlen(Source), 10);
		if (!TestRunner->TestTrue(TEXT("ScriptModule section diagnostic test should add the broken source section"), AddResult >= 0))
		{
			return;
		}

		const int BuildResult = Module->Build();
		if (!TestRunner->TestTrue(TEXT("ScriptModule section diagnostic test should fail to build invalid syntax"), BuildResult < 0))
		{
			return;
		}

		const FNativeMessageCollector& Messages = Engine.GetMessages();
		if (!TestRunner->TestTrue(TEXT("ScriptModule section diagnostic test should capture at least one diagnostic"), Messages.Entries.Num() > 0))
		{
			return;
		}

		const FNativeMessageEntry& FirstError = Messages.Entries[0];
		TestRunner->TestEqual(TEXT("ScriptModule section diagnostic test should report the failing section"), FirstError.Section, FString(TEXT("ScriptModuleSectionSyntaxError_A")));
		TestRunner->TestTrue(TEXT("ScriptModule section diagnostic test should include the section line offset"), FirstError.Row >= 12);
		TestRunner->TestTrue(TEXT("ScriptModule section diagnostic test should report a positive column"), FirstError.Column > 0);
	}

	TEST_METHOD(CrossSectionFunctionKeepsDeclaringSection)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		if (!TestRunner->TestNotNull(TEXT("ScriptModule cross-section diagnostic test should create a standalone SDK engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModuleName ModuleScope(Engine, "ScriptModuleCrossSectionNames");
		asIScriptModule* Module = ScriptEngine->GetModule(ModuleScope.Get(), asGM_ALWAYS_CREATE);
		if (!TestRunner->TestNotNull(TEXT("ScriptModule cross-section diagnostic test should create a module"), Module))
		{
			return;
		}

		const char* HelperSource = R"(
int Helper()
{
	return 40;
}
)";
		const char* EntrySource = R"(
int Entry()
{
	return Helper() + 2;
}
)";
		if (!TestRunner->TestTrue(TEXT("ScriptModule cross-section diagnostic test should add helper section"), Module->AddScriptSection("ScriptModuleCrossSection_Helper", HelperSource, std::strlen(HelperSource), 0) >= 0) ||
			!TestRunner->TestTrue(TEXT("ScriptModule cross-section diagnostic test should add entry section"), Module->AddScriptSection("ScriptModuleCrossSection_Entry", EntrySource, std::strlen(EntrySource), 0) >= 0))
		{
			return;
		}
		if (!TestRunner->TestEqual(TEXT("ScriptModule cross-section diagnostic test should build both sections"), Module->Build(), static_cast<int32>(asSUCCESS)))
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		asIScriptFunction* Helper = Module->GetFunctionByDecl("int Helper()");
		asIScriptFunction* Entry = Module->GetFunctionByDecl("int Entry()");
		if (!TestRunner->TestNotNull(TEXT("ScriptModule cross-section diagnostic test should expose Helper"), Helper) ||
			!TestRunner->TestNotNull(TEXT("ScriptModule cross-section diagnostic test should expose Entry"), Entry))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("ScriptModule cross-section diagnostic test should preserve Helper section"), FString(UTF8_TO_TCHAR(Helper->GetScriptSectionName())), FString(TEXT("ScriptModuleCrossSection_Helper")));
		TestRunner->TestEqual(TEXT("ScriptModule cross-section diagnostic test should preserve Entry section"), FString(UTF8_TO_TCHAR(Entry->GetScriptSectionName())), FString(TEXT("ScriptModuleCrossSection_Entry")));
	}

};

#endif
