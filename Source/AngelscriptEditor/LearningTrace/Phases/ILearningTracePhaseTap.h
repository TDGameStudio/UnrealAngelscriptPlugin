// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace AngelscriptEditor::LearningTrace
{
	struct FLearningTraceExample;
	class FLearningTraceEventStream;

	/**
	 * Common interface for every phase tap. Each tap is a self-contained
	 * implementation that observes one phase of the AS pipeline and emits
	 * events into the supplied stream.
	 *
	 * Adding a new tap (e.g. `FParserTap`) requires only a new file pair
	 * under `LearningTrace/Phases/` plus registry entry in the Exporter —
	 * no edits to `Core/` or to existing taps.
	 */
	class ILearningTracePhaseTap
	{
	public:
		virtual ~ILearningTracePhaseTap() = default;

		/** Stable phase identifier, e.g. "tokenizer", "parser". */
		virtual FString GetPhaseName() const = 0;

		/**
		 * Run the tap against one curated example, emitting events into the
		 * stream. Returns false on hard failure (rare; most tokenizer
		 * "errors" produce events, not failures).
		 */
		virtual bool Run(
			const FLearningTraceExample& Example,
			FLearningTraceEventStream& OutStream,
			FString& OutErrorMessage) = 0;
	};
}
