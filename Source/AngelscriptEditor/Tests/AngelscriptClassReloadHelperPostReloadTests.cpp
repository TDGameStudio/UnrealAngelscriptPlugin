#include "CQTest.h"
#include "HotReload/ClassReloadHelper.h"

#include "AngelscriptEngine.h"

#include "Engine/Level.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptEditor_Private_Tests_AngelscriptClassReloadHelperPostReloadTests_Private
{
	struct FPostReloadCallLog
	{
		int32 RefreshAllCalls = 0;
		int32 BlueprintCompiledCalls = 0;
		int32 InvalidateComponentRegistryCalls = 0;
		int32 ExecCalls = 0;
		TArray<FString> ExecCommands;
		TArray<TWeakObjectPtr<UWorld>> ExecWorlds;
	};

	TUniquePtr<FAngelscriptEngine> MakeClassReloadHelperPostReloadTestEngine()
	{
		FAngelscriptEngineConfig Config;
		Config.bSkipInitialCompile = true;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		return FAngelscriptEngine::Create(Config, Dependencies);
	}

	void EnsureClassReloadHelperInitialized(FAngelscriptEngine& Engine)
	{
		if (!Engine.GetOnClassReload().IsBound())
		{
			FClassReloadHelper::Init();
		}
	}

	void RootObject(TArray<UObject*>& RootedObjects, UObject* Object)
	{
		if (Object == nullptr || RootedObjects.Contains(Object))
		{
			return;
		}

		Object->AddToRoot();
		RootedObjects.Add(Object);
	}

	ULevel* AddTransientLevel(FAutomationTestBase& Test, UWorld& World, TArray<UObject*>& RootedObjects)
	{
		ULevel* Level = NewObject<ULevel>(
			&World,
			MakeUniqueObjectName(&World, ULevel::StaticClass(), TEXT("AngelscriptClassReloadHelperPostReloadLevel")),
			RF_Transient);
		if (!Test.TestNotNull(TEXT("ClassReloadHelper.OnPostReload test should create a transient non-default level"), Level))
		{
			return nullptr;
		}

		RootObject(RootedObjects, Level);
		if (!Test.TestTrue(TEXT("ClassReloadHelper.OnPostReload test should register the transient level with the editor world"), World.AddLevel(Level)))
		{
			return nullptr;
		}

		return Level;
	}

	void SeedPostReloadResetSentinels(FClassReloadHelper::FReloadState& ReloadState)
	{
		ReloadState = FClassReloadHelper::FReloadState();
		ReloadState.ReloadClasses.Add(UObject::StaticClass(), UObject::StaticClass());
		ReloadState.ReloadAssets.Add(GetTransientPackage(), GetTransientPackage());
	}
}

#define TestTrue(...) Test.TestTrue(__VA_ARGS__)
#define TestFalse(...) Test.TestFalse(__VA_ARGS__)
#define TestEqual(...) Test.TestEqual(__VA_ARGS__)
#define TestNotNull(...) Test.TestNotNull(__VA_ARGS__)

static bool RunOnPostReloadFullReloadEffects(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptClassReloadHelperPostReloadTests_Private;
	const FClassReloadHelper::FReloadState SavedState = FClassReloadHelper::ReloadState();
	TArray<FAngelscriptEngine*> SavedStack = FAngelscriptEngineContextStack::SnapshotAndClear();
	TUniquePtr<FAngelscriptEngine> Engine = MakeClassReloadHelperPostReloadTestEngine();
	TUniquePtr<FAngelscriptEngineScope> EngineScope;
	TArray<UObject*> RootedObjects;
	UWorld* EditorWorld = nullptr;
	ULevel* AddedLevel = nullptr;
	ULevel* SavedCurrentLevel = nullptr;
	FDelegateHandle BlueprintCompiledHandle;

	ON_SCOPE_EXIT
	{
		if (BlueprintCompiledHandle.IsValid() && GEditor != nullptr)
		{
			GEditor->OnBlueprintCompiled().Remove(BlueprintCompiledHandle);
		}

		FClassReloadHelperTestAccess::ResetPostReloadTestHooks();
		EngineScope.Reset();

		if (EditorWorld != nullptr)
		{
			if (SavedCurrentLevel != nullptr && EditorWorld->GetLevels().Contains(SavedCurrentLevel))
			{
				EditorWorld->SetCurrentLevel(SavedCurrentLevel);
			}

			if (AddedLevel != nullptr)
			{
				EditorWorld->RemoveLevel(AddedLevel);
			}
		}

		for (UObject* Object : RootedObjects)
		{
			if (Object != nullptr)
			{
				Object->RemoveFromRoot();
				Object->MarkAsGarbage();
			}
		}

		CollectGarbage(RF_NoFlags, true);
		FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(SavedStack));
		FClassReloadHelper::ReloadState() = SavedState;
	};

	if (!TestNotNull(TEXT("ClassReloadHelper.OnPostReload test should create a testing engine"), Engine.Get()))
	{
		return false;
	}
	if (!TestNotNull(TEXT("ClassReloadHelper.OnPostReload test should expose GEditor"), GEditor))
	{
		return false;
	}
	if (!TestNotNull(TEXT("ClassReloadHelper.OnPostReload test should expose GEngine"), GEngine))
	{
		return false;
	}

	EngineScope = MakeUnique<FAngelscriptEngineScope>(*Engine);
	Engine->bIsInitialCompileFinished = true;
	EnsureClassReloadHelperInitialized(*Engine);

	EditorWorld = GEditor->GetEditorWorldContext().World();
	if (!TestNotNull(TEXT("ClassReloadHelper.OnPostReload test should expose the editor world"), EditorWorld))
	{
		return false;
	}

	SavedCurrentLevel = EditorWorld->GetCurrentLevel();
	if (!TestNotNull(TEXT("ClassReloadHelper.OnPostReload test should expose the editor world's current level"), SavedCurrentLevel))
	{
		return false;
	}

	AddedLevel = AddTransientLevel(Test, *EditorWorld, RootedObjects);
	if (AddedLevel == nullptr)
	{
		return false;
	}
	if (!TestTrue(TEXT("ClassReloadHelper.OnPostReload test should keep the transient level in the editor world level list"), EditorWorld->GetLevels().Contains(AddedLevel)))
	{
		return false;
	}
	if (!TestTrue(TEXT("ClassReloadHelper.OnPostReload test should switch the editor world to the non-default transient level"), EditorWorld->SetCurrentLevel(AddedLevel)))
	{
		return false;
	}
	if (!TestTrue(TEXT("ClassReloadHelper.OnPostReload test should start on the non-default current level"), EditorWorld->GetCurrentLevel() == AddedLevel))
	{
		return false;
	}

	FPostReloadCallLog CallLog;
	BlueprintCompiledHandle = GEditor->OnBlueprintCompiled().AddLambda([&CallLog]()
	{
		++CallLog.BlueprintCompiledCalls;
	});

	FClassReloadHelperPostReloadTestHooks Hooks;
	Hooks.RefreshAllActions = [&CallLog]()
	{
		++CallLog.RefreshAllCalls;
	};
	Hooks.ExecCommand = [&CallLog, SavedCurrentLevel](UWorld* World, const TCHAR* Command)
	{
		++CallLog.ExecCalls;
		CallLog.ExecCommands.Add(Command != nullptr ? FString(Command) : FString());
		CallLog.ExecWorlds.Add(World);

		if (World != nullptr && SavedCurrentLevel != nullptr)
		{
			World->SetCurrentLevel(SavedCurrentLevel);
		}
	};
	FClassReloadHelperTestAccess::SetPostReloadTestHooks(MoveTemp(Hooks));

	FClassReloadHelper::FReloadState& ReloadState = FClassReloadHelper::ReloadState();
	ReloadState = FClassReloadHelper::FReloadState();
	ReloadState.bRefreshAllActions = true;
	ReloadState.bReloadedVolume = true;

	Engine->GetOnPostReload().Broadcast(true);


	if (!TestEqual(TEXT("ClassReloadHelper.OnPostReload full-reload test should refresh the blueprint action database once"), CallLog.RefreshAllCalls, 1))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.OnPostReload full-reload test should broadcast blueprint compiled once"), CallLog.BlueprintCompiledCalls, 1))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.OnPostReload full-reload test should execute one geometry rebuild command"), CallLog.ExecCalls, 1))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.OnPostReload full-reload test should capture one geometry rebuild command string"), CallLog.ExecCommands.Num(), 1))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.OnPostReload full-reload test should issue MAP REBUILD ALLVISIBLE"), CallLog.ExecCommands[0], FString(TEXT("MAP REBUILD ALLVISIBLE"))))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.OnPostReload full-reload test should pass the editor world into Exec"), CallLog.ExecWorlds.Num(), 1))
	{
		return false;
	}
	if (!TestTrue(TEXT("ClassReloadHelper.OnPostReload full-reload test should invoke Exec with the active editor world"), CallLog.ExecWorlds[0].Get() == EditorWorld))
	{
		return false;
	}
	if (!TestTrue(TEXT("ClassReloadHelper.OnPostReload full-reload test should restore the original non-default current level after exec mutates it"), EditorWorld->GetCurrentLevel() == AddedLevel))
	{
		return false;
	}
	if (!TestFalse(TEXT("ClassReloadHelper.OnPostReload full-reload test should clear bRefreshAllActions after the callback"), ReloadState.bRefreshAllActions))
	{
		return false;
	}
	if (!TestFalse(TEXT("ClassReloadHelper.OnPostReload full-reload test should clear bReloadedVolume after the callback"), ReloadState.bReloadedVolume))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.OnPostReload full-reload test should clear class reload mappings after the callback"), ReloadState.ReloadClasses.Num(), 0))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.OnPostReload full-reload test should clear struct reload mappings after the callback"), ReloadState.ReloadStructs.Num(), 0))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.OnPostReload full-reload test should clear delegate reload mappings after the callback"), ReloadState.ReloadDelegates.Num(), 0))
	{
		return false;
	}

	return TestEqual(TEXT("ClassReloadHelper.OnPostReload full-reload test should clear literal asset reload mappings after the callback"), ReloadState.ReloadAssets.Num(), 0);
}

static bool RunOnPostReloadSoftReloadInvalidation(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptClassReloadHelperPostReloadTests_Private;
	const FClassReloadHelper::FReloadState SavedState = FClassReloadHelper::ReloadState();
	TArray<FAngelscriptEngine*> SavedStack = FAngelscriptEngineContextStack::SnapshotAndClear();
	TUniquePtr<FAngelscriptEngine> Engine = MakeClassReloadHelperPostReloadTestEngine();
	TUniquePtr<FAngelscriptEngineScope> EngineScope;
	FDelegateHandle BlueprintCompiledHandle;

	ON_SCOPE_EXIT
	{
		if (BlueprintCompiledHandle.IsValid() && GEditor != nullptr)
		{
			GEditor->OnBlueprintCompiled().Remove(BlueprintCompiledHandle);
		}

		FClassReloadHelperTestAccess::ResetPostReloadTestHooks();
		EngineScope.Reset();
		FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(SavedStack));
		FClassReloadHelper::ReloadState() = SavedState;
	};

	if (!TestNotNull(TEXT("ClassReloadHelper.OnPostReload soft-reload test should create a testing engine"), Engine.Get()))
	{
		return false;
	}
	if (!TestNotNull(TEXT("ClassReloadHelper.OnPostReload soft-reload test should expose GEditor"), GEditor))
	{
		return false;
	}
	if (!TestNotNull(TEXT("ClassReloadHelper.OnPostReload soft-reload test should expose GEngine"), GEngine))
	{
		return false;
	}

	EngineScope = MakeUnique<FAngelscriptEngineScope>(*Engine);
	EnsureClassReloadHelperInitialized(*Engine);

	if (!TestTrue(TEXT("ClassReloadHelper.OnPostReload soft-reload test should run with an initialized Angelscript engine"), FAngelscriptEngine::IsInitialized()))
	{
		return false;
	}

	FPostReloadCallLog CallLog;
	BlueprintCompiledHandle = GEditor->OnBlueprintCompiled().AddLambda([&CallLog]()
	{
		++CallLog.BlueprintCompiledCalls;
	});

	FClassReloadHelperPostReloadTestHooks Hooks;
	Hooks.RefreshAllActions = [&CallLog]()
	{
		++CallLog.RefreshAllCalls;
	};
	Hooks.InvalidateComponentRegistry = [&CallLog]()
	{
		++CallLog.InvalidateComponentRegistryCalls;
	};
	Hooks.ExecCommand = [&CallLog](UWorld* World, const TCHAR* Command)
	{
		++CallLog.ExecCalls;
		CallLog.ExecCommands.Add(Command != nullptr ? FString(Command) : FString());
		CallLog.ExecWorlds.Add(World);
	};
	FClassReloadHelperTestAccess::SetPostReloadTestHooks(MoveTemp(Hooks));

	FClassReloadHelper::FReloadState& ReloadState = FClassReloadHelper::ReloadState();

	Engine->bIsInitialCompileFinished = false;
	SeedPostReloadResetSentinels(ReloadState);
	Engine->GetOnPostReload().Broadcast(false);

	if (!TestEqual(TEXT("ClassReloadHelper.OnPostReload soft-reload test should invalidate the component registry once when initial compile is unfinished"), CallLog.InvalidateComponentRegistryCalls, 1))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.OnPostReload soft-reload test should not broadcast blueprint compiled for soft reload"), CallLog.BlueprintCompiledCalls, 0))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.OnPostReload soft-reload test should not execute a geometry rebuild command"), CallLog.ExecCalls, 0))
	{
		return false;
	}
	if (!TestFalse(TEXT("ClassReloadHelper.OnPostReload soft-reload test should clear bRefreshAllActions after the unfinished-compile callback"), ReloadState.bRefreshAllActions))
	{
		return false;
	}
	if (!TestFalse(TEXT("ClassReloadHelper.OnPostReload soft-reload test should clear bReloadedVolume after the unfinished-compile callback"), ReloadState.bReloadedVolume))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.OnPostReload soft-reload test should clear class reload mappings after the unfinished-compile callback"), ReloadState.ReloadClasses.Num(), 0))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.OnPostReload soft-reload test should clear literal asset reload mappings after the unfinished-compile callback"), ReloadState.ReloadAssets.Num(), 0))
	{
		return false;
	}

	Engine->bIsInitialCompileFinished = true;
	SeedPostReloadResetSentinels(ReloadState);
	Engine->GetOnPostReload().Broadcast(false);

	if (!TestEqual(TEXT("ClassReloadHelper.OnPostReload soft-reload test should not invalidate the component registry again once initial compile is finished"), CallLog.InvalidateComponentRegistryCalls, 1))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.OnPostReload soft-reload test should keep blueprint compiled broadcasts at zero across both soft reloads"), CallLog.BlueprintCompiledCalls, 0))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.OnPostReload soft-reload test should keep geometry rebuild commands at zero across both soft reloads"), CallLog.ExecCalls, 0))
	{
		return false;
	}
	if (!TestFalse(TEXT("ClassReloadHelper.OnPostReload soft-reload test should clear bRefreshAllActions after the finished-compile callback"), ReloadState.bRefreshAllActions))
	{
		return false;
	}
	if (!TestFalse(TEXT("ClassReloadHelper.OnPostReload soft-reload test should clear bReloadedVolume after the finished-compile callback"), ReloadState.bReloadedVolume))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.OnPostReload soft-reload test should clear class reload mappings after the finished-compile callback"), ReloadState.ReloadClasses.Num(), 0))
	{
		return false;
	}

	return TestEqual(TEXT("ClassReloadHelper.OnPostReload soft-reload test should clear literal asset reload mappings after the finished-compile callback"), ReloadState.ReloadAssets.Num(), 0);
}

#undef TestTrue
#undef TestFalse
#undef TestEqual
#undef TestNotNull

TEST_CLASS_WITH_FLAGS(FAngelscriptClassReloadHelperPostReloadTests,
	"Angelscript.Editor.ClassReloadHelper",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(OnPostReloadFullReloadRefreshesActionsBroadcastsBlueprintCompiledAndRestoresCurrentLevel)
	{
		ASSERT_THAT(IsTrue(RunOnPostReloadFullReloadEffects(*TestRunner)));
	}

	TEST_METHOD(OnPostReloadSoftReloadInvalidatesComponentRegistryWithoutFullReloadSideEffects)
	{
		ASSERT_THAT(IsTrue(RunOnPostReloadSoftReloadInvalidation(*TestRunner)));
	}
};

#endif
