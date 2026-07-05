#include "AngelscriptEngine.h"
#include "AngelscriptTestMacros.h"
#include "ClassGenerator/AngelscriptClassGenerator.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

// Test Layer: Runtime Integration
#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptClassGeneratorTests,
	"Angelscript.TestModule.ClassGenerator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(EmptyModuleSetup)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		TSharedRef<FAngelscriptModuleDesc> Module = MakeShared<FAngelscriptModuleDesc>();
		Module->ModuleName = TEXT("Tests.ClassGenerator.EmptyModule");
		Module->ScriptModule = static_cast<asCModule*>(Engine.GetScriptEngine()->GetModule("Tests.ClassGenerator.EmptyModule", asGM_ALWAYS_CREATE));
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
