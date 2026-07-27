#include "AngelscriptNativeReferenceTestSupport.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FReferenceDirectionMutableGlobalRejectionTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.References.Direction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext =
		AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeTestEngine =
		AngelscriptNativeTestSupport::FNativeTestEngine;

	static int CompileAndReport(
		FAutomationTestBase& Test,
		asIScriptEngine& ScriptEngine,
		const FString& SourceId,
		const FString& ModuleName,
		const FString& Source,
		asIScriptModule*& OutModule)
	{
		AngelscriptNativeTestSupport::PrintGeneratedAsSource(
			Test,
			SourceId,
			ModuleName,
			Source);
		const FTCHARToUTF8 ModuleNameUtf8(
			*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		return AngelscriptNativeTestSupport::
			CompileNativeModule(
				&ScriptEngine,
				ModuleNameUtf8.Get(),
				SourceUtf8.Get(),
				OutModule);
	}

public:
	TEST_METHOD(CurrentForkRejectsMutableScriptGlobals)
	{
		using namespace AngelscriptNativeReferenceTestSupport;
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-REF-DIRECTION-FORK-GLOBAL",
			ENativeEvidence::Compile
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Cleanup);

		const FNativeCaseContext Case(
			TEXT("LANG-REF-DIRECTION-FORK-GLOBAL-MUTABLE"));
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			*Case.Describe(TEXT("fork restriction case should create a raw engine"))));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FString Source;
		AppendGeneratedAsLine(
			Source,
			TEXT("int GReferenceDirectionForkRestriction = 0;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("int RecoverReferenceDirectionForkRestriction()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 1;"));
		AppendGeneratedAsLine(Source, TEXT("}"));

		const FString ModuleName =
			TEXT("ReferenceDirectionForkMutableGlobal");
		asIScriptModule* Module = nullptr;
		const int BuildResult = CompileAndReport(
			*TestRunner,
			*ScriptEngine,
			Case.GetId(),
			ModuleName,
			Source,
			Module);
		ASSERT_THAT(IsTrue(
			BuildResult < 0,
			*Case.DescribeResult(
				TEXT("current fork mutable script-global restriction"),
				TEXT("negative build result"),
				DescribeReferenceBuild(Engine, BuildResult))));
		ASSERT_THAT(IsTrue(
			HasDiagnosticContaining(
				Engine,
				TEXT("must be const"))
				&& HasDiagnosticContaining(
					Engine,
					TEXT("Mutable global variables are not supported")),
			*Case.DescribeResult(
				TEXT("current fork mutable script-global diagnostic"),
				TEXT("documented mutable-global rejection"),
				DescribeReferenceBuild(Engine, BuildResult))));
		DiscardReferenceModule(
			*ScriptEngine,
			ModuleName);
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(
				TCHAR_TO_UTF8(*ModuleName),
				asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("fork restriction module shell should discard cleanly"))));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
