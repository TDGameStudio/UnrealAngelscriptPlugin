#include "Shared/AngelscriptFunctionalTestUtils.h"
#include "Shared/AngelscriptTestMacros.h"

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
		if (!TestRunner->TestNotNull(TEXT("Function-only script class test case should generate a UASClass"), ASClass))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("Function-only script class test case should remain UObject-derived"), ScriptClass->IsChildOf(UObject::StaticClass()));
		TestRunner->TestEqual(TEXT("Function-only script class test case should not synthesize any declared user properties"), ScriptClassStructureTests::CountDeclaredProperties(*ScriptClass), 0);
		TestRunner->TestNull(TEXT("Function-only script class test case should not expose undeclared properties"), FindFProperty<FProperty>(ScriptClass, TEXT("UnexpectedProperty")));

		UFunction* GetValueFunction = FindGeneratedFunction(ScriptClass, TEXT("GetValue"));
		if (!TestRunner->TestNotNull(TEXT("Function-only script class test case should generate GetValue"), GetValueFunction))
		{
			return;
		}

		UObject* Instance = NewObject<UObject>(GetTransientPackage(), ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Function-only script class test case should instantiate the generated class"), Instance))
		{
			return;
		}

		int32 Result = 0;
		if (!TestRunner->TestTrue(TEXT("Function-only script class test case should execute GetValue on the game thread"), ExecuteGeneratedIntEventOnGameThread(&Engine, Instance, GetValueFunction, Result)))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Function-only script class test case should keep GetValue returning 17"), Result, 17);

		}
	}
};

#endif
