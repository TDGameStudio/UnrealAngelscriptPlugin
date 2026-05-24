#include "CQTest.h"
#include "Shared/AngelscriptDebuggerTestContext.h"

#include "Core/AngelscriptEngine.h"
#include "Core/AngelscriptSettings.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS


namespace
{
	int32 CountMessagesOfType(const TArray<FAngelscriptDebugMessageEnvelope>& Messages, EDebugMessageType MessageType)
	{
		int32 Count = 0;
		for (const FAngelscriptDebugMessageEnvelope& Envelope : Messages)
		{
			if (Envelope.MessageType == MessageType)
			{
				++Count;
			}
		}
		return Count;
	}

	int32 FindFirstMessageIndex(const TArray<FAngelscriptDebugMessageEnvelope>& Messages, EDebugMessageType MessageType)
	{
		for (int32 Index = 0; Index < Messages.Num(); ++Index)
		{
			if (Messages[Index].MessageType == MessageType)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	bool IsAssetDatabaseMessageType(EDebugMessageType MessageType)
	{
		return MessageType == EDebugMessageType::AssetDatabaseInit
			|| MessageType == EDebugMessageType::AssetDatabase
			|| MessageType == EDebugMessageType::AssetDatabaseFinished;
	}

	int32 FindFirstAssetDatabaseMessageIndex(const TArray<FAngelscriptDebugMessageEnvelope>& Messages)
	{
		for (int32 Index = 0; Index < Messages.Num(); ++Index)
		{
			if (IsAssetDatabaseMessageType(Messages[Index].MessageType))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	bool ParseJsonObject(const FString& JsonString, TSharedPtr<FJsonObject>& OutObject)
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
		return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptDebuggerDatabaseTests,
	"Angelscript.TestModule.Debugger.Database",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	FDebuggerTestContext Ctx;

	BEFORE_EACH()
	{
		ASSERT_THAT(IsTrue(Ctx.SetUp(*TestRunner)));
	}

	AFTER_EACH()
	{
		Ctx.TearDown();
	}

	TEST_METHOD(RequestDebugDatabaseSequence)
	{
		TestRunner->TestTrue(TEXT("Debugger database protocol should enter debugging mode after StartDebugging"), Ctx.GetDebugServer().bIsDebugging);

		Ctx.Client.DrainPendingMessages();

		ASSERT_THAT(IsTrue(Ctx.Client.SendRequestDebugDatabase()));

		TArray<FAngelscriptDebugMessageEnvelope> Transcript;
		const bool bCollectedTranscript = Ctx.Session.PumpUntil(
			[this, &Transcript]()
			{
				return Ctx.Client.CollectMessagesUntil(EDebugMessageType::AssetDatabaseFinished, 0.0f, Transcript);
			},
			Ctx.Session.GetDefaultTimeoutSeconds());

		ASSERT_THAT(IsTrue(bCollectedTranscript));

		const int32 SettingsIndex = FindFirstMessageIndex(Transcript, EDebugMessageType::DebugDatabaseSettings);
		TestRunner->TestEqual(TEXT("Debugger database protocol should emit exactly one DebugDatabaseSettings message"), CountMessagesOfType(Transcript, EDebugMessageType::DebugDatabaseSettings), 1);
		TestRunner->TestEqual(TEXT("Debugger database protocol should start the transcript with DebugDatabaseSettings"), SettingsIndex, 0);

		const TOptional<FAngelscriptDebugDatabaseSettings> Settings = SettingsIndex != INDEX_NONE
			? FAngelscriptDebuggerTestClient::DeserializeMessage<FAngelscriptDebugDatabaseSettings>(Transcript[SettingsIndex])
			: TOptional<FAngelscriptDebugDatabaseSettings>();
		ASSERT_THAT(IsTrue(Settings.IsSet()));

		const UAngelscriptSettings* RuntimeSettings = GetDefault<UAngelscriptSettings>();
		ASSERT_THAT(IsNotNull(RuntimeSettings));

		TestRunner->TestEqual(TEXT("Debugger database protocol should mirror the automatic-import setting"), Settings->bAutomaticImports, Ctx.GetEngine().ShouldUseAutomaticImportMethod());
		TestRunner->TestEqual(TEXT("Debugger database protocol should mirror the script float width setting"), Settings->bFloatIsFloat64, RuntimeSettings->bScriptFloatIsFloat64);
		TestRunner->TestEqual(TEXT("Debugger database protocol should mirror the haze integration setting"), Settings->bUseAngelscriptHaze, !!WITH_ANGELSCRIPT_HAZE);
		TestRunner->TestEqual(TEXT("Debugger database protocol should mirror the static class deprecate setting"), Settings->bDeprecateStaticClass, RuntimeSettings->StaticClassDeprecation == EAngelscriptStaticClassMode::Deprecated);
		TestRunner->TestEqual(TEXT("Debugger database protocol should mirror the static class disallow setting"), Settings->bDisallowStaticClass, RuntimeSettings->StaticClassDeprecation == EAngelscriptStaticClassMode::Disallowed);

		const int32 DatabaseIndex = FindFirstMessageIndex(Transcript, EDebugMessageType::DebugDatabase);
		ASSERT_THAT(IsTrue(DatabaseIndex != INDEX_NONE));

		const TOptional<FAngelscriptDebugDatabase> Database = FAngelscriptDebuggerTestClient::DeserializeMessage<FAngelscriptDebugDatabase>(Transcript[DatabaseIndex]);
		ASSERT_THAT(IsTrue(Database.IsSet()));

		TestRunner->TestFalse(TEXT("Debugger database protocol should keep the first DebugDatabase payload non-empty"), Database->Database.IsEmpty());

		TSharedPtr<FJsonObject> DatabaseJsonObject;
		ASSERT_THAT(IsTrue(ParseJsonObject(Database->Database, DatabaseJsonObject)));

		const int32 DebugDatabaseFinishedIndex = FindFirstMessageIndex(Transcript, EDebugMessageType::DebugDatabaseFinished);
		const int32 FirstAssetDatabaseMessageIndex = FindFirstAssetDatabaseMessageIndex(Transcript);
		const int32 AssetDatabaseInitIndex = FindFirstMessageIndex(Transcript, EDebugMessageType::AssetDatabaseInit);
		const int32 AssetDatabaseFinishedIndex = FindFirstMessageIndex(Transcript, EDebugMessageType::AssetDatabaseFinished);

		ASSERT_THAT(IsTrue(DebugDatabaseFinishedIndex != INDEX_NONE));

		ASSERT_THAT(IsTrue(AssetDatabaseInitIndex != INDEX_NONE));

		ASSERT_THAT(IsTrue(AssetDatabaseFinishedIndex != INDEX_NONE));

		TestRunner->TestTrue(TEXT("Debugger database protocol should finish debug database emission before any asset database message"), FirstAssetDatabaseMessageIndex != INDEX_NONE && DebugDatabaseFinishedIndex < FirstAssetDatabaseMessageIndex);
		TestRunner->TestTrue(TEXT("Debugger database protocol should emit AssetDatabaseInit before AssetDatabaseFinished"), AssetDatabaseInitIndex < AssetDatabaseFinishedIndex);

		for (int32 Index = 0; Index < Transcript.Num(); ++Index)
		{
			if (Transcript[Index].MessageType != EDebugMessageType::AssetDatabase)
			{
				continue;
			}

			const TOptional<FAngelscriptAssetDatabase> AssetDatabase = FAngelscriptDebuggerTestClient::DeserializeMessage<FAngelscriptAssetDatabase>(Transcript[Index]);
			ASSERT_THAT(IsTrue(AssetDatabase.IsSet()));

			TestRunner->TestEqual(
				FString::Printf(TEXT("Debugger database protocol should keep AssetDatabase payload %d in path/class pairs"), Index),
				AssetDatabase->Assets.Num() % 2,
				0);
		}
	}
};

#endif
