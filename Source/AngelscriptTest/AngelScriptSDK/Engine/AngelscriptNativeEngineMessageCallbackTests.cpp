#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FEngineMessageCallbackTests,
	"Angelscript.TestModule.AngelScriptSDK.Engine.MessageCallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FMessageCase
	{
		const ANSICHAR* Section;
		int32 Row;
		int32 Column;
		asEMsgType Type;
		const ANSICHAR* Text;
		const TCHAR* Description;
	};

	static FString MakeReviewSource(const TCHAR* FunctionName)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("int %s()"), FunctionName));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\t// Message callback cases are sent through the raw engine API."));
		AppendGeneratedAsLine(Source, TEXT("\treturn 1;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static FString MakeCartesianSource(
		const FString& CaseToken,
		const ANSICHAR* MessageText)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int MessageCase()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			FString::Printf(TEXT("\t// case: %s"), *CaseToken));
		AppendGeneratedAsLine(
			Source,
			FString::Printf(TEXT("\t// callback payload: %s"), UTF8_TO_TCHAR(MessageText)));
		AppendGeneratedAsLine(Source, TEXT("\treturn 1;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static bool HasMessage(
		const AngelscriptNativeTestSupport::FNativeMessageEntry& Entry,
		const FMessageCase& Expected)
	{
		return Entry.Section == UTF8_TO_TCHAR(Expected.Section)
			&& Entry.Row == Expected.Row
			&& Entry.Column == Expected.Column
			&& Entry.Type == Expected.Type
			&& Entry.Message == UTF8_TO_TCHAR(Expected.Text);
	}

public:
	TEST_METHOD(MessagesBySeverityLocationAndText)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		AS_NATIVE_PRODUCT("ENG-MESSAGE-CALLBACK-FAMILIES",
			ENativeEvidence::Diagnostic
			| ENativeEvidence::Metadata
			| ENativeEvidence::Lifecycle);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("message callback family owner should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const FString GeneratedSource = MakeReviewSource(TEXT("MessageCallbackFamilies"));
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("ENG-MESSAGE-CALLBACK-FAMILIES"),
			TEXT("AS_SDK_EngineMessageCallback_Families"),
			GeneratedSource);

		const FMessageCase Cases[] =
		{
			{ "InfoSection", 1, 2, asMSGTYPE_INFORMATION, "information payload", TEXT("information message") },
			{ "WarningSection", 7, 11, asMSGTYPE_WARNING, "warning payload", TEXT("warning message") },
			{ "ErrorSection", 13, 17, asMSGTYPE_ERROR, "error payload", TEXT("error message") },
			{ "OffsetSection", 0, 0, asMSGTYPE_INFORMATION, "offset payload", TEXT("zero-offset message") },
		};

		for (const FMessageCase& Case : Cases)
		{
			Engine.ResetMessages();
			const int WriteResult = ScriptEngine->WriteMessage(
				Case.Section,
				Case.Row,
				Case.Column,
				Case.Type,
				Case.Text);
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asSUCCESS),
				WriteResult,
				FString::Printf(TEXT("%s should accept the raw message write"), Case.Description)));
			ASSERT_THAT(AreEqual(
				1,
				Engine.GetMessages().Entries.Num(),
				FString::Printf(TEXT("%s should produce exactly one callback record"), Case.Description)));
			if (Engine.GetMessages().Entries.Num() == 1)
			{
				ASSERT_THAT(IsTrue(
					HasMessage(Engine.GetMessages().Entries[0], Case),
					FString::Printf(TEXT("%s should preserve severity, section, row, column, and text"), Case.Description)));
			}
		}
	}

	TEST_METHOD(MessagesBySeverityLocationAndPayloadCartesianProduct)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		AS_NATIVE_PRODUCT("ENG-MESSAGE-CALLBACK-CARTESIAN",
			ENativeEvidence::Diagnostic
			| ENativeEvidence::Metadata
			| ENativeEvidence::Lifecycle);

		struct FSeverityCase
		{
			const TCHAR* Id;
			asEMsgType Type;
		};

		struct FLocationCase
		{
			const TCHAR* Id;
			const ANSICHAR* Section;
			int32 Row;
			int32 Column;
		};

		struct FPayloadCase
		{
			const TCHAR* Id;
			const ANSICHAR* Text;
		};

		const FSeverityCase Severities[] =
		{
			{ TEXT("information"), asMSGTYPE_INFORMATION },
			{ TEXT("warning"), asMSGTYPE_WARNING },
			{ TEXT("error"), asMSGTYPE_ERROR },
		};
		const FLocationCase Locations[] =
		{
			{ TEXT("zero_offset"), "ZeroOffset", 0, 0 },
			{ TEXT("first_line"), "FirstLine", 1, 1 },
			{ TEXT("middle_line"), "MiddleLine", 17, 23 },
			{ TEXT("large_offset"), "LargeOffset", 1024, 4096 },
		};
		const FPayloadCase Payloads[] =
		{
			{ TEXT("plain"), "plain payload" },
			{ TEXT("empty"), "" },
			{ TEXT("punctuation"), "payload: [] {} -> !?" },
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("message callback Cartesian owner should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		int32 ObservedCaseCount = 0;
		for (const FSeverityCase& Severity : Severities)
		{
			for (const FLocationCase& Location : Locations)
			{
				for (const FPayloadCase& Payload : Payloads)
				{
					const FString CaseId = MakeNativeCaseId(
						"ENG-MESSAGE-CALLBACK-CARTESIAN",
						{ Severity.Id, Location.Id, Payload.Id });
					const FString ModuleName = FString::Printf(
						TEXT("AS_SDK_EngineMessageCallback_%s_%s_%s"),
						Severity.Id,
						Location.Id,
						Payload.Id);
					const FString Source = MakeCartesianSource(CaseId, Payload.Text);
					PrintGeneratedAsSource(*TestRunner, CaseId, ModuleName, Source);

					Engine.ResetMessages();
					const int WriteResult = ScriptEngine->WriteMessage(
						Location.Section,
						Location.Row,
						Location.Column,
						Severity.Type,
						Payload.Text);
					ASSERT_THAT(AreEqual(
						static_cast<int32>(asSUCCESS),
						WriteResult,
						*FString::Printf(TEXT("%s should accept its raw callback payload"), *CaseId)));
					ASSERT_THAT(AreEqual(
						1,
						Engine.GetMessages().Entries.Num(),
						*FString::Printf(TEXT("%s should deliver exactly one callback record"), *CaseId)));
					if (Engine.GetMessages().Entries.Num() == 1)
					{
						const FMessageCase Expected =
							{ Location.Section, Location.Row, Location.Column, Severity.Type, Payload.Text, *CaseId };
						ASSERT_THAT(IsTrue(
							HasMessage(Engine.GetMessages().Entries[0], Expected),
							*FString::Printf(TEXT("%s should preserve severity, coordinates, section, and payload"), *CaseId)));
					}

					++ObservedCaseCount;
				}
			}
		}

		ASSERT_THAT(AreEqual(
			36,
			ObservedCaseCount,
			TEXT("Severity × location × payload should execute every callback cell")));
	}

	TEST_METHOD(CallbackReplacementClearAndRestore)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		AS_NATIVE_PRODUCT("ENG-MESSAGE-CALLBACK-LIFECYCLE",
			ENativeEvidence::Diagnostic
			| ENativeEvidence::Metadata
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Isolation);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("message callback lifecycle owner should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const FString GeneratedSource = MakeReviewSource(TEXT("MessageCallbackLifecycle"));
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("ENG-MESSAGE-CALLBACK-LIFECYCLE"),
			TEXT("AS_SDK_EngineMessageCallback_Lifecycle"),
			GeneratedSource);

		asSFuncPtr OriginalCallback{};
		void* OriginalObject = nullptr;
		asDWORD OriginalCallConv = 0;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			ScriptEngine->GetMessageCallback(&OriginalCallback, &OriginalObject, &OriginalCallConv),
			TEXT("message callback lifecycle owner should read the installed collector callback")));

		FSDKBufferedOutStream ReplacementStream;
		const int ReplaceResult = ScriptEngine->SetMessageCallback(
			asMETHODPR(FSDKBufferedOutStream, Callback, (asSMessageInfo*), void),
			&ReplacementStream,
			asCALL_THISCALL);
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetMessageCallback(
				OriginalCallback,
				OriginalObject,
				OriginalCallConv);
		};
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			ReplaceResult,
			TEXT("message callback lifecycle owner should replace the callback with a second receiver")));

		Engine.ResetMessages();
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			ScriptEngine->WriteMessage("Replacement", 21, 22, asMSGTYPE_INFORMATION, "replacement payload"),
			TEXT("message callback lifecycle owner should write through the replacement callback")));
		ASSERT_THAT(IsTrue(
			ReplacementStream.Buffer.find("replacement payload") != std::string::npos,
			TEXT("replacement callback should receive the message text")));
		ASSERT_THAT(AreEqual(
			0,
			Engine.GetMessages().Entries.Num(),
			TEXT("replacement callback should isolate the original collector")));

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			ScriptEngine->ClearMessageCallback(),
			TEXT("message callback lifecycle owner should clear the replacement callback")));
		const int GetAfterClearResult = ScriptEngine->GetMessageCallback(nullptr, nullptr, nullptr);
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asNO_FUNCTION),
			GetAfterClearResult,
			TEXT("message callback lifecycle owner should report no callback after clear")));

		ReplacementStream.Clear();
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			ScriptEngine->WriteMessage("Cleared", 31, 32, asMSGTYPE_WARNING, "cleared payload"),
			TEXT("message callback lifecycle owner should keep WriteMessage safe after clear")));
		ASSERT_THAT(IsTrue(
			ReplacementStream.Buffer.empty(),
			TEXT("cleared callback should not receive subsequent messages")));

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			ScriptEngine->SetMessageCallback(OriginalCallback, OriginalObject, OriginalCallConv),
			TEXT("message callback lifecycle owner should restore the original callback contract")));
		Engine.ResetMessages();
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			ScriptEngine->WriteMessage("Restored", 41, 42, asMSGTYPE_ERROR, "restored payload"),
			TEXT("message callback lifecycle owner should write through the restored callback")));
		ASSERT_THAT(AreEqual(
			1,
			Engine.GetMessages().Entries.Num(),
			TEXT("restored callback should repopulate the original collector")));
		if (Engine.GetMessages().Entries.Num() == 1)
		{
			const FMessageCase Expected =
				{ "Restored", 41, 42, asMSGTYPE_ERROR, "restored payload", TEXT("restored message") };
			ASSERT_THAT(IsTrue(
				HasMessage(Engine.GetMessages().Entries[0], Expected),
				TEXT("restored callback should preserve the complete message payload")));
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
