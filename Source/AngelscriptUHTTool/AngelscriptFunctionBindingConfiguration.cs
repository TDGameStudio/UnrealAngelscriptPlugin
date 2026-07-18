using System;
using System.Collections.Generic;
using System.IO;

namespace AngelscriptUHTTool;

internal sealed record AngelscriptFunctionBindingSettings(
	AngelscriptFunctionBindingMethod FunctionBindingMethod,
	HashSet<string> NativeRuntimeLinkedModules,
	HashSet<string> NativeModuleFunctionAddressModules);

internal static class AngelscriptFunctionBindingConfiguration
{
	internal const string SectionName = "/Script/AngelscriptRuntime.AngelscriptCompileOptions";
	internal const string FunctionBindingMethodKey = "FunctionBindingMethod";
	internal const string NativeRuntimeLinkedModulesKey = "NativeRuntimeLinkedModules";
	internal const string NativeModuleFunctionAddressModulesKey = "NativeModuleFunctionAddressModules";

	internal static AngelscriptFunctionBindingSettings ReadFile(string configPath)
	{
		AngelscriptFunctionBindingMethod method = AngelscriptFunctionBindingMethod.NativeRuntimeLinked;
		HashSet<string> runtimeLinkedModules = new(StringComparer.OrdinalIgnoreCase);
		HashSet<string> nativeModuleFunctionAddressModules = new(StringComparer.OrdinalIgnoreCase);
		bool inSection = false;

		foreach (string rawLine in File.ReadAllLines(configPath))
		{
			string line = rawLine.Trim();
			if (line.Length == 0 || line.StartsWith(';') || line.StartsWith('#'))
			{
				continue;
			}

			if (line.StartsWith('[') && line.EndsWith(']'))
			{
				inSection = string.Equals(line[1..^1], SectionName, StringComparison.Ordinal);
				continue;
			}

			if (!inSection)
			{
				continue;
			}

			int separatorIndex = line.IndexOf('=');
			if (separatorIndex <= 0)
			{
				continue;
			}

			string rawKey = line[..separatorIndex].Trim();
			char operation = rawKey.Length > 0 && (rawKey[0] == '+' || rawKey[0] == '-' || rawKey[0] == '!') ? rawKey[0] : '\0';
			string key = operation == '\0' ? rawKey : rawKey[1..].Trim();
			string value = line[(separatorIndex + 1)..].Trim();

			if (key.Equals(FunctionBindingMethodKey, StringComparison.OrdinalIgnoreCase))
			{
				if (operation != '\0')
				{
					throw new InvalidDataException($"{FunctionBindingMethodKey} does not support array operation '{operation}' in {configPath}.");
				}
				method = ParseMethod(value, configPath);
			}
			else if (key.Equals(NativeRuntimeLinkedModulesKey, StringComparison.OrdinalIgnoreCase))
			{
				ApplyArrayOperation(runtimeLinkedModules, operation, value, configPath, key);
			}
			else if (key.Equals(NativeModuleFunctionAddressModulesKey, StringComparison.OrdinalIgnoreCase))
			{
				ApplyArrayOperation(nativeModuleFunctionAddressModules, operation, value, configPath, key);
			}
		}

		return new AngelscriptFunctionBindingSettings(method, runtimeLinkedModules, nativeModuleFunctionAddressModules);
	}

	private static AngelscriptFunctionBindingMethod ParseMethod(string value, string configPath)
	{
		return value switch
		{
			"None" => AngelscriptFunctionBindingMethod.None,
			"NativeRuntimeLinked" => AngelscriptFunctionBindingMethod.NativeRuntimeLinked,
			"NativeModuleFunctionAddress" => AngelscriptFunctionBindingMethod.NativeModuleFunctionAddress,
			_ => throw new InvalidDataException($"Unknown FunctionBindingMethod '{value}' in {configPath}.")
		};
	}

	private static void ApplyArrayOperation(HashSet<string> modules, char operation, string value, string configPath, string key)
	{
		if (operation == '!')
		{
			if (value.Length > 0 && !value.Equals("ClearArray", StringComparison.OrdinalIgnoreCase))
			{
				throw new InvalidDataException($"{key} uses '!'; expected an empty value or ClearArray in {configPath}.");
			}
			modules.Clear();
			return;
		}

		if (value.Length == 0)
		{
			throw new InvalidDataException($"{key} contains an empty module name in {configPath}.");
		}

		switch (operation)
		{
			case '+':
				modules.Add(value);
				break;
			case '-':
				modules.Remove(value);
				break;
			default:
				modules.Clear();
				modules.Add(value);
				break;
		}
	}
}
