#pragma once

#include "AngelscriptNativeLanguageCaseTestSupport.h"

namespace AngelscriptNativeTestSupport
{
	struct FTokenCase
	{
		const char* Input;
		eTokenType ExpectedType;
		int32 ExpectedLength;
		const TCHAR* Description;
		bool bIdentifierBoundary = true;
		bool bForkRejected = false;
	};

	struct FTokenObservation
	{
		eTokenType Type = ttUnrecognizedToken;
		int32 Length = 0;
	};
	inline FTokenObservation ReadToken(const char* Input)
	{
		AngelscriptNativeTestSupport::FTokenizerAccessor Tokenizer;
		size_t TokenLength = 0;
		const eTokenType TokenType = Tokenizer.GetToken(Input, std::strlen(Input), &TokenLength);

		FTokenObservation Observation;
		Observation.Type = TokenType;
		Observation.Length = static_cast<int32>(TokenLength);
		return Observation;
	}

	inline void AppendCommentedCase(FString& Source, const TCHAR* Category, const char* Input)
	{
		FString PrintableInput = UTF8_TO_TCHAR(Input != nullptr ? Input : "");
		const FString EscapedNewline = FString::Chr(92) + TEXT("n");
		const TCHAR NewlineCharacters[] = { TCHAR(10), TCHAR(0) };
		PrintableInput.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
		PrintableInput.ReplaceInline(TEXT("\t"), TEXT("\\t"));
		PrintableInput.ReplaceInline(TEXT("\r"), TEXT("\\r"));
		PrintableInput.ReplaceInline(NewlineCharacters, *EscapedNewline);
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(
			Source,
			FString::Printf(TEXT("\t// %s: %s"), Category, *PrintableInput));
	}
}
