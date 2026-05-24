#include "Shared/AngelscriptFunctionalTestUtils.h"
#include "Shared/AngelscriptTestMacros.h"

#include "CQTest.h"
#include "ClassGenerator/ASClass.h"
#include "Components/ActorComponent.h"
#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtrTemplates.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

namespace ASClassComponentConstructionTest
{
	static const FName ModuleName(TEXT("ASClassComponentConstruction"));
	static const FString ScriptFilename(TEXT("ASClassComponentConstruction.as"));
	static const FName GeneratedClassName(TEXT("UComponentConstructionCarrier"));
	static const FName CtorCountPropertyName(TEXT("CtorCount"));
	static const FName DefaultValuePropertyName(TEXT("DefaultValue"));
	static const FName DefaultLabelPropertyName(TEXT("DefaultLabel"));
	static const FString ExpectedDefaultLabel(TEXT("ComponentDefaults"));
	static constexpr int32 ExpectedCtorCount = 1;
	static constexpr int32 ExpectedDefaultValue = 9;

	struct FComponentConstructionSnapshot
	{
		int32 CtorCount = INDEX_NONE;
		int32 DefaultValue = INDEX_NONE;
		FString DefaultLabel;
	};

	UASClass* CompileComponentConstructionCarrier(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		const FString ScriptSource = TEXT(R"AS(
UCLASS()
class UComponentConstructionCarrier : UActorComponent
{
	UPROPERTY()
	int CtorCount = 0;

	UPROPERTY()
	int DefaultValue = 0;

	UPROPERTY()
	FString DefaultLabel;

	UComponentConstructionCarrier()
	{
		CtorCount += 1;
	}

	default DefaultValue = 9;
	default DefaultLabel = "ComponentDefaults";
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
				TEXT("ASClass component-construction test case should compile the carrier into a UASClass"),
				GeneratedASClass))
		{
			return nullptr;
		}

		Test.TestNotNull(
			TEXT("ASClass component-construction test case should bind the script constructor function"),
			GeneratedASClass->ConstructFunction);
		Test.TestNotNull(
			TEXT("ASClass component-construction test case should bind the defaults function"),
			GeneratedASClass->DefaultsFunction);
		Test.TestNotNull(
			TEXT("ASClass component-construction test case should keep a live script type pointer"),
			GeneratedASClass->ScriptTypePtr);

		return GeneratedASClass;
	}

	bool ReadConstructionSnapshot(
		FAutomationTestBase& Test,
		UObject* Object,
		FComponentConstructionSnapshot& OutSnapshot)
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
		const FComponentConstructionSnapshot& Snapshot,
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

	UActorComponent* InstantiateScriptComponent(
		FAutomationTestBase& Test,
		AActor& OwnerActor,
		UClass* ComponentClass,
		const TCHAR* InstanceName,
		const TCHAR* Context)
	{
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("%s should compile to a valid generated component class"), Context),
				ComponentClass))
		{
			return nullptr;
		}

		UActorComponent* Component = NewObject<UActorComponent>(&OwnerActor, ComponentClass, InstanceName);
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("%s should instantiate a runtime component"), Context),
				Component))
		{
			return nullptr;
		}

		Test.TestTrue(
			*FString::Printf(TEXT("%s should keep the host actor as the typed outer"), Context),
			Component->GetTypedOuter<AActor>() == &OwnerActor);
		Test.TestTrue(
			*FString::Printf(TEXT("%s should resolve the host actor as owner even before registration"), Context),
			Component->GetOwner() == &OwnerActor);

		return Component;
	}

	void ReleaseComponent(TWeakObjectPtr<UActorComponent>& WeakComponent)
	{
		if (!WeakComponent.IsValid())
		{
			return;
		}

		WeakComponent->RemoveFromRoot();
		WeakComponent->MarkAsGarbage();
		WeakComponent = nullptr;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptASClassComponentConstructionTests,
	"Angelscript.TestModule.ClassGenerator.ASClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(StaticComponentConstructorAppliesScriptConstructorAndDefaultsOnce)
	{
		using namespace ASClassComponentConstructionTest;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ASClassComponentConstructionTest::ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
			CollectGarbage(RF_NoFlags, true);
		};

		UASClass* GeneratedASClass = ASClassComponentConstructionTest::CompileComponentConstructionCarrier(*TestRunner, Engine);
		if (GeneratedASClass == nullptr)
		{
			return;
		}

		UActorComponent* DefaultObject = Cast<UActorComponent>(GeneratedASClass->GetDefaultObject());
		if (!TestRunner->TestNotNull(
				TEXT("ASClass component-construction test case should expose a generated component class default object"),
				DefaultObject))
		{
			return;
		}

		ASClassComponentConstructionTest::FComponentConstructionSnapshot DefaultSnapshot;
		if (!ASClassComponentConstructionTest::ReadConstructionSnapshot(*TestRunner, DefaultObject, DefaultSnapshot))
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor& HostActor = Spawner.SpawnActor<AActor>();

		UActorComponent* FirstInstance = ASClassComponentConstructionTest::InstantiateScriptComponent(
			*TestRunner,
			HostActor,
			GeneratedASClass,
			TEXT("ComponentConstructionCarrierA"),
			TEXT("ASClass component-construction test case first instance"));
		UActorComponent* SecondInstance = ASClassComponentConstructionTest::InstantiateScriptComponent(
			*TestRunner,
			HostActor,
			GeneratedASClass,
			TEXT("ComponentConstructionCarrierB"),
			TEXT("ASClass component-construction test case second instance"));
		if (FirstInstance == nullptr || SecondInstance == nullptr)
		{
			return;
		}

		FirstInstance->AddToRoot();
		SecondInstance->AddToRoot();

		TWeakObjectPtr<UActorComponent> WeakFirstInstance = FirstInstance;
		TWeakObjectPtr<UActorComponent> WeakSecondInstance = SecondInstance;
		ON_SCOPE_EXIT
		{
			ASClassComponentConstructionTest::ReleaseComponent(WeakSecondInstance);
			ASClassComponentConstructionTest::ReleaseComponent(WeakFirstInstance);
		};

		ASClassComponentConstructionTest::FComponentConstructionSnapshot FirstSnapshot;
		ASClassComponentConstructionTest::FComponentConstructionSnapshot SecondSnapshot;
		if (!ASClassComponentConstructionTest::ReadConstructionSnapshot(*TestRunner, FirstInstance, FirstSnapshot)
			|| !ASClassComponentConstructionTest::ReadConstructionSnapshot(*TestRunner, SecondInstance, SecondSnapshot))
		{
			return;
		}

		TestRunner->TestTrue(
			TEXT("ASClass component-construction test case should compile a generated component class"),
			GeneratedASClass->IsChildOf(UActorComponent::StaticClass()));
		TestRunner->TestFalse(
			TEXT("ASClass component-construction test case should keep the generated class out of the actor hierarchy"),
			GeneratedASClass->IsChildOf(AActor::StaticClass()));
		TestRunner->TestTrue(
			TEXT("ASClass component-construction test case should create distinct runtime components"),
			FirstInstance != SecondInstance);
		TestRunner->TestTrue(
			TEXT("ASClass component-construction test case should keep runtime components distinct from the class default object"),
			FirstInstance != DefaultObject && SecondInstance != DefaultObject);
		TestRunner->TestTrue(
			TEXT("ASClass component-construction test case should keep both runtime components on the same generated class"),
			FirstInstance->GetClass() == GeneratedASClass && SecondInstance->GetClass() == GeneratedASClass);

		ASClassComponentConstructionTest::VerifySnapshot(
			*TestRunner,
			TEXT("ASClass component-construction test case class default object"),
			DefaultSnapshot,
			ASClassComponentConstructionTest::ExpectedCtorCount);
		ASClassComponentConstructionTest::VerifySnapshot(
			*TestRunner,
			TEXT("ASClass component-construction test case first instance"),
			FirstSnapshot,
			ASClassComponentConstructionTest::ExpectedCtorCount);
		ASClassComponentConstructionTest::VerifySnapshot(
			*TestRunner,
			TEXT("ASClass component-construction test case second instance"),
			SecondSnapshot,
			ASClassComponentConstructionTest::ExpectedCtorCount);

		TestRunner->TestEqual(
			TEXT("ASClass component-construction test case should keep the second instance constructor count isolated from the first instance"),
			SecondSnapshot.CtorCount,
			ASClassComponentConstructionTest::ExpectedCtorCount);
		TestRunner->TestEqual(
			TEXT("ASClass component-construction test case should keep the class default object on the same scripted integer default as runtime instances"),
			DefaultSnapshot.DefaultValue,
			FirstSnapshot.DefaultValue);
		TestRunner->TestEqual(
			TEXT("ASClass component-construction test case should keep both runtime components on the same scripted integer default"),
			FirstSnapshot.DefaultValue,
			SecondSnapshot.DefaultValue);
		TestRunner->TestEqual(
			TEXT("ASClass component-construction test case should keep the class default object on the same scripted string default as runtime instances"),
			DefaultSnapshot.DefaultLabel,
			FirstSnapshot.DefaultLabel);
		TestRunner->TestEqual(
			TEXT("ASClass component-construction test case should keep both runtime components on the same scripted string default"),
			FirstSnapshot.DefaultLabel,
			SecondSnapshot.DefaultLabel);

		}
	}
};

#endif
