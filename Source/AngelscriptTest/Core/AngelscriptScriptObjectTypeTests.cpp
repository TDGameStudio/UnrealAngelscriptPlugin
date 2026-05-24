#include "AngelscriptEngine.h"
#include "AngelscriptTestUtilities.h"
#include "ClassGenerator/ASClass.h"
#include "AngelscriptNativeScriptTestObject.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "UObject/UObjectGlobals.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_Core_AngelscriptScriptObjectTypeTests_Private
{
	struct FScriptObjectTypeFixture
	{
		FName ModuleName;
		FName GeneratedClassName;
		const TCHAR* Filename;
		const TCHAR* ScriptSource;
	};

	UASClass* CompileGeneratedObjectClass(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		bool& bPassed,
		const FScriptObjectTypeFixture& Fixture)
	{
		bPassed &= Test.TestTrue(
			*FString::Printf(TEXT("%s should compile the annotated script object module"), *Fixture.ModuleName.ToString()),
			CompileAnnotatedModuleFromMemory(&Engine, Fixture.ModuleName, Fixture.Filename, Fixture.ScriptSource));

		UClass* GeneratedClass = FindGeneratedClass(&Engine, Fixture.GeneratedClassName);
		bPassed &= Test.TestNotNull(
			*FString::Printf(TEXT("%s should resolve the generated class"), *Fixture.GeneratedClassName.ToString()),
			GeneratedClass);

		UASClass* GeneratedASClass = Cast<UASClass>(GeneratedClass);
		bPassed &= Test.TestNotNull(
			*FString::Printf(TEXT("%s should resolve as a generated UASClass"), *Fixture.GeneratedClassName.ToString()),
			GeneratedASClass);

		if (GeneratedASClass != nullptr)
		{
			bPassed &= Test.TestNotNull(
				*FString::Printf(TEXT("%s should publish a non-null ScriptTypePtr"), *Fixture.GeneratedClassName.ToString()),
				reinterpret_cast<asITypeInfo*>(GeneratedASClass->ScriptTypePtr));
		}

		return GeneratedASClass;
	}
}


TEST_CLASS_WITH_FLAGS(FAngelscriptScriptObjectTypeTests,
	"Angelscript.TestModule.Engine.ObjectModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ScriptObjectGetObjectTypeMatchesGeneratedASClass)
	{
		using namespace AngelscriptTest_Core_AngelscriptScriptObjectTypeTests_Private;
		bool bPassed = true;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		{
			FAngelscriptEngineScope _AutoEngineScope(Engine);
			ON_SCOPE_EXIT
			{
				const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
				for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
				{
					Engine.DiscardModule(*_Module->ModuleName);
				}
			};

		static const FScriptObjectTypeFixture FirstFixture = {
			TEXT("ObjectTypeProbeA"),
			TEXT("UObjectTypeProbeObjectA"),
			TEXT("ObjectTypeProbeA.as"),
			TEXT(R"(
UCLASS()
class UObjectTypeProbeObjectA : UObject
{
	UPROPERTY()
	int Value = 7;

	UFUNCTION()
	int GetValue()
	{
		return Value;
	}
}
)"),
		};

		static const FScriptObjectTypeFixture SecondFixture = {
			TEXT("ObjectTypeProbeB"),
			TEXT("UObjectTypeProbeObjectB"),
			TEXT("ObjectTypeProbeB.as"),
			TEXT(R"(
UCLASS()
class UObjectTypeProbeObjectB : UObject
{
	UPROPERTY()
	int Value = 11;

	UFUNCTION()
	int GetValue()
	{
		return Value;
	}
}
)"),
		};

		UASClass* FirstASClass = CompileGeneratedObjectClass(*TestRunner, Engine, bPassed, FirstFixture);
		if (!bPassed || FirstASClass == nullptr)
		{
			return;
		}

		UObject* FirstScriptObject = NewObject<UObject>(GetTransientPackage(), FirstASClass);
		if (!TestRunner->TestNotNull(TEXT("Script object type test should instantiate the first generated UObject"), FirstScriptObject))
		{
			return;
		}

		asIScriptObject* FirstScriptInterface = FAngelscriptEngine::UObjectToAngelscript(FirstScriptObject);
		if (!TestRunner->TestNotNull(TEXT("Script object type test should expose the generated UObject through the script-object view"), FirstScriptInterface))
		{
			return;
		}

		asITypeInfo* FirstObjectType = FirstScriptInterface->GetObjectType();
		asITypeInfo* FirstExpectedType = reinterpret_cast<asITypeInfo*>(FirstASClass->ScriptTypePtr);
		TestRunner->TestNotNull(TEXT("Script object type test should return a script type for the generated UObject instance"), FirstObjectType);
		if (FirstObjectType != nullptr && FirstExpectedType != nullptr)
		{
			TestRunner->TestTrue(
				TEXT("Script object type test should map the generated UObject instance to the owning UASClass ScriptTypePtr"),
				FirstObjectType == FirstExpectedType);
			TestRunner->TestEqual(
				TEXT("Script object type test should preserve the generated class name in the returned type info"),
				FString(UTF8_TO_TCHAR(FirstObjectType->GetName())),
				FirstFixture.GeneratedClassName.ToString());
		}

		UObject* NativeObject = NewObject<UAngelscriptNativeScriptTestObject>(GetTransientPackage());
		if (!TestRunner->TestNotNull(TEXT("Script object type test should instantiate a native UObject control case"), NativeObject))
		{
			return;
		}

		asIScriptObject* NativeScriptView = FAngelscriptEngine::UObjectToAngelscript(NativeObject);
		if (!TestRunner->TestNotNull(TEXT("Script object type test should expose the native UObject through the script-object view"), NativeScriptView))
		{
			return;
		}

		TestRunner->TestTrue(
			TEXT("Script object type test should not report a script type for a native UObject control case"),
			NativeScriptView->GetObjectType() == nullptr);

		FirstScriptObject = nullptr;
		NativeObject = nullptr;
		CollectGarbage(RF_NoFlags, true);

		{
			FAngelscriptEngineScope Scope(Engine);
			TestRunner->TestTrue(
				TEXT("Script object type test should discard the first generated module before compiling the next epoch"),
				Engine.DiscardModule(*FirstFixture.ModuleName.ToString()));
		}
		CollectGarbage(RF_NoFlags, true);

		UASClass* SecondASClass = CompileGeneratedObjectClass(*TestRunner, Engine, bPassed, SecondFixture);
		if (!bPassed || SecondASClass == nullptr)
		{
			return;
		}

		UObject* SecondScriptObject = NewObject<UObject>(GetTransientPackage(), SecondASClass);
		if (!TestRunner->TestNotNull(TEXT("Script object type test should instantiate the recompiled generated UObject"), SecondScriptObject))
		{
			return;
		}

		asIScriptObject* SecondScriptInterface = FAngelscriptEngine::UObjectToAngelscript(SecondScriptObject);
		if (!TestRunner->TestNotNull(TEXT("Script object type test should expose the recompiled UObject through the script-object view"), SecondScriptInterface))
		{
			return;
		}

		asITypeInfo* SecondObjectType = SecondScriptInterface->GetObjectType();
		asITypeInfo* SecondExpectedType = reinterpret_cast<asITypeInfo*>(SecondASClass->ScriptTypePtr);
		TestRunner->TestNotNull(TEXT("Script object type test should return a script type for the recompiled generated UObject instance"), SecondObjectType);
		if (SecondObjectType != nullptr && SecondExpectedType != nullptr)
		{
			TestRunner->TestTrue(
				TEXT("Script object type test should map the recompiled UObject instance to the current UASClass ScriptTypePtr"),
				SecondObjectType == SecondExpectedType);
			TestRunner->TestTrue(
				TEXT("Script object type test should return the current epoch type info instead of the discarded pointer"),
				SecondObjectType != FirstObjectType);
			TestRunner->TestEqual(
				TEXT("Script object type test should preserve the recompiled generated class name in the returned type info"),
				FString(UTF8_TO_TCHAR(SecondObjectType->GetName())),
				SecondFixture.GeneratedClassName.ToString());
		}

		}
	}
};

#endif
