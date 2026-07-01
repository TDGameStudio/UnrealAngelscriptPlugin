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
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptBindingsAssertions.h"

#include "Core/AngelscriptEngine.h"
#include "AngelscriptNativeInterfaceTestTypes.h"
#include "AngelscriptNativeInterfaceTestHelpers.h"
#include "AngelscriptFunctionalTestUtils.h"

#include "Components/ActorTestSpawner.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

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
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(Object, FString::Printf(TEXT("%s should have a valid object"), Context)))
		{
			return false;
		}

		FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Object->GetClass(), PropertyName);
		if (!LocalAssert.IsNotNull(
			Property,
			FString::Printf(TEXT("%s should expose object property '%s'"), Context, *PropertyName.ToString())))
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
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("ScriptClass should be valid")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();

		ATestNativeParentInterfaceActor* NativeFixtureActor = Spawner.GetWorld().SpawnActor<ATestNativeParentInterfaceActor>();
		AActor* ScriptActor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(NativeFixtureActor, TEXT("Native interface bridge fixture actor should spawn")));
		ASSERT_THAT(IsNotNull(ScriptActor, TEXT("Native interface bridge script actor should spawn")));

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

		ASSERT_THAT(AreEqual(1, bCastSucceeded, TEXT("Script-side cast to a pure C++ native interface implementer should succeed")));
		ASSERT_THAT(AreEqual(123, ReadValue, TEXT("Script-side interface getter should dispatch to the C++ implementer")));
		ASSERT_THAT(AreEqual(15, AdjustedValue, TEXT("Script-side ref parameter bridge should write back the adjusted value")));
		ASSERT_THAT(AreEqual(FName(TEXT("FromScript")), NativeFixtureActor->NativeMarker, TEXT("Script-side interface setter should update the C++ implementer marker")));
		ASSERT_THAT(AreEqual(5, NativeFixtureActor->LastAdjustmentDelta, TEXT("C++ fixture should observe the delta passed through the interface bridge")));
		ASSERT_THAT(AreEqual(15, NativeFixtureActor->LastAdjustedValue, TEXT("C++ fixture should observe the final ref value written by the interface bridge")));
	}
};

#endif
