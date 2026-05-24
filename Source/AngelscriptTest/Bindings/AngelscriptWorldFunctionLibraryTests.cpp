// ============================================================================
// AngelscriptWorldFunctionLibraryTests.cpp
//
// World function library binding coverage — CQTest refactor. Automation ID:
//   Angelscript.TestModule.FunctionLibraries.World.FAngelscriptWorldFunctionLibraryTest.*
//
// Sections:
//   WorldStreamingNullGuards — null world/level exception handling
//   WorldStreamingAccess    — streaming level count, order, editor visibility
//
// CQTest adaptation notes:
//   Two IMPLEMENT_SIMPLE_AUTOMATION_TEST merged into one TEST_CLASS.
//   Both tests use ASTEST_CREATE_ENGINE_FULL (requires world context via
//   FActorTestSpawner). Execution uses FAngelscriptTestExecutor and
//   Bindings/AngelscriptWorldCollisionBindingsTestHelpers.h for UObject args and exceptions.
//   $TOKEN$ → compute + ReplaceInline pattern preserved for WorldStreamingAccess.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"
#include "Bindings/AngelscriptWorldCollisionBindingsTestHelpers.h"

#include "Components/ActorTestSpawner.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Templates/Function.h"

#if WITH_DEV_AUTOMATION_TESTS



namespace AngelscriptTest_Bindings_AngelscriptWorldFunctionLibraryTests_Private
{
	static constexpr ANSICHAR ModuleName[] = "ASWorldStreamingNullGuards";
	static constexpr ANSICHAR WorldStreamingAccessModuleName[] = "ASWorldStreamingAccess";
}


TEST_CLASS_WITH_FLAGS(FAngelscriptWorldFunctionLibraryTest, "Angelscript.TestModule.FunctionLibraries.World",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(WorldStreamingNullGuards)
	{
		using namespace AngelscriptTest_Bindings_AngelscriptWorldFunctionLibraryTests_Private;
		TestRunner->AddExpectedError(TEXT("Null pointer access"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("ASWorldStreamingNullGuards"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("int GetStreamingLevelCount(UWorld) | Line 4 | Col 2"), EAutomationExpectedErrorFlags::Contains, 1, false);
		TestRunner->AddExpectedError(TEXT("bool GetLevelVisibleInEditor(ULevelStreaming) | Line 9 | Col 2"), EAutomationExpectedErrorFlags::Contains, 1, false);

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};

		asIScriptModule* Module = BuildModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT(R"(
int GetStreamingLevelCount(UWorld World)
{
	return World.GetStreamingLevels().Num();
}

bool GetLevelVisibleInEditor(ULevelStreaming Level)
{
	return Level.GetShouldBeVisibleInEditor();
}
)"));
		if (Module == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();

		AActor& ContextActor = Spawner.SpawnActor<AActor>();
		UWorld* TestWorld = ContextActor.GetWorld();
		if (!TestRunner->TestNotNull(TEXT("World function library test should access the spawned world"), TestWorld))
		{
			return;
		}

		ULevelStreamingDynamic* StreamingLevel = NewObject<ULevelStreamingDynamic>(TestWorld, TEXT("FunctionLibraryStreamingLevel"));
		if (!TestRunner->TestNotNull(TEXT("World function library test should create a streaming level"), StreamingLevel))
		{
			return;
		}

		TestWorld->AddStreamingLevel(StreamingLevel);
		ON_SCOPE_EXIT
		{
			if (TestWorld != nullptr && StreamingLevel != nullptr)
			{
				TestWorld->RemoveStreamingLevel(StreamingLevel);
			}
		};

#if WITH_EDITOR
		StreamingLevel->SetShouldBeVisibleInEditor(true);
#endif

		const int32 NativeStreamingLevelCount = TestWorld->GetStreamingLevels().Num();
		const bool bNativeEditorVisibility = StreamingLevel->GetShouldBeVisibleInEditor();

		int32 ScriptStreamingLevelCount = INDEX_NONE;
		if (!WorldCollisionExecuteIntFunction(
			*TestRunner,
			Engine,
			*Module,
			TEXT("int GetStreamingLevelCount(UWorld World)"),
			[this, TestWorld](asIScriptContext& Context)
			{
				return WorldCollisionSetArgObjectChecked(*TestRunner, Context, 0, TestWorld, TEXT("GetStreamingLevelCount(valid)"));
			},
			TEXT("GetStreamingLevelCount(valid)"),
			ScriptStreamingLevelCount))
		{
			return;
		}

		TestRunner->TestEqual(
			TEXT("GetStreamingLevels should preserve the native streaming-level count for a valid world"),
			ScriptStreamingLevelCount,
			NativeStreamingLevelCount);

		bool bScriptEditorVisibility = false;
		if (!WorldCollisionExecuteBoolFunction(
			*TestRunner,
			Engine,
			*Module,
			TEXT("bool GetLevelVisibleInEditor(ULevelStreaming Level)"),
			[this, StreamingLevel](asIScriptContext& Context)
			{
				return WorldCollisionSetArgObjectChecked(*TestRunner, Context, 0, StreamingLevel, TEXT("GetLevelVisibleInEditor"));
			},
			TEXT("GetLevelVisibleInEditor"),
			bScriptEditorVisibility))
		{
			return;
		}

		TestRunner->TestEqual(
			TEXT("GetShouldBeVisibleInEditor should match the native editor-visibility baseline for a valid level"),
			bScriptEditorVisibility,
			bNativeEditorVisibility);

		FString NullWorldException;
		if (!WorldCollisionExecuteFunctionExpectingException(
			*TestRunner,
			Engine,
			*Module,
			TEXT("int GetStreamingLevelCount(UWorld World)"),
			[this](asIScriptContext& Context)
			{
				return WorldCollisionSetArgObjectChecked(*TestRunner, Context, 0, nullptr, TEXT("GetStreamingLevelCount(null)"));
			},
			TEXT("GetStreamingLevelCount(null)"),
			TEXT("Null pointer access"),
			&NullWorldException))
		{
			return;
		}

		TestRunner->TestEqual(
			TEXT("GetStreamingLevels should report a stable null-pointer diagnostic for a null world receiver"),
			NullWorldException,
			FString(TEXT("Null pointer access")));

		FString NullLevelException;
		if (!WorldCollisionExecuteFunctionExpectingException(
			*TestRunner,
			Engine,
			*Module,
			TEXT("bool GetLevelVisibleInEditor(ULevelStreaming Level)"),
			[this](asIScriptContext& Context)
			{
				return WorldCollisionSetArgObjectChecked(*TestRunner, Context, 0, nullptr, TEXT("GetLevelVisibleInEditor(null)"));
			},
			TEXT("GetLevelVisibleInEditor(null)"),
			TEXT("Null pointer access"),
			&NullLevelException))
		{
			return;
		}

		TestRunner->TestEqual(
			TEXT("GetShouldBeVisibleInEditor should report a stable null-pointer diagnostic for a null level receiver"),
			NullLevelException,
			FString(TEXT("Null pointer access")));

		}
	}

	TEST_METHOD(WorldStreamingAccess)
	{
		using namespace AngelscriptTest_Bindings_AngelscriptWorldFunctionLibraryTests_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();

		AActor& ContextActor = Spawner.SpawnActor<AActor>();
		UWorld* TestWorld = ContextActor.GetWorld();
		if (!TestRunner->TestNotNull(TEXT("World streaming access test should access the spawned world"), TestWorld))
		{
			return;
		}

		if (!TestRunner->TestEqual(
			TEXT("World streaming access test should start from a world without pre-existing streaming levels"),
			TestWorld->GetStreamingLevels().Num(),
			0))
		{
			return;
		}

		ULevelStreamingDynamic* FirstStreamingLevel = NewObject<ULevelStreamingDynamic>(TestWorld, TEXT("WorldStreamingAccess_First"));
		ULevelStreamingDynamic* SecondStreamingLevel = NewObject<ULevelStreamingDynamic>(TestWorld, TEXT("WorldStreamingAccess_Second"));
		if (!TestRunner->TestNotNull(TEXT("World streaming access test should create the first streaming level"), FirstStreamingLevel)
			|| !TestRunner->TestNotNull(TEXT("World streaming access test should create the second streaming level"), SecondStreamingLevel))
		{
			return;
		}

		TestWorld->AddStreamingLevel(FirstStreamingLevel);
		TestWorld->AddStreamingLevel(SecondStreamingLevel);
		ON_SCOPE_EXIT
		{
			if (TestWorld != nullptr)
			{
				if (SecondStreamingLevel != nullptr)
				{
					TestWorld->RemoveStreamingLevel(SecondStreamingLevel);
				}
				if (FirstStreamingLevel != nullptr)
				{
					TestWorld->RemoveStreamingLevel(FirstStreamingLevel);
				}
			}
		};

#if WITH_EDITOR
		FirstStreamingLevel->SetShouldBeVisibleInEditor(true);
		SecondStreamingLevel->SetShouldBeVisibleInEditor(false);
#endif

		const TArray<ULevelStreaming*>& NativeStreamingLevels = TestWorld->GetStreamingLevels();
		if (!TestRunner->TestEqual(
			TEXT("World streaming access test should expose exactly the two streaming levels inserted by the fixture"),
			NativeStreamingLevels.Num(),
			2))
		{
			return;
		}

		const bool bNativeFirstVisibility = FirstStreamingLevel->GetShouldBeVisibleInEditor();
		const bool bNativeSecondVisibility = SecondStreamingLevel->GetShouldBeVisibleInEditor();
		const bool bExpectedSecondVisibility = bNativeSecondVisibility;

		FString Script = TEXT(R"(
int VerifyWorldStreamingAccess(UWorld World, ULevelStreaming ExpectedFirst, ULevelStreaming ExpectedSecond)
{
	int MismatchMask = 0;

	if (World.GetStreamingLevels().Num() != $EXPECTED_COUNT$)
		MismatchMask |= 1;
	if (World.GetStreamingLevels().Num() <= 0 || World.GetStreamingLevels()[0] != ExpectedFirst)
		MismatchMask |= 2;
	if (World.GetStreamingLevels().Num() <= 1 || World.GetStreamingLevels()[1] != ExpectedSecond)
		MismatchMask |= 4;
	if (ExpectedFirst.GetShouldBeVisibleInEditor() != $EXPECTED_FIRST_VISIBLE$)
		MismatchMask |= 8;
	if (ExpectedSecond.GetShouldBeVisibleInEditor() != $EXPECTED_SECOND_VISIBLE$)
		MismatchMask |= 16;

	return MismatchMask;
}
)");
		Script.ReplaceInline(TEXT("$EXPECTED_COUNT$"), *LexToString(NativeStreamingLevels.Num()));
		Script.ReplaceInline(TEXT("$EXPECTED_FIRST_VISIBLE$"), bNativeFirstVisibility ? TEXT("true") : TEXT("false"));
		Script.ReplaceInline(TEXT("$EXPECTED_SECOND_VISIBLE$"), bExpectedSecondVisibility ? TEXT("true") : TEXT("false"));

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, WorldStreamingAccessModuleName, Script);
		if (Module == nullptr)
		{
			return;
		}

		FScopedTestWorldContextScope WorldContextScope(&ContextActor);

		int32 ResultMask = INDEX_NONE;
		if (!WorldCollisionExecuteIntFunction(
			*TestRunner,
			Engine,
			*Module,
			TEXT("int VerifyWorldStreamingAccess(UWorld, ULevelStreaming, ULevelStreaming)"),
			[this, TestWorld, FirstStreamingLevel, SecondStreamingLevel](asIScriptContext& Context)
			{
				return WorldCollisionSetArgObjectChecked(*TestRunner, Context, 0, TestWorld, TEXT("VerifyWorldStreamingAccess"))
					&& WorldCollisionSetArgObjectChecked(*TestRunner, Context, 1, FirstStreamingLevel, TEXT("VerifyWorldStreamingAccess"))
					&& WorldCollisionSetArgObjectChecked(*TestRunner, Context, 2, SecondStreamingLevel, TEXT("VerifyWorldStreamingAccess"));
			},
			TEXT("VerifyWorldStreamingAccess"),
			ResultMask))
		{
			return;
		}

		TestRunner->TestEqual(
			TEXT("World streaming function libraries should preserve streaming-level count, order and editor visibility"),
			ResultMask,
			0);
		TestRunner->TestTrue(
			TEXT("World streaming access test should keep the first streaming level editor-visible"),
			bNativeFirstVisibility);
		TestRunner->TestFalse(
			TEXT("World streaming access test should keep the second streaming level editor-hidden"),
			bNativeSecondVisibility);

		}
	}
};

#endif
