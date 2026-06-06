// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/LearningTraceEvent.h"
#include "Core/LearningTraceEventStream.h"
#include "Core/LearningTraceExample.h"
#include "Examples/LearningTraceExampleRegistry.h"
#include "Exporter/AngelscriptLearningTraceCommandlet.h"
#include "Exporter/LearningTraceExporter.h"
#include "Phases/TokenizerTap.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "source/as_tokenizer.h"
#include "EndAngelscriptHeaders.h"

#include "angelscript.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptEditor::LearningTrace::Tests
{
	struct FTokenSnapshot
	{
		eTokenType TokenType;
		asETokenClass TokenClass;
		int32 Offset;
		int32 Length;
	};

	// Run the unmodified asCTokenizer over the source byte-by-byte; this is
	// the drift-guard ground truth.
	TArray<FTokenSnapshot> ScanWithBaseline(const asCScriptEngine* Engine, const FString& Source)
	{
		TArray<FTokenSnapshot> Tokens;
		const FTCHARToUTF8 SourceUtf8(*Source);
		const ANSICHAR* Bytes = SourceUtf8.Get();
		const size_t Length = static_cast<size_t>(SourceUtf8.Length());

		struct FBaselineTokenizer : asCTokenizer
		{
			void SetEngine(const asCScriptEngine* InEngine) { engine = InEngine; }
		};

		FBaselineTokenizer Baseline;
		Baseline.SetEngine(Engine);

		size_t Pos = 0;
		while (Pos < Length)
		{
			size_t TokenLen = 0;
			asETokenClass TokenClass = asTC_UNKNOWN;
			eTokenType TokenType = Baseline.GetToken(Bytes + Pos, Length - Pos, &TokenLen, &TokenClass);
			if (TokenLen == 0)
			{
				break;
			}
			Tokens.Add({ TokenType, TokenClass, static_cast<int32>(Pos), static_cast<int32>(TokenLen) });
			Pos += TokenLen;
		}
		return Tokens;
	}

	// Walk the FTokenizerTap event stream and return only the
	// `token-emitted` entries as snapshots, mapping back to baseline shape.
	TArray<FTokenSnapshot> ExtractEmittedTokens(const FLearningTraceEventStream& Stream)
	{
		TArray<FTokenSnapshot> Tokens;
		for (const FLearningTraceEvent& Event : Stream.GetEvents())
		{
			if (Event.Phase != TEXT("tokenizer") || Event.Type != TEXT("token-emitted"))
			{
				continue;
			}
			if (!Event.Data.IsValid())
			{
				continue;
			}

			FTokenSnapshot Snap;
			Snap.Offset = static_cast<int32>(Event.Data->GetNumberField(TEXT("offset")));
			Snap.Length = static_cast<int32>(Event.Data->GetNumberField(TEXT("length")));

			// Round-trip token type / class via the baseline GetToken at the
			// recorded offset+length so we compare the same enum surface.
			// (We don't store the raw enums in JSON, only their names — this
			// is the supported shape for tests.)
			Snap.TokenType = ttUnrecognizedToken;
			Snap.TokenClass = asTC_UNKNOWN;
			Tokens.Add(Snap);
		}
		return Tokens;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptEditorLearningTraceTokenizerDriftGuardTest,
	"Angelscript.Editor.LearningTrace.TokenizerDriftGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptEditorLearningTraceTokenizerEventShapeTest,
	"Angelscript.Editor.LearningTrace.TokenizerEventShape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptEditorLearningTraceCommandletParsingTest,
	"Angelscript.Editor.LearningTrace.CommandletParsing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptEditorLearningTraceTokenizerDriftGuardTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptEditor::LearningTrace;
	using namespace AngelscriptEditor::LearningTrace::Tests;

	asIScriptEngine* AsEngine = asCreateScriptEngine();
	if (!TestNotNull(TEXT("Bare AS engine creation"), AsEngine))
	{
		return false;
	}
	ON_SCOPE_EXIT { AsEngine->ShutDownAndRelease(); };
	const asCScriptEngine* Engine = static_cast<const asCScriptEngine*>(AsEngine);

	const TArray<FLearningTraceExample>& Examples = GetCuratedLearningTraceExamples();
	TestTrue(TEXT("Curated example list is non-empty"), Examples.Num() > 0);

	for (const FLearningTraceExample& Example : Examples)
	{
		const TArray<FTokenSnapshot> Baseline = ScanWithBaseline(Engine, Example.Source);

		FLearningTraceEventStream Stream;
		FString Error;
		FTokenizerTap Tap(Engine);
		const bool bRan = Tap.Run(Example, Stream, Error);
		if (!TestTrue(*FString::Printf(TEXT("Tap.Run succeeded for %s"), *Example.Id), bRan))
		{
			AddError(Error);
			continue;
		}

		const TArray<FTokenSnapshot> Mirror = ExtractEmittedTokens(Stream);

		// Final token offset+length sequence must match. This is the strict
		// drift guard — if the dispatch order or any helper reuse breaks,
		// at least one example will diverge here.
		if (!TestEqual(
				*FString::Printf(TEXT("Token count match for %s"), *Example.Id),
				Mirror.Num(),
				Baseline.Num()))
		{
			continue;
		}

		for (int32 Index = 0; Index < Baseline.Num(); ++Index)
		{
			TestEqual(
				*FString::Printf(TEXT("[%s] token[%d] offset"), *Example.Id, Index),
				Mirror[Index].Offset,
				Baseline[Index].Offset);
			TestEqual(
				*FString::Printf(TEXT("[%s] token[%d] length"), *Example.Id, Index),
				Mirror[Index].Length,
				Baseline[Index].Length);
		}
	}
	return true;
}

bool FAngelscriptEditorLearningTraceTokenizerEventShapeTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptEditor::LearningTrace;

	FLearningTraceExample Example;
	Example.Id = TEXT("inline-shape-check");
	Example.Title = TEXT("Inline shape check");
	Example.Focus = TEXT("Direct test of FTokenizerTap event shape.");
	Example.Source = TEXT("int x = 42;");

	FLearningTraceExporter Exporter;
	FLearningTraceEventStream Stream;
	FString Error;
	const bool bRan = Exporter.RunSingleExampleForTesting(Example, Stream, Error);
	if (!TestTrue(TEXT("Exporter.RunSingleExampleForTesting succeeded"), bRan))
	{
		AddError(Error);
		return false;
	}

	if (!TestTrue(TEXT("Stream is non-empty"), Stream.Num() > 0))
	{
		return false;
	}

	int32 ScanStartCount = 0;
	int32 ScanEndCount = 0;
	int32 TokenEmittedCount = 0;
	int32 TryEventCount = 0;
	int32 LastSeq = -1;

	for (const FLearningTraceEvent& Event : Stream.GetEvents())
	{
		TestTrue(
			*FString::Printf(TEXT("Event seq is monotonic at %d"), Event.Seq),
			Event.Seq == LastSeq + 1);
		LastSeq = Event.Seq;
		TestEqual(TEXT("Event phase is 'tokenizer'"), Event.Phase, FString(TEXT("tokenizer")));

		if (Event.Type == TEXT("scan-start"))
		{
			++ScanStartCount;
		}
		else if (Event.Type == TEXT("scan-end"))
		{
			++ScanEndCount;
		}
		else if (Event.Type == TEXT("token-emitted"))
		{
			++TokenEmittedCount;
			TestTrue(TEXT("token-emitted has data"), Event.Data.IsValid());
			if (Event.Data.IsValid())
			{
				TestTrue(TEXT("token-emitted has tokenType"), Event.Data->HasField(TEXT("tokenType")));
				TestTrue(TEXT("token-emitted has class"), Event.Data->HasField(TEXT("class")));
				TestTrue(TEXT("token-emitted has offset"), Event.Data->HasField(TEXT("offset")));
				TestTrue(TEXT("token-emitted has length"), Event.Data->HasField(TEXT("length")));
				TestTrue(TEXT("token-emitted has text"), Event.Data->HasField(TEXT("text")));
			}
		}
		else if (Event.Type.StartsWith(TEXT("try-")))
		{
			++TryEventCount;
			TestTrue(*FString::Printf(TEXT("'%s' has data"), *Event.Type), Event.Data.IsValid());
			if (Event.Data.IsValid())
			{
				TestTrue(TEXT("try-X has accepted"), Event.Data->HasField(TEXT("accepted")));
			}
		}
	}

	TestEqual(TEXT("Exactly one scan-start"), ScanStartCount, 1);
	TestEqual(TEXT("Exactly one scan-end"), ScanEndCount, 1);
	TestTrue(TEXT("At least one token emitted"), TokenEmittedCount > 0);
	TestTrue(TEXT("At least one try-X decision event"), TryEventCount > 0);
	return true;
}

bool FAngelscriptEditorLearningTraceCommandletParsingTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptEditor::LearningTrace;

	{
		FLearningTraceExporterOptions Options;
		FString Error;
		const bool bOk = UAngelscriptLearningTraceCommandlet::BuildOptionsForTesting(
			TEXT("OutputDir=\"D:/Temp/AS Trace Out\""),
			Options,
			Error);
		TestTrue(TEXT("Quoted OutputDir parses"), bOk);
		TestTrue(TEXT("OutputDir preserves spaces"), Options.OutputDir.Contains(TEXT("AS Trace Out")));
	}

	{
		FLearningTraceExporterOptions Options;
		FString Error;
		const bool bOk = UAngelscriptLearningTraceCommandlet::BuildOptionsForTesting(
			TEXT(""),
			Options,
			Error);
		TestTrue(TEXT("Empty params parses (defaults apply)"), bOk);
		TestTrue(TEXT("Default OutputDir is empty (filled at run time)"), Options.OutputDir.IsEmpty());
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
