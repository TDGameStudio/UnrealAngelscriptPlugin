// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace AngelscriptEditor::LearningTrace
{
	/**
	 * One trace event in the learning-trace event stream.
	 *
	 * - `Seq`: monotonic per-example sequence number (starts at 0).
	 * - `Phase`: identifies which phase tap emitted the event ("tokenizer",
	 *   "parser", etc.).
	 * - `Type`: phase-specific event subtype (e.g. "try-keyword",
	 *   "token-emitted").
	 * - `Pos`: source byte offset when applicable; INDEX_NONE otherwise.
	 * - `Data`: type-specific payload as a JSON object. Owned by the event.
	 */
	struct FLearningTraceEvent
	{
		int32 Seq = 0;
		FString Phase;
		FString Type;
		int32 Pos = INDEX_NONE;
		TSharedPtr<FJsonObject> Data;

		ANGELSCRIPTEDITOR_API TSharedRef<FJsonObject> ToJson() const;
	};
}
