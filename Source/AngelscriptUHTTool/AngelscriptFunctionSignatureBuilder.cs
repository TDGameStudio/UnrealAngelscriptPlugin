using System;
using System.Collections.Generic;
using System.Text;
using EpicGames.UHT.Types;

namespace AngelscriptUHTTool;

internal sealed record AngelscriptFunctionSignature(
	string OwningType,
	string FunctionName,
	string ReturnType,
	IReadOnlyList<string> ParameterTypes,
	bool IsStatic,
	bool IsConst,
	bool UseExplicitSignature)
{
	public string BuildEraseMacro()
	{
		if (UseExplicitSignature)
		{
			string parameterPack = ParameterTypes.Count == 0
				? "()"
				: $"({string.Join(", ", ParameterTypes)})";

			if (IsConst && !IsStatic)
			{
				parameterPack += " const";
			}

			return IsStatic
				? $"ERASE_FUNCTION_PTR({OwningType}::{FunctionName}, {parameterPack}, ERASE_ARGUMENT_PACK({ReturnType}))"
				: $"ERASE_METHOD_PTR({OwningType}, {FunctionName}, {parameterPack}, ERASE_ARGUMENT_PACK({ReturnType}))";
		}

		return IsStatic
			? $"ERASE_AUTO_FUNCTION_PTR({OwningType}::{FunctionName})"
			: $"ERASE_AUTO_METHOD_PTR({OwningType}, {FunctionName})";
	}
}

internal static class AngelscriptFunctionSignatureBuilder
{
	public static bool TryBuild(UhtClass classObj, UhtFunction function, out AngelscriptFunctionSignature? signature, out string? failureReason)
	{
		signature = null;

		if (AngelscriptHeaderSignatureResolver.TryBuild(classObj, function, out signature, out failureReason))
		{
			return true;
		}

		if (failureReason == "non-public" || failureReason == "unexported-symbol")
		{
			return false;
		}

		if (failureReason == "overloaded-unresolved" && !IsExplicitSignatureFallbackAllowed(classObj, function))
		{
			return false;
		}

		if (function.FunctionType != UhtFunctionType.Function)
		{
			failureReason = "non-function";
			return false;
		}

		List<string> parameterTypes = new();
		foreach (UhtType parameterType in function.ParameterProperties.Span)
		{
			if (parameterType is not UhtProperty property)
			{
				failureReason = "non-property-parameter";
				return false;
			}

			if (property.ArrayDimensions != null)
			{
				failureReason = "static-array-parameter";
				return false;
			}

			parameterTypes.Add(BuildParameterType(property));
		}

		string returnType = function.ReturnProperty is UhtProperty returnProperty
			? BuildReturnType(returnProperty)
			: "void";

		signature = new AngelscriptFunctionSignature(
			classObj.SourceName,
			function.SourceName,
			returnType,
			parameterTypes,
			HasFunctionFlag(function, "Static"),
			HasFunctionFlag(function, "Const"),
			true);

		failureReason = null;
		return true;
	}

	private static bool IsExplicitSignatureFallbackAllowed(UhtClass classObj, UhtFunction function)
	{
		return classObj.SourceName == "URuntimeFloatCurveMixinLibrary" &&
			(function.SourceName == "GetNumKeys" || function.SourceName == "GetTimeRange");
	}

	private static string BuildParameterType(UhtProperty property)
	{
		StringBuilder builder = new();
		property.AppendFullDecl(builder, UhtPropertyTextType.ClassFunctionArgOrRetVal, true);
		return builder.ToString().Trim();
	}

	private static string BuildReturnType(UhtProperty property)
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

	private static bool HasFunctionFlag(UhtFunction function, string flagName)
	{
		return function.FunctionFlags.ToString().Contains(flagName, StringComparison.Ordinal);
	}
}
