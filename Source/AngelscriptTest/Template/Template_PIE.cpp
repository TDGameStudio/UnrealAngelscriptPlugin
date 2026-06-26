#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"
#include "Editor/AngelscriptPIETestUtils.h"

#include "Engine/LevelScriptActor.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Tests/AutomationEditorCommon.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

TEST_CLASS_WITH_FLAGS(FAngelscriptTemplatePIETest,
	"Angelscript.Template.PIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName ModuleName = TEXT("ASTemplatePIE");
	inline static const FString Filename = TEXT("ASTemplatePIE.as");
	inline static const FName GameModeClassName = TEXT("ATemplatePIEGameMode");
	inline static const FName LevelScriptClassName = TEXT("ATemplatePIELevelScriptParent");
	static constexpr double DefaultTimeoutSeconds = 10.0;

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
				FRequestPlaySessionParams RequestParams = RequestProvider();
				if (FAutomationTestBase* CurrentTest = FAutomationTestFramework::Get().GetCurrentTest())
				{
					if (CurrentTest->HasAnyErrors())
					{
						return true;
					}
				}

				StartCommand = MakeUnique<FStartPIEForAutomationCommand>(RequestParams);
			}

			return StartCommand->Update();
		}

	private:
		TFunction<FRequestPlaySessionParams()> RequestProvider;
		TUniquePtr<FStartPIEForAutomationCommand> StartCommand;
	};

	static FString MakeScriptSource()
	{
		return ASTEST_AS(R"AS(
			UCLASS(Blueprintable)
			class ATemplatePIEGameMode : AGameModeBase
			{
			}

			UCLASS(Blueprintable, NotPlaceable)
			class ATemplatePIELevelScriptParent : ALevelScriptActor
			{
				default SetReplicates(false);

				UPROPERTY()
				int TemplateValue = 42;
			}
		)AS");
	}

	static void DiscardTemplateModule()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		Engine.DiscardModule(*ModuleName.ToString());
	}

	bool PrepareStandalonePIEFixture(const TCHAR* Context, FRequestPlaySessionParams& OutRequestParams, UClass*& OutGameModeClass, UClass*& OutLevelScriptClass)
	{
		FNoDiscardAsserter LocalAssert(*TestRunner);

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		OutGameModeClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			Filename,
			MakeScriptSource(),
			GameModeClassName);
		if (!LocalAssert.IsNotNull(OutGameModeClass, *FString::Printf(TEXT("%s should compile an AS GameMode class"), Context)))
		{
			return false;
		}

		if (!LocalAssert.IsTrue(OutGameModeClass->IsChildOf(AGameModeBase::StaticClass()), *FString::Printf(TEXT("%s AS GameMode should derive from AGameModeBase"), Context)))
		{
			return false;
		}

		OutLevelScriptClass = FindGeneratedClass(&Engine, LevelScriptClassName);
		if (!LocalAssert.IsNotNull(OutLevelScriptClass, *FString::Printf(TEXT("%s should compile an AS LevelScriptActor parent"), Context)))
		{
			return false;
		}

		if (!LocalAssert.IsTrue(OutLevelScriptClass->IsChildOf(ALevelScriptActor::StaticClass()), *FString::Printf(TEXT("%s AS LevelScript parent should derive from ALevelScriptActor"), Context)))
		{
			return false;
		}

		AngelscriptPIETestUtils::FScopedLevelScriptActorClassOverride LevelScriptOverride(OutLevelScriptClass);
		UWorld* EditorWorld = AngelscriptPIETestUtils::CreateTransientEmptyMap(*TestRunner, Context);
		if (!LocalAssert.IsNotNull(EditorWorld, *FString::Printf(TEXT("%s should create a transient editor map"), Context)))
		{
			return false;
		}

		ULevelScriptBlueprint* LevelBlueprint = AngelscriptPIETestUtils::CreateAndCompileLevelBlueprint(
			*TestRunner,
			EditorWorld,
			OutLevelScriptClass,
			Context);
		if (!LocalAssert.IsNotNull(LevelBlueprint, *FString::Printf(TEXT("%s should create a Level Blueprint with an AS parent"), Context)))
		{
			return false;
		}

		return LocalAssert.IsTrue(
			AngelscriptPIETestUtils::BuildStandalonePIERequest(*TestRunner, OutGameModeClass, OutRequestParams),
			*FString::Printf(TEXT("%s should build a standalone PIE request"), Context));
	}

	void AssertStandalonePIEWorld(UClass* GameModeClass, UClass* LevelScriptClass, const TCHAR* Context)
	{
		UWorld* PIEWorld = AngelscriptPIETestUtils::FindPIEWorld();
		ASSERT_THAT(IsNotNull(PIEWorld, *FString::Printf(TEXT("%s should expose a PIE world for assertions"), Context)));
		ASSERT_THAT(AreEqual(EWorldType::PIE, PIEWorld->WorldType, *FString::Printf(TEXT("%s world type should be PIE"), Context)));
		ASSERT_THAT(IsNotNull(PIEWorld->PersistentLevel.Get(), *FString::Printf(TEXT("%s PIE world should have a persistent level"), Context)));
		ASSERT_THAT(IsTrue(AngelscriptPIETestUtils::HasExpectedGameMode(PIEWorld, GameModeClass), *FString::Printf(TEXT("%s PIE world should use the AS GameMode class"), Context)));
		ASSERT_THAT(IsTrue(AngelscriptPIETestUtils::HasExpectedLevelScriptActor(PIEWorld, LevelScriptClass), *FString::Printf(TEXT("%s LevelScriptActor should inherit from the AS parent"), Context)));
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

	TEST_METHOD(EmptyMap_StartPIE_EndPIE_WithAngelscriptGameModeAndLevelBlueprint)
	{
		ASSERT_THAT(IsFalse(
			AngelscriptPIETestUtils::IsPIEWorldAlive(),
			TEXT("Template_PIE must start from editor mode with no existing PIE session")));

		FRequestPlaySessionParams RequestParams;
		UClass* GameModeClass = nullptr;
		UClass* LevelScriptClass = nullptr;
		if (!PrepareStandalonePIEFixture(TEXT("Template_PIE"), RequestParams, GameModeClass, LevelScriptClass))
		{
			return;
		}

		TestCommandBuilder.CleanUpWith(TEXT("Discard Template_PIE AS module"), []()
		{
			DiscardTemplateModule();
		});
		TestCommandBuilder.CleanUpWith(TEXT("End Template_PIE PIE cleanup"), []()
		{
			AngelscriptPIETestUtils::EndPIE();
		});

		if (TestRunner->HasAnyErrors())
		{
			return;
		}

		AddCommand(new FStartPIEForAutomationCommand(RequestParams));
		TestCommandBuilder
			.Until(TEXT("Wait for Template_PIE PIE world"), []()
			{
				return AngelscriptPIETestUtils::FindPIEWorld() != nullptr;
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds))
			.Then(TEXT("Assert Template_PIE PIE world"), [this, GameModeClass, LevelScriptClass]()
			{
				AssertStandalonePIEWorld(GameModeClass, LevelScriptClass, TEXT("Template_PIE"));
			})
			.Then(TEXT("End Template_PIE PIE session"), []()
			{
				AngelscriptPIETestUtils::EndPIE();
			})
			.Until(TEXT("Wait for Template_PIE PIE shutdown"), []()
			{
				return !AngelscriptPIETestUtils::IsPIEWorldAlive();
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds));
	}

	TEST_METHOD(EmptyMap_StartPIE_EndPIE_Twice_WithAngelscriptGameModeAndLevelBlueprint)
	{
		ASSERT_THAT(IsFalse(
			AngelscriptPIETestUtils::IsPIEWorldAlive(),
			TEXT("Template_PIE repeated session test must start from editor mode with no existing PIE session")));

		FRequestPlaySessionParams RequestParams;
		UClass* GameModeClass = nullptr;
		UClass* LevelScriptClass = nullptr;
		if (!PrepareStandalonePIEFixture(TEXT("Template_PIE repeated session"), RequestParams, GameModeClass, LevelScriptClass))
		{
			return;
		}

		TestCommandBuilder.CleanUpWith(TEXT("Discard Template_PIE repeated AS module"), []()
		{
			DiscardTemplateModule();
		});
		TestCommandBuilder.CleanUpWith(TEXT("End Template_PIE repeated PIE cleanup"), []()
		{
			AngelscriptPIETestUtils::EndPIE();
		});

		if (TestRunner->HasAnyErrors())
		{
			return;
		}

		QueuePIEStart([RequestParams]()
		{
			return RequestParams;
		});
		TestCommandBuilder
			.Until(TEXT("Wait for Template_PIE repeated first world"), []()
			{
				return AngelscriptPIETestUtils::FindPIEWorld() != nullptr;
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds))
			.Then(TEXT("Assert Template_PIE repeated first session"), [this, GameModeClass, LevelScriptClass]()
			{
				AssertStandalonePIEWorld(GameModeClass, LevelScriptClass, TEXT("Template_PIE repeated first session"));
			})
			.Then(TEXT("End Template_PIE repeated first session"), []()
			{
				AngelscriptPIETestUtils::EndPIE();
			})
			.Until(TEXT("Wait for Template_PIE repeated first shutdown"), []()
			{
				return !AngelscriptPIETestUtils::IsPIEWorldAlive();
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds))
			;

		QueuePIEStart([this, GameModeClass]()
		{
			FRequestPlaySessionParams RebuiltRequestParams;
			FNoDiscardAsserter LocalAssert(*TestRunner);
			const bool bBuiltRequest = LocalAssert.IsTrue(
				AngelscriptPIETestUtils::BuildStandalonePIERequest(*TestRunner, GameModeClass, RebuiltRequestParams),
				TEXT("Template_PIE repeated second session should rebuild a standalone PIE request"));
			if (!bBuiltRequest)
			{
				return FRequestPlaySessionParams();
			}

			return RebuiltRequestParams;
		});
		TestCommandBuilder
			.Until(TEXT("Wait for Template_PIE repeated second world"), []()
			{
				return AngelscriptPIETestUtils::FindPIEWorld() != nullptr;
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds))
			.Then(TEXT("Assert Template_PIE repeated second session"), [this, GameModeClass, LevelScriptClass]()
			{
				AssertStandalonePIEWorld(GameModeClass, LevelScriptClass, TEXT("Template_PIE repeated second session"));
			})
			.Then(TEXT("End Template_PIE repeated second session"), []()
			{
				AngelscriptPIETestUtils::EndPIE();
			})
			.Until(TEXT("Wait for Template_PIE repeated second shutdown"), []()
			{
				return !AngelscriptPIETestUtils::IsPIEWorldAlive();
			}, FTimespan::FromSeconds(DefaultTimeoutSeconds));
	}
};

#endif
