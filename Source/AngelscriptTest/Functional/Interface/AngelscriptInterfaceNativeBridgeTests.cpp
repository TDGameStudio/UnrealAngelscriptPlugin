// ============================================================================
// AngelscriptInterfaceNativeBridgeTests.cpp
//
// Native interface bridge tests — CQTest refactor. Automation ID:
//   Angelscript.TestModule.Interface.NativeBridge
//
// Validates that a script class can cast to and call methods on a C++
// native interface implementer through the script-side interface bridge.
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestModuleScope.h"
#include "Shared/AngelscriptBindingsAssertions.h"

#include "Core/AngelscriptEngine.h"
#include "Shared/AngelscriptNativeInterfaceTestTypes.h"
#include "Shared/AngelscriptNativeInterfaceTestHelpers.h"
#include "Shared/AngelscriptFunctionalTestUtils.h"

#include "Components/ActorTestSpawner.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptTestBindings;
using namespace AngelscriptReflectiveAccess;
using namespace AngelscriptFunctionalTestUtils;

// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------

namespace InterfaceNativeBridgeTests
{
	static const FName ModuleName(TEXT("TestInterfaceNativeCppImplementerBridge"));
	static const FString ScriptFilename(TEXT("TestInterfaceNativeCppImplementerBridge.as"));
	static const FName GeneratedClassName(TEXT("ATestInterfaceNativeCppImplementerBridge"));
	static const FName TargetPropertyName(TEXT("Target"));
	static const FName CastSucceededPropertyName(TEXT("bCastSucceeded"));
	static const FName ReadValuePropertyName(TEXT("ReadValue"));
	static const FName AdjustedValuePropertyName(TEXT("AdjustedValue"));

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

TEST_CLASS_WITH_FLAGS(FAngelscriptInterfaceNativeBridgeTest, "Angelscript.TestModule.Interface.NativeBridge", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL() { ASTEST_CREATE_ENGINE(); }
	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	TEST_METHOD(CppImplementerScriptCall)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		AngelscriptNativeInterfaceTestHelpers::EnsureNativeInterfaceBound(UAngelscriptNativeParentInterface::StaticClass());

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*InterfaceNativeBridgeTests::ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			InterfaceNativeBridgeTests::ModuleName,
			InterfaceNativeBridgeTests::ScriptFilename,
			TEXT(R"AS(
UCLASS()
class ATestInterfaceNativeCppImplementerBridge : AActor
{
	UPROPERTY()
	UObject Target;

	UPROPERTY()
	int bCastSucceeded = 0;

	UPROPERTY()
	int ReadValue = 0;

	UPROPERTY()
	int AdjustedValue = 0;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		UAngelscriptNativeParentInterface ParentRef = Cast<UAngelscriptNativeParentInterface>(Target);
		if (ParentRef == nullptr)
			return;

		bCastSucceeded = 1;
		ReadValue = ParentRef.GetNativeValue();

		int Value = 10;
		ParentRef.AdjustNativeValue(5, Value);
		AdjustedValue = Value;

		ParentRef.SetNativeMarker(n"FromScript");
	}
}
)AS"),
			InterfaceNativeBridgeTests::GeneratedClassName);
		if (!TestRunner->TestNotNull(TEXT("ScriptClass should be valid"), ScriptClass))
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();

		ATestNativeParentInterfaceActor* NativeFixtureActor = Spawner.GetWorld().SpawnActor<ATestNativeParentInterfaceActor>();
		AActor* ScriptActor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Native interface bridge fixture actor should spawn"), NativeFixtureActor)
			|| !TestRunner->TestNotNull(TEXT("Native interface bridge script actor should spawn"), ScriptActor))
		{
			return;
		}

		if (!InterfaceNativeBridgeTests::SetObjectReferenceProperty(
			*TestRunner,
			ScriptActor,
			InterfaceNativeBridgeTests::TargetPropertyName,
			NativeFixtureActor,
			TEXT("Native interface bridge script actor")))
		{
			return;
		}

		BeginPlayActor(Engine, *ScriptActor);

		int32 bCastSucceeded = 0;
		int32 ReadValue = 0;
		int32 AdjustedValue = 0;
		if (!ReadPropertyValue<FIntProperty>(*TestRunner, ScriptActor, InterfaceNativeBridgeTests::CastSucceededPropertyName, bCastSucceeded)
			|| !ReadPropertyValue<FIntProperty>(*TestRunner, ScriptActor, InterfaceNativeBridgeTests::ReadValuePropertyName, ReadValue)
			|| !ReadPropertyValue<FIntProperty>(*TestRunner, ScriptActor, InterfaceNativeBridgeTests::AdjustedValuePropertyName, AdjustedValue))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Script-side cast to a pure C++ native interface implementer should succeed"), bCastSucceeded, 1);
		TestRunner->TestEqual(TEXT("Script-side interface getter should dispatch to the C++ implementer"), ReadValue, 123);
		TestRunner->TestEqual(TEXT("Script-side ref parameter bridge should write back the adjusted value"), AdjustedValue, 15);
		TestRunner->TestEqual(TEXT("Script-side interface setter should update the C++ implementer marker"), NativeFixtureActor->NativeMarker, FName(TEXT("FromScript")));
		TestRunner->TestEqual(TEXT("C++ fixture should observe the delta passed through the interface bridge"), NativeFixtureActor->LastAdjustmentDelta, 5);
		TestRunner->TestEqual(TEXT("C++ fixture should observe the final ref value written by the interface bridge"), NativeFixtureActor->LastAdjustedValue, 15);
	}
};

#endif
