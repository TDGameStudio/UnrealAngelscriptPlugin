// ============================================================================
// AngelscriptInterfaceNativePointerOffsetTests.cpp
//
// Phase 2 regression tests for FScriptInterface pointer offset handling on
// C++ native implementing classes — CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Interface.NativePointerOffset.MultiInterfaceCast
//   Angelscript.TestModule.Interface.NativePointerOffset.ScriptClassStillZeroOffset
//
// When a C++ class implements more than one UInterface (or its chosen interface
// is not the first UObject-derived base), UE lays the interface vtable down at
// a non-zero `PointerOffset`. Correct behavior requires going through
// `UObject::GetInterfaceAddress(InterfaceClass)` to reach the interface pointer
// rather than assuming it coincides with the UObject pointer.
//
// These tests exercise the test case via `ATestNativeMultiInterfaceActor`
// (implements both Parent + Secondary native interfaces) and validate that:
//   1. Script-side `Cast<U...>(NativeActor)` succeeds for both interfaces.
//   2. Method dispatch through either interface reference lands in the
//      correct C++ `_Implementation` on the multi-interface actor.
//   3. State mutations made through each interface end up on the distinct
//      backing fields (Parent's NativeMarker vs Secondary's SecondaryLabel),
//      which would silently corrupt if the pointer offset were wrong.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptBindingsAssertions.h"

#include "Core/AngelscriptEngine.h"
#include "AngelscriptNativeInterfaceTestTypes.h"
#include "AngelscriptNativeInterfaceTestHelpers.h"
#include "AngelscriptFunctionalTestUtils.h"

#include "Components/ActorTestSpawner.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------

namespace AngelscriptTest_InterfaceNativePointerOffset_Private
{
	static const FName ModuleName(TEXT("TestInterfaceNativePointerOffset"));
	static const FString ScriptFilename(TEXT("TestInterfaceNativePointerOffset.as"));
	static const FName GeneratedClassName(TEXT("ATestInterfaceNativePointerOffset"));
	static const FName TargetPropertyName(TEXT("Target"));
	static const FName ParentCastPropertyName(TEXT("bParentCastSucceeded"));
	static const FName SecondaryCastPropertyName(TEXT("bSecondaryCastSucceeded"));
	static const FName ParentReadValuePropertyName(TEXT("ParentReadValue"));
	static const FName SecondaryReadValuePropertyName(TEXT("SecondaryReadValue"));

	bool SetObjectReferenceProperty(
		FAutomationTestBase& Test,
		UObject* Object,
		FName PropertyName,
		UObject* ReferencedObject,
		const TCHAR* Context)
	{
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s should have a valid object"), Context), Object))
		{
			return false;
		}

		FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Object->GetClass(), PropertyName);
		if (!Test.TestNotNull(
			*FString::Printf(TEXT("%s should expose object property '%s'"), Context, *PropertyName.ToString()),
			Property))
		{
			return false;
		}

		Property->SetObjectPropertyValue_InContainer(Object, ReferencedObject);
		return true;
	}
}


// ----------------------------------------------------------------------------
// Test Class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptInterfaceNativePointerOffsetTest, "Angelscript.TestModule.Interface.NativePointerOffset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL() { ASTEST_CREATE_ENGINE(); }
	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	TEST_METHOD(MultiInterfaceCast)
	{
		using namespace AngelscriptTest_InterfaceNativePointerOffset_Private;
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		AngelscriptNativeInterfaceTestHelpers::EnsureNativeInterfaceBound(UAngelscriptNativeParentInterface::StaticClass());
		AngelscriptNativeInterfaceTestHelpers::EnsureNativeInterfaceBound(UAngelscriptNativeSecondaryInterface::StaticClass());

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			ScriptFilename,
			TEXT(R"AS(
UCLASS()
class ATestInterfaceNativePointerOffset : AActor
{
	UPROPERTY()
	UObject Target;

	UPROPERTY()
	int bParentCastSucceeded = 0;

	UPROPERTY()
	int bSecondaryCastSucceeded = 0;

	UPROPERTY()
	int ParentReadValue = 0;

	UPROPERTY()
	int SecondaryReadValue = 0;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		UAngelscriptNativeParentInterface ParentRef = Cast<UAngelscriptNativeParentInterface>(Target);
		if (ParentRef != nullptr)
		{
			bParentCastSucceeded = 1;
			ParentReadValue = ParentRef.GetNativeValue();
			ParentRef.SetNativeMarker(n"FromParent");
		}

		UAngelscriptNativeSecondaryInterface SecondaryRef = Cast<UAngelscriptNativeSecondaryInterface>(Target);
		if (SecondaryRef != nullptr)
		{
			bSecondaryCastSucceeded = 1;
			SecondaryReadValue = SecondaryRef.GetSecondaryValue();
			SecondaryRef.SetSecondaryLabel("FromSecondary");
		}
	}
}
)AS"),
			GeneratedClassName);
		if (!TestRunner->TestNotNull(TEXT("ScriptClass should be valid"), ScriptClass))
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();

		ATestNativeMultiInterfaceActor* NativeFixtureActor = Spawner.GetWorld().SpawnActor<ATestNativeMultiInterfaceActor>();
		AActor* ScriptActor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Multi-interface native fixture actor should spawn"), NativeFixtureActor)
			|| !TestRunner->TestNotNull(TEXT("Script actor should spawn"), ScriptActor))
		{
			return;
		}

		if (!SetObjectReferenceProperty(
			*TestRunner,
			ScriptActor,
			TargetPropertyName,
			NativeFixtureActor,
			TEXT("Multi-interface native pointer offset script actor")))
		{
			return;
		}

		BeginPlayActor(Engine, *ScriptActor);

		int32 bParentCastSucceeded = 0;
		int32 bSecondaryCastSucceeded = 0;
		int32 ParentReadValue = 0;
		int32 SecondaryReadValue = 0;
		if (!ReadPropertyValue<FIntProperty>(*TestRunner, ScriptActor, ParentCastPropertyName, bParentCastSucceeded)
			|| !ReadPropertyValue<FIntProperty>(*TestRunner, ScriptActor, SecondaryCastPropertyName, bSecondaryCastSucceeded)
			|| !ReadPropertyValue<FIntProperty>(*TestRunner, ScriptActor, ParentReadValuePropertyName, ParentReadValue)
			|| !ReadPropertyValue<FIntProperty>(*TestRunner, ScriptActor, SecondaryReadValuePropertyName, SecondaryReadValue))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Script-side Cast<UAngelscriptNativeParentInterface> should succeed on multi-interface native actor"), bParentCastSucceeded, 1);
		TestRunner->TestEqual(TEXT("Script-side Cast<UAngelscriptNativeSecondaryInterface> should succeed on multi-interface native actor"), bSecondaryCastSucceeded, 1);
		TestRunner->TestEqual(TEXT("Parent interface method dispatch returns the correct C++ field"), ParentReadValue, 777);
		TestRunner->TestEqual(TEXT("Secondary interface method dispatch returns the correct C++ field"), SecondaryReadValue, 4242);

		TestRunner->TestEqual(TEXT("Parent interface setter writes the correct C++ backing field"), NativeFixtureActor->NativeMarker, FName(TEXT("FromParent")));
		TestRunner->TestEqual(TEXT("Secondary interface setter writes the distinct C++ backing field"), NativeFixtureActor->SecondaryLabel, FString(TEXT("FromSecondary")));
	}

	TEST_METHOD(ScriptClassStillZeroOffset)
	{
		using namespace AngelscriptTest_InterfaceNativePointerOffset_Private;
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		AngelscriptNativeInterfaceTestHelpers::EnsureNativeInterfaceBound(UAngelscriptNativeParentInterface::StaticClass());

		static const FName ScriptOnlyModuleName(TEXT("TestInterfaceNativePointerOffsetScriptZero"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ScriptOnlyModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ScriptOnlyModuleName,
			TEXT("TestInterfaceNativePointerOffsetScriptZero.as"),
			TEXT(R"AS(
UCLASS()
class ATestInterfaceNativePointerOffsetScriptZero : AActor, UAngelscriptNativeParentInterface
{
	UPROPERTY()
	int NativeValue = 321;

	UPROPERTY()
	FName NativeMarker = NAME_None;

	UPROPERTY()
	int bSelfCastSucceeded = 0;

	UPROPERTY()
	int DispatchedValue = 0;

	UFUNCTION()
	int GetNativeValue() const
	{
		return NativeValue;
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

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		UObject Self = this;
		UAngelscriptNativeParentInterface ParentRef = Cast<UAngelscriptNativeParentInterface>(Self);
		if (ParentRef != nullptr)
		{
			bSelfCastSucceeded = 1;
			DispatchedValue = ParentRef.GetNativeValue();
			ParentRef.SetNativeMarker(n"FromSelf");
		}
	}
}
)AS"),
			TEXT("ATestInterfaceNativePointerOffsetScriptZero"));
		if (!TestRunner->TestNotNull(TEXT("ScriptClass should be valid"), ScriptClass))
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Actor should be valid"), Actor))
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		// Script-implemented interfaces have `PointerOffset == 0`; this test guards
		// the fast-path: Cast + dispatch must behave identically to the pre-Phase-2
		// behavior on script classes. Any regression here would indicate the new
		// `GetInterfacePointerForCast` helper accidentally perturbed the script path.
		UClass* ImplementedInterface = FindGeneratedClass(&Engine, TEXT("UAngelscriptNativeParentInterface"));
		if (ImplementedInterface == nullptr)
		{
			ImplementedInterface = UAngelscriptNativeParentInterface::StaticClass();
		}
		const FImplementedInterface* ImplementedEntry = Actor->GetClass()->Interfaces.FindByPredicate(
			[ImplementedInterface](const FImplementedInterface& Entry) { return Entry.Class == ImplementedInterface; });
		if (TestRunner->TestNotNull(TEXT("Implemented interface entry should exist on the script class"), ImplementedEntry))
		{
			TestRunner->TestEqual(TEXT("Script-implemented interface must keep PointerOffset == 0"), ImplementedEntry->PointerOffset, 0);
		}

		int32 bSelfCastSucceeded = 0;
		int32 DispatchedValue = 0;
		if (!ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("bSelfCastSucceeded"), bSelfCastSucceeded)
			|| !ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("DispatchedValue"), DispatchedValue))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Script-implemented interface cast should still succeed after Phase 2"), bSelfCastSucceeded, 1);
		TestRunner->TestEqual(TEXT("Script-implemented interface dispatch should still return the script field"), DispatchedValue, 321);

		FName NativeMarker = NAME_None;
		if (!ReadPropertyValue<FNameProperty>(*TestRunner, Actor, TEXT("NativeMarker"), NativeMarker))
		{
			return;
		}
		TestRunner->TestEqual(TEXT("Script-implemented interface setter should still route through the script implementation"), NativeMarker, FName(TEXT("FromSelf")));
	}
};

#endif
