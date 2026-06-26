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

	static void DiscardTemplateModule()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		Engine.DiscardModule(*ModuleName.ToString());
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

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
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

		UClass* GameModeClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			Filename,
			ScriptSource,
			GameModeClassName);
		ASSERT_THAT(IsNotNull(GameModeClass, TEXT("Template_PIE should compile an AS GameMode class")));
		ASSERT_THAT(IsTrue(GameModeClass->IsChildOf(AGameModeBase::StaticClass()), TEXT("Template_PIE AS GameMode should derive from AGameModeBase")));

		UClass* LevelScriptClass = FindGeneratedClass(&Engine, LevelScriptClassName);
		ASSERT_THAT(IsNotNull(LevelScriptClass, TEXT("Template_PIE should compile an AS LevelScriptActor parent")));
		ASSERT_THAT(IsTrue(LevelScriptClass->IsChildOf(ALevelScriptActor::StaticClass()), TEXT("Template_PIE AS LevelScript parent should derive from ALevelScriptActor")));

		AngelscriptPIETestUtils::FScopedLevelScriptActorClassOverride LevelScriptOverride(LevelScriptClass);
		UWorld* EditorWorld = AngelscriptPIETestUtils::CreateTransientEmptyMap(*TestRunner, TEXT("Template_PIE"));
		ASSERT_THAT(IsNotNull(EditorWorld, TEXT("Template_PIE should create a transient editor map")));

		ULevelScriptBlueprint* LevelBlueprint = AngelscriptPIETestUtils::CreateAndCompileLevelBlueprint(
			*TestRunner,
			EditorWorld,
			LevelScriptClass,
			TEXT("Template_PIE"));
		ASSERT_THAT(IsNotNull(LevelBlueprint, TEXT("Template_PIE should create a Level Blueprint with an AS parent")));

		FRequestPlaySessionParams RequestParams;
		ASSERT_THAT(IsTrue(
			AngelscriptPIETestUtils::BuildStandalonePIERequest(*TestRunner, GameModeClass, RequestParams),
			TEXT("Template_PIE should build a standalone PIE request")));

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
				UWorld* PIEWorld = AngelscriptPIETestUtils::FindPIEWorld();
				ASSERT_THAT(IsNotNull(PIEWorld, TEXT("Template_PIE should expose a PIE world for assertions")));
				ASSERT_THAT(AreEqual(EWorldType::PIE, PIEWorld->WorldType, TEXT("Template_PIE world type should be PIE")));
				ASSERT_THAT(IsNotNull(PIEWorld->PersistentLevel.Get(), TEXT("Template_PIE PIE world should have a persistent level")));
				ASSERT_THAT(IsTrue(AngelscriptPIETestUtils::HasExpectedGameMode(PIEWorld, GameModeClass), TEXT("Template_PIE PIE world should use the AS GameMode class")));
				ASSERT_THAT(IsTrue(AngelscriptPIETestUtils::HasExpectedLevelScriptActor(PIEWorld, LevelScriptClass), TEXT("Template_PIE LevelScriptActor should inherit from the AS parent")));
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
};

#endif
