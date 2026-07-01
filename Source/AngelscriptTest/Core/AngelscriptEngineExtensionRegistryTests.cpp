#include "AngelscriptEngine.h"
#include "Core/AngelscriptEngineExtensionRegistry.h"
#include "AngelscriptTestUtilities.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptEngineExtensionRegistryTests,
	"Angelscript.TestModule.CppTests.Engine.Extension",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
// Stable per-engine identifier for attach/detach correlation. The hooks
// branch originally used `Engine.GetInstanceId()` here; Phase 7 of
// `refactor-as-engine-clone-removal` deleted that accessor along with
// the Clone-mode infrastructure. Tests only need a value that's
// identical for the same engine instance across attach/detach calls,
// which pointer-as-hex satisfies.
static FString MakeEngineIdentityString(const FAngelscriptEngine& Engine)
{
	return FString::Printf(TEXT("%p"), static_cast<const void*>(&Engine));
}

struct FExtensionRegistryContextGuard
{
	TArray<FAngelscriptEngine*> SavedStack;

	FExtensionRegistryContextGuard()
	{
		SavedStack = FAngelscriptEngineContextStack::SnapshotAndClear();
	}

	~FExtensionRegistryContextGuard()
	{
		FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(SavedStack));
	}

	void DiscardSavedStack()
	{
		SavedStack.Reset();
	}
};

struct FRecordingEngineExtension : IAngelscriptExtension
{
	int32 AttachCount = 0;
	int32 DetachCount = 0;
	TArray<FString> AttachedEngineIds;
	TArray<FString> DetachedEngineIds;

	void OnEngineAttached(FAngelscriptEngine& Engine) override
	{
		++AttachCount;
		AttachedEngineIds.Add(MakeEngineIdentityString(Engine));
	}

	void OnEngineDetached(FAngelscriptEngine& Engine) override
	{
		++DetachCount;
		DetachedEngineIds.Add(MakeEngineIdentityString(Engine));
	}
};

public:
	TEST_METHOD(EmptyRegistryReplayIsANoop)
	{
FExtensionRegistryContextGuard ContextGuard;
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

		TUniquePtr<FAngelscriptEngine> Engine = CreateFullTestEngine();
		ASSERT_THAT(IsNotNull(Engine.Get(), TEXT("Extension registry no-op test should create an isolated full engine")));

		FAngelscriptEngineScope EngineScope(*Engine);
		FRecordingEngineExtension Extension;
		const int32 BaselineExtensionCount = FAngelscriptEngineExtensionRegistry::Get().NumExtensions();

		FAngelscriptEngineExtensionRegistry::Get().ReplayCurrentEngine();

		ASSERT_THAT(AreEqual(0, Extension.AttachCount, TEXT("Extension registry no-op test should not attach anything when the registry is empty")));
		ASSERT_THAT(AreEqual(0, Extension.DetachCount, TEXT("Extension registry no-op test should not detach anything when the registry is empty")));
		ASSERT_THAT(AreEqual(BaselineExtensionCount, FAngelscriptEngineExtensionRegistry::Get().NumExtensions(), TEXT("Extension registry no-op test should not add extensions when replay is requested")));
	}

	TEST_METHOD(LateRegistrationReplaysToCurrentEngine)
	{
FExtensionRegistryContextGuard ContextGuard;
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

		TUniquePtr<FAngelscriptEngine> Engine = CreateFullTestEngine();
		ASSERT_THAT(IsNotNull(Engine.Get(), TEXT("Extension registry late-registration test should create an isolated full engine")));

		FAngelscriptEngineScope EngineScope(*Engine);
		TSharedRef<FRecordingEngineExtension> Extension = MakeShared<FRecordingEngineExtension>();
		const int32 BaselineExtensionCount = FAngelscriptEngineExtensionRegistry::Get().NumExtensions();

		const FDelegateHandle Handle = FAngelscriptEngineExtensionRegistry::Get().RegisterExtension(Extension);
		ASSERT_THAT(IsTrue(Handle.IsValid(), TEXT("Extension registry late-registration test should return a valid handle")));

		ASSERT_THAT(AreEqual(1, Extension->AttachCount, TEXT("Extension registry late-registration test should attach the extension to the active engine immediately")));
		ASSERT_THAT(AreEqual(1, Extension->AttachedEngineIds.Num(), TEXT("Extension registry late-registration test should record the current engine id")));
		ASSERT_THAT(AreEqual(MakeEngineIdentityString(*Engine), Extension->AttachedEngineIds[0], TEXT("Extension registry late-registration test should replay onto the active engine")));

		FAngelscriptEngineExtensionRegistry::Get().UnregisterExtension(Handle);
		ASSERT_THAT(AreEqual(BaselineExtensionCount, FAngelscriptEngineExtensionRegistry::Get().NumExtensions(), TEXT("Extension registry late-registration test should restore the baseline registry count after unregister")));
	}

	TEST_METHOD(UnregisterStopsFutureReplay)
	{
FExtensionRegistryContextGuard ContextGuard;
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
		ASSERT_THAT(IsNotNull(EngineA.Get(), TEXT("Extension registry unregister test should create engine A")));
		ASSERT_THAT(IsNotNull(EngineB.Get(), TEXT("Extension registry unregister test should create engine B")));

		TSharedRef<FRecordingEngineExtension> Extension = MakeShared<FRecordingEngineExtension>();
		FDelegateHandle Handle;

		{
			FAngelscriptEngineScope EngineScope(*EngineA);
			Handle = FAngelscriptEngineExtensionRegistry::Get().RegisterExtension(Extension);
			ASSERT_THAT(IsTrue(Handle.IsValid(), TEXT("Extension registry unregister test should return a valid handle")));
		}

		ASSERT_THAT(AreEqual(1, Extension->AttachCount, TEXT("Extension registry unregister test should attach the extension once before unregister")));

		FAngelscriptEngineExtensionRegistry::Get().UnregisterExtension(Handle);

		{
			FAngelscriptEngineScope EngineScope(*EngineB);
			FAngelscriptEngineExtensionRegistry::Get().ReplayCurrentEngine();
		}

		ASSERT_THAT(AreEqual(1, Extension->AttachCount, TEXT("Extension registry unregister test should not replay unregistered extensions onto a new engine")));
		ASSERT_THAT(AreEqual(0, Extension->DetachCount, TEXT("Extension registry unregister test should not detach more than once when the extension is removed")));
	}

	TEST_METHOD(EngineLifecycleAttachesAndDetachesRegisteredExtension)
	{
FExtensionRegistryContextGuard ContextGuard;
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

		TSharedRef<FRecordingEngineExtension> Extension = MakeShared<FRecordingEngineExtension>();
		const FDelegateHandle Handle = FAngelscriptEngineExtensionRegistry::Get().RegisterExtension(Extension);
		ASSERT_THAT(IsTrue(Handle.IsValid(), TEXT("Extension registry lifecycle test should return a valid handle")));

		FString EngineId;
		{
			TUniquePtr<FAngelscriptEngine> Engine = CreateFullTestEngine();
			ASSERT_THAT(IsNotNull(Engine.Get(), TEXT("Extension registry lifecycle test should create an isolated full engine")));

			EngineId = MakeEngineIdentityString(*Engine);
			ASSERT_THAT(AreEqual(1, Extension->AttachCount, TEXT("Extension registry lifecycle test should attach a pre-registered extension during engine initialization")));
			ASSERT_THAT(AreEqual(1, Extension->AttachedEngineIds.Num(), TEXT("Extension registry lifecycle test should record the initialized engine id")));
			if (Extension->AttachedEngineIds.Num() == 1)
			{
				ASSERT_THAT(AreEqual(EngineId, Extension->AttachedEngineIds[0], TEXT("Extension registry lifecycle test should attach to the initialized engine")));
			}
		}

		ASSERT_THAT(AreEqual(1, Extension->DetachCount, TEXT("Extension registry lifecycle test should detach the extension during engine shutdown")));
		ASSERT_THAT(AreEqual(1, Extension->DetachedEngineIds.Num(), TEXT("Extension registry lifecycle test should record the detached engine id")));
		if (Extension->DetachedEngineIds.Num() == 1)
		{
			ASSERT_THAT(AreEqual(EngineId, Extension->DetachedEngineIds[0], TEXT("Extension registry lifecycle test should detach from the same engine it attached to")));
		}

		FAngelscriptEngineExtensionRegistry::Get().UnregisterExtension(Handle);
	}
};

#endif
