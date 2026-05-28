using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;
using EpicGames.UHT.Types;

namespace AngelscriptUHTTool;

internal static class AngelscriptHeaderSignatureResolver
{
	private sealed record CandidateDeclaration(string Declaration, int NameIndex, bool IsPublic);

	private static readonly Dictionary<string, string> SanitizedHeaderCache = new(StringComparer.OrdinalIgnoreCase);
	private static readonly Regex MacroInvocationPattern = new(@"\b[A-Z_][A-Z0-9_]*\s*\([^;{}]*\)", RegexOptions.Compiled);
	private static readonly Regex ApiMacroPattern = new(@"\b(?:[A-Z_][A-Z0-9_]*_API|UE_API|RequiredAPI)\b", RegexOptions.Compiled);

	public static bool TryBuild(UhtClass classObj, UhtFunction function, out AngelscriptFunctionSignature? signature, out string? failureReason)
	{
		signature = null;

		if (classObj.HeaderFile == null || string.IsNullOrEmpty(classObj.HeaderFile.FilePath) || !File.Exists(classObj.HeaderFile.FilePath))
		{
			failureReason = "header-missing";
			return false;
		}

		string header = GetSanitizedHeader(classObj.HeaderFile.FilePath);
		if (!TryFindClassBody(header, classObj.SourceName, out int classBodyStart, out int classBodyEnd, out string classDeclaration))
		{
			failureReason = "class-range";
			return false;
		}

		List<CandidateDeclaration> candidates = FindCandidates(header, classBodyStart, classBodyEnd, function.SourceName);
		if (candidates.Count == 0)
		{
			failureReason = "declaration-missing";
			return false;
		}

		List<CandidateDeclaration> publicCandidates = candidates.FindAll(static candidate => candidate.IsPublic);
		if (publicCandidates.Count == 0)
		{
			failureReason = "non-public";
			return false;
		}

		if (candidates.Count == 1 && publicCandidates.Count == 1)
		{
			CandidateDeclaration candidate = publicCandidates[0];
			if (!HasLinkableExport(classObj, classDeclaration, candidate.Declaration))
			{
				failureReason = "unexported-symbol";
				return false;
			}

			signature = new AngelscriptFunctionSignature(
				classObj.SourceName,
				function.SourceName,
				string.Empty,
				Array.Empty<string>(),
				function.FunctionFlags.ToString().Contains("Static", StringComparison.Ordinal),
				function.FunctionFlags.ToString().Contains("Const", StringComparison.Ordinal),
				false);
			failureReason = null;
			return true;
		}

		List<string> expectedParameterTypes = BuildExpectedParameterTypes(function);
		string expectedReturnType = function.ReturnProperty is UhtProperty returnProperty
			? BuildExpectedReturnType(returnProperty)
			: "void";

		List<AngelscriptFunctionSignature> exactMatches = new();
		bool matchedUnexportedSymbol = false;
		foreach (CandidateDeclaration candidate in publicCandidates)
		{
			if (!HasLinkableExport(classObj, classDeclaration, candidate.Declaration))
			{
				matchedUnexportedSymbol = true;
				continue;
			}

			if (!TryParseDeclaration(classObj, function, candidate.Declaration, true, out AngelscriptFunctionSignature? parsedSignature, out _))
			{
				continue;
			}

			if (parsedSignature!.ParameterTypes.Count == expectedParameterTypes.Count &&
				AreTypesEquivalent(expectedParameterTypes, parsedSignature.ParameterTypes) &&
				NormalizeTypeText(expectedReturnType) == NormalizeTypeText(parsedSignature.ReturnType))
			{
				exactMatches.Add(parsedSignature);
			}
		}

		if (exactMatches.Count == 1)
		{
			signature = exactMatches[0];
			failureReason = null;
			return true;
		}

		failureReason = matchedUnexportedSymbol ? "unexported-symbol" : "overloaded-unresolved";
		return false;
	}

	public static bool TryBuildCrossModule(UhtClass classObj, UhtFunction function, out AngelscriptFunctionSignature? signature, out string? failureReason)
	{
		signature = null;

		if (classObj.HeaderFile == null || string.IsNullOrEmpty(classObj.HeaderFile.FilePath) || !File.Exists(classObj.HeaderFile.FilePath))
		{
			failureReason = "header-missing";
			return false;
		}

		string header = GetSanitizedHeader(classObj.HeaderFile.FilePath);
		if (!TryFindClassBody(header, classObj.SourceName, out int classBodyStart, out int classBodyEnd, out _))
		{
			failureReason = "class-range";
			return false;
		}

		List<CandidateDeclaration> candidates = FindCandidates(header, classBodyStart, classBodyEnd, function.SourceName);
		if (candidates.Count == 0)
		{
			failureReason = "declaration-missing";
			return false;
		}

		List<CandidateDeclaration> publicCandidates = candidates.FindAll(static candidate => candidate.IsPublic);
		if (publicCandidates.Count == 0)
		{
			failureReason = "non-public";
			return false;
		}

		List<string> expectedParameterTypes = BuildExpectedParameterTypes(function);
		string expectedReturnType = function.ReturnProperty is UhtProperty returnProperty
			? BuildExpectedReturnType(returnProperty)
			: "void";

		List<AngelscriptFunctionSignature> exactMatches = new();
		foreach (CandidateDeclaration candidate in publicCandidates)
		{
			if (!TryParseDeclaration(classObj, function, candidate.Declaration, true, out AngelscriptFunctionSignature? parsedSignature, out _))
			{
				continue;
			}

			if (parsedSignature!.ParameterTypes.Count == expectedParameterTypes.Count &&
				AreTypesEquivalent(expectedParameterTypes, parsedSignature.ParameterTypes) &&
				NormalizeTypeText(expectedReturnType) == NormalizeTypeText(parsedSignature.ReturnType))
			{
				exactMatches.Add(parsedSignature);
			}
		}

		if (exactMatches.Count == 1)
		{
			signature = exactMatches[0];
			failureReason = null;
			return true;
		}

		failureReason = exactMatches.Count > 1 ? "overloaded-ambiguous" : "cross-module-signature-unresolved";
		return false;
	}

	private static bool HasLinkableExport(UhtClass classObj, string classDeclaration, string declaration)
	{
		if (classObj.HeaderFile?.Module?.ShortName.Equals("AngelscriptRuntime", StringComparison.OrdinalIgnoreCase) == true)
		{
			return true;
		}

		return IsLinkVisible(classDeclaration, declaration);
	}

	private static List<string> BuildExpectedParameterTypes(UhtFunction function)
	{
		List<string> types = new();
		foreach (UhtType parameterType in function.ParameterProperties.Span)
		{
			if (parameterType is UhtProperty property)
			{
				types.Add(BuildExpectedParameterType(property));
			}
		}
		return types;
	}

	private static string BuildExpectedParameterType(UhtProperty property)
	{
		StringBuilder builder = new();
		property.AppendFullDecl(builder, UhtPropertyTextType.ClassFunctionArgOrRetVal, true);
		return builder.ToString().Trim();
	}

	private static string BuildExpectedReturnType(UhtProperty property)
	{
		string typeText = property.TypeTokens.ToString().Trim();
		string propertyFlags = property.PropertyFlags.ToString();

		if (propertyFlags.Contains("ConstParm", StringComparison.Ordinal) &&
			!typeText.StartsWith("const ", StringComparison.Ordinal))
		{
			typeText = "const " + typeText;
		}

		return typeText;
	}

	private static bool AreTypesEquivalent(IReadOnlyList<string> expectedTypes, IReadOnlyList<string> candidateTypes)
	{
		if (expectedTypes.Count != candidateTypes.Count)
		{
			return false;
		}

		for (int index = 0; index < expectedTypes.Count; index++)
		{
			if (NormalizeTypeText(expectedTypes[index]) != NormalizeTypeText(candidateTypes[index]))
			{
				return false;
			}
		}

		return true;
	}

	private static string NormalizeTypeText(string typeText)
	{
		return CollapseWhitespace(typeText)
			.Replace("const ", string.Empty, StringComparison.Ordinal)
			.Replace("&", string.Empty, StringComparison.Ordinal)
			.Replace(" ", string.Empty, StringComparison.Ordinal)
			.Trim();
	}

	private static string GetSanitizedHeader(string headerPath)
	{
		if (SanitizedHeaderCache.TryGetValue(headerPath, out string? cachedHeader))
		{
			return cachedHeader;
		}

		string header = File.ReadAllText(headerPath);
		StringBuilder builder = new(header.Length);
		bool inLineComment = false;
		bool inBlockComment = false;

		for (int index = 0; index < header.Length; index++)
		{
			char current = header[index];
			char next = index + 1 < header.Length ? header[index + 1] : '\0';

			if (!inLineComment && !inBlockComment && current == '/' && next == '/')
			{
				inLineComment = true;
				builder.Append(' ');
				index++;
				builder.Append(' ');
				continue;
			}

			if (!inLineComment && !inBlockComment && current == '/' && next == '*')
			{
				inBlockComment = true;
				builder.Append(' ');
				index++;
				builder.Append(' ');
				continue;
			}

			if (inLineComment)
			{
				if (current == '\n')
				{
					inLineComment = false;
					builder.Append('\n');
				}
				else
				{
					builder.Append(' ');
				}
				continue;
			}

			if (inBlockComment)
			{
				if (current == '*' && next == '/')
				{
					inBlockComment = false;
					builder.Append(' ');
					index++;
					builder.Append(' ');
				}
				else
				{
					builder.Append(current == '\n' ? '\n' : ' ');
				}
				continue;
			}

			builder.Append(current);
		}

		string sanitizedHeader = builder.ToString();
		SanitizedHeaderCache.Add(headerPath, sanitizedHeader);
		return sanitizedHeader;
	}

	private static bool TryFindClassBody(string header, string className, out int classBodyStart, out int classBodyEnd, out string classDeclaration)
	{
		foreach (string marker in new[] { "UCLASS(", "UINTERFACE(" })
		{
			int searchIndex = 0;
			while (searchIndex < header.Length)
			{
				int markerIndex = header.IndexOf(marker, searchIndex, StringComparison.Ordinal);
				if (markerIndex < 0)
				{
					break;
				}

				int braceIndex = header.IndexOf('{', markerIndex);
				if (braceIndex < 0)
				{
					break;
				}

				string declarationRegion = header.Substring(markerIndex, braceIndex - markerIndex);
				if (ContainsWholeWord(declarationRegion, className))
				{
					int classEnd = FindMatchingBrace(header, braceIndex);
					if (classEnd > braceIndex)
					{
						classBodyStart = braceIndex + 1;
						classBodyEnd = classEnd;
						classDeclaration = declarationRegion;
						return true;
					}
				}

				searchIndex = braceIndex + 1;
			}
		}

		classBodyStart = -1;
		classBodyEnd = -1;
		classDeclaration = string.Empty;
		return false;
	}

	private static bool IsLinkVisible(string classDeclaration, string declaration)
	{
		string normalizedDeclaration = StripLeadingMacroInvocations(declaration);
		int openParenIndex = normalizedDeclaration.IndexOf('(');
		string declarationPrefix = openParenIndex >= 0 ? normalizedDeclaration.Substring(0, openParenIndex) : normalizedDeclaration;
		bool functionHasApiMacro = ApiMacroPattern.IsMatch(declarationPrefix);
		bool classHasApiMacro = ApiMacroPattern.IsMatch(classDeclaration);
		bool classIsMinimalApi = classDeclaration.Contains("MinimalAPI", StringComparison.Ordinal);
		bool isInlineDefinition = declarationPrefix.Contains("inline ", StringComparison.Ordinal) ||
			declarationPrefix.Contains("FORCEINLINE", StringComparison.Ordinal) ||
			declarationPrefix.Contains("constexpr ", StringComparison.Ordinal) ||
			normalizedDeclaration.Contains('{', StringComparison.Ordinal);

		if (functionHasApiMacro || isInlineDefinition)
		{
			return true;
		}

		return classHasApiMacro && !classIsMinimalApi;
	}

	private static string StripLeadingMacroInvocations(string declaration)
	{
		string normalizedDeclaration = declaration.TrimStart();
		while (normalizedDeclaration.Length > 0)
		{
			int tokenEnd = 0;
			while (tokenEnd < normalizedDeclaration.Length &&
				(char.IsLetterOrDigit(normalizedDeclaration[tokenEnd]) || normalizedDeclaration[tokenEnd] == '_'))
			{
				tokenEnd++;
			}

			if (tokenEnd == 0)
			{
				break;
			}

			string token = normalizedDeclaration.Substring(0, tokenEnd);
			if (!Regex.IsMatch(token, "^[A-Z_][A-Z0-9_]*$", RegexOptions.CultureInvariant))
			{
				break;
			}

			int nextIndex = tokenEnd;
			while (nextIndex < normalizedDeclaration.Length && char.IsWhiteSpace(normalizedDeclaration[nextIndex]))
			{
				nextIndex++;
			}

			if (nextIndex >= normalizedDeclaration.Length || normalizedDeclaration[nextIndex] != '(')
			{
				break;
			}

			int closeParenIndex = FindMatchingParen(normalizedDeclaration, nextIndex);
			if (closeParenIndex < 0)
			{
				break;
			}

			normalizedDeclaration = normalizedDeclaration.Substring(closeParenIndex + 1).TrimStart();
		}

		return normalizedDeclaration;
	}

	private static List<CandidateDeclaration> FindCandidates(string header, int classBodyStart, int classBodyEnd, string functionName)
	{
		List<CandidateDeclaration> candidates = new();
		HashSet<string> seenDeclarations = new(StringComparer.Ordinal);
		int searchIndex = classBodyStart;
		string marker = functionName + "(";

		while (searchIndex < classBodyEnd)
		{
			int nameIndex = header.IndexOf(marker, searchIndex, StringComparison.Ordinal);
			if (nameIndex < 0 || nameIndex >= classBodyEnd)
			{
				break;
			}

			if (nameIndex > classBodyStart && IsWordChar(header[nameIndex - 1]))
			{
				searchIndex = nameIndex + marker.Length;
				continue;
			}

			int declarationStart = FindDeclarationStart(header, classBodyStart, nameIndex);
			int declarationEnd = FindDeclarationEnd(header, nameIndex, classBodyEnd);
			if (declarationEnd > declarationStart)
			{
				string declaration = header.Substring(declarationStart, declarationEnd - declarationStart).Trim();
				if (seenDeclarations.Add(declaration))
				{
					bool isPublic = FindAccessSpecifier(header, classBodyStart, nameIndex) == "public";
					candidates.Add(new CandidateDeclaration(declaration, nameIndex, isPublic));
				}
			}

			searchIndex = nameIndex + marker.Length;
		}

		return candidates;
	}

	private static int FindDeclarationStart(string header, int classBodyStart, int nameIndex)
	{
		for (int index = nameIndex - 1; index >= classBodyStart; index--)
		{
			char current = header[index];
			if (current == ';' || current == '{' || current == '}')
			{
				return index + 1;
			}
		}

		return classBodyStart;
	}

	private static int FindDeclarationEnd(string header, int nameIndex, int classBodyEnd)
	{
		int parenDepth = 0;
		for (int index = nameIndex; index < classBodyEnd; index++)
		{
			char current = header[index];
			if (current == '(')
			{
				parenDepth++;
			}
			else if (current == ')')
			{
				parenDepth = Math.Max(0, parenDepth - 1);
			}
			else if (current == ';' && parenDepth == 0)
			{
				return index;
			}
		}

		return classBodyEnd;
	}

	private static string FindAccessSpecifier(string header, int classBodyStart, int targetIndex)
	{
		string access = "private";
		for (int index = classBodyStart; index < targetIndex; index++)
		{
			if (MatchesLabel(header, index, "public:"))
			{
				access = "public";
			}
			else if (MatchesLabel(header, index, "protected:"))
			{
				access = "protected";
			}
			else if (MatchesLabel(header, index, "private:"))
			{
				access = "private";
			}
		}
		return access;
	}

	private static bool MatchesLabel(string header, int index, string label)
	{
		return index + label.Length <= header.Length &&
			header.AsSpan(index, label.Length).SequenceEqual(label.AsSpan());
	}

	private static bool TryParseDeclaration(UhtClass classObj, UhtFunction function, string declaration, bool useExplicitSignature, out AngelscriptFunctionSignature? signature, out string? failureReason)
	{
		signature = null;

		int functionNameIndex = declaration.IndexOf(function.SourceName + "(", StringComparison.Ordinal);
		if (functionNameIndex < 0)
		{
			failureReason = "function-name";
			return false;
		}

		int openParenIndex = functionNameIndex + function.SourceName.Length;
		int closeParenIndex = FindMatchingParen(declaration, openParenIndex);
		if (closeParenIndex < 0)
		{
			failureReason = "closing-paren";
			return false;
		}

		string prefix = declaration.Substring(0, functionNameIndex).Trim();
		bool isStatic = function.FunctionFlags.ToString().Contains("Static", StringComparison.Ordinal);
		string returnType = function.ReturnProperty is UhtProperty returnProperty
			? BuildReturnTypeFromTokens(returnProperty)
			: CleanReturnType(prefix);
		if (string.IsNullOrWhiteSpace(returnType))
		{
			failureReason = "return-type";
			return false;
		}

		List<string> parameterTypes = ParseParameterTypes(function, declaration.Substring(openParenIndex + 1, closeParenIndex - openParenIndex - 1));
		if (parameterTypes.Count != function.ParameterProperties.Span.Length)
		{
			failureReason = "parameter-count";
			return false;
		}

		bool isConst = function.FunctionFlags.ToString().Contains("Const", StringComparison.Ordinal);

		signature = new AngelscriptFunctionSignature(classObj.SourceName, function.SourceName, returnType, parameterTypes, isStatic, isConst, useExplicitSignature);
		failureReason = null;
		return true;
	}

	private static string CleanReturnType(string prefix)
	{
		string cleaned = CollapseWhitespace(prefix);
		cleaned = MacroInvocationPattern.Replace(cleaned, string.Empty);
		foreach (string token in new[] { "virtual ", "static ", "inline ", "FORCEINLINE ", "FORCENOINLINE ", "constexpr " })
		{
			cleaned = cleaned.Replace(token, string.Empty, StringComparison.Ordinal);
		}

		List<string> keptTokens = new();
		foreach (string token in cleaned.Split(' ', StringSplitOptions.RemoveEmptyEntries))
		{
			if (token.EndsWith("_API", StringComparison.Ordinal) || token == "UE_API" || token == "RequiredAPI")
			{
				continue;
			}
			keptTokens.Add(token);
		}

		return string.Join(' ', keptTokens).Trim();
	}

	private static string BuildReturnTypeFromTokens(UhtProperty property)
	{
		string typeText = property.TypeTokens.ToString().Trim();
		string propertyFlags = property.PropertyFlags.ToString();

		if (propertyFlags.Contains("ConstParm", StringComparison.Ordinal) &&
			!typeText.StartsWith("const ", StringComparison.Ordinal))
		{
			typeText = "const " + typeText;
		}

		return typeText;
	}

	private static List<string> ParseParameterTypes(UhtFunction function, string parameterSegment)
	{
		List<string> results = new();
		parameterSegment = parameterSegment.Trim();
		if (string.IsNullOrEmpty(parameterSegment) || parameterSegment == "void")
		{
			return results;
		}

		List<string> rawParameters = SplitTopLevel(parameterSegment);
		int parameterIndex = 0;
		foreach (string rawParameter in rawParameters)
		{
			string parameter = StripDefaultValue(StripLeadingUparam(rawParameter.Trim()));
			if (parameterIndex >= function.ParameterProperties.Span.Length || function.ParameterProperties.Span[parameterIndex] is not UhtProperty property)
			{
				break;
			}

			if (property.ArrayDimensions != null)
			{
				return new List<string>();
			}

			results.Add(StripTrailingIdentifier(parameter));
			parameterIndex++;
		}

		return results;
	}

	private static string StripTrailingIdentifier(string parameter)
	{
		parameter = parameter.Trim();
		int index = parameter.Length - 1;
		while (index >= 0 && char.IsWhiteSpace(parameter[index]))
		{
			index--;
		}

		int end = index;
		while (index >= 0 && (char.IsLetterOrDigit(parameter[index]) || parameter[index] == '_'))
		{
			index--;
		}

		if (end >= 0 && end > index)
		{
			string stripped = parameter.Substring(0, index + 1).TrimEnd();
			if (!string.IsNullOrEmpty(stripped))
			{
				return stripped;
			}
		}

		return parameter;
	}

	private static List<string> SplitTopLevel(string text)
	{
		List<string> parts = new();
		if (string.IsNullOrWhiteSpace(text))
		{
			return parts;
		}

		int start = 0;
		int angleDepth = 0;
		int parenDepth = 0;
		int braceDepth = 0;
		for (int index = 0; index < text.Length; index++)
		{
			switch (text[index])
			{
				case '<': angleDepth++; break;
				case '>': angleDepth = Math.Max(0, angleDepth - 1); break;
				case '(': parenDepth++; break;
				case ')': parenDepth = Math.Max(0, parenDepth - 1); break;
				case '{': braceDepth++; break;
				case '}': braceDepth = Math.Max(0, braceDepth - 1); break;
				case ',':
					if (angleDepth == 0 && parenDepth == 0 && braceDepth == 0)
					{
						parts.Add(text.Substring(start, index - start));
						start = index + 1;
					}
					break;
			}
		}
		parts.Add(text.Substring(start));
		return parts;
	}

	private static string StripDefaultValue(string parameter)
	{
		int angleDepth = 0;
		int parenDepth = 0;
		int braceDepth = 0;
		for (int index = 0; index < parameter.Length; index++)
		{
			switch (parameter[index])
			{
				case '<': angleDepth++; break;
				case '>': angleDepth = Math.Max(0, angleDepth - 1); break;
				case '(': parenDepth++; break;
				case ')': parenDepth = Math.Max(0, parenDepth - 1); break;
				case '{': braceDepth++; break;
				case '}': braceDepth = Math.Max(0, braceDepth - 1); break;
				case '=':
					if (angleDepth == 0 && parenDepth == 0 && braceDepth == 0)
					{
						return parameter.Substring(0, index).Trim();
					}
					break;
			}
		}
		return parameter.Trim();
	}

	private static string StripLeadingUparam(string parameter)
	{
		while (parameter.StartsWith("UPARAM(", StringComparison.Ordinal))
		{
			int closeParen = FindMatchingParen(parameter, parameter.IndexOf('('));
			if (closeParen < 0 || closeParen + 1 >= parameter.Length)
			{
				break;
			}
			parameter = parameter.Substring(closeParen + 1).Trim();
		}
		return parameter;
	}

	private static int FindMatchingParen(string text, int openParenIndex)
	{
		int depth = 0;
		for (int index = openParenIndex; index < text.Length; index++)
		{
			if (text[index] == '(')
			{
				depth++;
			}
			else if (text[index] == ')')
			{
				depth--;
				if (depth == 0)
				{
					return index;
				}
			}
		}
		return -1;
	}

	private static int FindMatchingBrace(string text, int openBraceIndex)
	{
		int depth = 0;
		for (int index = openBraceIndex; index < text.Length; index++)
		{
			if (text[index] == '{')
			{
				depth++;
			}
			else if (text[index] == '}')
			{
				depth--;
				if (depth == 0)
				{
					return index;
				}
			}
		}
		return -1;
	}

	private static bool ContainsWholeWord(string text, string word)
	{
		int searchIndex = 0;
		while (searchIndex < text.Length)
		{
			int matchIndex = text.IndexOf(word, searchIndex, StringComparison.Ordinal);
			if (matchIndex < 0)
			{
				return false;
			}

			bool startOk = matchIndex == 0 || !IsWordChar(text[matchIndex - 1]);
			int endIndex = matchIndex + word.Length;
			bool endOk = endIndex >= text.Length || !IsWordChar(text[endIndex]);
			if (startOk && endOk)
			{
				return true;
			}

			searchIndex = matchIndex + word.Length;
		}

		return false;
	}

	private static bool IsWordChar(char value)
	{
		return char.IsLetterOrDigit(value) || value == '_';
	}

	private static string CollapseWhitespace(string input)
	{
		StringBuilder builder = new(input.Length);
		bool lastWasWhitespace = false;
		foreach (char ch in input)
		{
			if (char.IsWhiteSpace(ch))
			{
				if (!lastWasWhitespace)
				{
					builder.Append(' ');
					lastWasWhitespace = true;
				}
			}
			else
			{
				builder.Append(ch);
				lastWasWhitespace = false;
			}
		}
		return builder.ToString().Trim();
	}
}
