#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "ClassGenerator/ASClass.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtrTemplates.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

namespace ASClassObjectConstructionTest
{
	static const FName ModuleName(TEXT("ASClassObjectConstruction"));
	static const FString ScriptFilename(TEXT("ASClassObjectConstruction.as"));
	static const FName GeneratedClassName(TEXT("UObjectConstructionCarrier"));
	static const FName CtorCountPropertyName(TEXT("CtorCount"));
	static const FName DefaultValuePropertyName(TEXT("DefaultValue"));
	static const FName DefaultLabelPropertyName(TEXT("DefaultLabel"));
	static const FString ExpectedDefaultLabel(TEXT("ObjectDefaults"));
	static constexpr int32 ExpectedCtorCount = 1;
	static constexpr int32 ExpectedDefaultValue = 7;

	struct FObjectConstructionSnapshot
	{
		int32 CtorCount = INDEX_NONE;
		int32 DefaultValue = INDEX_NONE;
		FString DefaultLabel;
	};

	UASClass* CompileObjectConstructionCarrier(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		const FString ScriptSource = TEXT(R"AS(
UCLASS()
class UObjectConstructionCarrier : UObject
{
	UPROPERTY()
	int CtorCount = 0;

	UPROPERTY()
	int DefaultValue = 0;

	UPROPERTY()
	FString DefaultLabel;

	UObjectConstructionCarrier()
	{
		CtorCount += 1;
	}

	default DefaultValue = 7;
	default DefaultLabel = "ObjectDefaults";
}
)AS");

		UClass* GeneratedClass = CompileScriptModule(
			Test,
			Engine,
			ModuleName,
			ScriptFilename,
			ScriptSource,
			GeneratedClassName);
		if (GeneratedClass == nullptr)
		{
			return nullptr;
		}

		UASClass* GeneratedASClass = Cast<UASClass>(GeneratedClass);
		if (!Test.TestNotNull(
				TEXT("ASClass object-construction test case should compile the carrier into a UASClass"),
				GeneratedASClass))
		{
			return nullptr;
		}

		Test.TestNotNull(
			TEXT("ASClass object-construction test case should bind the script constructor function"),
			GeneratedASClass->ConstructFunction);
		Test.TestNotNull(
			TEXT("ASClass object-construction test case should bind the defaults function"),
			GeneratedASClass->DefaultsFunction);
		Test.TestNotNull(
			TEXT("ASClass object-construction test case should keep a live script type pointer"),
			GeneratedASClass->ScriptTypePtr);

		return GeneratedASClass;
	}

	bool ReadConstructionSnapshot(
		FAutomationTestBase& Test,
		UObject* Object,
		FObjectConstructionSnapshot& OutSnapshot)
	{
		if (!ReadPropertyValue<FIntProperty>(Test, Object, CtorCountPropertyName, OutSnapshot.CtorCount))
		{
			return false;
		}

		if (!ReadPropertyValue<FIntProperty>(Test, Object, DefaultValuePropertyName, OutSnapshot.DefaultValue))
		{
			return false;
		}

		if (!ReadPropertyValue<FStrProperty>(Test, Object, DefaultLabelPropertyName, OutSnapshot.DefaultLabel))
		{
			return false;
		}

		return true;
	}

	bool VerifySnapshot(
		FAutomationTestBase& Test,
		const FString& ScopeLabel,
		const FObjectConstructionSnapshot& Snapshot,
		int32 ExpectedCtorCountForScope)
	{
		const bool bCtorCountMatches = Test.TestEqual(
			*FString::Printf(TEXT("%s should observe the expected constructor count"), *ScopeLabel),
			Snapshot.CtorCount,
			ExpectedCtorCountForScope);
		const bool bDefaultValueMatches = Test.TestEqual(
			*FString::Printf(TEXT("%s should preserve the scripted integer default"), *ScopeLabel),
			Snapshot.DefaultValue,
			ExpectedDefaultValue);
		const bool bDefaultLabelMatches = Test.TestEqual(
			*FString::Printf(TEXT("%s should preserve the scripted string default"), *ScopeLabel),
			Snapshot.DefaultLabel,
			ExpectedDefaultLabel);

		return bCtorCountMatches && bDefaultValueMatches && bDefaultLabelMatches;
	}

	void ReleaseObject(TWeakObjectPtr<UObject>& WeakObject)
	{
		if (!WeakObject.IsValid())
		{
			return;
		}

		WeakObject->RemoveFromRoot();
		WeakObject->MarkAsGarbage();
		WeakObject = nullptr;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptASClassObjectConstructionTests,
	"Angelscript.TestModule.ClassGenerator.ASClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(StaticObjectConstructorAppliesScriptConstructorAndDefaultsOnce)
	{
		using namespace ASClassObjectConstructionTest;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ASClassObjectConstructionTest::ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
			CollectGarbage(RF_NoFlags, true);
		};

		UASClass* GeneratedASClass = ASClassObjectConstructionTest::CompileObjectConstructionCarrier(*TestRunner, Engine);
		if (GeneratedASClass == nullptr)
		{
			return;
		}

		UObject* DefaultObject = GeneratedASClass->GetDefaultObject();
		if (!TestRunner->TestNotNull(
				TEXT("ASClass object-construction test case should expose a generated class default object"),
				DefaultObject))
		{
			return;
		}

		ASClassObjectConstructionTest::FObjectConstructionSnapshot DefaultSnapshot;
		if (!ASClassObjectConstructionTest::ReadConstructionSnapshot(*TestRunner, DefaultObject, DefaultSnapshot))
		{
			return;
		}

		UObject* FirstInstance = NewObject<UObject>(GetTransientPackage(), GeneratedASClass, TEXT("ObjectConstructionCarrierA"));
		UObject* SecondInstance = NewObject<UObject>(GetTransientPackage(), GeneratedASClass, TEXT("ObjectConstructionCarrierB"));
		if (!TestRunner->TestNotNull(
				TEXT("ASClass object-construction test case should create the first generated UObject instance"),
				FirstInstance)
			|| !TestRunner->TestNotNull(
				TEXT("ASClass object-construction test case should create the second generated UObject instance"),
				SecondInstance))
		{
			return;
		}

		FirstInstance->AddToRoot();
		SecondInstance->AddToRoot();

		TWeakObjectPtr<UObject> WeakFirstInstance = FirstInstance;
		TWeakObjectPtr<UObject> WeakSecondInstance = SecondInstance;
		ON_SCOPE_EXIT
		{
			ASClassObjectConstructionTest::ReleaseObject(WeakSecondInstance);
			ASClassObjectConstructionTest::ReleaseObject(WeakFirstInstance);
		};

		ASClassObjectConstructionTest::FObjectConstructionSnapshot FirstSnapshot;
		ASClassObjectConstructionTest::FObjectConstructionSnapshot SecondSnapshot;
		if (!ASClassObjectConstructionTest::ReadConstructionSnapshot(*TestRunner, FirstInstance, FirstSnapshot)
			|| !ASClassObjectConstructionTest::ReadConstructionSnapshot(*TestRunner, SecondInstance, SecondSnapshot))
		{
			return;
		}

		TestRunner->TestTrue(
			TEXT("ASClass object-construction test case should compile a plain UObject-generated class"),
			GeneratedASClass->IsChildOf(UObject::StaticClass()));
		TestRunner->TestFalse(
			TEXT("ASClass object-construction test case should keep the generated class out of the actor hierarchy"),
			GeneratedASClass->IsChildOf(AActor::StaticClass()));
		TestRunner->TestTrue(
			TEXT("ASClass object-construction test case should create distinct runtime instances"),
			FirstInstance != SecondInstance);
		TestRunner->TestTrue(
			TEXT("ASClass object-construction test case should keep runtime instances distinct from the class default object"),
			FirstInstance != DefaultObject && SecondInstance != DefaultObject);

		ASClassObjectConstructionTest::VerifySnapshot(
			*TestRunner,
			TEXT("ASClass object-construction test case class default object"),
			DefaultSnapshot,
			ASClassObjectConstructionTest::ExpectedCtorCount);
		ASClassObjectConstructionTest::VerifySnapshot(
			*TestRunner,
			TEXT("ASClass object-construction test case first instance"),
			FirstSnapshot,
			ASClassObjectConstructionTest::ExpectedCtorCount);
		ASClassObjectConstructionTest::VerifySnapshot(
			*TestRunner,
			TEXT("ASClass object-construction test case second instance"),
			SecondSnapshot,
			ASClassObjectConstructionTest::ExpectedCtorCount);

		TestRunner->TestEqual(
			TEXT("ASClass object-construction test case should keep the second instance constructor count isolated from the first instance"),
			SecondSnapshot.CtorCount,
			ASClassObjectConstructionTest::ExpectedCtorCount);
		TestRunner->TestEqual(
			TEXT("ASClass object-construction test case should keep both runtime instances on the same scripted integer default"),
			FirstSnapshot.DefaultValue,
			SecondSnapshot.DefaultValue);
		TestRunner->TestEqual(
			TEXT("ASClass object-construction test case should keep both runtime instances on the same scripted string default"),
			FirstSnapshot.DefaultLabel,
			SecondSnapshot.DefaultLabel);

		}
	}
};

#endif
