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
//   OpenSpec: test-coverage/coverage-matrix.md
//
// Axes covered here:
//   * AS-defined USaveGame subclasses compile and expose SaveGame properties
//   * default SaveGame data is visible on CDO/new instances
//   * UGameplayStatics synchronous save/load/delete slot lifecycle preserves AS data
//   * nested USTRUCT and TArray SaveGame data round-trips through slots
//   * missing-slot load returns null and existence checks remain accurate
// -----------------------------------------------------------------------------

#if WITH_ANGELSCRIPT_UNITTESTS

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

	static UClass* CompileCoverageComplexSaveGameClass(FAutomationTestBase& Test, FAngelscriptEngine& Engine, FName ModuleName)
	{
		return CompileScriptModule(
			Test,
			Engine,
			ModuleName,
			TEXT("ASCoverageComplexSaveGame.as"),
			ASTEST_AS(R"AS(
			USTRUCT()
			struct FCoverageSaveGameStats
			{
				UPROPERTY(SaveGame)
				int Level = 1;

				UPROPERTY(SaveGame)
				FString Region = "Start";

				UPROPERTY(SaveGame)
				bool bHardMode = false;
			}

			USTRUCT()
			struct FCoverageSaveGameItem
			{
				UPROPERTY(SaveGame)
				FString ItemId;

				UPROPERTY(SaveGame)
				int Quantity = 0;
			}

			UCLASS()
			class UCoverageComplexSaveGameObject : USaveGame
			{
				UPROPERTY(SaveGame)
				FCoverageSaveGameStats Stats;

				UPROPERTY(SaveGame)
				TArray<int> Milestones;

				UPROPERTY(SaveGame)
				TArray<FCoverageSaveGameItem> Inventory;

				UFUNCTION()
				void ApplyComplexProgress()
				{
					Stats.Level = 42;
					Stats.Region = "DeepSave";
					Stats.bHardMode = true;

					Milestones.Reset();
					Milestones.Add(10);
					Milestones.Add(20);
					Milestones.Add(35);

					Inventory.Reset();

					FCoverageSaveGameItem Sword;
					Sword.ItemId = "Sword";
					Sword.Quantity = 1;
					Inventory.Add(Sword);

					FCoverageSaveGameItem Potion;
					Potion.ItemId = "Potion";
					Potion.Quantity = 5;
					Inventory.Add(Potion);
				}
			}
			)AS"),
			TEXT("UCoverageComplexSaveGameObject"));
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

	static bool InvokeApplyComplexProgress(FAutomationTestBase& Test, UObject* SaveGameObject)
	{
		FFunctionInvoker Invoker(Test, SaveGameObject, TEXT("ApplyComplexProgress"));
		if (!Invoker.IsValid())
		{
			return false;
		}

		return Invoker.Call();
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

	TEST_METHOD(ComplexStructAndArraySlotRoundTrip)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageSaveGame_ComplexRoundTrip"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* SaveGameClass = CompileCoverageComplexSaveGameClass(*TestRunner, Engine, ModuleName);
		if (SaveGameClass == nullptr)
		{
			return;
		}

		FStructProperty* StatsProperty = FindFProperty<FStructProperty>(SaveGameClass, TEXT("Stats"));
		FArrayProperty* MilestonesProperty = FindFProperty<FArrayProperty>(SaveGameClass, TEXT("Milestones"));
		FArrayProperty* InventoryProperty = FindFProperty<FArrayProperty>(SaveGameClass, TEXT("Inventory"));
		ASSERT_THAT(IsNotNull(StatsProperty, TEXT("Stats SaveGame USTRUCT property should exist")));
		ASSERT_THAT(IsNotNull(MilestonesProperty, TEXT("Milestones SaveGame array property should exist")));
		ASSERT_THAT(IsNotNull(InventoryProperty, TEXT("Inventory SaveGame array property should exist")));
		if (StatsProperty == nullptr || MilestonesProperty == nullptr || InventoryProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(StatsProperty->HasAnyPropertyFlags(CPF_SaveGame),
			TEXT("Stats should carry CPF_SaveGame")));
		ASSERT_THAT(IsTrue(MilestonesProperty->HasAnyPropertyFlags(CPF_SaveGame),
			TEXT("Milestones should carry CPF_SaveGame")));
		ASSERT_THAT(IsTrue(InventoryProperty->HasAnyPropertyFlags(CPF_SaveGame),
			TEXT("Inventory should carry CPF_SaveGame")));
		ASSERT_THAT(IsNotNull(CastField<FIntProperty>(MilestonesProperty->Inner),
			TEXT("Milestones should reflect as TArray<int>")));
		ASSERT_THAT(IsNotNull(CastField<FStructProperty>(InventoryProperty->Inner),
			TEXT("Inventory should reflect as TArray<FCoverageSaveGameItem>")));

		const FString SlotName = FString::Printf(TEXT("ASCoverageSaveGame_Complex_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
		UGameplayStatics::DeleteGameInSlot(SlotName, SaveGameUserIndex);
		ON_SCOPE_EXIT
		{
			UGameplayStatics::DeleteGameInSlot(SlotName, SaveGameUserIndex);
		};

		USaveGame* SaveGame = Cast<USaveGame>(UGameplayStatics::CreateSaveGameObject(SaveGameClass));
		ASSERT_THAT(IsNotNull(SaveGame, TEXT("CreateSaveGameObject should instantiate the AS complex SaveGame class")));
		if (SaveGame == nullptr)
		{
			return;
		}

		if (!InvokeApplyComplexProgress(*TestRunner, SaveGame))
		{
			return;
		}

		ASSERT_THAT(IsTrue(UGameplayStatics::SaveGameToSlot(SaveGame, SlotName, SaveGameUserIndex),
			TEXT("SaveGameToSlot should save AS nested struct and array SaveGame data")));

		USaveGame* LoadedSaveGame = UGameplayStatics::LoadGameFromSlot(SlotName, SaveGameUserIndex);
		ASSERT_THAT(IsNotNull(LoadedSaveGame, TEXT("LoadGameFromSlot should load the complex AS SaveGame object")));
		if (LoadedSaveGame == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(LoadedSaveGame->GetClass()->IsChildOf(SaveGameClass),
			TEXT("Loaded complex SaveGame should preserve the AS generated class")));

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, LoadedSaveGame, TEXT("Stats.Level"), 42,
			TEXT("Loaded SaveGame should preserve nested USTRUCT int data"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, LoadedSaveGame, TEXT("Stats.Region"), FString(TEXT("DeepSave")),
			TEXT("Loaded SaveGame should preserve nested USTRUCT FString data"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, LoadedSaveGame, TEXT("Stats.bHardMode"), true,
			TEXT("Loaded SaveGame should preserve nested USTRUCT bool data"))));

		int32 MilestoneCount = 0;
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, LoadedSaveGame, TEXT("Milestones"), MilestoneCount),
			TEXT("Loaded SaveGame should preserve Milestones array size")));
		ASSERT_THAT(AreEqual(3, MilestoneCount, TEXT("Loaded Milestones array should contain three entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, LoadedSaveGame, TEXT("Milestones[0]"), 10,
			TEXT("Loaded SaveGame should preserve first array entry"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, LoadedSaveGame, TEXT("Milestones[2]"), 35,
			TEXT("Loaded SaveGame should preserve last array entry"))));

		int32 InventoryCount = 0;
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, LoadedSaveGame, TEXT("Inventory"), InventoryCount),
			TEXT("Loaded SaveGame should preserve Inventory array size")));
		ASSERT_THAT(AreEqual(2, InventoryCount, TEXT("Loaded Inventory array should contain two entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, LoadedSaveGame, TEXT("Inventory[0].ItemId"), FString(TEXT("Sword")),
			TEXT("Loaded SaveGame should preserve first struct-array string field"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, LoadedSaveGame, TEXT("Inventory[1].Quantity"), 5,
			TEXT("Loaded SaveGame should preserve second struct-array int field"))));
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

#endif // WITH_ANGELSCRIPT_UNITTESTS
