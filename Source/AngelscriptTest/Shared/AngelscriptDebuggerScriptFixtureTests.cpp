#include "AngelscriptDebuggerScriptFixture.h"

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptDebuggerFixtureIdentityIsolatedPerInstanceTest,
	"Angelscript.TestModule.Debugger.Shared.FixtureIdentity.IsolatedPerInstance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptDebuggerFixtureIdentityIsolatedPerInstanceTest::RunTest(const FString& Parameters)
{
	DestroySharedAndStrayGlobalTestEngine();
	FAngelscriptEngine& Engine = AcquireCleanSharedCloneEngine();
	const FAngelscriptDebuggerScriptFixture FixtureA =
		FAngelscriptDebuggerScriptFixture::CreateNamedBreakpointFixture(
			TEXT("DebuggerBreakpointFixtureA"),
			TEXT("DebuggerBreakpointFixtureA.as"),
			5);
	const FAngelscriptDebuggerScriptFixture FixtureB =
		FAngelscriptDebuggerScriptFixture::CreateNamedBreakpointFixture(
			TEXT("DebuggerBreakpointFixtureB"),
			TEXT("DebuggerBreakpointFixtureB.as"),
			9);

	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*FixtureA.ModuleName.ToString());
		Engine.DiscardModule(*FixtureB.ModuleName.ToString());
	};

	bool bPassed = true;
	bPassed &= TestTrue(
		TEXT("Debugger.Shared.FixtureIdentity.IsolatedPerInstance should give fixture A and B distinct module names"),
		FixtureA.ModuleName != FixtureB.ModuleName);
	bPassed &= TestTrue(
		TEXT("Debugger.Shared.FixtureIdentity.IsolatedPerInstance should give fixture A and B distinct filenames"),
		FixtureA.Filename != FixtureB.Filename);

	if (!TestTrue(TEXT("Debugger.Shared.FixtureIdentity.IsolatedPerInstance should compile fixture A"), FixtureA.Compile(Engine))
		|| !TestTrue(TEXT("Debugger.Shared.FixtureIdentity.IsolatedPerInstance should compile fixture B"), FixtureB.Compile(Engine)))
	{
		return false;
	}

	const FString AbsoluteFilenameA = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), FixtureA.Filename);
	const FString AbsoluteFilenameB = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), FixtureB.Filename);

	TSharedPtr<FAngelscriptModuleDesc> ModuleA =
		Engine.GetModuleByFilenameOrModuleName(AbsoluteFilenameA, FixtureA.ModuleName.ToString());
	TSharedPtr<FAngelscriptModuleDesc> ModuleB =
		Engine.GetModuleByFilenameOrModuleName(AbsoluteFilenameB, FixtureB.ModuleName.ToString());

	if (!TestTrue(TEXT("Debugger.Shared.FixtureIdentity.IsolatedPerInstance should resolve fixture A by its own identity"), ModuleA.IsValid())
		|| !TestTrue(TEXT("Debugger.Shared.FixtureIdentity.IsolatedPerInstance should resolve fixture B by its own identity"), ModuleB.IsValid()))
	{
		return false;
	}

	bPassed &= TestEqual(
		TEXT("Debugger.Shared.FixtureIdentity.IsolatedPerInstance should keep fixture A mapped to its own module"),
		ModuleA->ModuleName,
		FixtureA.ModuleName.ToString());
	bPassed &= TestEqual(
		TEXT("Debugger.Shared.FixtureIdentity.IsolatedPerInstance should keep fixture B mapped to its own module"),
		ModuleB->ModuleName,
		FixtureB.ModuleName.ToString());

	int32 ResultA = 0;
	int32 ResultB = 0;
	if (!TestTrue(
			TEXT("Debugger.Shared.FixtureIdentity.IsolatedPerInstance should execute fixture A through its own filename/module pair"),
			ExecuteIntFunction(&Engine, AbsoluteFilenameA, FixtureA.ModuleName, FixtureA.EntryFunctionDeclaration, ResultA))
		|| !TestTrue(
			TEXT("Debugger.Shared.FixtureIdentity.IsolatedPerInstance should execute fixture B through its own filename/module pair"),
			ExecuteIntFunction(&Engine, AbsoluteFilenameB, FixtureB.ModuleName, FixtureB.EntryFunctionDeclaration, ResultB)))
	{
		return false;
	}

	bPassed &= TestEqual(
		TEXT("Debugger.Shared.FixtureIdentity.IsolatedPerInstance should preserve fixture A's distinct stored value"),
		ResultA,
		8);
	bPassed &= TestEqual(
		TEXT("Debugger.Shared.FixtureIdentity.IsolatedPerInstance should preserve fixture B's distinct stored value"),
		ResultB,
		12);

	Engine.DiscardModule(*FixtureA.ModuleName.ToString());
	bPassed &= TestFalse(
		TEXT("Debugger.Shared.FixtureIdentity.IsolatedPerInstance should invalidate fixture A lookup after discarding only A"),
		Engine.GetModuleByFilenameOrModuleName(AbsoluteFilenameA, FixtureA.ModuleName.ToString()).IsValid());

	TSharedPtr<FAngelscriptModuleDesc> ModuleBAfterDiscard =
		Engine.GetModuleByFilenameOrModuleName(AbsoluteFilenameB, FixtureB.ModuleName.ToString());
	if (!TestTrue(
			TEXT("Debugger.Shared.FixtureIdentity.IsolatedPerInstance should keep fixture B lookup alive after discarding A"),
			ModuleBAfterDiscard.IsValid()))
	{
		return false;
	}

	int32 ResultBAfterDiscard = 0;
	if (!TestTrue(
			TEXT("Debugger.Shared.FixtureIdentity.IsolatedPerInstance should keep executing fixture B after discarding A"),
			ExecuteIntFunction(&Engine, AbsoluteFilenameB, FixtureB.ModuleName, FixtureB.EntryFunctionDeclaration, ResultBAfterDiscard)))
	{
		return false;
	}

	bPassed &= TestEqual(
		TEXT("Debugger.Shared.FixtureIdentity.IsolatedPerInstance should keep fixture B's result stable after discarding A"),
		ResultBAfterDiscard,
		12);
	return bPassed;
}

#endif
