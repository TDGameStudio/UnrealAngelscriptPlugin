#include "Shared/AngelscriptFunctionalTestUtils.h"
#include "Shared/AngelscriptTestMacros.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

// Test Layer: UE Functional - Round1 deep-fill (f-string interpolation + n-name literal runtime equality)
#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptTypesStringInterpolationAndFNameLiteralTests,
	"Angelscript.TestModule.Functional.Types.StringInterpolationAndFNameLiteral",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(FStringInterpolationAndFNameLiteralRuntimeValues)
	{
		using namespace AngelscriptFunctionalTestUtils;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		static const FName ModuleName(TEXT("FunctionalStringInterpolationAndFNameLiteral"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* ActorClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("FunctionalStringInterpolationAndFNameLiteral.as"),
			TEXT(R"AS(
UCLASS()
class AFunctionalStringInterpolationActor : AActor
{
	UPROPERTY()
	FString Greeting;

	UPROPERTY()
	FString Composite;

	UPROPERTY()
	bool bFNameLiteralEqualsConstructor = false;

	UPROPERTY()
	bool bFNameLiteralIsCaseInsensitive = false;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		FString WhoName = "World";
		Greeting = f"Hello {WhoName}!";

		int32 A = 1;
		int32 B = 2;
		int32 C = 3;
		Composite = f"{A} {B} in {C}s";

		bFNameLiteralEqualsConstructor = (n"Tag" == FName("Tag"));
		bFNameLiteralIsCaseInsensitive = (n"tag" == FName("TAG"));
	}
}
)AS"),
			TEXT("AFunctionalStringInterpolationActor"));
		if (ActorClass == nullptr) { return; }

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ActorClass);
		if (Actor == nullptr) { return; }
		BeginPlayActor(Engine, *Actor);

		FString Greeting;
		ReadPropertyValue<FStrProperty>(*TestRunner, Actor, TEXT("Greeting"), Greeting);
		TestRunner->TestEqual(TEXT("Greeting should be the interpolated literal 'Hello World!'"), Greeting, FString(TEXT("Hello World!")));

		FString Composite;
		ReadPropertyValue<FStrProperty>(*TestRunner, Actor, TEXT("Composite"), Composite);
		TestRunner->TestEqual(TEXT("Composite should be '1 2 in 3s'"), Composite, FString(TEXT("1 2 in 3s")));

		bool bLiteralEquality = false;
		ReadPropertyValue<FBoolProperty>(*TestRunner, Actor, TEXT("bFNameLiteralEqualsConstructor"), bLiteralEquality);
		TestRunner->TestTrue(TEXT("n\"Tag\" should equal FName(\"Tag\")"), bLiteralEquality);

		bool bCaseInsensitive = false;
		ReadPropertyValue<FBoolProperty>(*TestRunner, Actor, TEXT("bFNameLiteralIsCaseInsensitive"), bCaseInsensitive);
		TestRunner->TestTrue(TEXT("n\"tag\" should equal FName(\"TAG\") (case-insensitive)"), bCaseInsensitive);
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
