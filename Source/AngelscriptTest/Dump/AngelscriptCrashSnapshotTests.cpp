#include "Dump/AngelscriptCrashSnapshot.h"

#include "AngelscriptEngine.h"
#include "AngelscriptTestUtilities.h"
#include "CQTest.h"

#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCrashSnapshotTest,
	"Angelscript.TestModule.Dump.CrashSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
static FString MakeUniqueCrashSnapshotPath(const FString& Prefix)
{
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("Automation"),
		TEXT("CrashSnapshot"),
		FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
}

static bool LoadSnapshotJson(FAutomationTestBase& Test, const FString& SnapshotPath, TSharedPtr<FJsonObject>& OutObject)
{
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *SnapshotPath))
	{
		Test.AddError(FString::Printf(TEXT("Failed to load crash snapshot '%s'"), *SnapshotPath));
		return false;
	}

	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, OutObject) || !OutObject.IsValid())
	{
		Test.AddError(FString::Printf(TEXT("Failed to parse crash snapshot JSON '%s'"), *SnapshotPath));
		return false;
	}

	return true;
}

struct FCrashSnapshotContextGuard
{
	TArray<FAngelscriptEngine*> SavedStack;

	FCrashSnapshotContextGuard()
	{
		SavedStack = FAngelscriptEngineContextStack::SnapshotAndClear();
	}

	~FCrashSnapshotContextGuard()
	{
		FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(SavedStack));
	}

	void DiscardSavedStack()
	{
		SavedStack.Reset();
	}
};

public:
	TEST_METHOD(Write)
	{
const FString OutputDir = MakeUniqueCrashSnapshotPath(TEXT("Write"));
		const FString Marker = TEXT("snapshot-write-test-marker");

		const FAngelscriptCrashSnapshot::FWriteResult Result =
			FAngelscriptCrashSnapshot::WriteSnapshotForTesting(OutputDir, Marker);

		if (!Result.bSuccess)
		{
			TestRunner->AddError(Result.ErrorMessage);
		}
		ASSERT_THAT(IsTrue(Result.bSuccess, TEXT("Crash snapshot writer should succeed")));

		if (!IFileManager::Get().FileExists(*Result.SnapshotPath))
		{
			TestRunner->AddError(Result.SnapshotPath);
		}
		ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(*Result.SnapshotPath), TEXT("Crash snapshot writer should create a snapshot file")));

		TSharedPtr<FJsonObject> SnapshotObject;
		ASSERT_THAT(IsTrue(LoadSnapshotJson(*TestRunner, Result.SnapshotPath, SnapshotObject), TEXT("Crash snapshot JSON should load")));

		ASSERT_THAT(AreEqual(1, static_cast<int32>(SnapshotObject->GetIntegerField(TEXT("schemaVersion"))), TEXT("Crash snapshot should store a schema version")));
		ASSERT_THAT(AreEqual(Marker, SnapshotObject->GetStringField(TEXT("marker")), TEXT("Crash snapshot should store the test marker")));
		ASSERT_THAT(IsTrue(SnapshotObject->HasTypedField<EJson::Number>(TEXT("processId")), TEXT("Crash snapshot should store the process id")));
		ASSERT_THAT(IsTrue(SnapshotObject->HasTypedField<EJson::Number>(TEXT("threadId")), TEXT("Crash snapshot should store the current thread id")));
		ASSERT_THAT(IsTrue(SnapshotObject->HasTypedField<EJson::Boolean>(TEXT("engineInitialized")), TEXT("Crash snapshot should store engine initialization state")));
		ASSERT_THAT(IsTrue(SnapshotObject->HasTypedField<EJson::Number>(TEXT("activeModuleCount")), TEXT("Crash snapshot should store active module count")));
		ASSERT_THAT(IsTrue(SnapshotObject->HasTypedField<EJson::Array>(TEXT("modules")), TEXT("Crash snapshot should store a modules array")));
	}

	TEST_METHOD(TestCommandRegistered)
	{
		IConsoleObject* Command = IConsoleManager::Get().FindConsoleObject(TEXT("as.Test.ConfigureCrashSnapshot"));
		ASSERT_THAT(IsNotNull(Command, TEXT("Crash snapshot test configuration command should be registered")));
	}

	TEST_METHOD(SequentialEngineLifecycleRegistersAndUnregistersHandler)
	{
		FCrashSnapshotContextGuard ContextGuard;
		DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized())
		{
			FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
		}
		ContextGuard.DiscardSavedStack();

		const int32 BaselineCount = FAngelscriptCrashSnapshotExtension::GetActiveEngineCountForTesting();
		const bool bBaselineRegistered = FAngelscriptCrashSnapshot::IsHandlerRegisteredForTesting();

		ON_SCOPE_EXIT
		{
			FAngelscriptEngineContextStack::SnapshotAndClear();
			if (FAngelscriptEngine::IsInitialized())
			{
				FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
			}
			DestroySharedTestEngine();
		};

		{
			TUniquePtr<FAngelscriptEngine> Engine = CreateFullTestEngine();
			ASSERT_THAT(IsNotNull(Engine.Get(), TEXT("Crash snapshot lifecycle should create the first isolated engine")));
			ASSERT_THAT(AreEqual(BaselineCount + 1, FAngelscriptCrashSnapshotExtension::GetActiveEngineCountForTesting(), TEXT("Crash snapshot extension should count the first attached engine")));
			ASSERT_THAT(IsTrue(FAngelscriptCrashSnapshot::IsHandlerRegisteredForTesting(), TEXT("Crash snapshot handler should register while an engine is active")));
		}

		ASSERT_THAT(AreEqual(BaselineCount, FAngelscriptCrashSnapshotExtension::GetActiveEngineCountForTesting(), TEXT("Crash snapshot extension should restore active count after the first engine is destroyed")));
		ASSERT_THAT(AreEqual(bBaselineRegistered, FAngelscriptCrashSnapshot::IsHandlerRegisteredForTesting(), TEXT("Crash snapshot handler registration should return to baseline after the last engine is destroyed")));

		{
			TUniquePtr<FAngelscriptEngine> Engine = CreateFullTestEngine();
			ASSERT_THAT(IsNotNull(Engine.Get(), TEXT("Crash snapshot lifecycle should create the second isolated engine")));
			ASSERT_THAT(AreEqual(BaselineCount + 1, FAngelscriptCrashSnapshotExtension::GetActiveEngineCountForTesting(), TEXT("Crash snapshot extension should count the second attached engine")));
			ASSERT_THAT(IsTrue(FAngelscriptCrashSnapshot::IsHandlerRegisteredForTesting(), TEXT("Crash snapshot handler should register again for the second engine")));
		}

		ASSERT_THAT(AreEqual(BaselineCount, FAngelscriptCrashSnapshotExtension::GetActiveEngineCountForTesting(), TEXT("Crash snapshot extension should restore active count after the second engine is destroyed")));
		ASSERT_THAT(AreEqual(bBaselineRegistered, FAngelscriptCrashSnapshot::IsHandlerRegisteredForTesting(), TEXT("Crash snapshot handler registration should return to baseline after sequential lifetimes")));
	}

	TEST_METHOD(OverlappingEngineLifecycleKeepsHandlerUntilLastDetach)
	{
		FCrashSnapshotContextGuard ContextGuard;
		DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized())
		{
			FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
		}
		ContextGuard.DiscardSavedStack();

		const int32 BaselineCount = FAngelscriptCrashSnapshotExtension::GetActiveEngineCountForTesting();
		const bool bBaselineRegistered = FAngelscriptCrashSnapshot::IsHandlerRegisteredForTesting();

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
		ASSERT_THAT(IsNotNull(EngineA.Get(), TEXT("Crash snapshot overlap test should create engine A")));
		ASSERT_THAT(AreEqual(BaselineCount + 1, FAngelscriptCrashSnapshotExtension::GetActiveEngineCountForTesting(), TEXT("Crash snapshot extension should count engine A")));
		ASSERT_THAT(IsTrue(FAngelscriptCrashSnapshot::IsHandlerRegisteredForTesting(), TEXT("Crash snapshot handler should register after engine A attaches")));

		TUniquePtr<FAngelscriptEngine> EngineB = CreateFullTestEngine();
		ASSERT_THAT(IsNotNull(EngineB.Get(), TEXT("Crash snapshot overlap test should create engine B")));
		ASSERT_THAT(AreEqual(BaselineCount + 2, FAngelscriptCrashSnapshotExtension::GetActiveEngineCountForTesting(), TEXT("Crash snapshot extension should count overlapping engine lifetimes")));
		ASSERT_THAT(IsTrue(FAngelscriptCrashSnapshot::IsHandlerRegisteredForTesting(), TEXT("Crash snapshot handler should stay registered while two engines are active")));

		EngineA.Reset();
		ASSERT_THAT(AreEqual(BaselineCount + 1, FAngelscriptCrashSnapshotExtension::GetActiveEngineCountForTesting(), TEXT("Crash snapshot extension should decrement when a non-final engine detaches")));
		ASSERT_THAT(IsTrue(FAngelscriptCrashSnapshot::IsHandlerRegisteredForTesting(), TEXT("Crash snapshot handler should stay registered until the last engine detaches")));

		EngineB.Reset();
		ASSERT_THAT(AreEqual(BaselineCount, FAngelscriptCrashSnapshotExtension::GetActiveEngineCountForTesting(), TEXT("Crash snapshot extension should restore active count after all engines detach")));
		ASSERT_THAT(AreEqual(bBaselineRegistered, FAngelscriptCrashSnapshot::IsHandlerRegisteredForTesting(), TEXT("Crash snapshot handler registration should return to baseline after overlapping lifetimes")));
	}
};

#endif
