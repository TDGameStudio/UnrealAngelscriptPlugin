using System;
using EpicGames.UHT.Tables;
using EpicGames.UHT.Types;
using EpicGames.UHT.Utils;

namespace AngelscriptUHTTool;

[UnrealHeaderTool]
internal static class AngelscriptFunctionBindingExporter
{
	[UhtExporter(
		Name = "AngelscriptFunctionBinding",
		Description = "Exports Angelscript function binding data",
		Options = UhtExporterOptions.Default,
		CppFilters = ["AS_FunctionBinding_*.gen.cpp", "AS_FunctionBinding_*.cpp"],
		ModuleName = "AngelscriptRuntime")]
	private static void Export(IUhtExportFactory factory)
	{
		int generatedFileCount = AngelscriptFunctionBindingCodeGenerator.Generate(factory);
		Console.WriteLine("AngelscriptUHTTool function binding exporter wrote {0} generated files.", generatedFileCount);
	}

	internal static bool IsAngelscriptCallable(UhtFunction function)
	{
		string functionFlags = function.FunctionFlags.ToString();
		return function.FunctionType == UhtFunctionType.Function &&
			(functionFlags.Contains("BlueprintCallable", StringComparison.Ordinal) ||
			functionFlags.Contains("BlueprintPure", StringComparison.Ordinal) ||
			function.MetaData.ContainsKey("ScriptCallable"));
	}
}
