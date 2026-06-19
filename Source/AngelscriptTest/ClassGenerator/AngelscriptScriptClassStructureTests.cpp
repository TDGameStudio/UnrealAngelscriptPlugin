#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "ClassGenerator/ASClass.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

namespace ScriptClassStructureTests
{
	int32 CountDeclaredProperties(const UClass& ScriptClass)
	{
		int32 PropertyCount = 0;
		for (TFieldIterator<FProperty> It(&ScriptClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			++PropertyCount;
		}

		return PropertyCount;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptScriptClassStructureTests,
	"Angelscript.TestModule.ScriptClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(FunctionOnlyClassCompilesAndExecutes)
	{
		using namespace ScriptClassStructureTests;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		static const FName ModuleName(TEXT("ASFunctionOnlyScriptClassStructure"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		static const FString ScriptSource = TEXT(R"AS(
UCLASS()
class UFunctionOnlyScriptClass : UObject
{
	UFUNCTION()
	int GetValue()
	{
		return 17;
	}
}
)AS");

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("FunctionOnlyScriptClass.as"),
			ScriptSource,
			TEXT("UFunctionOnlyScriptClass"));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UASClass* ASClass = Cast<UASClass>(ScriptClass);
		ASSERT_THAT(IsNotNull(ASClass, TEXT("Function-only script class test case should generate a UASClass")));

		ASSERT_THAT(IsTrue(ScriptClass->IsChildOf(UObject::StaticClass()), TEXT("Function-only script class test case should remain UObject-derived")));
		ASSERT_THAT(AreEqual(0, ScriptClassStructureTests::CountDeclaredProperties(*ScriptClass), TEXT("Function-only script class test case should not synthesize any declared user properties")));
		ASSERT_THAT(IsNull(FindFProperty<FProperty>(ScriptClass, TEXT("UnexpectedProperty")), TEXT("Function-only script class test case should not expose undeclared properties")));

		UFunction* GetValueFunction = FindGeneratedFunction(ScriptClass, TEXT("GetValue"));
		ASSERT_THAT(IsNotNull(GetValueFunction, TEXT("Function-only script class test case should generate GetValue")));

		UObject* Instance = NewObject<UObject>(GetTransientPackage(), ScriptClass);
		ASSERT_THAT(IsNotNull(Instance, TEXT("Function-only script class test case should instantiate the generated class")));

		int32 Result = 0;
		ASSERT_THAT(IsTrue(
			ExecuteGeneratedIntEventOnGameThread(&Engine, Instance, GetValueFunction, Result),
			TEXT("Function-only script class test case should execute GetValue on the game thread")));

		ASSERT_THAT(AreEqual(17, Result, TEXT("Function-only script class test case should keep GetValue returning 17")));

		}
	}
};

#endif
