#include "CQTest.h"
#include "HotReload/ClassReloadHelper.h"

#include "AngelscriptEngine.h"

#include "Components/ActorComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/DataTable.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptEditor_Private_Tests_AngelscriptClassReloadHelperClassReloadTests_Private
{
	struct FClassReloadImmediateCallLog
	{
		TArray<UClass*> RefreshedClasses;
		TArray<UClass*> InvalidatedClasses;

		void Reset()
		{
			RefreshedClasses.Reset();
			InvalidatedClasses.Reset();
		}
	};

	TUniquePtr<FAngelscriptEngine> MakeClassReloadHelperImmediateEffectTestEngine()
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

	int32 CountClassHits(const TArray<UClass*>& Classes, UClass* ExpectedClass)
	{
		int32 Hits = 0;
		for (UClass* Class : Classes)
		{
			if (Class == ExpectedClass)
			{
				++Hits;
			}
		}

		return Hits;
	}
}

#define TestTrue(...) Test.TestTrue(__VA_ARGS__)
#define TestFalse(...) Test.TestFalse(__VA_ARGS__)
#define TestEqual(...) Test.TestEqual(__VA_ARGS__)
#define TestNotNull(...) Test.TestNotNull(__VA_ARGS__)

static bool RunOnClassReloadRefreshesClassActionsAndInvalidatesComponentRegistryForNonInterfaceComponents(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptClassReloadHelperClassReloadTests_Private;
	const FClassReloadHelper::FReloadState SavedState = FClassReloadHelper::ReloadState();
	TArray<FAngelscriptEngine*> SavedStack = FAngelscriptEngineContextStack::SnapshotAndClear();
	TUniquePtr<FAngelscriptEngine> Engine = MakeClassReloadHelperImmediateEffectTestEngine();
	TUniquePtr<FAngelscriptEngineScope> EngineScope;

	ON_SCOPE_EXIT
	{
		FClassReloadHelperTestAccess::ResetClassReloadTestHooks();
		EngineScope.Reset();
		FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(SavedStack));
		FClassReloadHelper::ReloadState() = SavedState;
	};

	if (!TestNotNull(TEXT("ClassReloadHelper.OnClassReload immediate-effects test should create a testing engine"), Engine.Get()))
	{
		return false;
	}
	if (!TestNotNull(TEXT("ClassReloadHelper.OnClassReload immediate-effects test should expose GEngine"), GEngine))
	{
		return false;
	}

	EngineScope = MakeUnique<FAngelscriptEngineScope>(*Engine);
	Engine->bIsInitialCompileFinished = true;
	EnsureClassReloadHelperInitialized(*Engine);

	UClass* OldComponentClass = USceneComponent::StaticClass();
	UClass* NewComponentClass = UBillboardComponent::StaticClass();
	UClass* OldRegularClass = UObject::StaticClass();
	UClass* NewRegularClass = UDataTable::StaticClass();

	FClassReloadImmediateCallLog CallLog;
	FClassReloadHelperClassReloadTestHooks Hooks;
	Hooks.RefreshClassActions = [&CallLog](UClass* Class)
	{
		CallLog.RefreshedClasses.Add(Class);
	};
	Hooks.InvalidateComponentClass = [&CallLog](UClass* Class)
	{
		CallLog.InvalidatedClasses.Add(Class);
	};
	FClassReloadHelperTestAccess::SetClassReloadTestHooks(MoveTemp(Hooks));

	FClassReloadHelper::FReloadState& ReloadState = FClassReloadHelper::ReloadState();

	ReloadState = FClassReloadHelper::FReloadState();
	CallLog.Reset();
	Engine->GetOnClassReload().Broadcast(OldComponentClass, NewComponentClass);

	if (!TestTrue(TEXT("ClassReloadHelper.OnClassReload immediate-effects test should track the reloaded component class pair"), ReloadState.ReloadClasses.FindRef(OldComponentClass) == NewComponentClass))
	{
		return false;
	}
	if (!TestFalse(TEXT("ClassReloadHelper.OnClassReload immediate-effects test should keep bRefreshAllActions false for non-interface component reloads"), ReloadState.bRefreshAllActions))
	{
		return false;
	}
	if (!TestFalse(TEXT("ClassReloadHelper.OnClassReload immediate-effects test should keep bReloadedVolume false for non-volume component reloads"), ReloadState.bReloadedVolume))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.OnClassReload immediate-effects test should refresh old component class actions once"), CountClassHits(CallLog.RefreshedClasses, OldComponentClass), 1))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.OnClassReload immediate-effects test should refresh new component class actions once"), CountClassHits(CallLog.RefreshedClasses, NewComponentClass), 1))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.OnClassReload immediate-effects test should only refresh the old/new component classes"), CallLog.RefreshedClasses.Num(), 2))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.OnClassReload immediate-effects test should invalidate the replacement component class once"), CountClassHits(CallLog.InvalidatedClasses, NewComponentClass), 1))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.OnClassReload immediate-effects test should only invalidate the replacement component class"), CallLog.InvalidatedClasses.Num(), 1))
	{
		return false;
	}

	ReloadState = FClassReloadHelper::FReloadState();
	CallLog.Reset();
	Engine->GetOnClassReload().Broadcast(OldRegularClass, NewRegularClass);

	if (!TestTrue(TEXT("ClassReloadHelper.OnClassReload immediate-effects test should track the reloaded regular class pair"), ReloadState.ReloadClasses.FindRef(OldRegularClass) == NewRegularClass))
	{
		return false;
	}
	if (!TestFalse(TEXT("ClassReloadHelper.OnClassReload immediate-effects test should keep bRefreshAllActions false for non-interface regular reloads"), ReloadState.bRefreshAllActions))
	{
		return false;
	}
	if (!TestFalse(TEXT("ClassReloadHelper.OnClassReload immediate-effects test should keep bReloadedVolume false for non-volume regular reloads"), ReloadState.bReloadedVolume))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.OnClassReload immediate-effects test should refresh old regular class actions once"), CountClassHits(CallLog.RefreshedClasses, OldRegularClass), 1))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.OnClassReload immediate-effects test should refresh new regular class actions once"), CountClassHits(CallLog.RefreshedClasses, NewRegularClass), 1))
	{
		return false;
	}
	return TestEqual(TEXT("ClassReloadHelper.OnClassReload immediate-effects test should not invalidate non-component replacement classes"), CallLog.InvalidatedClasses.Num(), 0);
}

#undef TestTrue
#undef TestFalse
#undef TestEqual
#undef TestNotNull

TEST_CLASS_WITH_FLAGS(FAngelscriptClassReloadHelperClassReloadTests,
	"Angelscript.Editor.ClassReloadHelper",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(OnClassReloadRefreshesClassActionsAndInvalidatesComponentRegistryForNonInterfaceComponents)
	{
		ASSERT_THAT(IsTrue(RunOnClassReloadRefreshesClassActionsAndInvalidatesComponentRegistryForNonInterfaceComponents(*TestRunner)));
	}
};

#endif
