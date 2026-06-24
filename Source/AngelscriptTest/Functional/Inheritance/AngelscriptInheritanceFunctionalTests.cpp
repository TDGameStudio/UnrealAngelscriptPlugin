#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptBindingsAssertions.h"

#include "ClassGenerator/AngelscriptClassGenerator.h"
#include "Components/ActorTestSpawner.h"
#include "Misc/ScopeExit.h"

// Test Layer: UE Functional
#if WITH_DEV_AUTOMATION_TESTS



TEST_CLASS_WITH_FLAGS(FAngelscriptInheritanceFunctionalTests, "Angelscript.TestModule.Inheritance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
static void InitializeInheritanceTestCaseSpawner(FActorTestSpawner& Spawner)
{
	Spawner.InitializeGameSubsystems();
}

public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	TEST_METHOD(ScriptToScript)
	{
FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestInheritanceScriptToScript"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString BaselineScript = TEXT(R"AS(
UCLASS()
class ATestInheritanceBaseline : AActor
{
}
)AS");
		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, ModuleName, TEXT("TestInheritanceScriptToScript.as"), BaselineScript),
			TEXT("TestCase inheritance baseline module should compile before reload analysis")));

		FAngelscriptClassGenerator::EReloadRequirement ReloadRequirement = FAngelscriptClassGenerator::Error;
		bool bWantsFullReload = false;
		bool bNeedsFullReload = false;
		const bool bAnalyzed = AnalyzeReloadFromMemory(
			&Engine,
			ModuleName,
			TEXT("TestInheritanceScriptToScript.as"),
			TEXT(R"AS(
UCLASS()
class ATestInheritanceBase : AActor
{
	UFUNCTION()
	int GetTestCaseValue()
	{
		return 1;
	}
}

UCLASS()
class ATestInheritanceDerived : ATestCaseInheritanceBase
{
	UFUNCTION()
	int GetTestCaseValue()
	{
		return 2;
	}
}
)AS"),
			ReloadRequirement,
			bWantsFullReload,
			bNeedsFullReload);

		ASSERT_THAT(IsFalse(
			bAnalyzed,
			TEXT("TestCase script-to-script actor inheritance with overridden UFUNCTIONs remains unsupported on this branch")));

		ASSERT_THAT(AreEqual(FAngelscriptClassGenerator::Error, ReloadRequirement, TEXT("TestCase script-to-script actor inheritance should currently stay in the error state")));
	}

	TEST_METHOD(Super)
	{
FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestInheritanceSuper"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString BaselineScript = TEXT(R"AS(
UCLASS()
class ATestInheritanceSuperBaseline : AActor
{
}
)AS");
		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, ModuleName, TEXT("TestInheritanceSuper.as"), BaselineScript),
			TEXT("TestCase inheritance super baseline module should compile before reload analysis")));

		FAngelscriptClassGenerator::EReloadRequirement ReloadRequirement = FAngelscriptClassGenerator::Error;
		bool bWantsFullReload = false;
		bool bNeedsFullReload = false;
		const bool bAnalyzed = AnalyzeReloadFromMemory(
			&Engine,
			ModuleName,
			TEXT("TestInheritanceSuper.as"),
			TEXT(R"AS(
UCLASS()
class ATestInheritanceSuperBase : AActor
{
	UFUNCTION()
	int GetTestCaseValue()
	{
		return 10;
	}
}

UCLASS()
class ATestInheritanceSuperDerived : ATestCaseInheritanceSuperBase
{
	UFUNCTION()
	int GetTestCaseValue()
	{
		return Super::GetTestCaseValue() + 5;
	}
}
)AS"),
			ReloadRequirement,
			bWantsFullReload,
			bNeedsFullReload);

		ASSERT_THAT(IsFalse(
			bAnalyzed,
			TEXT("TestCase script-to-script Super calls remain unsupported on this branch")));

		ASSERT_THAT(AreEqual(FAngelscriptClassGenerator::Error, ReloadRequirement, TEXT("TestCase inheritance with Super should currently stay in the error state")));
	}

	TEST_METHOD(IsA)
	{
FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("TestInheritanceIsA"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString BaselineScript = TEXT(R"AS(
UCLASS()
class ATestInheritanceIsABaseline : AActor
{
}
)AS");
		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, ModuleName, TEXT("TestInheritanceIsA.as"), BaselineScript),
			TEXT("TestCase inheritance IsA baseline module should compile before reload analysis")));

		FAngelscriptClassGenerator::EReloadRequirement ReloadRequirement = FAngelscriptClassGenerator::Error;
		bool bWantsFullReload = false;
		bool bNeedsFullReload = false;
		const bool bAnalyzed = AnalyzeReloadFromMemory(
			&Engine,
			ModuleName,
			TEXT("TestInheritanceIsA.as"),
			TEXT(R"AS(
UCLASS()
class ATestInheritanceIsABase : AActor
{
}

UCLASS()
class ATestInheritanceIsADerived : ATestInheritanceIsABase
{
	UFUNCTION()
	int VerifyBaseCast()
	{
		ATestInheritanceIsABase BaseRef = Cast<ATestInheritanceIsABase>(this);
		return BaseRef == null ? 0 : 1;
	}
}
)AS"),
			ReloadRequirement,
			bWantsFullReload,
			bNeedsFullReload);

		ASSERT_THAT(IsTrue(
			bAnalyzed,
			TEXT("TestCase inheritance IsA/Cast syntax should analyze without crashing")));

		ASSERT_THAT(IsTrue(
			bWantsFullReload || bNeedsFullReload,
			TEXT("TestCase inheritance IsA/Cast currently requires the full-reload path on this branch")));
		ASSERT_THAT(IsTrue(
			ReloadRequirement == FAngelscriptClassGenerator::FullReloadRequired
			|| ReloadRequirement == FAngelscriptClassGenerator::FullReloadSuggested,
			TEXT("TestCase inheritance IsA/Cast should not remain on the soft-reload path")));
	}
};

#endif
