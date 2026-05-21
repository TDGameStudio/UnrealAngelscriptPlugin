#include "AngelscriptEngine.h"
#include "Core/AngelscriptEngineExtensionRegistry.h"
#include "Shared/AngelscriptTestUtilities.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_Core_AngelscriptEngineExtensionRegistryTests_Private
{
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
}

TEST_CLASS_WITH_FLAGS(FAngelscriptEngineExtensionRegistryTests,
	"Angelscript.TestModule.CppTests.Engine.Extension",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(EmptyRegistryReplayIsANoop)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineExtensionRegistryTests_Private;
		FExtensionRegistryContextGuard ContextGuard;
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

		TUniquePtr<FAngelscriptEngine> Engine = AngelscriptTestSupport::CreateFullTestEngine();
		if (!TestRunner->TestNotNull(TEXT("Extension registry no-op test should create an isolated full engine"), Engine.Get()))
		{
			return;
		}

		FAngelscriptEngineScope EngineScope(*Engine);
		FRecordingEngineExtension Extension;

		FAngelscriptEngineExtensionRegistry::Get().ReplayCurrentEngine();

		TestRunner->TestEqual(
			TEXT("Extension registry no-op test should not attach anything when the registry is empty"),
			Extension.AttachCount,
			0);
		TestRunner->TestEqual(
			TEXT("Extension registry no-op test should not detach anything when the registry is empty"),
			Extension.DetachCount,
			0);
		TestRunner->TestEqual(
			TEXT("Extension registry no-op test should stay empty when the registry is unused"),
			FAngelscriptEngineExtensionRegistry::Get().NumExtensions(),
			0);
	}

	TEST_METHOD(LateRegistrationReplaysToCurrentEngine)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineExtensionRegistryTests_Private;
		FExtensionRegistryContextGuard ContextGuard;
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

		TUniquePtr<FAngelscriptEngine> Engine = AngelscriptTestSupport::CreateFullTestEngine();
		if (!TestRunner->TestNotNull(TEXT("Extension registry late-registration test should create an isolated full engine"), Engine.Get()))
		{
			return;
		}

		FAngelscriptEngineScope EngineScope(*Engine);
		TSharedRef<FRecordingEngineExtension> Extension = MakeShared<FRecordingEngineExtension>();

		const FDelegateHandle Handle = FAngelscriptEngineExtensionRegistry::Get().RegisterExtension(Extension);
		if (!TestRunner->TestTrue(TEXT("Extension registry late-registration test should return a valid handle"), Handle.IsValid()))
		{
			return;
		}

		TestRunner->TestEqual(
			TEXT("Extension registry late-registration test should attach the extension to the active engine immediately"),
			Extension->AttachCount,
			1);
		TestRunner->TestEqual(
			TEXT("Extension registry late-registration test should record the current engine id"),
			Extension->AttachedEngineIds.Num(),
			1);
		TestRunner->TestEqual(
			TEXT("Extension registry late-registration test should replay onto the active engine"),
			Extension->AttachedEngineIds[0],
			MakeEngineIdentityString(*Engine));

		FAngelscriptEngineExtensionRegistry::Get().UnregisterExtension(Handle);
		TestRunner->TestEqual(
			TEXT("Extension registry late-registration test should keep the registry empty after unregister"),
			FAngelscriptEngineExtensionRegistry::Get().NumExtensions(),
			0);
	}

	TEST_METHOD(UnregisterStopsFutureReplay)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineExtensionRegistryTests_Private;
		FExtensionRegistryContextGuard ContextGuard;
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
		if (!TestRunner->TestNotNull(TEXT("Extension registry unregister test should create engine A"), EngineA.Get())
			|| !TestRunner->TestNotNull(TEXT("Extension registry unregister test should create engine B"), EngineB.Get()))
		{
			return;
		}

		TSharedRef<FRecordingEngineExtension> Extension = MakeShared<FRecordingEngineExtension>();
		FDelegateHandle Handle;

		{
			FAngelscriptEngineScope EngineScope(*EngineA);
			Handle = FAngelscriptEngineExtensionRegistry::Get().RegisterExtension(Extension);
			if (!TestRunner->TestTrue(TEXT("Extension registry unregister test should return a valid handle"), Handle.IsValid()))
			{
				return;
			}
		}

		TestRunner->TestEqual(
			TEXT("Extension registry unregister test should attach the extension once before unregister"),
			Extension->AttachCount,
			1);

		FAngelscriptEngineExtensionRegistry::Get().UnregisterExtension(Handle);

		{
			FAngelscriptEngineScope EngineScope(*EngineB);
			FAngelscriptEngineExtensionRegistry::Get().ReplayCurrentEngine();
		}

		TestRunner->TestEqual(
			TEXT("Extension registry unregister test should not replay unregistered extensions onto a new engine"),
			Extension->AttachCount,
			1);
		TestRunner->TestEqual(
			TEXT("Extension registry unregister test should not detach more than once when the extension is removed"),
			Extension->DetachCount,
			0);
	}

	TEST_METHOD(EngineLifecycleAttachesAndDetachesRegisteredExtension)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineExtensionRegistryTests_Private;
		FExtensionRegistryContextGuard ContextGuard;
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

		TSharedRef<FRecordingEngineExtension> Extension = MakeShared<FRecordingEngineExtension>();
		const FDelegateHandle Handle = FAngelscriptEngineExtensionRegistry::Get().RegisterExtension(Extension);
		if (!TestRunner->TestTrue(TEXT("Extension registry lifecycle test should return a valid handle"), Handle.IsValid()))
		{
			return;
		}

		FString EngineId;
		{
			TUniquePtr<FAngelscriptEngine> Engine = AngelscriptTestSupport::CreateFullTestEngine();
			if (!TestRunner->TestNotNull(TEXT("Extension registry lifecycle test should create an isolated full engine"), Engine.Get()))
			{
				return;
			}

			EngineId = MakeEngineIdentityString(*Engine);
			TestRunner->TestEqual(
				TEXT("Extension registry lifecycle test should attach a pre-registered extension during engine initialization"),
				Extension->AttachCount,
				1);
			TestRunner->TestEqual(
				TEXT("Extension registry lifecycle test should record the initialized engine id"),
				Extension->AttachedEngineIds.Num(),
				1);
			if (Extension->AttachedEngineIds.Num() == 1)
			{
				TestRunner->TestEqual(
					TEXT("Extension registry lifecycle test should attach to the initialized engine"),
					Extension->AttachedEngineIds[0],
					EngineId);
			}
		}

		TestRunner->TestEqual(
			TEXT("Extension registry lifecycle test should detach the extension during engine shutdown"),
			Extension->DetachCount,
			1);
		TestRunner->TestEqual(
			TEXT("Extension registry lifecycle test should record the detached engine id"),
			Extension->DetachedEngineIds.Num(),
			1);
		if (Extension->DetachedEngineIds.Num() == 1)
		{
			TestRunner->TestEqual(
				TEXT("Extension registry lifecycle test should detach from the same engine it attached to"),
				Extension->DetachedEngineIds[0],
				EngineId);
		}

		FAngelscriptEngineExtensionRegistry::Get().UnregisterExtension(Handle);
	}
};

#endif
