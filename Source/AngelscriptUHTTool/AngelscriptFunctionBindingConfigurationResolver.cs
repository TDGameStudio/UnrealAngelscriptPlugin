using System;
using System.Collections.Generic;
using System.IO;
using EpicGames.UHT.Tables;
using EpicGames.UHT.Types;
using EpicGames.UHT.Utils;

namespace AngelscriptUHTTool;

internal static class AngelscriptFunctionBindingConfigurationResolver
{
	internal static IEnumerable<string> EnumerateCompileOptionsCandidates(string runtimeBuildCsPath)
	{
		DirectoryInfo? currentDirectory = Directory.GetParent(runtimeBuildCsPath);
		for (int attempt = 0; attempt < 8 && currentDirectory != null; attempt++)
		{
			yield return Path.Combine(currentDirectory.FullName, "Config", "DefaultAngelscriptCompileOptions.ini");
			currentDirectory = currentDirectory.Parent;
		}
	}

	internal static string ResolveRuntimeBuildCsPath(IUhtExportFactory factory)
	{
		foreach (UhtModule module in factory.Session.Modules)
		{
			if (!module.ShortName.Equals("AngelscriptRuntime", StringComparison.Ordinal))
			{
				continue;
			}

			if (TryFindFirstHeaderPath(module.ScriptPackage, out string? headerPath) && headerPath != null)
			{
				string normalizedPath = headerPath.Replace('\\', '/');
				const string marker = "/Source/AngelscriptRuntime/";
				int markerIndex = normalizedPath.IndexOf(marker, StringComparison.OrdinalIgnoreCase);
				if (markerIndex >= 0)
				{
					return Path.Combine(normalizedPath[..(markerIndex + marker.Length - 1)], "AngelscriptRuntime.Build.cs");
				}
			}
		}

		throw new InvalidOperationException("Unable to locate AngelscriptRuntime.Build.cs from UHT session modules.");
	}

	internal static bool TryFindFirstHeaderPath(UhtType type, out string? headerPath)
	{
		if (type is UhtClass classObj && classObj.HeaderFile != null)
		{
			headerPath = classObj.HeaderFile.FilePath;
			return true;
		}

		foreach (UhtType child in type.Children)
		{
			if (TryFindFirstHeaderPath(child, out headerPath))
			{
				return true;
			}
		}

		headerPath = null;
		return false;
	}

	internal static string? ResolveEngineDirectory(IUhtExportFactory factory)
	{
		foreach (UhtModule module in factory.Session.Modules)
		{
			if (TryFindFirstHeaderPath(module.ScriptPackage, out string? headerPath) && TryExtractEngineDirectory(headerPath, out string? engineDirectory))
			{
				return engineDirectory;
			}
		}

		return null;
	}

	internal static bool TryExtractEngineDirectory(string? path, out string? engineDirectory)
	{
		engineDirectory = null;
		if (string.IsNullOrEmpty(path))
		{
			return false;
		}

		string normalizedPath = path.Replace('\\', '/');
		foreach (string marker in new[] { "/Engine/Source/", "/Engine/Plugins/" })
		{
			int markerIndex = normalizedPath.IndexOf(marker, StringComparison.OrdinalIgnoreCase);
			if (markerIndex >= 0)
			{
				engineDirectory = Path.GetFullPath(normalizedPath[..(markerIndex + "/Engine".Length)]);
				return true;
			}
		}

		return false;
	}

	internal static string ClassifyEngineDistribution(string? engineDirectory)
	{
		if (string.IsNullOrEmpty(engineDirectory))
		{
			return "unknown";
		}

		string normalizedDirectory = engineDirectory.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
		if (File.Exists(Path.Combine(normalizedDirectory, "Build", "InstalledBuild.txt")))
		{
			return "installed";
		}
		if (File.Exists(Path.Combine(normalizedDirectory, "Build", "SourceDistribution.txt")))
		{
			return "source";
		}

		for (DirectoryInfo? directory = new DirectoryInfo(normalizedDirectory); directory != null; directory = directory.Parent)
		{
			if (File.Exists(Path.Combine(directory.FullName, ".git")) || Directory.Exists(Path.Combine(directory.FullName, ".git")))
			{
				return "source";
			}
		}

		return "unknown";
	}
}
