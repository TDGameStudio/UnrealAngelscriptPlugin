// Copyright Epic Games, Inc. All Rights Reserved.

#include "Exporter/AngelscriptLearningTraceCommandlet.h"

#include "Exporter/LearningTraceExporter.h"

#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	enum class ELearningTraceExitCode : int32
	{
		Success = 0,
		InvalidArguments = 1,
		ExportFailure = 2,
	};

	bool TryParseOptions(
		const FString& Params,
		AngelscriptEditor::LearningTrace::FLearningTraceExporterOptions& OutOptions,
		FString& OutErrorMessage)
	{
		OutOptions = AngelscriptEditor::LearningTrace::FLearningTraceExporterOptions();
		OutErrorMessage.Reset();

		FString OutputDir;
		if (FParse::Value(*Params, TEXT("OutputDir="), OutputDir))
		{
			if (OutputDir.IsEmpty())
			{
				OutErrorMessage = TEXT("OutputDir is empty.");
				return false;
			}
			OutOptions.OutputDir = OutputDir;
		}

		return true;
	}
}

#if WITH_DEV_AUTOMATION_TESTS
bool UAngelscriptLearningTraceCommandlet::BuildOptionsForTesting(
	const FString& Params,
	AngelscriptEditor::LearningTrace::FLearningTraceExporterOptions& OutOptions,
	FString& OutErrorMessage)
{
	return TryParseOptions(Params, OutOptions, OutErrorMessage);
}
#endif

int32 UAngelscriptLearningTraceCommandlet::Main(const FString& Params)
{
	using namespace AngelscriptEditor::LearningTrace;

	FLearningTraceExporterOptions Options;
	FString ErrorMessage;
	if (!TryParseOptions(Params, Options, ErrorMessage))
	{
		UE_LOG(LogTemp, Error, TEXT("AngelscriptLearningTrace invalid arguments: %s"), *ErrorMessage);
		return static_cast<int32>(ELearningTraceExitCode::InvalidArguments);
	}

	FLearningTraceExporter Exporter;
	const FLearningTraceExporterResult Result = Exporter.Run(Options);
	if (!Result.bSuccess)
	{
		UE_LOG(LogTemp, Error, TEXT("AngelscriptLearningTrace export failed: %s"), *Result.ErrorMessage);
		return static_cast<int32>(ELearningTraceExitCode::ExportFailure);
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("AngelscriptLearningTrace wrote %d examples + index to %s"),
		Result.ExamplesWritten,
		*Result.IndexFilePath);
	return static_cast<int32>(ELearningTraceExitCode::Success);
}
