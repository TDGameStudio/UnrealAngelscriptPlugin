// AngelscriptInputBindingsTests.cpp
// CQTest coverage for FInputActionKeyMapping, FInputBindingHandle, InputEvents.
// Automation IDs: Angelscript.TestModule.Bindings.Input.*

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#if WITH_DEV_AUTOMATION_TESTS



TEST_CLASS_WITH_FLAGS(FAngelscriptInputBindingsTest,
	"Angelscript.TestModule.Bindings.Input",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL() { ASTEST_CREATE_ENGINE(); }
	AFTER_ALL() { FAngelscriptEngine& E = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(E); }

	TEST_METHOD(FInputActionValue)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASInput_ActionValue"), TEXT(R"(
int InputActionValue_DefaultZero()
{
	FInputActionValue V;
	return V.IsNonZero() ? 0 : 1;
}
)"));
		if (!Mod.IsValid()) return;
		ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), 
			TEXT("int InputActionValue_DefaultZero()"), TEXT("Default FInputActionValue is zero"), 1);
	}

	TEST_METHOD(FKeyConstruction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASInput_Key"), TEXT(R"(
int Key_IsValid()
{
	FKey K = EKeys::SpaceBar;
	return K.IsValid() ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), 
			TEXT("int Key_IsValid()"), TEXT("SpaceBar key is valid"), 1);
	}
};

#endif
