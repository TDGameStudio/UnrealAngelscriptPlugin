// ============================================================================
// AngelscriptInterfaceNativeTests.cpp
//
// Native interface implementation tests �?CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Interface.Native.NativeImplement
//   Angelscript.TestModule.Interface.Native.NativeInheritedImplement
//   Angelscript.TestModule.Interface.Native.NativeReferenceRoundTrip
//   Angelscript.TestModule.Interface.Native.NativeReferenceRoundTripCppBridgeMutatesActorState
//   Angelscript.TestModule.Interface.Native.NativeInheritedParentBridgeSetterAndRef
//
// Validates script classes implementing native interfaces: single interface,
// inherited child interface, ref parameter round-trip, C++ Execute_ bridge
// dispatching back into script, and parent bridge through child implementation.
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
// Test Class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptInterfaceNativeTest, "Angelscript.TestModule.Interface.Native", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
template <typename ActualType, typename ExpectedType>
static bool CheckEqual(FAutomationTestBase& Test, const TCHAR* Message, const ActualType& Actual, const ExpectedType& Expected)
{
	FNoDiscardAsserter LocalAssert(Test);
	return LocalAssert.AreEqual(Expected, Actual, Message);
}

template <typename ValueType>
static bool CheckNotNull(FAutomationTestBase& Test, const TCHAR* Message, const ValueType& Value)
{
	FNoDiscardAsserter LocalAssert(Test);
	return LocalAssert.IsNotNull(Value, Message);
}

public:
	BEFORE_ALL() { ASTEST_CREATE_ENGINE(); }
	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	TEST_METHOD(NativeImplement)
	{
FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		AngelscriptNativeInterfaceTestHelpers::EnsureNativeInterfaceBound(UAngelscriptNativeParentInterface::StaticClass());
		AngelscriptNativeInterfaceTestHelpers::EnsureNativeInterfaceBound(UAngelscriptNativeChildInterface::StaticClass());

		static const FName ModuleName(TEXT("TestInterfaceNativeImplement"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("TestInterfaceNativeImplement.as"),
			TEXT(R"AS(
UCLASS()
class ATestInterfaceNativeImplement : AActor, UAngelscriptNativeParentInterface
{
	UPROPERTY()
	int NativeValue = 123;

	UPROPERTY()
	FName NativeMarker = NAME_None;

	UPROPERTY()
	int ParentCastWorked = 0;

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
			ParentCastWorked = 1;
			NativeValue = ParentRef.GetNativeValue();
			ParentRef.SetNativeMarker(n"FromScript");
		}
	}
}
)AS"),
			TEXT("ATestInterfaceNativeImplement"));
		if (!CheckNotNull(*TestRunner, TEXT("ScriptClass should be valid"), ScriptClass))
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		if (!CheckNotNull(*TestRunner, TEXT("Actor should be valid"), Actor))
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(ScriptClass->ImplementsInterface(UAngelscriptNativeParentInterface::StaticClass()), TEXT("Script actor should implement native parent interface")));

		int32 ParentCastWorked = 0;
		if (!ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("ParentCastWorked"), ParentCastWorked))
		{
			return;
		}
		ASSERT_THAT(AreEqual(1, ParentCastWorked, TEXT("Cast to native parent interface should succeed in script")));

		int32 NativeValue = 0;
		if (!ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("NativeValue"), NativeValue))
		{
			return;
		}
		ASSERT_THAT(AreEqual(123, NativeValue, TEXT("Script-side native interface call should preserve the returned value")));

		FName NativeMarker = NAME_None;
		if (!ReadPropertyValue<FNameProperty>(*TestRunner, Actor, TEXT("NativeMarker"), NativeMarker))
		{
			return;
		}
		ASSERT_THAT(AreEqual(FName(TEXT("FromScript")), NativeMarker, TEXT("Script-side native interface setter should run through the interface reference")));

		ASSERT_THAT(AreEqual(123, IAngelscriptNativeParentInterface::Execute_GetNativeValue(Actor),
			TEXT("C++ Execute_ bridge should call the script implementation of GetNativeValue")));
		IAngelscriptNativeParentInterface::Execute_SetNativeMarker(Actor, TEXT("FromCpp"));

		if (!ReadPropertyValue<FNameProperty>(*TestRunner, Actor, TEXT("NativeMarker"), NativeMarker))
		{
			return;
		}
		ASSERT_THAT(AreEqual(FName(TEXT("FromCpp")), NativeMarker, TEXT("C++ Execute_ bridge should call the script implementation of SetNativeMarker")));
	}

	TEST_METHOD(NativeInheritedImplement)
	{
FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		AngelscriptNativeInterfaceTestHelpers::EnsureNativeInterfaceBound(UAngelscriptNativeParentInterface::StaticClass());
		AngelscriptNativeInterfaceTestHelpers::EnsureNativeInterfaceBound(UAngelscriptNativeChildInterface::StaticClass());

		static const FName ModuleName(TEXT("TestInterfaceNativeInheritedImplement"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("TestInterfaceNativeInheritedImplement.as"),
			TEXT(R"AS(
UCLASS()
class ATestInterfaceNativeInheritedImplement : AActor, UAngelscriptNativeChildInterface
{
	UPROPERTY()
	int ParentCastWorked = 0;

	UPROPERTY()
	int ChildCastWorked = 0;

	UPROPERTY()
	int ParentResult = 0;

	UPROPERTY()
	int ChildResult = 0;

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
		UAngelscriptNativeParentInterface ParentRef = Cast<UAngelscriptNativeParentInterface>(Self);
		if (ParentRef != nullptr)
		{
			ParentCastWorked = 1;
			ParentResult = ParentRef.GetNativeValue();
			ParentRef.SetNativeMarker(n"ParentRoute");
		}

		UAngelscriptNativeChildInterface ChildRef = Cast<UAngelscriptNativeChildInterface>(Self);
		if (ChildRef != nullptr)
		{
			ChildCastWorked = 1;
			ChildResult = ChildRef.GetChildValue();
		}
	}
}
)AS"),
			TEXT("ATestInterfaceNativeInheritedImplement"));
		if (!CheckNotNull(*TestRunner, TEXT("ScriptClass should be valid"), ScriptClass))
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		if (!CheckNotNull(*TestRunner, TEXT("Actor should be valid"), Actor))
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(ScriptClass->ImplementsInterface(UAngelscriptNativeChildInterface::StaticClass()), TEXT("Script actor should implement native child interface")));
		ASSERT_THAT(IsTrue(ScriptClass->ImplementsInterface(UAngelscriptNativeParentInterface::StaticClass()), TEXT("Script actor implementing child interface should also satisfy native parent interface")));

		int32 ParentCastWorked = 0;
		int32 ChildCastWorked = 0;
		int32 ParentResult = 0;
		int32 ChildResult = 0;
		FName NativeMarker = NAME_None;

		if (!ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("ParentCastWorked"), ParentCastWorked)
			|| !ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("ChildCastWorked"), ChildCastWorked)
			|| !ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("ParentResult"), ParentResult)
			|| !ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("ChildResult"), ChildResult)
			|| !ReadPropertyValue<FNameProperty>(*TestRunner, Actor, TEXT("NativeMarker"), NativeMarker))
		{
			return;
		}

		ASSERT_THAT(AreEqual(1, ParentCastWorked, TEXT("Script-side cast to native parent interface should succeed through child implementation")));
		ASSERT_THAT(AreEqual(1, ChildCastWorked, TEXT("Script-side cast to native child interface should succeed")));
		ASSERT_THAT(AreEqual(7, ParentResult, TEXT("Parent native interface method should return the script implementation value")));
		ASSERT_THAT(AreEqual(11, ChildResult, TEXT("Child native interface method should return the script implementation value")));
		ASSERT_THAT(AreEqual(FName(TEXT("ParentRoute")), NativeMarker, TEXT("Parent native interface setter should execute through the parent reference")));

		ASSERT_THAT(AreEqual(7, IAngelscriptNativeParentInterface::Execute_GetNativeValue(Actor),
			TEXT("C++ Execute_ should dispatch parent interface method on child implementation")));
		ASSERT_THAT(AreEqual(11, IAngelscriptNativeChildInterface::Execute_GetChildValue(Actor),
			TEXT("C++ Execute_ should dispatch child interface method on child implementation")));
	}

	TEST_METHOD(NativeReferenceRoundTrip)
	{
FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		AngelscriptNativeInterfaceTestHelpers::EnsureNativeInterfaceBound(UAngelscriptNativeParentInterface::StaticClass());
		AngelscriptNativeInterfaceTestHelpers::EnsureNativeInterfaceBound(UAngelscriptNativeChildInterface::StaticClass());

		static const FName ModuleName(TEXT("TestInterfaceNativeReferenceRoundTrip"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("TestInterfaceNativeReferenceRoundTrip.as"),
			TEXT(R"AS(
UCLASS()
class ATestInterfaceNativeReferenceRoundTrip : AActor, UAngelscriptNativeParentInterface
{
	UPROPERTY()
	int ScriptAdjustedValue = 0;

	UFUNCTION()
	int GetNativeValue() const
	{
		return 0;
	}

	UFUNCTION()
	void SetNativeMarker(FName Marker)
	{
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
		if (ParentRef == nullptr)
			return;

		int Value = 10;
		ParentRef.AdjustNativeValue(5, Value);
		ScriptAdjustedValue = Value;
	}
}
)AS"),
			TEXT("ATestInterfaceNativeReferenceRoundTrip"));
		if (!CheckNotNull(*TestRunner, TEXT("ScriptClass should be valid"), ScriptClass))
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		if (!CheckNotNull(*TestRunner, TEXT("Actor should be valid"), Actor))
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		int32 ScriptAdjustedValue = 0;
		if (!ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("ScriptAdjustedValue"), ScriptAdjustedValue))
		{
			return;
		}
		ASSERT_THAT(AreEqual(15, ScriptAdjustedValue, TEXT("Script-side native interface call should round-trip ref parameters")));

		int32 CppAdjustedValue = 20;
		IAngelscriptNativeParentInterface::Execute_AdjustNativeValue(Actor, 7, CppAdjustedValue);
		ASSERT_THAT(AreEqual(27, CppAdjustedValue, TEXT("C++ Execute_ bridge should round-trip ref parameters through the script implementation")));
	}

	TEST_METHOD(NativeReferenceRoundTripCppBridgeMutatesActorState)
	{
FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		AngelscriptNativeInterfaceTestHelpers::EnsureNativeInterfaceBound(UAngelscriptNativeParentInterface::StaticClass());
		AngelscriptNativeInterfaceTestHelpers::EnsureNativeInterfaceBound(UAngelscriptNativeChildInterface::StaticClass());

		static const FName ModuleName(TEXT("TestInterfaceNativeReferenceRoundTripCppBridgeState"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("TestInterfaceNativeReferenceRoundTripCppBridgeState.as"),
			TEXT(R"AS(
UCLASS()
class ATestInterfaceNativeReferenceRoundTripCppBridgeState : AActor, UAngelscriptNativeParentInterface
{
	UPROPERTY()
	int ScriptAdjustedValue = 0;

	UPROPERTY()
	int AdjustCallCount = 0;

	UPROPERTY()
	int LastAdjustedValue = 0;

	UFUNCTION()
	int GetNativeValue() const
	{
		return 0;
	}

	UFUNCTION()
	void SetNativeMarker(FName Marker)
	{
	}

	UFUNCTION()
	void AdjustNativeValue(int Delta, int& Value)
	{
		Value += Delta;
		AdjustCallCount += 1;
		LastAdjustedValue = Value;
	}

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		UObject Self = this;
		UAngelscriptNativeParentInterface ParentRef = Cast<UAngelscriptNativeParentInterface>(Self);
		if (ParentRef == nullptr)
			return;

		int Value = 10;
		ParentRef.AdjustNativeValue(5, Value);
		ScriptAdjustedValue = Value;
	}
}
)AS"),
			TEXT("ATestInterfaceNativeReferenceRoundTripCppBridgeState"));
		if (!CheckNotNull(*TestRunner, TEXT("ScriptClass should be valid"), ScriptClass))
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		if (!CheckNotNull(*TestRunner, TEXT("Actor should be valid"), Actor))
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		int32 ScriptAdjustedValue = 0;
		int32 AdjustCallCount = 0;
		int32 LastAdjustedValue = 0;
		if (!ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("ScriptAdjustedValue"), ScriptAdjustedValue)
			|| !ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("AdjustCallCount"), AdjustCallCount)
			|| !ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("LastAdjustedValue"), LastAdjustedValue))
		{
			return;
		}

		ASSERT_THAT(AreEqual(15, ScriptAdjustedValue, TEXT("Script-side native interface call should still round-trip ref parameters")));
		ASSERT_THAT(AreEqual(1, AdjustCallCount, TEXT("Script-side BeginPlay route should increment the adjust call count once")));
		ASSERT_THAT(AreEqual(15, LastAdjustedValue, TEXT("Script-side BeginPlay route should persist the last adjusted value on the actor state")));

		int32 CppAdjustedValue = 20;
		IAngelscriptNativeParentInterface::Execute_AdjustNativeValue(Actor, 7, CppAdjustedValue);
		ASSERT_THAT(AreEqual(27, CppAdjustedValue, TEXT("C++ Execute_ bridge should still round-trip ref parameters through the script implementation")));

		if (!ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("AdjustCallCount"), AdjustCallCount)
			|| !ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("LastAdjustedValue"), LastAdjustedValue))
		{
			return;
		}

		ASSERT_THAT(AreEqual(2, AdjustCallCount, TEXT("C++ Execute_ bridge should re-enter the script implementation and increment actor state")));
		ASSERT_THAT(AreEqual(27, LastAdjustedValue, TEXT("C++ Execute_ bridge should update the actor's last adjusted value after mutating the caller buffer")));
	}

	TEST_METHOD(NativeInheritedParentBridgeSetterAndRef)
	{
FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		AngelscriptNativeInterfaceTestHelpers::EnsureNativeInterfaceBound(UAngelscriptNativeParentInterface::StaticClass());
		AngelscriptNativeInterfaceTestHelpers::EnsureNativeInterfaceBound(UAngelscriptNativeChildInterface::StaticClass());

		static const FName ModuleName(TEXT("TestInterfaceNativeInheritedParentBridge"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("TestInterfaceNativeInheritedParentBridge.as"),
			TEXT(R"AS(
UCLASS()
class ATestInterfaceNativeInheritedParentBridge : AActor, UAngelscriptNativeChildInterface
{
	UPROPERTY()
	FName NativeMarker = NAME_None;

	UPROPERTY()
	int ParentAdjustedValue = 0;

	UFUNCTION()
	int GetNativeValue() const
	{
		return 0;
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
		ParentAdjustedValue = Value;
	}

	UFUNCTION()
	int GetChildValue() const
	{
		return 11;
	}
}
)AS"),
			TEXT("ATestInterfaceNativeInheritedParentBridge"));
		if (!CheckNotNull(*TestRunner, TEXT("ScriptClass should be valid"), ScriptClass))
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		if (!CheckNotNull(*TestRunner, TEXT("Actor should be valid"), Actor))
		{
			return;
		}

		IAngelscriptNativeParentInterface::Execute_SetNativeMarker(Actor, TEXT("FromParentExecute"));

		int32 AdjustedValue = 20;
		IAngelscriptNativeParentInterface::Execute_AdjustNativeValue(Actor, 9, AdjustedValue);

		FName NativeMarker = NAME_None;
		int32 ParentAdjustedValue = 0;
		if (!ReadPropertyValue<FNameProperty>(*TestRunner, Actor, TEXT("NativeMarker"), NativeMarker)
			|| !ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("ParentAdjustedValue"), ParentAdjustedValue))
		{
			return;
		}

		ASSERT_THAT(AreEqual(FName(TEXT("FromParentExecute")), NativeMarker, TEXT("Parent Execute_ setter should dispatch through the child implementation")));
		ASSERT_THAT(AreEqual(29, AdjustedValue, TEXT("Parent Execute_ ref parameter should round-trip through the child implementation")));
		ASSERT_THAT(AreEqual(29, ParentAdjustedValue, TEXT("Child implementation should persist the adjusted parent-interface value")));
	}
};

#endif
