#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "Containers/StringConv.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCompilerVirtualScriptPathTests,
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
		ASSERT_THAT(IsTrue(bCompiled, TEXT("Memory source with a full virtual path should compile")));

		TSharedPtr<FAngelscriptModuleDesc> Module = Engine.GetModule(ModuleName);
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("Compiled memory source should publish a module descriptor")));

		ASSERT_THAT(AreEqual(ModuleName, Module->ModuleName, TEXT("Memory source module should use the full virtual path derived module name")));
		ASSERT_THAT(AreEqual(1, Module->Code.Num(), TEXT("Memory source module should contain one code section")));
		ASSERT_THAT(AreEqual(
			VirtualPath,
			Module->Code[0].VirtualPath,
			TEXT("Compiled memory source section should keep the full virtual path")));
		ASSERT_THAT(AreEqual(
			FString(),
			Module->Code[0].AbsoluteFilename,
			TEXT("Compiled memory source section should have no physical absolute filename")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("CompilerFullName.as")),
			Module->Code[0].RelativeFilename,
			TEXT("Compiled memory source section should keep provider-relative filename")));

		ASSERT_THAT(IsNotNull(Module->ScriptModule, TEXT("Compiled memory source should publish a backing AS module")));
		const auto EntryDeclarationUtf8 = StringCast<ANSICHAR>(TEXT("int Entry()"));
		asIScriptFunction* EntryFunction = Module->ScriptModule->GetFunctionByDecl(EntryDeclarationUtf8.Get());
		ASSERT_THAT(IsNotNull(EntryFunction, TEXT("Compiled memory source should expose Entry()")));
		ASSERT_THAT(AreEqual(
			VirtualPath,
			FString(UTF8_TO_TCHAR(EntryFunction->GetScriptSectionName())),
			TEXT("AS compiler section name should use the full virtual path when no physical file exists")));

		int32 EntryResult = 0;
		const bool bExecuted = ExecuteIntFunction(&Engine, FName(*ModuleName), TEXT("int Entry()"), EntryResult);
		ASSERT_THAT(IsTrue(bExecuted, TEXT("Compiled memory source should execute by module name")));
		ASSERT_THAT(AreEqual(37, EntryResult, TEXT("Compiled memory source should return the expected value")));

		}
	}
};

#endif
