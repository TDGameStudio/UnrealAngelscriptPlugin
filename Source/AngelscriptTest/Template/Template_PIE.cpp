#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

namespace TemplatePIETest
{
	constexpr double DefaultTimeoutSeconds = 10.0;

	UWorld* FindPIEWorld()
	{
		if (GEditor != nullptr)
		{
			if (FWorldContext* PIEWorldContext = GEditor->GetPIEWorldContext())
			{
				if (UWorld* World = PIEWorldContext->World())
				{
					return World;
				}
			}
		}

		if (GEngine == nullptr)
		{
			return nullptr;
		}

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World() != nullptr)
			{
				return Context.World();
			}
		}

		return nullptr;
	}

	bool IsPIEWorldAlive()
	{
		return FindPIEWorld() != nullptr || (GEditor != nullptr && GEditor->PlayWorld != nullptr);
	}

	UWorld* CreateTransientEmptyMap(FAutomationTestBase& Test)
	{
		UWorld* EditorWorld = FAutomationEditorCommonUtils::CreateNewMap();
		if (!Test.TestNotNull(TEXT("Template_PIE should create a transient empty editor map"), EditorWorld))
		{
			return nullptr;
		}

		Test.TestTrue(TEXT("Template_PIE editor map should be an editor world"), EditorWorld->WorldType == EWorldType::Editor);
		Test.TestNotNull(TEXT("Template_PIE editor map should have a persistent level"), EditorWorld->PersistentLevel.Get());
		return EditorWorld;
	}

	class FWaitForPIEWorldCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FWaitForPIEWorldCommand(FAutomationTestBase& InTest, double InTimeoutSeconds = DefaultTimeoutSeconds)
			: Test(InTest)
			, TimeoutSeconds(InTimeoutSeconds)
		{
		}

		virtual bool Update() override
		{
			if (UWorld* PIEWorld = FindPIEWorld())
			{
				Test.TestTrue(TEXT("Template_PIE should run in a PIE world"), PIEWorld->WorldType == EWorldType::PIE);
				Test.TestNotNull(TEXT("Template_PIE PIE world should have a persistent level"), PIEWorld->PersistentLevel.Get());
				return true;
			}

			if (GetCurrentRunTime() > TimeoutSeconds)
			{
				Test.AddError(FString::Printf(TEXT("Template_PIE timed out after %.1f seconds waiting for PIE world creation."), TimeoutSeconds));
				return true;
			}

			return false;
		}

	private:
		FAutomationTestBase& Test;
		double TimeoutSeconds;
	};

	class FWaitForPIEEndCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FWaitForPIEEndCommand(FAutomationTestBase& InTest, double InTimeoutSeconds = DefaultTimeoutSeconds)
			: Test(InTest)
			, TimeoutSeconds(InTimeoutSeconds)
		{
		}

		virtual bool Update() override
		{
			if (!IsPIEWorldAlive())
			{
				return true;
			}

			if (GetCurrentRunTime() > TimeoutSeconds)
			{
				Test.AddError(FString::Printf(TEXT("Template_PIE timed out after %.1f seconds waiting for PIE shutdown."), TimeoutSeconds));
				return true;
			}

			return false;
		}

	private:
		FAutomationTestBase& Test;
		double TimeoutSeconds;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTemplatePIETest,
	"Angelscript.Template.PIE.EmptyMap_StartPIE_EndPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptTemplatePIETest::RunTest(const FString& Parameters)
{
	using namespace TemplatePIETest;

	if (IsPIEWorldAlive())
	{
		AddError(TEXT("Template_PIE must start from editor mode with no existing PIE session."));
		return false;
	}

	if (CreateTransientEmptyMap(*this) == nullptr)
	{
		return false;
	}

	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEWorldCommand(*this));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this]() -> bool
	{
		UWorld* PIEWorld = TemplatePIETest::FindPIEWorld();
		if (!TestNotNull(TEXT("Template_PIE should expose a PIE world for assertions"), PIEWorld))
		{
			return true;
		}

		TestTrue(TEXT("Template_PIE world type should be PIE"), PIEWorld->WorldType == EWorldType::PIE);
		TestNotNull(TEXT("Template_PIE PIE world should have a persistent level"), PIEWorld->PersistentLevel.Get());

		if (GEditor != nullptr && GEditor->PlayWorld != nullptr)
		{
			TestTrue(TEXT("Template_PIE PlayWorld should match the discovered PIE world"), GEditor->PlayWorld == PIEWorld);
		}

		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEEndCommand(*this));

	return true;
}

#endif
