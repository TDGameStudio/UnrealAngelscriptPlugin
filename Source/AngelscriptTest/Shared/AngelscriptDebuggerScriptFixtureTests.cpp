#include "AngelscriptDebuggerScriptFixture.h"
#include "AngelscriptTestEngineAcquisition.h"

#include "CQTest.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(
	FAngelscriptDebuggerFixtureIdentityIsolatedPerInstanceTest,
	"Angelscript.TestModule.Debugger.Shared.FixtureIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(IsolatedPerInstance)
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

		TestRunner->TestTrue(
			TEXT("Debugger.Shared.FixtureIdentity.IsolatedPerInstance should give fixture A and B distinct module names"),
			FixtureA.ModuleName != FixtureB.ModuleName);
		TestRunner->TestTrue(
			TEXT("Debugger.Shared.FixtureIdentity.IsolatedPerInstance should give fixture A and B distinct filenames"),
			FixtureA.Filename != FixtureB.Filename);

		ASSERT_THAT(IsTrue(FixtureA.Compile(Engine)));
		ASSERT_THAT(IsTrue(FixtureB.Compile(Engine)));

		const FString AbsoluteFilenameA = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), FixtureA.Filename);
		const FString AbsoluteFilenameB = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), FixtureB.Filename);

		TSharedPtr<FAngelscriptModuleDesc> ModuleA =
			Engine.GetModuleByFilenameOrModuleName(AbsoluteFilenameA, FixtureA.ModuleName.ToString());
		TSharedPtr<FAngelscriptModuleDesc> ModuleB =
			Engine.GetModuleByFilenameOrModuleName(AbsoluteFilenameB, FixtureB.ModuleName.ToString());

		ASSERT_THAT(IsTrue(ModuleA.IsValid()));
		ASSERT_THAT(IsTrue(ModuleB.IsValid()));

		TestRunner->TestEqual(
			TEXT("Debugger.Shared.FixtureIdentity.IsolatedPerInstance should keep fixture A mapped to its own module"),
			ModuleA->ModuleName,
			FixtureA.ModuleName.ToString());
		TestRunner->TestEqual(
			TEXT("Debugger.Shared.FixtureIdentity.IsolatedPerInstance should keep fixture B mapped to its own module"),
			ModuleB->ModuleName,
			FixtureB.ModuleName.ToString());

		int32 ResultA = 0;
		int32 ResultB = 0;
		ASSERT_THAT(IsTrue(ExecuteIntFunction(&Engine, AbsoluteFilenameA, FixtureA.ModuleName, FixtureA.EntryFunctionDeclaration, ResultA)));
		ASSERT_THAT(IsTrue(ExecuteIntFunction(&Engine, AbsoluteFilenameB, FixtureB.ModuleName, FixtureB.EntryFunctionDeclaration, ResultB)));

		TestRunner->TestEqual(
			TEXT("Debugger.Shared.FixtureIdentity.IsolatedPerInstance should preserve fixture A's distinct stored value"),
			ResultA,
			8);
		TestRunner->TestEqual(
			TEXT("Debugger.Shared.FixtureIdentity.IsolatedPerInstance should preserve fixture B's distinct stored value"),
			ResultB,
			12);

		Engine.DiscardModule(*FixtureA.ModuleName.ToString());
		TestRunner->TestFalse(
			TEXT("Debugger.Shared.FixtureIdentity.IsolatedPerInstance should invalidate fixture A lookup after discarding only A"),
			Engine.GetModuleByFilenameOrModuleName(AbsoluteFilenameA, FixtureA.ModuleName.ToString()).IsValid());

		TSharedPtr<FAngelscriptModuleDesc> ModuleBAfterDiscard =
			Engine.GetModuleByFilenameOrModuleName(AbsoluteFilenameB, FixtureB.ModuleName.ToString());
		ASSERT_THAT(IsTrue(ModuleBAfterDiscard.IsValid()));

		int32 ResultBAfterDiscard = 0;
		ASSERT_THAT(IsTrue(ExecuteIntFunction(&Engine, AbsoluteFilenameB, FixtureB.ModuleName, FixtureB.EntryFunctionDeclaration, ResultBAfterDiscard)));

		TestRunner->TestEqual(
			TEXT("Debugger.Shared.FixtureIdentity.IsolatedPerInstance should keep fixture B's result stable after discarding A"),
			ResultBAfterDiscard,
			12);
	}
};

#endif
