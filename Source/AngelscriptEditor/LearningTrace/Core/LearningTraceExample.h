// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace AngelscriptEditor::LearningTrace
{
	/**
	 * Curated AS source snippet metadata. The Exporter loops over instances
	 * of this struct registered in `LearningTraceExampleRegistry`.
	 */
	struct FLearningTraceExample
	{
		/** Stable kebab-case identifier — also used as the JSON filename. */
		FString Id;
		/** Human-readable example title for UI / animation captions. */
		FString Title;
		/** Short focus statement: what this example teaches. */
		FString Focus;
		/** AS source code (raw — no preprocessing). UTF-8. */
		FString Source;
		/** Tags / topics this example exercises (whitespace, keyword, etc.). */
		TArray<FString> ExpectedTopics;
	};
}
