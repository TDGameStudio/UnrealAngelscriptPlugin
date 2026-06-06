// Copyright Epic Games, Inc. All Rights Reserved.

#include "Examples/LearningTraceExampleRegistry.h"

namespace AngelscriptEditor::LearningTrace
{
	namespace
	{
		FLearningTraceExample MakeExample(
			const TCHAR* Id,
			const TCHAR* Title,
			const TCHAR* Focus,
			const TCHAR* Source,
			std::initializer_list<const TCHAR*> ExpectedTopics)
		{
			FLearningTraceExample Example;
			Example.Id = Id;
			Example.Title = Title;
			Example.Focus = Focus;
			Example.Source = Source;
			for (const TCHAR* Topic : ExpectedTopics)
			{
				Example.ExpectedTopics.Add(Topic);
			}
			return Example;
		}
	}

	const TArray<FLearningTraceExample>& GetCuratedLearningTraceExamples()
	{
		static const TArray<FLearningTraceExample> Examples = {
			MakeExample(
				TEXT("simple-int"),
				TEXT("Simple integer literal"),
				TEXT("Keyword + identifier + integer constant + operator chain."),
				TEXT("int x = 42;"),
				{ TEXT("keyword"), TEXT("identifier"), TEXT("constant"), TEXT("operator") }),
			MakeExample(
				TEXT("string-with-escapes"),
				TEXT("String with escape sequences"),
				TEXT("Tokenizer keeps escape sequences inside one string token."),
				TEXT("string s = \"hello\\n\\tworld\";"),
				{ TEXT("string"), TEXT("escape") }),
			MakeExample(
				TEXT("single-line-comment"),
				TEXT("One-line comment then code"),
				TEXT("Tokenizer consumes the trailing-newline single-line comment as one token."),
				TEXT("// trailing comment\nint x;"),
				{ TEXT("comment-oneline"), TEXT("keyword"), TEXT("identifier") }),
			MakeExample(
				TEXT("multi-line-comment"),
				TEXT("Multi-line comment spanning lines"),
				TEXT("Block comments are one token regardless of line count."),
				TEXT("/* spans\nlines */ int y;"),
				{ TEXT("comment-multiline"), TEXT("keyword"), TEXT("identifier") }),
			MakeExample(
				TEXT("keywords-vs-identifiers"),
				TEXT("Keywords vs identifiers"),
				TEXT("`return_value` is a valid identifier; `return` and `class` are keywords. Tokenizer disambiguates greedily."),
				TEXT("int return_value = 0; class MyClass {}"),
				{ TEXT("keyword"), TEXT("identifier") }),
			MakeExample(
				TEXT("operators-and-arithmetic"),
				TEXT("Arithmetic operators"),
				TEXT("Tokenizer emits one token per operator, including multi-char sequences."),
				TEXT("int z = a + b * c / d % e;"),
				{ TEXT("operator"), TEXT("identifier"), TEXT("keyword") }),
			MakeExample(
				TEXT("based-numeric-literals"),
				TEXT("Numeric literals in multiple radices"),
				TEXT("Binary, octal, decimal, hex prefixes plus float exponent."),
				TEXT("int b = 0b1010; int o = 0o755; int x = 0xFF; double f = 1.5e3;"),
				{ TEXT("constant-bits"), TEXT("constant-int"), TEXT("constant-float") }),
			MakeExample(
				TEXT("nested-block-comment"),
				TEXT("Nested-looking block comment"),
				TEXT("AS does NOT support nested block comments — the inner '*/' closes the outer one. Useful teaching contrast."),
				TEXT("/* outer /* inner */ still-outer */"),
				{ TEXT("comment-multiline"), TEXT("operator") }),
			MakeExample(
				TEXT("unclosed-string"),
				TEXT("Unclosed string literal (error)"),
				TEXT("Tokenizer emits a non-terminated string constant — error reporting belongs to the parser."),
				TEXT("string s = \"oops"),
				{ TEXT("string"), TEXT("error-recovery") }),
			MakeExample(
				TEXT("whitespace-edges"),
				TEXT("Whitespace edges"),
				TEXT("Leading, trailing, and mixed-line whitespace flow through the tokenizer as discrete whitespace tokens."),
				TEXT("\n\n  int   x  ;\t\n  "),
				{ TEXT("whitespace"), TEXT("keyword"), TEXT("identifier") }),
		};
		return Examples;
	}
}
