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


TEST_CLASS_WITH_FLAGS(FAngelscriptScriptObjectTypeTests,
	"Angelscript.TestModule.Engine.ObjectModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
struct FScriptObjectTypeFixture
{
	FName ModuleName;
	FName GeneratedClassName;
	const TCHAR* Filename;
	const TCHAR* ScriptSource;
};

static UASClass* CompileGeneratedObjectClass(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	bool& bPassed,
	const FScriptObjectTypeFixture& Fixture)
{
	FNoDiscardAsserter LocalAssert(Test);
	bPassed &= LocalAssert.IsTrue(
		CompileAnnotatedModuleFromMemory(&Engine, Fixture.ModuleName, Fixture.Filename, Fixture.ScriptSource),
		*FString::Printf(TEXT("%s should compile the annotated script object module"), *Fixture.ModuleName.ToString()));

	UClass* GeneratedClass = FindGeneratedClass(&Engine, Fixture.GeneratedClassName);
	bPassed &= LocalAssert.IsNotNull(
		GeneratedClass,
		*FString::Printf(TEXT("%s should resolve the generated class"), *Fixture.GeneratedClassName.ToString()));

	UASClass* GeneratedASClass = Cast<UASClass>(GeneratedClass);
	bPassed &= LocalAssert.IsNotNull(
		GeneratedASClass,
		*FString::Printf(TEXT("%s should resolve as a generated UASClass"), *Fixture.GeneratedClassName.ToString()));

	if (GeneratedASClass != nullptr)
	{
		bPassed &= LocalAssert.IsNotNull(
			reinterpret_cast<asITypeInfo*>(GeneratedASClass->ScriptTypePtr),
			*FString::Printf(TEXT("%s should publish a non-null ScriptTypePtr"), *Fixture.GeneratedClassName.ToString()));
	}

	return GeneratedASClass;
}

public:
	TEST_METHOD(ScriptObjectGetObjectTypeMatchesGeneratedASClass)
	{
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
		ASSERT_THAT(IsNotNull(FirstScriptObject, TEXT("Script object type test should instantiate the first generated UObject")));

		asIScriptObject* FirstScriptInterface = FAngelscriptEngine::UObjectToAngelscript(FirstScriptObject);
		ASSERT_THAT(IsNotNull(FirstScriptInterface, TEXT("Script object type test should expose the generated UObject through the script-object view")));

		asITypeInfo* FirstObjectType = FirstScriptInterface->GetObjectType();
		asITypeInfo* FirstExpectedType = reinterpret_cast<asITypeInfo*>(FirstASClass->ScriptTypePtr);
		ASSERT_THAT(IsNotNull(FirstObjectType, TEXT("Script object type test should return a script type for the generated UObject instance")));
		if (FirstObjectType != nullptr && FirstExpectedType != nullptr)
		{
			ASSERT_THAT(IsTrue(
				FirstObjectType == FirstExpectedType,
				TEXT("Script object type test should map the generated UObject instance to the owning UASClass ScriptTypePtr")));
			ASSERT_THAT(AreEqual(
				FirstFixture.GeneratedClassName.ToString(),
				FString(UTF8_TO_TCHAR(FirstObjectType->GetName())),
				TEXT("Script object type test should preserve the generated class name in the returned type info")));
		}

		UObject* NativeObject = NewObject<UAngelscriptNativeScriptTestObject>(GetTransientPackage());
		ASSERT_THAT(IsNotNull(NativeObject, TEXT("Script object type test should instantiate a native UObject control case")));

		asIScriptObject* NativeScriptView = FAngelscriptEngine::UObjectToAngelscript(NativeObject);
		ASSERT_THAT(IsNotNull(NativeScriptView, TEXT("Script object type test should expose the native UObject through the script-object view")));

		ASSERT_THAT(IsTrue(
			NativeScriptView->GetObjectType() == nullptr,
			TEXT("Script object type test should not report a script type for a native UObject control case")));

		FirstScriptObject = nullptr;
		NativeObject = nullptr;
		CollectGarbage(RF_NoFlags, true);

		{
			FAngelscriptEngineScope Scope(Engine);
			ASSERT_THAT(IsTrue(
				Engine.DiscardModule(*FirstFixture.ModuleName.ToString()),
				TEXT("Script object type test should discard the first generated module before compiling the next epoch")));
		}
		CollectGarbage(RF_NoFlags, true);

		UASClass* SecondASClass = CompileGeneratedObjectClass(*TestRunner, Engine, bPassed, SecondFixture);
		if (!bPassed || SecondASClass == nullptr)
		{
			return;
		}

		UObject* SecondScriptObject = NewObject<UObject>(GetTransientPackage(), SecondASClass);
		ASSERT_THAT(IsNotNull(SecondScriptObject, TEXT("Script object type test should instantiate the recompiled generated UObject")));

		asIScriptObject* SecondScriptInterface = FAngelscriptEngine::UObjectToAngelscript(SecondScriptObject);
		ASSERT_THAT(IsNotNull(SecondScriptInterface, TEXT("Script object type test should expose the recompiled UObject through the script-object view")));

		asITypeInfo* SecondObjectType = SecondScriptInterface->GetObjectType();
		asITypeInfo* SecondExpectedType = reinterpret_cast<asITypeInfo*>(SecondASClass->ScriptTypePtr);
		ASSERT_THAT(IsNotNull(SecondObjectType, TEXT("Script object type test should return a script type for the recompiled generated UObject instance")));
		if (SecondObjectType != nullptr && SecondExpectedType != nullptr)
		{
			ASSERT_THAT(IsTrue(
				SecondObjectType == SecondExpectedType,
				TEXT("Script object type test should map the recompiled UObject instance to the current UASClass ScriptTypePtr")));
			ASSERT_THAT(IsTrue(
				SecondObjectType != FirstObjectType,
				TEXT("Script object type test should return the current epoch type info instead of the discarded pointer")));
			ASSERT_THAT(AreEqual(
				SecondFixture.GeneratedClassName.ToString(),
				FString(UTF8_TO_TCHAR(SecondObjectType->GetName())),
				TEXT("Script object type test should preserve the recompiled generated class name in the returned type info")));
		}

		}
	}
};

#endif
