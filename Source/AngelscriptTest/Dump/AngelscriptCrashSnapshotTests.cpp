#include "Dump/AngelscriptCrashSnapshot.h"

#include "CQTest.h"

#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_Dump_CrashSnapshotTests_Private
{
	FString MakeUniqueCrashSnapshotPath(const FString& Prefix)
	{
		return FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation"),
			TEXT("CrashSnapshot"),
			FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	}

	bool LoadSnapshotJson(FAutomationTestBase& Test, const FString& SnapshotPath, TSharedPtr<FJsonObject>& OutObject)
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
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCrashSnapshotTest,
	"Angelscript.TestModule.Dump.CrashSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(Write)
	{
		using namespace AngelscriptTest_Dump_CrashSnapshotTests_Private;

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
};

#endif
