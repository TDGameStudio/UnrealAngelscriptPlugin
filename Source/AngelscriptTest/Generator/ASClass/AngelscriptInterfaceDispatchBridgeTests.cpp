#include "Core/AngelscriptBinds.h"
#include "Core/AngelscriptEngine.h"
#include "AngelscriptNativeInterfaceTestTypes.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "ClassGenerator/AngelscriptClassGenerator.h"

#include "Components/ActorTestSpawner.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptInterfaceDispatchBridgeTests,
	"Angelscript.TestModule.Generator.ASClass.Interface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName ModuleName = FName(TEXT("ASInterfaceDispatchBridge"));
	inline static const FString ScriptFilename = FString(TEXT("ASInterfaceDispatchBridge.as"));
	inline static const FName GeneratedClassName = FName(TEXT("AInterfaceDispatchBridgeCarrier"));

	static void BindProductionInterfaceMethod(FAngelscriptBinds& Binds, const TCHAR* Declaration, const TCHAR* FunctionName)
	{
		FInterfaceMethodSignature* Signature =
			Binds.GetTargetEngine().RegisterInterfaceMethodSignature(FName(FunctionName));
		Binds.GenericMethod(FString(Declaration), CallInterfaceMethod, Signature);
	}

	static void EnsureProductionNativeInterfaceBound(FAngelscriptEngine& Engine, UClass* InterfaceClass)
	{
		if (InterfaceClass == nullptr)
		{
			return;
		}

		asIScriptEngine* ScriptEngine = Engine.Engine;
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const FString TypeName = FAngelscriptType::GetBoundClassName(InterfaceClass);
		if (ScriptEngine->GetTypeInfoByName(TCHAR_TO_ANSI(*TypeName)) != nullptr)
		{
			return;
		}

		FAngelscriptBinds Binds(Engine);
		FAngelscriptBinds InterfaceBinds = Binds.ReferenceClassForTarget(TypeName, InterfaceClass);
		asCTypeInfo* TypeInfo = static_cast<asCTypeInfo*>(InterfaceBinds.GetTypeInfo());
		if (TypeInfo != nullptr)
		{
			TypeInfo->plainUserData = reinterpret_cast<SIZE_T>(InterfaceClass);
		}

		BindProductionInterfaceMethod(InterfaceBinds, TEXT("int GetNativeValue() const"), TEXT("GetNativeValue"));
		BindProductionInterfaceMethod(InterfaceBinds, TEXT("void SetNativeMarker(FName Marker)"), TEXT("SetNativeMarker"));
		BindProductionInterfaceMethod(
			InterfaceBinds,
			TEXT("void AdjustNativeValue(int Delta, int& Value)"),
			TEXT("AdjustNativeValue"));
	}

	static void EnsureFixturesBound(FAngelscriptEngine& Engine)
	{
		EnsureProductionNativeInterfaceBound(Engine, UAngelscriptNativeParentInterface::StaticClass());
	}

public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(CallInterfaceMethodDispatchesToImplementingUFunction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		FAngelscriptInterfaceDispatchBridgeTests::EnsureFixturesBound(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*FAngelscriptInterfaceDispatchBridgeTests::ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
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
			)AS");

		UClass* ScriptClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			FAngelscriptInterfaceDispatchBridgeTests::ModuleName,
			FAngelscriptInterfaceDispatchBridgeTests::ScriptFilename,
			ScriptSource,
			FAngelscriptInterfaceDispatchBridgeTests::GeneratedClassName);
		if (ScriptClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			ScriptClass->ImplementsInterface(UAngelscriptNativeParentInterface::StaticClass()),
			TEXT("Production bridge test case should generate a class that implements the native parent interface")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();

		AActor* Actor = AngelscriptFunctionalTestUtils::SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		if (Actor == nullptr)
		{
			return;
		}

		AngelscriptFunctionalTestUtils::BeginPlayActor(Engine, *Actor);

		int32 ScriptObservedValue = INDEX_NONE;
		int32 ScriptAdjustedValue = INDEX_NONE;
		FName ScriptObservedMarker = NAME_None;
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("ScriptObservedValue"), ScriptObservedValue)
			|| !AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("ScriptAdjustedValue"), ScriptAdjustedValue)
			|| !AngelscriptFunctionalTestUtils::ReadPropertyValue<FNameProperty>(*TestRunner, Actor, TEXT("ScriptObservedMarker"), ScriptObservedMarker))
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
};

#endif
