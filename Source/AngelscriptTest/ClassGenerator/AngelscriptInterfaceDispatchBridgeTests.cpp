#include "Core/AngelscriptBinds.h"
#include "Core/AngelscriptEngine.h"
#include "AngelscriptNativeInterfaceTestTypes.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "ClassGenerator/AngelscriptClassGenerator.h"

#include "Components/ActorTestSpawner.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

namespace InterfaceDispatchBridgeTests
{
	static const FName ModuleName(TEXT("ASInterfaceDispatchBridge"));
	static const FString ScriptFilename(TEXT("ASInterfaceDispatchBridge.as"));
	static const FName GeneratedClassName(TEXT("AInterfaceDispatchBridgeCarrier"));

	void BindProductionInterfaceMethod(FAngelscriptBinds& Binds, const TCHAR* Declaration, const TCHAR* FunctionName)
	{
		FInterfaceMethodSignature* Signature = FAngelscriptEngine::Get().RegisterInterfaceMethodSignature(FName(FunctionName));
		Binds.GenericMethod(FString(Declaration), CallInterfaceMethod, Signature);
	}

	void EnsureProductionNativeInterfaceBound(UClass* InterfaceClass)
	{
		if (InterfaceClass == nullptr)
		{
			return;
		}

		asIScriptEngine* ScriptEngine = FAngelscriptEngine::Get().Engine;
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const FString TypeName = FAngelscriptType::GetBoundClassName(InterfaceClass);
		if (ScriptEngine->GetTypeInfoByName(TCHAR_TO_ANSI(*TypeName)) != nullptr)
		{
			return;
		}

		FAngelscriptBinds Binds = FAngelscriptBinds::ReferenceClass(TypeName, InterfaceClass);
		asCTypeInfo* TypeInfo = static_cast<asCTypeInfo*>(Binds.GetTypeInfo());
		if (TypeInfo != nullptr)
		{
			TypeInfo->plainUserData = reinterpret_cast<SIZE_T>(InterfaceClass);
		}

		BindProductionInterfaceMethod(Binds, TEXT("int GetNativeValue() const"), TEXT("GetNativeValue"));
		BindProductionInterfaceMethod(Binds, TEXT("void SetNativeMarker(FName Marker)"), TEXT("SetNativeMarker"));
		BindProductionInterfaceMethod(Binds, TEXT("void AdjustNativeValue(int Delta, int& Value)"), TEXT("AdjustNativeValue"));
	}

	void EnsureFixturesBound()
	{
		EnsureProductionNativeInterfaceBound(UAngelscriptNativeParentInterface::StaticClass());
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptInterfaceDispatchBridgeTests,
	"Angelscript.TestModule.ClassGenerator.Interface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CallInterfaceMethodDispatchesToImplementingUFunction)
	{
		using namespace InterfaceDispatchBridgeTests;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		InterfaceDispatchBridgeTests::EnsureFixturesBound();

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*InterfaceDispatchBridgeTests::ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			InterfaceDispatchBridgeTests::ModuleName,
			InterfaceDispatchBridgeTests::ScriptFilename,
			TEXT(R"AS(
UCLASS()
class AInterfaceDispatchBridgeCarrier : AActor, UAngelscriptNativeParentInterface
{
	UPROPERTY()
	int ScriptObservedValue = 0;

	UPROPERTY()
	int ScriptAdjustedValue = 0;

	UPROPERTY()
	FName ScriptObservedMarker = NAME_None;

	UFUNCTION()
	int GetNativeValue() const
	{
		return 55;
	}

	UFUNCTION()
	void SetNativeMarker(FName Marker)
	{
		ScriptObservedMarker = Marker;
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

		ScriptObservedValue = ParentRef.GetNativeValue();

		int Value = 10;
		ParentRef.AdjustNativeValue(5, Value);
		ScriptAdjustedValue = Value;

		ParentRef.SetNativeMarker(n"BridgeHit");
	}
}
)AS"),
			InterfaceDispatchBridgeTests::GeneratedClassName);
		if (ScriptClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			ScriptClass->ImplementsInterface(UAngelscriptNativeParentInterface::StaticClass()),
			TEXT("Production bridge test case should generate a class that implements the native parent interface")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();

		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		int32 ScriptObservedValue = INDEX_NONE;
		int32 ScriptAdjustedValue = INDEX_NONE;
		FName ScriptObservedMarker = NAME_None;
		if (!ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("ScriptObservedValue"), ScriptObservedValue)
			|| !ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("ScriptAdjustedValue"), ScriptAdjustedValue)
			|| !ReadPropertyValue<FNameProperty>(*TestRunner, Actor, TEXT("ScriptObservedMarker"), ScriptObservedMarker))
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			55,
			ScriptObservedValue,
			TEXT("Production bridge should dispatch GetNativeValue through the implementing UFunction")));
		ASSERT_THAT(AreEqual(
			15,
			ScriptAdjustedValue,
			TEXT("Production bridge should round-trip ref parameters through the implementing UFunction")));
		ASSERT_THAT(AreEqual(
			FName(TEXT("BridgeHit")),
			ScriptObservedMarker,
			TEXT("Production bridge should route void calls with payload arguments through the implementing UFunction")));

		}
	}
};

#endif
