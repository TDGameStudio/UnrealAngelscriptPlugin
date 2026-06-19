#include "CQTest.h"
#include "Core/AngelscriptEngine.h"
#include "AngelscriptNativeInterfaceTestTypes.h"
#include "AngelscriptNativeInterfaceTestHelpers.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptBindingsAssertions.h"

#include "Components/ActorTestSpawner.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

namespace InterfaceNativeInheritedChildSurfaceTests
{
	static const FName ModuleName(TEXT("TestInterfaceNativeInheritedChildSurface"));
	static const FString ScriptFilename(TEXT("TestInterfaceNativeInheritedChildSurface.as"));
	static const FName GeneratedClassName(TEXT("ATestInterfaceNativeInheritedChildSurface"));
}


TEST_CLASS_WITH_FLAGS(FAngelscriptInterfaceNativeInheritedChildSurfaceTests, "Angelscript.TestModule.Interface.NativeInheritedChild", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	TEST_METHOD(ChildSurfaceIncludesParentMethods)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		AngelscriptNativeInterfaceTestHelpers::EnsureNativeInterfaceBound(UAngelscriptNativeParentInterface::StaticClass());
		AngelscriptNativeInterfaceTestHelpers::EnsureNativeInterfaceBound(UAngelscriptNativeChildInterface::StaticClass());

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*InterfaceNativeInheritedChildSurfaceTests::ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			InterfaceNativeInheritedChildSurfaceTests::ModuleName,
			InterfaceNativeInheritedChildSurfaceTests::ScriptFilename,
			TEXT(R"AS(
UCLASS()
class ATestInterfaceNativeInheritedChildSurface : AActor, UAngelscriptNativeChildInterface
{
	UPROPERTY()
	int ChildCastWorked = 0;

	UPROPERTY()
	int ChildParentResult = 0;

	UPROPERTY()
	int ChildAdjustedValue = 0;

	UPROPERTY()
	int ChildOwnResult = 0;

	UPROPERTY()
	FName NativeMarker = NAME_None;

	UFUNCTION()
	int GetNativeValue() const
	{
		return 7;
	}

	UFUNCTION()
	void SetNativeMarker(FName Marker)
	{
		NativeMarker = Marker;
	}

	UFUNCTION()
	void AdjustNativeValue(int Delta, int& Value)
	{
		Value += Delta;
	}

	UFUNCTION()
	int GetChildValue() const
	{
		return 11;
	}

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		UObject Self = this;
		UAngelscriptNativeChildInterface ChildRef = Cast<UAngelscriptNativeChildInterface>(Self);
		if (ChildRef == nullptr)
			return;

		ChildCastWorked = 1;
		ChildParentResult = ChildRef.GetNativeValue();
		int Value = 20;
		ChildRef.AdjustNativeValue(9, Value);
		ChildAdjustedValue = Value;
		ChildOwnResult = ChildRef.GetChildValue();
		ChildRef.SetNativeMarker(n"ChildRoute");
	}
}
)AS"),
			InterfaceNativeInheritedChildSurfaceTests::GeneratedClassName);
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("ScriptClass should be valid")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Actor should be valid")));

		BeginPlayActor(Engine, *Actor);

		int32 ChildCastWorked = 0;
		int32 ChildParentResult = 0;
		int32 ChildAdjustedValue = 0;
		int32 ChildOwnResult = 0;
		FName NativeMarker = NAME_None;
		if (!ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("ChildCastWorked"), ChildCastWorked)
			|| !ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("ChildParentResult"), ChildParentResult)
			|| !ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("ChildAdjustedValue"), ChildAdjustedValue)
			|| !ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("ChildOwnResult"), ChildOwnResult)
			|| !ReadPropertyValue<FNameProperty>(*TestRunner, Actor, TEXT("NativeMarker"), NativeMarker))
		{
			return;
		}

		ASSERT_THAT(IsTrue(ScriptClass->ImplementsInterface(UAngelscriptNativeChildInterface::StaticClass()), TEXT("Script actor should implement native child interface")));
		ASSERT_THAT(IsTrue(ScriptClass->ImplementsInterface(UAngelscriptNativeParentInterface::StaticClass()), TEXT("Script actor implementing child interface should also satisfy native parent interface")));
		ASSERT_THAT(AreEqual(1, ChildCastWorked, TEXT("Script-side cast to native child interface should succeed")));
		ASSERT_THAT(AreEqual(7, ChildParentResult, TEXT("Child native interface ref should expose inherited parent getter")));
		ASSERT_THAT(AreEqual(29, ChildAdjustedValue, TEXT("Child native interface ref should expose inherited parent ref-parameter method")));
		ASSERT_THAT(AreEqual(11, ChildOwnResult, TEXT("Child native interface ref should still expose child-owned methods")));
		ASSERT_THAT(AreEqual(FName(TEXT("ChildRoute")), NativeMarker, TEXT("Child native interface ref should expose inherited parent setter")));
	}
};

#endif
