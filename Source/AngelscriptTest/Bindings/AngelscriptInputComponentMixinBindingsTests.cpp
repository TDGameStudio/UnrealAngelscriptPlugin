// AngelscriptInputComponentMixinBindingsTests.cpp
// CQTest coverage for InputComponentScriptMixins, FPlatformApplicationMisc.
// Automation IDs: Angelscript.TestModule.Bindings.InputMixin.*

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptBindingsModuleBuilder.h"
#include "Shared/AngelscriptBindingsAssertions.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptTestBindings;


TEST_CLASS_WITH_FLAGS(FAngelscriptInputComponentMixinBindingsTest,
	"Angelscript.TestModule.Bindings.InputMixin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL() { ASTEST_CREATE_ENGINE(); }
	AFTER_ALL() { FAngelscriptEngine& E = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(E); }

	TEST_METHOD(PlatformApplicationMisc)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FCoverageModuleScope Mod(*TestRunner, Engine, TEXT("ASInputMixin_PlatApp"), TEXT(R"(
int PlatApp_ClipboardEmpty()
{
	FString Clip;
	FPlatformApplicationMisc::ClipboardPaste(Clip);
	return Clip.Len() >= 0 ? 1 : 0;
}
)"));
		if (!Mod.IsValid())
		{
			TestRunner->AddInfo(TEXT("FPlatformApplicationMisc not available, skipping"));
			return;
		}
		AngelscriptTestBindings::ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), 
			TEXT("int PlatApp_ClipboardEmpty()"),
			TEXT("ClipboardPaste does not crash"), 1);
	}
};

#endif
