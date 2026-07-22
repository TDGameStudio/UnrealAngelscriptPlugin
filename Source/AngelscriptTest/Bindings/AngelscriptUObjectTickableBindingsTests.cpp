#include "CQTest.h"

#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Binds/UObjectTickable.h"

#include "Engine/EngineBaseTypes.h"
#include "Tickable.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptUObjectTickableBindingsTest,
	"Angelscript.TestModule.Bindings.UObjectTickable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(DestroyObjectStopsTicking)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("ASUObjectTickableDestroy"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = AngelscriptFunctionalTestUtils::CompileScriptModule(*TestRunner, Engine, ModuleName,
			TEXT("ASUObjectTickableDestroy.as"),
			TEXT(R"AS(
UCLASS()
class UObjectTickableDestroyTest : UObjectTickable
{
	UPROPERTY()
	int TickCount = 0;

	UFUNCTION(BlueprintOverride)
	void Tick(float DeltaTime)
	{
		TickCount += 1;
	}
}
)AS"),
			TEXT("UObjectTickableDestroyTest"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UObjectTickable regression class should be generated")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UObject* RuntimeObject = NewObject<UObject>(GetTransientPackage(), ScriptClass, TEXT("UObjectTickableDestroyTestInstance"));
		UObjectTickable* Tickable = Cast<UObjectTickable>(RuntimeObject);
		ASSERT_THAT(IsNotNull(Tickable, TEXT("UObjectTickable test object should be created")));
		if (Tickable == nullptr)
		{
			return;
		}

		FIntProperty* TickCountProperty = FindFProperty<FIntProperty>(ScriptClass, TEXT("TickCount"));
		ASSERT_THAT(IsNotNull(TickCountProperty, TEXT("UObjectTickable regression class should expose TickCount")));
		if (TickCountProperty == nullptr)
		{
			return;
		}

		FTickableGameObject::TickObjects(nullptr, LEVELTICK_All, false, 0.016f);
		const int32 TickCountBeforeDestroy = TickCountProperty->GetPropertyValue_InContainer(Tickable);
		ASSERT_THAT(IsTrue(TickCountBeforeDestroy > 0, TEXT("UObjectTickable should tick before destruction")));

		Tickable->DestroyObject();
		FTickableGameObject::TickObjects(nullptr, LEVELTICK_All, false, 0.016f);
		const int32 TickCountAfterDestroy = TickCountProperty->GetPropertyValue_InContainer(Tickable);
		ASSERT_THAT(AreEqual(
			TickCountBeforeDestroy,
			TickCountAfterDestroy,
			TEXT("DestroyObject should stop UObjectTickable ticking immediately")));
	}
};

#endif
