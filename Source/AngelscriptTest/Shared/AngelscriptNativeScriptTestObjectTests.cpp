#include "AngelscriptNativeScriptTestObject.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(
	FAngelscriptNativeScriptTestObjectInstantiationTest,
	"Angelscript.TestModule.Shared.NativeScriptTestObject",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(Instantiate)
	{
		UAngelscriptNativeScriptTestObject* Object = NewObject<UAngelscriptNativeScriptTestObject>(GetTransientPackage());
		ASSERT_THAT(IsNotNull(Object));

		ASSERT_THAT(AreEqual(42, Object->NativeNoArgValue()));
	}
};

#endif
