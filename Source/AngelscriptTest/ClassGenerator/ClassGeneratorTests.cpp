#include "AngelscriptEngine.h"
#include "AngelscriptTestUtilities.h"
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
		ASSERT_THAT(IsNotNull(Engine, TEXT("ClassGenerator test should have an initialized engine")));
		FAngelscriptEngineScope EngineScope(*Engine);

		TSharedRef<FAngelscriptModuleDesc> Module = MakeShared<FAngelscriptModuleDesc>();
		Module->ModuleName = TEXT("Tests.ClassGenerator.EmptyModule");
		Module->ScriptModule = static_cast<asCModule*>(Engine->GetScriptEngine()->GetModule("Tests.ClassGenerator.EmptyModule", asGM_ALWAYS_CREATE));
		ASSERT_THAT(IsNotNull(Module->ScriptModule, TEXT("ClassGenerator scaffold should create a backing script module")));

		FAngelscriptClassGenerator Generator;
		Generator.AddModule(Module);

		const FAngelscriptClassGenerator::EReloadRequirement ReloadRequirement = Generator.Setup();
		ASSERT_THAT(AreEqual(
			FAngelscriptClassGenerator::EReloadRequirement::SoftReload,
			ReloadRequirement,
			TEXT("An empty module should default to soft reload requirements")));
		ASSERT_THAT(IsFalse(Generator.WantsFullReload(Module), TEXT("An empty module should not request a suggested full reload")));
		ASSERT_THAT(IsFalse(Generator.NeedsFullReload(Module), TEXT("An empty module should not require a full reload")));
	}
};

#endif
