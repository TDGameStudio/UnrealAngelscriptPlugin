#include "CQTest.h"

#include "AngelscriptTestEngineHelper.h"
#include "StaticJIT/AOT/AngelscriptStaticJITAotFixture.h"
#include "StaticJIT/AngelscriptStaticJIT.h"
#include "StaticJIT/StaticJITDiagnostics.h"
#include "StaticJIT/StaticJITHeader.h"

#include "HAL/IConsoleManager.h"
#include "Misc/OutputDeviceNull.h"

#if AS_WITH_STATIC_JIT_DIAGNOSTICS

namespace AngelscriptTest_StaticJIT_Diagnostics_Private
{
	struct FScopedClearedCurrentEngineStack
	{
		FScopedClearedCurrentEngineStack()
			: SavedStack(FAngelscriptEngineContextStack::SnapshotAndClear())
		{
		}

		~FScopedClearedCurrentEngineStack()
		{
			FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(SavedStack));
		}

	private:
		TArray<FAngelscriptEngine*> SavedStack;
	};

	bool LoadAotFixtureForDiagnosticsTest(FAutomationTestBase& Test, FAngelscriptEngine& Engine)
	{
		FString AvailabilityError;
		if (!Test.TestTrue(TEXT("StaticJIT diagnostics should have generated AOT fixture output"), AngelscriptStaticJITAotFixture::IsGeneratedOutputAvailable(&AvailabilityError)))
		{
			Test.AddError(AvailabilityError);
			return false;
		}

		FString LoadError;
		if (!Test.TestTrue(TEXT("StaticJIT diagnostics should load fixture precompiled data"), FStaticJITDiagnostics::LoadPrecompiledData(Engine, AngelscriptStaticJITAotFixture::GetPrecompiledCacheFilename(), &LoadError)))
		{
			Test.AddError(LoadError);
			return false;
		}

		FString CompileError;
		if (!Test.TestTrue(TEXT("StaticJIT diagnostics should compile fixture from precompiled data"), FStaticJITDiagnostics::CompileLoadedPrecompiledData(Engine, ECompileType::Initial, &CompileError)))
		{
			Test.AddError(CompileError);
			return false;
		}

		return true;
	}

	IConsoleCommand* FindDumpDiagnosticsCommand()
	{
		return static_cast<IConsoleCommand*>(IConsoleManager::Get().FindConsoleObject(TEXT("as.StaticJIT.DumpDiagnostics")));
	}

	bool ExecuteDumpDiagnosticsCommand(FAutomationTestBase& Test, const TArray<FString>& Args, const TCHAR* Context)
	{
		IConsoleCommand* Command = FindDumpDiagnosticsCommand();
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("%s should find the StaticJIT diagnostics dump command"), Context),
				Command))
		{
			return false;
		}

		FOutputDeviceNull OutputDevice;
		return Test.TestTrue(
			*FString::Printf(TEXT("%s should execute the StaticJIT diagnostics dump command"), Context),
			Command->Execute(Args, nullptr, OutputDevice));
	}
}

namespace AngelscriptTest_StaticJIT_Diagnostics_Private
{

bool RunDiagnosticsResolveFixtureState(FAutomationTestBase& Test)
{
	using namespace AngelscriptTest_StaticJIT_Diagnostics_Private;

	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	FAngelscriptEngineScope EngineScope(Engine);

	if (!LoadAotFixtureForDiagnosticsTest(Test, Engine))
	{
		return false;
	}

	asCScriptFunction* EntryFunction = nullptr;
	{
		TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByModuleName(AngelscriptStaticJITAotFixture::GetModuleName().ToString());
		if (ModuleDesc.IsValid() && ModuleDesc->ScriptModule != nullptr)
		{
			FTCHARToUTF8 EntryDecl(*AngelscriptStaticJITAotFixture::GetEntryDeclaration());
			EntryFunction = static_cast<asCScriptFunction*>(ModuleDesc->ScriptModule->GetFunctionByDecl(EntryDecl.Get()));
		}
	}

	if (!Test.TestNotNull(TEXT("StaticJIT diagnostics red test should resolve the fixture entry function"), EntryFunction))
	{
		return false;
	}

	uint32 FunctionId = 0;
	if (!Test.TestTrue(TEXT("StaticJIT diagnostics should resolve the fixture function id"), FStaticJITDiagnostics::ResolveFunctionId(Engine, EntryFunction, FunctionId)))
	{
		return false;
	}

	Test.TestTrue(TEXT("StaticJIT diagnostics should see generated registry state"), FStaticJITDiagnostics::IsFunctionRegistered(FunctionId));
	Test.TestTrue(TEXT("StaticJIT diagnostics should see jitFunction attachment"), FStaticJITDiagnostics::HasJitFunction(EntryFunction));
	Test.TestEqual(TEXT("StaticJIT diagnostics should report no entry marker before execution"), FStaticJITDiagnostics::GetEntryCount(FunctionId), 0);

	FStaticJITDiagnostics::MarkEntry(FunctionId);
	Test.TestEqual(TEXT("StaticJIT diagnostics should report generated entry marker counts"), FStaticJITDiagnostics::GetEntryCount(FunctionId), 1);

	const FStaticJITDiagnostics::FSnapshot Snapshot = FStaticJITDiagnostics::CaptureSnapshot(&Engine);
	Test.TestTrue(TEXT("StaticJIT diagnostics snapshot should report a current engine"), Snapshot.bHasCurrentEngine);
	Test.TestTrue(TEXT("StaticJIT diagnostics snapshot should report precompiled data"), Snapshot.bHasPrecompiledData);
	Test.TestTrue(TEXT("StaticJIT diagnostics snapshot should report registered functions"), Snapshot.RegisteredFunctionCount > 0);
	return true;
}

bool RunDiagnosticsConsoleCommandRegistered(FAutomationTestBase& Test)
{
	using namespace AngelscriptTest_StaticJIT_Diagnostics_Private;

	FScopedClearedCurrentEngineStack NoCurrentEngineScope;
	const FStaticJITDiagnostics::FSnapshot Snapshot = FStaticJITDiagnostics::CaptureSnapshot();
	Test.TestFalse(TEXT("StaticJIT diagnostics command test should run with no current engine"), Snapshot.bHasCurrentEngine);

	bool bPassed = true;
	bPassed &= ExecuteDumpDiagnosticsCommand(Test, {}, TEXT("StaticJIT diagnostics process dump without current engine"));
	Test.AddExpectedError(
		TEXT("StaticJIT diagnostics cannot resolve function 'DefinitelyMissingStaticJITDiagnosticFunction': no current Angelscript engine is available."),
		EAutomationExpectedErrorFlags::Contains,
		1);
	bPassed &= ExecuteDumpDiagnosticsCommand(Test, { TEXT("DefinitelyMissingStaticJITDiagnosticFunction") }, TEXT("StaticJIT diagnostics function dump without current engine"));
	return bPassed;
}

bool RunDiagnosticsConsoleCommandFunctionPaths(FAutomationTestBase& Test)
{
	using namespace AngelscriptTest_StaticJIT_Diagnostics_Private;

	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	FAngelscriptEngineScope EngineScope(Engine);

	if (!LoadAotFixtureForDiagnosticsTest(Test, Engine))
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= ExecuteDumpDiagnosticsCommand(Test, {}, TEXT("StaticJIT diagnostics dump with current engine"));
	Test.AddExpectedError(
		TEXT("Could not resolve StaticJIT diagnostics function argument 'DefinitelyMissingStaticJITDiagnosticFunction'."),
		EAutomationExpectedErrorFlags::Contains,
		1);
	bPassed &= ExecuteDumpDiagnosticsCommand(Test, { TEXT("DefinitelyMissingStaticJITDiagnosticFunction") }, TEXT("StaticJIT diagnostics dump missing function"));
	bPassed &= ExecuteDumpDiagnosticsCommand(Test, { AngelscriptStaticJITAotFixture::GetEntryDeclaration() }, TEXT("StaticJIT diagnostics dump fixture function"));
	return bPassed;
}

}

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITAotDiagnosticTests,
	"Angelscript.TestModule.StaticJIT.AOT.Diagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ResolveFixtureState)
	{
		using namespace AngelscriptTest_StaticJIT_Diagnostics_Private;
		ASSERT_THAT(IsTrue(RunDiagnosticsResolveFixtureState(*TestRunner)));
	}

	TEST_METHOD(ConsoleCommandRegistered)
	{
		using namespace AngelscriptTest_StaticJIT_Diagnostics_Private;
		ASSERT_THAT(IsTrue(RunDiagnosticsConsoleCommandRegistered(*TestRunner)));
	}

	TEST_METHOD(ConsoleCommandFunctionPaths)
	{
		using namespace AngelscriptTest_StaticJIT_Diagnostics_Private;
		ASSERT_THAT(IsTrue(RunDiagnosticsConsoleCommandFunctionPaths(*TestRunner)));
	}
};

#endif
