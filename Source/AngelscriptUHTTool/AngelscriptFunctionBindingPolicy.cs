using System;
using System.Collections.Generic;
using EpicGames.Core;
using EpicGames.UHT.Tables;
using EpicGames.UHT.Types;

namespace AngelscriptUHTTool;

internal static class AngelscriptFunctionBindingPolicy
{
	private static readonly HashSet<(string ClassName, string FunctionName)> ExcludedFunctions = new()
	{
		("UUniversalObjectLocatorScriptingExtensions", "MakeUniversalObjectLocator"),
		("UUniversalObjectLocatorScriptingExtensions", "UniversalObjectLocatorFromString"),
	};

	internal static bool IsEligible(UhtClass classObj, UhtFunction function, bool supportedHeader)
	{
		if (!supportedHeader || !AngelscriptFunctionBindingExporter.IsAngelscriptCallable(function))
		{
			return false;
		}

		if (function.MetaData.ContainsKey("NotInAngelscript") ||
			(function.MetaData.ContainsKey("BlueprintInternalUseOnly") && !function.MetaData.ContainsKey("UsableInAngelscript")))
		{
			return false;
		}

		if (ExcludedFunctions.Contains((classObj.SourceName, function.SourceName)))
		{
			return false;
		}

		return !function.FunctionExportFlags.HasAnyFlags(UhtFunctionExportFlags.CustomThunk) &&
			!function.FunctionFlags.HasAnyFlags(EFunctionFlags.BlueprintEvent);
	}

	internal static string? GetRuntimeFallbackReason(UhtClass classObj, UhtFunction function)
	{
		if (classObj.ClassType is UhtClassType.Interface or UhtClassType.NativeInterface)
		{
			return "interface-function";
		}

		return IsRpcNetFunction(function) ? "rpc-net-function" : null;
	}

	internal static bool IsRpcNetFunction(UhtFunction function)
	{
		return function.FunctionFlags.HasAnyFlags(EFunctionFlags.Net | EFunctionFlags.NetServer | EFunctionFlags.NetClient | EFunctionFlags.NetMulticast);
	}

	internal static bool IsSafeNativeModuleFunctionAddressSignature(AngelscriptFunctionSignature signature, UhtClass classObj, UhtFunction function)
	{
		return IsSafeReturn(signature, function) && HasOnlySafeParameters(function) &&
			!HasOutParams(function) && !HasWorldContext(function) && !HasReturnReference(signature, function) &&
			!HasScriptMethodMixinProjection(signature, classObj, function);
	}

	internal static string ClassifyUnsupportedNativeModuleFunctionBindingSignature(AngelscriptFunctionSignature signature, UhtClass classObj, UhtFunction function)
	{
		if (HasWorldContext(function)) return "needs-world-context-policy";
		if (HasOutParams(function) || HasReferenceParameters(function)) return "needs-out-param-marshalling";
		if (HasReturnReference(signature, function)) return "needs-ref-return-marshalling";
		if (HasStaticArrayParameter(function) || ReturnsStaticArray(function)) return "needs-static-array-marshalling";
		if (HasContainerParameter(function) || ReturnsContainer(function)) return "needs-container-marshalling";
		if (HasInterfaceParameter(function) || ReturnsInterface(function)) return "needs-interface-marshalling";
		if (HasDelegateParameter(function) || ReturnsDelegate(function)) return "needs-delegate-marshalling";
		if (HasFieldPathParameter(function) || ReturnsFieldPath(function)) return "needs-field-path-marshalling";
		if (HasScriptMethodMixinProjection(signature, classObj, function)) return "needs-script-this-projection";
		return "native-module-function-binding-unsupported-signature";
	}

	private static bool HasScriptMethodMixinProjection(AngelscriptFunctionSignature signature, UhtClass classObj, UhtFunction function)
	{
		return signature.IsStatic && (function.MetaData.ContainsKey("ScriptMethod") || classObj.MetaData.ContainsKey("ScriptMixin"));
	}

	private static bool IsSafeReturn(AngelscriptFunctionSignature signature, UhtFunction function)
	{
		if (signature.ReturnType == "void") return true;
		if (function.ReturnProperty is not UhtProperty returnProperty) return false;
		return returnProperty is UhtBoolProperty or UhtNumericProperty or UhtEnumProperty or UhtStructProperty or UhtStrProperty or UhtNameProperty or UhtTextProperty ||
			returnProperty is UhtObjectProperty && signature.ReturnType.EndsWith("*", StringComparison.Ordinal);
	}

	private static bool HasOnlySafeParameters(UhtFunction function)
	{
		foreach (UhtType parameterType in function.ParameterProperties.Span)
		{
			if (parameterType is not UhtProperty property || !IsSafeParameter(property))
			{
				return false;
			}
		}
		return true;
	}

	private static bool IsSafeParameter(UhtProperty property)
	{
		return property.ArrayDimensions == null && (property is UhtBoolProperty or UhtNumericProperty or UhtEnumProperty or UhtStructProperty or UhtStrProperty or UhtNameProperty or UhtTextProperty or UhtObjectProperty or UhtClassProperty or UhtSoftObjectProperty or UhtWeakObjectPtrProperty);
	}

	internal static bool HasWorldContext(UhtFunction function) => function.MetaData.ContainsKey("WorldContext");
	internal static bool HasOutParams(UhtFunction function) => HasParameter(function, static type => type is UhtProperty property && property.PropertyFlags.HasAnyFlags(EPropertyFlags.OutParm));
	private static bool HasReferenceParameters(UhtFunction function) => HasParameter(function, static type => type is UhtProperty property && property.PropertyFlags.HasAnyFlags(EPropertyFlags.ReferenceParm));
	private static bool HasStaticArrayParameter(UhtFunction function) => HasParameter(function, static type => type is UhtProperty property && property.ArrayDimensions != null);
	private static bool ReturnsStaticArray(UhtFunction function) => function.ReturnProperty is UhtProperty property && property.ArrayDimensions != null;
	private static bool HasContainerParameter(UhtFunction function) => HasParameter(function, static type => type is UhtContainerBaseProperty);
	private static bool ReturnsContainer(UhtFunction function) => function.ReturnProperty is UhtContainerBaseProperty;
	private static bool HasInterfaceParameter(UhtFunction function) => HasParameter(function, static type => type is UhtInterfaceProperty);
	private static bool ReturnsInterface(UhtFunction function) => function.ReturnProperty is UhtInterfaceProperty;
	private static bool HasDelegateParameter(UhtFunction function) => HasParameter(function, static type => type is UhtDelegateProperty or UhtMulticastDelegateProperty);
	private static bool ReturnsDelegate(UhtFunction function) => function.ReturnProperty is UhtDelegateProperty or UhtMulticastDelegateProperty;
	private static bool HasFieldPathParameter(UhtFunction function) => HasParameter(function, static type => type is UhtFieldPathProperty);
	private static bool ReturnsFieldPath(UhtFunction function) => function.ReturnProperty is UhtFieldPathProperty;
	private static bool ReturnsByRef(UhtFunction function) => function.ReturnProperty is UhtProperty property && property.PropertyFlags.HasAnyFlags(EPropertyFlags.ReferenceParm);
	internal static bool HasReturnReference(AngelscriptFunctionSignature signature, UhtFunction function) => ReturnsByRef(function) || signature.ReturnType.Contains("&", StringComparison.Ordinal);

	private static bool HasParameter(UhtFunction function, Func<UhtType, bool> predicate)
	{
		foreach (UhtType parameterType in function.ParameterProperties.Span)
		{
			if (predicate(parameterType))
			{
				return true;
			}
		}
		return false;
	}
}
