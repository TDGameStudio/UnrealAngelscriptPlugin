#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "ClassGenerator/ASClass.h"
#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"

#if WITH_ANGELSCRIPT_UNITTESTS

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
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(
				GeneratedASClass,
				TEXT("ASClass actor-construction test case should compile the carrier into a UASClass")))
		{
			return nullptr;
		}

		bool bHasRequiredFunctions = true;
		bHasRequiredFunctions &= LocalAssert.IsNotNull(
			GeneratedASClass->ConstructFunction,
			TEXT("ASClass actor-construction test case should bind the script constructor function"));
		bHasRequiredFunctions &= LocalAssert.IsNotNull(
			GeneratedASClass->DefaultsFunction,
			TEXT("ASClass actor-construction test case should bind the defaults function"));
		bHasRequiredFunctions &= LocalAssert.IsNotNull(
			GeneratedASClass->ScriptTypePtr,
			TEXT("ASClass actor-construction test case should keep a live script type pointer"));
		if (!bHasRequiredFunctions)
		{
			return nullptr;
		}

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
		FNoDiscardAsserter LocalAssert(Test);
		const bool bDefaultValueMatches = LocalAssert.AreEqual(
			ExpectedDefaultValue,
			Snapshot.DefaultValue,
			*FString::Printf(TEXT("%s should preserve the scripted integer default"), *ScopeLabel));
		const bool bDefaultLabelMatches = LocalAssert.AreEqual(
			ExpectedDefaultLabel,
			Snapshot.DefaultLabel,
			*FString::Printf(TEXT("%s should preserve the scripted string default"), *ScopeLabel));

		return bDefaultValueMatches && bDefaultLabelMatches;
	}

	bool VerifyInstanceSnapshot(
		FAutomationTestBase& Test,
		const FString& ScopeLabel,
		const FActorConstructionSnapshot& Snapshot)
	{
		FNoDiscardAsserter LocalAssert(Test);
		const bool bCtorCountMatches = LocalAssert.AreEqual(
			ExpectedCtorCount,
			Snapshot.CtorCount,
			*FString::Printf(TEXT("%s should observe the expected constructor count"), *ScopeLabel));

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
		ASSERT_THAT(IsNotNull(DefaultObject, TEXT("ASClass actor-construction test case should expose a generated actor class default object")));
		if (DefaultObject == nullptr)
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
		ASSERT_THAT(IsNotNull(FirstActor, TEXT("ASClass actor-construction test case should spawn the first generated actor")));
		ASSERT_THAT(IsNotNull(SecondActor, TEXT("ASClass actor-construction test case should spawn the second generated actor")));
		if (FirstActor == nullptr || SecondActor == nullptr)
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

		ASSERT_THAT(IsTrue(
			GeneratedASClass->IsChildOf(AActor::StaticClass()),
			TEXT("ASClass actor-construction test case should compile a generated actor class")));
		ASSERT_THAT(IsTrue(
			FirstActor->GetClass() == GeneratedASClass && SecondActor->GetClass() == GeneratedASClass,
			TEXT("ASClass actor-construction test case should keep runtime actors on the generated class")));
		ASSERT_THAT(IsTrue(
			FirstActor != SecondActor,
			TEXT("ASClass actor-construction test case should create distinct runtime actor instances")));
		ASSERT_THAT(IsTrue(
			FirstActor != DefaultObject && SecondActor != DefaultObject,
			TEXT("ASClass actor-construction test case should keep runtime actors distinct from the class default object")));

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

		ASSERT_THAT(AreEqual(
			ASClassActorConstructionTest::ExpectedCtorCount,
			SecondSnapshot.CtorCount,
			TEXT("ASClass actor-construction test case should keep the second actor constructor count isolated from the first actor")));
		ASSERT_THAT(AreEqual(
			DefaultSnapshot.DefaultValue,
			FirstSnapshot.DefaultValue,
			TEXT("ASClass actor-construction test case should keep the class default object on the same scripted integer default as spawned actors")));
		ASSERT_THAT(AreEqual(
			FirstSnapshot.DefaultValue,
			SecondSnapshot.DefaultValue,
			TEXT("ASClass actor-construction test case should keep both spawned actors on the same scripted integer default")));
		ASSERT_THAT(AreEqual(
			DefaultSnapshot.DefaultLabel,
			FirstSnapshot.DefaultLabel,
			TEXT("ASClass actor-construction test case should keep the class default object on the same scripted string default as spawned actors")));
		ASSERT_THAT(AreEqual(
			FirstSnapshot.DefaultLabel,
			SecondSnapshot.DefaultLabel,
			TEXT("ASClass actor-construction test case should keep both spawned actors on the same scripted string default")));

		}
	}
};

#endif
