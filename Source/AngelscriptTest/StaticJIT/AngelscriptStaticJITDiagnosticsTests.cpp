#include "Misc/AutomationTest.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptStaticJITDiagnosticsResolveFixtureStateTest,
	"Angelscript.TestModule.StaticJIT.AOT.Diagnostics.ResolveFixtureState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptStaticJITDiagnosticsResolveFixtureStateTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_StaticJIT_Diagnostics_Private;

	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	FAngelscriptEngineScope EngineScope(Engine);

	if (!LoadAotFixtureForDiagnosticsTest(*this, Engine))
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

	if (!TestNotNull(TEXT("StaticJIT diagnostics red test should resolve the fixture entry function"), EntryFunction))
	{
		return false;
	}

	uint32 FunctionId = 0;
	if (!TestTrue(TEXT("StaticJIT diagnostics should resolve the fixture function id"), FStaticJITDiagnostics::ResolveFunctionId(Engine, EntryFunction, FunctionId)))
	{
		return false;
	}

	TestTrue(TEXT("StaticJIT diagnostics should see generated registry state"), FStaticJITDiagnostics::IsFunctionRegistered(FunctionId));
	TestTrue(TEXT("StaticJIT diagnostics should see jitFunction attachment"), FStaticJITDiagnostics::HasJitFunction(EntryFunction));
	TestEqual(TEXT("StaticJIT diagnostics should report no entry marker before execution"), FStaticJITDiagnostics::GetEntryCount(FunctionId), 0);

	FStaticJITDiagnostics::MarkEntry(FunctionId);
	TestEqual(TEXT("StaticJIT diagnostics should report generated entry marker counts"), FStaticJITDiagnostics::GetEntryCount(FunctionId), 1);

	const FStaticJITDiagnostics::FSnapshot Snapshot = FStaticJITDiagnostics::CaptureSnapshot(&Engine);
	TestTrue(TEXT("StaticJIT diagnostics snapshot should report a current engine"), Snapshot.bHasCurrentEngine);
	TestTrue(TEXT("StaticJIT diagnostics snapshot should report precompiled data"), Snapshot.bHasPrecompiledData);
	TestTrue(TEXT("StaticJIT diagnostics snapshot should report registered functions"), Snapshot.RegisteredFunctionCount > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptStaticJITDiagnosticsConsoleCommandRegisteredTest,
	"Angelscript.TestModule.StaticJIT.AOT.Diagnostics.ConsoleCommandRegistered",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptStaticJITDiagnosticsConsoleCommandRegisteredTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_StaticJIT_Diagnostics_Private;

	FScopedClearedCurrentEngineStack NoCurrentEngineScope;
	const FStaticJITDiagnostics::FSnapshot Snapshot = FStaticJITDiagnostics::CaptureSnapshot();
	TestFalse(TEXT("StaticJIT diagnostics command test should run with no current engine"), Snapshot.bHasCurrentEngine);

	bool bPassed = true;
	bPassed &= ExecuteDumpDiagnosticsCommand(*this, {}, TEXT("StaticJIT diagnostics process dump without current engine"));
	AddExpectedError(
		TEXT("StaticJIT diagnostics cannot resolve function 'DefinitelyMissingStaticJITDiagnosticFunction': no current Angelscript engine is available."),
		EAutomationExpectedErrorFlags::Contains,
		1);
	bPassed &= ExecuteDumpDiagnosticsCommand(*this, { TEXT("DefinitelyMissingStaticJITDiagnosticFunction") }, TEXT("StaticJIT diagnostics function dump without current engine"));
	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptStaticJITDiagnosticsConsoleCommandFunctionPathsTest,
	"Angelscript.TestModule.StaticJIT.AOT.Diagnostics.ConsoleCommandFunctionPaths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptStaticJITDiagnosticsConsoleCommandFunctionPathsTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_StaticJIT_Diagnostics_Private;

	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	FAngelscriptEngineScope EngineScope(Engine);

	if (!LoadAotFixtureForDiagnosticsTest(*this, Engine))
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= ExecuteDumpDiagnosticsCommand(*this, {}, TEXT("StaticJIT diagnostics dump with current engine"));
	AddExpectedError(
		TEXT("Could not resolve StaticJIT diagnostics function argument 'DefinitelyMissingStaticJITDiagnosticFunction'."),
		EAutomationExpectedErrorFlags::Contains,
		1);
	bPassed &= ExecuteDumpDiagnosticsCommand(*this, { TEXT("DefinitelyMissingStaticJITDiagnosticFunction") }, TEXT("StaticJIT diagnostics dump missing function"));
	bPassed &= ExecuteDumpDiagnosticsCommand(*this, { AngelscriptStaticJITAotFixture::GetEntryDeclaration() }, TEXT("StaticJIT diagnostics dump fixture function"));
	return bPassed;
}

#endif
