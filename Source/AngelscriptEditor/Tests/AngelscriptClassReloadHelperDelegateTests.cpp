#include "CQTest.h"
#include "HotReload/ClassReloadHelper.h"
#include "BlueprintImpact/AngelscriptBlueprintImpactScanner.h"

#include "AngelscriptEngine.h"

#include "EdGraph/EdGraph.h"
#include "GameFramework/Actor.h"
#include "Engine/Blueprint.h"
#include "HAL/IConsoleManager.h"
#include "K2Node_Event.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "Preprocessor/AngelscriptPreprocessor.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptEditor_Private_Tests_AngelscriptClassReloadHelperDelegateTests_Private
{
	struct FPerformReinstanceDelegateCallLog
	{
		int32 FlushCompilationQueueAndReinstanceCalls = 0;
		TArray<TWeakObjectPtr<UBlueprint>> QueuedBlueprints;
	};

	TUniquePtr<FAngelscriptEngine> MakeClassReloadHelperDelegateTestEngine()
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

	UDelegateFunction* FindActorDelegateSignature(FAutomationTestBase& Test, const FName PropertyName)
	{
		const FMulticastDelegateProperty* DelegateProperty =
			FindFProperty<FMulticastDelegateProperty>(AActor::StaticClass(), PropertyName);
		if (!Test.TestNotNull(*FString::Printf(TEXT("ClassReloadHelper.DelegateReinstance test should find delegate property %s"), *PropertyName.ToString()), DelegateProperty))
		{
			return nullptr;
		}

		UDelegateFunction* SignatureFunction = Cast<UDelegateFunction>(DelegateProperty->SignatureFunction.Get());
		if (!Test.TestNotNull(*FString::Printf(TEXT("ClassReloadHelper.DelegateReinstance test should expose signature function for %s"), *PropertyName.ToString()), SignatureFunction))
		{
			return nullptr;
		}

		return SignatureFunction;
	}

	bool CompileMemoryModule(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const TCHAR* ModuleName,
		const TCHAR* ScriptSource,
		ECompileType CompileType = ECompileType::FullReload)
	{
		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddSource(FAngelscriptSource::FromMemorySource(
			FString::Printf(TEXT("/Angelscript/Memory/ClassReloadHelperDelegate/%s.as"), ModuleName),
			ScriptSource));
		if (!Test.TestTrue(TEXT("ClassReloadHelper.DelegateReinstance test should preprocess the AS delegate module"), Preprocessor.Preprocess()))
		{
			return false;
		}

		TArray<TSharedRef<FAngelscriptModuleDesc>> ModulesToCompile = Preprocessor.GetModulesToCompile();
		TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
		const ECompileResult CompileResult = Engine.CompileModules(CompileType, ModulesToCompile, CompiledModules);
		if (!Test.TestTrue(TEXT("ClassReloadHelper.DelegateReinstance test should compile the AS delegate module"), CompileResult != ECompileResult::Error && CompileResult != ECompileResult::ErrorNeedFullReload))
		{
			return false;
		}

		return Test.TestEqual(TEXT("ClassReloadHelper.DelegateReinstance test should compile one AS delegate module"), CompiledModules.Num(), 1);
	}

	UBlueprint* CreateTransientBlueprintChild(FAutomationTestBase& Test, UClass* ParentClass, FStringView Suffix, TArray<UObject*>& RootedObjects)
	{
		const FString PackagePath = FString::Printf(
			TEXT("/Temp/AngelscriptClassReloadHelperDelegate_%.*s_%s"),
			Suffix.Len(),
			Suffix.GetData(),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));

		UPackage* BlueprintPackage = CreatePackage(*PackagePath);
		if (!Test.TestNotNull(TEXT("ClassReloadHelper.DelegateReinstance test should create a transient blueprint package"), BlueprintPackage))
		{
			return nullptr;
		}

		BlueprintPackage->SetFlags(RF_Transient);
		RootObject(RootedObjects, BlueprintPackage);

		const FName BlueprintName(*FPackageName::GetLongPackageAssetName(PackagePath));
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			ParentClass,
			BlueprintPackage,
			BlueprintName,
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("AngelscriptClassReloadHelperDelegateTests"));
		if (!Test.TestNotNull(*FString::Printf(TEXT("ClassReloadHelper.DelegateReinstance test should create blueprint %.*s"), Suffix.Len(), Suffix.GetData()), Blueprint))
		{
			return nullptr;
		}

		RootObject(RootedObjects, Blueprint);
		if (UClass* GeneratedClass = Blueprint->GeneratedClass)
		{
			RootObject(RootedObjects, GeneratedClass);
		}

		return Blueprint;
	}

	UK2Node_Event* AddExternalDelegateEventNode(
		FAutomationTestBase& Test,
		UBlueprint& Blueprint,
		UDelegateFunction* DelegateSignature,
		FStringView NodeSuffix)
	{
		UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(&Blueprint);
		if (!Test.TestNotNull(TEXT("ClassReloadHelper.DelegateReinstance test should find the event graph"), EventGraph))
		{
			return nullptr;
		}

		UK2Node_Event* EventNode = NewObject<UK2Node_Event>(EventGraph);
		EventGraph->AddNode(EventNode, false, false);
		EventNode->EventReference.SetFromField<UFunction>(DelegateSignature, false);
		EventNode->bOverrideFunction = true;
		EventNode->CustomFunctionName = FName(*FString::Printf(TEXT("DelegateReload_%.*s"), NodeSuffix.Len(), NodeSuffix.GetData()));
		EventNode->CreateNewGuid();
		EventNode->PostPlacedNewNode();
		EventNode->AllocateDefaultPins();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(&Blueprint);

		UDelegateFunction* ResolvedSignature = Cast<UDelegateFunction>(EventNode->FindEventSignatureFunction());
		if (!Test.TestNotNull(TEXT("ClassReloadHelper.DelegateReinstance test should resolve an external delegate signature for the created event node"), ResolvedSignature))
		{
			return nullptr;
		}
		if (!Test.TestTrue(TEXT("ClassReloadHelper.DelegateReinstance test should bind the event node to the requested old delegate"), ResolvedSignature == DelegateSignature))
		{
			return nullptr;
		}

		return EventNode;
	}
}

#define TestTrue(...) Test.TestTrue(__VA_ARGS__)
#define TestFalse(...) Test.TestFalse(__VA_ARGS__)
#define TestEqual(...) Test.TestEqual(__VA_ARGS__)
#define TestNotNull(...) Test.TestNotNull(__VA_ARGS__)

static bool RunPerformReinstanceDelegateDependency(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptClassReloadHelperDelegateTests_Private;
	const FClassReloadHelper::FReloadState SavedState = FClassReloadHelper::ReloadState();
	TArray<FAngelscriptEngine*> SavedStack = FAngelscriptEngineContextStack::SnapshotAndClear();
	TUniquePtr<FAngelscriptEngine> Engine = MakeClassReloadHelperDelegateTestEngine();
	TUniquePtr<FAngelscriptEngineScope> EngineScope;
	TArray<UObject*> RootedObjects;
	IConsoleVariable* UseUnrealReloadCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("angelscript.UseUnrealReload"));
	const int32 SavedUseUnrealReload = UseUnrealReloadCVar != nullptr ? UseUnrealReloadCVar->GetInt() : 0;

	ON_SCOPE_EXIT
	{
		FClassReloadHelperTestAccess::ResetPerformReinstanceTestHooks();
		if (UseUnrealReloadCVar != nullptr)
		{
			UseUnrealReloadCVar->Set(SavedUseUnrealReload, ECVF_SetByCode);
		}

		EngineScope.Reset();
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

	if (!TestNotNull(TEXT("ClassReloadHelper.DelegateReinstance test should create a testing engine"), Engine.Get()))
	{
		return false;
	}
	if (!TestNotNull(TEXT("ClassReloadHelper.DelegateReinstance test should expose GEditor"), GEditor))
	{
		return false;
	}

	EngineScope = MakeUnique<FAngelscriptEngineScope>(*Engine);
	Engine->bIsInitialCompileFinished = true;
	EnsureClassReloadHelperInitialized(*Engine);

	if (UseUnrealReloadCVar != nullptr)
	{
		UseUnrealReloadCVar->Set(0, ECVF_SetByCode);
	}

	UDelegateFunction* OldDelegate = FindActorDelegateSignature(Test, GET_MEMBER_NAME_CHECKED(AActor, OnActorBeginOverlap));
	UDelegateFunction* NewDelegate = FindActorDelegateSignature(Test, GET_MEMBER_NAME_CHECKED(AActor, OnActorEndOverlap));
	if (OldDelegate == nullptr || NewDelegate == nullptr)
	{
		return false;
	}

	UBlueprint* DependentBlueprint = CreateTransientBlueprintChild(Test, UObject::StaticClass(), TEXT("Dependent"), RootedObjects);
	UBlueprint* ControlBlueprint = CreateTransientBlueprintChild(Test, UObject::StaticClass(), TEXT("Control"), RootedObjects);
	if (DependentBlueprint == nullptr || ControlBlueprint == nullptr)
	{
		return false;
	}

	if (AddExternalDelegateEventNode(Test, *DependentBlueprint, OldDelegate, TEXT("Dependent")) == nullptr)
	{
		return false;
	}

	AngelscriptEditor::BlueprintImpact::FBlueprintImpactSymbols Symbols;
	Symbols.Delegates.Add(OldDelegate);
	Symbols.Delegates.Add(NewDelegate);

	TArray<AngelscriptEditor::BlueprintImpact::EBlueprintImpactReason> DependentReasons;
	const bool bDependentImpacted = AngelscriptEditor::BlueprintImpact::AnalyzeLoadedBlueprint(*DependentBlueprint, Symbols, DependentReasons);
	if (!TestTrue(TEXT("ClassReloadHelper.DelegateReinstance test should confirm the dependent blueprint is impacted by the reloaded delegates"), bDependentImpacted))
	{
		return false;
	}
	if (!TestTrue(TEXT("ClassReloadHelper.DelegateReinstance test should classify the dependent blueprint as a delegate-signature impact"), DependentReasons.Contains(AngelscriptEditor::BlueprintImpact::EBlueprintImpactReason::DelegateSignature)))
	{
		return false;
	}

	TArray<AngelscriptEditor::BlueprintImpact::EBlueprintImpactReason> ControlReasons;
	if (!TestFalse(TEXT("ClassReloadHelper.DelegateReinstance test should keep the control blueprint out of delegate impact analysis"), AngelscriptEditor::BlueprintImpact::AnalyzeLoadedBlueprint(*ControlBlueprint, Symbols, ControlReasons)))
	{
		return false;
	}

	FPerformReinstanceDelegateCallLog CallLog;
	FClassReloadHelper::FReloadState& ReloadState = FClassReloadHelper::ReloadState();
	ReloadState = FClassReloadHelper::FReloadState();
	ReloadState.ReloadDelegates.Add(OldDelegate, NewDelegate);
	ReloadState.NewDelegates.Add(NewDelegate);

	FClassReloadHelperPerformReinstanceTestHooks Hooks;
	Hooks.QueueBlueprintForCompilation = [&CallLog](UBlueprint* Blueprint)
	{
		CallLog.QueuedBlueprints.Add(Blueprint);
	};
	Hooks.FlushCompilationQueueAndReinstance = [&CallLog]()
	{
		++CallLog.FlushCompilationQueueAndReinstanceCalls;
	};
	FClassReloadHelperTestAccess::SetPerformReinstanceTestHooks(MoveTemp(Hooks));

	ReloadState.PerformReinstance();

	int32 DependentQueueCount = 0;
	int32 ControlQueueCount = 0;
	for (const TWeakObjectPtr<UBlueprint>& QueuedBlueprint : CallLog.QueuedBlueprints)
	{
		if (QueuedBlueprint.Get() == DependentBlueprint)
		{
			++DependentQueueCount;
		}
		if (QueuedBlueprint.Get() == ControlBlueprint)
		{
			++ControlQueueCount;
		}
	}

	if (!TestEqual(TEXT("ClassReloadHelper.DelegateReinstance should queue the dependent blueprint exactly once"), DependentQueueCount, 1))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.DelegateReinstance should not queue the unrelated control blueprint"), ControlQueueCount, 0))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.DelegateReinstance should flush the compilation queue once after delegate-dependent blueprints are enqueued"), CallLog.FlushCompilationQueueAndReinstanceCalls, 1))
	{
		return false;
	}
	if (!TestTrue(TEXT("ClassReloadHelper.DelegateReinstance should keep the old-to-new delegate mapping alive until post reload"), ReloadState.ReloadDelegates.FindRef(OldDelegate) == NewDelegate))
	{
		return false;
	}
	return TestTrue(TEXT("ClassReloadHelper.DelegateReinstance should keep the created delegate visible until post reload resets the state"), ReloadState.NewDelegates.Contains(NewDelegate));
}

static bool RunPerformReinstanceAngelscriptDelegateGraphDependency(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptClassReloadHelperDelegateTests_Private;
	const FClassReloadHelper::FReloadState SavedState = FClassReloadHelper::ReloadState();
	TArray<FAngelscriptEngine*> SavedStack = FAngelscriptEngineContextStack::SnapshotAndClear();
	TUniquePtr<FAngelscriptEngine> Engine = MakeClassReloadHelperDelegateTestEngine();
	TUniquePtr<FAngelscriptEngineScope> EngineScope;
	TArray<UObject*> RootedObjects;
	IConsoleVariable* UseUnrealReloadCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("angelscript.UseUnrealReload"));
	const int32 SavedUseUnrealReload = UseUnrealReloadCVar != nullptr ? UseUnrealReloadCVar->GetInt() : 0;

	ON_SCOPE_EXIT
	{
		FClassReloadHelperTestAccess::ResetPerformReinstanceTestHooks();
		if (UseUnrealReloadCVar != nullptr)
		{
			UseUnrealReloadCVar->Set(SavedUseUnrealReload, ECVF_SetByCode);
		}

		EngineScope.Reset();
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

	if (!TestNotNull(TEXT("ClassReloadHelper.ASDelegateGraph test should create a testing engine"), Engine.Get()))
	{
		return false;
	}
	if (!TestNotNull(TEXT("ClassReloadHelper.ASDelegateGraph test should expose GEditor"), GEditor))
	{
		return false;
	}

	EngineScope = MakeUnique<FAngelscriptEngineScope>(*Engine);
	Engine->bIsInitialCompileFinished = true;
	EnsureClassReloadHelperInitialized(*Engine);

	if (UseUnrealReloadCVar != nullptr)
	{
		UseUnrealReloadCVar->Set(0, ECVF_SetByCode);
	}

	const TCHAR* ScriptV1 = TEXT(R"AS(
		event void FClassReloadHelperASGraphSignal(int Value);

		UCLASS()
		class UClassReloadHelperASGraphCarrier : UObject
		{
			UPROPERTY(BlueprintReadWrite)
			FClassReloadHelperASGraphSignal OnSignal;
		}
	)AS");
	if (!CompileMemoryModule(Test, *Engine, TEXT("ASDelegateGraph"), ScriptV1))
	{
		return false;
	}

	const TSharedPtr<FAngelscriptDelegateDesc> OldDelegateDesc = Engine->GetDelegate(TEXT("FClassReloadHelperASGraphSignal"));
	if (!TestTrue(TEXT("ClassReloadHelper.ASDelegateGraph test should expose the V1 AS delegate metadata"), OldDelegateDesc.IsValid()))
	{
		return false;
	}
	UDelegateFunction* OldDelegate = OldDelegateDesc->Function;
	if (!TestNotNull(TEXT("ClassReloadHelper.ASDelegateGraph test should expose the V1 AS delegate function"), OldDelegate))
	{
		return false;
	}

	UBlueprint* DependentBlueprint = CreateTransientBlueprintChild(Test, UObject::StaticClass(), TEXT("ASDependent"), RootedObjects);
	UBlueprint* ControlBlueprint = CreateTransientBlueprintChild(Test, UObject::StaticClass(), TEXT("ASControl"), RootedObjects);
	if (DependentBlueprint == nullptr || ControlBlueprint == nullptr)
	{
		return false;
	}

	UK2Node_Event* EventNode = AddExternalDelegateEventNode(Test, *DependentBlueprint, OldDelegate, TEXT("ASDependent"));
	if (EventNode == nullptr)
	{
		return false;
	}

	const TCHAR* ScriptV2 = TEXT(R"AS(
		event void FClassReloadHelperASGraphSignal(int Value, int Bonus);

		UCLASS()
		class UClassReloadHelperASGraphCarrier : UObject
		{
			UPROPERTY(BlueprintReadWrite)
			FClassReloadHelperASGraphSignal OnSignal;
		}
	)AS");

	FPerformReinstanceDelegateCallLog CallLog;
	FClassReloadHelper::FReloadState& ReloadState = FClassReloadHelper::ReloadState();
	ReloadState = FClassReloadHelper::FReloadState();

	FClassReloadHelperPerformReinstanceTestHooks Hooks;
	Hooks.QueueBlueprintForCompilation = [&CallLog](UBlueprint* Blueprint)
	{
		CallLog.QueuedBlueprints.Add(Blueprint);
	};
	Hooks.FlushCompilationQueueAndReinstance = [&CallLog]()
	{
		++CallLog.FlushCompilationQueueAndReinstanceCalls;
	};
	FClassReloadHelperTestAccess::SetPerformReinstanceTestHooks(MoveTemp(Hooks));

	if (!CompileMemoryModule(Test, *Engine, TEXT("ASDelegateGraph"), ScriptV2))
	{
		return false;
	}

	const TSharedPtr<FAngelscriptDelegateDesc> NewDelegateDesc = Engine->GetDelegate(TEXT("FClassReloadHelperASGraphSignal"));
	if (!TestTrue(TEXT("ClassReloadHelper.ASDelegateGraph test should expose the V2 AS delegate metadata"), NewDelegateDesc.IsValid()))
	{
		return false;
	}
	UDelegateFunction* NewDelegate = NewDelegateDesc->Function;
	if (!TestNotNull(TEXT("ClassReloadHelper.ASDelegateGraph test should expose the V2 AS delegate function"), NewDelegate))
	{
		return false;
	}
	if (!TestTrue(TEXT("ClassReloadHelper.ASDelegateGraph test should replace the AS delegate function across signature reload"), OldDelegate != NewDelegate))
	{
		return false;
	}

	AngelscriptEditor::BlueprintImpact::FBlueprintImpactSymbols Symbols;
	Symbols.Delegates.Add(OldDelegate);
	Symbols.Delegates.Add(NewDelegate);

	TArray<AngelscriptEditor::BlueprintImpact::EBlueprintImpactReason> DependentReasons;
	const bool bDependentImpacted = AngelscriptEditor::BlueprintImpact::AnalyzeLoadedBlueprint(*DependentBlueprint, Symbols, DependentReasons);
	if (!TestTrue(TEXT("ClassReloadHelper.ASDelegateGraph test should detect a Blueprint graph event node bound to the reloaded AS delegate"), bDependentImpacted))
	{
		return false;
	}
	if (!TestTrue(TEXT("ClassReloadHelper.ASDelegateGraph test should classify the AS graph dependency as a delegate-signature impact"), DependentReasons.Contains(AngelscriptEditor::BlueprintImpact::EBlueprintImpactReason::DelegateSignature)))
	{
		return false;
	}

	TArray<AngelscriptEditor::BlueprintImpact::EBlueprintImpactReason> ControlReasons;
	if (!TestFalse(TEXT("ClassReloadHelper.ASDelegateGraph test should leave unrelated Blueprints out of delegate impact analysis"), AngelscriptEditor::BlueprintImpact::AnalyzeLoadedBlueprint(*ControlBlueprint, Symbols, ControlReasons)))
	{
		return false;
	}

	int32 DependentQueueCount = 0;
	int32 ControlQueueCount = 0;
	for (const TWeakObjectPtr<UBlueprint>& QueuedBlueprint : CallLog.QueuedBlueprints)
	{
		if (QueuedBlueprint.Get() == DependentBlueprint)
		{
			++DependentQueueCount;
		}
		if (QueuedBlueprint.Get() == ControlBlueprint)
		{
			++ControlQueueCount;
		}
	}

	if (!TestEqual(TEXT("ClassReloadHelper.ASDelegateGraph should queue the Blueprint graph bound to the old AS delegate exactly once"), DependentQueueCount, 1))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.ASDelegateGraph should not queue the unrelated control Blueprint"), ControlQueueCount, 0))
	{
		return false;
	}
	if (!TestEqual(TEXT("ClassReloadHelper.ASDelegateGraph should flush the compilation queue once"), CallLog.FlushCompilationQueueAndReinstanceCalls, 1))
	{
		return false;
	}
	return TestTrue(TEXT("ClassReloadHelper.ASDelegateGraph should reconstruct the event node against the new AS delegate signature"), EventNode->FindEventSignatureFunction() == NewDelegate);
}

#undef TestTrue
#undef TestFalse
#undef TestEqual
#undef TestNotNull

TEST_CLASS_WITH_FLAGS(FAngelscriptClassReloadHelperDelegateTests,
	"Angelscript.Editor.ClassReloadHelper",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(PerformReinstanceRecompilesBlueprintsBoundToReloadedDelegates)
	{
		ASSERT_THAT(IsTrue(RunPerformReinstanceDelegateDependency(*TestRunner)));
	}

	TEST_METHOD(PerformReinstanceRecompilesBlueprintsBoundToReloadedAngelscriptDelegates)
	{
		ASSERT_THAT(IsTrue(RunPerformReinstanceAngelscriptDelegateGraphDependency(*TestRunner)));
	}
};

#endif
