#include "AngelscriptEngine.h"
#include "ClassGenerator/ASClass.h"
#include "Binds/Bind_Actor.h"
#include "Binds/Helper_ToString.h"
#include "Debugging/AngelscriptDebugServer.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptDebuggerTestClient.h"
#include "AngelscriptDebuggerTestHelpers.h"
#include "AngelscriptDebuggerTestSession.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestWorld.h"
#include "CQTest.h"
#include "Components/SceneComponent.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "UObject/Package.h"
#include "AngelscriptNativeScriptTestObject.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptEngineHooksTests,
	"Angelscript.TestModule.CppTests.Engine.Hooks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
struct FEngineHooksContextGuard
{
	TArray<FAngelscriptEngine*> SavedStack;

	FEngineHooksContextGuard()
	{
		SavedStack = FAngelscriptEngineContextStack::SnapshotAndClear();
	}

	~FEngineHooksContextGuard()
	{
		FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(SavedStack));
	}

	void DiscardSavedStack()
	{
		SavedStack.Reset();
	}
};

static bool WaitForDebuggerEnvelopeType(
	FAutomationTestBase& Test,
	FAngelscriptDebuggerTestSession& Session,
	FAngelscriptDebuggerTestClient& Client,
	EDebugMessageType ExpectedType,
	TOptional<FAngelscriptDebugMessageEnvelope>& OutEnvelope,
	const TCHAR* Context)
{
	FNoDiscardAsserter LocalAssert(Test);
	const bool bReceivedEnvelope = Session.PumpUntil(
		[&Client, &OutEnvelope, ExpectedType]()
		{
			if (OutEnvelope.IsSet())
			{
				return true;
			}

			TOptional<FAngelscriptDebugMessageEnvelope> Envelope = Client.ReceiveEnvelope();
			if (Envelope.IsSet() && Envelope->MessageType == ExpectedType)
			{
				OutEnvelope = MoveTemp(Envelope);
				return true;
			}

			return false;
		},
		Session.GetDefaultTimeoutSeconds());

	if (!LocalAssert.IsTrue(bReceivedEnvelope, Context))
	{
		if (!Client.GetLastError().IsEmpty())
		{
			Test.AddError(Client.GetLastError());
		}
		return false;
	}

	return true;
}

public:
	TEST_METHOD(PreCompileHookDoesNotLeakBetweenEngines)
	{
FEngineHooksContextGuard ContextGuard;
		DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized())
		{
			FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
		}
		ContextGuard.DiscardSavedStack();

		ON_SCOPE_EXIT
		{
			FAngelscriptEngineContextStack::SnapshotAndClear();
			if (FAngelscriptEngine::IsInitialized())
			{
				FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
			}
			DestroySharedTestEngine();
		};

		TUniquePtr<FAngelscriptEngine> EngineA = CreateFullTestEngine();
		TUniquePtr<FAngelscriptEngine> EngineB = CreateFullTestEngine();
		if (!this->Assert.IsNotNull(EngineA.Get(), TEXT("Engine hook isolation test should create engine A"))
			|| !this->Assert.IsNotNull(EngineB.Get(), TEXT("Engine hook isolation test should create engine B")))
		{
			return;
		}

		int32 EngineAHookCount = 0;
		FDelegateHandle EngineAHandle = EngineA->GetPreCompile().AddLambda(
			[&EngineAHookCount]()
			{
				++EngineAHookCount;
			});
		ON_SCOPE_EXIT
		{
			EngineA->GetPreCompile().Remove(EngineAHandle);
		};

		EngineB->GetPreCompile().Broadcast();
		bool bOk = true;
		bOk &= this->Assert.AreEqual(
			0,
			EngineAHookCount,
			TEXT("Broadcasting engine B pre-compile hooks should not fire hooks registered on engine A"));

		EngineA->GetPreCompile().Broadcast();
		bOk &= this->Assert.AreEqual(
			1,
			EngineAHookCount,
			TEXT("Broadcasting engine A pre-compile hooks should fire hooks registered on engine A"));
		(void)bOk;
	}

	TEST_METHOD(RuntimeHookDelegatesAreEngineOwned)
	{
FEngineHooksContextGuard ContextGuard;
		DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized())
		{
			FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
		}
		ContextGuard.DiscardSavedStack();

		ON_SCOPE_EXIT
		{
			FAngelscriptEngineContextStack::SnapshotAndClear();
			if (FAngelscriptEngine::IsInitialized())
			{
				FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
			}
			DestroySharedTestEngine();
		};

		TUniquePtr<FAngelscriptEngine> EngineA = CreateFullTestEngine();
		TUniquePtr<FAngelscriptEngine> EngineB = CreateFullTestEngine();
		if (!this->Assert.IsNotNull(EngineA.Get(), TEXT("Runtime hook ownership test should create engine A"))
			|| !this->Assert.IsNotNull(EngineB.Get(), TEXT("Runtime hook ownership test should create engine B")))
		{
			return;
		}

		UActorComponent* Component = NewObject<USceneComponent>();
		UObject* LiteralAsset = NewObject<UWorld>();
		UObject* DebugObject = NewObject<UWorld>();
		ULevel* SpawnLevel = NewObject<ULevel>();

		int32 ComponentCreatedCount = 0;
		EngineA->GetComponentCreated().BindLambda(
			[&ComponentCreatedCount, Component](UActorComponent* InComponent)
			{
				if (InComponent == Component)
				{
					++ComponentCreatedCount;
				}
			});

		UObject* LiteralCreatedObject = nullptr;
		FString LiteralCreatedName;
		const FDelegateHandle LiteralCreatedHandle = EngineA->GetOnLiteralAssetCreated().AddLambda(
			[&LiteralCreatedObject, &LiteralCreatedName](UObject* InAsset, const FString& InName)
			{
				LiteralCreatedObject = InAsset;
				LiteralCreatedName = InName;
			});
		ON_SCOPE_EXIT
		{
			EngineA->GetOnLiteralAssetCreated().Remove(LiteralCreatedHandle);
		};

		UObject* LiteralSetupObject = nullptr;
		FString LiteralSetupName;
		const FDelegateHandle LiteralSetupHandle = EngineA->GetPostLiteralAssetSetup().AddLambda(
			[&LiteralSetupObject, &LiteralSetupName](UObject* InAsset, const FString& InName)
			{
				LiteralSetupObject = InAsset;
				LiteralSetupName = InName;
			});
		ON_SCOPE_EXIT
		{
			EngineA->GetPostLiteralAssetSetup().Remove(LiteralSetupHandle);
		};

		EngineA->GetDynamicSpawnLevel().BindLambda([SpawnLevel]() { return SpawnLevel; });

		int32 BreakCheckCount = 0;
		EngineA->GetDebugCheckBreakOptions().BindLambda(
			[&BreakCheckCount](const FAngelscriptDebugBreakOptions& BreakOptions, UObject* WorldContext)
			{
				++BreakCheckCount;
				return BreakOptions.Contains(TEXT("break:any")) && WorldContext != nullptr;
			});

		EngineA->GetDebugBreakFilters().BindLambda(
			[](FAngelscriptDebugBreakFilters& OutFilters)
			{
				OutFilters.Add(TEXT("break:engine-owned-test"), TEXT("Engine Owned Test"));
			});

		EngineA->GetDebugObjectSuffix().BindLambda(
			[](UObject* Object, FString& OutSuffix)
			{
				if (Object != nullptr)
				{
					OutSuffix = TEXT("[engine-hook]");
				}
			});

		EngineB->GetComponentCreated().ExecuteIfBound(Component);
		EngineB->GetOnLiteralAssetCreated().Broadcast(LiteralAsset, TEXT("WrongEngineAsset"));
		EngineB->GetPostLiteralAssetSetup().Broadcast(LiteralAsset, TEXT("WrongEngineSetup"));
		bool bOk = true;
		bOk &= this->Assert.AreEqual(0, ComponentCreatedCount, TEXT("Component-created hook should not leak to engine B"));
		bOk &= this->Assert.IsNull(LiteralCreatedObject, TEXT("Literal-created hook should not leak to engine B"));
		bOk &= this->Assert.IsNull(LiteralSetupObject, TEXT("Literal-setup hook should not leak to engine B"));

		EngineA->GetComponentCreated().ExecuteIfBound(Component);
		EngineA->GetOnLiteralAssetCreated().Broadcast(LiteralAsset, TEXT("OwnedAsset"));
		EngineA->GetPostLiteralAssetSetup().Broadcast(LiteralAsset, TEXT("OwnedSetup"));
		bOk &= this->Assert.AreEqual(1, ComponentCreatedCount, TEXT("Component-created hook should fire for its owning engine"));
		bOk &= this->Assert.AreEqual(LiteralAsset, LiteralCreatedObject, TEXT("Literal-created hook should carry object for its owning engine"));
		bOk &= this->Assert.AreEqual(FString(TEXT("OwnedAsset")), LiteralCreatedName, TEXT("Literal-created hook should carry name for its owning engine"));
		bOk &= this->Assert.AreEqual(LiteralAsset, LiteralSetupObject, TEXT("Literal-setup hook should carry object for its owning engine"));
		bOk &= this->Assert.AreEqual(FString(TEXT("OwnedSetup")), LiteralSetupName, TEXT("Literal-setup hook should carry name for its owning engine"));

		bOk &= this->Assert.AreEqual(SpawnLevel, EngineA->GetDynamicSpawnLevel().Execute(), TEXT("Dynamic-spawn-level hook should return the owning engine value"));

		FAngelscriptDebugBreakOptions BreakOptions;
		BreakOptions.Add(TEXT("break:any"));
		bOk &= this->Assert.IsTrue(EngineA->GetDebugCheckBreakOptions().Execute(BreakOptions, DebugObject), TEXT("Debug break check hook should run through the owning engine"));
		bOk &= this->Assert.AreEqual(1, BreakCheckCount, TEXT("Debug break check hook should fire once"));

		FAngelscriptDebugBreakFilters BreakFilters;
		EngineA->GetDebugBreakFilters().ExecuteIfBound(BreakFilters);
		const FString* BreakFilterTitle = BreakFilters.Find(TEXT("break:engine-owned-test"));
		bOk &= this->Assert.IsNotNull(BreakFilterTitle, TEXT("Debug break filters hook should populate the owning engine filters"));
		if (BreakFilterTitle != nullptr)
		{
			bOk &= this->Assert.AreEqual(FString(TEXT("Engine Owned Test")), *BreakFilterTitle, TEXT("Debug break filters hook should carry the registered title"));
		}

		FString DebugSuffix;
		EngineA->GetDebugObjectSuffix().ExecuteIfBound(DebugObject, DebugSuffix);
		bOk &= this->Assert.AreEqual(FString(TEXT("[engine-hook]")), DebugSuffix, TEXT("Debug object suffix hook should mutate the suffix"));
		(void)bOk;
	}

	TEST_METHOD(RuntimeBindCallSitesUseCurrentEngineHooks)
	{
		DestroySharedAndStrayGlobalTestEngine();
		FAngelscriptEngine& Engine = AcquireCleanSharedCloneEngine();
		FAngelscriptTestWorld World(*TestRunner, Engine);
		if (!World.IsValid())
		{
			return;
		}

		AActor* Owner = World.GetWorld().SpawnActor<AActor>();
		if (!this->Assert.IsNotNull(Owner, TEXT("Runtime hook bind-path test should spawn an owner actor")))
		{
			return;
		}

		int32 ComponentCreatedCount = 0;
		Engine.GetComponentCreated().BindLambda(
			[&ComponentCreatedCount](UActorComponent* Component)
			{
				if (Component != nullptr && Component->GetFName() == TEXT("EngineOwnedHookComponent"))
				{
					++ComponentCreatedCount;
				}
			});
		ON_SCOPE_EXIT
		{
			Engine.GetComponentCreated().Unbind();
		};

		UActorComponent* CreatedComponent = FAngelscriptActorBinds::CreateComponent(
			Owner,
			USceneComponent::StaticClass(),
			TEXT("EngineOwnedHookComponent"));

		bool bOk = true;
		bOk &= this->Assert.IsNotNull(CreatedComponent, TEXT("Runtime hook bind-path test should create a component through the bind helper"));
		bOk &= this->Assert.AreEqual(1, ComponentCreatedCount, TEXT("CreateComponent bind path should invoke the current engine's component-created hook"));

		UObject* LiteralCreatedObject = nullptr;
		FString LiteralCreatedName;
		const FDelegateHandle LiteralCreatedHandle = Engine.GetOnLiteralAssetCreated().AddLambda(
			[&LiteralCreatedObject, &LiteralCreatedName](UObject* Asset, const FString& Name)
			{
				LiteralCreatedObject = Asset;
				LiteralCreatedName = Name;
			});
		ON_SCOPE_EXIT
		{
			Engine.GetOnLiteralAssetCreated().Remove(LiteralCreatedHandle);
		};

		UObject* LiteralSetupObject = nullptr;
		FString LiteralSetupName;
		const FDelegateHandle LiteralSetupHandle = Engine.GetPostLiteralAssetSetup().AddLambda(
			[&LiteralSetupObject, &LiteralSetupName](UObject* Asset, const FString& Name)
			{
				LiteralSetupObject = Asset;
				LiteralSetupName = Name;
			});
		ON_SCOPE_EXIT
		{
			Engine.GetPostLiteralAssetSetup().Remove(LiteralSetupHandle);
		};

		asIScriptModule* Module = BuildModule(
			*TestRunner,
			Engine,
			"RuntimeBindHookCallSites",
			TEXT(R"AS(
UCLASS()
class UEngineOwnedHookLiteralAsset : UObject
{
}

int Run()
{
	UObject Asset = __CreateLiteralAsset(UEngineOwnedHookLiteralAsset, "EngineOwnedHookLiteralAsset");
	if (Asset == nullptr)
		return 10;

	__PostLiteralAssetSetup(Asset, "EngineOwnedHookLiteralAsset");
	return 1;
}
)AS"));
		if (Module == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(TEXT("RuntimeBindHookCallSites"));
		};

		asIScriptFunction* RunFunction = GetFunctionByDecl(*TestRunner, *Module, TEXT("int Run()"));
		if (RunFunction == nullptr)
		{
			return;
		}

		int32 Result = 0;
		if (!ExecuteIntFunction(*TestRunner, Engine, *RunFunction, Result))
		{
			return;
		}

		bOk &= this->Assert.AreEqual(1, Result, TEXT("Runtime hook literal-asset script should execute"));
		bOk &= this->Assert.IsNotNull(LiteralCreatedObject, TEXT("__CreateLiteralAsset bind path should invoke the current engine's literal-created hook"));
		bOk &= this->Assert.AreEqual(FString(TEXT("EngineOwnedHookLiteralAsset")), LiteralCreatedName, TEXT("__CreateLiteralAsset bind path should pass the literal asset name"));
		bOk &= this->Assert.AreEqual(LiteralCreatedObject, LiteralSetupObject, TEXT("__PostLiteralAssetSetup bind path should pass the literal asset object"));
		bOk &= this->Assert.AreEqual(FString(TEXT("EngineOwnedHookLiteralAsset")), LiteralSetupName, TEXT("__PostLiteralAssetSetup bind path should pass the literal asset name"));

		ULevel* OverrideLevel = World.GetWorld().PersistentLevel;
		int32 DynamicSpawnLevelCalls = 0;
		Engine.GetDynamicSpawnLevel().BindLambda(
			[OverrideLevel, &DynamicSpawnLevelCalls]()
			{
				++DynamicSpawnLevelCalls;
				return OverrideLevel;
			});
		ON_SCOPE_EXIT
		{
			Engine.GetDynamicSpawnLevel().Unbind();
		};

		{
			FAngelscriptEngineScope EngineScope(Engine, &World.GetWorld());
			AActor* SpawnedActor = FAngelscriptActorBinds::SpawnActor(
				AActor::StaticClass(),
				FVector::ZeroVector,
				FRotator::ZeroRotator,
				TEXT("EngineOwnedHookSpawnedActor"),
				false,
				nullptr);

			bOk &= this->Assert.IsNotNull(SpawnedActor, TEXT("Runtime hook bind-path test should spawn an actor through the bind helper"));
			bOk &= this->Assert.AreEqual(1, DynamicSpawnLevelCalls, TEXT("SpawnActor bind path should invoke the current engine's dynamic-spawn-level hook"));
			if (SpawnedActor != nullptr)
			{
				bOk &= this->Assert.AreEqual(OverrideLevel, SpawnedActor->GetLevel(), TEXT("SpawnActor bind path should use the current engine's dynamic-spawn-level hook"));
			}
		}
		(void)bOk;
	}

	TEST_METHOD(DebugServerCallSitesUseCurrentEngineHooks)
	{
		FAngelscriptDebuggerTestSession Session;
		FAngelscriptDebuggerSessionConfig SessionConfig;
		SessionConfig.DefaultTimeoutSeconds = kDefaultDebuggerTestTimeoutSeconds;
		if (!this->Assert.IsTrue(Session.Initialize(SessionConfig), TEXT("Debug hook call-site test should initialize a debugger session")))
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			Session.Shutdown();
		};

		FAngelscriptEngine& Engine = Session.GetEngine();
		FAngelscriptEngineScope EngineScope(Engine);

		UClass* ScriptClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			TEXT("DebugHookCallSites"),
			TEXT("DebugHookCallSites.as"),
			TEXT(R"AS(
UCLASS()
class ADebugHookObjectSuffixActor : AActor
{
}
)AS"),
			TEXT("ADebugHookObjectSuffixActor"));
		if (ScriptClass == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(TEXT("DebugHookCallSites"));
		};

		int32 DebugSuffixCalls = 0;
		Engine.GetDebugObjectSuffix().BindLambda(
			[&DebugSuffixCalls](UObject* Object, FString& OutSuffix)
			{
				if (Object != nullptr)
				{
					++DebugSuffixCalls;
					OutSuffix = TEXT("[engine-owned-debug-suffix]");
				}
			});
		ON_SCOPE_EXIT
		{
			Engine.GetDebugObjectSuffix().Unbind();
		};

		UASClass* ScriptASClass = Cast<UASClass>(ScriptClass);
		if (!this->Assert.IsNotNull(ScriptASClass, TEXT("Debug hook call-site test should find the generated suffix actor class")))
		{
			return;
		}

		asITypeInfo* ScriptTypeInfo = reinterpret_cast<asITypeInfo*>(ScriptASClass->ScriptTypePtr);
		if (ScriptTypeInfo == nullptr)
		{
			const FString BoundTypeName = FAngelscriptType::GetBoundClassName(ScriptClass);
			const FTCHARToUTF8 BoundTypeNameUtf8(*BoundTypeName);
			ScriptTypeInfo = Engine.GetScriptEngine()->GetTypeInfoByName(BoundTypeNameUtf8.Get());
		}
		if (!this->Assert.IsNotNull(ScriptTypeInfo, TEXT("Debug hook call-site test should resolve the generated suffix actor script type")))
		{
			return;
		}

		UObject* ScriptObject = NewObject<UObject>(GetTransientPackage(), ScriptClass, TEXT("EngineOwnedDebugSuffixObject"));
		if (!this->Assert.IsNotNull(ScriptObject, TEXT("Debug hook call-site test should create a generated suffix object")))
		{
			return;
		}

		const int32 ScriptObjectTypeId = ScriptTypeInfo->GetTypeId();
		UObject* ScriptObjectValue = ScriptObject;
		FString DebugString;
		FToStringHelper::Generic_AppendToString(DebugString, &ScriptObjectValue, ScriptObjectTypeId);
		bool bOk = true;
		bOk &= this->Assert.IsTrue(
			DebugString.Contains(TEXT("[engine-owned-debug-suffix]")),
			TEXT("ToString debug path should use the current engine's debug-object-suffix hook"));
		bOk &= this->Assert.AreEqual(
			1,
			DebugSuffixCalls,
			TEXT("ToString debug path should invoke the current engine's debug-object-suffix hook once"));

		int32 BreakFilterCalls = 0;
		Engine.GetDebugBreakFilters().BindLambda(
			[&BreakFilterCalls](FAngelscriptDebugBreakFilters& OutFilters)
			{
				++BreakFilterCalls;
				OutFilters.Add(FName(TEXT("break:engine-owned-runtime")), TEXT("Engine Owned Runtime"));
			});
		ON_SCOPE_EXIT
		{
			Engine.GetDebugBreakFilters().Unbind();
		};

		FAngelscriptDebuggerTestClient Client;
		if (!this->Assert.IsTrue(Client.Connect(TEXT("127.0.0.1"), Session.GetPort()), TEXT("Debug hook call-site test should connect a debugger client")))
		{
			TestRunner->AddError(Client.GetLastError());
			return;
		}
		ON_SCOPE_EXIT
		{
			Client.SendStopDebugging();
			Client.SendDisconnect();
			Client.Disconnect();
		};

		if (!this->Assert.IsTrue(Client.SendStartDebugging(2), TEXT("Debug hook call-site test should send StartDebugging")))
		{
			TestRunner->AddError(Client.GetLastError());
			return;
		}

		TOptional<FAngelscriptDebugMessageEnvelope> VersionEnvelope;
		if (!WaitForDebuggerEnvelopeType(
			*TestRunner,
			Session,
			Client,
			EDebugMessageType::DebugServerVersion,
			VersionEnvelope,
			TEXT("Debug hook call-site test should receive the DebugServerVersion response")))
		{
			return;
		}

		if (!this->Assert.IsTrue(Client.SendRequestBreakFilters(), TEXT("Debug hook call-site test should request break filters")))
		{
			TestRunner->AddError(Client.GetLastError());
			return;
		}

		TOptional<FAngelscriptDebugMessageEnvelope> BreakFiltersEnvelope;
		if (!WaitForDebuggerEnvelopeType(
			*TestRunner,
			Session,
			Client,
			EDebugMessageType::BreakFilters,
			BreakFiltersEnvelope,
			TEXT("Debug hook call-site test should receive a BreakFilters response")))
		{
			return;
		}

		const TOptional<FAngelscriptBreakFilters> BreakFilters =
			FAngelscriptDebuggerTestClient::DeserializeMessage<FAngelscriptBreakFilters>(BreakFiltersEnvelope.GetValue());
		if (!this->Assert.IsTrue(BreakFilters.IsSet(), TEXT("Debug hook call-site test should deserialize the BreakFilters response")))
		{
			return;
		}

		bOk &= this->Assert.AreEqual(1, BreakFilterCalls, TEXT("RequestBreakFilters debug-server path should invoke the current engine's hook once"));
		const int32 FilterIndex = BreakFilters->Filters.IndexOfByKey(FString(TEXT("break:engine-owned-runtime")));
		if (!this->Assert.IsTrue(FilterIndex != INDEX_NONE, TEXT("RequestBreakFilters debug-server path should include the engine-owned filter")))
		{
			return;
		}

		bOk &= this->Assert.IsTrue(
			BreakFilters->FilterTitles.IsValidIndex(FilterIndex)
			&& BreakFilters->FilterTitles[FilterIndex] == TEXT("Engine Owned Runtime"),
			TEXT("RequestBreakFilters debug-server path should preserve the engine-owned filter title"));

		int32 BreakCheckCalls = 0;
		Engine.GetDebugCheckBreakOptions().BindLambda(
			[&BreakCheckCalls](const FAngelscriptDebugBreakOptions& BreakOptions, UObject* WorldContext)
			{
				++BreakCheckCalls;
				return BreakOptions.Contains(FName(TEXT("break:allow-engine-owned"))) && WorldContext != nullptr;
			});
		ON_SCOPE_EXIT
		{
			Engine.GetDebugCheckBreakOptions().Unbind();
		};

		Engine.AssignWorldContext(NewObject<UAngelscriptNativeScriptTestObject>(GetTransientPackage(), TEXT("EngineOwnedHookDebugWorldContext")));
		ON_SCOPE_EXIT
		{
			Engine.AssignWorldContext(nullptr);
		};

		Session.GetDebugServer().BreakOptions.Reset();
		Session.GetDebugServer().BreakOptions.Add(FName(TEXT("break:allow-engine-owned")));
		bOk &= this->Assert.IsTrue(Session.GetDebugServer().ShouldBreakOnActiveSide(), TEXT("ShouldBreakOnActiveSide should use the current engine's debug-check hook"));
		bOk &= this->Assert.AreEqual(1, BreakCheckCalls, TEXT("ShouldBreakOnActiveSide should invoke the current engine's debug-check hook once"));
		(void)bOk;
	}
};

#endif
