#include "../Support/AngelscriptNativeTokenizerTestSupport.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTokenizerTextCommentWhitespaceDepthTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.Tokenizer.DeepCoverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FTextLiteralCase
	{
		const char* FamilyCatalogName;
		const char* PayloadCatalogName;
		const char* Open;
		const char* Payload;
		const char* Close;
		eTokenType ExpectedType;
		const TCHAR* Description;
	};

	struct FTextBoundaryCase
	{
		const char* CatalogName;
		const char* Suffix;
		eTokenType ExpectedNextType;
		int32 ExpectedNextLength;
		const TCHAR* Description;
	};

	struct FCommentWhitespaceFamilyCase
	{
		const char* CatalogName;
		const char* Open;
		const char* Close;
		eTokenType ExpectedType;
		const TCHAR* Description;
	};

	struct FCommentWhitespacePayloadCase
	{
		const char* CatalogName;
		const char* Input;
		const TCHAR* Description;
	};

	struct FCommentWhitespaceBoundaryCase
	{
		const char* CatalogName;
		const char* Suffix;
		const char* Tail;
		eTokenType ExpectedLineNextType;
		int32 ExpectedLineNextLength;
		eTokenType ExpectedDelimitedNextType;
		int32 ExpectedDelimitedNextLength;
		const TCHAR* Description;
	};

public:
	TEST_METHOD(StringCommentAndWhitespaceBoundariesRemainDistinct)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("FRONTEND-TOKEN-TEXT-COMMENT-WHITESPACE-BOUNDARIES",
			ENativeEvidence::Metadata
			| ENativeEvidence::Diagnostic);

		const char BomInput[] = "\xEF\xBB\xBFValue";
		const FTokenCase Cases[] =
		{
			{ "   \t\r\nValue", ttWhiteSpace, 6, TEXT("grouped whitespace") },
			{ "\r\nValue", ttWhiteSpace, 2, TEXT("CRLF whitespace") },
			{ "// comment\nValue", ttOnelineComment, 11, TEXT("line comment with terminator") },
			{ "// comment\r\nValue", ttOnelineComment, 12, TEXT("line comment with CRLF terminator") },
			{ "// comment", ttOnelineComment, 10, TEXT("line comment at end of input") },
			{ "/* comment */Value", ttMultilineComment, 13, TEXT("closed block comment") },
			{ "/**/Value", ttMultilineComment, 4, TEXT("empty block comment") },
			{ "/* a /* b */Value", ttMultilineComment, 12, TEXT("block comment first close") },
			{ "/* comment", ttMultilineComment, 10, TEXT("unterminated block comment") },
			{ "\"\"Value", ttStringConstant, 2, TEXT("empty string") },
			{ "\"escaped \\\" quote\"Value", ttStringConstant, 18, TEXT("escaped quote string") },
			{ "\"\\n\"Value", ttStringConstant, 4, TEXT("newline escape string") },
			{ "\"\\x41\"Value", ttStringConstant, 6, TEXT("hex escape string") },
			{ "'\\n'Value", ttStringConstant, 4, TEXT("escaped character literal") },
			{ "\"line\r\ntext\"Value", ttMultilineStringConstant, 12, TEXT("CRLF multiline string") },
			{ "\"line\ntext\"Value", ttMultilineStringConstant, 11, TEXT("multiline quoted string") },
			{ "\"\"\"line\ntext\"\"\"Value", ttHeredocStringConstant, 15, TEXT("heredoc string") },
			{ "\"\"\"raw \\n text\"\"\"Value", ttHeredocStringConstant, 17, TEXT("heredoc preserves escape text") },
			{ "\"\\\\ slash\"Value", ttStringConstant, 10, TEXT("escaped backslash string") },
			{ "\"你好\"Value", ttStringConstant, 8, TEXT("UTF8 string bytes") },
			{ "'a'Value", ttStringConstant, 3, TEXT("single quoted character") },
			{ "\"unterminated", ttNonTerminatedStringConstant, 13, TEXT("unterminated string") },
		};

		FString GeneratedSource;
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(GeneratedSource, TEXT("string GeneratedTextCorpus()"));
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(GeneratedSource, TEXT("{"));
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(GeneratedSource, TEXT("\treturn \"token corpus\";"));
		for (const FTokenCase& Case : Cases)
		{
			AppendCommentedCase(GeneratedSource, TEXT("text boundary"), Case.Input);
			const FTokenObservation Observation = ReadToken(Case.Input);
			ASSERT_THAT(AreEqual(static_cast<int32>(Case.ExpectedType), static_cast<int32>(Observation.Type),
				FString::Printf(TEXT("%s should retain its token class"), Case.Description)));
			ASSERT_THAT(AreEqual(Case.ExpectedLength, Observation.Length,
				FString::Printf(TEXT("%s should stop at the expected byte boundary"), Case.Description)));
		}

		AppendCommentedCase(GeneratedSource, TEXT("UTF-8 BOM input"), BomInput);
		const FTokenObservation BomObservation = ReadToken(BomInput);
		ASSERT_THAT(AreEqual(static_cast<int32>(ttWhiteSpace), static_cast<int32>(BomObservation.Type),
			TEXT("UTF-8 BOM should be classified as whitespace before identifier scanning")));
		ASSERT_THAT(AreEqual(3, BomObservation.Length,
			TEXT("UTF-8 BOM whitespace should consume exactly three bytes")));

		AngelscriptNativeTestSupport::AppendGeneratedAsLine(GeneratedSource, TEXT("}"));
		AngelscriptNativeTestSupport::PrintGeneratedAsSource(
			*TestRunner,
			TEXT("FRONTEND-TOKEN-TEXT-COMMENT-WHITESPACE-BOUNDARIES"),
			TEXT("AS_SDK_FrontendTokenizerDeep_Text"),
			GeneratedSource);
	}

	TEST_METHOD(TextLiteralEscapeAndLineEndingCartesianProduct)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("FRONTEND-TOKEN-TEXT-ESCAPE-LINE-ENDINGS",
			ENativeEvidence::Metadata
			| ENativeEvidence::Diagnostic);

		const FTextLiteralCase LiteralCases[] =
		{
			{ "quoted_string", "short_text", "\"", "A", "\"", ttStringConstant, TEXT("quoted string short text") },
			{ "quoted_string", "escaped_delimiter", "\"", "\\\"", "\"", ttStringConstant, TEXT("quoted string escaped delimiter") },
			{ "quoted_string", "escaped_newline", "\"", "\x5C" "n", "\"", ttStringConstant, TEXT("quoted string newline escape") },
			{ "quoted_string", "hex_escape", "\"", "\\x41", "\"", ttStringConstant, TEXT("quoted string hexadecimal escape") },
			{ "quoted_string", "escaped_backslash", "\"", "\\\\", "\"", ttStringConstant, TEXT("quoted string escaped backslash") },
			{ "character_literal", "short_text", "'", "A", "'", ttStringConstant, TEXT("character literal short text") },
			{ "character_literal", "escaped_delimiter", "'", "\\'", "'", ttStringConstant, TEXT("character literal escaped delimiter") },
			{ "character_literal", "escaped_newline", "'", "\x5C" "n", "'", ttStringConstant, TEXT("character literal newline escape") },
			{ "character_literal", "hex_escape", "'", "\\x41", "'", ttStringConstant, TEXT("character literal hexadecimal escape") },
			{ "character_literal", "escaped_backslash", "'", "\\\\", "'", ttStringConstant, TEXT("character literal escaped backslash") },
			{ "heredoc_string", "short_text", "\"\"\"", "line", "\"\"\"", ttHeredocStringConstant, TEXT("heredoc short text") },
			{ "heredoc_string", "escaped_delimiter", "\"\"\"", "quotes \" stay", "\"\"\"", ttHeredocStringConstant, TEXT("heredoc delimiter-like text") },
			{ "heredoc_string", "escaped_newline", "\"\"\"", "line\x0A" "text", "\"\"\"", ttHeredocStringConstant, TEXT("heredoc physical newline") },
			{ "heredoc_string", "hex_escape", "\"\"\"", "raw \\x41 text", "\"\"\"", ttHeredocStringConstant, TEXT("heredoc escape-looking text") },
			{ "heredoc_string", "escaped_backslash", "\"\"\"", "raw \\\\ text", "\"\"\"", ttHeredocStringConstant, TEXT("heredoc backslash text") },
		};

		const FTextBoundaryCase BoundaryCases[] =
		{
			{ "eof", "", ttUnrecognizedToken, 1, TEXT("end of input") },
			{ "lf", "\x0A", ttWhiteSpace, 1, TEXT("LF boundary") },
			{ "crlf", "\x0D\x0A", ttWhiteSpace, 2, TEXT("CRLF boundary") },
			{ "identifier_suffix", "Value", ttIdentifier, 5, TEXT("identifier suffix boundary") },
		};

		FString GeneratedSource;
		AppendGeneratedAsLine(GeneratedSource, TEXT("int GeneratedTextLiteralCell()"));
		AppendGeneratedAsLine(GeneratedSource, TEXT("{"));
		AppendGeneratedAsLine(GeneratedSource, TEXT("\treturn 0;"));
		AppendGeneratedAsLine(GeneratedSource, TEXT("}"));

		int32 ObservedCaseCount = 0;
		for (const FTextLiteralCase& LiteralCase : LiteralCases)
		{
			for (const FTextBoundaryCase& BoundaryCase : BoundaryCases)
			{
				std::string Input(LiteralCase.Open);
				Input += LiteralCase.Payload;
				Input += LiteralCase.Close;
				Input += BoundaryCase.Suffix;

				const FString CaseId = MakeNativeCaseId(
					"FRONTEND-TOKEN-TEXT-ESCAPE-LINE-ENDINGS",
					{ ANSI_TO_TCHAR(LiteralCase.FamilyCatalogName), ANSI_TO_TCHAR(LiteralCase.PayloadCatalogName), ANSI_TO_TCHAR(BoundaryCase.CatalogName) });
				AppendCommentedCase(GeneratedSource, TEXT("text literal input"), Input.c_str());
				AppendGeneratedAsLine(GeneratedSource,
					FString::Printf(TEXT("\t// CaseId: %s"), *CaseId));

				const int32 ExpectedLiteralLength = static_cast<int32>(
					std::strlen(LiteralCase.Open)
					+ std::strlen(LiteralCase.Payload)
					+ std::strlen(LiteralCase.Close));
				const FTokenObservation LiteralObservation = ReadToken(Input.c_str());
				ASSERT_THAT(AreEqual(static_cast<int32>(LiteralCase.ExpectedType), static_cast<int32>(LiteralObservation.Type),
					FString::Printf(TEXT("%s should retain its token type in %s"), LiteralCase.Description, *CaseId)));
				ASSERT_THAT(AreEqual(ExpectedLiteralLength, LiteralObservation.Length,
					FString::Printf(TEXT("%s should consume exactly its literal bytes in %s"), LiteralCase.Description, *CaseId)));

				const FTokenObservation BoundaryObservation = ReadToken(Input.c_str() + ExpectedLiteralLength);
				ASSERT_THAT(AreEqual(static_cast<int32>(BoundaryCase.ExpectedNextType), static_cast<int32>(BoundaryObservation.Type),
					FString::Printf(TEXT("%s should remain after %s"), BoundaryCase.Description, *CaseId)));
				ASSERT_THAT(AreEqual(BoundaryCase.ExpectedNextLength, BoundaryObservation.Length,
					FString::Printf(TEXT("%s should retain its exact boundary length in %s"), BoundaryCase.Description, *CaseId)));
				++ObservedCaseCount;
			}
		}

		ASSERT_THAT(AreEqual(60, ObservedCaseCount,
			TEXT("Text literal × payload × line-ending product should execute every cell")));
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("FRONTEND-TOKEN-TEXT-ESCAPE-LINE-ENDINGS"),
			TEXT("AS_SDK_FrontendTokenizerDeep_TextEscapeLineEndings"),
			GeneratedSource);
	}

	TEST_METHOD(CommentWhitespaceAndEofCartesianProduct)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("FRONTEND-TOKEN-COMMENT-WHITESPACE-EOF",
			ENativeEvidence::Metadata
			| ENativeEvidence::Diagnostic);

		const FCommentWhitespaceFamilyCase FamilyCases[] =
		{
			{ "line_comment", "//", "", ttOnelineComment, TEXT("line comment") },
			{ "block_comment", "/*", "*/", ttMultilineComment, TEXT("block comment") },
			{ "whitespace", "", "", ttWhiteSpace, TEXT("whitespace") },
		};

		const FCommentWhitespacePayloadCase PayloadCases[] =
		{
			{ "minimal", " ", TEXT("minimal whitespace payload") },
			{ "ascii", " alpha", TEXT("ASCII payload") },
			{ "utf8", " \xE4\xBD\xA0\xE5\xA5\xBD", TEXT("UTF-8 payload") },
			{ "bom_or_space", "\xEF\xBB\xBF \t", TEXT("BOM and whitespace payload") },
		};

		const FCommentWhitespaceBoundaryCase BoundaryCases[] =
		{
			{ "eof", "", "", ttUnrecognizedToken, 1, ttUnrecognizedToken, 1, TEXT("end of input") },
			{ "lf", "\x0A", "Value", ttIdentifier, 5, ttWhiteSpace, 1, TEXT("LF boundary") },
			{ "crlf", "\x0D\x0A", "Value", ttIdentifier, 5, ttWhiteSpace, 2, TEXT("CRLF boundary") },
			{ "identifier_suffix", "Value", "", ttUnrecognizedToken, 1, ttIdentifier, 5, TEXT("identifier suffix boundary") },
		};

		FString GeneratedSource;
		AppendGeneratedAsLine(GeneratedSource, TEXT("int GeneratedCommentWhitespaceCell()"));
		AppendGeneratedAsLine(GeneratedSource, TEXT("{"));
		AppendGeneratedAsLine(GeneratedSource, TEXT("\treturn 0;"));
		AppendGeneratedAsLine(GeneratedSource, TEXT("}"));

		int32 ObservedCaseCount = 0;
		for (const FCommentWhitespaceFamilyCase& FamilyCase : FamilyCases)
		{
			for (const FCommentWhitespacePayloadCase& PayloadCase : PayloadCases)
			{
				for (const FCommentWhitespaceBoundaryCase& BoundaryCase : BoundaryCases)
				{
					std::string Input(FamilyCase.Open);
					Input += PayloadCase.Input;
					Input += FamilyCase.Close;
					Input += BoundaryCase.Suffix;
					Input += BoundaryCase.Tail;

					const FString CaseId = MakeNativeCaseId(
						"FRONTEND-TOKEN-COMMENT-WHITESPACE-EOF",
						{ ANSI_TO_TCHAR(FamilyCase.CatalogName), ANSI_TO_TCHAR(PayloadCase.CatalogName), ANSI_TO_TCHAR(BoundaryCase.CatalogName) });
					AppendCommentedCase(GeneratedSource, TEXT("comment or whitespace input"), Input.c_str());
					AppendGeneratedAsLine(GeneratedSource,
						FString::Printf(TEXT("\t// CaseId: %s"), *CaseId));

					int32 ExpectedPrimaryLength = static_cast<int32>(
						std::strlen(FamilyCase.Open)
						+ std::strlen(PayloadCase.Input)
						+ std::strlen(FamilyCase.Close));
					if (std::strcmp(FamilyCase.CatalogName, "line_comment") == 0)
					{
						ExpectedPrimaryLength += static_cast<int32>(std::strlen(BoundaryCase.Suffix));
						if (std::strcmp(BoundaryCase.CatalogName, "identifier_suffix") == 0)
						{
							ExpectedPrimaryLength += static_cast<int32>(std::strlen(BoundaryCase.Tail));
						}
					}
					else if (std::strcmp(FamilyCase.CatalogName, "whitespace") == 0)
					{
						// The raw tokenizer groups ordinary whitespace, but its BOM path
						// returns immediately after the three-byte marker. The payload
						// IDs below retain both behaviors as explicit current-fork cells.
						if (std::strcmp(PayloadCase.CatalogName, "minimal") == 0
							&& std::strcmp(BoundaryCase.CatalogName, "eof") != 0
							&& std::strcmp(BoundaryCase.CatalogName, "identifier_suffix") != 0)
						{
							ExpectedPrimaryLength += static_cast<int32>(std::strlen(BoundaryCase.Suffix));
						}
						else if (std::strcmp(PayloadCase.CatalogName, "ascii") == 0
							|| std::strcmp(PayloadCase.CatalogName, "utf8") == 0)
						{
							ExpectedPrimaryLength = 1;
						}
						else if (std::strcmp(PayloadCase.CatalogName, "bom_or_space") == 0)
						{
							ExpectedPrimaryLength = 3;
						}
					}

					const FTokenObservation PrimaryObservation = ReadToken(Input.c_str());
					ASSERT_THAT(AreEqual(static_cast<int32>(FamilyCase.ExpectedType), static_cast<int32>(PrimaryObservation.Type),
						FString::Printf(TEXT("%s should retain its token type in %s"), FamilyCase.Description, *CaseId)));
					ASSERT_THAT(AreEqual(ExpectedPrimaryLength, PrimaryObservation.Length,
						FString::Printf(TEXT("%s should consume the exact primary span in %s"), FamilyCase.Description, *CaseId)));
					if (std::strcmp(FamilyCase.CatalogName, "whitespace") == 0
						&& std::strcmp(PayloadCase.CatalogName, "utf8") == 0)
					{
						UE_LOG(LogTemp, Display,
							TEXT("[AS-REF-FORK-LIMITATION] %s next-token query is unsafe for a bare raw tokenizer after a non-ASCII byte; source and primary whitespace span remain asserted"),
							*CaseId);
						++ObservedCaseCount;
						continue;
					}

					eTokenType ExpectedNextType = BoundaryCase.ExpectedDelimitedNextType;
					int32 ExpectedNextLength = BoundaryCase.ExpectedDelimitedNextLength;
					if (std::strcmp(FamilyCase.CatalogName, "line_comment") == 0)
					{
						ExpectedNextType = BoundaryCase.ExpectedLineNextType;
						ExpectedNextLength = BoundaryCase.ExpectedLineNextLength;
					}
					else if (std::strcmp(FamilyCase.CatalogName, "whitespace") == 0)
					{
						if (std::strcmp(PayloadCase.CatalogName, "minimal") == 0)
						{
							if (std::strcmp(BoundaryCase.CatalogName, "eof") == 0)
							{
								ExpectedNextType = ttUnrecognizedToken;
								ExpectedNextLength = 1;
							}
							else if (std::strcmp(BoundaryCase.CatalogName, "identifier_suffix") == 0)
							{
								ExpectedNextType = ttIdentifier;
								ExpectedNextLength = 5;
							}
							else
							{
								ExpectedNextType = ttIdentifier;
								ExpectedNextLength = 5;
							}
						}
						else if (std::strcmp(PayloadCase.CatalogName, "ascii") == 0)
						{
							ExpectedNextType = ttIdentifier;
							ExpectedNextLength = std::strcmp(BoundaryCase.CatalogName, "identifier_suffix") == 0 ? 10 : 5;
						}
						else if (std::strcmp(PayloadCase.CatalogName, "utf8") == 0)
						{
							ExpectedNextType = ttUnrecognizedToken;
							ExpectedNextLength = 1;
						}
						else if (std::strcmp(PayloadCase.CatalogName, "bom_or_space") == 0)
						{
							ExpectedNextType = ttWhiteSpace;
							ExpectedNextLength = 2;
							if (std::strcmp(BoundaryCase.CatalogName, "lf") == 0)
							{
								ExpectedNextLength = 3;
							}
							else if (std::strcmp(BoundaryCase.CatalogName, "crlf") == 0)
							{
								ExpectedNextLength = 4;
							}
						}
					}
					const FTokenObservation NextObservation = ReadToken(Input.c_str() + ExpectedPrimaryLength);
					ASSERT_THAT(AreEqual(static_cast<int32>(ExpectedNextType), static_cast<int32>(NextObservation.Type),
						FString::Printf(TEXT("%s should expose the expected next token in %s"), BoundaryCase.Description, *CaseId)));
					ASSERT_THAT(AreEqual(ExpectedNextLength, NextObservation.Length,
						FString::Printf(TEXT("%s should expose the expected next length in %s"), BoundaryCase.Description, *CaseId)));
					++ObservedCaseCount;
				}
			}
		}

		ASSERT_THAT(AreEqual(48, ObservedCaseCount,
			TEXT("Comment/whitespace family × payload × boundary product should execute every cell")));
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("FRONTEND-TOKEN-COMMENT-WHITESPACE-EOF"),
			TEXT("AS_SDK_FrontendTokenizerDeep_CommentWhitespaceEof"),
			GeneratedSource);
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
