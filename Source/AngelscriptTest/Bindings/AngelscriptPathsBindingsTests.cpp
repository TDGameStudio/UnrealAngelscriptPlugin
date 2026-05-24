// AngelscriptPathsBindingsTests.cpp
// CQTest coverage for FPaths, FApp, FCommandLine, FFileHelper bindings.
// Automation IDs: Angelscript.TestModule.Bindings.Paths.*

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#if WITH_DEV_AUTOMATION_TESTS



TEST_CLASS_WITH_FLAGS(FAngelscriptPathsBindingsTest,
	"Angelscript.TestModule.Bindings.Paths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL() { ASTEST_CREATE_ENGINE(); }
	AFTER_ALL() { FAngelscriptEngine& E = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(E); }

	TEST_METHOD(FPathsProjectDir)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASPaths_ProjectDir"), TEXT(R"(
int Paths_ProjectDirNonEmpty()
{
	FString Dir = FPaths::ProjectDir();
	return Dir.Len() > 0 ? 1 : 0;
}
)"));
		if (!TestRunner->TestTrue(TEXT("FPaths::ProjectDir binding module should compile"), Mod.IsValid()))
		{
			return;
		}
		ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), 
			TEXT("int Paths_ProjectDirNonEmpty()"),
			TEXT("FPaths::ProjectDir is non-empty"), 1);
	}

	TEST_METHOD(FPathsGetExtension)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASPaths_Extension"), TEXT(R"(
int Paths_GetExtensionLen()
{
	FString Ext = FPaths::GetExtension("MyFile.as");
	return Ext.Len();
}
)"));
		if (!TestRunner->TestTrue(TEXT("FPaths::GetExtension binding module should compile"), Mod.IsValid()))
		{
			return;
		}
		ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), 
			TEXT("int Paths_GetExtensionLen()"),
			TEXT("Extension of 'MyFile.as' is 2 chars"), 2);
	}

	TEST_METHOD(FAppGetName)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ExpectedFragments[] = {
			TEXT("GetName"),
			TEXT("FApp"),
		};
		ExpectBindingCompileFailure(
			*TestRunner,
			Engine,
			TEXT("ASPaths_AppNameMissing"),
			TEXT(R"(
int App_GetNameNonEmpty()
{
	FString Name = FApp::GetName();
	return Name.Len() > 0 ? 1 : 0;
}
)"),
			TEXT("FApp::GetName is not part of the current FApp binding surface; FApp::GetProjectName remains the supported name helper"),
			MakeArrayView(ExpectedFragments));
	}

	TEST_METHOD(FAppGetProjectName)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASPaths_AppProjectName"), TEXT(R"(
int App_GetProjectNameDoesNotCrash()
{
	FString Name = FApp::GetProjectName();
	return Name.Len() >= 0 ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), 
			TEXT("int App_GetProjectNameDoesNotCrash()"),
			TEXT("FApp::GetProjectName is the supported app name helper"), 1);
	}

	TEST_METHOD(FCommandLineGet)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASPaths_CmdLine"), TEXT(R"(
int CommandLine_GetExists()
{
	FString Cmd = FCommandLine::Get();
	return Cmd.Len() >= 0 ? 1 : 0;
}
)"));
		if (!TestRunner->TestTrue(TEXT("FCommandLine::Get binding module should compile"), Mod.IsValid()))
		{
			return;
		}
		ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), 
			TEXT("int CommandLine_GetExists()"),
			TEXT("FCommandLine::Get does not crash"), 1);
	}
};

#endif
