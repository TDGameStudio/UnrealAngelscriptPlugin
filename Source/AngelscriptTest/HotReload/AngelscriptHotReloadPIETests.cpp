#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"
#include "ClassGenerator/ASClass.h"
#include "Editor/AngelscriptPIETestUtils.h"

#include "Engine/LevelScriptActor.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadPIETests,
	"Angelscript.TestModule.HotReload.PIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FString BeforePIEFilename = TEXT("ASHotReloadPIEBefore.as");
	inline static const FName BeforePIEModuleName = TEXT("ASHotReloadPIEBefore");
	inline static const FName BeforePIEGameModeClassName = TEXT("AHotReloadPIEBeforeGameMode");
	inline static const FName BeforePIELevelScriptClassName = TEXT("AHotReloadPIEBeforeLevelScript");

	inline static const FString DuringPIESoftFilename = TEXT("ASHotReloadPIEDuringSoft.as");
	inline static const FName DuringPIESoftModuleName = TEXT("ASHotReloadPIEDuringSoft");
	inline static const FName DuringPIESoftGameModeClassName = TEXT("AHotReloadPIEDuringSoftGameMode");
	inline static const FName DuringPIESoftLevelScriptClassName = TEXT("AHotReloadPIEDuringSoftLevelScript");

	inline static const FString DuringPIESuggestedFilename = TEXT("ASHotReloadPIEDuringSuggested.as");
	inline static const FName DuringPIESuggestedModuleName = TEXT("ASHotReloadPIEDuringSuggested");
	inline static const FName DuringPIESuggestedGameModeClassName = TEXT("AHotReloadPIEDuringSuggestedGameMode");
	inline static const FName DuringPIESuggestedLevelScriptClassName = TEXT("AHotReloadPIEDuringSuggestedLevelScript");

	inline static const FString DuringPIERequiredFilename = TEXT("ASHotReloadPIEDuringRequired.as");
	inline static const FName DuringPIERequiredModuleName = TEXT("ASHotReloadPIEDuringRequired");
	inline static const FName DuringPIERequiredGameModeClassName = TEXT("AHotReloadPIEDuringRequiredGameMode");
	inline static const FName DuringPIERequiredLevelScriptClassName = TEXT("AHotReloadPIEDuringRequiredLevelScript");

	inline static const FString AfterPIEFilename = TEXT("ASHotReloadPIEAfter.as");
	inline static const FName AfterPIEModuleName = TEXT("ASHotReloadPIEAfter");
	inline static const FName AfterPIEGameModeClassName = TEXT("AHotReloadPIEAfterGameMode");
	inline static const FName AfterPIELevelScriptClassName = TEXT("AHotReloadPIEAfterLevelScript");

	inline static const FString SequenceFilename = TEXT("ASHotReloadPIESequence.as");
	inline static const FName SequenceModuleName = TEXT("ASHotReloadPIESequence");
	inline static const FName SequenceGameModeClassName = TEXT("AHotReloadPIESequenceGameMode");
	inline static const FName SequenceLevelScriptClassName = TEXT("AHotReloadPIESequenceLevelScript");

	static constexpr double DefaultTimeoutSeconds = 10.0;

	struct FPIEFixture
	{
		UClass* GameModeClass = nullptr;
		UClass* LevelScriptClass = nullptr;
		UASClass* InitialLevelScriptASClass = nullptr;
		UWorld* EditorWorld = nullptr;
		ULevelScriptBlueprint* LevelBlueprint = nullptr;
		FRequestPlaySessionParams RequestParams;
	};

	class FStartPIEFromRequestProvider : public IAutomationLatentCommand
	{
	public:
		explicit FStartPIEFromRequestProvider(TFunction<FRequestPlaySessionParams()> InRequestProvider)
			: RequestProvider(MoveTemp(InRequestProvider))
		{
		}

		bool Update() override
		{
			if (!StartCommand.IsValid())
			{
				StartCommand = MakeUnique<FStartPIEForAutomationCommand>(RequestProvider());
			}

			return StartCommand->Update();
		}

	private:
		TFunction<FRequestPlaySessionParams()> RequestProvider;
		TUniquePtr<FStartPIEForAutomationCommand> StartCommand;
	};

	static bool IsHandledReloadResult(const ECompileResult ReloadResult)
	{
		return ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled;
	}

	static void DiscardModule(const FName ModuleName)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		Engine.DiscardModule(*ModuleName.ToString());
	}

	static bool PreparePIEFixture(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FName ModuleName,
		const FString& Filename,
		const FString& ScriptSource,
		const FName GameModeClassName,
		const FName LevelScriptClassName,
		const TCHAR* Context,
		FPIEFixture& OutFixture)
	{
		FNoDiscardAsserter LocalAssert(Test);

		OutFixture.GameModeClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			Test,
			Engine,
			ModuleName,
			Filename,
			ScriptSource,
			GameModeClassName);
		if (!LocalAssert.IsNotNull(OutFixture.GameModeClass, *FString::Printf(TEXT("%s should compile an AS GameMode"), Context)))
		{
			return false;
		}

		if (!LocalAssert.IsTrue(
				OutFixture.GameModeClass->IsChildOf(AGameModeBase::StaticClass()),
				*FString::Printf(TEXT("%s GameMode should derive from AGameModeBase"), Context)))
		{
			return false;
		}

		OutFixture.LevelScriptClass = FindGeneratedClass(&Engine, LevelScriptClassName);
		if (!LocalAssert.IsNotNull(OutFixture.LevelScriptClass, *FString::Printf(TEXT("%s should compile an AS LevelScriptActor parent"), Context)))
		{
			return false;
		}

		if (!LocalAssert.IsTrue(
				OutFixture.LevelScriptClass->IsChildOf(ALevelScriptActor::StaticClass()),
				*FString::Printf(TEXT("%s LevelScript parent should derive from ALevelScriptActor"), Context)))
		{
			return false;
		}

		OutFixture.InitialLevelScriptASClass = Cast<UASClass>(OutFixture.LevelScriptClass);
		if (!LocalAssert.IsNotNull(OutFixture.InitialLevelScriptASClass, *FString::Printf(TEXT("%s LevelScript parent should be a UASClass"), Context)))
		{
			return false;
		}

		AngelscriptPIETestUtils::FScopedLevelScriptActorClassOverride LevelScriptOverride(OutFixture.LevelScriptClass);
		OutFixture.EditorWorld = AngelscriptPIETestUtils::CreateTransientEmptyMap(Test, Context);
		if (!LocalAssert.IsNotNull(OutFixture.EditorWorld, *FString::Printf(TEXT("%s should create a transient editor map"), Context)))
		{
			return false;
		}

		OutFixture.LevelBlueprint = AngelscriptPIETestUtils::CreateAndCompileLevelBlueprint(
			Test,
			OutFixture.EditorWorld,
			OutFixture.LevelScriptClass,
			Context);
		if (!LocalAssert.IsNotNull(OutFixture.LevelBlueprint, *FString::Printf(TEXT("%s should create a Level Blueprint"), Context)))
		{
			return false;
		}

		return AngelscriptPIETestUtils::BuildStandalonePIERequest(Test, OutFixture.GameModeClass, OutFixture.RequestParams);
	}

	static bool RebuildStandalonePIERequest(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FName GameModeClassName,
		const TCHAR* Context,
		FRequestPlaySessionParams& OutRequestParams)
	{
		FNoDiscardAsserter LocalAssert(Test);
		UClass* ReloadedGameModeClass = FindGeneratedClass(&Engine, GameModeClassName);
		if (!LocalAssert.IsNotNull(ReloadedGameModeClass, *FString::Printf(TEXT("%s should resolve current GameMode class"), Context)))
		{
			return false;
		}

		OutRequestParams = FRequestPlaySessionParams();
		return AngelscriptPIETestUtils::BuildStandalonePIERequest(Test, ReloadedGameModeClass, OutRequestParams);
	}

	static ALevelScriptActor* GetPIELevelScriptActor(FAutomationTestBase& Test, UWorld* PIEWorld, const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(PIEWorld, *FString::Printf(TEXT("%s should have a PIE world"), Context)))
		{
			return nullptr;
		}

		if (!LocalAssert.IsNotNull(PIEWorld->PersistentLevel.Get(), *FString::Printf(TEXT("%s PIE world should have a persistent level"), Context)))
		{
			return nullptr;
		}

		ALevelScriptActor* LevelScriptActor = PIEWorld->PersistentLevel->GetLevelScriptActor();
		return LocalAssert.IsNotNull(LevelScriptActor, *FString::Printf(TEXT("%s should expose a PIE LevelScriptActor"), Context))
			? LevelScriptActor
			: nullptr;
	}

	static bool AssertPIEWorldUsesClasses(
		FAutomationTestBase& Test,
		UWorld* PIEWorld,
		UClass* ExpectedGameModeClass,
		UClass* ExpectedLevelScriptParentClass,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(PIEWorld, *FString::Printf(TEXT("%s should expose a PIE world"), Context)))
		{
			return false;
		}

		if (!LocalAssert.AreEqual(EWorldType::PIE, PIEWorld->WorldType, *FString::Printf(TEXT("%s world type should be PIE"), Context)))
		{
			return false;
		}

		if (!LocalAssert.IsNotNull(PIEWorld->PersistentLevel.Get(), *FString::Printf(TEXT("%s PIE world should have a persistent level"), Context)))
		{
			return false;
		}

		if (!LocalAssert.IsTrue(
				AngelscriptPIETestUtils::HasExpectedGameMode(PIEWorld, ExpectedGameModeClass),
				*FString::Printf(TEXT("%s PIE world should use the expected GameMode"), Context)))
		{
			return false;
		}

		ALevelScriptActor* LevelScriptActor = GetPIELevelScriptActor(Test, PIEWorld, Context);
		if (!LocalAssert.IsNotNull(LevelScriptActor, *FString::Printf(TEXT("%s PIE world should expose a LevelScriptActor"), Context)))
		{
			return false;
		}

		UASClass* ASParent = UASClass::GetFirstASClass(LevelScriptActor->GetClass());
		if (!LocalAssert.IsNotNull(ASParent, *FString::Printf(TEXT("%s PIE LevelScriptActor should resolve an AS parent"), Context)))
		{
			return false;
		}

		return LocalAssert.AreEqual(
			ExpectedLevelScriptParentClass,
			ASParent->GetMostUpToDateClass(),
			*FString::Printf(TEXT("%s PIE LevelScriptActor should resolve the expected most up-to-date AS parent"), Context));
	}

	static bool InvokeGetValue(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		UObject* Object,
		UClass* OwnerClass,
		const int32 ExpectedValue,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(Object, *FString::Printf(TEXT("%s should have a target object"), Context)))
		{
			return false;
		}

		if (!LocalAssert.IsNotNull(OwnerClass, *FString::Printf(TEXT("%s should have an owner class"), Context)))
		{
			return false;
		}

		UFunction* GetValueFunction = FindGeneratedFunction(OwnerClass, TEXT("GetValue"));
		if (!LocalAssert.IsNotNull(GetValueFunction, *FString::Printf(TEXT("%s should expose GetValue"), Context)))
		{
			return false;
		}

		int32 ActualValue = 0;
		if (!LocalAssert.IsTrue(
				ExecuteGeneratedIntEventOnGameThread(&Engine, Object, GetValueFunction, ActualValue),
				*FString::Printf(TEXT("%s should execute GetValue"), Context)))
		{
			return false;
		}

		return LocalAssert.AreEqual(
			ExpectedValue,
			ActualValue,
			*FString::Printf(TEXT("%s should observe the expected GetValue result"), Context));
	}

	static bool ReadIntProperty(
		FAutomationTestBase& Test,
		UObject* Object,
		const FName PropertyName,
		int32& OutValue,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		const bool bRead = AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(Test, Object, PropertyName, OutValue);
		return LocalAssert.IsTrue(bRead, *FString::Printf(TEXT("%s should read property '%s'"), Context, *PropertyName.ToString()));
	}

	static bool LevelBlueprintParentChainResolvesTo(
		FAutomationTestBase& Test,
		UClass* LevelBlueprintClass,
		UClass* ExpectedMostUpToDateClass,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		UASClass* ASParent = UASClass::GetFirstASClass(LevelBlueprintClass);
		if (!LocalAssert.IsNotNull(ASParent, *FString::Printf(TEXT("%s should resolve an AS parent through the Level Blueprint parent chain"), Context)))
		{
			return false;
		}

		return LocalAssert.AreEqual(
			ExpectedMostUpToDateClass,
			ASParent->GetMostUpToDateClass(),
			*FString::Printf(TEXT("%s should resolve the expected most up-to-date AS parent"), Context));
	}

	static bool RecompileLevelBlueprint(FAutomationTestBase& Test, ULevelScriptBlueprint* LevelBlueprint, const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(LevelBlueprint, *FString::Printf(TEXT("%s should have a Level Blueprint to compile"), Context)))
		{
			return false;
		}

		FKismetEditorUtilities::CompileBlueprint(LevelBlueprint);
		return LocalAssert.IsNotNull(LevelBlueprint->GeneratedClass.Get(), *FString::Printf(TEXT("%s should expose a generated Level Blueprint class"), Context));
	}

	void RegisterCleanup(const FName ModuleName, const TCHAR* Context)
	{
		TestCommandBuilder.CleanUpWith(TEXT("End HotReload PIE cleanup"), []()
		{
			AngelscriptPIETestUtils::EndPIE();
		});

		TestCommandBuilder.CleanUpWith(TEXT("Discard HotReload PIE AS module"), [ModuleName]()
		{
			DiscardModule(ModuleName);
		});
	}

	void QueuePIEStart(TFunction<FRequestPlaySessionParams()> RequestProvider)
	{
		AddCommand(MakeShared<FStartPIEFromRequestProvider>(MoveTemp(RequestProvider)));
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

	TEST_METHOD(ReloadBeforePIEStartsUsesReloadedScriptInPIE)
	{
		ASSERT_THAT(IsFalse(
			AngelscriptPIETestUtils::IsPIEWorldAlive(),
			TEXT("HotReload PIE before-start test must start with no active PIE session")));

		RegisterCleanup(BeforePIEModuleName, TEXT("HotReload PIE before-start"));

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		const FString ScriptV1 = ASTEST_AS(R"AS(
			UCLASS(Blueprintable)
			class AHotReloadPIEBeforeGameMode : AGameModeBase
			{
			}

			UCLASS(Blueprintable, NotPlaceable)
			class AHotReloadPIEBeforeLevelScript : ALevelScriptActor
			{
				default SetReplicates(false);

				UFUNCTION()
				int GetValue()
				{
					return 11;
				}
			}
			)AS");

		const FString ScriptV2 = ASTEST_AS(R"AS(
			UCLASS(Blueprintable)
			class AHotReloadPIEBeforeGameMode : AGameModeBase
			{
			}

			UCLASS(Blueprintable, NotPlaceable)
			class AHotReloadPIEBeforeLevelScript : ALevelScriptActor
			{
				default SetReplicates(false);

				UFUNCTION()
				int GetValue()
				{
					return 22;
				}
			}
			)AS");

		FPIEFixture Fixture;
		ASSERT_THAT(IsTrue(PreparePIEFixture(
			*TestRunner,
			Engine,
			BeforePIEModuleName,
			BeforePIEFilename,
			ScriptV1,
			BeforePIEGameModeClassName,
			BeforePIELevelScriptClassName,
			TEXT("HotReload PIE before-start"),
			Fixture)));

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, BeforePIEModuleName, BeforePIEFilename, ScriptV2, ReloadResult),
			TEXT("HotReload PIE before-start should compile the pre-PIE body-only reload")));
		ASSERT_THAT(AreEqual(ECompileResult::FullyHandled, ReloadResult, TEXT("HotReload PIE before-start should stay on the pure soft reload path")));

		UClass* ReloadedLevelScriptClass = FindGeneratedClass(&Engine, BeforePIELevelScriptClassName);
		ASSERT_THAT(IsNotNull(ReloadedLevelScriptClass, TEXT("HotReload PIE before-start should resolve the reloaded LevelScript class")));
		ASSERT_THAT(AreEqual(Fixture.LevelScriptClass, ReloadedLevelScriptClass, TEXT("HotReload PIE before-start soft reload should keep LevelScript UClass identity")));

		if (TestRunner->HasAnyErrors())
		{
			return;
		}

		AddCommand(new FStartPIEForAutomationCommand(Fixture.RequestParams));
		TestCommandBuilder
			.Until(TEXT("Wait for HotReload PIE before-start world"), []()
			{
				return AngelscriptPIETestUtils::FindPIEWorld() != nullptr;
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds))
			.Then(TEXT("Assert HotReload PIE before-start reload result"), [this, Fixture, ReloadedLevelScriptClass]()
			{
				FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
				UWorld* PIEWorld = AngelscriptPIETestUtils::FindPIEWorld();
				ASSERT_THAT(IsTrue(AssertPIEWorldUsesClasses(
					*TestRunner,
					PIEWorld,
					Fixture.GameModeClass,
					ReloadedLevelScriptClass,
					TEXT("HotReload PIE before-start"))));

				ALevelScriptActor* LevelScriptActor = GetPIELevelScriptActor(*TestRunner, PIEWorld, TEXT("HotReload PIE before-start"));
				ASSERT_THAT(IsNotNull(LevelScriptActor, TEXT("HotReload PIE before-start should expose a LevelScriptActor")));
				ASSERT_THAT(IsTrue(InvokeGetValue(
					*TestRunner,
					Engine,
					LevelScriptActor,
					ReloadedLevelScriptClass,
					22,
					TEXT("HotReload PIE before-start"))));
			})
			.Then(TEXT("End HotReload PIE before-start session"), []()
			{
				AngelscriptPIETestUtils::EndPIE();
			})
			.Until(TEXT("Wait for HotReload PIE before-start shutdown"), []()
			{
				return !AngelscriptPIETestUtils::IsPIEWorldAlive();
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds));
	}

	TEST_METHOD(SoftReloadDuringPIEUpdatesLiveLevelScriptBody)
	{
		ASSERT_THAT(IsFalse(
			AngelscriptPIETestUtils::IsPIEWorldAlive(),
			TEXT("HotReload PIE soft test must start with no active PIE session")));

		RegisterCleanup(DuringPIESoftModuleName, TEXT("HotReload PIE soft"));

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		const FString ScriptV1 = ASTEST_AS(R"AS(
			UCLASS(Blueprintable)
			class AHotReloadPIEDuringSoftGameMode : AGameModeBase
			{
			}

			UCLASS(Blueprintable, NotPlaceable)
			class AHotReloadPIEDuringSoftLevelScript : ALevelScriptActor
			{
				default SetReplicates(false);

				UFUNCTION()
				int GetValue()
				{
					return 10;
				}
			}
			)AS");

		const FString ScriptV2 = ASTEST_AS(R"AS(
			UCLASS(Blueprintable)
			class AHotReloadPIEDuringSoftGameMode : AGameModeBase
			{
			}

			UCLASS(Blueprintable, NotPlaceable)
			class AHotReloadPIEDuringSoftLevelScript : ALevelScriptActor
			{
				default SetReplicates(false);

				UFUNCTION()
				int GetValue()
				{
					return 20;
				}
			}
			)AS");

		FPIEFixture Fixture;
		ASSERT_THAT(IsTrue(PreparePIEFixture(
			*TestRunner,
			Engine,
			DuringPIESoftModuleName,
			DuringPIESoftFilename,
			ScriptV1,
			DuringPIESoftGameModeClassName,
			DuringPIESoftLevelScriptClassName,
			TEXT("HotReload PIE soft"),
			Fixture)));

		if (TestRunner->HasAnyErrors())
		{
			return;
		}

		AddCommand(new FStartPIEForAutomationCommand(Fixture.RequestParams));
		TestCommandBuilder
			.Until(TEXT("Wait for HotReload PIE soft world"), []()
			{
				return AngelscriptPIETestUtils::FindPIEWorld() != nullptr;
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds))
			.Then(TEXT("Assert HotReload PIE soft baseline"), [this, Fixture]()
			{
				FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
				UWorld* PIEWorld = AngelscriptPIETestUtils::FindPIEWorld();
				ASSERT_THAT(IsTrue(AssertPIEWorldUsesClasses(
					*TestRunner,
					PIEWorld,
					Fixture.GameModeClass,
					Fixture.LevelScriptClass,
					TEXT("HotReload PIE soft baseline"))));

				ALevelScriptActor* LevelScriptActor = GetPIELevelScriptActor(*TestRunner, PIEWorld, TEXT("HotReload PIE soft baseline"));
				ASSERT_THAT(IsNotNull(LevelScriptActor, TEXT("HotReload PIE soft baseline should expose a LevelScriptActor")));
				ASSERT_THAT(IsTrue(InvokeGetValue(
					*TestRunner,
					Engine,
					LevelScriptActor,
					Fixture.LevelScriptClass,
					10,
					TEXT("HotReload PIE soft baseline"))));
			})
			.Then(TEXT("Soft reload HotReload PIE body while PIE is running"), [this, Fixture, ScriptV2]()
			{
				FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
				ECompileResult ReloadResult = ECompileResult::Error;
				ASSERT_THAT(IsTrue(
					CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, DuringPIESoftModuleName, DuringPIESoftFilename, ScriptV2, ReloadResult),
					TEXT("HotReload PIE soft should compile the body-only reload during PIE")));
				ASSERT_THAT(AreEqual(ECompileResult::FullyHandled, ReloadResult, TEXT("HotReload PIE soft should stay on the pure soft reload path during PIE")));

				UClass* ReloadedLevelScriptClass = FindGeneratedClass(&Engine, DuringPIESoftLevelScriptClassName);
				ASSERT_THAT(IsNotNull(ReloadedLevelScriptClass, TEXT("HotReload PIE soft should resolve LevelScript class after reload")));
				ASSERT_THAT(AreEqual(Fixture.LevelScriptClass, ReloadedLevelScriptClass, TEXT("HotReload PIE soft should keep LevelScript UClass identity")));

				UWorld* PIEWorld = AngelscriptPIETestUtils::FindPIEWorld();
				ALevelScriptActor* LevelScriptActor = GetPIELevelScriptActor(*TestRunner, PIEWorld, TEXT("HotReload PIE soft after reload"));
				ASSERT_THAT(IsNotNull(LevelScriptActor, TEXT("HotReload PIE soft after reload should keep the live LevelScriptActor")));
				ASSERT_THAT(IsTrue(InvokeGetValue(
					*TestRunner,
					Engine,
					LevelScriptActor,
					ReloadedLevelScriptClass,
					20,
					TEXT("HotReload PIE soft after reload"))));
			})
			.Then(TEXT("End HotReload PIE soft session"), []()
			{
				AngelscriptPIETestUtils::EndPIE();
			})
			.Until(TEXT("Wait for HotReload PIE soft shutdown"), []()
			{
				return !AngelscriptPIETestUtils::IsPIEWorldAlive();
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds));
	}

	TEST_METHOD(SuggestedFullReloadDuringPIESoftAppliesBodyButDefersShape)
	{
		ASSERT_THAT(IsFalse(
			AngelscriptPIETestUtils::IsPIEWorldAlive(),
			TEXT("HotReload PIE suggested test must start with no active PIE session")));

		RegisterCleanup(DuringPIESuggestedModuleName, TEXT("HotReload PIE suggested"));

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		const FString ScriptV1 = ASTEST_AS(R"AS(
			UENUM(BlueprintType)
			enum class EHotReloadPIESuggestedState : uint16
			{
				Alpha = 1,
				Beta = 4
			}

			UCLASS(Blueprintable)
			class AHotReloadPIEDuringSuggestedGameMode : AGameModeBase
			{
			}

			UCLASS(Blueprintable, NotPlaceable)
			class AHotReloadPIEDuringSuggestedLevelScript : ALevelScriptActor
			{
				default SetReplicates(false);

				UPROPERTY()
				EHotReloadPIESuggestedState State;

				default State = EHotReloadPIESuggestedState::Alpha;

				UFUNCTION()
				int GetValue()
				{
					return 33;
				}
			}
			)AS");

		const FString ScriptV2 = ASTEST_AS(R"AS(
			UENUM(BlueprintType)
			enum class EHotReloadPIESuggestedState : uint16
			{
				Alpha = 1,
				Beta = 7
			}

			UCLASS(Blueprintable)
			class AHotReloadPIEDuringSuggestedGameMode : AGameModeBase
			{
			}

			UCLASS(Blueprintable, NotPlaceable)
			class AHotReloadPIEDuringSuggestedLevelScript : ALevelScriptActor
			{
				default SetReplicates(false);

				UPROPERTY()
				EHotReloadPIESuggestedState State;

				default State = EHotReloadPIESuggestedState::Alpha;

				UFUNCTION()
				int GetValue()
				{
					return 44;
				}
			}
			)AS");

		FPIEFixture Fixture;
		ASSERT_THAT(IsTrue(PreparePIEFixture(
			*TestRunner,
			Engine,
			DuringPIESuggestedModuleName,
			DuringPIESuggestedFilename,
			ScriptV1,
			DuringPIESuggestedGameModeClassName,
			DuringPIESuggestedLevelScriptClassName,
			TEXT("HotReload PIE suggested"),
			Fixture)));

		if (TestRunner->HasAnyErrors())
		{
			return;
		}

		AddCommand(new FStartPIEForAutomationCommand(Fixture.RequestParams));
		TestCommandBuilder
			.Until(TEXT("Wait for HotReload PIE suggested world"), []()
			{
				return AngelscriptPIETestUtils::FindPIEWorld() != nullptr;
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds))
			.Then(TEXT("Assert HotReload PIE suggested baseline"), [this, Fixture]()
			{
				FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
				UWorld* PIEWorld = AngelscriptPIETestUtils::FindPIEWorld();
				ALevelScriptActor* LevelScriptActor = GetPIELevelScriptActor(*TestRunner, PIEWorld, TEXT("HotReload PIE suggested baseline"));
				ASSERT_THAT(IsNotNull(LevelScriptActor, TEXT("HotReload PIE suggested baseline should expose a LevelScriptActor")));
				ASSERT_THAT(IsTrue(InvokeGetValue(
					*TestRunner,
					Engine,
					LevelScriptActor,
					Fixture.LevelScriptClass,
					33,
					TEXT("HotReload PIE suggested baseline"))));
			})
			.Then(TEXT("Apply suggested full reload while PIE is running"), [this, Fixture, ScriptV2]()
			{
				TestRunner->AddExpectedErrorPlain(TEXT("Performing a Soft Reload during PIE"), EAutomationExpectedErrorFlags::Contains, 0);

				FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
				ECompileResult ReloadResult = ECompileResult::Error;
				ASSERT_THAT(IsTrue(
					CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, DuringPIESuggestedModuleName, DuringPIESuggestedFilename, ScriptV2, ReloadResult),
					TEXT("HotReload PIE suggested should compile the suggested-full reload during PIE")));
				ASSERT_THAT(AreEqual(ECompileResult::PartiallyHandled, ReloadResult, TEXT("HotReload PIE suggested should report the deferred full reload path during PIE")));

				UClass* ReloadedLevelScriptClass = FindGeneratedClass(&Engine, DuringPIESuggestedLevelScriptClassName);
				ASSERT_THAT(IsNotNull(ReloadedLevelScriptClass, TEXT("HotReload PIE suggested should resolve LevelScript class after soft reload")));
				ASSERT_THAT(AreEqual(Fixture.LevelScriptClass, ReloadedLevelScriptClass, TEXT("HotReload PIE suggested soft reload should keep LevelScript UClass identity during PIE")));

				UWorld* PIEWorld = AngelscriptPIETestUtils::FindPIEWorld();
				ALevelScriptActor* LevelScriptActor = GetPIELevelScriptActor(*TestRunner, PIEWorld, TEXT("HotReload PIE suggested after soft reload"));
				ASSERT_THAT(IsNotNull(LevelScriptActor, TEXT("HotReload PIE suggested after soft reload should keep the live LevelScriptActor")));
				ASSERT_THAT(IsTrue(InvokeGetValue(
					*TestRunner,
					Engine,
					LevelScriptActor,
					ReloadedLevelScriptClass,
					44,
					TEXT("HotReload PIE suggested after soft reload"))));
			})
			.Then(TEXT("End HotReload PIE suggested first session"), []()
			{
				AngelscriptPIETestUtils::EndPIE();
			})
			.Until(TEXT("Wait for HotReload PIE suggested first shutdown"), []()
			{
				return !AngelscriptPIETestUtils::IsPIEWorldAlive();
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds))
			.Then(TEXT("Apply full reload after HotReload PIE suggested session"), [this, Fixture, ScriptV2]()
			{
				FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
				ECompileResult FullReloadResult = ECompileResult::Error;
				ASSERT_THAT(IsTrue(
					CompileModuleWithResult(&Engine, ECompileType::FullReload, DuringPIESuggestedModuleName, DuringPIESuggestedFilename, ScriptV2, FullReloadResult),
					TEXT("HotReload PIE suggested should accept a full reload after PIE ends")));
				ASSERT_THAT(IsTrue(IsHandledReloadResult(FullReloadResult), TEXT("HotReload PIE suggested full reload after PIE should be handled")));

				UClass* ReloadedGameModeClass = FindGeneratedClass(&Engine, DuringPIESuggestedGameModeClassName);
				ASSERT_THAT(IsNotNull(ReloadedGameModeClass, TEXT("HotReload PIE suggested should resolve GameMode after full reload")));
				UClass* ReloadedLevelScriptClass = FindGeneratedClass(&Engine, DuringPIESuggestedLevelScriptClassName);
				ASSERT_THAT(IsNotNull(ReloadedLevelScriptClass, TEXT("HotReload PIE suggested should resolve LevelScript after full reload")));

				ASSERT_THAT(IsTrue(RecompileLevelBlueprint(*TestRunner, Fixture.LevelBlueprint, TEXT("HotReload PIE suggested after full reload"))));
				ASSERT_THAT(IsTrue(LevelBlueprintParentChainResolvesTo(
					*TestRunner,
					Fixture.LevelBlueprint->GeneratedClass.Get(),
					ReloadedLevelScriptClass,
					TEXT("HotReload PIE suggested after full reload"))));
			});
	}

	TEST_METHOD(RequiredFullReloadDuringPIEKeepsOldCodeActive)
	{
		ASSERT_THAT(IsFalse(
			AngelscriptPIETestUtils::IsPIEWorldAlive(),
			TEXT("HotReload PIE required test must start with no active PIE session")));

		RegisterCleanup(DuringPIERequiredModuleName, TEXT("HotReload PIE required"));

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		const FString ScriptV1 = ASTEST_AS(R"AS(
			UCLASS(Blueprintable)
			class AHotReloadPIEDuringRequiredGameMode : AGameModeBase
			{
			}

			UCLASS(Blueprintable, NotPlaceable)
			class AHotReloadPIEDuringRequiredLevelScript : ALevelScriptActor
			{
				default SetReplicates(false);

				UFUNCTION()
				int GetValue()
				{
					return 31;
				}
			}
			)AS");

		const FString ScriptV2 = ASTEST_AS(R"AS(
			UCLASS(Blueprintable)
			class AHotReloadPIEDuringRequiredGameMode : AGameModeBase
			{
			}

			UCLASS(Blueprintable, NotPlaceable)
			class AHotReloadPIEDuringRequiredLevelScript : ALevelScriptActor
			{
				default SetReplicates(false);

				UFUNCTION()
				int GetValue(int Extra)
				{
					return 99 + Extra;
				}
			}
			)AS");

		FPIEFixture Fixture;
		ASSERT_THAT(IsTrue(PreparePIEFixture(
			*TestRunner,
			Engine,
			DuringPIERequiredModuleName,
			DuringPIERequiredFilename,
			ScriptV1,
			DuringPIERequiredGameModeClassName,
			DuringPIERequiredLevelScriptClassName,
			TEXT("HotReload PIE required"),
			Fixture)));

		if (TestRunner->HasAnyErrors())
		{
			return;
		}

		AddCommand(new FStartPIEForAutomationCommand(Fixture.RequestParams));
		TestCommandBuilder
			.Until(TEXT("Wait for HotReload PIE required world"), []()
			{
				return AngelscriptPIETestUtils::FindPIEWorld() != nullptr;
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds))
			.Then(TEXT("Assert HotReload PIE required baseline"), [this, Fixture]()
			{
				FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
				UWorld* PIEWorld = AngelscriptPIETestUtils::FindPIEWorld();
				ALevelScriptActor* LevelScriptActor = GetPIELevelScriptActor(*TestRunner, PIEWorld, TEXT("HotReload PIE required baseline"));
				ASSERT_THAT(IsNotNull(LevelScriptActor, TEXT("HotReload PIE required baseline should expose a LevelScriptActor")));
				ASSERT_THAT(IsTrue(InvokeGetValue(
					*TestRunner,
					Engine,
					LevelScriptActor,
					Fixture.LevelScriptClass,
					31,
					TEXT("HotReload PIE required baseline"))));
			})
			.Then(TEXT("Reject required full reload while PIE is running"), [this, Fixture, ScriptV2]()
			{
				TestRunner->AddExpectedErrorPlain(
					TEXT("Full Reload is required due to UPROPERTY() or UFUNCTION() changes"),
					EAutomationExpectedErrorFlags::Contains,
					0);

				FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
				ECompileResult ReloadResult = ECompileResult::FullyHandled;
				ASSERT_THAT(IsFalse(
					CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, DuringPIERequiredModuleName, DuringPIERequiredFilename, ScriptV2, ReloadResult),
					TEXT("HotReload PIE required should reject required full reload while PIE is running")));
				ASSERT_THAT(AreEqual(ECompileResult::ErrorNeedFullReload, ReloadResult, TEXT("HotReload PIE required should report ErrorNeedFullReload during PIE")));

				UClass* ClassAfterRejectedReload = FindGeneratedClass(&Engine, DuringPIERequiredLevelScriptClassName);
				ASSERT_THAT(IsNotNull(ClassAfterRejectedReload, TEXT("HotReload PIE required should keep the old LevelScript class published")));
				ASSERT_THAT(AreEqual(Fixture.LevelScriptClass, ClassAfterRejectedReload, TEXT("HotReload PIE required should keep old LevelScript UClass identity after rejected reload")));

				UWorld* PIEWorld = AngelscriptPIETestUtils::FindPIEWorld();
				ALevelScriptActor* LevelScriptActor = GetPIELevelScriptActor(*TestRunner, PIEWorld, TEXT("HotReload PIE required after rejected reload"));
				ASSERT_THAT(IsNotNull(LevelScriptActor, TEXT("HotReload PIE required after rejected reload should keep the live LevelScriptActor")));
				ASSERT_THAT(IsTrue(InvokeGetValue(
					*TestRunner,
					Engine,
					LevelScriptActor,
					ClassAfterRejectedReload,
					31,
					TEXT("HotReload PIE required after rejected reload"))));
			})
			.Then(TEXT("End HotReload PIE required session"), []()
			{
				AngelscriptPIETestUtils::EndPIE();
			})
			.Until(TEXT("Wait for HotReload PIE required shutdown"), []()
			{
				return !AngelscriptPIETestUtils::IsPIEWorldAlive();
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds));
	}

	TEST_METHOD(ReloadAfterPIEEndsAppliesToNextPIESession)
	{
		ASSERT_THAT(IsFalse(
			AngelscriptPIETestUtils::IsPIEWorldAlive(),
			TEXT("HotReload PIE after-end test must start with no active PIE session")));

		RegisterCleanup(AfterPIEModuleName, TEXT("HotReload PIE after-end"));

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		const FString ScriptV1 = ASTEST_AS(R"AS(
			UCLASS(Blueprintable)
			class AHotReloadPIEAfterGameMode : AGameModeBase
			{
			}

			UCLASS(Blueprintable, NotPlaceable)
			class AHotReloadPIEAfterLevelScript : ALevelScriptActor
			{
				default SetReplicates(false);

				UPROPERTY()
				int ExistingValue = 12;

				UFUNCTION()
				int GetValue()
				{
					return ExistingValue;
				}
			}
			)AS");

		const FString ScriptV2 = ASTEST_AS(R"AS(
			UCLASS(Blueprintable)
			class AHotReloadPIEAfterGameMode : AGameModeBase
			{
			}

			UCLASS(Blueprintable, NotPlaceable)
			class AHotReloadPIEAfterLevelScript : ALevelScriptActor
			{
				default SetReplicates(false);

				UPROPERTY()
				int ExistingValue = 12;

				UPROPERTY()
				int AddedValue = 30;

				UFUNCTION()
				int GetValue()
				{
					return ExistingValue + AddedValue;
				}
			}
			)AS");

		TSharedRef<FPIEFixture> Fixture = MakeShared<FPIEFixture>();
		ASSERT_THAT(IsTrue(PreparePIEFixture(
			*TestRunner,
			Engine,
			AfterPIEModuleName,
			AfterPIEFilename,
			ScriptV1,
			AfterPIEGameModeClassName,
			AfterPIELevelScriptClassName,
			TEXT("HotReload PIE after-end"),
			Fixture.Get())));

		if (TestRunner->HasAnyErrors())
		{
			return;
		}

		AddCommand(new FStartPIEForAutomationCommand(Fixture->RequestParams));
		TestCommandBuilder
			.Until(TEXT("Wait for HotReload PIE after-end first world"), []()
			{
				return AngelscriptPIETestUtils::FindPIEWorld() != nullptr;
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds))
			.Then(TEXT("Assert HotReload PIE after-end first session"), [this, Fixture]()
			{
				FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
				UWorld* PIEWorld = AngelscriptPIETestUtils::FindPIEWorld();
				ALevelScriptActor* LevelScriptActor = GetPIELevelScriptActor(*TestRunner, PIEWorld, TEXT("HotReload PIE after-end first session"));
				ASSERT_THAT(IsNotNull(LevelScriptActor, TEXT("HotReload PIE after-end first session should expose a LevelScriptActor")));
				ASSERT_THAT(IsTrue(InvokeGetValue(
					*TestRunner,
					Engine,
					LevelScriptActor,
					Fixture->LevelScriptClass,
					12,
					TEXT("HotReload PIE after-end first session"))));
			})
			.Then(TEXT("End HotReload PIE after-end first session"), []()
			{
				AngelscriptPIETestUtils::EndPIE();
			})
			.Until(TEXT("Wait for HotReload PIE after-end first shutdown"), []()
			{
				return !AngelscriptPIETestUtils::IsPIEWorldAlive();
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds))
			.Then(TEXT("Full reload HotReload PIE after-end after PIE shutdown"), [this, Fixture, ScriptV2]()
			{
				FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
				ECompileResult ReloadResult = ECompileResult::Error;
				ASSERT_THAT(IsTrue(
					CompileModuleWithResult(&Engine, ECompileType::FullReload, AfterPIEModuleName, AfterPIEFilename, ScriptV2, ReloadResult),
					TEXT("HotReload PIE after-end should compile a structural reload after PIE ends")));
				ASSERT_THAT(IsTrue(IsHandledReloadResult(ReloadResult), TEXT("HotReload PIE after-end full reload after PIE should be handled")));

				UClass* ReloadedLevelScriptClass = FindGeneratedClass(&Engine, AfterPIELevelScriptClassName);
				ASSERT_THAT(IsNotNull(ReloadedLevelScriptClass, TEXT("HotReload PIE after-end should resolve LevelScript after full reload")));
				ASSERT_THAT(IsTrue(ReloadedLevelScriptClass != Fixture->LevelScriptClass, TEXT("HotReload PIE after-end structural reload should replace LevelScript UClass")));
				ASSERT_THAT(AreEqual(ReloadedLevelScriptClass, Fixture->InitialLevelScriptASClass->GetMostUpToDateClass(), TEXT("HotReload PIE after-end should chain initial LevelScript to current class")));
				Fixture->LevelScriptClass = ReloadedLevelScriptClass;

				ASSERT_THAT(IsTrue(RecompileLevelBlueprint(*TestRunner, Fixture->LevelBlueprint, TEXT("HotReload PIE after-end after full reload"))));
				ASSERT_THAT(IsTrue(LevelBlueprintParentChainResolvesTo(
					*TestRunner,
					Fixture->LevelBlueprint->GeneratedClass.Get(),
					ReloadedLevelScriptClass,
					TEXT("HotReload PIE after-end after full reload"))));

				ASSERT_THAT(IsTrue(RebuildStandalonePIERequest(
					*TestRunner,
					Engine,
					AfterPIEGameModeClassName,
					TEXT("HotReload PIE after-end after full reload"),
					Fixture->RequestParams)));
			})
			;
		QueuePIEStart([Fixture]()
		{
			return Fixture->RequestParams;
		});
		TestCommandBuilder
			.Until(TEXT("Wait for HotReload PIE after-end second world"), []()
			{
				return AngelscriptPIETestUtils::FindPIEWorld() != nullptr;
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds))
			.Then(TEXT("Assert HotReload PIE after-end second session"), [this, Fixture]()
			{
				FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
				UWorld* PIEWorld = AngelscriptPIETestUtils::FindPIEWorld();
				ASSERT_THAT(IsTrue(AssertPIEWorldUsesClasses(
					*TestRunner,
					PIEWorld,
					FindGeneratedClass(&Engine, AfterPIEGameModeClassName),
					Fixture->LevelScriptClass,
					TEXT("HotReload PIE after-end second session"))));

				ALevelScriptActor* LevelScriptActor = GetPIELevelScriptActor(*TestRunner, PIEWorld, TEXT("HotReload PIE after-end second session"));
				ASSERT_THAT(IsNotNull(LevelScriptActor, TEXT("HotReload PIE after-end second session should expose a LevelScriptActor")));

				int32 AddedValue = 0;
				ASSERT_THAT(IsTrue(ReadIntProperty(*TestRunner, LevelScriptActor, TEXT("AddedValue"), AddedValue, TEXT("HotReload PIE after-end second session"))));
				ASSERT_THAT(AreEqual(30, AddedValue, TEXT("HotReload PIE after-end second session should expose the added AS property")));
				ASSERT_THAT(IsTrue(InvokeGetValue(
					*TestRunner,
					Engine,
					LevelScriptActor,
					Fixture->LevelScriptClass,
					42,
					TEXT("HotReload PIE after-end second session"))));
			})
			.Then(TEXT("End HotReload PIE after-end second session"), []()
			{
				AngelscriptPIETestUtils::EndPIE();
			})
			.Until(TEXT("Wait for HotReload PIE after-end second shutdown"), []()
			{
				return !AngelscriptPIETestUtils::IsPIEWorldAlive();
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds));
	}

	TEST_METHOD(MultiplePIESessionsAndReloadsStayConsistent)
	{
		ASSERT_THAT(IsFalse(
			AngelscriptPIETestUtils::IsPIEWorldAlive(),
			TEXT("HotReload PIE sequence test must start with no active PIE session")));

		RegisterCleanup(SequenceModuleName, TEXT("HotReload PIE sequence"));

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		const FString ScriptV1 = ASTEST_AS(R"AS(
			UCLASS(Blueprintable)
			class AHotReloadPIESequenceGameMode : AGameModeBase
			{
			}

			UCLASS(Blueprintable, NotPlaceable)
			class AHotReloadPIESequenceLevelScript : ALevelScriptActor
			{
				default SetReplicates(false);

				UFUNCTION()
				int GetValue()
				{
					return 1;
				}
			}
			)AS");

		const FString ScriptV2 = ASTEST_AS(R"AS(
			UCLASS(Blueprintable)
			class AHotReloadPIESequenceGameMode : AGameModeBase
			{
			}

			UCLASS(Blueprintable, NotPlaceable)
			class AHotReloadPIESequenceLevelScript : ALevelScriptActor
			{
				default SetReplicates(false);

				UFUNCTION()
				int GetValue()
				{
					return 2;
				}
			}
			)AS");

		const FString ScriptV3 = ASTEST_AS(R"AS(
			UCLASS(Blueprintable)
			class AHotReloadPIESequenceGameMode : AGameModeBase
			{
			}

			UCLASS(Blueprintable, NotPlaceable)
			class AHotReloadPIESequenceLevelScript : ALevelScriptActor
			{
				default SetReplicates(false);

				UFUNCTION()
				int GetValue()
				{
					return 3;
				}
			}
			)AS");

		const FString ScriptV4 = ASTEST_AS(R"AS(
			UCLASS(Blueprintable)
			class AHotReloadPIESequenceGameMode : AGameModeBase
			{
			}

			UCLASS(Blueprintable, NotPlaceable)
			class AHotReloadPIESequenceLevelScript : ALevelScriptActor
			{
				default SetReplicates(false);

				UFUNCTION()
				int GetValue()
				{
					return 4;
				}
			}
			)AS");

		TSharedRef<FPIEFixture> Fixture = MakeShared<FPIEFixture>();
		ASSERT_THAT(IsTrue(PreparePIEFixture(
			*TestRunner,
			Engine,
			SequenceModuleName,
			SequenceFilename,
			ScriptV1,
			SequenceGameModeClassName,
			SequenceLevelScriptClassName,
			TEXT("HotReload PIE sequence"),
			Fixture.Get())));

		if (TestRunner->HasAnyErrors())
		{
			return;
		}

		AddCommand(new FStartPIEForAutomationCommand(Fixture->RequestParams));
		TestCommandBuilder
			.Until(TEXT("Wait for HotReload PIE sequence first world"), []()
			{
				return AngelscriptPIETestUtils::FindPIEWorld() != nullptr;
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds))
			.Then(TEXT("Assert HotReload PIE sequence V1"), [this, Fixture]()
			{
				FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
				ALevelScriptActor* LevelScriptActor = GetPIELevelScriptActor(*TestRunner, AngelscriptPIETestUtils::FindPIEWorld(), TEXT("HotReload PIE sequence V1"));
				ASSERT_THAT(IsNotNull(LevelScriptActor, TEXT("HotReload PIE sequence V1 should expose a LevelScriptActor")));
				ASSERT_THAT(IsTrue(InvokeGetValue(*TestRunner, Engine, LevelScriptActor, Fixture->LevelScriptClass, 1, TEXT("HotReload PIE sequence V1"))));
			})
			.Then(TEXT("End HotReload PIE sequence first session"), []()
			{
				AngelscriptPIETestUtils::EndPIE();
			})
			.Until(TEXT("Wait for HotReload PIE sequence first shutdown"), []()
			{
				return !AngelscriptPIETestUtils::IsPIEWorldAlive();
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds))
			.Then(TEXT("Reload HotReload PIE sequence V2 after first session"), [this, Fixture, ScriptV2]()
			{
				FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
				ECompileResult ReloadResult = ECompileResult::Error;
				ASSERT_THAT(IsTrue(
					CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, SequenceModuleName, SequenceFilename, ScriptV2, ReloadResult),
					TEXT("HotReload PIE sequence V2 should compile after first PIE session")));
				ASSERT_THAT(AreEqual(ECompileResult::FullyHandled, ReloadResult, TEXT("HotReload PIE sequence V2 should be a pure soft reload")));
				Fixture->LevelScriptClass = FindGeneratedClass(&Engine, SequenceLevelScriptClassName);
				ASSERT_THAT(IsNotNull(Fixture->LevelScriptClass, TEXT("HotReload PIE sequence should resolve LevelScript V2")));
				ASSERT_THAT(IsTrue(RebuildStandalonePIERequest(
					*TestRunner,
					Engine,
					SequenceGameModeClassName,
					TEXT("HotReload PIE sequence V2"),
					Fixture->RequestParams)));
			})
			;
		QueuePIEStart([Fixture]()
		{
			return Fixture->RequestParams;
		});
		TestCommandBuilder
			.Until(TEXT("Wait for HotReload PIE sequence second world"), []()
			{
				return AngelscriptPIETestUtils::FindPIEWorld() != nullptr;
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds))
			.Then(TEXT("Assert HotReload PIE sequence V2"), [this, Fixture]()
			{
				FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
				ALevelScriptActor* LevelScriptActor = GetPIELevelScriptActor(*TestRunner, AngelscriptPIETestUtils::FindPIEWorld(), TEXT("HotReload PIE sequence V2"));
				ASSERT_THAT(IsNotNull(LevelScriptActor, TEXT("HotReload PIE sequence V2 should expose a LevelScriptActor")));
				ASSERT_THAT(IsTrue(InvokeGetValue(*TestRunner, Engine, LevelScriptActor, Fixture->LevelScriptClass, 2, TEXT("HotReload PIE sequence V2"))));
			})
			.Then(TEXT("Soft reload HotReload PIE sequence V3 during second session"), [this, Fixture, ScriptV3]()
			{
				FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
				ECompileResult ReloadResult = ECompileResult::Error;
				ASSERT_THAT(IsTrue(
					CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, SequenceModuleName, SequenceFilename, ScriptV3, ReloadResult),
					TEXT("HotReload PIE sequence V3 should compile during second PIE session")));
				ASSERT_THAT(AreEqual(ECompileResult::FullyHandled, ReloadResult, TEXT("HotReload PIE sequence V3 should be a pure soft reload")));
				Fixture->LevelScriptClass = FindGeneratedClass(&Engine, SequenceLevelScriptClassName);
				ASSERT_THAT(IsNotNull(Fixture->LevelScriptClass, TEXT("HotReload PIE sequence should resolve LevelScript V3")));

				ALevelScriptActor* LevelScriptActor = GetPIELevelScriptActor(*TestRunner, AngelscriptPIETestUtils::FindPIEWorld(), TEXT("HotReload PIE sequence V3"));
				ASSERT_THAT(IsNotNull(LevelScriptActor, TEXT("HotReload PIE sequence V3 should expose a LevelScriptActor")));
				ASSERT_THAT(IsTrue(InvokeGetValue(*TestRunner, Engine, LevelScriptActor, Fixture->LevelScriptClass, 3, TEXT("HotReload PIE sequence V3"))));
			})
			.Then(TEXT("End HotReload PIE sequence second session"), []()
			{
				AngelscriptPIETestUtils::EndPIE();
			})
			.Until(TEXT("Wait for HotReload PIE sequence second shutdown"), []()
			{
				return !AngelscriptPIETestUtils::IsPIEWorldAlive();
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds))
			.Then(TEXT("Reload HotReload PIE sequence V4 after second session"), [this, Fixture, ScriptV4]()
			{
				FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
				ECompileResult ReloadResult = ECompileResult::Error;
				ASSERT_THAT(IsTrue(
					CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, SequenceModuleName, SequenceFilename, ScriptV4, ReloadResult),
					TEXT("HotReload PIE sequence V4 should compile after second PIE session")));
				ASSERT_THAT(AreEqual(ECompileResult::FullyHandled, ReloadResult, TEXT("HotReload PIE sequence V4 should be a pure soft reload")));
				Fixture->LevelScriptClass = FindGeneratedClass(&Engine, SequenceLevelScriptClassName);
				ASSERT_THAT(IsNotNull(Fixture->LevelScriptClass, TEXT("HotReload PIE sequence should resolve LevelScript V4")));
				ASSERT_THAT(IsTrue(RebuildStandalonePIERequest(
					*TestRunner,
					Engine,
					SequenceGameModeClassName,
					TEXT("HotReload PIE sequence V4"),
					Fixture->RequestParams)));
			})
			;
		QueuePIEStart([Fixture]()
		{
			return Fixture->RequestParams;
		});
		TestCommandBuilder
			.Until(TEXT("Wait for HotReload PIE sequence third world"), []()
			{
				return AngelscriptPIETestUtils::FindPIEWorld() != nullptr;
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds))
			.Then(TEXT("Assert HotReload PIE sequence V4"), [this, Fixture]()
			{
				FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
				ALevelScriptActor* LevelScriptActor = GetPIELevelScriptActor(*TestRunner, AngelscriptPIETestUtils::FindPIEWorld(), TEXT("HotReload PIE sequence V4"));
				ASSERT_THAT(IsNotNull(LevelScriptActor, TEXT("HotReload PIE sequence V4 should expose a LevelScriptActor")));
				ASSERT_THAT(IsTrue(InvokeGetValue(*TestRunner, Engine, LevelScriptActor, Fixture->LevelScriptClass, 4, TEXT("HotReload PIE sequence V4"))));
			})
			.Then(TEXT("End HotReload PIE sequence third session"), []()
			{
				AngelscriptPIETestUtils::EndPIE();
			})
			.Until(TEXT("Wait for HotReload PIE sequence third shutdown"), []()
			{
				return !AngelscriptPIETestUtils::IsPIEWorldAlive();
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds));
	}
};

#endif
