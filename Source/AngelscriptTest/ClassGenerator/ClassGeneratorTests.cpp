#include "AngelscriptEngine.h"
#include "Shared/AngelscriptTestUtilities.h"
#include "ClassGenerator/AngelscriptClassGenerator.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_ClassGenerator_ClassGeneratorTests_Private
{
	FAngelscriptEngine* GetEngineForClassGeneratorTests(FAutomationTestBase* Test)
	{
		if (FAngelscriptEngine* ProductionEngine = TryGetRunningProductionEngine())
		{
			return ProductionEngine;
		}

		return &GetOrCreateSharedCloneEngine();
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptClassGeneratorTests,
	"Angelscript.TestModule.ClassGenerator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(EmptyModuleSetup)
	{
		using namespace AngelscriptTest_ClassGenerator_ClassGeneratorTests_Private;
		FAngelscriptEngine* Engine = GetEngineForClassGeneratorTests(TestRunner);
		if (!TestRunner->TestNotNull(TEXT("ClassGenerator test should have an initialized engine"), Engine))
		{
			return;
		}
		FAngelscriptEngineScope EngineScope(*Engine);

		TSharedRef<FAngelscriptModuleDesc> Module = MakeShared<FAngelscriptModuleDesc>();
		Module->ModuleName = TEXT("Tests.ClassGenerator.EmptyModule");
		Module->ScriptModule = static_cast<asCModule*>(Engine->GetScriptEngine()->GetModule("Tests.ClassGenerator.EmptyModule", asGM_ALWAYS_CREATE));
		if (!TestRunner->TestNotNull(TEXT("ClassGenerator scaffold should create a backing script module"), Module->ScriptModule))
		{
			return;
		}

		FAngelscriptClassGenerator Generator;
		Generator.AddModule(Module);

		const FAngelscriptClassGenerator::EReloadRequirement ReloadRequirement = Generator.Setup();
		TestRunner->TestEqual(TEXT("An empty module should default to soft reload requirements"), ReloadRequirement, FAngelscriptClassGenerator::EReloadRequirement::SoftReload);
		TestRunner->TestFalse(TEXT("An empty module should not request a suggested full reload"), Generator.WantsFullReload(Module));
		TestRunner->TestFalse(TEXT("An empty module should not require a full reload"), Generator.NeedsFullReload(Module));
	}
};

#endif
