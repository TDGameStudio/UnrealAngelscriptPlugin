#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptNativeInterfaceTestHelpers.h"
#include "AngelscriptNativeInterfaceTestTypes.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "Components/ActorTestSpawner.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadInterfaceTests,
	"Angelscript.TestModule.HotReload.Interface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName NativeInterfaceModuleName = FName(TEXT("HotReloadInterfaceNativeBridge"));
	inline static const FString NativeInterfaceFilename = FString(TEXT("HotReloadInterfaceNativeBridge.as"));
	inline static const FName NativeInterfaceClassName = FName(TEXT("AHotReloadInterfaceNativeBridgeActor"));

	static bool IsHandledReloadResult(const ECompileResult ReloadResult)
	{
		return ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled;
	}

	static bool ReadMarker(FAutomationTestBase& Test, UObject* Object, FName& OutMarker, const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		const bool bRead = AngelscriptFunctionalTestUtils::ReadPropertyValue<FNameProperty>(Test, Object, TEXT("NativeMarker"), OutMarker);
		return LocalAssert.IsTrue(bRead, *FString::Printf(TEXT("%s should read NativeMarker"), Context));
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

	TEST_METHOD(NativeInterfaceDispatchUsesReloadedSoftBody)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		AngelscriptNativeInterfaceTestHelpers::EnsureNativeInterfaceBound(UAngelscriptNativeParentInterface::StaticClass());

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*NativeInterfaceModuleName.ToString());
		};

		const FString ReloadV1Source = ASTEST_AS(R"AS(
			UCLASS()
			class AHotReloadInterfaceNativeBridgeActor : AActor, UAngelscriptNativeParentInterface
			{
				UPROPERTY()
				int NativeValue = 10;

				UPROPERTY()
				FName NativeMarker = NAME_None;

				UFUNCTION()
				int GetNativeValue() const
				{
					return NativeValue + 1;
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
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, NativeInterfaceModuleName, NativeInterfaceFilename, ReloadV1Source),
			TEXT("Initial native-interface hot reload module should compile")));

		UClass* ClassBeforeReload = FindGeneratedClass(&Engine, NativeInterfaceClassName);
		ASSERT_THAT(IsNotNull(ClassBeforeReload, TEXT("Native-interface class should exist before reload")));
		ASSERT_THAT(IsTrue(ClassBeforeReload->ImplementsInterface(UAngelscriptNativeParentInterface::StaticClass()), TEXT("Native-interface class should implement the parent interface before reload")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = AngelscriptFunctionalTestUtils::SpawnScriptActor(*TestRunner, Spawner, ClassBeforeReload);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Native-interface actor should spawn before reload")));

		ASSERT_THAT(AreEqual(11, IAngelscriptNativeParentInterface::Execute_GetNativeValue(Actor), TEXT("C++ interface Execute_ should dispatch to V1 GetNativeValue")));

		int32 AdjustedValueBeforeReload = 5;
		IAngelscriptNativeParentInterface::Execute_AdjustNativeValue(Actor, 3, AdjustedValueBeforeReload);
		ASSERT_THAT(AreEqual(8, AdjustedValueBeforeReload, TEXT("C++ interface Execute_ should dispatch to V1 ref parameter body")));

		IAngelscriptNativeParentInterface::Execute_SetNativeMarker(Actor, TEXT("BeforeReload"));
		FName MarkerBeforeReload = NAME_None;
		ASSERT_THAT(IsTrue(ReadMarker(*TestRunner, Actor, MarkerBeforeReload, TEXT("Native-interface V1"))));
		ASSERT_THAT(AreEqual(FName(TEXT("BeforeReload")), MarkerBeforeReload, TEXT("C++ interface Execute_ should dispatch to V1 setter")));

		const FString ReloadV2Source = ASTEST_AS(R"AS(
			UCLASS()
			class AHotReloadInterfaceNativeBridgeActor : AActor, UAngelscriptNativeParentInterface
			{
				UPROPERTY()
				int NativeValue = 10;

				UPROPERTY()
				FName NativeMarker = NAME_None;

				UFUNCTION()
				int GetNativeValue() const
				{
					return NativeValue + 25;
				}

				UFUNCTION()
				void SetNativeMarker(FName Marker)
				{
					NativeMarker = n"Reloaded";
				}

				UFUNCTION()
				void AdjustNativeValue(int Delta, int& Value)
				{
					Value += Delta * 2;
				}
			}
			)AS");

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, NativeInterfaceModuleName, NativeInterfaceFilename, ReloadV2Source, ReloadResult),
			TEXT("Native-interface soft reload should compile")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(ReloadResult), TEXT("Native-interface soft reload should be handled")));

		UClass* ClassAfterReload = FindGeneratedClass(&Engine, NativeInterfaceClassName);
		ASSERT_THAT(IsNotNull(ClassAfterReload, TEXT("Native-interface class should exist after reload")));
		ASSERT_THAT(AreEqual(ClassBeforeReload, ClassAfterReload, TEXT("Soft reload should preserve native-interface UClass identity")));
		ASSERT_THAT(IsTrue(ClassAfterReload->ImplementsInterface(UAngelscriptNativeParentInterface::StaticClass()), TEXT("Native-interface class should still implement the parent interface after reload")));

		ASSERT_THAT(AreEqual(35, IAngelscriptNativeParentInterface::Execute_GetNativeValue(Actor), TEXT("Existing actor should dispatch interface GetNativeValue to the reloaded body")));

		int32 AdjustedValueAfterReload = 5;
		IAngelscriptNativeParentInterface::Execute_AdjustNativeValue(Actor, 3, AdjustedValueAfterReload);
		ASSERT_THAT(AreEqual(11, AdjustedValueAfterReload, TEXT("Existing actor should dispatch interface ref parameter call to the reloaded body")));

		IAngelscriptNativeParentInterface::Execute_SetNativeMarker(Actor, TEXT("Ignored"));
		FName MarkerAfterReload = NAME_None;
		ASSERT_THAT(IsTrue(ReadMarker(*TestRunner, Actor, MarkerAfterReload, TEXT("Native-interface V2"))));
		ASSERT_THAT(AreEqual(FName(TEXT("Reloaded")), MarkerAfterReload, TEXT("Existing actor should dispatch interface setter to the reloaded body")));
	}
};

#endif
