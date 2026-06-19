#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

namespace TemplateBlueprintWorldTickTest
{
	UBlueprint* CreateTransientBlueprintChild(
		FAutomationTestBase& Test,
		UClass* ParentClass,
		FStringView Suffix,
		const TCHAR* CallingContext = TEXT("Template_BlueprintWorldTick"))
	{
		if (!Test.TestNotNull(TEXT("Blueprint parent actor class should be valid"), ParentClass))
		{
			return nullptr;
		}

		const FString PackagePath = FString::Printf(
			TEXT("/Temp/AngelscriptTemplateBlueprintActor_%.*s_%s"),
			Suffix.Len(),
			Suffix.GetData(),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		UPackage* BlueprintPackage = CreatePackage(*PackagePath);
		if (!Test.TestNotNull(TEXT("Transient blueprint package should be created"), BlueprintPackage))
		{
			return nullptr;
		}

		BlueprintPackage->SetFlags(RF_Transient);
		const FName BlueprintName(*FPackageName::GetLongPackageAssetName(PackagePath));

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			ParentClass,
			BlueprintPackage,
			BlueprintName,
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			CallingContext);
		if (!Test.TestNotNull(TEXT("Transient blueprint actor asset should be created"), Blueprint))
		{
			return nullptr;
		}

		return Blueprint;
	}

	bool CompileAndValidateBlueprint(FAutomationTestBase& Test, UBlueprint& Blueprint)
	{
		FKismetEditorUtilities::CompileBlueprint(&Blueprint);
		return Test.TestNotNull(TEXT("Blueprint world-tick template should compile a generated class"), Blueprint.GeneratedClass.Get());
	}

	void CleanupBlueprint(UBlueprint*& Blueprint)
	{
		if (Blueprint == nullptr)
		{
			return;
		}

		if (UClass* BlueprintClass = Blueprint->GeneratedClass)
		{
			BlueprintClass->MarkAsGarbage();
		}

		if (UPackage* BlueprintPackage = Blueprint->GetOutermost())
		{
			BlueprintPackage->MarkAsGarbage();
		}

		Blueprint->MarkAsGarbage();
		CollectGarbage(RF_NoFlags, true);
		Blueprint = nullptr;
	}

	struct FScopedTransientBlueprint
	{
		UBlueprint* Blueprint = nullptr;

		~FScopedTransientBlueprint()
		{
			CleanupBlueprint(Blueprint);
		}

		bool CreateAndCompile(
			FAutomationTestBase& Test,
			UClass* ParentClass,
			FStringView Suffix,
			const TCHAR* CallingContext = TEXT("Template_BlueprintWorldTick"))
		{
			Blueprint = CreateTransientBlueprintChild(Test, ParentClass, Suffix, CallingContext);
			return Blueprint != nullptr && CompileAndValidateBlueprint(Test, *Blueprint);
		}

		UClass* GetGeneratedClass() const
		{
			return Blueprint != nullptr ? Blueprint->GeneratedClass.Get() : nullptr;
		}
	};
}

TEST_CLASS_WITH_FLAGS(FAngelscriptTemplateBlueprintWorldTickTest,
	"Angelscript.Template.Blueprint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ActorChildWorldTick)
	{
		FAngelscriptEngine& Engine = AcquireCleanSharedCloneEngine();
		FAngelscriptEngineScope EngineScope(Engine);
		static const FName ModuleName(TEXT("TemplateBlueprintActorChildWorldTick"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* ScriptParentClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("TemplateBlueprintActorChildWorldTick.as"),
			TEXT(R"AS(
UCLASS()
class ATemplateBlueprintActorChildWorldTickParent : AActor
{
	UPROPERTY()
	int BeginPlayCount = 0;

	UPROPERTY()
	int TickCount = 0;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		BeginPlayCount += 1;
	}

	UFUNCTION(BlueprintOverride)
	void Tick(float DeltaTime)
	{
		TickCount += 1;
	}
}
)AS"),
			TEXT("ATemplateBlueprintActorChildWorldTickParent"));
		ASSERT_THAT(IsNotNull(ScriptParentClass));

		TemplateBlueprintWorldTickTest::FScopedTransientBlueprint Blueprint;
		ASSERT_THAT(IsTrue(Blueprint.CreateAndCompile(*TestRunner, ScriptParentClass, TEXT("ActorChildWorldTick"))));

		UClass* BlueprintClass = Blueprint.GetGeneratedClass();
		ASSERT_THAT(IsNotNull(BlueprintClass, TEXT("Blueprint actor child world-tick template should expose a generated class")));

		ASSERT_THAT(IsTrue(BlueprintClass->IsChildOf(ScriptParentClass),
			TEXT("Blueprint class should inherit from the script actor parent")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();

		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, BlueprintClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Blueprint actor child world-tick template should spawn the blueprint child")));

		BeginPlayActor(*Actor);

		UWorld& World = Spawner.GetWorld();
		for (int32 i = 0; i < 3; ++i)
		{
			World.Tick(ELevelTick::LEVELTICK_All, 0.016f);
		}

		int32 BeginPlayCount = 0;
		ASSERT_THAT(IsTrue(ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("BeginPlayCount"), BeginPlayCount)));

		int32 TickCount = 0;
		ASSERT_THAT(IsTrue(ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("TickCount"), TickCount)));

		ASSERT_THAT(IsTrue(BeginPlayCount >= 1,
			TEXT("Blueprint actor child world-tick template should run inherited BeginPlay at least once")));
		ASSERT_THAT(IsTrue(TickCount >= 1,
			TEXT("Blueprint actor child world-tick template should run inherited Tick at least once")));
	}
};

#endif
