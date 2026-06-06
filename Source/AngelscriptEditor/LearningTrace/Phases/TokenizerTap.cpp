// Copyright Epic Games, Inc. All Rights Reserved.

#include "Phases/TokenizerTap.h"

#include "Core/LearningTraceEvent.h"
#include "Core/LearningTraceEventStream.h"
#include "Core/LearningTraceExample.h"

#include "Dom/JsonObject.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_tokendef.h"
#include "EndAngelscriptHeaders.h"

namespace AngelscriptEditor::LearningTrace
{
	namespace
	{
		// MIRRORED FROM as_tokenizer.cpp::GetDefinition — short stable
		// names for teaching JSON. Kept narrow on purpose.
		FString TokenTypeName(eTokenType TokenType)
		{
			switch (TokenType)
			{
			case ttUnrecognizedToken: return TEXT("ttUnrecognizedToken");
			case ttEnd: return TEXT("ttEnd");
			case ttWhiteSpace: return TEXT("ttWhiteSpace");
			case ttOnelineComment: return TEXT("ttOnelineComment");
			case ttMultilineComment: return TEXT("ttMultilineComment");
			case ttIdentifier: return TEXT("ttIdentifier");
			case ttIntConstant: return TEXT("ttIntConstant");
			case ttFloat32Constant: return TEXT("ttFloat32Constant");
			case ttFloat64Constant: return TEXT("ttFloat64Constant");
			case ttStringConstant: return TEXT("ttStringConstant");
			case ttMultilineStringConstant: return TEXT("ttMultilineStringConstant");
			case ttNonTerminatedStringConstant: return TEXT("ttNonTerminatedStringConstant");
			case ttHeredocStringConstant: return TEXT("ttHeredocStringConstant");
			case ttBitsConstant: return TEXT("ttBitsConstant");
			default:
				if (const char* Definition = asCTokenizer::GetDefinition(TokenType))
				{
					return UTF8_TO_TCHAR(Definition);
				}
				return FString::Printf(TEXT("TokenType%d"), static_cast<int32>(TokenType));
			}
		}

		FString TokenClassName(asETokenClass TokenClass)
		{
			switch (TokenClass)
			{
			case asTC_UNKNOWN: return TEXT("Unknown");
			case asTC_KEYWORD: return TEXT("Keyword");
			case asTC_VALUE: return TEXT("Value");
			case asTC_IDENTIFIER: return TEXT("Identifier");
			case asTC_WHITESPACE: return TEXT("Whitespace");
			case asTC_COMMENT: return TEXT("Comment");
			default:
				return FString::Printf(TEXT("TokenClass%d"), static_cast<int32>(TokenClass));
			}
		}

		FString EscapeForJson(const FString& Text)
		{
			FString Escaped = Text;
			Escaped.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
			Escaped.ReplaceInline(TEXT("\""), TEXT("\\\""));
			Escaped.ReplaceInline(TEXT("\r"), TEXT("\\r"));
			Escaped.ReplaceInline(TEXT("\n"), TEXT("\\n"));
			Escaped.ReplaceInline(TEXT("\t"), TEXT("\\t"));
			return Escaped;
		}

		FString DisplayTextFromBytes(const ANSICHAR* Source, size_t Offset, size_t Length)
		{
			// Tokenizer treats source as bytes; we expose a UTF-8 conversion
			// for display. Non-UTF-8 byte sequences fall back to a per-byte
			// textual form so JSON stays valid.
			FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(Source + Offset), static_cast<int32>(Length));
			FString Text(Converter.Length(), Converter.Get());
			return EscapeForJson(Text);
		}

		void SetIfPositive(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, int32 Value)
		{
			if (Value >= 0)
			{
				Object->SetNumberField(Field, Value);
			}
		}

		FString ConstantTypeNameFor(eTokenType TokenType)
		{
			switch (TokenType)
			{
			case ttIntConstant: return TEXT("int");
			case ttFloat32Constant: return TEXT("float32");
			case ttFloat64Constant: return TEXT("float64");
			case ttBitsConstant: return TEXT("bits");
			case ttStringConstant: return TEXT("string");
			case ttMultilineStringConstant: return TEXT("multiline-string");
			case ttNonTerminatedStringConstant: return TEXT("nonterminated-string");
			case ttHeredocStringConstant: return TEXT("heredoc-string");
			default:
				return FString::Printf(TEXT("constant-%d"), static_cast<int32>(TokenType));
			}
		}

		FString CommentKindFor(eTokenType TokenType)
		{
			switch (TokenType)
			{
			case ttOnelineComment: return TEXT("oneline");
			case ttMultilineComment: return TEXT("multiline");
			default:
				return TEXT("unknown");
			}
		}
	}

	FTokenizerTap::FTokenizerTap(const asCScriptEngine* InEngine)
	{
		// `engine` is a protected member of asCTokenizer; subclass access OK.
		engine = InEngine;
	}

	bool FTokenizerTap::Run(
		const FLearningTraceExample& Example,
		FLearningTraceEventStream& OutStream,
		FString& OutErrorMessage)
	{
		const FTCHARToUTF8 SourceUtf8(*Example.Source);
		const ANSICHAR* SourceBytes = SourceUtf8.Get();
		const size_t SourceLength = static_cast<size_t>(SourceUtf8.Length());

		{
			TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
			Data->SetNumberField(TEXT("sourceLength"), static_cast<int32>(SourceLength));
			OutStream.Emit(TEXT("tokenizer"), TEXT("scan-start"), 0, Data);
		}

		size_t Pos = 0;
		int32 TokenIndex = 0;
		while (Pos < SourceLength)
		{
			const ANSICHAR* Cursor = SourceBytes + Pos;
			const size_t Remaining = SourceLength - Pos;
			const int32 PosI = static_cast<int32>(Pos);

			// MIRRORED FROM as_tokenizer.cpp::ParseToken — same dispatch order.
			// Each Is* helper is the original from asCTokenizer (inherited),
			// not a copy. We only reproduce the call order and emit events
			// around each call.

			// 1. IsWhiteSpace
			{
				size_t TokenLen = 0;
				eTokenType TokenType = ttUnrecognizedToken;
				const bool bAccepted = IsWhiteSpace(Cursor, Remaining, TokenLen, TokenType);
				TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
				Data->SetBoolField(TEXT("accepted"), bAccepted);
				if (bAccepted)
				{
					Data->SetNumberField(TEXT("length"), static_cast<int32>(TokenLen));
				}
				OutStream.Emit(TEXT("tokenizer"), TEXT("try-whitespace"), PosI, Data);

				if (bAccepted)
				{
					TSharedPtr<FJsonObject> Emit = MakeShared<FJsonObject>();
					Emit->SetNumberField(TEXT("index"), TokenIndex);
					Emit->SetStringField(TEXT("tokenType"), TokenTypeName(TokenType));
					Emit->SetStringField(TEXT("class"), TokenClassName(asTC_WHITESPACE));
					Emit->SetNumberField(TEXT("offset"), PosI);
					Emit->SetNumberField(TEXT("length"), static_cast<int32>(TokenLen));
					Emit->SetStringField(TEXT("text"), DisplayTextFromBytes(SourceBytes, Pos, TokenLen));
					OutStream.Emit(TEXT("tokenizer"), TEXT("token-emitted"), PosI, Emit);
					Pos += TokenLen;
					++TokenIndex;
					continue;
				}
			}

			// 2. IsComment
			{
				size_t TokenLen = 0;
				eTokenType TokenType = ttUnrecognizedToken;
				const bool bAccepted = IsComment(Cursor, Remaining, TokenLen, TokenType);
				TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
				Data->SetBoolField(TEXT("accepted"), bAccepted);
				if (bAccepted)
				{
					Data->SetStringField(TEXT("kind"), CommentKindFor(TokenType));
					Data->SetNumberField(TEXT("length"), static_cast<int32>(TokenLen));
				}
				OutStream.Emit(TEXT("tokenizer"), TEXT("try-comment"), PosI, Data);

				if (bAccepted)
				{
					TSharedPtr<FJsonObject> Emit = MakeShared<FJsonObject>();
					Emit->SetNumberField(TEXT("index"), TokenIndex);
					Emit->SetStringField(TEXT("tokenType"), TokenTypeName(TokenType));
					Emit->SetStringField(TEXT("class"), TokenClassName(asTC_COMMENT));
					Emit->SetNumberField(TEXT("offset"), PosI);
					Emit->SetNumberField(TEXT("length"), static_cast<int32>(TokenLen));
					Emit->SetStringField(TEXT("text"), DisplayTextFromBytes(SourceBytes, Pos, TokenLen));
					OutStream.Emit(TEXT("tokenizer"), TEXT("token-emitted"), PosI, Emit);
					Pos += TokenLen;
					++TokenIndex;
					continue;
				}
			}

			// 3. IsConstant
			{
				size_t TokenLen = 0;
				eTokenType TokenType = ttUnrecognizedToken;
				const bool bAccepted = IsConstant(Cursor, Remaining, TokenLen, TokenType);
				TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
				Data->SetBoolField(TEXT("accepted"), bAccepted);
				if (bAccepted)
				{
					Data->SetStringField(TEXT("constantType"), ConstantTypeNameFor(TokenType));
					Data->SetNumberField(TEXT("length"), static_cast<int32>(TokenLen));
				}
				OutStream.Emit(TEXT("tokenizer"), TEXT("try-constant"), PosI, Data);

				if (bAccepted)
				{
					TSharedPtr<FJsonObject> Emit = MakeShared<FJsonObject>();
					Emit->SetNumberField(TEXT("index"), TokenIndex);
					Emit->SetStringField(TEXT("tokenType"), TokenTypeName(TokenType));
					Emit->SetStringField(TEXT("class"), TokenClassName(asTC_VALUE));
					Emit->SetNumberField(TEXT("offset"), PosI);
					Emit->SetNumberField(TEXT("length"), static_cast<int32>(TokenLen));
					Emit->SetStringField(TEXT("text"), DisplayTextFromBytes(SourceBytes, Pos, TokenLen));
					OutStream.Emit(TEXT("tokenizer"), TEXT("token-emitted"), PosI, Emit);
					Pos += TokenLen;
					++TokenIndex;
					continue;
				}
			}

			// 4. IsIdentifier (NOTE: this internally calls IsKeyWord to
			// reject identifiers that match keywords, so a "true" here
			// means it is genuinely an identifier — not a keyword.)
			{
				size_t TokenLen = 0;
				eTokenType TokenType = ttUnrecognizedToken;
				const bool bAccepted = IsIdentifier(Cursor, Remaining, TokenLen, TokenType);
				TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
				Data->SetBoolField(TEXT("accepted"), bAccepted);
				if (bAccepted)
				{
					Data->SetStringField(TEXT("value"), DisplayTextFromBytes(SourceBytes, Pos, TokenLen));
					Data->SetNumberField(TEXT("length"), static_cast<int32>(TokenLen));
				}
				OutStream.Emit(TEXT("tokenizer"), TEXT("try-identifier"), PosI, Data);

				if (bAccepted)
				{
					TSharedPtr<FJsonObject> Emit = MakeShared<FJsonObject>();
					Emit->SetNumberField(TEXT("index"), TokenIndex);
					Emit->SetStringField(TEXT("tokenType"), TokenTypeName(TokenType));
					Emit->SetStringField(TEXT("class"), TokenClassName(asTC_IDENTIFIER));
					Emit->SetNumberField(TEXT("offset"), PosI);
					Emit->SetNumberField(TEXT("length"), static_cast<int32>(TokenLen));
					Emit->SetStringField(TEXT("text"), DisplayTextFromBytes(SourceBytes, Pos, TokenLen));
					OutStream.Emit(TEXT("tokenizer"), TEXT("token-emitted"), PosI, Emit);
					Pos += TokenLen;
					++TokenIndex;
					continue;
				}
			}

			// 5. IsKeyWord
			{
				size_t TokenLen = 0;
				eTokenType TokenType = ttUnrecognizedToken;
				const bool bAccepted = IsKeyWord(Cursor, Remaining, TokenLen, TokenType);
				TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
				Data->SetBoolField(TEXT("accepted"), bAccepted);
				if (bAccepted)
				{
					Data->SetStringField(TEXT("keyword"), DisplayTextFromBytes(SourceBytes, Pos, TokenLen));
					Data->SetStringField(TEXT("tokenType"), TokenTypeName(TokenType));
					Data->SetNumberField(TEXT("length"), static_cast<int32>(TokenLen));
				}
				OutStream.Emit(TEXT("tokenizer"), TEXT("try-keyword"), PosI, Data);

				if (bAccepted)
				{
					TSharedPtr<FJsonObject> Emit = MakeShared<FJsonObject>();
					Emit->SetNumberField(TEXT("index"), TokenIndex);
					Emit->SetStringField(TEXT("tokenType"), TokenTypeName(TokenType));
					Emit->SetStringField(TEXT("class"), TokenClassName(asTC_KEYWORD));
					Emit->SetNumberField(TEXT("offset"), PosI);
					Emit->SetNumberField(TEXT("length"), static_cast<int32>(TokenLen));
					Emit->SetStringField(TEXT("text"), DisplayTextFromBytes(SourceBytes, Pos, TokenLen));
					OutStream.Emit(TEXT("tokenizer"), TEXT("token-emitted"), PosI, Emit);
					Pos += TokenLen;
					++TokenIndex;
					continue;
				}
			}

			// Unrecognized: advance one byte to mirror asCTokenizer fallback.
			{
				TSharedPtr<FJsonObject> Emit = MakeShared<FJsonObject>();
				Emit->SetNumberField(TEXT("index"), TokenIndex);
				Emit->SetStringField(TEXT("tokenType"), TokenTypeName(ttUnrecognizedToken));
				Emit->SetStringField(TEXT("class"), TokenClassName(asTC_UNKNOWN));
				Emit->SetNumberField(TEXT("offset"), PosI);
				Emit->SetNumberField(TEXT("length"), 1);
				Emit->SetStringField(TEXT("text"), DisplayTextFromBytes(SourceBytes, Pos, 1));
				OutStream.Emit(TEXT("tokenizer"), TEXT("token-emitted"), PosI, Emit);
				Pos += 1;
				++TokenIndex;
			}
		}

		{
			TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
			Data->SetNumberField(TEXT("totalTokens"), TokenIndex);
			Data->SetNumberField(TEXT("totalEvents"), OutStream.Num() + 1); // include this scan-end
			OutStream.Emit(TEXT("tokenizer"), TEXT("scan-end"), static_cast<int32>(SourceLength), Data);
		}

		(void)OutErrorMessage;
		return true;
	}
}
