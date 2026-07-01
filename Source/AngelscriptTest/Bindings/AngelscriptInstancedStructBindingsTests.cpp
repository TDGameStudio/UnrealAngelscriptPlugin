// AngelscriptInstancedStructBindingsTests.cpp
// CQTest coverage for FInstancedStruct binding.
// Automation IDs: Angelscript.TestModule.Bindings.InstancedStruct.*

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#if WITH_ANGELSCRIPT_UNITTESTS



TEST_CLASS_WITH_FLAGS(FAngelscriptInstancedStructBindingsTest,
	"Angelscript.TestModule.Bindings.InstancedStruct",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL() { ASTEST_CREATE_ENGINE(); }
	AFTER_ALL() { FAngelscriptEngine& E = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(E); }

	TEST_METHOD(DefaultConstruction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASInstancedStruct_Default"), TEXT(R"(
int InstancedStruct_DefaultInvalid()
{
	FInstancedStruct S;
	return S.IsValid() ? 0 : 1;
}
)"));
		if (!Mod.IsValid())
		{
			TestRunner->AddInfo(TEXT("FInstancedStruct not available, skipping"));
			return;
		}
		ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), 
			TEXT("int InstancedStruct_DefaultInvalid()"),
			TEXT("Default FInstancedStruct is invalid"), 1);
	}

	TEST_METHOD(ResetClears)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASInstancedStruct_Reset"), TEXT(R"(
int InstancedStruct_ResetMakesInvalid()
{
	FInstancedStruct S;
	S.Reset();
	return S.IsValid() ? 0 : 1;
}
)"));
		if (!Mod.IsValid())
		{
			TestRunner->AddInfo(TEXT("FInstancedStruct not available, skipping"));
			return;
		}
		ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), 
			TEXT("int InstancedStruct_ResetMakesInvalid()"),
			TEXT("Reset FInstancedStruct is invalid"), 1);
	}
};

#endif
