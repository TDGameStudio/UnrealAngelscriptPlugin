#include "CQTest.h"
#include "AngelscriptDebuggerTestContext.h"

#include "Core/AngelscriptEngine.h"
#include "Core/AngelscriptSettings.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_ANGELSCRIPT_UNITTESTS


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

	struct FHazelightLspDebugDatabaseSettings
	{
		int32 Version = 0;
		bool bAutomaticImports = false;
		bool bFloatIsFloat64 = false;
		bool bLegacyCompatibilitySlot = false;
		bool bDeprecateStaticClass = false;
		bool bDisallowStaticClass = false;
		bool bGlobalFunctionCompatibilitySlot = false;
		bool bDeprecateActorGenerics = false;
		bool bDisallowActorGenerics = false;

		FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FHazelightLspDebugDatabaseSettings& Msg)
		{
			Ar << Msg.Version;
			Ar << Msg.bAutomaticImports;
			Ar << Msg.bFloatIsFloat64;
			Ar << Msg.bLegacyCompatibilitySlot;
			Ar << Msg.bDeprecateStaticClass;
			Ar << Msg.bDisallowStaticClass;
			Ar << Msg.bGlobalFunctionCompatibilitySlot;
			Ar << Msg.bDeprecateActorGenerics;
			Ar << Msg.bDisallowActorGenerics;
			return Ar;
		}
	};
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
		ASSERT_THAT(IsTrue(Ctx.GetDebugServer().bIsDebugging, TEXT("Debugger database protocol should enter debugging mode after StartDebugging")));

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
		ASSERT_THAT(AreEqual(1, CountMessagesOfType(Transcript, EDebugMessageType::DebugDatabaseSettings), TEXT("Debugger database protocol should emit exactly one DebugDatabaseSettings message")));
		ASSERT_THAT(AreEqual(0, SettingsIndex, TEXT("Debugger database protocol should start the transcript with DebugDatabaseSettings")));

		const TOptional<FAngelscriptDebugDatabaseSettings> Settings = SettingsIndex != INDEX_NONE
			? FAngelscriptDebuggerTestClient::DeserializeMessage<FAngelscriptDebugDatabaseSettings>(Transcript[SettingsIndex])
			: TOptional<FAngelscriptDebugDatabaseSettings>();
		ASSERT_THAT(IsTrue(Settings.IsSet()));

		const UAngelscriptSettings* RuntimeSettings = GetDefault<UAngelscriptSettings>();
		ASSERT_THAT(IsNotNull(RuntimeSettings));

		ASSERT_THAT(AreEqual(Ctx.GetEngine().ShouldUseAutomaticImportMethod(), Settings->bAutomaticImports, TEXT("Debugger database protocol should mirror the automatic-import setting")));
		ASSERT_THAT(AreEqual(RuntimeSettings->bScriptFloatIsFloat64, Settings->bFloatIsFloat64, TEXT("Debugger database protocol should mirror the script float width setting")));
		ASSERT_THAT(AreEqual(RuntimeSettings->StaticClassDeprecation == EAngelscriptStaticClassMode::Deprecated, Settings->bDeprecateStaticClass, TEXT("Debugger database protocol should mirror the static class deprecate setting")));
		ASSERT_THAT(AreEqual(RuntimeSettings->StaticClassDeprecation == EAngelscriptStaticClassMode::Disallowed, Settings->bDisallowStaticClass, TEXT("Debugger database protocol should mirror the static class disallow setting")));

		TArray<uint8> SettingsBody;
		FMemoryWriter SettingsWriter(SettingsBody);
		FAngelscriptDebugDatabaseSettings SettingsCopy = Settings.GetValue();
		SettingsWriter << SettingsCopy;

		FHazelightLspDebugDatabaseSettings HazelightSettings;
		FMemoryReader SettingsReader(SettingsBody);
		SettingsReader << HazelightSettings;
		ASSERT_THAT(IsFalse(SettingsReader.IsError(), TEXT("Debugger database settings should be readable by the Hazelight Language Server field order")));
		ASSERT_THAT(AreEqual(7, HazelightSettings.Version, TEXT("Debugger database settings should use Hazelight wire version 7")));
		ASSERT_THAT(IsFalse(HazelightSettings.bLegacyCompatibilitySlot, TEXT("Debugger database settings should keep the legacy compatibility slot disabled")));
		ASSERT_THAT(IsFalse(HazelightSettings.bGlobalFunctionCompatibilitySlot, TEXT("Debugger database settings should keep the global-function compatibility slot disabled")));
		ASSERT_THAT(IsFalse(HazelightSettings.bDeprecateActorGenerics, TEXT("Debugger database settings should keep the actor-generic compatibility slot disabled")));
		ASSERT_THAT(IsFalse(HazelightSettings.bDisallowActorGenerics, TEXT("Debugger database settings should keep the actor-generic compatibility slot disabled")));
		ASSERT_THAT(IsFalse(Settings->bReservedLegacySlot, TEXT("Debugger database settings should keep the legacy compatibility slot disabled in the runtime DTO")));
		ASSERT_THAT(IsFalse(Settings->bReservedGlobalFunctionSlot, TEXT("Debugger database settings should keep the global-function compatibility slot disabled in the runtime DTO")));
		ASSERT_THAT(IsFalse(Settings->bReservedActorGenericDeprecationSlot, TEXT("Debugger database settings should keep the actor-generic deprecation slot disabled in the runtime DTO")));
		ASSERT_THAT(IsFalse(Settings->bReservedActorGenericDisallowSlot, TEXT("Debugger database settings should keep the actor-generic disallowance slot disabled in the runtime DTO")));

		const int32 DatabaseIndex = FindFirstMessageIndex(Transcript, EDebugMessageType::DebugDatabase);
		ASSERT_THAT(IsTrue(DatabaseIndex != INDEX_NONE));

		const TOptional<FAngelscriptDebugDatabase> Database = FAngelscriptDebuggerTestClient::DeserializeMessage<FAngelscriptDebugDatabase>(Transcript[DatabaseIndex]);
		ASSERT_THAT(IsTrue(Database.IsSet()));

		ASSERT_THAT(IsFalse(Database->Database.IsEmpty(), TEXT("Debugger database protocol should keep the first DebugDatabase payload non-empty")));

		TSharedPtr<FJsonObject> DatabaseJsonObject;
		ASSERT_THAT(IsTrue(ParseJsonObject(Database->Database, DatabaseJsonObject)));

		const int32 DebugDatabaseFinishedIndex = FindFirstMessageIndex(Transcript, EDebugMessageType::DebugDatabaseFinished);
		const int32 FirstAssetDatabaseMessageIndex = FindFirstAssetDatabaseMessageIndex(Transcript);
		const int32 AssetDatabaseInitIndex = FindFirstMessageIndex(Transcript, EDebugMessageType::AssetDatabaseInit);
		const int32 AssetDatabaseFinishedIndex = FindFirstMessageIndex(Transcript, EDebugMessageType::AssetDatabaseFinished);

		ASSERT_THAT(IsTrue(DebugDatabaseFinishedIndex != INDEX_NONE));

		ASSERT_THAT(IsTrue(AssetDatabaseInitIndex != INDEX_NONE));

		ASSERT_THAT(IsTrue(AssetDatabaseFinishedIndex != INDEX_NONE));

		ASSERT_THAT(IsTrue(
			FirstAssetDatabaseMessageIndex != INDEX_NONE && DebugDatabaseFinishedIndex < FirstAssetDatabaseMessageIndex,
			TEXT("Debugger database protocol should finish debug database emission before any asset database message")));
		ASSERT_THAT(IsTrue(
			AssetDatabaseInitIndex < AssetDatabaseFinishedIndex,
			TEXT("Debugger database protocol should emit AssetDatabaseInit before AssetDatabaseFinished")));

		for (int32 Index = 0; Index < Transcript.Num(); ++Index)
		{
			if (Transcript[Index].MessageType != EDebugMessageType::AssetDatabase)
			{
				continue;
			}

			const TOptional<FAngelscriptAssetDatabase> AssetDatabase = FAngelscriptDebuggerTestClient::DeserializeMessage<FAngelscriptAssetDatabase>(Transcript[Index]);
			ASSERT_THAT(IsTrue(AssetDatabase.IsSet()));

			ASSERT_THAT(AreEqual(
				0,
				AssetDatabase->Assets.Num() % 2,
				FString::Printf(TEXT("Debugger database protocol should keep AssetDatabase payload %d in path/class pairs"), Index)));
		}
	}
};

#endif
