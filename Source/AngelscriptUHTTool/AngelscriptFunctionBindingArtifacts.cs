using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;
using EpicGames.UHT.Utils;

namespace AngelscriptUHTTool;

internal static partial class AngelscriptFunctionBindingCodeGenerator
{
	private static void WriteStatistics(
		IUhtExportFactory factory,
		AngelscriptFunctionBindingModuleConfiguration supportedModules,
		List<AngelscriptFunctionBindingModuleStatistics> summaries,
		List<AngelscriptFunctionBindingDiagnosticRow> generatedDiagnostics,
		List<AngelscriptSkippedFunctionDiagnostic> skippedDiagnostics,
		int generatedFileCount)
	{
		int totalAnalyzedFunctions = summaries.Sum(static summary => summary.TotalAnalyzedFunctions);
		int totalNativeRuntimeLinkedCount = summaries.Sum(static summary => summary.NativeRuntimeLinkedCount);
		int totalReflectiveFallbackCount = summaries.Sum(static summary => summary.ReflectiveFallbackCount);
		int totalNativeModuleFunctionAddressCount = summaries.Sum(static summary => summary.NativeModuleFunctionAddressCount);
		int outcomeCount = totalNativeRuntimeLinkedCount + totalReflectiveFallbackCount + totalNativeModuleFunctionAddressCount + skippedDiagnostics.Count;
		if (outcomeCount != totalAnalyzedFunctions)
		{
			throw new InvalidOperationException($"Function binding diagnostics do not reconcile: analyzed={totalAnalyzedFunctions}, nativeRuntimeLinked={totalNativeRuntimeLinkedCount}, reflectiveFallback={totalReflectiveFallbackCount}, nativeModuleFunctionAddress={totalNativeModuleFunctionAddressCount}, skipped={skippedDiagnostics.Count}.");
		}

		AngelscriptFunctionBindingStatistics statistics = new(
			totalAnalyzedFunctions,
			totalNativeRuntimeLinkedCount,
			totalReflectiveFallbackCount,
			totalNativeModuleFunctionAddressCount,
			skippedDiagnostics.Count,
			supportedModules.ConfiguredModuleMissCount,
			generatedFileCount);
		string statisticsPath = factory.MakePath("AS_FunctionBindingStatistics", ".json");
		string statisticsJson = JsonSerializer.Serialize(new
		{
			functionBindingMethod = supportedModules.FunctionBindingMethod.ToString(),
			compileOptionsPath = supportedModules.CompileOptionsPath,
			engineDistribution = supportedModules.EngineDistribution,
			totalAnalyzedFunctions = statistics.TotalAnalyzedFunctions,
			totalNativeRuntimeLinkedCount = statistics.NativeRuntimeLinkedCount,
			totalReflectiveFallbackCount = statistics.ReflectiveFallbackCount,
			totalNativeModuleFunctionAddressCount = statistics.NativeModuleFunctionAddressCount,
			nativeRuntimeLinkedRate = GetRate(statistics.NativeRuntimeLinkedCount, statistics.TotalAnalyzedFunctions),
			reflectiveFallbackRate = GetRate(statistics.ReflectiveFallbackCount, statistics.TotalAnalyzedFunctions),
			nativeModuleFunctionAddressRate = GetRate(statistics.NativeModuleFunctionAddressCount, statistics.TotalAnalyzedFunctions),
			totalSkippedFunctionCount = statistics.SkippedFunctionCount,
			configuredModuleMissCount = statistics.ConfiguredModuleMissCount,
			configuredModuleMisses = supportedModules.ConfiguredModuleMisses,
			totalShardCount = statistics.TotalShardCount,
			modules = summaries.Select(static summary => new
			{
				moduleName = summary.ModuleName,
				editorOnly = summary.EditorOnly,
				totalAnalyzedFunctions = summary.TotalAnalyzedFunctions,
				nativeRuntimeLinkedCount = summary.NativeRuntimeLinkedCount,
				reflectiveFallbackCount = summary.ReflectiveFallbackCount,
				nativeModuleFunctionAddressCount = summary.NativeModuleFunctionAddressCount,
				skippedFunctionCount = summary.SkippedFunctionCount,
				shardCount = summary.ShardCount,
			}).ToArray(),
		}, new JsonSerializerOptions { WriteIndented = true });
		Directory.CreateDirectory(Path.GetDirectoryName(statisticsPath)!);
		File.WriteAllText(statisticsPath, statisticsJson, Encoding.UTF8);
		WriteModuleStatisticsCsv(factory, summaries);
		WriteBindingDiagnosticsCsv(factory, generatedDiagnostics);
		WriteSkippedDiagnosticsCsv(factory, skippedDiagnostics);
		Console.WriteLine("AngelscriptUHTTool analyzed {0} functions: NativeRuntimeLinked={1}, ReflectiveFallback={2}, NativeModuleFunctionAddress={3}, skipped={4}, shards={5}", totalAnalyzedFunctions, totalNativeRuntimeLinkedCount, totalReflectiveFallbackCount, totalNativeModuleFunctionAddressCount, skippedDiagnostics.Count, generatedFileCount);
	}

	private static double GetRate(int numerator, int denominator)
	{
		return denominator > 0 ? (double)numerator / denominator : 0.0;
	}

	private static void WriteModuleStatisticsCsv(IUhtExportFactory factory, List<AngelscriptFunctionBindingModuleStatistics> summaries)
	{
		string path = factory.MakePath("AS_FunctionBindingModuleStatistics", ".csv");
		StringBuilder builder = new("ModuleName,EditorOnly,TotalAnalyzedFunctions,NativeRuntimeLinkedCount,ReflectiveFallbackCount,NativeModuleFunctionAddressCount,SkippedFunctionCount,ShardCount\r\n");
		foreach (AngelscriptFunctionBindingModuleStatistics summary in summaries)
		{
			builder.Append(EscapeCsv(summary.ModuleName)).Append(',').Append(summary.EditorOnly ? "true" : "false").Append(',').Append(summary.TotalAnalyzedFunctions).Append(',').Append(summary.NativeRuntimeLinkedCount).Append(',').Append(summary.ReflectiveFallbackCount).Append(',').Append(summary.NativeModuleFunctionAddressCount).Append(',').Append(summary.SkippedFunctionCount).Append(',').Append(summary.ShardCount).Append("\r\n");
		}
		File.WriteAllText(path, builder.ToString(), Encoding.UTF8);
	}

	private static void WriteBindingDiagnosticsCsv(IUhtExportFactory factory, List<AngelscriptFunctionBindingDiagnosticRow> diagnostics)
	{
		string path = factory.MakePath("AS_FunctionBindingDiagnostics", ".csv");
		StringBuilder builder = new("ModuleName,EditorOnly,ClassName,FunctionName,FunctionBindingCategory,FailureReason,EraseMacro,ArtifactName,ShardIndex\r\n");
		foreach (AngelscriptFunctionBindingDiagnosticRow diagnostic in diagnostics)
		{
			builder.Append(EscapeCsv(diagnostic.ModuleName)).Append(',').Append(diagnostic.EditorOnly ? "true" : "false").Append(',').Append(EscapeCsv(diagnostic.ClassName)).Append(',').Append(EscapeCsv(diagnostic.FunctionName)).Append(',').Append(EscapeCsv(diagnostic.FunctionBindingCategory)).Append(',').Append(EscapeCsv(diagnostic.FailureReason ?? string.Empty)).Append(',').Append(EscapeCsv(diagnostic.EraseMacro)).Append(',').Append(EscapeCsv(diagnostic.ArtifactName)).Append(',').Append(diagnostic.ShardIndex).Append("\r\n");
		}
		File.WriteAllText(path, builder.ToString(), Encoding.UTF8);
	}

	private static void WriteSkippedDiagnosticsCsv(IUhtExportFactory factory, List<AngelscriptSkippedFunctionDiagnostic> diagnostics)
	{
		string path = factory.MakePath("AS_FunctionBindingSkippedFunctions", ".csv");
		StringBuilder builder = new("ModuleName,ClassName,FunctionName,Result,FailureReason,ArtifactName\r\n");
		foreach (AngelscriptSkippedFunctionDiagnostic diagnostic in diagnostics)
		{
			builder.Append(EscapeCsv(diagnostic.ModuleName)).Append(',').Append(EscapeCsv(diagnostic.ClassName)).Append(',').Append(EscapeCsv(diagnostic.FunctionName)).Append(',').Append(EscapeCsv(diagnostic.Result)).Append(',').Append(EscapeCsv(diagnostic.FailureReason)).Append(',').Append(EscapeCsv(diagnostic.ArtifactName)).Append("\r\n");
		}
		File.WriteAllText(path, builder.ToString(), Encoding.UTF8);
	}

	private static void WriteCoverageDiagnostics(List<AngelscriptFunctionBindingModuleStatistics> summaries)
	{
		foreach (AngelscriptFunctionBindingModuleStatistics summary in summaries.OrderByDescending(static summary => summary.ReflectiveFallbackCount).ThenBy(static summary => summary.ModuleName, StringComparer.Ordinal))
		{
			Console.WriteLine("  - {0}: analyzed={1}, nativeRuntimeLinked={2}, reflectiveFallback={3}, nativeModuleFunctionAddress={4}, skipped={5}, shards={6}", summary.ModuleName, summary.TotalAnalyzedFunctions, summary.NativeRuntimeLinkedCount, summary.ReflectiveFallbackCount, summary.NativeModuleFunctionAddressCount, summary.SkippedFunctionCount, summary.ShardCount);
		}
	}

	private static string EscapeCsv(string value)
	{
		return value.IndexOfAny(new[] { ',', '"', '\r', '\n' }) == -1 ? value : '"' + value.Replace("\"", "\"\"", StringComparison.Ordinal) + '"';
	}
}
