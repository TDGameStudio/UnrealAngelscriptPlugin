// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace AngelscriptEditor::LearningTrace
{
	class FLearningTraceEventStream;
	struct FLearningTraceExample;

	struct FLearningTraceExporterOptions
	{
		/** Output directory; defaults to <ProjectSavedDir>/LearningTrace/. */
		FString OutputDir;
	};

	struct FLearningTraceExporterResult
	{
		bool bSuccess = false;
		int32 ExamplesWritten = 0;
		FString ErrorMessage;
		FString IndexFilePath;
	};

	/**
	 * Drives the curated example list across all registered phase taps and
	 * writes one JSON file per example plus an index.json listing them.
	 */
	class FLearningTraceExporter
	{
	public:
		FLearningTraceExporter() = default;

		ANGELSCRIPTEDITOR_API FLearningTraceExporterResult Run(const FLearningTraceExporterOptions& Options) const;

		/**
		 * In-process variant: builds the events for a single example using
		 * the same pipeline. Used by tests.
		 */
		ANGELSCRIPTEDITOR_API bool RunSingleExampleForTesting(
			const FLearningTraceExample& Example,
			FLearningTraceEventStream& OutStream,
			FString& OutErrorMessage) const;
	};
}
