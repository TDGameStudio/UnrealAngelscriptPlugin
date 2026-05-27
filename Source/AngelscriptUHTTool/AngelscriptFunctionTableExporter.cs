using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using EpicGames.UHT.Tables;
using EpicGames.Core;
using EpicGames.UHT.Types;
using EpicGames.UHT.Utils;

namespace AngelscriptUHTTool;

[UnrealHeaderTool]
internal static class AngelscriptFunctionTableExporter
{
	private sealed record AngelscriptSkippedFunctionEntry(
		string ModuleName,
		string ClassName,
		string FunctionName,
		string FailureReason);

	[UhtExporter(
		Name = "AngelscriptFunctionTable",
		Description = "Exports Angelscript function table data",
		Options = UhtExporterOptions.Default | UhtExporterOptions.CompileOutput,
		CppFilters = ["AS_FunctionTable_*.cpp"],
		ModuleName = "AngelscriptRuntime")]
	private static void Export(IUhtExportFactory factory)
	{
		int packageCount = 0;
		int classCount = 0;
		int functionCount = 0;
		int reconstructedCount = 0;
		int skippedCount = 0;
		int crossModuleReconstructedCount = 0;
		List<AngelscriptSkippedFunctionEntry> skippedEntries = new();
		AngelscriptSupportedModules supportedModules = AngelscriptFunctionTableCodeGenerator.LoadSupportedModules(factory);
		int generatedFileCount = AngelscriptFunctionTableCodeGenerator.Generate(factory);

		foreach (UhtModule module in factory.Session.Modules)
		{
			packageCount++;
			CountAngelscriptCallableFunctions(module.ShortName, module.ScriptPackage, supportedModules, skippedEntries, ref classCount, ref functionCount, ref reconstructedCount, ref crossModuleReconstructedCount, ref skippedCount);
		}

		WriteSkippedEntriesCsv(factory, skippedEntries);
		WriteSkippedReasonSummaryCsv(factory, skippedEntries);

		Console.WriteLine(
			"AngelscriptUHTTool exporter visited {0} packages, {1} classes, {2} AS-callable functions, reconstructed {3}, reconstructed cross-module {4}, skipped {5}, wrote {6} module files.",
			packageCount,
			classCount,
			functionCount,
			reconstructedCount,
			crossModuleReconstructedCount,
			skippedCount,
			generatedFileCount);
	}

	internal static bool IsAngelscriptCallable(UhtFunction function)
	{
		string functionFlags = function.FunctionFlags.ToString();

		return function.FunctionType == UhtFunctionType.Function &&
			(functionFlags.Contains("BlueprintCallable", StringComparison.Ordinal) ||
			functionFlags.Contains("BlueprintPure", StringComparison.Ordinal) ||
			function.MetaData.ContainsKey("ScriptCallable"));
	}

	private static void CountAngelscriptCallableFunctions(string moduleName, UhtType type, AngelscriptSupportedModules supportedModules, List<AngelscriptSkippedFunctionEntry> skippedEntries, ref int classCount, ref int functionCount, ref int reconstructedCount, ref int crossModuleReconstructedCount, ref int skippedCount)
	{
		if (type is UhtClass classObj)
		{
			classCount++;
			foreach (UhtType child in classObj.Children)
			{
				if (child is UhtFunction function && IsAngelscriptCallable(function))
				{
					functionCount++;
					if (IsRpcNetFunction(function))
					{
						skippedCount++;
						skippedEntries.Add(new AngelscriptSkippedFunctionEntry(
							moduleName,
							classObj.SourceName,
							function.SourceName,
							"rpc-net-function"));
						continue;
					}

					if (AngelscriptFunctionSignatureBuilder.TryBuild(classObj, function, out AngelscriptFunctionSignature? signature, out string? failureReason))
					{
						_ = signature!.BuildEraseMacro();
						reconstructedCount++;
					}
					else if (TryClassifyCrossModuleOutcome(moduleName, supportedModules, classObj, function, failureReason, out string? crossModuleFailureReason))
					{
						if (crossModuleFailureReason == null)
						{
							crossModuleReconstructedCount++;
						}
						else
						{
							skippedCount++;
							skippedEntries.Add(new AngelscriptSkippedFunctionEntry(
								moduleName,
								classObj.SourceName,
								function.SourceName,
								crossModuleFailureReason));
						}
					}
					else
					{
						skippedCount++;
						skippedEntries.Add(new AngelscriptSkippedFunctionEntry(
							moduleName,
							classObj.SourceName,
							function.SourceName,
							string.IsNullOrEmpty(failureReason) ? "unknown" : failureReason));
					}
				}
			}
		}

		foreach (UhtType child in type.Children)
		{
			CountAngelscriptCallableFunctions(moduleName, child, supportedModules, skippedEntries, ref classCount, ref functionCount, ref reconstructedCount, ref crossModuleReconstructedCount, ref skippedCount);
		}
	}

	private static bool TryClassifyCrossModuleOutcome(string moduleName, AngelscriptSupportedModules supportedModules, UhtClass classObj, UhtFunction function, string? directFailureReason, out string? skippedReason)
	{
		skippedReason = null;
		if (!string.Equals(directFailureReason, "unexported-symbol", StringComparison.Ordinal))
		{
			return false;
		}

		bool runtimeLinked = supportedModules.RuntimeLinked.Contains(moduleName);
		bool crossModuleOnly = supportedModules.CrossModuleOnly.Contains(moduleName);
		if (!runtimeLinked && !crossModuleOnly)
		{
			if (AngelscriptFunctionTableCodeGenerator.TryClassifyCrossModuleOutcome(classObj, function, out _, out string? disabledModuleReason))
			{
				skippedReason = "disabled-safe-cross-module";
			}
			else
			{
				skippedReason = string.IsNullOrEmpty(disabledModuleReason)
					? "target-module-disabled"
					: $"disabled-{disabledModuleReason}";
			}
			return true;
		}

		if (IsRpcNetFunction(function))
		{
			skippedReason = "rpc-net-function";
			return true;
		}

		if (!AngelscriptFunctionTableCodeGenerator.TryClassifyCrossModuleOutcome(classObj, function, out _, out skippedReason))
		{
			return true;
		}

		return true;
	}

	private static bool IsRpcNetFunction(UhtFunction function)
	{
		return function.FunctionFlags.HasAnyFlags(
			EFunctionFlags.Net |
			EFunctionFlags.NetServer |
			EFunctionFlags.NetClient |
			EFunctionFlags.NetMulticast);
	}

	private static void WriteSkippedEntriesCsv(IUhtExportFactory factory, List<AngelscriptSkippedFunctionEntry> skippedEntries)
	{
		string csvPath = factory.MakePath("AS_FunctionTable_SkippedEntries", ".csv");
		Directory.CreateDirectory(Path.GetDirectoryName(csvPath)!);

		skippedEntries.Sort(static (left, right) =>
		{
			int moduleComparison = StringComparer.Ordinal.Compare(left.ModuleName, right.ModuleName);
			if (moduleComparison != 0)
			{
				return moduleComparison;
			}

			int classComparison = StringComparer.Ordinal.Compare(left.ClassName, right.ClassName);
			if (classComparison != 0)
			{
				return classComparison;
			}

			int functionComparison = StringComparer.Ordinal.Compare(left.FunctionName, right.FunctionName);
			return functionComparison != 0
				? functionComparison
				: StringComparer.Ordinal.Compare(left.FailureReason, right.FailureReason);
		});

		StringBuilder builder = new();
		builder.AppendLine("ModuleName,ClassName,FunctionName,FailureReason");
		foreach (AngelscriptSkippedFunctionEntry entry in skippedEntries)
		{
			builder
				.Append(EscapeCsv(entry.ModuleName)).Append(',')
				.Append(EscapeCsv(entry.ClassName)).Append(',')
				.Append(EscapeCsv(entry.FunctionName)).Append(',')
				.Append(EscapeCsv(entry.FailureReason))
				.Append("\r\n");
		}

		File.WriteAllText(csvPath, builder.ToString(), Encoding.UTF8);
		Console.WriteLine("AngelscriptUHTTool skipped entry dump written: {0} ({1} rows)", csvPath, skippedEntries.Count);
	}

	private static void WriteSkippedReasonSummaryCsv(IUhtExportFactory factory, List<AngelscriptSkippedFunctionEntry> skippedEntries)
	{
		string csvPath = factory.MakePath("AS_FunctionTable_SkippedReasonSummary", ".csv");
		Directory.CreateDirectory(Path.GetDirectoryName(csvPath)!);

		StringBuilder builder = new();
		builder.AppendLine("FailureReason,SkippedCount");

		foreach (var reasonGroup in skippedEntries
			.GroupBy(static entry => entry.FailureReason, StringComparer.Ordinal)
			.OrderByDescending(static group => group.Count())
			.ThenBy(static group => group.Key, StringComparer.Ordinal))
		{
			builder
				.Append(EscapeCsv(reasonGroup.Key))
				.Append(',')
				.Append(reasonGroup.Count())
				.Append("\r\n");
		}

		File.WriteAllText(csvPath, builder.ToString(), Encoding.UTF8);
		Console.WriteLine("AngelscriptUHTTool skipped reason summary written: {0} ({1} reasons)", csvPath, skippedEntries.Select(static entry => entry.FailureReason).Distinct(StringComparer.Ordinal).Count());
	}

	private static string EscapeCsv(string value)
	{
		if (value.IndexOfAny(new[] { ',', '"', '\r', '\n' }) == -1)
		{
			return value;
		}

		return '"' + value.Replace("\"", "\"\"") + '"';
	}
}
