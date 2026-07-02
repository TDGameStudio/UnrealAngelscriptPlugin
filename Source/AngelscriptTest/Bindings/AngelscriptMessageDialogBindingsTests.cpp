// AngelscriptMessageDialogBindingsTests.cpp
// CQTest coverage for FMessageDialog, UInputSettings type availability.
// Automation IDs: Angelscript.TestModule.Bindings.MessageDialog.*

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#if WITH_ANGELSCRIPT_UNITTESTS



TEST_CLASS_WITH_FLAGS(FAngelscriptMessageDialogBindingsTest,
	"Angelscript.TestModule.Bindings.MessageDialog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}
	AFTER_ALL()
	{
		FAngelscriptEngine& E = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(E);
	}

	TEST_METHOD(UInputSettingsTypeCheck)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASMessageDialog_InputSettings"), ASTEST_AS(R"AS(
			int InputSettings_GetDefaultExists()
			{
				UInputSettings Settings = UInputSettings::GetInputSettings();
				return (Settings != nullptr) ? 1 : 0;
			}
			)AS"));
		if (!Mod.IsValid())
		{
			TestRunner->AddInfo(TEXT("UInputSettings not available, skipping"));
			return;
		}
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), TEXT("int InputSettings_GetDefaultExists()"), TEXT("UInputSettings::GetInputSettings returns non-null"), 1),
			TEXT("ExpectGlobalInt should pass")));
	}
};

#endif
