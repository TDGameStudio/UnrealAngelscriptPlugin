#include "Shared/AngelscriptFunctionalTestUtils.h"
#include "Shared/AngelscriptTestMacros.h"

#include "CQTest.h"
#include "ClassGenerator/ASClass.h"
#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

namespace ASClassActorConstructionTest
{
	static const FName ModuleName(TEXT("ASClassActorConstruction"));
	static const FString ScriptFilename(TEXT("ASClassActorConstruction.as"));
	static const FName GeneratedClassName(TEXT("AActorConstructionCarrier"));
	static const FName CtorCountPropertyName(TEXT("CtorCount"));
	static const FName DefaultValuePropertyName(TEXT("DefaultValue"));
	static const FName DefaultLabelPropertyName(TEXT("DefaultLabel"));
	static const FString ExpectedDefaultLabel(TEXT("ActorDefaults"));
	static constexpr int32 ExpectedCtorCount = 1;
	static constexpr int32 ExpectedDefaultValue = 11;

	struct FActorConstructionSnapshot
	{
		int32 CtorCount = INDEX_NONE;
		int32 DefaultValue = INDEX_NONE;
		FString DefaultLabel;
	};

	UASClass* CompileActorConstructionCarrier(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		const FString ScriptSource = TEXT(R"AS(
UCLASS()
class AActorConstructionCarrier : AActor
{
	UPROPERTY()
	int CtorCount = 0;

	UPROPERTY()
	int DefaultValue = 0;

	UPROPERTY()
	FString DefaultLabel;

	AActorConstructionCarrier()
	{
		CtorCount += 1;
	}

	default DefaultValue = 11;
	default DefaultLabel = "ActorDefaults";
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
				TEXT("ASClass actor-construction test case should compile the carrier into a UASClass"),
				GeneratedASClass))
		{
			return nullptr;
		}

		Test.TestNotNull(
			TEXT("ASClass actor-construction test case should bind the script constructor function"),
			GeneratedASClass->ConstructFunction);
		Test.TestNotNull(
			TEXT("ASClass actor-construction test case should bind the defaults function"),
			GeneratedASClass->DefaultsFunction);
		Test.TestNotNull(
			TEXT("ASClass actor-construction test case should keep a live script type pointer"),
			GeneratedASClass->ScriptTypePtr);

		return GeneratedASClass;
	}

	bool ReadConstructionSnapshot(
		FAutomationTestBase& Test,
		UObject* Object,
		FActorConstructionSnapshot& OutSnapshot)
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

	bool VerifyDefaults(
		FAutomationTestBase& Test,
		const FString& ScopeLabel,
		const FActorConstructionSnapshot& Snapshot)
	{
		const bool bDefaultValueMatches = Test.TestEqual(
			*FString::Printf(TEXT("%s should preserve the scripted integer default"), *ScopeLabel),
			Snapshot.DefaultValue,
			ExpectedDefaultValue);
		const bool bDefaultLabelMatches = Test.TestEqual(
			*FString::Printf(TEXT("%s should preserve the scripted string default"), *ScopeLabel),
			Snapshot.DefaultLabel,
			ExpectedDefaultLabel);

		return bDefaultValueMatches && bDefaultLabelMatches;
	}

	bool VerifyInstanceSnapshot(
		FAutomationTestBase& Test,
		const FString& ScopeLabel,
		const FActorConstructionSnapshot& Snapshot)
	{
		const bool bCtorCountMatches = Test.TestEqual(
			*FString::Printf(TEXT("%s should observe the expected constructor count"), *ScopeLabel),
			Snapshot.CtorCount,
			ExpectedCtorCount);

		return bCtorCountMatches
			&& VerifyDefaults(Test, ScopeLabel, Snapshot);
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptASClassActorConstructionTests,
	"Angelscript.TestModule.ClassGenerator.ASClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(StaticActorConstructorAppliesScriptConstructorAndDefaultsOnce)
	{
		using namespace ASClassActorConstructionTest;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ASClassActorConstructionTest::ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
			CollectGarbage(RF_NoFlags, true);
		};

		UASClass* GeneratedASClass = ASClassActorConstructionTest::CompileActorConstructionCarrier(*TestRunner, Engine);
		if (GeneratedASClass == nullptr)
		{
			return;
		}

		AActor* DefaultObject = Cast<AActor>(GeneratedASClass->GetDefaultObject());
		if (!TestRunner->TestNotNull(
				TEXT("ASClass actor-construction test case should expose a generated actor class default object"),
				DefaultObject))
		{
			return;
		}

		ASClassActorConstructionTest::FActorConstructionSnapshot DefaultSnapshot;
		if (!ASClassActorConstructionTest::ReadConstructionSnapshot(*TestRunner, DefaultObject, DefaultSnapshot))
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();

		AActor* FirstActor = SpawnScriptActor(*TestRunner, Spawner, GeneratedASClass);
		AActor* SecondActor = SpawnScriptActor(*TestRunner, Spawner, GeneratedASClass);
		if (!TestRunner->TestNotNull(TEXT("ASClass actor-construction test case should spawn the first generated actor"), FirstActor)
			|| !TestRunner->TestNotNull(TEXT("ASClass actor-construction test case should spawn the second generated actor"), SecondActor))
		{
			return;
		}

		ASClassActorConstructionTest::FActorConstructionSnapshot FirstSnapshot;
		ASClassActorConstructionTest::FActorConstructionSnapshot SecondSnapshot;
		if (!ASClassActorConstructionTest::ReadConstructionSnapshot(*TestRunner, FirstActor, FirstSnapshot)
			|| !ASClassActorConstructionTest::ReadConstructionSnapshot(*TestRunner, SecondActor, SecondSnapshot))
		{
			return;
		}

		TestRunner->TestTrue(
			TEXT("ASClass actor-construction test case should compile a generated actor class"),
			GeneratedASClass->IsChildOf(AActor::StaticClass()));
		TestRunner->TestTrue(
			TEXT("ASClass actor-construction test case should keep runtime actors on the generated class"),
			FirstActor->GetClass() == GeneratedASClass && SecondActor->GetClass() == GeneratedASClass);
		TestRunner->TestTrue(
			TEXT("ASClass actor-construction test case should create distinct runtime actor instances"),
			FirstActor != SecondActor);
		TestRunner->TestTrue(
			TEXT("ASClass actor-construction test case should keep runtime actors distinct from the class default object"),
			FirstActor != DefaultObject && SecondActor != DefaultObject);

		ASClassActorConstructionTest::VerifyDefaults(
			*TestRunner,
			TEXT("ASClass actor-construction test case class default object"),
			DefaultSnapshot);
		ASClassActorConstructionTest::VerifyInstanceSnapshot(
			*TestRunner,
			TEXT("ASClass actor-construction test case first spawned actor"),
			FirstSnapshot);
		ASClassActorConstructionTest::VerifyInstanceSnapshot(
			*TestRunner,
			TEXT("ASClass actor-construction test case second spawned actor"),
			SecondSnapshot);

		TestRunner->TestEqual(
			TEXT("ASClass actor-construction test case should keep the second actor constructor count isolated from the first actor"),
			SecondSnapshot.CtorCount,
			ASClassActorConstructionTest::ExpectedCtorCount);
		TestRunner->TestEqual(
			TEXT("ASClass actor-construction test case should keep the class default object on the same scripted integer default as spawned actors"),
			DefaultSnapshot.DefaultValue,
			FirstSnapshot.DefaultValue);
		TestRunner->TestEqual(
			TEXT("ASClass actor-construction test case should keep both spawned actors on the same scripted integer default"),
			FirstSnapshot.DefaultValue,
			SecondSnapshot.DefaultValue);
		TestRunner->TestEqual(
			TEXT("ASClass actor-construction test case should keep the class default object on the same scripted string default as spawned actors"),
			DefaultSnapshot.DefaultLabel,
			FirstSnapshot.DefaultLabel);
		TestRunner->TestEqual(
			TEXT("ASClass actor-construction test case should keep both spawned actors on the same scripted string default"),
			FirstSnapshot.DefaultLabel,
			SecondSnapshot.DefaultLabel);

		}
	}
};

#endif
