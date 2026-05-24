#include "AngelscriptEngine.h"
#include "ClassGenerator/ASClass.h"
#include "Binds/Bind_Actor.h"
#include "Binds/Helper_ToString.h"
#include "Debugging/AngelscriptDebugServer.h"
#include "Shared/AngelscriptFunctionalTestUtils.h"
#include "Shared/AngelscriptDebuggerTestClient.h"
#include "Shared/AngelscriptDebuggerTestHelpers.h"
#include "Shared/AngelscriptDebuggerTestSession.h"
#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestWorld.h"
#include "CQTest.h"
#include "Components/SceneComponent.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "UObject/Package.h"
#include "Shared/AngelscriptNativeScriptTestObject.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_Core_AngelscriptEngineHooksTests_Private
{
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

	bool WaitForDebuggerEnvelopeType(
		FAutomationTestBase& Test,
		AngelscriptTestSupport::FAngelscriptDebuggerTestSession& Session,
		AngelscriptTestSupport::FAngelscriptDebuggerTestClient& Client,
		EDebugMessageType ExpectedType,
		TOptional<FAngelscriptDebugMessageEnvelope>& OutEnvelope,
		const TCHAR* Context)
	{
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

		if (!Test.TestTrue(Context, bReceivedEnvelope))
		{
			if (!Client.GetLastError().IsEmpty())
			{
				Test.AddError(Client.GetLastError());
			}
			return false;
		}

		return true;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptEngineHooksTests,
	"Angelscript.TestModule.CppTests.Engine.Hooks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(PreCompileHookDoesNotLeakBetweenEngines)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineHooksTests_Private;
		FEngineHooksContextGuard ContextGuard;
		AngelscriptTestSupport::DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized())
		{
			AngelscriptTestSupport::FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
		}
		ContextGuard.DiscardSavedStack();

		ON_SCOPE_EXIT
		{
			FAngelscriptEngineContextStack::SnapshotAndClear();
			if (FAngelscriptEngine::IsInitialized())
			{
				AngelscriptTestSupport::FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
			}
			AngelscriptTestSupport::DestroySharedTestEngine();
		};

		TUniquePtr<FAngelscriptEngine> EngineA = AngelscriptTestSupport::CreateFullTestEngine();
		TUniquePtr<FAngelscriptEngine> EngineB = AngelscriptTestSupport::CreateFullTestEngine();
		if (!TestRunner->TestNotNull(TEXT("Engine hook isolation test should create engine A"), EngineA.Get())
			|| !TestRunner->TestNotNull(TEXT("Engine hook isolation test should create engine B"), EngineB.Get()))
		{
			return;
		}

		int32 EngineAHookCount = 0;
		FDelegateHandle EngineAHandle = EngineA->GetHooks().GetPreCompile().AddLambda(
			[&EngineAHookCount]()
			{
				++EngineAHookCount;
			});
		ON_SCOPE_EXIT
		{
			EngineA->GetHooks().GetPreCompile().Remove(EngineAHandle);
		};

		EngineB->GetHooks().GetPreCompile().Broadcast();
		TestRunner->TestEqual(
			TEXT("Broadcasting engine B pre-compile hooks should not fire hooks registered on engine A"),
			EngineAHookCount,
			0);

		EngineA->GetHooks().GetPreCompile().Broadcast();
		TestRunner->TestEqual(
			TEXT("Broadcasting engine A pre-compile hooks should fire hooks registered on engine A"),
			EngineAHookCount,
			1);
	}

	TEST_METHOD(RuntimeHookDelegatesAreEngineOwned)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineHooksTests_Private;
		FEngineHooksContextGuard ContextGuard;
		AngelscriptTestSupport::DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized())
		{
			AngelscriptTestSupport::FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
		}
		ContextGuard.DiscardSavedStack();

		ON_SCOPE_EXIT
		{
			FAngelscriptEngineContextStack::SnapshotAndClear();
			if (FAngelscriptEngine::IsInitialized())
			{
				AngelscriptTestSupport::FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
			}
			AngelscriptTestSupport::DestroySharedTestEngine();
		};

		TUniquePtr<FAngelscriptEngine> EngineA = AngelscriptTestSupport::CreateFullTestEngine();
		TUniquePtr<FAngelscriptEngine> EngineB = AngelscriptTestSupport::CreateFullTestEngine();
		if (!TestRunner->TestNotNull(TEXT("Runtime hook ownership test should create engine A"), EngineA.Get())
			|| !TestRunner->TestNotNull(TEXT("Runtime hook ownership test should create engine B"), EngineB.Get()))
		{
			return;
		}

		UActorComponent* Component = NewObject<USceneComponent>();
		UObject* LiteralAsset = NewObject<UWorld>();
		UObject* DebugObject = NewObject<UWorld>();
		ULevel* SpawnLevel = NewObject<ULevel>();

		int32 ComponentCreatedCount = 0;
		EngineA->GetHooks().GetComponentCreated().BindLambda(
			[&ComponentCreatedCount, Component](UActorComponent* InComponent)
			{
				if (InComponent == Component)
				{
					++ComponentCreatedCount;
				}
			});

		UObject* LiteralCreatedObject = nullptr;
		FString LiteralCreatedName;
		const FDelegateHandle LiteralCreatedHandle = EngineA->GetHooks().GetOnLiteralAssetCreated().AddLambda(
			[&LiteralCreatedObject, &LiteralCreatedName](UObject* InAsset, const FString& InName)
			{
				LiteralCreatedObject = InAsset;
				LiteralCreatedName = InName;
			});
		ON_SCOPE_EXIT
		{
			EngineA->GetHooks().GetOnLiteralAssetCreated().Remove(LiteralCreatedHandle);
		};

		UObject* LiteralSetupObject = nullptr;
		FString LiteralSetupName;
		const FDelegateHandle LiteralSetupHandle = EngineA->GetHooks().GetPostLiteralAssetSetup().AddLambda(
			[&LiteralSetupObject, &LiteralSetupName](UObject* InAsset, const FString& InName)
			{
				LiteralSetupObject = InAsset;
				LiteralSetupName = InName;
			});
		ON_SCOPE_EXIT
		{
			EngineA->GetHooks().GetPostLiteralAssetSetup().Remove(LiteralSetupHandle);
		};

		EngineA->GetHooks().GetDynamicSpawnLevel().BindLambda([SpawnLevel]() { return SpawnLevel; });

		int32 BreakCheckCount = 0;
		EngineA->GetHooks().GetDebugCheckBreakOptions().BindLambda(
			[&BreakCheckCount](const FAngelscriptDebugBreakOptions& BreakOptions, UObject* WorldContext)
			{
				++BreakCheckCount;
				return BreakOptions.Contains(TEXT("break:any")) && WorldContext != nullptr;
			});

		EngineA->GetHooks().GetDebugBreakFilters().BindLambda(
			[](FAngelscriptDebugBreakFilters& OutFilters)
			{
				OutFilters.Add(TEXT("break:engine-owned-test"), TEXT("Engine Owned Test"));
			});

		EngineA->GetHooks().GetDebugObjectSuffix().BindLambda(
			[](UObject* Object, FString& OutSuffix)
			{
				if (Object != nullptr)
				{
					OutSuffix = TEXT("[engine-hook]");
				}
			});

		EngineB->GetHooks().GetComponentCreated().ExecuteIfBound(Component);
		EngineB->GetHooks().GetOnLiteralAssetCreated().Broadcast(LiteralAsset, TEXT("WrongEngineAsset"));
		EngineB->GetHooks().GetPostLiteralAssetSetup().Broadcast(LiteralAsset, TEXT("WrongEngineSetup"));
		TestRunner->TestEqual(TEXT("Component-created hook should not leak to engine B"), ComponentCreatedCount, 0);
		TestRunner->TestNull(TEXT("Literal-created hook should not leak to engine B"), LiteralCreatedObject);
		TestRunner->TestNull(TEXT("Literal-setup hook should not leak to engine B"), LiteralSetupObject);

		EngineA->GetHooks().GetComponentCreated().ExecuteIfBound(Component);
		EngineA->GetHooks().GetOnLiteralAssetCreated().Broadcast(LiteralAsset, TEXT("OwnedAsset"));
		EngineA->GetHooks().GetPostLiteralAssetSetup().Broadcast(LiteralAsset, TEXT("OwnedSetup"));
		TestRunner->TestEqual(TEXT("Component-created hook should fire for its owning engine"), ComponentCreatedCount, 1);
		TestRunner->TestEqual(TEXT("Literal-created hook should carry object for its owning engine"), LiteralCreatedObject, LiteralAsset);
		TestRunner->TestEqual(TEXT("Literal-created hook should carry name for its owning engine"), LiteralCreatedName, FString(TEXT("OwnedAsset")));
		TestRunner->TestEqual(TEXT("Literal-setup hook should carry object for its owning engine"), LiteralSetupObject, LiteralAsset);
		TestRunner->TestEqual(TEXT("Literal-setup hook should carry name for its owning engine"), LiteralSetupName, FString(TEXT("OwnedSetup")));

		TestRunner->TestEqual(TEXT("Dynamic-spawn-level hook should return the owning engine value"), EngineA->GetHooks().GetDynamicSpawnLevel().Execute(), SpawnLevel);

		FAngelscriptDebugBreakOptions BreakOptions;
		BreakOptions.Add(TEXT("break:any"));
		TestRunner->TestTrue(TEXT("Debug break check hook should run through the owning engine"), EngineA->GetHooks().GetDebugCheckBreakOptions().Execute(BreakOptions, DebugObject));
		TestRunner->TestEqual(TEXT("Debug break check hook should fire once"), BreakCheckCount, 1);

		FAngelscriptDebugBreakFilters BreakFilters;
		EngineA->GetHooks().GetDebugBreakFilters().ExecuteIfBound(BreakFilters);
		const FString* BreakFilterTitle = BreakFilters.Find(TEXT("break:engine-owned-test"));
		TestRunner->TestNotNull(TEXT("Debug break filters hook should populate the owning engine filters"), BreakFilterTitle);
		if (BreakFilterTitle != nullptr)
		{
			TestRunner->TestEqual(TEXT("Debug break filters hook should carry the registered title"), *BreakFilterTitle, FString(TEXT("Engine Owned Test")));
		}

		FString DebugSuffix;
		EngineA->GetHooks().GetDebugObjectSuffix().ExecuteIfBound(DebugObject, DebugSuffix);
		TestRunner->TestEqual(TEXT("Debug object suffix hook should mutate the suffix"), DebugSuffix, FString(TEXT("[engine-hook]")));
	}

	TEST_METHOD(RuntimeBindCallSitesUseCurrentEngineHooks)
	{
		AngelscriptTestSupport::DestroySharedAndStrayGlobalTestEngine();
		FAngelscriptEngine& Engine = AngelscriptTestSupport::AcquireCleanSharedCloneEngine();
		AngelscriptTestSupport::FAngelscriptTestWorld World(*TestRunner, Engine);
		if (!World.IsValid())
		{
			return;
		}

		AActor* Owner = World.GetWorld().SpawnActor<AActor>();
		if (!TestRunner->TestNotNull(TEXT("Runtime hook bind-path test should spawn an owner actor"), Owner))
		{
			return;
		}

		int32 ComponentCreatedCount = 0;
		Engine.GetHooks().GetComponentCreated().BindLambda(
			[&ComponentCreatedCount](UActorComponent* Component)
			{
				if (Component != nullptr && Component->GetFName() == TEXT("EngineOwnedHookComponent"))
				{
					++ComponentCreatedCount;
				}
			});
		ON_SCOPE_EXIT
		{
			Engine.GetHooks().GetComponentCreated().Unbind();
		};

		UActorComponent* CreatedComponent = FAngelscriptActorBinds::CreateComponent(
			Owner,
			USceneComponent::StaticClass(),
			TEXT("EngineOwnedHookComponent"));

		TestRunner->TestNotNull(TEXT("Runtime hook bind-path test should create a component through the bind helper"), CreatedComponent);
		TestRunner->TestEqual(TEXT("CreateComponent bind path should invoke the current engine's component-created hook"), ComponentCreatedCount, 1);

		UObject* LiteralCreatedObject = nullptr;
		FString LiteralCreatedName;
		const FDelegateHandle LiteralCreatedHandle = Engine.GetHooks().GetOnLiteralAssetCreated().AddLambda(
			[&LiteralCreatedObject, &LiteralCreatedName](UObject* Asset, const FString& Name)
			{
				LiteralCreatedObject = Asset;
				LiteralCreatedName = Name;
			});
		ON_SCOPE_EXIT
		{
			Engine.GetHooks().GetOnLiteralAssetCreated().Remove(LiteralCreatedHandle);
		};

		UObject* LiteralSetupObject = nullptr;
		FString LiteralSetupName;
		const FDelegateHandle LiteralSetupHandle = Engine.GetHooks().GetPostLiteralAssetSetup().AddLambda(
			[&LiteralSetupObject, &LiteralSetupName](UObject* Asset, const FString& Name)
			{
				LiteralSetupObject = Asset;
				LiteralSetupName = Name;
			});
		ON_SCOPE_EXIT
		{
			Engine.GetHooks().GetPostLiteralAssetSetup().Remove(LiteralSetupHandle);
		};

		asIScriptModule* Module = AngelscriptTestSupport::BuildModule(
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

		asIScriptFunction* RunFunction = AngelscriptTestSupport::GetFunctionByDecl(*TestRunner, *Module, TEXT("int Run()"));
		if (RunFunction == nullptr)
		{
			return;
		}

		int32 Result = 0;
		if (!AngelscriptTestSupport::ExecuteIntFunction(*TestRunner, Engine, *RunFunction, Result))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Runtime hook literal-asset script should execute"), Result, 1);
		TestRunner->TestNotNull(TEXT("__CreateLiteralAsset bind path should invoke the current engine's literal-created hook"), LiteralCreatedObject);
		TestRunner->TestEqual(TEXT("__CreateLiteralAsset bind path should pass the literal asset name"), LiteralCreatedName, FString(TEXT("EngineOwnedHookLiteralAsset")));
		TestRunner->TestEqual(TEXT("__PostLiteralAssetSetup bind path should pass the literal asset object"), LiteralSetupObject, LiteralCreatedObject);
		TestRunner->TestEqual(TEXT("__PostLiteralAssetSetup bind path should pass the literal asset name"), LiteralSetupName, FString(TEXT("EngineOwnedHookLiteralAsset")));

		ULevel* OverrideLevel = World.GetWorld().PersistentLevel;
		int32 DynamicSpawnLevelCalls = 0;
		Engine.GetHooks().GetDynamicSpawnLevel().BindLambda(
			[OverrideLevel, &DynamicSpawnLevelCalls]()
			{
				++DynamicSpawnLevelCalls;
				return OverrideLevel;
			});
		ON_SCOPE_EXIT
		{
			Engine.GetHooks().GetDynamicSpawnLevel().Unbind();
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

			TestRunner->TestNotNull(TEXT("Runtime hook bind-path test should spawn an actor through the bind helper"), SpawnedActor);
			TestRunner->TestEqual(TEXT("SpawnActor bind path should invoke the current engine's dynamic-spawn-level hook"), DynamicSpawnLevelCalls, 1);
			if (SpawnedActor != nullptr)
			{
				TestRunner->TestEqual(TEXT("SpawnActor bind path should use the current engine's dynamic-spawn-level hook"), SpawnedActor->GetLevel(), OverrideLevel);
			}
		}
	}

	TEST_METHOD(DebugServerCallSitesUseCurrentEngineHooks)
	{
		AngelscriptTestSupport::FAngelscriptDebuggerTestSession Session;
		AngelscriptTestSupport::FAngelscriptDebuggerSessionConfig SessionConfig;
		SessionConfig.DefaultTimeoutSeconds = AngelscriptTestSupport::kDefaultDebuggerTestTimeoutSeconds;
		if (!TestRunner->TestTrue(TEXT("Debug hook call-site test should initialize a debugger session"), Session.Initialize(SessionConfig)))
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
		Engine.GetHooks().GetDebugObjectSuffix().BindLambda(
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
			Engine.GetHooks().GetDebugObjectSuffix().Unbind();
		};

		UASClass* ScriptASClass = Cast<UASClass>(ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Debug hook call-site test should find the generated suffix actor class"), ScriptASClass))
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
		if (!TestRunner->TestNotNull(TEXT("Debug hook call-site test should resolve the generated suffix actor script type"), ScriptTypeInfo))
		{
			return;
		}

		UObject* ScriptObject = NewObject<UObject>(GetTransientPackage(), ScriptClass, TEXT("EngineOwnedDebugSuffixObject"));
		if (!TestRunner->TestNotNull(TEXT("Debug hook call-site test should create a generated suffix object"), ScriptObject))
		{
			return;
		}

		const int32 ScriptObjectTypeId = ScriptTypeInfo->GetTypeId();
		UObject* ScriptObjectValue = ScriptObject;
		FString DebugString;
		FToStringHelper::Generic_AppendToString(DebugString, &ScriptObjectValue, ScriptObjectTypeId);
		TestRunner->TestTrue(
			TEXT("ToString debug path should use the current engine's debug-object-suffix hook"),
			DebugString.Contains(TEXT("[engine-owned-debug-suffix]")));
		TestRunner->TestEqual(
			TEXT("ToString debug path should invoke the current engine's debug-object-suffix hook once"),
			DebugSuffixCalls,
			1);

		int32 BreakFilterCalls = 0;
		Engine.GetHooks().GetDebugBreakFilters().BindLambda(
			[&BreakFilterCalls](FAngelscriptDebugBreakFilters& OutFilters)
			{
				++BreakFilterCalls;
				OutFilters.Add(FName(TEXT("break:engine-owned-runtime")), TEXT("Engine Owned Runtime"));
			});
		ON_SCOPE_EXIT
		{
			Engine.GetHooks().GetDebugBreakFilters().Unbind();
		};

		AngelscriptTestSupport::FAngelscriptDebuggerTestClient Client;
		if (!TestRunner->TestTrue(TEXT("Debug hook call-site test should connect a debugger client"), Client.Connect(TEXT("127.0.0.1"), Session.GetPort())))
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

		if (!TestRunner->TestTrue(TEXT("Debug hook call-site test should send StartDebugging"), Client.SendStartDebugging(2)))
		{
			TestRunner->AddError(Client.GetLastError());
			return;
		}

		TOptional<FAngelscriptDebugMessageEnvelope> VersionEnvelope;
		if (!AngelscriptTest_Core_AngelscriptEngineHooksTests_Private::WaitForDebuggerEnvelopeType(
			*TestRunner,
			Session,
			Client,
			EDebugMessageType::DebugServerVersion,
			VersionEnvelope,
			TEXT("Debug hook call-site test should receive the DebugServerVersion response")))
		{
			return;
		}

		if (!TestRunner->TestTrue(TEXT("Debug hook call-site test should request break filters"), Client.SendRequestBreakFilters()))
		{
			TestRunner->AddError(Client.GetLastError());
			return;
		}

		TOptional<FAngelscriptDebugMessageEnvelope> BreakFiltersEnvelope;
		if (!AngelscriptTest_Core_AngelscriptEngineHooksTests_Private::WaitForDebuggerEnvelopeType(
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
			AngelscriptTestSupport::FAngelscriptDebuggerTestClient::DeserializeMessage<FAngelscriptBreakFilters>(BreakFiltersEnvelope.GetValue());
		if (!TestRunner->TestTrue(TEXT("Debug hook call-site test should deserialize the BreakFilters response"), BreakFilters.IsSet()))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("RequestBreakFilters debug-server path should invoke the current engine's hook once"), BreakFilterCalls, 1);
		const int32 FilterIndex = BreakFilters->Filters.IndexOfByKey(FString(TEXT("break:engine-owned-runtime")));
		if (!TestRunner->TestTrue(TEXT("RequestBreakFilters debug-server path should include the engine-owned filter"), FilterIndex != INDEX_NONE))
		{
			return;
		}

		TestRunner->TestTrue(
			TEXT("RequestBreakFilters debug-server path should preserve the engine-owned filter title"),
			BreakFilters->FilterTitles.IsValidIndex(FilterIndex)
			&& BreakFilters->FilterTitles[FilterIndex] == TEXT("Engine Owned Runtime"));

		int32 BreakCheckCalls = 0;
		Engine.GetHooks().GetDebugCheckBreakOptions().BindLambda(
			[&BreakCheckCalls](const FAngelscriptDebugBreakOptions& BreakOptions, UObject* WorldContext)
			{
				++BreakCheckCalls;
				return BreakOptions.Contains(FName(TEXT("break:allow-engine-owned"))) && WorldContext != nullptr;
			});
		ON_SCOPE_EXIT
		{
			Engine.GetHooks().GetDebugCheckBreakOptions().Unbind();
		};

		Engine.AssignWorldContext(NewObject<UAngelscriptNativeScriptTestObject>(GetTransientPackage(), TEXT("EngineOwnedHookDebugWorldContext")));
		ON_SCOPE_EXIT
		{
			Engine.AssignWorldContext(nullptr);
		};

		Session.GetDebugServer().BreakOptions.Reset();
		Session.GetDebugServer().BreakOptions.Add(FName(TEXT("break:allow-engine-owned")));
		TestRunner->TestTrue(TEXT("ShouldBreakOnActiveSide should use the current engine's debug-check hook"), Session.GetDebugServer().ShouldBreakOnActiveSide());
		TestRunner->TestEqual(TEXT("ShouldBreakOnActiveSide should invoke the current engine's debug-check hook once"), BreakCheckCalls, 1);
	}
};

#endif
