#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageMixinTests
// -----------------------------------------------------------------------------
// Coverage landing file for AS mixin functions. The AS 2.33 fork used here does
// not support `mixin class`; supported production coverage is the Hazelight-style
// free-function mixin form: `mixin void Func(Receiver Self, ...)`.
//
// Existing syntax-level expectations live in Syntax/AngelscriptSyntaxMixinTests.
// This file exercises the runtime-dispatch surface that projects free functions
// as receiver methods.
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageMixinTest,
	"Angelscript.TestModule.Coverage.Mixin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(FreeFunctionMixinDispatchAndDefaults)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMixin_DispatchDefaults"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMixinDispatchDefaults.as"),
			ASTEST_AS(R"AS(
mixin void MarkReady(ACoverageMixinHostActor Self)
{
	Self.bReady = true;
}

mixin void AddScore(ACoverageMixinHostActor Self, int Amount = 5)
{
	Self.Score += Amount;
}

mixin void CopyScoreTo(ACoverageMixinHostActor Self, ACoverageMixinHostActor Other, int Bonus = 1)
{
	if (Other != nullptr)
	{
		Other.Score = Self.Score + Bonus;
	}
}

UCLASS()
class ACoverageMixinHostActor : AActor
{
	UPROPERTY()
	bool bReady = false;

	UPROPERTY()
	int Score = 0;

	UPROPERTY()
	int OtherScore = 0;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		this.MarkReady();
		this.AddScore();
		this.AddScore(7);

		ACoverageMixinHostActor Other = SpawnActor<ACoverageMixinHostActor>();
		this.CopyScoreTo(Other, 3);
		if (Other != nullptr)
		{
			OtherScore = Other.Score;
			Other.DestroyActor();
		}
	}
}
)AS"),
			TEXT("ACoverageMixinHostActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("mixin host actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("mixin host actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bReady"), true, TEXT("single-receiver mixin should mutate receiver"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Score"), 12, TEXT("default and explicit mixin args should accumulate"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("OtherScore"), 15, TEXT("multi-receiver mixin should dispatch with extra args"));
	}

	TEST_METHOD(MixinMethodsCanBeComposed)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageMixin_Composition"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageMixinComposition.as"),
			ASTEST_AS(R"AS(
mixin bool IsScoreAtLeast(ACoverageMixinCompositionActor Self, int Threshold)
{
	return Self.Score >= Threshold;
}

mixin void ClampScore(ACoverageMixinCompositionActor Self, int MinValue, int MaxValue)
{
	if (Self.Score < MinValue)
	{
		Self.Score = MinValue;
	}
	else if (Self.Score > MaxValue)
	{
		Self.Score = MaxValue;
	}
}

UCLASS()
class ACoverageMixinCompositionActor : AActor
{
	UPROPERTY()
	int Score = 0;

	UPROPERTY()
	bool bBelowBeforeClamp = false;

	UPROPERTY()
	bool bWithinAfterClamp = false;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		Score = 25;
		bBelowBeforeClamp = this.IsScoreAtLeast(30);
		this.ClampScore(0, 20);
		bWithinAfterClamp = this.IsScoreAtLeast(20);
	}
}
)AS"),
			TEXT("ACoverageMixinCompositionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("composed mixin actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("composed mixin actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bBelowBeforeClamp"), false, TEXT("query mixin should return false before clamp"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Score"), 20, TEXT("mutating mixin should clamp receiver state"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bWithinAfterClamp"), true, TEXT("query mixin should observe clamped state"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
