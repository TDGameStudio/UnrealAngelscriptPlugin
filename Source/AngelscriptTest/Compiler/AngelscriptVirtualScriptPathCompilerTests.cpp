#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "Containers/StringConv.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptVirtualScriptPathCompilerTest,
	"Angelscript.TestModule.Compiler.VirtualScriptPaths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(MemorySourceCompilesWithFullVirtualPathIdentity)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		const FString VirtualPath = TEXT("/Angelscript/Memory/Immediate/CompilerFullName.as");
		const FString ModuleName = TEXT("Angelscript.Memory.Immediate.CompilerFullName");

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName);
		};

		const bool bCompiled = CompileMemorySource(
			&Engine,
			VirtualPath,
			TEXT(R"(
int Entry()
{
	return 37;
}
)"));
		TestRunner->TestTrue(TEXT("Memory source with a full virtual path should compile"), bCompiled);
		if (!bCompiled)
		{
			return;
		}

		TSharedPtr<FAngelscriptModuleDesc> Module = Engine.GetModule(ModuleName);
		if (!TestRunner->TestTrue(TEXT("Compiled memory source should publish a module descriptor"), Module.IsValid()))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Memory source module should use the full virtual path derived module name"), Module->ModuleName, ModuleName);
		TestRunner->TestEqual(TEXT("Memory source module should contain one code section"), Module->Code.Num(), 1);
		if (Module->Code.Num() == 1)
		{
			TestRunner->TestEqual(
				TEXT("Compiled memory source section should keep the full virtual path"),
				Module->Code[0].VirtualPath,
				VirtualPath);
			TestRunner->TestEqual(
				TEXT("Compiled memory source section should have no physical absolute filename"),
				Module->Code[0].AbsoluteFilename,
				FString());
			TestRunner->TestEqual(
				TEXT("Compiled memory source section should keep provider-relative filename"),
				Module->Code[0].RelativeFilename,
				FString(TEXT("CompilerFullName.as")));
		}

		if (TestRunner->TestNotNull(TEXT("Compiled memory source should publish a backing AS module"), Module->ScriptModule))
		{
			const auto EntryDeclarationUtf8 = StringCast<ANSICHAR>(TEXT("int Entry()"));
			asIScriptFunction* EntryFunction = Module->ScriptModule->GetFunctionByDecl(EntryDeclarationUtf8.Get());
			if (TestRunner->TestNotNull(TEXT("Compiled memory source should expose Entry()"), EntryFunction))
			{
				TestRunner->TestEqual(
					TEXT("AS compiler section name should use the full virtual path when no physical file exists"),
					FString(UTF8_TO_TCHAR(EntryFunction->GetScriptSectionName())),
					VirtualPath);
			}
		}

		int32 EntryResult = 0;
		const bool bExecuted = ExecuteIntFunction(&Engine, FName(*ModuleName), TEXT("int Entry()"), EntryResult);
		TestRunner->TestTrue(TEXT("Compiled memory source should execute by module name"), bExecuted);
		if (bExecuted)
		{
			TestRunner->TestEqual(TEXT("Compiled memory source should return the expected value"), EntryResult, 37);
		}

		}
	}
};

#endif
