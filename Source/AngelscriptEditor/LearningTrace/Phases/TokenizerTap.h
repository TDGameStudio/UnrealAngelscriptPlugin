// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Phases/ILearningTracePhaseTap.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_tokenizer.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

namespace AngelscriptEditor::LearningTrace
{
	/**
	 * Tokenizer phase tap.
	 *
	 * MIRRORED FROM as_tokenizer.cpp ParseToken() — keep dispatch order in
	 * sync after AS upstream merges. The drift guard test
	 * (`LearningTraceTokenizerTests.cpp`) catches divergence by comparing
	 * final tokens against the unmodified `asCTokenizer::GetToken`.
	 *
	 * Subclassing grants access to the protected `IsWhiteSpace` / `IsComment`
	 * / `IsConstant` / `IsIdentifier` / `IsKeyWord` helpers which we call
	 * directly. We do NOT copy their bodies — only the dispatch order
	 * (~5 lines).
	 *
	 * The tap requires a backing `asCScriptEngine*` because `asCTokenizer`
	 * reads `engine->ep.allowUnicodeIdentifiers` (and conditionally
	 * `engine->ep.scanner` under AS_DOUBLEBYTE_CHARSET). The engine is
	 * provided by the Exporter and stored via the inherited protected
	 * `engine` field.
	 */
	class FTokenizerTap : public asCTokenizer, public ILearningTracePhaseTap
	{
	public:
		explicit FTokenizerTap(const asCScriptEngine* InEngine);

		virtual FString GetPhaseName() const override { return TEXT("tokenizer"); }

		virtual bool Run(
			const FLearningTraceExample& Example,
			FLearningTraceEventStream& OutStream,
			FString& OutErrorMessage) override;
	};
}
