// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/LearningTraceExample.h"

namespace AngelscriptEditor::LearningTrace
{
	/**
	 * Returns the curated example list driven by the exporter. The set is
	 * intentionally small (5–10 entries) and chosen to exercise distinct
	 * tokenizer decision branches.
	 */
	ANGELSCRIPTEDITOR_API const TArray<FLearningTraceExample>& GetCuratedLearningTraceExamples();
}
