#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "ClassGenerator/ASClass.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptScriptClassStructureTests,
	"Angelscript.TestModule.ScriptClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static int32 CountDeclaredProperties(const UClass& ScriptClass)
	{
		int32 PropertyCount = 0;
		for (TFieldIterator<FProperty> It(&ScriptClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			++PropertyCount;
		}

		return PropertyCount;
	}
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

	TEST_METHOD(FunctionOnlyClassCompilesAndExecutes)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		static const FName ModuleName(TEXT("ASFunctionOnlyScriptClassStructure"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		static const FString ScriptSource = ASTEST_AS(R"AS(
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

		UClass* ScriptClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
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
		ASSERT_THAT(AreEqual(0, FAngelscriptScriptClassStructureTests::CountDeclaredProperties(*ScriptClass), TEXT("Function-only script class test case should not synthesize any declared user properties")));
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

	TEST_METHOD(NamespacedUClassPublishesReflectionArtifacts)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		static const FName ModuleName(TEXT("ASNamespacedScriptClassStructure"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		static const FString ScriptSource = ASTEST_AS(R"AS(
			namespace ScriptClassNamespace
			{
				UCLASS()
				class UNamespacedScriptClass : UObject
				{
					UPROPERTY()
					int StoredValue = 29;

					UFUNCTION()
					int GetStoredValue()
					{
						return StoredValue;
					}
				}
			}
			)AS");

		UClass* ScriptClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("NamespacedScriptClassStructure.as"),
			ScriptSource,
			TEXT("UNamespacedScriptClass"));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UASClass* ASClass = Cast<UASClass>(ScriptClass);
		ASSERT_THAT(IsNotNull(ASClass, TEXT("Namespaced script class test case should generate a UASClass")));
		ASSERT_THAT(IsTrue(ScriptClass->IsChildOf(UObject::StaticClass()), TEXT("Namespaced script class test case should remain UObject-derived")));
		ASSERT_THAT(AreEqual(1, FAngelscriptScriptClassStructureTests::CountDeclaredProperties(*ScriptClass), TEXT("Namespaced script class test case should expose exactly one declared user property")));

		UObject* DefaultObject = ScriptClass->GetDefaultObject();
		ASSERT_THAT(IsNotNull(DefaultObject, TEXT("Namespaced script class test case should publish a class default object")));
		if (DefaultObject == nullptr)
		{
			return;
		}

		int32 DefaultStoredValue = 0;
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(*TestRunner, DefaultObject, TEXT("StoredValue"), DefaultStoredValue))
		{
			return;
		}

		UFunction* GetStoredValueFunction = FindGeneratedFunction(ScriptClass, TEXT("GetStoredValue"));
		ASSERT_THAT(IsNotNull(GetStoredValueFunction, TEXT("Namespaced script class test case should generate GetStoredValue")));
		if (GetStoredValueFunction == nullptr)
		{
			return;
		}

		UObject* Instance = NewObject<UObject>(GetTransientPackage(), ScriptClass);
		ASSERT_THAT(IsNotNull(Instance, TEXT("Namespaced script class test case should instantiate the generated UObject")));
		if (Instance == nullptr)
		{
			return;
		}

		int32 Result = 0;
		ASSERT_THAT(IsTrue(
			ExecuteGeneratedIntEventOnGameThread(&Engine, Instance, GetStoredValueFunction, Result),
			TEXT("Namespaced script class test case should execute GetStoredValue on the generated class")));

		ASSERT_THAT(AreEqual(29, DefaultStoredValue, TEXT("Namespaced script class test case should preserve the reflected property default")));
		ASSERT_THAT(AreEqual(29, Result, TEXT("Namespaced script class test case should execute the namespaced generated method")));
	}
};

#endif
