#include "Shared/AngelscriptLearningTrace.h"

#include "HAL/FileManager.h"
#include "Logging/LogMacros.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogAngelscriptLearningTrace, Log, All);

FAngelscriptLearningTraceSession::FAngelscriptLearningTraceSession(
	const FString& InSessionName,
	const FAngelscriptLearningTraceSinkConfig& InSinkConfig)
	: SessionName(InSessionName)
	, SinkConfig(InSinkConfig)
{
}

void FAngelscriptLearningTraceSession::BeginPhase(EAngelscriptLearningTracePhase InPhase)
{
	CurrentPhase = InPhase;
	CurrentPhaseStepIndex = 0;
}

void FAngelscriptLearningTraceSession::AddStep(const FString& Action, const FString& Observation)
{
	++CurrentPhaseStepIndex;

	FAngelscriptLearningTraceEvent& Event = Events.AddDefaulted_GetRef();
	Event.SequenceIndex = NextSequenceIndex++;
	Event.Phase = CurrentPhase;
	Event.StepId = FString::Printf(TEXT("%s.%02d"), *GetPhaseLabel(CurrentPhase), CurrentPhaseStepIndex);
	Event.Action = Action;
	Event.Observation = Observation;
}

void FAngelscriptLearningTraceSession::AddKeyValue(const FString& Key, const FString& Value)
{
	if (FAngelscriptLearningTraceEvent* Event = GetLastEvent())
	{
		Event->Evidence.Add({Key, Value});
	}
}

void FAngelscriptLearningTraceSession::AddCodeBlock(const FString& CodeBlock)
{
	if (FAngelscriptLearningTraceEvent* Event = GetLastEvent())
	{
		Event->CodeBlocks.Add(CodeBlock);
	}
}

const TArray<FAngelscriptLearningTraceEvent>& FAngelscriptLearningTraceSession::GetEvents() const
{
	return Events;
}

TArray<FString> FAngelscriptLearningTraceSession::BuildSummaryLines() const
{
	TArray<FString> Lines;
	Lines.Reserve(Events.Num());
	for (const FAngelscriptLearningTraceEvent& Event : Events)
	{
		Lines.Add(BuildEventLine(Event));
	}
	return Lines;
}

void FAngelscriptLearningTraceSession::FlushToAutomation(FAutomationTestBase& Test) const
{
	if (!SinkConfig.bEmitToAutomation)
	{
		return;
	}

	for (const FString& Line : BuildSummaryLines())
	{
		Test.AddInfo(Line);
	}
}

void FAngelscriptLearningTraceSession::FlushToLog() const
{
	if (!SinkConfig.bEmitToLog)
	{
		return;
	}

	for (const FString& Line : BuildSummaryLines())
	{
		UE_LOG(LogAngelscriptLearningTrace, Display, TEXT("%s"), *Line);
	}
}

bool FAngelscriptLearningTraceSession::FlushToFile() const
{
	if (!SinkConfig.bEmitToFile)
	{
		return true;
	}

	const FString OutputDirectory = GetOutputRoot();
	if (!IFileManager::Get().MakeDirectory(*OutputDirectory, true))
	{
		return false;
	}

	FString OutputText;
	for (const FAngelscriptLearningTraceEvent& Event : Events)
	{
		if (!OutputText.IsEmpty())
		{
			OutputText += LINE_TERMINATOR;
		}

		OutputText += BuildEventLine(Event);
		for (const FAngelscriptLearningTraceKeyValue& Pair : Event.Evidence)
		{
			OutputText += LINE_TERMINATOR;
			OutputText += FString::Printf(TEXT("  - %s: %s"), *Pair.Key, *Pair.Value);
		}

		if (SinkConfig.DetailLevel == EAngelscriptLearningTraceDetailLevel::Verbose)
		{
			for (const FString& CodeBlock : Event.CodeBlocks)
			{
				OutputText += LINE_TERMINATOR;
				OutputText += CodeBlock;
			}
		}
	}

	const FString OutputPath = FPaths::Combine(OutputDirectory, SessionName + TEXT(".trace.txt"));
	return FFileHelper::SaveStringToFile(OutputText, *OutputPath);
}

const FAngelscriptLearningTraceEvent* FAngelscriptLearningTraceSession::GetLastEvent() const
{
	return Events.Num() > 0 ? &Events.Last() : nullptr;
}

FAngelscriptLearningTraceEvent* FAngelscriptLearningTraceSession::GetLastEvent()
{
	return Events.Num() > 0 ? &Events.Last() : nullptr;
}

FString FAngelscriptLearningTraceSession::BuildEventLine(const FAngelscriptLearningTraceEvent& Event) const
{
	FString Line = FString::Printf(TEXT("[%03d] %s | %s | %s"), Event.SequenceIndex, *Event.StepId, *Event.Action, *Event.Observation);
	if (Event.Evidence.Num() > 0)
	{
		for (const FAngelscriptLearningTraceKeyValue& Pair : Event.Evidence)
		{
			Line += FString::Printf(TEXT(" | %s=%s"), *Pair.Key, *Pair.Value);
		}
	}
	return Line;
}

FString FAngelscriptLearningTraceSession::GetPhaseLabel(EAngelscriptLearningTracePhase Phase)
{
	switch (Phase)
	{
	case EAngelscriptLearningTracePhase::EngineBootstrap:
		return TEXT("EngineBootstrap");
	case EAngelscriptLearningTracePhase::Binding:
		return TEXT("Binding");
	case EAngelscriptLearningTracePhase::Compile:
		return TEXT("Compile");
	case EAngelscriptLearningTracePhase::Bytecode:
		return TEXT("Bytecode");
	case EAngelscriptLearningTracePhase::ClassGeneration:
		return TEXT("ClassGeneration");
	case EAngelscriptLearningTracePhase::Execution:
		return TEXT("Execution");
	case EAngelscriptLearningTracePhase::UEBridge:
		return TEXT("UEBridge");
	case EAngelscriptLearningTracePhase::Debug:
		return TEXT("Debug");
	case EAngelscriptLearningTracePhase::GC:
		return TEXT("GC");
	case EAngelscriptLearningTracePhase::Editor:
		return TEXT("Editor");
	default:
		return TEXT("Trace");
	}
}

FString FAngelscriptLearningTraceSession::GetOutputRoot() const
{
	if (!SinkConfig.OutputRoot.IsEmpty())
	{
		return SinkConfig.OutputRoot;
	}

	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/AngelscriptLearning"));
}

FString FormatLearningTraceKeyValueList(const TArray<FAngelscriptLearningTraceKeyValue>& Entries, const FString& Title)
{
	FString Result = Title;
	for (const FAngelscriptLearningTraceKeyValue& Entry : Entries)
	{
		Result += LINE_TERMINATOR;
		Result += FString::Printf(TEXT("- %s=%s"), *Entry.Key, *Entry.Value);
	}
	return Result;
}

FString FormatLearningTraceDiagnostics(const TArray<FAngelscriptLearningTraceDiagnostic>& Diagnostics)
{
	FString Result;
	for (const FAngelscriptLearningTraceDiagnostic& Diagnostic : Diagnostics)
	{
		if (!Result.IsEmpty())
		{
			Result += LINE_TERMINATOR;
		}
		Result += FString::Printf(
			TEXT("%s:%d:%d [%s] %s"),
			*Diagnostic.Section,
			Diagnostic.Row,
			Diagnostic.Column,
			*Diagnostic.Severity,
			*Diagnostic.Message);
	}
	return Result;
}

FString FormatLearningTraceStringList(const TArray<FString>& Lines, const FString& Title)
{
	FString Result = Title;
	for (const FString& Line : Lines)
	{
		Result += LINE_TERMINATOR;
		Result += FString::Printf(TEXT("- %s"), *Line);
	}
	return Result;
}

FString FormatLearningTraceBytecode(const TArray<uint32>& Dwords, int32 PreviewCount)
{
	const int32 SafePreviewCount = FMath::Max(0, PreviewCount);
	const int32 VisibleCount = FMath::Min(Dwords.Num(), SafePreviewCount);
	FString Preview;
	for (int32 Index = 0; Index < VisibleCount; ++Index)
	{
		if (!Preview.IsEmpty())
		{
			Preview += TEXT(", ");
		}
		Preview += FString::Printf(TEXT("0x%08X"), Dwords[Index]);
	}

	return FString::Printf(TEXT("%d dwords | %s"), Dwords.Num(), Preview.IsEmpty() ? TEXT("<empty>") : *Preview);
}

FString FormatLearningTraceClassSummary(
	const FString& ClassName,
	const FString& SuperClassName,
	bool bIsActorDerived,
	const TArray<FString>& FunctionSummaries,
	const TArray<FString>& PropertySummaries)
{
	FString Result = FString::Printf(
		TEXT("Class=%s | Super=%s | ActorDerived=%s"),
		*ClassName,
		*SuperClassName,
		bIsActorDerived ? TEXT("true") : TEXT("false"));

	if (FunctionSummaries.Num() > 0)
	{
		Result += LINE_TERMINATOR;
		Result += FormatLearningTraceStringList(FunctionSummaries, TEXT("Functions"));
	}

	if (PropertySummaries.Num() > 0)
	{
		Result += LINE_TERMINATOR;
		Result += FormatLearningTraceStringList(PropertySummaries, TEXT("Properties"));
	}

	return Result;
}

bool AssertLearningTracePhaseSequence(
	FAutomationTestBase& Test,
	const TArray<FAngelscriptLearningTraceEvent>& Events,
	const TArray<EAngelscriptLearningTracePhase>& ExpectedPhases,
	bool bEmitErrors)
{
	TArray<EAngelscriptLearningTracePhase> ActualPhases;
	for (const FAngelscriptLearningTraceEvent& Event : Events)
	{
		if (ActualPhases.Num() == 0 || ActualPhases.Last() != Event.Phase)
		{
			ActualPhases.Add(Event.Phase);
		}
	}

	if (ActualPhases.Num() != ExpectedPhases.Num())
	{
		if (bEmitErrors)
		{
			Test.AddError(FString::Printf(TEXT("Expected %d phases but captured %d"), ExpectedPhases.Num(), ActualPhases.Num()));
		}
		return false;
	}

	for (int32 Index = 0; Index < ExpectedPhases.Num(); ++Index)
	{
		if (ActualPhases[Index] != ExpectedPhases[Index])
		{
			if (bEmitErrors)
			{
				Test.AddError(FString::Printf(
					TEXT("Phase mismatch at index %d: expected %s but captured %s"),
					Index,
					*FAngelscriptLearningTraceSession::GetPhaseLabel(ExpectedPhases[Index]),
					*FAngelscriptLearningTraceSession::GetPhaseLabel(ActualPhases[Index])));
			}
			return false;
		}
	}

	return true;
}

bool AssertLearningTraceContainsKeyword(
	FAutomationTestBase& Test,
	const TArray<FAngelscriptLearningTraceEvent>& Events,
	const FString& Keyword,
	bool bEmitErrors)
{
	for (const FAngelscriptLearningTraceEvent& Event : Events)
	{
		if (Event.StepId.Contains(Keyword) || Event.Action.Contains(Keyword) || Event.Observation.Contains(Keyword))
		{
			return true;
		}

		for (const FAngelscriptLearningTraceKeyValue& Entry : Event.Evidence)
		{
			if (Entry.Key.Contains(Keyword) || Entry.Value.Contains(Keyword))
			{
				return true;
			}
		}
	}

	if (bEmitErrors)
	{
		Test.AddError(FString::Printf(TEXT("Trace output did not contain required keyword '%s'"), *Keyword));
	}
	return false;
}

bool AssertLearningTraceMinimumEventCount(
	FAutomationTestBase& Test,
	const TArray<FAngelscriptLearningTraceEvent>& Events,
	int32 MinimumEventCount,
	bool bEmitErrors)
{
	if (Events.Num() < MinimumEventCount)
	{
		if (bEmitErrors)
		{
			Test.AddError(FString::Printf(TEXT("Expected at least %d trace events but only captured %d"), MinimumEventCount, Events.Num()));
		}
		return false;
	}

	return true;
}

