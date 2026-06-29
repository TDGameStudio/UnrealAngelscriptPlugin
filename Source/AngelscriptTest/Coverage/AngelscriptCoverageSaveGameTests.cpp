#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"

#include "GameFramework/SaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageSaveGameTests
// -----------------------------------------------------------------------------
// Coverage for the high-priority SaveGame slice from:
//
//   OpenSpec: test-coverage-matrix-consolidation/coverage-matrix.md
//
// Axes covered here:
//   * AS-defined USaveGame subclasses compile and expose SaveGame properties
//   * default SaveGame data is visible on CDO/new instances
//   * UGameplayStatics synchronous save/load/delete slot lifecycle preserves AS data
//   * missing-slot load returns null and existence checks remain accurate
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageSaveGameTest,
	"Angelscript.TestModule.Coverage.SaveGame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static constexpr int32 SaveGameUserIndex = 0;

	static UClass* CompileCoverageSaveGameClass(FAutomationTestBase& Test, FAngelscriptEngine& Engine, FName ModuleName)
	{
		return CompileScriptModule(
			Test,
			Engine,
			ModuleName,
			TEXT("ASCoverageSaveGame.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageSaveGameObject : USaveGame
			{
				UPROPERTY(SaveGame)
				int Progress = 17;

				UPROPERTY(SaveGame)
				FString PlayerName = "InitialPlayer";

				UPROPERTY(SaveGame)
				bool bUnlocked = false;

				UFUNCTION()
				void ApplyProgress(int NewProgress, const FString& NewPlayerName, bool bNewUnlocked)
				{
					Progress = NewProgress;
					PlayerName = NewPlayerName;
					bUnlocked = bNewUnlocked;
				}
			}
			)AS"),
			TEXT("UCoverageSaveGameObject"));
	}

	static bool InvokeApplyProgress(
		FAutomationTestBase& Test,
		UObject* SaveGameObject,
		int32 Progress,
		const FString& PlayerName,
		bool bUnlocked)
	{
		FFunctionInvoker Invoker(Test, SaveGameObject, TEXT("ApplyProgress"));
		if (!Invoker.IsValid())
		{
			return false;
		}

		return Invoker
			.AddParam<int32>(Progress)
			.AddParam<FString>(PlayerName)
			.AddParam<bool>(bUnlocked)
			.Call();
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

	TEST_METHOD(SaveGameSubclassAndProperties)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSaveGame_SubclassAndProperties"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* SaveGameClass = CompileCoverageSaveGameClass(*TestRunner, Engine, ModuleName);
		if (SaveGameClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(SaveGameClass->IsChildOf(USaveGame::StaticClass()),
			TEXT("AS SaveGame coverage class should derive from USaveGame")));

		FIntProperty* ProgressProperty = FindFProperty<FIntProperty>(SaveGameClass, TEXT("Progress"));
		FStrProperty* PlayerNameProperty = FindFProperty<FStrProperty>(SaveGameClass, TEXT("PlayerName"));
		FBoolProperty* UnlockedProperty = FindFProperty<FBoolProperty>(SaveGameClass, TEXT("bUnlocked"));
		ASSERT_THAT(IsNotNull(ProgressProperty, TEXT("Progress SaveGame property should exist")));
		ASSERT_THAT(IsNotNull(PlayerNameProperty, TEXT("PlayerName SaveGame property should exist")));
		ASSERT_THAT(IsNotNull(UnlockedProperty, TEXT("bUnlocked SaveGame property should exist")));
		if (ProgressProperty == nullptr
			|| PlayerNameProperty == nullptr
			|| UnlockedProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ProgressProperty->HasAnyPropertyFlags(CPF_SaveGame),
			TEXT("Progress should carry CPF_SaveGame")));
		ASSERT_THAT(IsTrue(PlayerNameProperty->HasAnyPropertyFlags(CPF_SaveGame),
			TEXT("PlayerName should carry CPF_SaveGame")));
		ASSERT_THAT(IsTrue(UnlockedProperty->HasAnyPropertyFlags(CPF_SaveGame),
			TEXT("bUnlocked should carry CPF_SaveGame")));

		UObject* CDO = SaveGameClass->GetDefaultObject();
		ASSERT_THAT(IsNotNull(CDO, TEXT("AS SaveGame class should expose a CDO")));
		if (CDO == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(17, ProgressProperty->GetPropertyValue_InContainer(CDO),
			TEXT("Progress CDO default should match AS initializer")));
		ASSERT_THAT(AreEqual(FString(TEXT("InitialPlayer")), PlayerNameProperty->GetPropertyValue_InContainer(CDO),
			TEXT("PlayerName CDO default should match AS initializer")));
		ASSERT_THAT(IsFalse(UnlockedProperty->GetPropertyValue_InContainer(CDO),
			TEXT("bUnlocked CDO default should match AS initializer")));
	}

	TEST_METHOD(SynchronousSlotRoundTrip)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSaveGame_SynchronousSlotRoundTrip"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* SaveGameClass = CompileCoverageSaveGameClass(*TestRunner, Engine, ModuleName);
		if (SaveGameClass == nullptr)
		{
			return;
		}

		const FString SlotName = FString::Printf(TEXT("ASCoverageSaveGame_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
		UGameplayStatics::DeleteGameInSlot(SlotName, SaveGameUserIndex);
		ON_SCOPE_EXIT
		{
			UGameplayStatics::DeleteGameInSlot(SlotName, SaveGameUserIndex);
		};

		USaveGame* SaveGame = Cast<USaveGame>(UGameplayStatics::CreateSaveGameObject(SaveGameClass));
		ASSERT_THAT(IsNotNull(SaveGame, TEXT("CreateSaveGameObject should instantiate the AS SaveGame class")));
		if (SaveGame == nullptr)
		{
			return;
		}

		if (!InvokeApplyProgress(*TestRunner, SaveGame, 91, TEXT("RoundTripPlayer"), true))
		{
			return;
		}

		ASSERT_THAT(IsTrue(UGameplayStatics::SaveGameToSlot(SaveGame, SlotName, SaveGameUserIndex),
			TEXT("SaveGameToSlot should save an AS SaveGame object")));
		ASSERT_THAT(IsTrue(UGameplayStatics::DoesSaveGameExist(SlotName, SaveGameUserIndex),
			TEXT("DoesSaveGameExist should report true after saving the slot")));

		USaveGame* LoadedSaveGame = UGameplayStatics::LoadGameFromSlot(SlotName, SaveGameUserIndex);
		ASSERT_THAT(IsNotNull(LoadedSaveGame, TEXT("LoadGameFromSlot should load the saved AS SaveGame object")));
		if (LoadedSaveGame == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(LoadedSaveGame->GetClass()->IsChildOf(SaveGameClass),
			TEXT("Loaded SaveGame should preserve the AS generated class")));

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, LoadedSaveGame, TEXT("Progress"), 91,
			TEXT("Loaded SaveGame should preserve int SaveGame data"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, LoadedSaveGame, TEXT("PlayerName"), FString(TEXT("RoundTripPlayer")),
			TEXT("Loaded SaveGame should preserve FString SaveGame data"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, LoadedSaveGame, TEXT("bUnlocked"), true,
			TEXT("Loaded SaveGame should preserve bool SaveGame data"))));

		ASSERT_THAT(IsTrue(UGameplayStatics::DeleteGameInSlot(SlotName, SaveGameUserIndex),
			TEXT("DeleteGameInSlot should remove the saved slot")));
		ASSERT_THAT(IsFalse(UGameplayStatics::DoesSaveGameExist(SlotName, SaveGameUserIndex),
			TEXT("DoesSaveGameExist should report false after deleting the slot")));
	}

	TEST_METHOD(MissingSlotReturnsNull)
	{
		const FString MissingSlotName = FString::Printf(TEXT("ASCoverageSaveGame_Missing_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
		UGameplayStatics::DeleteGameInSlot(MissingSlotName, SaveGameUserIndex);
		ON_SCOPE_EXIT
		{
			UGameplayStatics::DeleteGameInSlot(MissingSlotName, SaveGameUserIndex);
		};

		ASSERT_THAT(IsFalse(UGameplayStatics::DoesSaveGameExist(MissingSlotName, SaveGameUserIndex),
			TEXT("Missing slot should not exist before load")));
		ASSERT_THAT(IsNull(UGameplayStatics::LoadGameFromSlot(MissingSlotName, SaveGameUserIndex),
			TEXT("LoadGameFromSlot should return null for a missing slot")));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
