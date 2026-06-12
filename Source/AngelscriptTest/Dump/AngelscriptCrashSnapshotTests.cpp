#include "Dump/AngelscriptCrashSnapshot.h"

#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCrashSnapshotWriteTest,
	"Angelscript.TestModule.Dump.CrashSnapshot.Write",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCrashSnapshotTestCommandRegisteredTest,
	"Angelscript.TestModule.Dump.CrashSnapshot.TestCommandRegistered",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

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

bool FAngelscriptCrashSnapshotWriteTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Dump_CrashSnapshotTests_Private;

	const FString OutputDir = MakeUniqueCrashSnapshotPath(TEXT("Write"));
	const FString Marker = TEXT("snapshot-write-test-marker");

	const FAngelscriptCrashSnapshot::FWriteResult Result =
		FAngelscriptCrashSnapshot::WriteSnapshotForTesting(OutputDir, Marker);

	if (!TestTrue(TEXT("Crash snapshot writer should succeed"), Result.bSuccess))
	{
		AddError(Result.ErrorMessage);
		return false;
	}

	if (!TestTrue(TEXT("Crash snapshot writer should create a snapshot file"), IFileManager::Get().FileExists(*Result.SnapshotPath)))
	{
		AddError(Result.SnapshotPath);
		return false;
	}

	TSharedPtr<FJsonObject> SnapshotObject;
	if (!LoadSnapshotJson(*this, Result.SnapshotPath, SnapshotObject))
	{
		return false;
	}

	TestEqual(TEXT("Crash snapshot should store a schema version"), static_cast<int32>(SnapshotObject->GetIntegerField(TEXT("schemaVersion"))), 1);
	TestEqual(TEXT("Crash snapshot should store the test marker"), SnapshotObject->GetStringField(TEXT("marker")), Marker);
	TestTrue(TEXT("Crash snapshot should store the process id"), SnapshotObject->HasTypedField<EJson::Number>(TEXT("processId")));
	TestTrue(TEXT("Crash snapshot should store the current thread id"), SnapshotObject->HasTypedField<EJson::Number>(TEXT("threadId")));
	TestTrue(TEXT("Crash snapshot should store engine initialization state"), SnapshotObject->HasTypedField<EJson::Boolean>(TEXT("engineInitialized")));
	TestTrue(TEXT("Crash snapshot should store active module count"), SnapshotObject->HasTypedField<EJson::Number>(TEXT("activeModuleCount")));
	TestTrue(TEXT("Crash snapshot should store a modules array"), SnapshotObject->HasTypedField<EJson::Array>(TEXT("modules")));

	return true;
}

bool FAngelscriptCrashSnapshotTestCommandRegisteredTest::RunTest(const FString& Parameters)
{
	IConsoleObject* Command = IConsoleManager::Get().FindConsoleObject(TEXT("as.Test.ConfigureCrashSnapshot"));
	return TestNotNull(TEXT("Crash snapshot test configuration command should be registered"), Command);
}

#endif
