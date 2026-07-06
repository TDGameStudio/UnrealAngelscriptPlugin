#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "ClassGenerator/ASClass.h"
#include "Components/ActorComponent.h"
#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtrTemplates.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptASClassComponentConstructionTests,
	"Angelscript.TestModule.Generator.ASClass.ComponentConstruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName ModuleName = FName(TEXT("ASClassComponentConstruction"));
	inline static const FString ScriptFilename = FString(TEXT("ASClassComponentConstruction.as"));
	inline static const FName GeneratedClassName = FName(TEXT("UComponentConstructionCarrier"));
	inline static const FName CtorCountPropertyName = FName(TEXT("CtorCount"));
	inline static const FName DefaultValuePropertyName = FName(TEXT("DefaultValue"));
	inline static const FName DefaultLabelPropertyName = FName(TEXT("DefaultLabel"));
	inline static const FString ExpectedDefaultLabel = FString(TEXT("ComponentDefaults"));
	static constexpr int32 ExpectedCtorCount = 1;
	static constexpr int32 ExpectedDefaultValue = 9;

	struct FComponentConstructionSnapshot
	{
		int32 CtorCount = INDEX_NONE;
		int32 DefaultValue = INDEX_NONE;
		FString DefaultLabel;
	};

	static UASClass* CompileComponentConstructionCarrier(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		const FString ScriptSource = ASTEST_AS(R"AS(
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

		UClass* GeneratedClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
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
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(
				GeneratedASClass,
				TEXT("ASClass component-construction test case should compile the carrier into a UASClass")))
		{
			return nullptr;
		}

		bool bHasRequiredFunctions = true;
		bHasRequiredFunctions &= LocalAssert.IsNotNull(
			GeneratedASClass->ConstructFunction,
			TEXT("ASClass component-construction test case should bind the script constructor function"));
		bHasRequiredFunctions &= LocalAssert.IsNotNull(
			GeneratedASClass->DefaultsFunction,
			TEXT("ASClass component-construction test case should bind the defaults function"));
		bHasRequiredFunctions &= LocalAssert.IsNotNull(
			GeneratedASClass->ScriptTypePtr,
			TEXT("ASClass component-construction test case should keep a live script type pointer"));
		if (!bHasRequiredFunctions)
		{
			return nullptr;
		}

		return GeneratedASClass;
	}

	static bool ReadConstructionSnapshot(
		FAutomationTestBase& Test,
		UObject* Object,
		FComponentConstructionSnapshot& OutSnapshot)
	{
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(Test, Object, CtorCountPropertyName, OutSnapshot.CtorCount))
		{
			return false;
		}

		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(Test, Object, DefaultValuePropertyName, OutSnapshot.DefaultValue))
		{
			return false;
		}

		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FStrProperty>(Test, Object, DefaultLabelPropertyName, OutSnapshot.DefaultLabel))
		{
			return false;
		}

		return true;
	}

	static bool VerifySnapshot(
		FAutomationTestBase& Test,
		const FString& ScopeLabel,
		const FComponentConstructionSnapshot& Snapshot,
		int32 ExpectedCtorCountForScope)
	{
		FNoDiscardAsserter LocalAssert(Test);
		const bool bCtorCountMatches = LocalAssert.AreEqual(
			ExpectedCtorCountForScope,
			Snapshot.CtorCount,
			*FString::Printf(TEXT("%s should observe the expected constructor count"), *ScopeLabel));
		const bool bDefaultValueMatches = LocalAssert.AreEqual(
			ExpectedDefaultValue,
			Snapshot.DefaultValue,
			*FString::Printf(TEXT("%s should preserve the scripted integer default"), *ScopeLabel));
		const bool bDefaultLabelMatches = LocalAssert.AreEqual(
			ExpectedDefaultLabel,
			Snapshot.DefaultLabel,
			*FString::Printf(TEXT("%s should preserve the scripted string default"), *ScopeLabel));

		return bCtorCountMatches && bDefaultValueMatches && bDefaultLabelMatches;
	}

	static UActorComponent* InstantiateScriptComponent(
		FAutomationTestBase& Test,
		AActor& OwnerActor,
		UClass* ComponentClass,
		const TCHAR* InstanceName,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(
				ComponentClass,
				*FString::Printf(TEXT("%s should compile to a valid generated component class"), Context)))
		{
			return nullptr;
		}

		UActorComponent* Component = NewObject<UActorComponent>(&OwnerActor, ComponentClass, InstanceName);
		if (!LocalAssert.IsNotNull(
				Component,
				*FString::Printf(TEXT("%s should instantiate a runtime component"), Context)))
		{
			return nullptr;
		}

		bool bHasExpectedOwner = true;
		bHasExpectedOwner &= LocalAssert.IsTrue(
			Component->GetTypedOuter<AActor>() == &OwnerActor,
			*FString::Printf(TEXT("%s should keep the host actor as the typed outer"), Context));
		bHasExpectedOwner &= LocalAssert.IsTrue(
			Component->GetOwner() == &OwnerActor,
			*FString::Printf(TEXT("%s should resolve the host actor as owner even before registration"), Context));
		if (!bHasExpectedOwner)
		{
			return nullptr;
		}

		return Component;
	}

	static void ReleaseComponent(TWeakObjectPtr<UActorComponent>& WeakComponent)
	{
		if (!WeakComponent.IsValid())
		{
			return;
		}

		WeakComponent->RemoveFromRoot();
		WeakComponent->MarkAsGarbage();
		WeakComponent = nullptr;
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

	TEST_METHOD(StaticComponentConstructorAppliesScriptConstructorAndDefaultsOnce)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*FAngelscriptASClassComponentConstructionTests::ModuleName.ToString());
			CollectGarbage(RF_NoFlags, true);
		};

		UASClass* GeneratedASClass = FAngelscriptASClassComponentConstructionTests::CompileComponentConstructionCarrier(*TestRunner, Engine);
		if (GeneratedASClass == nullptr)
		{
			return;
		}

		UActorComponent* DefaultObject = Cast<UActorComponent>(GeneratedASClass->GetDefaultObject());
		ASSERT_THAT(IsNotNull(DefaultObject, TEXT("ASClass component-construction test case should expose a generated component class default object")));
		if (DefaultObject == nullptr)
		{
			return;
		}

		FAngelscriptASClassComponentConstructionTests::FComponentConstructionSnapshot DefaultSnapshot;
		if (!FAngelscriptASClassComponentConstructionTests::ReadConstructionSnapshot(*TestRunner, DefaultObject, DefaultSnapshot))
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor& HostActor = Spawner.SpawnActor<AActor>();

		UActorComponent* FirstInstance = FAngelscriptASClassComponentConstructionTests::InstantiateScriptComponent(
			*TestRunner,
			HostActor,
			GeneratedASClass,
			TEXT("ComponentConstructionCarrierA"),
			TEXT("ASClass component-construction test case first instance"));
		UActorComponent* SecondInstance = FAngelscriptASClassComponentConstructionTests::InstantiateScriptComponent(
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
			FAngelscriptASClassComponentConstructionTests::ReleaseComponent(WeakSecondInstance);
			FAngelscriptASClassComponentConstructionTests::ReleaseComponent(WeakFirstInstance);
		};

		FAngelscriptASClassComponentConstructionTests::FComponentConstructionSnapshot FirstSnapshot;
		FAngelscriptASClassComponentConstructionTests::FComponentConstructionSnapshot SecondSnapshot;
		if (!FAngelscriptASClassComponentConstructionTests::ReadConstructionSnapshot(*TestRunner, FirstInstance, FirstSnapshot)
			|| !FAngelscriptASClassComponentConstructionTests::ReadConstructionSnapshot(*TestRunner, SecondInstance, SecondSnapshot))
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			GeneratedASClass->IsChildOf(UActorComponent::StaticClass()),
			TEXT("ASClass component-construction test case should compile a generated component class")));
		ASSERT_THAT(IsFalse(
			GeneratedASClass->IsChildOf(AActor::StaticClass()),
			TEXT("ASClass component-construction test case should keep the generated class out of the actor hierarchy")));
		ASSERT_THAT(IsTrue(
			FirstInstance != SecondInstance,
			TEXT("ASClass component-construction test case should create distinct runtime components")));
		ASSERT_THAT(IsTrue(
			FirstInstance != DefaultObject && SecondInstance != DefaultObject,
			TEXT("ASClass component-construction test case should keep runtime components distinct from the class default object")));
		ASSERT_THAT(IsTrue(
			FirstInstance->GetClass() == GeneratedASClass && SecondInstance->GetClass() == GeneratedASClass,
			TEXT("ASClass component-construction test case should keep both runtime components on the same generated class")));

		FAngelscriptASClassComponentConstructionTests::VerifySnapshot(
			*TestRunner,
			TEXT("ASClass component-construction test case class default object"),
			DefaultSnapshot,
			FAngelscriptASClassComponentConstructionTests::ExpectedCtorCount);
		FAngelscriptASClassComponentConstructionTests::VerifySnapshot(
			*TestRunner,
			TEXT("ASClass component-construction test case first instance"),
			FirstSnapshot,
			FAngelscriptASClassComponentConstructionTests::ExpectedCtorCount);
		FAngelscriptASClassComponentConstructionTests::VerifySnapshot(
			*TestRunner,
			TEXT("ASClass component-construction test case second instance"),
			SecondSnapshot,
			FAngelscriptASClassComponentConstructionTests::ExpectedCtorCount);

		ASSERT_THAT(AreEqual(
			FAngelscriptASClassComponentConstructionTests::ExpectedCtorCount,
			SecondSnapshot.CtorCount,
			TEXT("ASClass component-construction test case should keep the second instance constructor count isolated from the first instance")));
		ASSERT_THAT(AreEqual(
			DefaultSnapshot.DefaultValue,
			FirstSnapshot.DefaultValue,
			TEXT("ASClass component-construction test case should keep the class default object on the same scripted integer default as runtime instances")));
		ASSERT_THAT(AreEqual(
			FirstSnapshot.DefaultValue,
			SecondSnapshot.DefaultValue,
			TEXT("ASClass component-construction test case should keep both runtime components on the same scripted integer default")));
		ASSERT_THAT(AreEqual(
			DefaultSnapshot.DefaultLabel,
			FirstSnapshot.DefaultLabel,
			TEXT("ASClass component-construction test case should keep the class default object on the same scripted string default as runtime instances")));
		ASSERT_THAT(AreEqual(
			FirstSnapshot.DefaultLabel,
			SecondSnapshot.DefaultLabel,
			TEXT("ASClass component-construction test case should keep both runtime components on the same scripted string default")));
	}
};

#endif
