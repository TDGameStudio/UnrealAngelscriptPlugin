using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using EpicGames.UHT.Utils;

namespace AngelscriptUHTTool;

internal static partial class AngelscriptFunctionBindingCodeGenerator
{
	private static void DeleteStaleOutputs(IUhtExportFactory factory, HashSet<string> generatedPaths, IReadOnlyDictionary<string, string> moduleOutputDirectories)
	{
		HashSet<string> livePaths = new(generatedPaths.Select(Path.GetFullPath), StringComparer.OrdinalIgnoreCase);
		string runtimeOutputDirectory = Path.GetDirectoryName(Path.GetFullPath(factory.MakePath("AS_FunctionBinding_Stale", ".gen.cpp")))!;
		DeleteStaleFiles(runtimeOutputDirectory, "AS_FunctionBinding_*.gen.cpp", livePaths);
		DeleteStaleFiles(runtimeOutputDirectory, "AS_FunctionBinding_*.cpp", livePaths);
		DeleteStaleFiles(runtimeOutputDirectory, "AS_FunctionTable_*.gen.cpp", livePaths);
		DeleteStaleFiles(runtimeOutputDirectory, "AS_FunctionTable_*.cpp", livePaths);
		DeleteStaleFiles(runtimeOutputDirectory, "AS_FunctionTable_*.json", livePaths);
		DeleteStaleFiles(runtimeOutputDirectory, "AS_FunctionTable_*.csv", livePaths);
		foreach ((string moduleName, string outputDirectory) in moduleOutputDirectories)
		{
			DeleteStaleFiles(outputDirectory, $"AS_FunctionBinding_{moduleName}_NativeModuleFunctionAddress_*.cpp", livePaths);
			DeleteStaleFiles(outputDirectory, $"AS_FunctionBinding_{moduleName}_CrossModule_*.cpp", livePaths);
			if (moduleName.Equals("Engine", StringComparison.OrdinalIgnoreCase))
			{
				DeleteStaleFiles(outputDirectory, "AS_FunctionBinding_Engine_NativeModuleFunctionBindingBridgeProbe.cpp", livePaths);
				DeleteStaleFiles(outputDirectory, "AS_FunctionTable_Engine_LinkProbe.cpp", livePaths);
				DeleteStaleFiles(outputDirectory, "AS_FunctionTable_Engine_ModuleBinding_LinkProbe.cpp", livePaths);
			}
		}
	}

	private static void DeleteStaleFiles(string outputDirectory, string pattern, HashSet<string> livePaths)
	{
		if (!Directory.Exists(outputDirectory))
		{
			Console.WriteLine("Warning: UHT output directory '{0}' is absent while reconciling '{1}'.", outputDirectory, pattern);
			return;
		}

		string fullOutputDirectory = Path.GetFullPath(outputDirectory);
		if (!fullOutputDirectory.Contains($"{Path.DirectorySeparatorChar}Intermediate{Path.DirectorySeparatorChar}", StringComparison.OrdinalIgnoreCase) &&
			!fullOutputDirectory.Contains($"{Path.AltDirectorySeparatorChar}Intermediate{Path.AltDirectorySeparatorChar}", StringComparison.OrdinalIgnoreCase))
		{
			throw new InvalidOperationException($"Refusing to clean non-UHT output directory '{fullOutputDirectory}'.");
		}

		foreach (string existingFile in Directory.EnumerateFiles(outputDirectory, pattern))
		{
			if (livePaths.Contains(Path.GetFullPath(existingFile)))
			{
				continue;
			}

			try
			{
				File.Delete(existingFile);
			}
			catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
			{
				throw new InvalidOperationException($"Unable to delete stale UHT FunctionBinding output '{existingFile}'.", exception);
			}
		}
	}
}
