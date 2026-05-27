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

internal sealed record AngelscriptGeneratedFunctionEntry(
	string ClassName,
	string FunctionName,
	string EraseMacro,
	string EntryKind,
	string ThunkStyle)
{
	public string BuildRegistrationLine()
	{
		return $"\tFAngelscriptBinds::AddFunctionEntry({ClassName}::StaticClass(), \"{FunctionName}\", {{ {EraseMacro} }});";
	}
}

internal sealed record AngelscriptCrossModuleFunctionEntry(
	string ModuleName,
	string ClassName,
	string FunctionName,
	string IncludePath,
	string ReturnType,
	IReadOnlyList<string> ParameterTypes,
	bool IsStatic,
	bool IsConst,
	bool HasOutParams,
	bool HasWorldContext,
	bool ReturnsByRef,
	int StableIndex);

internal sealed record AngelscriptSupportedModules(
	HashSet<string> All,
	HashSet<string> EditorOnly);

internal sealed record AngelscriptModuleGenerationSummary(
	string ModuleName,
	bool EditorOnly,
	int TotalEntries,
	int DirectBindEntries,
	int StubEntries,
	int CrossModuleEntries,
	int ShardCount);

internal sealed record AngelscriptGeneratedFunctionCsvEntry(
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

	public static int Generate(IUhtExportFactory factory)
	{
		AngelscriptSupportedModules supportedModules = LoadSupportedModules(factory);
		string layoutVersion = LoadCrossModuleLayoutVersion(factory);
		int generatedFileCount = 0;
		HashSet<string> generatedPaths = new(StringComparer.OrdinalIgnoreCase);
		List<AngelscriptModuleGenerationSummary> moduleSummaries = new();
		List<AngelscriptGeneratedFunctionCsvEntry> csvEntries = new();

		if (TryEmitCrossModuleLinkProbe(factory, generatedPaths, layoutVersion))
		{
			generatedFileCount++;
		}

		Dictionary<string, string> supportedModuleOutputDirectories = new(StringComparer.OrdinalIgnoreCase);
		foreach (UhtModule module in factory.Session.Modules)
		{
			if (!supportedModules.All.Contains(module.ShortName))
			{
				continue;
			}

			supportedModuleOutputDirectories[module.ShortName] = module.Module.OutputDirectory;
			AngelscriptModuleGenerationSummary? moduleSummary = GenerateModule(factory, module, supportedModules.EditorOnly.Contains(module.ShortName), layoutVersion, generatedPaths, csvEntries);
			if (moduleSummary != null)
			{
				generatedFileCount += moduleSummary.ShardCount;
				moduleSummaries.Add(moduleSummary);
			}
		}

		DeleteStaleOutputs(factory, generatedPaths, supportedModuleOutputDirectories);
		WriteGenerationSummary(factory, moduleSummaries, csvEntries, generatedFileCount);
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
		builder.AppendLine("\tstruct FProbeEntry");
		builder.AppendLine("\t{");
		builder.AppendLine("\t\tconst TCHAR* Tag;");
		builder.AppendLine("\t\tuint32 Magic;");
		builder.AppendLine("\t};");
		builder.AppendLine();
		builder.AppendLine("\tstruct FProbeFeature : public IModularFeature");
		builder.AppendLine("\t{");
		builder.AppendLine("\t\tconst FProbeEntry* Entries;");
		builder.AppendLine("\t\tint32 Count;");
		builder.AppendLine("\t\tconst TCHAR* ModuleName;");
		builder.AppendLine("\t\tuint32 LayoutVersion;");
		builder.AppendLine();
		builder.AppendLine("\t\tFProbeFeature(const FProbeEntry* InEntries, int32 InCount, const TCHAR* InModuleName, uint32 InLayoutVersion)");
		builder.AppendLine("\t\t\t: Entries(InEntries)");
		builder.AppendLine("\t\t\t, Count(InCount)");
		builder.AppendLine("\t\t\t, ModuleName(InModuleName)");
		builder.AppendLine("\t\t\t, LayoutVersion(InLayoutVersion)");
		builder.AppendLine("\t\t{");
		builder.AppendLine("\t\t}");
		builder.AppendLine("\t};");
		builder.AppendLine();
		builder.AppendLine("\tstatic const FProbeEntry GProbeTable[] =");
		builder.AppendLine("\t{");
		builder.AppendLine("\t\t{ TEXT(\"Engine.Probe\"), GProbeLayoutVersion },");
		builder.AppendLine("\t};");
		builder.AppendLine();
		builder.AppendLine("\tstatic FProbeFeature GProbeFeature(GProbeTable, 1, TEXT(\"Engine\"), GProbeLayoutVersion);");
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

	private static AngelscriptModuleGenerationSummary? GenerateModule(IUhtExportFactory factory, UhtModule module, bool editorOnly, string layoutVersion, HashSet<string> generatedPaths, List<AngelscriptGeneratedFunctionCsvEntry> csvEntries)
	{
		SortedSet<string> includes = new(StringComparer.Ordinal);
		List<AngelscriptGeneratedFunctionEntry> entries = new();
		List<AngelscriptCrossModuleFunctionEntry> crossModuleEntries = new();

		CollectEntries(factory, module.ScriptPackage, module.ShortName, includes, entries, crossModuleEntries);
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
		foreach (AngelscriptGeneratedFunctionEntry entry in entries)
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

		int shardCount = entries.Count == 0 ? 0 : (entries.Count + MaxEntriesPerShard - 1) / MaxEntriesPerShard;
		for (int shardIndex = 0; shardIndex < shardCount; shardIndex++)
		{
			int startIndex = shardIndex * MaxEntriesPerShard;
			int entryCount = Math.Min(MaxEntriesPerShard, entries.Count - startIndex);
			string outputPath = factory.MakePath($"AS_FunctionTable_{module.ShortName}_{shardIndex:D3}", ".cpp");
			factory.CommitOutput(outputPath, BuildShard(module.ShortName, editorOnly, includes, entries, startIndex, entryCount, shardIndex, shardCount));
			generatedPaths.Add(outputPath);
			generatedShardCount++;

			for (int entryIndex = startIndex; entryIndex < startIndex + entryCount; entryIndex++)
			{
				AngelscriptGeneratedFunctionEntry entry = entries[entryIndex];
				csvEntries.Add(new AngelscriptGeneratedFunctionCsvEntry(
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

	private static void WriteGenerationSummary(IUhtExportFactory factory, List<AngelscriptModuleGenerationSummary> moduleSummaries, List<AngelscriptGeneratedFunctionCsvEntry> csvEntries, int generatedFileCount)
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

	private static void WriteEntryCsv(IUhtExportFactory factory, List<AngelscriptGeneratedFunctionCsvEntry> csvEntries)
	{
		string csvPath = factory.MakePath("AS_FunctionTable_Entries", ".csv");
		Directory.CreateDirectory(Path.GetDirectoryName(csvPath)!);

		StringBuilder builder = new();
		builder.AppendLine("ModuleName,EditorOnly,ClassName,FunctionName,EntryKind,EraseMacro,ShardIndex,ThunkStyle");
		foreach (AngelscriptGeneratedFunctionCsvEntry entry in csvEntries)
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

	private static StringBuilder BuildShard(string moduleShortName, bool editorOnly, SortedSet<string> includes, List<AngelscriptGeneratedFunctionEntry> entries, int startIndex, int entryCount, int shardIndex, int shardCount)
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
			builder.AppendLine(entries[entryIndex].BuildRegistrationLine());
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

	private static StringBuilder BuildCrossModuleShard(string moduleShortName, List<AngelscriptCrossModuleFunctionEntry> entries, int startIndex, int entryCount, int shardIndex, int shardCount, string layoutVersion)
	{
		StringBuilder builder = new();
		SortedSet<string> includes = new(StringComparer.Ordinal);
		for (int entryIndex = startIndex; entryIndex < startIndex + entryCount; entryIndex++)
		{
			includes.Add(entries[entryIndex].IncludePath);
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
		builder.AppendLine("\tstruct FCrossModuleEntry");
		builder.AppendLine("\t{");
		builder.AppendLine("\t\tconst TCHAR* ClassName;");
		builder.AppendLine("\t\tconst TCHAR* FunctionName;");
		builder.AppendLine("\t\tvoid (*Thunk)(UObject* Self, FCrossModuleCallFrame* Frame);");
		builder.AppendLine("\t\tuint16 ArgCount;");
		builder.AppendLine("\t\tuint16 RetSize;");
		builder.AppendLine("\t\tuint32 Flags;");
		builder.AppendLine("\t};");
		builder.AppendLine();
		builder.AppendLine("\tstruct FCrossModuleFeature : public IModularFeature");
		builder.AppendLine("\t{");
		builder.AppendLine("\t\tconst FCrossModuleEntry* Table;");
		builder.AppendLine("\t\tint32 Count;");
		builder.AppendLine("\t\tconst TCHAR* ModuleName;");
		builder.AppendLine("\t\tuint32 LayoutVersion;");
		builder.AppendLine();
		builder.AppendLine("\t\tFCrossModuleFeature(const FCrossModuleEntry* InTable, int32 InCount, const TCHAR* InModuleName, uint32 InLayoutVersion)");
		builder.AppendLine("\t\t\t: Table(InTable)");
		builder.AppendLine("\t\t\t, Count(InCount)");
		builder.AppendLine("\t\t\t, ModuleName(InModuleName)");
		builder.AppendLine("\t\t\t, LayoutVersion(InLayoutVersion)");
		builder.AppendLine("\t\t{");
		builder.AppendLine("\t\t}");
		builder.AppendLine("\t};");
		builder.AppendLine();
		builder.AppendLine("\tstatic_assert(sizeof(FCrossModuleCallFrame) == 48, \"FCrossModuleCallFrame ABI layout changed; bump cross-module-layout-version.txt.\");");
		builder.AppendLine("\tstatic_assert(sizeof(FCrossModuleEntry) == 32, \"FCrossModuleEntry ABI layout changed; bump cross-module-layout-version.txt.\");");
		builder.AppendLine("\tstatic_assert(sizeof(FCrossModuleFeature) == 32, \"FCrossModuleFeature ABI layout changed; bump cross-module-layout-version.txt.\");");
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
			AngelscriptCrossModuleFunctionEntry entry = entries[entryIndex];
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

		builder.AppendLine("\tstatic const FCrossModuleEntry GCrossModuleTable[] =");
		builder.AppendLine("\t{");
		for (int entryIndex = startIndex; entryIndex < startIndex + entryCount; entryIndex++)
		{
			AngelscriptCrossModuleFunctionEntry entry = entries[entryIndex];
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
		builder.AppendLine("\tstatic FCrossModuleFeature GCrossModuleFeature(GCrossModuleTable, UE_ARRAY_COUNT(GCrossModuleTable), TEXT(\"" + moduleShortName + "\"), GCrossModuleLayoutVersion);");
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
		builder.AppendLine("\t\t\tIModularFeatures::Get().RegisterModularFeature(FName(TEXT(\"AngelscriptCrossModuleBindings\")), &GCrossModuleFeature);");
		builder.AppendLine("\t\t\tFCoreDelegates::OnPreExit.AddStatic(&MarkCrossModuleShutdown);");
		builder.AppendLine("\t\t}");
		builder.AppendLine();
		builder.AppendLine("\t\t~FCrossModuleAutoRegistration()");
		builder.AppendLine("\t\t{");
		builder.AppendLine("\t\t\tif (!GCrossModuleShuttingDown)");
		builder.AppendLine("\t\t\t{");
		builder.AppendLine("\t\t\t\tIModularFeatures::Get().UnregisterModularFeature(FName(TEXT(\"AngelscriptCrossModuleBindings\")), &GCrossModuleFeature);");
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

	private static string BuildThunkName(AngelscriptCrossModuleFunctionEntry entry)
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

	private static string BuildCrossModuleCallArguments(AngelscriptCrossModuleFunctionEntry entry)
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

	private static string BuildCrossModuleFlagsExpression(AngelscriptCrossModuleFunctionEntry entry)
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

		HashSet<string> allModules = new(StringComparer.OrdinalIgnoreCase)
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
					allModules.Add(moduleName);
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

		return new AngelscriptSupportedModules(allModules, editorOnlyModules);
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

	private static void DeleteStaleOutputs(IUhtExportFactory factory, HashSet<string> generatedPaths, IReadOnlyDictionary<string, string> supportedModuleOutputDirectories)
	{
		HashSet<string> livePaths = new(generatedPaths.Select(Path.GetFullPath), StringComparer.OrdinalIgnoreCase);
		string runtimeOutputDirectory = Path.GetDirectoryName(Path.GetFullPath(factory.MakePath("AS_FunctionTable_Stale", ".cpp")))!;
		DeleteStaleFilesInDirectory(runtimeOutputDirectory, "AS_FunctionTable_*.cpp", livePaths);

		foreach ((string moduleName, string outputDirectory) in supportedModuleOutputDirectories)
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

	private static void CollectEntries(IUhtExportFactory factory, UhtType type, string moduleShortName, SortedSet<string> includes, List<AngelscriptGeneratedFunctionEntry> entries, List<AngelscriptCrossModuleFunctionEntry> crossModuleEntries)
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
					if (classObj.ClassType != UhtClassType.Interface && classObj.ClassType != UhtClassType.NativeInterface)
					{
						if (IsRpcNetFunction(function))
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
							TryCreateCrossModuleEntry(factory, moduleShortName, classObj, function, includePath, crossModuleEntries.Count, out AngelscriptCrossModuleFunctionEntry? crossModuleEntry, out _))
						{
							crossModuleEntries.Add(crossModuleEntry!);
							entryKind = "CrossModule";
							thunkStyle = "FrameWrapper";
						}
					}

					entries.Add(new AngelscriptGeneratedFunctionEntry(classObj.SourceName, function.SourceName, eraseMacro, entryKind, thunkStyle));
				}
			}
		}

		foreach (UhtType child in type.Children)
		{
			CollectEntries(factory, child, moduleShortName, includes, entries, crossModuleEntries);
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

	private static bool TryCreateCrossModuleEntry(IUhtExportFactory factory, string moduleShortName, UhtClass classObj, UhtFunction function, string includePath, int stableIndex, out AngelscriptCrossModuleFunctionEntry? entry, out string? skippedReason)
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

		entry = new AngelscriptCrossModuleFunctionEntry(
			moduleShortName,
			classObj.SourceName,
			function.SourceName,
			includePath,
			signature!.ReturnType,
			signature.ParameterTypes,
			signature.IsStatic,
			signature.IsConst,
			HasOutParams(function),
			HasWorldContext(function),
			ReturnsByRef(function),
			stableIndex);
		return true;
	}

	internal static bool IsSafeAutomaticCrossModuleSignature(AngelscriptFunctionSignature signature, UhtClass classObj, UhtFunction function)
	{
		return IsSafeAutomaticCrossModuleReturn(signature, function) &&
			HasOnlySafeAutomaticCrossModuleParameters(function) &&
			!HasOutParams(function) &&
			!HasWorldContext(function) &&
			!ReturnsByRef(function) &&
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

		if (ReturnsByRef(function))
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

	private static bool ReturnsByRef(UhtFunction function)
	{
		return function.ReturnProperty is UhtProperty returnProperty &&
			returnProperty.PropertyFlags.ToString().Contains("ReferenceParm", StringComparison.Ordinal);
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
