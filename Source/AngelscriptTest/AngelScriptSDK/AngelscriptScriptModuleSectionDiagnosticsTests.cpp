#include "AngelscriptNativeTestSupport.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptScriptModuleSectionDiagnosticsTests,
	"Angelscript.TestModule.AngelScriptSDK.ScriptModule.SectionDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	inline static AngelscriptNativeTestSupport::FNativeTestEngine Engine;

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
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("ScriptModule section diagnostic test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "ScriptModuleSectionSyntaxError");
		asIScriptModule* Module = ScriptEngine->GetModule(ModuleScope.Get(), asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module,
			TEXT("ScriptModule section diagnostic test should create a module")));

		const char* Source = R"(

int Broken(
{
	return 1;
}
)";
		const int AddResult = Module->AddScriptSection("ScriptModuleSectionSyntaxError_A", Source, std::strlen(Source), 10);
		ASSERT_THAT(IsTrue(AddResult >= 0,
			TEXT("ScriptModule section diagnostic test should add the broken source section")));

		const int BuildResult = Module->Build();
		ASSERT_THAT(IsTrue(BuildResult < 0,
			TEXT("ScriptModule section diagnostic test should fail to build invalid syntax")));

		const AngelscriptNativeTestSupport::FNativeMessageCollector& Messages = Engine.GetMessages();
		ASSERT_THAT(IsTrue(Messages.Entries.Num() > 0,
			TEXT("ScriptModule section diagnostic test should capture at least one diagnostic")));

		const AngelscriptNativeTestSupport::FNativeMessageEntry& FirstError = Messages.Entries[0];
		ASSERT_THAT(AreEqual(FString(TEXT("ScriptModuleSectionSyntaxError_A")), FirstError.Section,
			TEXT("ScriptModule section diagnostic test should report the failing section")));
		ASSERT_THAT(IsTrue(FirstError.Row >= 12,
			TEXT("ScriptModule section diagnostic test should include the section line offset")));
		ASSERT_THAT(IsTrue(FirstError.Column > 0,
			TEXT("ScriptModule section diagnostic test should report a positive column")));
	}

	TEST_METHOD(CrossSectionFunctionKeepsDeclaringSection)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("ScriptModule cross-section diagnostic test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "ScriptModuleCrossSectionNames");
		asIScriptModule* Module = ScriptEngine->GetModule(ModuleScope.Get(), asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module,
			TEXT("ScriptModule cross-section diagnostic test should create a module")));

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
		ASSERT_THAT(IsTrue(Module->AddScriptSection("ScriptModuleCrossSection_Helper", HelperSource, std::strlen(HelperSource), 0) >= 0,
			TEXT("ScriptModule cross-section diagnostic test should add helper section")));
		ASSERT_THAT(IsTrue(Module->AddScriptSection("ScriptModuleCrossSection_Entry", EntrySource, std::strlen(EntrySource), 0) >= 0,
			TEXT("ScriptModule cross-section diagnostic test should add entry section")));
		if (!this->Assert.AreEqual(static_cast<int32>(asSUCCESS), Module->Build(),
			TEXT("ScriptModule cross-section diagnostic test should build both sections")))
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		asIScriptFunction* Helper = Module->GetFunctionByDecl("int Helper()");
		asIScriptFunction* Entry = Module->GetFunctionByDecl("int Entry()");
		ASSERT_THAT(IsNotNull(Helper,
			TEXT("ScriptModule cross-section diagnostic test should expose Helper")));
		ASSERT_THAT(IsNotNull(Entry,
			TEXT("ScriptModule cross-section diagnostic test should expose Entry")));

		ASSERT_THAT(AreEqual(FString(TEXT("ScriptModuleCrossSection_Helper")), FString(UTF8_TO_TCHAR(Helper->GetScriptSectionName())),
			TEXT("ScriptModule cross-section diagnostic test should preserve Helper section")));
		ASSERT_THAT(AreEqual(FString(TEXT("ScriptModuleCrossSection_Entry")), FString(UTF8_TO_TCHAR(Entry->GetScriptSectionName())),
			TEXT("ScriptModule cross-section diagnostic test should preserve Entry section")));
	}

};

#endif
