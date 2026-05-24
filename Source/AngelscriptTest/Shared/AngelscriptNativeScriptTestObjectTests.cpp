#include "AngelscriptNativeScriptTestObject.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptNativeScriptTestObjectInstantiationTest,
	"Angelscript.TestModule.Shared.NativeScriptTestObject.Instantiate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptNativeScriptTestObjectInstantiationTest::RunTest(const FString& Parameters)
{
	UAngelscriptNativeScriptTestObject* Object = NewObject<UAngelscriptNativeScriptTestObject>(GetTransientPackage());
	if (!TestNotNull(TEXT("Native script test object should instantiate"), Object))
	{
		return false;
	}

	TestEqual(TEXT("NativeNoArgValue returns the expected native constant"), Object->NativeNoArgValue(), 42);
	return true;
}

#endif
