#include "HotReload/ClassReloadHelper.h"

#include "AngelscriptEngine.h"

#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/Interface.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptClassReloadHelperReloadStateTest,
	"Angelscript.Editor.ClassReloadHelper.ReloadStateTracksAndResetsOnPostReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptClassReloadHelperEnumAndAssetReloadStateTest,
	"Angelscript.TestModule.Editor.ClassReloadHelper.ReloadStateTracksLiteralAssetsAndEnumChanges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptClassReloadHelperPerformReinstanceInitialCompileGateTest,
	"Angelscript.TestModule.Editor.ClassReloadHelper.PerformReinstanceNoOpsBeforeInitialCompileFinishes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace AngelscriptEditor_Private_Tests_AngelscriptClassReloadHelperTests_Private
{
	struct FPerformReinstanceCallLog
	{
		int32 EnterPerformReinstanceBodyCalls = 0;
		int32 NotifyCustomizationModuleChangedCalls = 0;
		int32 RefreshAssetActionsCalls = 0;
		int32 AddActorFactoryCalls = 0;
		int32 BroadcastAllPlaceableAssetsChangedCalls = 0;
		int32 BroadcastPlaceableItemFilteringChangedCalls = 0;

		void Reset()
		{
			EnterPerformReinstanceBodyCalls = 0;
			NotifyCustomizationModuleChangedCalls = 0;
			RefreshAssetActionsCalls = 0;
			AddActorFactoryCalls = 0;
			BroadcastAllPlaceableAssetsChangedCalls = 0;
			BroadcastPlaceableItemFilteringChangedCalls = 0;
		}
	};

	TUniquePtr<FAngelscriptEngine> MakeClassReloadHelperTestEngine()
	{
		FAngelscriptEngineConfig Config;
		Config.bSkipInitialCompile = true;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		return FAngelscriptEngine::Create(Config, Dependencies);
	}

	void EnsureReloadHelperInitialized(FAngelscriptEngine& Engine)
	{
		if (!Engine.GetHooks().GetOnClassReload().IsBound())
		{
			FClassReloadHelper::Init();
		}
	}

	UDelegateFunction* FindActorDelegateSignature(FAutomationTestBase& Test, const FName PropertyName)
	{
		const FMulticastDelegateProperty* DelegateProperty =
			FindFProperty<FMulticastDelegateProperty>(AActor::StaticClass(), PropertyName);
		if (!Test.TestNotNull(*FString::Printf(TEXT("ClassReloadHelper.ReloadState should find delegate property %s"), *PropertyName.ToString()), DelegateProperty))
		{
			return nullptr;
		}

		UDelegateFunction* SignatureFunction = Cast<UDelegateFunction>(DelegateProperty->SignatureFunction.Get());
		if (!Test.TestNotNull(*FString::Printf(TEXT("ClassReloadHelper.ReloadState should expose signature function for %s"), *PropertyName.ToString()), SignatureFunction))
		{
			return nullptr;
		}

		return SignatureFunction;
	}
}


bool FAngelscriptClassReloadHelperPerformReinstanceInitialCompileGateTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptClassReloadHelperTests_Private;
	const FClassReloadHelper::FReloadState SavedState = FClassReloadHelper::ReloadState();
	TArray<FAngelscriptEngine*> SavedStack = FAngelscriptEngineContextStack::SnapshotAndClear();
	TUniquePtr<FAngelscriptEngine> Engine = MakeClassReloadHelperTestEngine();
	TUniquePtr<FAngelscriptEngineScope> EngineScope;

	ON_SCOPE_EXIT
	{
		FClassReloadHelperTestAccess::ResetPerformReinstanceTestHooks();
		EngineScope.Reset();
		FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(SavedStack));
		FClassReloadHelper::ReloadState() = SavedState;
	};

	if (!TestNotNull(TEXT("ClassReloadHelper.PerformReinstance gate test should create a testing engine"), Engine.Get()))
	{
		return false;
	}
	if (!TestNotNull(TEXT("ClassReloadHelper.PerformReinstance gate test should expose GEditor"), GEditor))
	{
		return false;
	}

	EngineScope = MakeUnique<FAngelscriptEngineScope>(*Engine);
	Engine->bIsInitialCompileFinished = true;
	EnsureReloadHelperInitialized(*Engine);

	UEnum* ReloadedEnum = StaticEnum<EAutoReceiveInput::Type>();
	if (!TestNotNull(TEXT("ClassReloadHelper.PerformReinstance gate test should expose a reload enum fixture"), ReloadedEnum))
	{
		return false;
	}

	FClassReloadHelper::FReloadState& ReloadState = FClassReloadHelper::ReloadState();
	ReloadState = FClassReloadHelper::FReloadState();
	ReloadState.ReloadClasses.Add(UInterface::StaticClass(), UInterface::StaticClass());
	ReloadState.ReloadEnums.Add(ReloadedEnum);
	ReloadState.NewClasses.Add(AVolume::StaticClass());

	const int32 InitialActorFactoryCount = GEditor->ActorFactories.Num();
	FPerformReinstanceCallLog CallLog;
	CallLog.Reset();

	FClassReloadHelperPerformReinstanceTestHooks Hooks;
	Hooks.EnterPerformReinstanceBody = [&CallLog]()
	{
		++CallLog.EnterPerformReinstanceBodyCalls;
		return true;
	};
	Hooks.NotifyCustomizationModuleChanged = [&CallLog]()
	{
		++CallLog.NotifyCustomizationModuleChangedCalls;
	};
	Hooks.RefreshAssetActions = [&CallLog](UEnum*)
	{
		++CallLog.RefreshAssetActionsCalls;
	};
	Hooks.AddActorFactory = [&CallLog]()
	{
		++CallLog.AddActorFactoryCalls;
	};
	Hooks.BroadcastAllPlaceableAssetsChanged = [&CallLog]()
	{
		++CallLog.BroadcastAllPlaceableAssetsChangedCalls;
	};
	Hooks.BroadcastPlaceableItemFilteringChanged = [&CallLog]()
	{
		++CallLog.BroadcastPlaceableItemFilteringChangedCalls;
	};
	FClassReloadHelperTestAccess::SetPerformReinstanceTestHooks(MoveTemp(Hooks));

	{
		TGuardValue<bool> InitialCompileFinishedGuard(Engine->bIsInitialCompileFinished, false);
		ReloadState.PerformReinstance();

		if (!TestFalse(TEXT("ClassReloadHelper.PerformReinstance gate test should execute under an initial-compile-incomplete engine state"), Engine->bIsInitialCompileFinished))
		{
			return false;
		}
	}

	if (!TestTrue(TEXT("ClassReloadHelper.PerformReinstance gate test should restore the initial compile flag after the no-op guard"), Engine->bIsInitialCompileFinished))
	{
		return false;
	}

	if (!TestEqual(TEXT("ClassReloadHelper.PerformReinstance should not enter the guarded body before initial compile finishes"), CallLog.EnterPerformReinstanceBodyCalls, 0))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.PerformReinstance should keep reloaded class count unchanged before initial compile finishes"), ReloadState.ReloadClasses.Num(), 1))
	{
		return false;
	}
	if (!TestTrue(TEXT("ClassReloadHelper.PerformReinstance should preserve the reloaded class mapping before initial compile finishes"), ReloadState.ReloadClasses.FindRef(UInterface::StaticClass()) == UInterface::StaticClass()))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.PerformReinstance should keep reloaded enum count unchanged before initial compile finishes"), ReloadState.ReloadEnums.Num(), 1))
	{
		return false;
	}
	if (!TestTrue(TEXT("ClassReloadHelper.PerformReinstance should preserve the reloaded enum entry before initial compile finishes"), ReloadState.ReloadEnums.Contains(ReloadedEnum)))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.PerformReinstance should keep new class count unchanged before initial compile finishes"), ReloadState.NewClasses.Num(), 1))
	{
		return false;
	}
	if (!TestTrue(TEXT("ClassReloadHelper.PerformReinstance should preserve the new class entry before initial compile finishes"), ReloadState.NewClasses.Contains(AVolume::StaticClass())))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.PerformReinstance should not refresh property editor customizations before initial compile finishes"), CallLog.NotifyCustomizationModuleChangedCalls, 0))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.PerformReinstance should not refresh enum asset actions before initial compile finishes"), CallLog.RefreshAssetActionsCalls, 0))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.PerformReinstance should not add actor factories before initial compile finishes"), CallLog.AddActorFactoryCalls, 0))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.PerformReinstance should not broadcast placeable asset changes before initial compile finishes"), CallLog.BroadcastAllPlaceableAssetsChangedCalls, 0))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.PerformReinstance should not broadcast placeable filtering changes before initial compile finishes"), CallLog.BroadcastPlaceableItemFilteringChangedCalls, 0))
	{
		return false;
	}
	return TestEqual(TEXT("ClassReloadHelper.PerformReinstance should not mutate GEditor actor factory count before initial compile finishes"), GEditor->ActorFactories.Num(), InitialActorFactoryCount);
}

bool FAngelscriptClassReloadHelperReloadStateTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptClassReloadHelperTests_Private;
	FAngelscriptEngine& Engine = FAngelscriptEngine::Get();
	EnsureReloadHelperInitialized(Engine);

	const FClassReloadHelper::FReloadState SavedState = FClassReloadHelper::ReloadState();
	ON_SCOPE_EXIT
	{
		FClassReloadHelper::ReloadState() = SavedState;
	};

	UDelegateFunction* OldDelegate = FindActorDelegateSignature(*this, GET_MEMBER_NAME_CHECKED(AActor, OnActorBeginOverlap));
	UDelegateFunction* NewDelegate = FindActorDelegateSignature(*this, GET_MEMBER_NAME_CHECKED(AActor, OnActorEndOverlap));
	if (OldDelegate == nullptr || NewDelegate == nullptr)
	{
		return false;
	}

	FClassReloadHelper::ReloadState() = FClassReloadHelper::FReloadState();

	FAngelscriptEngineHooks& Hooks = Engine.GetHooks();
	Hooks.GetOnClassReload().Broadcast(UInterface::StaticClass(), UInterface::StaticClass());
	Hooks.GetOnClassReload().Broadcast(AActor::StaticClass(), APawn::StaticClass());
	Hooks.GetOnClassReload().Broadcast(nullptr, AVolume::StaticClass());
	Hooks.GetOnStructReload().Broadcast(TBaseStructure<FVector>::Get(), TBaseStructure<FTransform>::Get());
	Hooks.GetOnDelegateReload().Broadcast(OldDelegate, NewDelegate);

	FClassReloadHelper::FReloadState& ReloadState = FClassReloadHelper::ReloadState();

	if (!TestTrue(TEXT("ClassReloadHelper.ReloadState should request full blueprint action refresh after interface or struct reload"), ReloadState.bRefreshAllActions))
	{
		return false;
	}
	if (!TestTrue(TEXT("ClassReloadHelper.ReloadState should flag volume reloads when a new volume class appears"), ReloadState.bReloadedVolume))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.ReloadState should track two reloaded classes"), ReloadState.ReloadClasses.Num(), 2))
	{
		return false;
	}
	if (!TestTrue(TEXT("ClassReloadHelper.ReloadState should map the interface class reload"), ReloadState.ReloadClasses.FindRef(UInterface::StaticClass()) == UInterface::StaticClass()))
	{
		return false;
	}
	if (!TestTrue(TEXT("ClassReloadHelper.ReloadState should map the ordinary class reload"), ReloadState.ReloadClasses.FindRef(AActor::StaticClass()) == APawn::StaticClass()))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.ReloadState should track one new class"), ReloadState.NewClasses.Num(), 1))
	{
		return false;
	}
	if (!TestTrue(TEXT("ClassReloadHelper.ReloadState should collect the reloaded volume class in NewClasses"), ReloadState.NewClasses.Contains(AVolume::StaticClass())))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.ReloadState should track one reloaded struct"), ReloadState.ReloadStructs.Num(), 1))
	{
		return false;
	}
	if (!TestTrue(TEXT("ClassReloadHelper.ReloadState should map the struct reload"), ReloadState.ReloadStructs.FindRef(TBaseStructure<FVector>::Get()) == TBaseStructure<FTransform>::Get()))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.ReloadState should track one reloaded delegate"), ReloadState.ReloadDelegates.Num(), 1))
	{
		return false;
	}
	if (!TestTrue(TEXT("ClassReloadHelper.ReloadState should map the delegate reload"), ReloadState.ReloadDelegates.FindRef(OldDelegate) == NewDelegate))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.ReloadState should track one new delegate"), ReloadState.NewDelegates.Num(), 1))
	{
		return false;
	}
	if (!TestTrue(TEXT("ClassReloadHelper.ReloadState should collect the new delegate in NewDelegates"), ReloadState.NewDelegates.Contains(NewDelegate)))
	{
		return false;
	}

	Hooks.GetOnPostReload().Broadcast(true);

	if (!TestFalse(TEXT("ClassReloadHelper.ReloadState should clear bRefreshAllActions after post reload"), ReloadState.bRefreshAllActions))
	{
		return false;
	}
	if (!TestFalse(TEXT("ClassReloadHelper.ReloadState should clear bReloadedVolume after post reload"), ReloadState.bReloadedVolume))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.ReloadState should clear reloaded classes after post reload"), ReloadState.ReloadClasses.Num(), 0))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.ReloadState should clear new classes after post reload"), ReloadState.NewClasses.Num(), 0))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.ReloadState should clear reloaded structs after post reload"), ReloadState.ReloadStructs.Num(), 0))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.ReloadState should clear reloaded delegates after post reload"), ReloadState.ReloadDelegates.Num(), 0))
	{
		return false;
	}
	return TestEqual(TEXT("ClassReloadHelper.ReloadState should clear new delegates after post reload"), ReloadState.NewDelegates.Num(), 0);
}

bool FAngelscriptClassReloadHelperEnumAndAssetReloadStateTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptClassReloadHelperTests_Private;
	FAngelscriptEngine& Engine = FAngelscriptEngine::Get();
	EnsureReloadHelperInitialized(Engine);

	const FClassReloadHelper::FReloadState SavedState = FClassReloadHelper::ReloadState();
	ON_SCOPE_EXIT
	{
		FClassReloadHelper::ReloadState() = SavedState;
	};

	UObject* OldAsset = NewObject<UDataTable>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UDataTable::StaticClass(), TEXT("ClassReloadHelperOldAsset")));
	UObject* NewAsset = NewObject<UDataTable>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UDataTable::StaticClass(), TEXT("ClassReloadHelperNewAsset")));
	if (!TestNotNull(TEXT("ClassReloadHelper.ReloadState should create the old literal asset test object"), OldAsset) ||
		!TestNotNull(TEXT("ClassReloadHelper.ReloadState should create the new literal asset test object"), NewAsset))
	{
		return false;
	}

	UEnum* ChangedEnum = StaticEnum<EAutoReceiveInput::Type>();
	UEnum* NewEnum = StaticEnum<EAutoPossessAI>();
	if (!TestNotNull(TEXT("ClassReloadHelper.ReloadState should expose a changed enum fixture"), ChangedEnum) ||
		!TestNotNull(TEXT("ClassReloadHelper.ReloadState should expose a created enum fixture"), NewEnum))
	{
		return false;
	}

	const TArray<TPair<FName, int64>> OldNames =
	{
		TPair<FName, int64>(TEXT("Disabled"), 0),
		TPair<FName, int64>(TEXT("Player0"), 1),
	};

	FClassReloadHelper::ReloadState() = FClassReloadHelper::FReloadState();

	FAngelscriptEngineHooks& Hooks = Engine.GetHooks();
	Hooks.GetOnLiteralAssetReload().Broadcast(OldAsset, NewAsset);
	Hooks.GetOnEnumChanged().Broadcast(ChangedEnum, OldNames);
	Hooks.GetOnEnumCreated().Broadcast(NewEnum);

	FClassReloadHelper::FReloadState& ReloadState = FClassReloadHelper::ReloadState();

	if (!TestEqual(TEXT("ClassReloadHelper.ReloadState should track one literal asset reload"), ReloadState.ReloadAssets.Num(), 1))
	{
		return false;
	}
	if (!TestTrue(TEXT("ClassReloadHelper.ReloadState should map the old literal asset to the new one"), ReloadState.ReloadAssets.FindRef(OldAsset) == NewAsset))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.ReloadState should track one changed enum"), ReloadState.ReloadEnums.Num(), 1))
	{
		return false;
	}
	if (!TestTrue(TEXT("ClassReloadHelper.ReloadState should collect the changed enum"), ReloadState.ReloadEnums.Contains(ChangedEnum)))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.ReloadState should track one created enum"), ReloadState.NewEnums.Num(), 1))
	{
		return false;
	}
	if (!TestTrue(TEXT("ClassReloadHelper.ReloadState should collect the created enum"), ReloadState.NewEnums.Contains(NewEnum)))
	{
		return false;
	}

	Hooks.GetOnPostReload().Broadcast(false);

	if (!TestEqual(TEXT("ClassReloadHelper.ReloadState should clear literal asset reloads after post reload"), ReloadState.ReloadAssets.Num(), 0))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.ReloadState should clear changed enums after post reload"), ReloadState.ReloadEnums.Num(), 0))
	{
		return false;
	}
	return TestEqual(TEXT("ClassReloadHelper.ReloadState should clear created enums after post reload"), ReloadState.NewEnums.Num(), 0);
}

#endif
