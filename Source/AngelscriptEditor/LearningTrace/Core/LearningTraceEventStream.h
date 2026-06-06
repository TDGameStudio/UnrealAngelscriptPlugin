// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/LearningTraceEvent.h"
#include "Dom/JsonValue.h"

namespace AngelscriptEditor::LearningTrace
{
	/**
	 * Time-ordered accumulator for `FLearningTraceEvent`s emitted by phase
	 * taps over a single curated example. Sequence numbers restart at 0 per
	 * stream — one stream per example.
	 */
	class ANGELSCRIPTEDITOR_API FLearningTraceEventStream
	{
	public:
		FLearningTraceEventStream() = default;

		/** Append an event; assigns the next monotonic Seq. */
		void Append(FLearningTraceEvent&& Event);

		/** Convenience builder: phase + type + pos + (optional) data. */
		void Emit(
			const TCHAR* Phase,
			const TCHAR* Type,
			int32 Pos = INDEX_NONE,
			TSharedPtr<FJsonObject> Data = nullptr);

		/** Number of events accumulated so far. */
		int32 Num() const { return Events.Num(); }

		/** Read-only access to the underlying ordered event array. */
		const TArray<FLearningTraceEvent>& GetEvents() const { return Events; }

		/** Serialize the entire stream to a JSON array of event objects. */
		TArray<TSharedPtr<FJsonValue>> ToJsonArray() const;

	private:
		TArray<FLearningTraceEvent> Events;
		int32 NextSeq = 0;
	};
}
