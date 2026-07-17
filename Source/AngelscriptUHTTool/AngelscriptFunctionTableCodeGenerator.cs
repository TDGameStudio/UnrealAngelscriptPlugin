using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;
using EpicGames.Core;
using EpicGames.UHT.Types;
using EpicGames.UHT.Utils;

namespace AngelscriptUHTTool;

internal sealed record AngelscriptGeneratedFunctionBinding(
	string ClassName,
	string FunctionName,
	string EraseMacro,
	string EntryKind,
	string ThunkStyle,
	// Non-null when the bound UFUNCTION only exists in editor builds (declared inside
	// #if WITH_EDITOR / WITH_EDITORONLY_DATA). Holds the exact preprocessor macro the
	// registration line must be guarded with so non-editor (packaged) targets compile.
	string? EditorOnlyGuard = null)
{
	public string BuildBindingRegistrationLine()
	{
		return $"\tFAngelscriptBinds::RegisterFunctionBinding({ClassName}::StaticClass(), \"{FunctionName}\", {{ {EraseMacro} }});";
	}
}

internal sealed record AngelscriptCrossModuleFunctionBinding(
	string ModuleName,
	string ClassName,
	string FunctionName,
	string IncludePath,
	IReadOnlyList<string> IncludePaths,
	string ReturnType,
	IReadOnlyList<string> ParameterTypes,
	bool IsStatic,
	bool IsConst,
	bool HasOutParams,
	bool HasWorldContext,
	bool ReturnsByRef,
	int StableIndex);

internal sealed record AngelscriptSupportedModules(
	HashSet<string> RuntimeLinked,
	HashSet<string> CrossModuleOnly,
	HashSet<string> EditorOnly,
	bool CrossModuleGenerationEnabled,
	string CrossModuleGenerationProfile,
	string CrossModuleGenerationConfigPath,
	HashSet<string> CrossModuleConfigured);

internal sealed class AngelscriptCrossModuleGenerationConfig
{
	public int Version { get; init; }
	public bool Enabled { get; init; }
	public AngelscriptCrossModuleGenerationProfiles Profiles { get; init; } = new();
}

internal sealed class AngelscriptCrossModuleGenerationProfiles
{
	public List<string> Common { get; init; } = new();
	public List<string> Source { get; init; } = new();
	public List<string> Installed { get; init; } = new();
}

internal sealed record AngelscriptCrossModuleGenerationSelection(
	bool Enabled,
	string Profile,
	string ConfigPath,
	HashSet<string> ConfiguredModules,
	HashSet<string> CrossModuleOnlyModules);

internal sealed record AngelscriptModuleGenerationSummary(
	string ModuleName,
	bool EditorOnly,
	int TotalEntries,
	int DirectBindEntries,
	int StubEntries,
	int CrossModuleEntries,
	int ShardCount);

internal sealed record AngelscriptGeneratedFunctionDiagnosticRow(
	string ModuleName,
	bool EditorOnly,
	string ClassName,
	string FunctionName,
	string EntryKind,
	string EraseMacro,
	int ShardIndex,
	string ThunkStyle);

internal static class AngelscriptFunctionTableCodeGenerator
{
	private static readonly Regex QuotedStringPattern = new("\"([^\"]+)\"", RegexOptions.Compiled);
	private const int MaxEntriesPerShard = 256;
	private const string LayoutVersionFileName = "cross-module-layout-version.txt";
	private const string CrossModuleGenerationModulesFileName = "cross-module-generation-modules.json";
	private const string CompileOptionsFileName = "DefaultAngelscriptCompileOptions.ini";
	private const string CompileOptionsSectionName = "/Script/AngelscriptRuntime.AngelscriptCompileOptions";
	private const string ModuleLocalBindingsSettingName = "bCompileAngelscriptModuleLocalBindings";

	public static int Generate(IUhtExportFactory factory)
	{
		AngelscriptSupportedModules supportedModules = LoadSupportedModules(factory);
		string layoutVersion = LoadCrossModuleLayoutVersion(factory);
		int generatedFileCount = 0;
		HashSet<string> generatedPaths = new(StringComparer.OrdinalIgnoreCase);
		List<AngelscriptModuleGenerationSummary> moduleSummaries = new();
		List<AngelscriptGeneratedFunctionDiagnosticRow> csvEntries = new();
		HashSet<string> effectiveCrossModuleOnlyModules = new(StringComparer.OrdinalIgnoreCase);

		if (supportedModules.CrossModuleGenerationEnabled && TryEmitCrossModuleLinkProbe(factory, generatedPaths, layoutVersion))
		{
			generatedFileCount++;
		}

		Dictionary<string, string> allModuleOutputDirectories = new(StringComparer.OrdinalIgnoreCase);
		foreach (UhtModule module in factory.Session.Modules)
		{
			allModuleOutputDirectories[module.ShortName] = module.Module.OutputDirectory;

			bool runtimeLinked = supportedModules.RuntimeLinked.Contains(module.ShortName);
			bool crossModuleOnly = supportedModules.CrossModuleOnly.Contains(module.ShortName);
			if (!runtimeLinked && !crossModuleOnly)
			{
				continue;
			}
			if (crossModuleOnly)
			{
				effectiveCrossModuleOnlyModules.Add(module.ShortName);
			}

			AngelscriptModuleGenerationSummary? moduleSummary = GenerateModule(
				factory,
				module,
				supportedModules.EditorOnly.Contains(module.ShortName),
				runtimeLinked,
				supportedModules.CrossModuleGenerationEnabled,
				layoutVersion,
				generatedPaths,
				csvEntries);
			if (moduleSummary != null)
			{
				generatedFileCount += moduleSummary.ShardCount;
				moduleSummaries.Add(moduleSummary);
			}
		}

		DeleteStaleOutputs(factory, generatedPaths, allModuleOutputDirectories);
		WriteGenerationSummary(factory, supportedModules, effectiveCrossModuleOnlyModules, moduleSummaries, csvEntries, generatedFileCount);
		WriteCoverageDiagnostics(moduleSummaries);

		return generatedFileCount;
	}

	private static string LoadCrossModuleLayoutVersion(IUhtExportFactory factory)
	{
		foreach (string candidate in EnumerateLayoutVersionCandidates(factory))
		{
			if (File.Exists(candidate))
			{
				factory.AddExternalDependency(candidate);
				return ParseLayoutVersionToken(candidate);
			}
		}

		throw new FileNotFoundException($"Unable to locate {LayoutVersionFileName} for cross-module binding ABI generation.");
	}

	private static IEnumerable<string> EnumerateLayoutVersionCandidates(IUhtExportFactory factory)
	{
		string runtimeBuildCsPath = ResolveRuntimeBuildCsPath(factory);
		string? runtimeModuleDirectory = Path.GetDirectoryName(runtimeBuildCsPath);
		string? sourceDirectory = runtimeModuleDirectory != null
			? Directory.GetParent(runtimeModuleDirectory)?.FullName
			: null;
		if (!string.IsNullOrEmpty(sourceDirectory))
		{
			yield return Path.Combine(sourceDirectory, "AngelscriptUHTTool", LayoutVersionFileName);
		}

		string baseDirectory = AppContext.BaseDirectory;
		for (int attempt = 0; attempt < 8 && baseDirectory != null; attempt++)
		{
			yield return Path.Combine(baseDirectory, "Source", "AngelscriptUHTTool", LayoutVersionFileName);
			yield return Path.Combine(baseDirectory, LayoutVersionFileName);

			DirectoryInfo? parent = Directory.GetParent(baseDirectory);
			if (parent == null)
			{
				break;
			}

			baseDirectory = parent.FullName;
		}
	}

	private static string ParseLayoutVersionToken(string filePath)
	{
		foreach (string rawLine in File.ReadAllLines(filePath))
		{
			string line = rawLine.Trim();
			if (line.Length == 0 || line.StartsWith("#", StringComparison.Ordinal))
			{
				continue;
			}

			if (!Regex.IsMatch(line, "^0x[0-9A-Fa-f]{8}$"))
			{
				throw new InvalidDataException($"Invalid cross-module layout version token '{line}' in {filePath}.");
			}

			return line;
		}

		throw new InvalidDataException($"No cross-module layout version token found in {filePath}.");
	}

	private static bool TryEmitCrossModuleLinkProbe(IUhtExportFactory factory, HashSet<string> generatedPaths, string layoutVersion)
	{
		UhtModule? engineModule = factory.Session.Modules.FirstOrDefault(static module =>
			StringComparer.Ordinal.Equals(module.ShortName, "Engine"));
		if (engineModule == null)
		{
			Console.WriteLine("AngelscriptUHTTool cross-module link probe skipped: Engine module was not present in the UHT session.");
			return false;
		}

		string outputPath = Path.Combine(engineModule.Module.OutputDirectory, "AS_FunctionTable_Engine_LinkProbe.cpp");
		factory.CommitOutput(outputPath, BuildCrossModuleLinkProbeShard(layoutVersion));
		generatedPaths.Add(outputPath);
		Console.WriteLine("AngelscriptUHTTool cross-module link probe written: {0}", outputPath);
		return true;
	}

	private static string BuildCrossModuleLinkProbeShard(string layoutVersion)
	{
		StringBuilder builder = new();
		builder.AppendLine("#include \"Features/IModularFeatures.h\"");
		builder.AppendLine("#include \"Misc/CoreDelegates.h\"");
		builder.AppendLine();
		builder.AppendLine("namespace");
		builder.AppendLine("{");
		builder.AppendLine($"\tconstexpr uint32 GProbeLayoutVersion = {layoutVersion}u;");
		builder.AppendLine("\tbool GProbeShuttingDown = false;");
		builder.AppendLine();
		builder.AppendLine("\tstruct FProbeBinding");
		builder.AppendLine("\t{");
		builder.AppendLine("\t\tconst TCHAR* Tag;");
		builder.AppendLine("\t\tuint32 Magic;");
		builder.AppendLine("\t};");
		builder.AppendLine();
		builder.AppendLine("\tstruct FProbeBindingFeature : public IModularFeature");
		builder.AppendLine("\t{");
		builder.AppendLine("\t\tconst FProbeBinding* Entries;");
		builder.AppendLine("\t\tint32 Count;");
		builder.AppendLine("\t\tconst TCHAR* ModuleName;");
		builder.AppendLine("\t\tuint32 LayoutVersion;");
		builder.AppendLine();
		builder.AppendLine("\t\tFProbeBindingFeature(const FProbeBinding* InEntries, int32 InCount, const TCHAR* InModuleName, uint32 InLayoutVersion)");
		builder.AppendLine("\t\t\t: Entries(InEntries)");
		builder.AppendLine("\t\t\t, Count(InCount)");
		builder.AppendLine("\t\t\t, ModuleName(InModuleName)");
		builder.AppendLine("\t\t\t, LayoutVersion(InLayoutVersion)");
		builder.AppendLine("\t\t{");
		builder.AppendLine("\t\t}");
		builder.AppendLine("\t};");
		builder.AppendLine();
		builder.AppendLine("\tstatic const FProbeBinding GProbeTable[] =");
		builder.AppendLine("\t{");
		builder.AppendLine("\t\t{ TEXT(\"Engine.Probe\"), GProbeLayoutVersion },");
		builder.AppendLine("\t};");
		builder.AppendLine();
		builder.AppendLine("\tstatic FProbeBindingFeature GProbeFeature(GProbeTable, 1, TEXT(\"Engine\"), GProbeLayoutVersion);");
		builder.AppendLine();
		builder.AppendLine("\tvoid MarkProbeShutdown()");
		builder.AppendLine("\t{");
		builder.AppendLine("\t\tGProbeShuttingDown = true;");
		builder.AppendLine("\t}");
		builder.AppendLine();
		builder.AppendLine("\tstruct FProbeAutoRegistration");
		builder.AppendLine("\t{");
		builder.AppendLine("\t\tFProbeAutoRegistration()");
		builder.AppendLine("\t\t{");
		builder.AppendLine("\t\t\tIModularFeatures::Get().RegisterModularFeature(FName(TEXT(\"AngelscriptCrossModuleLinkProbe\")), &GProbeFeature);");
		builder.AppendLine("\t\t\tFCoreDelegates::OnPreExit.AddStatic(&MarkProbeShutdown);");
		builder.AppendLine("\t\t}");
		builder.AppendLine();
		builder.AppendLine("\t\t~FProbeAutoRegistration()");
		builder.AppendLine("\t\t{");
		builder.AppendLine("\t\t\tif (!GProbeShuttingDown)");
		builder.AppendLine("\t\t\t{");
		builder.AppendLine("\t\t\t\tIModularFeatures::Get().UnregisterModularFeature(FName(TEXT(\"AngelscriptCrossModuleLinkProbe\")), &GProbeFeature);");
		builder.AppendLine("\t\t\t}");
		builder.AppendLine("\t\t}");
		builder.AppendLine("\t};");
		builder.AppendLine();
		builder.AppendLine("\tstatic FProbeAutoRegistration GProbeAutoRegistration;");
		builder.AppendLine("}");
		return builder.ToString();
	}

	private static AngelscriptModuleGenerationSummary? GenerateModule(IUhtExportFactory factory, UhtModule module, bool editorOnly, bool emitRuntimeShard, bool allowCrossModuleGeneration, string layoutVersion, HashSet<string> generatedPaths, List<AngelscriptGeneratedFunctionDiagnosticRow> csvEntries)
	{
		SortedSet<string> includes = new(StringComparer.Ordinal);
		List<AngelscriptGeneratedFunctionBinding> entries = new();
		List<AngelscriptCrossModuleFunctionBinding> crossModuleEntries = new();

		CollectEntries(factory, module.ScriptPackage, module.ShortName, emitRuntimeShard, allowCrossModuleGeneration, includes, entries, crossModuleEntries);
		if (entries.Count == 0 && crossModuleEntries.Count == 0)
		{
			return null;
		}

		entries.Sort(static (left, right) =>
		{
			int classComparison = StringComparer.Ordinal.Compare(left.ClassName, right.ClassName);
			if (classComparison != 0)
			{
				return classComparison;
			}

			int functionComparison = StringComparer.Ordinal.Compare(left.FunctionName, right.FunctionName);
			return functionComparison != 0
				? functionComparison
				: StringComparer.Ordinal.Compare(left.EntryKind, right.EntryKind);
		});

		crossModuleEntries.Sort(static (left, right) =>
		{
			int classComparison = StringComparer.Ordinal.Compare(left.ClassName, right.ClassName);
			if (classComparison != 0)
			{
				return classComparison;
			}

			int functionComparison = StringComparer.Ordinal.Compare(left.FunctionName, right.FunctionName);
			return functionComparison != 0
				? functionComparison
				: left.StableIndex.CompareTo(right.StableIndex);
		});

		int generatedShardCount = 0;
		int directBindEntries = 0;
		int stubEntries = 0;
		int crossModuleEntryCount = 0;
		foreach (AngelscriptGeneratedFunctionBinding entry in entries)
		{
			switch (entry.EntryKind)
			{
				case "Direct":
					directBindEntries++;
					break;
				case "CrossModule":
					crossModuleEntryCount++;
					break;
				default:
					stubEntries++;
					break;
			}
		}

		int shardCount = emitRuntimeShard && entries.Count != 0 ? (entries.Count + MaxEntriesPerShard - 1) / MaxEntriesPerShard : 0;
		for (int shardIndex = 0; shardIndex < shardCount; shardIndex++)
		{
			int startIndex = shardIndex * MaxEntriesPerShard;
			int entryCount = Math.Min(MaxEntriesPerShard, entries.Count - startIndex);
			string outputPath = factory.MakePath($"AS_FunctionTable_{module.ShortName}_{shardIndex:D3}", ".gen.cpp");
			factory.CommitOutput(outputPath, BuildShard(module.ShortName, editorOnly, includes, entries, startIndex, entryCount, shardIndex, shardCount));
			generatedPaths.Add(outputPath);
			generatedShardCount++;

			for (int entryIndex = startIndex; entryIndex < startIndex + entryCount; entryIndex++)
			{
				AngelscriptGeneratedFunctionBinding entry = entries[entryIndex];
				csvEntries.Add(new AngelscriptGeneratedFunctionDiagnosticRow(
					module.ShortName,
					editorOnly,
					entry.ClassName,
					entry.FunctionName,
					entry.EntryKind,
					entry.EraseMacro,
					shardIndex + 1,
					entry.ThunkStyle));
			}
		}

		int crossModuleShardCount = crossModuleEntries.Count == 0 ? 0 : (crossModuleEntries.Count + MaxEntriesPerShard - 1) / MaxEntriesPerShard;
		for (int shardIndex = 0; shardIndex < crossModuleShardCount; shardIndex++)
		{
			int startIndex = shardIndex * MaxEntriesPerShard;
			int entryCount = Math.Min(MaxEntriesPerShard, crossModuleEntries.Count - startIndex);
			string outputPath = Path.Combine(module.Module.OutputDirectory, $"AS_FunctionTable_{module.ShortName}_CrossModule_{shardIndex:D3}.cpp");
			factory.CommitOutput(outputPath, BuildCrossModuleShard(module.ShortName, crossModuleEntries, startIndex, entryCount, shardIndex, crossModuleShardCount, layoutVersion));
			generatedPaths.Add(outputPath);
			generatedShardCount++;

			if (!emitRuntimeShard)
			{
				for (int entryIndex = startIndex; entryIndex < startIndex + entryCount; entryIndex++)
				{
					AngelscriptCrossModuleFunctionBinding entry = crossModuleEntries[entryIndex];
					csvEntries.Add(new AngelscriptGeneratedFunctionDiagnosticRow(
						module.ShortName,
						editorOnly,
						entry.ClassName,
						entry.FunctionName,
						"CrossModule",
						"ERASE_NO_FUNCTION()",
						shardIndex + 1,
						"FrameWrapper"));
				}
			}
		}

		return new AngelscriptModuleGenerationSummary(module.ShortName, editorOnly, entries.Count, directBindEntries, stubEntries, crossModuleEntryCount, generatedShardCount);
	}

	private static void WriteCoverageDiagnostics(List<AngelscriptModuleGenerationSummary> moduleSummaries)
	{
		moduleSummaries.Sort(static (left, right) =>
		{
			int stubComparison = right.StubEntries.CompareTo(left.StubEntries);
			return stubComparison != 0
				? stubComparison
				: StringComparer.Ordinal.Compare(left.ModuleName, right.ModuleName);
		});

		Console.WriteLine("AngelscriptUHTTool per-module coverage diagnostics:");
		foreach (AngelscriptModuleGenerationSummary summary in moduleSummaries)
		{
			Console.WriteLine(
				"  - {0}{1}: total={2}, direct={3}, stubs={4}, crossModule={5}, shards={6}",
				summary.ModuleName,
				summary.EditorOnly ? " [EditorOnly]" : string.Empty,
				summary.TotalEntries,
				summary.DirectBindEntries,
				summary.StubEntries,
				summary.CrossModuleEntries,
				summary.ShardCount);
		}
	}

	private static void WriteGenerationSummary(IUhtExportFactory factory, AngelscriptSupportedModules supportedModules, HashSet<string> effectiveCrossModuleOnlyModules, List<AngelscriptModuleGenerationSummary> moduleSummaries, List<AngelscriptGeneratedFunctionDiagnosticRow> csvEntries, int generatedFileCount)
	{
		int totalGeneratedEntries = moduleSummaries.Sum(static summary => summary.TotalEntries);
		int totalDirectBindEntries = moduleSummaries.Sum(static summary => summary.DirectBindEntries);
		int totalStubEntries = moduleSummaries.Sum(static summary => summary.StubEntries);
		int totalCrossModuleEntries = moduleSummaries.Sum(static summary => summary.CrossModuleEntries);
		double directBindRate = totalGeneratedEntries > 0 ? (double)totalDirectBindEntries / totalGeneratedEntries : 0.0;
		double stubRate = totalGeneratedEntries > 0 ? (double)totalStubEntries / totalGeneratedEntries : 0.0;
		double crossModuleRate = totalGeneratedEntries > 0 ? (double)totalCrossModuleEntries / totalGeneratedEntries : 0.0;

		string summaryPath = factory.MakePath("AS_FunctionTable_Summary", ".json");
		Directory.CreateDirectory(Path.GetDirectoryName(summaryPath)!);

		string summaryJson = JsonSerializer.Serialize(
			new
			{
				totalGeneratedEntries,
				totalDirectBindEntries,
				totalStubEntries,
				totalCrossModuleEntries,
				directBindRate,
				stubRate,
				crossModuleRate,
				totalShardCount = generatedFileCount,
				moduleCount = moduleSummaries.Count,
				crossModuleGenerationEnabled = supportedModules.CrossModuleGenerationEnabled,
				crossModuleGenerationProfile = supportedModules.CrossModuleGenerationProfile,
				crossModuleGenerationConfigPath = supportedModules.CrossModuleGenerationConfigPath,
				crossModuleConfiguredModules = supportedModules.CrossModuleConfigured
					.OrderBy(static moduleName => moduleName, StringComparer.OrdinalIgnoreCase)
					.ToArray(),
				crossModuleEffectiveModules = effectiveCrossModuleOnlyModules
					.OrderBy(static moduleName => moduleName, StringComparer.OrdinalIgnoreCase)
					.ToArray(),
				modules = moduleSummaries.Select(summary => new
				{
					moduleName = summary.ModuleName,
					editorOnly = summary.EditorOnly,
					totalEntries = summary.TotalEntries,
					directBindEntries = summary.DirectBindEntries,
					stubEntries = summary.StubEntries,
					crossModuleEntries = summary.CrossModuleEntries,
					directBindRate = summary.TotalEntries > 0 ? (double)summary.DirectBindEntries / summary.TotalEntries : 0.0,
					stubRate = summary.TotalEntries > 0 ? (double)summary.StubEntries / summary.TotalEntries : 0.0,
					crossModuleRate = summary.TotalEntries > 0 ? (double)summary.CrossModuleEntries / summary.TotalEntries : 0.0,
					shardCount = summary.ShardCount,
				}),
			},
			new JsonSerializerOptions
			{
				WriteIndented = true,
			});

		File.WriteAllText(summaryPath, summaryJson, Encoding.UTF8);
		WriteModuleSummaryCsv(factory, moduleSummaries);
		WriteEntryCsv(factory, csvEntries);

		Console.WriteLine(
			"AngelscriptUHTTool generated {0} binding entries ({1} direct, {2} stubs, {3} cross-module) across {4} modules and {5} shard files. Summary: {6}",
			totalGeneratedEntries,
			totalDirectBindEntries,
			totalStubEntries,
			totalCrossModuleEntries,
			moduleSummaries.Count,
			generatedFileCount,
			summaryPath);
	}

	private static void WriteModuleSummaryCsv(IUhtExportFactory factory, List<AngelscriptModuleGenerationSummary> moduleSummaries)
	{
		string csvPath = factory.MakePath("AS_FunctionTable_ModuleSummary", ".csv");
		Directory.CreateDirectory(Path.GetDirectoryName(csvPath)!);

		StringBuilder builder = new();
		builder.AppendLine("ModuleName,EditorOnly,TotalEntries,DirectBindEntries,StubEntries,CrossModuleEntries,DirectBindRate,StubRate,CrossModuleRate,ShardCount");
		foreach (AngelscriptModuleGenerationSummary summary in moduleSummaries)
		{
			double directBindRate = summary.TotalEntries > 0 ? (double)summary.DirectBindEntries / summary.TotalEntries : 0.0;
			double stubRate = summary.TotalEntries > 0 ? (double)summary.StubEntries / summary.TotalEntries : 0.0;
			double crossModuleRate = summary.TotalEntries > 0 ? (double)summary.CrossModuleEntries / summary.TotalEntries : 0.0;
			builder
				.Append(EscapeCsv(summary.ModuleName)).Append(',')
				.Append(summary.EditorOnly ? "true" : "false").Append(',')
				.Append(summary.TotalEntries).Append(',')
				.Append(summary.DirectBindEntries).Append(',')
				.Append(summary.StubEntries).Append(',')
				.Append(summary.CrossModuleEntries).Append(',')
				.Append(FormatRate(directBindRate)).Append(',')
				.Append(FormatRate(stubRate)).Append(',')
				.Append(FormatRate(crossModuleRate)).Append(',')
				.Append(summary.ShardCount)
				.Append("\r\n");
		}

		File.WriteAllText(csvPath, builder.ToString(), Encoding.UTF8);
	}

	private static void WriteEntryCsv(IUhtExportFactory factory, List<AngelscriptGeneratedFunctionDiagnosticRow> csvEntries)
	{
		string csvPath = factory.MakePath("AS_FunctionTable_Entries", ".csv");
		Directory.CreateDirectory(Path.GetDirectoryName(csvPath)!);

		StringBuilder builder = new();
		builder.AppendLine("ModuleName,EditorOnly,ClassName,FunctionName,EntryKind,EraseMacro,ShardIndex,ThunkStyle");
		foreach (AngelscriptGeneratedFunctionDiagnosticRow entry in csvEntries)
		{
			builder
				.Append(EscapeCsv(entry.ModuleName)).Append(',')
				.Append(entry.EditorOnly ? "true" : "false").Append(',')
				.Append(EscapeCsv(entry.ClassName)).Append(',')
				.Append(EscapeCsv(entry.FunctionName)).Append(',')
				.Append(EscapeCsv(entry.EntryKind)).Append(',')
				.Append(EscapeCsv(entry.EraseMacro)).Append(',')
				.Append(entry.ShardIndex).Append(',')
				.Append(EscapeCsv(entry.ThunkStyle))
				.Append("\r\n");
		}

		File.WriteAllText(csvPath, builder.ToString(), Encoding.UTF8);
	}

	private static string FormatRate(double value)
	{
		return value.ToString("0.################", CultureInfo.InvariantCulture);
	}

	private static string EscapeCsv(string value)
	{
		if (value.IndexOfAny(new[] { ',', '"', '\r', '\n' }) == -1)
		{
			return value;
		}

		return '"' + value.Replace("\"", "\"\"") + '"';
	}

	private static StringBuilder BuildShard(string moduleShortName, bool editorOnly, SortedSet<string> includes, List<AngelscriptGeneratedFunctionBinding> entries, int startIndex, int entryCount, int shardIndex, int shardCount)
	{
		StringBuilder builder = new();
		if (editorOnly)
		{
			builder.AppendLine("#if WITH_EDITOR");
		}

		builder.AppendLine("PRAGMA_DISABLE_DEPRECATION_WARNINGS");
		builder.AppendLine("#include \"CoreMinimal.h\"");
		builder.AppendLine("#include \"Core/AngelscriptBinds.h\"");
		builder.AppendLine("#include \"Core/AngelscriptEngine.h\"");
		builder.AppendLine("#include \"Core/FunctionCallers.h\"");

		foreach (string include in includes)
		{
			builder.Append("#include \"").Append(include).AppendLine("\"");
		}

		builder.AppendLine();
		builder.Append("AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_AS_FunctionTable_")
			.Append(moduleShortName)
			.Append('_')
			.Append(shardIndex.ToString("D3"))
			.AppendLine("((int32)FAngelscriptBinds::EOrder::Late + 50, []()");
		builder.AppendLine("{");
		builder.AppendLine("\tconst double GeneratedFunctionTableStartSeconds = FPlatformTime::Seconds();");

		for (int entryIndex = startIndex; entryIndex < startIndex + entryCount; entryIndex++)
		{
			AngelscriptGeneratedFunctionBinding entry = entries[entryIndex];
			bool needsEditorGuard = !editorOnly && entry.EditorOnlyGuard != null;
			if (needsEditorGuard)
			{
				builder.Append("#if ").AppendLine(entry.EditorOnlyGuard);
			}

			builder.AppendLine(entry.BuildBindingRegistrationLine());

			if (needsEditorGuard)
			{
				builder.AppendLine("#endif");
			}
		}

		builder.AppendLine("\tconst double GeneratedFunctionTableElapsedMilliseconds = (FPlatformTime::Seconds() - GeneratedFunctionTableStartSeconds) * 1000.0;");
		builder.Append("\tFAngelscriptBinds::RecordGeneratedFunctionTableShardTiming(TEXT(\"")
			.Append(moduleShortName)
			.Append("\"), ")
			.Append(shardIndex + 1)
			.Append(", ")
			.Append(shardCount)
			.Append(", ")
			.Append(entryCount)
			.AppendLine(", GeneratedFunctionTableElapsedMilliseconds);");

		builder.Append("\tUE_LOG(Angelscript, Log, TEXT(\"[UHT] Registered %d generated AS-callable entries for module %s shard %d/%d in %.3f ms\"), ")
			.Append(entryCount)
			.Append(", TEXT(\"")
			.Append(moduleShortName)
			.Append("\"), ")
			.Append(shardIndex + 1)
			.Append(", ")
			.Append(shardCount)
			.AppendLine(", GeneratedFunctionTableElapsedMilliseconds);");

		builder.AppendLine("});");
		builder.AppendLine("PRAGMA_ENABLE_DEPRECATION_WARNINGS");
		if (editorOnly)
		{
			builder.AppendLine("#endif");
		}

		return builder;
	}

	private static StringBuilder BuildCrossModuleShard(string moduleShortName, List<AngelscriptCrossModuleFunctionBinding> entries, int startIndex, int entryCount, int shardIndex, int shardCount, string layoutVersion)
	{
		StringBuilder builder = new();
		SortedSet<string> includes = new(StringComparer.Ordinal);
		for (int entryIndex = startIndex; entryIndex < startIndex + entryCount; entryIndex++)
		{
			foreach (string includePath in entries[entryIndex].IncludePaths)
			{
				includes.Add(includePath);
			}
		}

		builder.AppendLine("#include \"CoreMinimal.h\"");
		builder.AppendLine("#include \"Features/IModularFeatures.h\"");
		builder.AppendLine("#include \"Misc/CoreDelegates.h\"");
		foreach (string include in includes)
		{
			builder.Append("#include \"").Append(include).AppendLine("\"");
		}

		builder.AppendLine();
		builder.Append("namespace AngelscriptCrossModule_")
			.Append(SanitizeIdentifier(moduleShortName))
			.Append('_')
			.Append(shardIndex.ToString("D3", CultureInfo.InvariantCulture))
			.AppendLine();
		builder.AppendLine("{");
		builder.AppendLine($"\tconstexpr uint32 GCrossModuleLayoutVersion = {layoutVersion}u;");
		builder.AppendLine("\tconstexpr uint32 GCrossModuleFlagNone = 0u;");
		builder.AppendLine("\tconstexpr uint32 GCrossModuleFlagStatic = 1u << 0;");
		builder.AppendLine("\tconstexpr uint32 GCrossModuleFlagConst = 1u << 1;");
		builder.AppendLine("\tconstexpr uint32 GCrossModuleFlagWorldContext = 1u << 2;");
		builder.AppendLine("\tconstexpr uint32 GCrossModuleFlagHasOutParams = 1u << 3;");
		builder.AppendLine("\tconstexpr uint32 GCrossModuleFlagReturnByRef = 1u << 4;");
		builder.AppendLine("\tbool GCrossModuleShuttingDown = false;");
		builder.AppendLine();
		builder.AppendLine("\tstruct FCrossModuleCallFrame");
		builder.AppendLine("\t{");
		builder.AppendLine("\t\tvoid** ArgSlots;");
		builder.AppendLine("\t\tuint16 ArgCount;");
		builder.AppendLine("\t\tuint16 Reserved0;");
		builder.AppendLine("\t\tvoid* ReturnSlot;");
		builder.AppendLine("\t\tUObject* ScriptSelf;");
		builder.AppendLine("\t\tUObject* WorldContext;");
		builder.AppendLine("\t\tuint32 Flags;");
		builder.AppendLine("\t\tuint32 Reserved1;");
		builder.AppendLine("\t};");
		builder.AppendLine();
		builder.AppendLine("\tstruct FCrossModuleBinding");
		builder.AppendLine("\t{");
		builder.AppendLine("\t\tconst TCHAR* ClassName;");
		builder.AppendLine("\t\tconst TCHAR* FunctionName;");
		builder.AppendLine("\t\tvoid (*Thunk)(UObject* Self, FCrossModuleCallFrame* Frame);");
		builder.AppendLine("\t\tuint16 ArgCount;");
		builder.AppendLine("\t\tuint16 RetSize;");
		builder.AppendLine("\t\tuint32 Flags;");
		builder.AppendLine("\t};");
		builder.AppendLine();
		builder.AppendLine("\tstruct FCrossModuleBindingFeature : public IModularFeature");
		builder.AppendLine("\t{");
		builder.AppendLine("\t\tconst FCrossModuleBinding* Table;");
		builder.AppendLine("\t\tint32 Count;");
		builder.AppendLine("\t\tconst TCHAR* ModuleName;");
		builder.AppendLine("\t\tuint32 LayoutVersion;");
		builder.AppendLine();
		builder.AppendLine("\t\tFCrossModuleBindingFeature(const FCrossModuleBinding* InTable, int32 InCount, const TCHAR* InModuleName, uint32 InLayoutVersion)");
		builder.AppendLine("\t\t\t: Table(InTable)");
		builder.AppendLine("\t\t\t, Count(InCount)");
		builder.AppendLine("\t\t\t, ModuleName(InModuleName)");
		builder.AppendLine("\t\t\t, LayoutVersion(InLayoutVersion)");
		builder.AppendLine("\t\t{");
		builder.AppendLine("\t\t}");
		builder.AppendLine("\t};");
		builder.AppendLine();
		builder.AppendLine("\tstatic_assert(sizeof(FCrossModuleCallFrame) == 48, \"FCrossModuleCallFrame ABI layout changed; bump cross-module-layout-version.txt.\");");
		builder.AppendLine("\tstatic_assert(sizeof(FCrossModuleBinding) == 32, \"FCrossModuleBinding ABI layout changed; bump cross-module-layout-version.txt.\");");
		builder.AppendLine("\tstatic_assert(sizeof(FCrossModuleBindingFeature) == 32, \"FCrossModuleBindingFeature ABI layout changed; bump cross-module-layout-version.txt.\");");
		builder.AppendLine();
		builder.AppendLine("\ttemplate <typename T>");
		builder.AppendLine("\tdecltype(auto) PassCrossModuleArg(FCrossModuleCallFrame* Frame, uint16 Index)");
		builder.AppendLine("\t{");
		builder.AppendLine("\t\tcheck(Frame != nullptr);");
		builder.AppendLine("\t\tcheck(Frame->ArgSlots != nullptr);");
		builder.AppendLine("\t\tcheck(Index < Frame->ArgCount);");
		builder.AppendLine("\t\tusing ValueType = typename TRemoveReference<T>::Type;");
		builder.AppendLine("\t\tif constexpr (TIsReferenceType<T>::Value)");
		builder.AppendLine("\t\t{");
		builder.AppendLine("\t\t\treturn *static_cast<ValueType*>(Frame->ArgSlots[Index]);");
		builder.AppendLine("\t\t}");
		builder.AppendLine("\t\telse if constexpr (TIsPointer<T>::Value)");
		builder.AppendLine("\t\t{");
		builder.AppendLine("\t\t\treturn *static_cast<T*>(Frame->ArgSlots[Index]);");
		builder.AppendLine("\t\t}");
		builder.AppendLine("\t\telse");
		builder.AppendLine("\t\t{");
		builder.AppendLine("\t\t\treturn *static_cast<T*>(Frame->ArgSlots[Index]);");
		builder.AppendLine("\t\t}");
		builder.AppendLine("\t}");
		builder.AppendLine();
		builder.AppendLine("\ttemplate <typename T>");
		builder.AppendLine("\tvoid BuildCrossModuleReturn(FCrossModuleCallFrame* Frame, T&& Value)");
		builder.AppendLine("\t{");
		builder.AppendLine("\t\tif (Frame == nullptr || Frame->ReturnSlot == nullptr)");
		builder.AppendLine("\t\t{");
		builder.AppendLine("\t\t\treturn;");
		builder.AppendLine("\t\t}");
		builder.AppendLine();
		builder.AppendLine("\t\tusing ReturnType = typename TRemoveReference<T>::Type;");
		builder.AppendLine("\t\tif constexpr (TIsArithmetic<ReturnType>::Value || TIsEnum<ReturnType>::Value || TIsPointer<ReturnType>::Value)");
		builder.AppendLine("\t\t{");
		builder.AppendLine("\t\t\t*static_cast<ReturnType*>(Frame->ReturnSlot) = Value;");
		builder.AppendLine("\t\t}");
		builder.AppendLine("\t\telse");
		builder.AppendLine("\t\t{");
		builder.AppendLine("\t\t\tnew (Frame->ReturnSlot) ReturnType(Forward<T>(Value));");
		builder.AppendLine("\t\t}");
		builder.AppendLine("\t}");
		builder.AppendLine();

		for (int entryIndex = startIndex; entryIndex < startIndex + entryCount; entryIndex++)
		{
			AngelscriptCrossModuleFunctionBinding entry = entries[entryIndex];
			string thunkName = BuildThunkName(entry);
			string callArguments = BuildCrossModuleCallArguments(entry);
			builder.Append("\tvoid ").Append(thunkName).AppendLine("(UObject* Self, FCrossModuleCallFrame* Frame)");
			builder.AppendLine("\t{");
			if (entry.ParameterTypes.Count > 0)
			{
				builder.AppendLine("\t\tif (Frame == nullptr || Frame->ArgSlots == nullptr || Frame->ArgCount < " + entry.ParameterTypes.Count.ToString(CultureInfo.InvariantCulture) + ")");
				builder.AppendLine("\t\t{");
				builder.AppendLine("\t\t\treturn;");
				builder.AppendLine("\t\t}");
				builder.AppendLine();
			}
			if (entry.IsStatic)
			{
				builder.Append("\t\t");
				if (entry.ReturnType != "void")
				{
					builder.Append("BuildCrossModuleReturn<").Append(entry.ReturnType).Append(">(Frame, ");
				}
				builder.Append(entry.ClassName).Append("::").Append(entry.FunctionName).Append("(").Append(callArguments).Append(")");
				builder.AppendLine(entry.ReturnType == "void" ? ";" : ");");
			}
			else
			{
				builder.AppendLine("\t\tif (Self == nullptr)");
				builder.AppendLine("\t\t{");
				builder.AppendLine("\t\t\treturn;");
				builder.AppendLine("\t\t}");
				builder.AppendLine();
				builder.Append("\t\t");
				if (entry.ReturnType != "void")
				{
					builder.Append("BuildCrossModuleReturn<").Append(entry.ReturnType).Append(">(Frame, ");
				}
				builder.Append("static_cast<");
				if (entry.IsConst)
				{
					builder.Append("const ");
				}
				builder.Append(entry.ClassName).Append("*>(Self)->").Append(entry.FunctionName).Append("(").Append(callArguments).Append(")");
				builder.AppendLine(entry.ReturnType == "void" ? ";" : ");");
			}
			builder.AppendLine("\t}");
			builder.AppendLine();
		}

		builder.AppendLine("\tstatic const FCrossModuleBinding GCrossModuleTable[] =");
		builder.AppendLine("\t{");
		for (int entryIndex = startIndex; entryIndex < startIndex + entryCount; entryIndex++)
		{
			AngelscriptCrossModuleFunctionBinding entry = entries[entryIndex];
			builder.Append("\t\t{ TEXT(\"")
				.Append(entry.ClassName)
				.Append("\"), TEXT(\"")
				.Append(entry.FunctionName)
				.Append("\"), &")
				.Append(BuildThunkName(entry))
				.Append(", ")
				.Append(entry.ParameterTypes.Count)
				.Append(", ")
				.Append(GetReturnSizeExpression(entry.ReturnType))
				.Append(", ")
				.Append(BuildCrossModuleFlagsExpression(entry))
				.AppendLine(" },");
		}
		builder.AppendLine("\t};");
		builder.AppendLine();
		builder.AppendLine("\tstatic FCrossModuleBindingFeature GCrossModuleFeature(GCrossModuleTable, UE_ARRAY_COUNT(GCrossModuleTable), TEXT(\"" + moduleShortName + "\"), GCrossModuleLayoutVersion);");
		builder.AppendLine();
		builder.AppendLine("\tvoid MarkCrossModuleShutdown()");
		builder.AppendLine("\t{");
		builder.AppendLine("\t\tGCrossModuleShuttingDown = true;");
		builder.AppendLine("\t}");
		builder.AppendLine();
		builder.AppendLine("\tstruct FCrossModuleAutoRegistration");
		builder.AppendLine("\t{");
		builder.AppendLine("\t\tFCrossModuleAutoRegistration()");
		builder.AppendLine("\t\t{");
		builder.AppendLine("\t\t\tIModularFeatures::Get().RegisterModularFeature(FName(TEXT(\"AngelscriptCrossModuleFunctionBindings\")), &GCrossModuleFeature);");
		builder.AppendLine("\t\t\tFCoreDelegates::OnPreExit.AddStatic(&MarkCrossModuleShutdown);");
		builder.AppendLine("\t\t}");
		builder.AppendLine();
		builder.AppendLine("\t\t~FCrossModuleAutoRegistration()");
		builder.AppendLine("\t\t{");
		builder.AppendLine("\t\t\tif (!GCrossModuleShuttingDown)");
		builder.AppendLine("\t\t\t{");
			builder.AppendLine("\t\t\t\tIModularFeatures::Get().UnregisterModularFeature(FName(TEXT(\"AngelscriptCrossModuleFunctionBindings\")), &GCrossModuleFeature);");
		builder.AppendLine("\t\t\t}");
		builder.AppendLine("\t\t}");
		builder.AppendLine("\t};");
		builder.AppendLine();
		builder.Append("\tstatic FCrossModuleAutoRegistration GCrossModuleAutoRegistration_")
			.Append(shardIndex.ToString("D3", CultureInfo.InvariantCulture))
			.AppendLine(";");
		builder.AppendLine("}");
		return builder;
	}

	private static string BuildThunkName(AngelscriptCrossModuleFunctionBinding entry)
	{
		return "Call_" + SanitizeIdentifier(entry.ClassName) + "_" + SanitizeIdentifier(entry.FunctionName) + "_" + entry.StableIndex.ToString("D5", CultureInfo.InvariantCulture);
	}

	private static string SanitizeIdentifier(string value)
	{
		StringBuilder builder = new(value.Length);
		foreach (char character in value)
		{
			builder.Append(char.IsLetterOrDigit(character) || character == '_' ? character : '_');
		}
		return builder.ToString();
	}

	private static string GetReturnSizeExpression(string returnType)
	{
		return returnType == "void" ? "0" : $"sizeof({returnType})";
	}

	private static string BuildCrossModuleCallArguments(AngelscriptCrossModuleFunctionBinding entry)
	{
		if (entry.ParameterTypes.Count == 0)
		{
			return string.Empty;
		}

		List<string> arguments = new(entry.ParameterTypes.Count);
		for (int index = 0; index < entry.ParameterTypes.Count; index++)
		{
			arguments.Add($"PassCrossModuleArg<{entry.ParameterTypes[index]}>(Frame, {index})");
		}
		return string.Join(", ", arguments);
	}

	private static string BuildCrossModuleFlagsExpression(AngelscriptCrossModuleFunctionBinding entry)
	{
		List<string> flags = new();
		if (entry.IsStatic)
		{
			flags.Add("GCrossModuleFlagStatic");
		}
		if (entry.IsConst)
		{
			flags.Add("GCrossModuleFlagConst");
		}
		if (entry.HasWorldContext)
		{
			flags.Add("GCrossModuleFlagWorldContext");
		}
		if (entry.HasOutParams)
		{
			flags.Add("GCrossModuleFlagHasOutParams");
		}
		if (entry.ReturnsByRef)
		{
			flags.Add("GCrossModuleFlagReturnByRef");
		}

		return flags.Count == 0 ? "GCrossModuleFlagNone" : string.Join(" | ", flags);
	}

	internal static AngelscriptSupportedModules LoadSupportedModules(IUhtExportFactory factory)
	{
		string buildCsPath = ResolveRuntimeBuildCsPath(factory);
		factory.AddExternalDependency(buildCsPath);

		HashSet<string> runtimeLinkedModules = new(StringComparer.OrdinalIgnoreCase)
		{
			"AngelscriptRuntime",
		};
		HashSet<string> editorOnlyModules = new(StringComparer.OrdinalIgnoreCase);

		bool inDependencyBlock = false;
		bool inEditorBlock = false;
		foreach (string rawLine in File.ReadAllLines(buildCsPath))
		{
			string line = rawLine.Trim();
			if (line.StartsWith("if (Target.bBuildEditor)", StringComparison.Ordinal))
			{
				inEditorBlock = true;
			}

			if (line.Contains("DependencyModuleNames.AddRange", StringComparison.Ordinal))
			{
				inDependencyBlock = true;
			}

			if (inDependencyBlock)
			{
				foreach (Match match in QuotedStringPattern.Matches(line))
				{
					string moduleName = match.Groups[1].Value;
					runtimeLinkedModules.Add(moduleName);
					if (inEditorBlock)
					{
						editorOnlyModules.Add(moduleName);
					}
				}

				if (line.Contains("});", StringComparison.Ordinal))
				{
					inDependencyBlock = false;
				}
			}

			if (inEditorBlock && line == "}")
			{
				inEditorBlock = false;
			}
		}

		AngelscriptCrossModuleGenerationSelection crossModuleSelection = LoadCrossModuleGenerationModules(factory, runtimeLinkedModules);
		return new AngelscriptSupportedModules(
			runtimeLinkedModules,
			crossModuleSelection.CrossModuleOnlyModules,
			editorOnlyModules,
			crossModuleSelection.Enabled,
			crossModuleSelection.Profile,
			crossModuleSelection.ConfigPath,
			crossModuleSelection.ConfiguredModules);
	}

	private static AngelscriptCrossModuleGenerationSelection LoadCrossModuleGenerationModules(IUhtExportFactory factory, HashSet<string> runtimeLinkedModules)
	{
		foreach (string candidate in EnumerateCrossModuleGenerationModuleCandidates(factory))
		{
			if (!File.Exists(candidate))
			{
				continue;
			}

			factory.AddExternalDependency(candidate);
			AngelscriptCrossModuleGenerationConfig config = LoadCrossModuleGenerationConfig(candidate);
			string runtimeBuildCsPath = ResolveRuntimeBuildCsPath(factory);
			bool moduleLocalBindingsEnabled = ReadModuleLocalBindingsSetting(factory, runtimeBuildCsPath);
			string? engineDirectory = ResolveEngineDirectory(factory);
			string engineDistribution = ClassifyEngineDistribution(engineDirectory);
			if (moduleLocalBindingsEnabled && !engineDistribution.Equals("source", StringComparison.OrdinalIgnoreCase))
			{
				throw new InvalidOperationException(
					$"Angelscript ModuleLocal binding compilation requires a source engine. Engine '{engineDirectory ?? "<unknown>"}' is classified as {engineDistribution}; disable {ModuleLocalBindingsSettingName} or use a source engine.");
			}

			string profile = ResolveCrossModuleGenerationProfile(factory);
			bool enabled = moduleLocalBindingsEnabled && config.Enabled;
			HashSet<string> configuredModules = new(StringComparer.OrdinalIgnoreCase);
			AddConfiguredModules(configuredModules, config.Profiles.Common, candidate, "common");
			AddConfiguredModules(
				configuredModules,
				profile.Equals("source", StringComparison.OrdinalIgnoreCase) ? config.Profiles.Source : config.Profiles.Installed,
				candidate,
				profile);

			HashSet<string> crossModuleOnlyModules = new(StringComparer.OrdinalIgnoreCase);
			if (enabled)
			{
				foreach (string moduleName in configuredModules)
				{
					if (!runtimeLinkedModules.Contains(moduleName))
					{
						crossModuleOnlyModules.Add(moduleName);
					}
				}
			}

			Console.WriteLine(
				"AngelscriptUHTTool cross-module generation profile '{0}' loaded from {1}: enabled={2}, configured={3}, effective={4}",
				profile,
				candidate,
				enabled,
				configuredModules.Count,
				crossModuleOnlyModules.Count);

			return new AngelscriptCrossModuleGenerationSelection(enabled, profile, candidate, configuredModules, crossModuleOnlyModules);
		}

		throw new FileNotFoundException($"Unable to locate {CrossModuleGenerationModulesFileName} for cross-module generation profiles.");
	}

	private static AngelscriptCrossModuleGenerationConfig LoadCrossModuleGenerationConfig(string configPath)
	{
		AngelscriptCrossModuleGenerationConfig? config;
		try
		{
			config = JsonSerializer.Deserialize<AngelscriptCrossModuleGenerationConfig>(
				File.ReadAllText(configPath),
				new JsonSerializerOptions
				{
					AllowTrailingCommas = true,
					PropertyNameCaseInsensitive = true,
					ReadCommentHandling = JsonCommentHandling.Skip,
				});
		}
		catch (JsonException exception)
		{
			throw new InvalidDataException($"Unable to parse {CrossModuleGenerationModulesFileName}: {configPath}", exception);
		}

		if (config == null)
		{
			throw new InvalidDataException($"{CrossModuleGenerationModulesFileName} is empty: {configPath}");
		}
		if (config.Version != 1)
		{
			throw new InvalidDataException($"{CrossModuleGenerationModulesFileName} version {config.Version} is not supported: {configPath}");
		}
		if (config.Profiles.Common == null || config.Profiles.Source == null || config.Profiles.Installed == null)
		{
			throw new InvalidDataException($"{CrossModuleGenerationModulesFileName} must define common, source, and installed profile arrays: {configPath}");
		}

		return config;
	}

	private static void AddConfiguredModules(HashSet<string> configuredModules, IReadOnlyList<string> profileModules, string configPath, string profileName)
	{
		for (int index = 0; index < profileModules.Count; index++)
		{
			string moduleName = profileModules[index].Trim();
			if (moduleName.Length == 0)
			{
				throw new InvalidDataException($"{CrossModuleGenerationModulesFileName} profile '{profileName}' contains an empty module name at index {index}: {configPath}");
			}

			configuredModules.Add(moduleName);
		}
	}

	private static string ResolveCrossModuleGenerationProfile(IUhtExportFactory factory)
	{
		string? engineDirectory = ResolveEngineDirectory(factory);
		switch (ClassifyEngineDistribution(engineDirectory))
		{
			case "installed":
				return "installed";
			case "source":
				return "source";
		}

		Console.WriteLine(
			string.IsNullOrEmpty(engineDirectory)
				? "Warning: AngelscriptUHTTool could not resolve engine root; using installed cross-module generation profile."
				: $"Warning: AngelscriptUHTTool could not classify engine distribution at {engineDirectory}; using installed cross-module generation profile.");
		return "installed";
	}

	private static string ClassifyEngineDistribution(string? engineDirectory)
	{
		if (string.IsNullOrEmpty(engineDirectory))
		{
			return "unknown";
		}

		string normalizedEngineDirectory = engineDirectory.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
		if (File.Exists(Path.Combine(normalizedEngineDirectory, "Build", "InstalledBuild.txt")))
		{
			return "installed";
		}

		if (File.Exists(Path.Combine(normalizedEngineDirectory, "Build", "SourceDistribution.txt")) ||
			Directory.Exists(Path.Combine(normalizedEngineDirectory, ".git")) ||
			(Directory.GetParent(normalizedEngineDirectory) is DirectoryInfo engineParent && Directory.Exists(Path.Combine(engineParent.FullName, ".git"))))
		{
			return "source";
		}

		return "unknown";
	}

	private static string? ResolveEngineDirectory(IUhtExportFactory factory)
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

	private static bool TryExtractEngineDirectory(string? path, out string? engineDirectory)
	{
		engineDirectory = null;
		if (string.IsNullOrEmpty(path))
		{
			return false;
		}

		string normalizedPath = path.Replace('\\', '/');
		string[] markers = ["/Engine/Source/", "/Engine/Plugins/"];
		foreach (string marker in markers)
		{
			int markerIndex = normalizedPath.IndexOf(marker, StringComparison.OrdinalIgnoreCase);
			if (markerIndex >= 0)
			{
				engineDirectory = Path.GetFullPath(normalizedPath.Substring(0, markerIndex + "/Engine".Length));
				return true;
			}
		}

		return false;
	}

	private static IEnumerable<string> EnumerateCrossModuleGenerationModuleCandidates(IUhtExportFactory factory)
	{
		string runtimeBuildCsPath = ResolveRuntimeBuildCsPath(factory);
		string? runtimeModuleDirectory = Path.GetDirectoryName(runtimeBuildCsPath);
		string? sourceDirectory = runtimeModuleDirectory != null
			? Directory.GetParent(runtimeModuleDirectory)?.FullName
			: null;
		if (!string.IsNullOrEmpty(sourceDirectory))
		{
			yield return Path.Combine(sourceDirectory, "AngelscriptUHTTool", CrossModuleGenerationModulesFileName);
		}

		string baseDirectory = AppContext.BaseDirectory;
		for (int attempt = 0; attempt < 8 && baseDirectory != null; attempt++)
		{
			yield return Path.Combine(baseDirectory, "Source", "AngelscriptUHTTool", CrossModuleGenerationModulesFileName);
			yield return Path.Combine(baseDirectory, CrossModuleGenerationModulesFileName);

			DirectoryInfo? parent = Directory.GetParent(baseDirectory);
			if (parent == null)
			{
				break;
			}

			baseDirectory = parent.FullName;
		}
	}

	private static bool ReadModuleLocalBindingsSetting(IUhtExportFactory factory, string runtimeBuildCsPath)
	{
		foreach (string candidate in EnumerateCompileOptionsCandidates(runtimeBuildCsPath))
		{
			if (!File.Exists(candidate))
			{
				continue;
			}

			factory.AddExternalDependency(candidate);
			bool bInSection = false;
			foreach (string rawLine in File.ReadAllLines(candidate))
			{
				string line = rawLine.Trim();
				if (line.Length == 0 || line.StartsWith(";", StringComparison.Ordinal) || line.StartsWith("#", StringComparison.Ordinal))
				{
					continue;
				}

				if (line.StartsWith("[", StringComparison.Ordinal) && line.EndsWith("]", StringComparison.Ordinal))
				{
					bInSection = string.Equals(line.Substring(1, line.Length - 2), CompileOptionsSectionName, StringComparison.Ordinal);
					continue;
				}

				if (!bInSection)
				{
					continue;
				}

				int separatorIndex = line.IndexOf('=');
				if (separatorIndex <= 0 || !string.Equals(line.Substring(0, separatorIndex).Trim(), ModuleLocalBindingsSettingName, StringComparison.OrdinalIgnoreCase))
				{
					continue;
				}

				string value = line.Substring(separatorIndex + 1).Trim();
				return value.Equals("true", StringComparison.OrdinalIgnoreCase) ||
					value.Equals("1", StringComparison.OrdinalIgnoreCase) ||
					value.Equals("yes", StringComparison.OrdinalIgnoreCase);
			}

			return false;
		}

		return false;
	}

	private static IEnumerable<string> EnumerateCompileOptionsCandidates(string runtimeBuildCsPath)
	{
		DirectoryInfo? currentDirectory = Directory.GetParent(runtimeBuildCsPath);
		for (int attempt = 0; attempt < 8 && currentDirectory != null; attempt++)
		{
			yield return Path.Combine(currentDirectory.FullName, "Config", CompileOptionsFileName);
			currentDirectory = currentDirectory.Parent;
		}
	}

	private static string ResolveRuntimeBuildCsPath(IUhtExportFactory factory)
	{
		foreach (UhtModule module in factory.Session.Modules)
		{
			if (!module.ShortName.Equals("AngelscriptRuntime", StringComparison.Ordinal))
			{
				continue;
			}

			if (TryFindFirstHeaderPath(module.ScriptPackage, out string? headerPath) && !string.IsNullOrEmpty(headerPath))
			{
				string normalizedHeaderPath = headerPath.Replace('\\', '/');
				string marker = "/Source/AngelscriptRuntime/";
				int markerIndex = normalizedHeaderPath.IndexOf(marker, StringComparison.OrdinalIgnoreCase);
				if (markerIndex >= 0)
				{
					string moduleRoot = normalizedHeaderPath.Substring(0, markerIndex + marker.Length - 1);
					return Path.Combine(moduleRoot, "AngelscriptRuntime.Build.cs");
				}
			}
		}

		throw new InvalidOperationException("Unable to locate AngelscriptRuntime.Build.cs from UHT session modules.");
	}

	private static bool TryFindFirstHeaderPath(UhtType type, out string? headerPath)
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

	private static void DeleteStaleOutputs(IUhtExportFactory factory, HashSet<string> generatedPaths, IReadOnlyDictionary<string, string> allModuleOutputDirectories)
	{
		HashSet<string> livePaths = new(generatedPaths.Select(Path.GetFullPath), StringComparer.OrdinalIgnoreCase);
		string runtimeOutputDirectory = Path.GetDirectoryName(Path.GetFullPath(factory.MakePath("AS_FunctionTable_Stale", ".gen.cpp")))!;
		DeleteStaleFilesInDirectory(runtimeOutputDirectory, "AS_FunctionTable_*.gen.cpp", livePaths);
		DeleteStaleFilesInDirectory(runtimeOutputDirectory, "AS_FunctionTable_*.cpp", livePaths);

		foreach ((string moduleName, string outputDirectory) in allModuleOutputDirectories)
		{
			DeleteStaleFilesInDirectory(outputDirectory, $"AS_FunctionTable_{moduleName}_CrossModule_*.cpp", livePaths);
		}
	}

	private static void DeleteStaleFilesInDirectory(string outputDirectory, string searchPattern, HashSet<string> livePaths)
	{
		if (!Directory.Exists(outputDirectory))
		{
			return;
		}

		foreach (string existingFile in Directory.EnumerateFiles(outputDirectory, searchPattern))
		{
			if (!livePaths.Contains(Path.GetFullPath(existingFile)))
			{
				File.Delete(existingFile);
			}
		}
	}

	private static void CollectEntries(IUhtExportFactory factory, UhtType type, string moduleShortName, bool emitRuntimeShard, bool allowCrossModuleGeneration, SortedSet<string> includes, List<AngelscriptGeneratedFunctionBinding> entries, List<AngelscriptCrossModuleFunctionBinding> crossModuleEntries)
	{
		if (type is UhtClass classObj)
		{
			foreach (UhtType child in classObj.Children)
			{
				if (child is UhtFunction function && ShouldGenerate(classObj, function))
				{
					string includePath = string.Empty;
					if (classObj.HeaderFile != null)
					{
						factory.AddExternalDependency(classObj.HeaderFile.FilePath);
						includePath = factory.GetModuleShortestIncludePath(classObj.HeaderFile.Module, classObj.HeaderFile.FilePath).Replace('\\', '/');
						includes.Add(includePath);
					}

					string eraseMacro = "ERASE_NO_FUNCTION()";
					string entryKind = "Stub";
					string thunkStyle = "Stub";
					if (!emitRuntimeShard && (classObj.ClassType == UhtClassType.Interface || classObj.ClassType == UhtClassType.NativeInterface))
					{
						continue;
					}

					if (classObj.ClassType != UhtClassType.Interface && classObj.ClassType != UhtClassType.NativeInterface)
					{
						if (!emitRuntimeShard)
						{
							if (allowCrossModuleGeneration &&
								TryCreateCrossModuleEntry(factory, moduleShortName, classObj, function, includePath, includes, crossModuleEntries.Count, out AngelscriptCrossModuleFunctionBinding? crossModuleEntry, out _))
							{
								crossModuleEntries.Add(crossModuleEntry!);
								entryKind = "CrossModule";
								thunkStyle = "FrameWrapper";
							}
							else
							{
								continue;
							}
						}
						else if (IsRpcNetFunction(function))
						{
							entryKind = "Stub";
							thunkStyle = "Stub";
						}
						else if (AngelscriptFunctionSignatureBuilder.TryBuild(classObj, function, out AngelscriptFunctionSignature? signature, out string? failureReason))
						{
							eraseMacro = signature!.BuildEraseMacro();
							entryKind = "Direct";
							thunkStyle = "DirectNative";
						}
						else if (failureReason == "unexported-symbol" &&
							allowCrossModuleGeneration &&
							TryCreateCrossModuleEntry(factory, moduleShortName, classObj, function, includePath, includes, crossModuleEntries.Count, out AngelscriptCrossModuleFunctionBinding? crossModuleEntry, out _))
						{
							crossModuleEntries.Add(crossModuleEntry!);
							entryKind = "CrossModule";
							thunkStyle = "FrameWrapper";
						}
					}

					entries.Add(new AngelscriptGeneratedFunctionBinding(classObj.SourceName, function.SourceName, eraseMacro, entryKind, thunkStyle, ResolveEditorOnlyGuard(function)));
				}
			}
		}

		foreach (UhtType child in type.Children)
		{
			CollectEntries(factory, child, moduleShortName, emitRuntimeShard, allowCrossModuleGeneration, includes, entries, crossModuleEntries);
		}
	}

	internal static bool TryClassifyCrossModuleOutcome(UhtClass classObj, UhtFunction function, out AngelscriptFunctionSignature? signature, out string? skippedReason)
	{
		signature = null;
		skippedReason = null;
		if (IsRpcNetFunction(function))
		{
			skippedReason = "rpc-net-function";
			return false;
		}

		if (!AngelscriptHeaderSignatureResolver.TryBuildCrossModule(classObj, function, out signature, out string? crossModuleFailureReason))
		{
			skippedReason = string.IsNullOrEmpty(crossModuleFailureReason) ? "cross-module-signature-unresolved" : crossModuleFailureReason;
			return false;
		}

		if (!IsSafeAutomaticCrossModuleSignature(signature!, classObj, function))
		{
			skippedReason = ClassifyUnsupportedCrossModuleProtocol(signature!, classObj, function);
			return false;
		}

		return true;
	}

	private static bool TryCreateCrossModuleEntry(IUhtExportFactory factory, string moduleShortName, UhtClass classObj, UhtFunction function, string includePath, SortedSet<string> includes, int stableIndex, out AngelscriptCrossModuleFunctionBinding? entry, out string? skippedReason)
	{
		entry = null;
		if (string.IsNullOrEmpty(includePath))
		{
			skippedReason = "target-include-missing";
			return false;
		}

		if (!TryClassifyCrossModuleOutcome(classObj, function, out AngelscriptFunctionSignature? signature, out skippedReason))
		{
			return false;
		}

		SortedSet<string> crossModuleIncludes = new(StringComparer.Ordinal)
		{
			includePath,
		};
		if (!TryCollectCrossModuleReferencedIncludes(factory, signature!, function, crossModuleIncludes, out skippedReason))
		{
			return false;
		}
		foreach (string crossModuleInclude in crossModuleIncludes)
		{
			includes.Add(crossModuleInclude);
		}

		entry = new AngelscriptCrossModuleFunctionBinding(
			moduleShortName,
			classObj.SourceName,
			function.SourceName,
			includePath,
			crossModuleIncludes.ToArray(),
			signature!.ReturnType,
			signature.ParameterTypes,
			signature.IsStatic,
			signature.IsConst,
			HasOutParams(function),
			HasWorldContext(function),
			HasReturnReference(signature!, function),
			stableIndex);
		return true;
	}

	private static bool TryCollectCrossModuleReferencedIncludes(IUhtExportFactory factory, AngelscriptFunctionSignature signature, UhtFunction function, SortedSet<string> includes, out string? skippedReason)
	{
		if (function.ReturnProperty is UhtProperty returnProperty &&
			!TryAddCrossModuleReferencedInclude(factory, returnProperty, includes, out skippedReason))
		{
			return false;
		}
		if (!TryAddCrossModuleStructIncludeByTypeName(factory, signature.ReturnType, includes, out skippedReason))
		{
			return false;
		}

		int parameterIndex = 0;
		foreach (UhtType parameterType in function.ParameterProperties.Span)
		{
			if (parameterType is UhtProperty property &&
				!TryAddCrossModuleReferencedInclude(factory, property, includes, out skippedReason))
			{
				return false;
			}
			if (parameterIndex < signature.ParameterTypes.Count &&
				!TryAddCrossModuleStructIncludeByTypeName(factory, signature.ParameterTypes[parameterIndex], includes, out skippedReason))
			{
				return false;
			}

			parameterIndex++;
		}

		skippedReason = null;
		return true;
	}

	private static bool TryAddCrossModuleReferencedInclude(IUhtExportFactory factory, UhtProperty property, SortedSet<string> includes, out string? skippedReason)
	{
		if (property is not UhtStructProperty structProperty || structProperty.ScriptStruct.IsCoreType)
		{
			skippedReason = null;
			return true;
		}

		string structHeaderPath = structProperty.ScriptStruct.HeaderFile.FilePath;
		if (string.IsNullOrEmpty(structHeaderPath) || !IsSupportedHeader(structHeaderPath))
		{
			skippedReason = "cross-module-struct-header-unavailable";
			return false;
		}

		factory.AddExternalDependency(structHeaderPath);
		includes.Add(factory.GetModuleShortestIncludePath(structProperty.ScriptStruct.HeaderFile.Module, structHeaderPath).Replace('\\', '/'));
		skippedReason = null;
		return true;
	}

	private static bool TryAddCrossModuleStructIncludeByTypeName(IUhtExportFactory factory, string typeName, SortedSet<string> includes, out string? skippedReason)
	{
		string structName = ExtractStructTypeName(typeName);
		if (structName.Length == 0)
		{
			skippedReason = null;
			return true;
		}

		if (factory.Session.FindType(null, UhtFindOptions.SourceName | UhtFindOptions.ScriptStruct, structName) is not UhtScriptStruct scriptStruct ||
			scriptStruct.IsCoreType)
		{
			skippedReason = null;
			return true;
		}

		string structHeaderPath = scriptStruct.HeaderFile.FilePath;
		if (string.IsNullOrEmpty(structHeaderPath) || !IsSupportedHeader(structHeaderPath))
		{
			skippedReason = "cross-module-struct-header-unavailable";
			return false;
		}

		factory.AddExternalDependency(structHeaderPath);
		includes.Add(factory.GetModuleShortestIncludePath(scriptStruct.HeaderFile.Module, structHeaderPath).Replace('\\', '/'));
		skippedReason = null;
		return true;
	}

	private static string ExtractStructTypeName(string typeName)
	{
		string normalized = typeName
			.Replace("const ", string.Empty, StringComparison.Ordinal)
			.Replace("&", string.Empty, StringComparison.Ordinal)
			.Replace("*", string.Empty, StringComparison.Ordinal)
			.Trim();
		if (normalized.StartsWith("struct ", StringComparison.Ordinal))
		{
			normalized = normalized.Substring("struct ".Length).Trim();
		}

		int templateIndex = normalized.IndexOf('<', StringComparison.Ordinal);
		if (templateIndex >= 0)
		{
			return string.Empty;
		}

		if (normalized.StartsWith("::", StringComparison.Ordinal))
		{
			normalized = normalized.Substring(2);
		}

		int scopeIndex = normalized.LastIndexOf("::", StringComparison.Ordinal);
		if (scopeIndex >= 0)
		{
			normalized = normalized.Substring(scopeIndex + 2);
		}

		return normalized.StartsWith("F", StringComparison.Ordinal) ? normalized : string.Empty;
	}

	internal static bool IsSafeAutomaticCrossModuleSignature(AngelscriptFunctionSignature signature, UhtClass classObj, UhtFunction function)
	{
		return IsSafeAutomaticCrossModuleReturn(signature, function) &&
			HasOnlySafeAutomaticCrossModuleParameters(function) &&
			!HasOutParams(function) &&
			!HasWorldContext(function) &&
			!HasReturnReference(signature, function) &&
			!HasScriptMethodMixinProjection(signature, classObj, function);
	}

	private static string ClassifyUnsupportedCrossModuleProtocol(AngelscriptFunctionSignature signature, UhtClass classObj, UhtFunction function)
	{
		if (HasWorldContext(function))
		{
			return "needs-world-context-policy";
		}

		if (HasOutParams(function) || HasReferenceParameters(function))
		{
			return "needs-out-param-protocol";
		}

		if (HasReturnReference(signature, function))
		{
			return "needs-ref-return-protocol";
		}

		if (HasStaticArrayParameter(function) || ReturnsStaticArray(function))
		{
			return "needs-static-array-protocol";
		}

		if (HasContainerParameter(function) || ReturnsContainer(function))
		{
			return "needs-container-frame-protocol";
		}

		if (HasInterfaceParameter(function) || ReturnsInterface(function))
		{
			return "needs-interface-frame-protocol";
		}

		if (HasDelegateParameter(function) || ReturnsDelegate(function))
		{
			return "needs-delegate-frame-protocol";
		}

		if (HasFieldPathParameter(function) || ReturnsFieldPath(function))
		{
			return "needs-field-path-frame-protocol";
		}

		if (HasScriptMethodMixinProjection(signature, classObj, function))
		{
			return "needs-script-this-projection";
		}

		return "cross-module-unsupported-signature";
	}

	private static bool HasScriptMethodMixinProjection(AngelscriptFunctionSignature signature, UhtClass classObj, UhtFunction function)
	{
		return signature.IsStatic &&
			(function.MetaData.ContainsKey("ScriptMethod") || classObj.MetaData.ContainsKey("ScriptMixin"));
	}

	private static bool IsSafeAutomaticCrossModuleReturn(AngelscriptFunctionSignature signature, UhtFunction function)
	{
		if (signature.ReturnType == "void")
		{
			return true;
		}

		if (function.ReturnProperty is not UhtProperty returnProperty)
		{
			return false;
		}

		if (returnProperty is UhtBoolProperty ||
			returnProperty is UhtNumericProperty ||
			returnProperty is UhtEnumProperty ||
			returnProperty is UhtStructProperty ||
			returnProperty is UhtStrProperty ||
			returnProperty is UhtNameProperty ||
			returnProperty is UhtTextProperty)
		{
			return true;
		}

		return returnProperty is UhtObjectProperty && signature.ReturnType.EndsWith("*", StringComparison.Ordinal);
	}

	private static bool HasOnlySafeAutomaticCrossModuleParameters(UhtFunction function)
	{
		foreach (UhtType parameterType in function.ParameterProperties.Span)
		{
			if (parameterType is not UhtProperty property || !IsSafeAutomaticCrossModuleParameter(property))
			{
				return false;
			}
		}

		return true;
	}

	private static bool IsSafeAutomaticCrossModuleParameter(UhtProperty property)
	{
		if (property.ArrayDimensions != null)
		{
			return false;
		}

		if (property is UhtBoolProperty ||
			property is UhtNumericProperty ||
			property is UhtEnumProperty ||
			property is UhtStructProperty ||
			property is UhtStrProperty ||
			property is UhtNameProperty ||
			property is UhtTextProperty ||
			property is UhtObjectProperty ||
			property is UhtClassProperty ||
			property is UhtSoftObjectProperty ||
			property is UhtWeakObjectPtrProperty)
		{
			return true;
		}

		return false;
	}

	private static bool IsRpcNetFunction(UhtFunction function)
	{
		return function.FunctionFlags.HasAnyFlags(
			EFunctionFlags.Net |
			EFunctionFlags.NetServer |
			EFunctionFlags.NetClient |
			EFunctionFlags.NetMulticast);
	}

	private static bool HasWorldContext(UhtFunction function)
	{
		return function.MetaData.ContainsKey("WorldContext");
	}

	private static bool HasOutParams(UhtFunction function)
	{
		foreach (UhtType parameterType in function.ParameterProperties.Span)
		{
			if (parameterType is UhtProperty property && property.PropertyFlags.ToString().Contains("OutParm", StringComparison.Ordinal))
			{
				return true;
			}
		}

		return false;
	}

	private static bool HasReferenceParameters(UhtFunction function)
	{
		foreach (UhtType parameterType in function.ParameterProperties.Span)
		{
			if (parameterType is UhtProperty property && property.PropertyFlags.ToString().Contains("ReferenceParm", StringComparison.Ordinal))
			{
				return true;
			}
		}

		return false;
	}

	private static bool HasStaticArrayParameter(UhtFunction function)
	{
		foreach (UhtType parameterType in function.ParameterProperties.Span)
		{
			if (parameterType is UhtProperty property && property.ArrayDimensions != null)
			{
				return true;
			}
		}

		return false;
	}

	private static bool ReturnsStaticArray(UhtFunction function)
	{
		return function.ReturnProperty is UhtProperty returnProperty && returnProperty.ArrayDimensions != null;
	}

	private static bool HasContainerParameter(UhtFunction function)
	{
		foreach (UhtType parameterType in function.ParameterProperties.Span)
		{
			if (parameterType is UhtContainerBaseProperty)
			{
				return true;
			}
		}

		return false;
	}

	private static bool ReturnsContainer(UhtFunction function)
	{
		return function.ReturnProperty is UhtContainerBaseProperty;
	}

	private static bool HasInterfaceParameter(UhtFunction function)
	{
		foreach (UhtType parameterType in function.ParameterProperties.Span)
		{
			if (parameterType is UhtInterfaceProperty)
			{
				return true;
			}
		}

		return false;
	}

	private static bool ReturnsInterface(UhtFunction function)
	{
		return function.ReturnProperty is UhtInterfaceProperty;
	}

	private static bool HasDelegateParameter(UhtFunction function)
	{
		foreach (UhtType parameterType in function.ParameterProperties.Span)
		{
			if (parameterType is UhtDelegateProperty or UhtMulticastDelegateProperty)
			{
				return true;
			}
		}

		return false;
	}

	private static bool ReturnsDelegate(UhtFunction function)
	{
		return function.ReturnProperty is UhtDelegateProperty or UhtMulticastDelegateProperty;
	}

	private static bool HasFieldPathParameter(UhtFunction function)
	{
		foreach (UhtType parameterType in function.ParameterProperties.Span)
		{
			if (parameterType is UhtFieldPathProperty)
			{
				return true;
			}
		}

		return false;
	}

	private static bool ReturnsFieldPath(UhtFunction function)
	{
		return function.ReturnProperty is UhtFieldPathProperty;
	}

	private static bool ReturnsByRef(UhtFunction function)
	{
		return function.ReturnProperty is UhtProperty returnProperty &&
			returnProperty.PropertyFlags.ToString().Contains("ReferenceParm", StringComparison.Ordinal);
	}

	private static bool HasReturnReference(AngelscriptFunctionSignature signature, UhtFunction function)
	{
		return ReturnsByRef(function) || signature.ReturnType.Contains("&", StringComparison.Ordinal);
	}

	// Editor-only UFUNCTIONs (declared inside #if WITH_EDITOR / WITH_EDITORONLY_DATA) do not
	// exist when compiling a non-editor (packaged) target, so a direct bind to their C++ symbol
	// fails to compile. Mirror the exact preprocessor guard UHT used for the declaration so the
	// generated registration line is excluded in non-editor builds and kept in editor builds.
	private static string? ResolveEditorOnlyGuard(UhtFunction function)
	{
		if (!function.FunctionFlags.HasAnyFlags(EFunctionFlags.EditorOnly))
		{
			return null;
		}

		bool isWithEditor = (function.DefineScope & UhtDefineScope.Editor) != 0;
		bool isWithEditorOnlyData = (function.DefineScope & UhtDefineScope.EditorOnlyData) != 0;
		if (isWithEditorOnlyData && !isWithEditor)
		{
			return "WITH_EDITORONLY_DATA";
		}

		return "WITH_EDITOR";
	}

	private static bool ShouldGenerate(UhtClass classObj, UhtFunction function)
	{
		if (classObj.HeaderFile == null || !IsSupportedHeader(classObj.HeaderFile.FilePath))
		{
			return false;
		}

		if (!AngelscriptFunctionTableExporter.IsAngelscriptCallable(function))
		{
			return false;
		}

		if (function.MetaData.ContainsKey("NotInAngelscript") ||
			(function.MetaData.ContainsKey("BlueprintInternalUseOnly") && !function.MetaData.ContainsKey("UsableInAngelscript")))
		{
			return false;
		}

		if (classObj.SourceName == "UUniversalObjectLocatorScriptingExtensions" &&
			(function.SourceName == "MakeUniversalObjectLocator" || function.SourceName == "UniversalObjectLocatorFromString"))
		{
			return false;
		}

		return !function.FunctionExportFlags.ToString().Contains("CustomThunk", StringComparison.Ordinal);
	}

	private static bool IsSupportedHeader(string headerPath)
	{
		string normalizedHeaderPath = headerPath.Replace('\\', '/');
		if (normalizedHeaderPath.Contains("/Private/", StringComparison.OrdinalIgnoreCase))
		{
			return false;
		}

		if (normalizedHeaderPath.EndsWith("/Components/InstancedSkinnedMeshComponent.h", StringComparison.OrdinalIgnoreCase))
		{
			return false;
		}

		return true;
	}
}
