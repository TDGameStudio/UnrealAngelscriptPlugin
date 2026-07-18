using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using EpicGames.Core;
using EpicGames.UHT.Tables;
using EpicGames.UHT.Types;
using EpicGames.UHT.Utils;

namespace AngelscriptUHTTool;

internal static partial class AngelscriptFunctionBindingCodeGenerator
{
	private const int MaxEntriesPerShard = 256;
	private const int MaxRuntimeWrapperShardCount = 64;
	private const string LayoutVersionFileName = "native-module-function-binding-layout-version.txt";

	public static int Generate(IUhtExportFactory factory)
	{
		AngelscriptFunctionBindingModuleConfiguration supportedModules = LoadFunctionBindingModuleConfiguration(factory);
		HashSet<string> generatedPaths = new(StringComparer.OrdinalIgnoreCase);
		Dictionary<string, string> moduleOutputDirectories = factory.Session.Modules.ToDictionary(
			static module => module.ShortName,
			static module => module.Module.OutputDirectory,
			StringComparer.OrdinalIgnoreCase);
		List<AngelscriptFunctionBindingModuleStatistics> summaries = new();
		List<AngelscriptFunctionBindingDiagnosticRow> generatedDiagnostics = new();
		List<AngelscriptSkippedFunctionDiagnostic> skippedDiagnostics = new();
		int generatedFileCount = 0;

		string layoutVersion = string.Empty;
		if (supportedModules.FunctionBindingMethod == AngelscriptFunctionBindingMethod.NativeModuleFunctionAddress)
		{
			layoutVersion = LoadLayoutVersion(factory);
			if (TryEmitNativeModuleFunctionBindingBridgeProbe(factory, generatedPaths, layoutVersion))
			{
				generatedFileCount++;
			}
		}

		foreach (UhtModule module in factory.Session.Modules)
		{
			bool emitRuntimeLinked = supportedModules.FunctionBindingMethod == AngelscriptFunctionBindingMethod.NativeRuntimeLinked &&
				supportedModules.RuntimeLinkedModules.Contains(module.ShortName);
			bool emitNativeModuleFunctionAddress = supportedModules.FunctionBindingMethod == AngelscriptFunctionBindingMethod.NativeModuleFunctionAddress &&
				supportedModules.NativeModuleFunctionAddressModules.Contains(module.ShortName);
			if (!emitRuntimeLinked && !emitNativeModuleFunctionAddress)
			{
				continue;
			}

			AngelscriptFunctionBindingModuleStatistics? summary = GenerateModule(
				factory,
				module,
				supportedModules.EditorOnlyModules.Contains(module.ShortName),
				emitRuntimeLinked,
				emitNativeModuleFunctionAddress,
				layoutVersion,
				generatedPaths,
				generatedDiagnostics,
				skippedDiagnostics);
			if (summary != null)
			{
				summaries.Add(summary);
				generatedFileCount += summary.ShardCount;
			}
		}

		DeleteStaleOutputs(factory, generatedPaths, moduleOutputDirectories);
		WriteStatistics(factory, supportedModules, summaries, generatedDiagnostics, skippedDiagnostics, generatedFileCount);
		WriteCoverageDiagnostics(summaries);
		return generatedFileCount;
	}

	private static AngelscriptFunctionBindingModuleStatistics? GenerateModule(
		IUhtExportFactory factory,
		UhtModule module,
		bool editorOnly,
		bool emitRuntimeLinked,
		bool emitNativeModuleFunctionAddress,
		string layoutVersion,
		HashSet<string> generatedPaths,
		List<AngelscriptFunctionBindingDiagnosticRow> generatedDiagnostics,
		List<AngelscriptSkippedFunctionDiagnostic> skippedDiagnostics)
	{
		if (emitRuntimeLinked && emitNativeModuleFunctionAddress)
		{
			throw new InvalidOperationException($"Function binding backend conflict for module '{module.ShortName}': Runtime-linked and NativeModuleFunctionAddress output cannot be emitted together.");
		}

		SortedSet<string> includes = new(StringComparer.Ordinal);
		List<AngelscriptGeneratedFunctionRegistration> runtimeBindings = new();
		List<AngelscriptNativeModuleFunctionBinding> nativeModuleFunctionBindings = new();
		int analyzedFunctionCount = 0;
		int skippedFunctionStart = skippedDiagnostics.Count;

		CollectFunctionBindings(
			factory,
			module.ScriptPackage,
			module.ShortName,
			emitRuntimeLinked,
			emitNativeModuleFunctionAddress,
			includes,
			runtimeBindings,
			nativeModuleFunctionBindings,
			skippedDiagnostics,
			ref analyzedFunctionCount);

		if (runtimeBindings.Count == 0 && nativeModuleFunctionBindings.Count == 0)
		{
			return analyzedFunctionCount == 0
				? null
				: new AngelscriptFunctionBindingModuleStatistics(module.ShortName, editorOnly, analyzedFunctionCount, 0, 0, 0, skippedDiagnostics.Count - skippedFunctionStart, 0);
		}

		runtimeBindings.Sort(static (left, right) =>
		{
			int classComparison = StringComparer.Ordinal.Compare(left.ClassName, right.ClassName);
			if (classComparison != 0)
			{
				return classComparison;
			}

			int functionComparison = StringComparer.Ordinal.Compare(left.FunctionName, right.FunctionName);
		return functionComparison != 0
				? functionComparison
				: StringComparer.Ordinal.Compare(left.FunctionBindingCategory, right.FunctionBindingCategory);
		});
		nativeModuleFunctionBindings.Sort(static (left, right) =>
		{
			int classComparison = StringComparer.Ordinal.Compare(left.ClassName, right.ClassName);
			if (classComparison != 0)
			{
				return classComparison;
			}

			int functionComparison = StringComparer.Ordinal.Compare(left.FunctionName, right.FunctionName);
		return functionComparison != 0 ? functionComparison : left.StableIndex.CompareTo(right.StableIndex);
		});

		int nativeRuntimeLinkedCount = runtimeBindings.Count(static binding => binding.FunctionBindingCategory == "NativeRuntimeLinked");
		int reflectiveFallbackCount = runtimeBindings.Count(static binding => binding.FunctionBindingCategory == "ReflectiveFallback");
		int nativeModuleFunctionAddressCount = nativeModuleFunctionBindings.Count;
		int shardCount = 0;

		int runtimeShardCount = emitRuntimeLinked ? GetShardCount(runtimeBindings.Count) : 0;
		if (runtimeShardCount > MaxRuntimeWrapperShardCount)
		{
			throw new InvalidOperationException($"Runtime-linked module '{module.ShortName}' requires {runtimeShardCount} function binding shards, exceeding the Build.cs wrapper limit of {MaxRuntimeWrapperShardCount}.");
		}
		for (int shardIndex = 0; shardIndex < runtimeShardCount; shardIndex++)
		{
			int startIndex = shardIndex * MaxEntriesPerShard;
			int entryCount = Math.Min(MaxEntriesPerShard, runtimeBindings.Count - startIndex);
			string outputPath = factory.MakePath($"AS_FunctionBinding_{module.ShortName}_{shardIndex:D3}", ".gen.cpp");
			factory.CommitOutput(outputPath, BuildRuntimeShard(module.ShortName, editorOnly, runtimeBindings, startIndex, entryCount, shardIndex, runtimeShardCount));
			generatedPaths.Add(outputPath);
			shardCount++;
			for (int entryIndex = startIndex; entryIndex < startIndex + entryCount; entryIndex++)
			{
				AngelscriptGeneratedFunctionRegistration binding = runtimeBindings[entryIndex];
				generatedDiagnostics.Add(new AngelscriptFunctionBindingDiagnosticRow(
					module.ShortName,
					editorOnly,
					binding.ClassName,
					binding.FunctionName,
					binding.FunctionBindingCategory,
					binding.FailureReason,
					binding.EraseMacro,
					Path.GetFileName(outputPath),
					shardIndex + 1));
			}
		}

		int nativeModuleFunctionAddressShardCount = emitNativeModuleFunctionAddress ? GetShardCount(nativeModuleFunctionBindings.Count) : 0;
		for (int shardIndex = 0; shardIndex < nativeModuleFunctionAddressShardCount; shardIndex++)
		{
			int startIndex = shardIndex * MaxEntriesPerShard;
			int entryCount = Math.Min(MaxEntriesPerShard, nativeModuleFunctionBindings.Count - startIndex);
			string outputPath = Path.Combine(module.Module.OutputDirectory, $"AS_FunctionBinding_{module.ShortName}_NativeModuleFunctionAddress_{shardIndex:D3}.cpp");
			factory.CommitOutput(outputPath, BuildNativeModuleFunctionAddressShard(module.ShortName, nativeModuleFunctionBindings, startIndex, entryCount, shardIndex, nativeModuleFunctionAddressShardCount, layoutVersion));
			generatedPaths.Add(outputPath);
			shardCount++;
			for (int entryIndex = startIndex; entryIndex < startIndex + entryCount; entryIndex++)
			{
				AngelscriptNativeModuleFunctionBinding binding = nativeModuleFunctionBindings[entryIndex];
				generatedDiagnostics.Add(new AngelscriptFunctionBindingDiagnosticRow(
					module.ShortName,
					editorOnly,
					binding.ClassName,
					binding.FunctionName,
					"NativeModuleFunctionAddress",
					null,
					"ERASE_NO_FUNCTION()",
					Path.GetFileName(outputPath),
					shardIndex + 1));
			}
		}

		return new AngelscriptFunctionBindingModuleStatistics(
			module.ShortName,
			editorOnly,
			analyzedFunctionCount,
			nativeRuntimeLinkedCount,
			reflectiveFallbackCount,
			nativeModuleFunctionAddressCount,
			skippedDiagnostics.Count - skippedFunctionStart,
			shardCount);
	}

	private static int GetShardCount(int entryCount)
	{
		return entryCount == 0 ? 0 : (entryCount + MaxEntriesPerShard - 1) / MaxEntriesPerShard;
	}

	private static void CollectFunctionBindings(
		IUhtExportFactory factory,
		UhtType type,
		string moduleName,
		bool emitRuntimeLinked,
		bool emitNativeModuleFunctionAddress,
		SortedSet<string> includes,
		List<AngelscriptGeneratedFunctionRegistration> runtimeBindings,
		List<AngelscriptNativeModuleFunctionBinding> nativeModuleFunctionBindings,
		List<AngelscriptSkippedFunctionDiagnostic> skippedDiagnostics,
		ref int analyzedFunctionCount)
	{
		if (type is UhtClass classObj)
		{
			foreach (UhtType child in classObj.Children)
			{
				if (child is not UhtFunction function || !ShouldGenerate(classObj, function))
				{
					continue;
				}

				analyzedFunctionCount++;
				string includePath = ResolveIncludePath(factory, classObj, includes);
				if (emitRuntimeLinked)
				{
					AngelscriptFunctionBindingAnalysisResult analysis = AnalyzeRuntimeLinkedFunction(moduleName, classObj, function);
					runtimeBindings.Add(new AngelscriptGeneratedFunctionRegistration(
						classObj.SourceName,
						function.SourceName,
						includePath,
						analysis.Signature?.BuildEraseMacro() ?? "ERASE_NO_FUNCTION()",
						analysis.FunctionBindingCategory,
						EditorOnlyGuard: ResolveEditorOnlyGuard(function),
						FailureReason: analysis.FailureReason));
					continue;
				}

				if (!emitNativeModuleFunctionAddress)
				{
					continue;
				}
				if (classObj.ClassType is UhtClassType.Interface or UhtClassType.NativeInterface)
				{
					skippedDiagnostics.Add(new AngelscriptSkippedFunctionDiagnostic(
						moduleName,
						classObj.SourceName,
						function.SourceName,
						"interface-function"));
					continue;
				}

				AngelscriptFunctionBindingAnalysisResult targetAnalysis = AnalyzeNativeModuleFunctionAddress(
					factory,
					moduleName,
					classObj,
					function,
					includePath,
					nativeModuleFunctionBindings.Count,
					includes);
				if (targetAnalysis.NativeModuleFunctionBinding != null)
				{
					nativeModuleFunctionBindings.Add(targetAnalysis.NativeModuleFunctionBinding);
				}
				else
				{
					skippedDiagnostics.Add(new AngelscriptSkippedFunctionDiagnostic(
						moduleName,
						classObj.SourceName,
						function.SourceName,
						targetAnalysis.FailureReason ?? "native-module-function-address-unsupported"));
				}
			}
		}

		foreach (UhtType child in type.Children)
		{
			CollectFunctionBindings(factory, child, moduleName, emitRuntimeLinked, emitNativeModuleFunctionAddress, includes, runtimeBindings, nativeModuleFunctionBindings, skippedDiagnostics, ref analyzedFunctionCount);
		}
	}

	private static string ResolveIncludePath(IUhtExportFactory factory, UhtClass classObj, SortedSet<string> includes)
	{
		if (classObj.HeaderFile == null)
		{
			return string.Empty;
		}

		factory.AddExternalDependency(classObj.HeaderFile.FilePath);
		string includePath = factory.GetModuleShortestIncludePath(classObj.HeaderFile.Module, classObj.HeaderFile.FilePath).Replace('\\', '/');
		includes.Add(includePath);
		return includePath;
	}

	private static AngelscriptFunctionBindingAnalysisResult AnalyzeRuntimeLinkedFunction(string moduleName, UhtClass classObj, UhtFunction function)
	{
		string? policyFallbackReason = AngelscriptFunctionBindingPolicy.GetRuntimeFallbackReason(classObj, function);
		if (policyFallbackReason != null)
		{
			return new AngelscriptFunctionBindingAnalysisResult(moduleName, classObj.SourceName, function.SourceName, "ReflectiveFallback", policyFallbackReason, null, null);
		}

		if (AngelscriptFunctionSignatureBuilder.TryBuild(classObj, function, out AngelscriptFunctionSignature? signature, out string? failureReason))
		{
			return new AngelscriptFunctionBindingAnalysisResult(moduleName, classObj.SourceName, function.SourceName, "NativeRuntimeLinked", null, signature, null);
		}

		return new AngelscriptFunctionBindingAnalysisResult(moduleName, classObj.SourceName, function.SourceName, "ReflectiveFallback", failureReason ?? "signature-unresolved", null, null);
	}

	private static AngelscriptFunctionBindingAnalysisResult AnalyzeNativeModuleFunctionAddress(
		IUhtExportFactory factory,
		string moduleName,
		UhtClass classObj,
		UhtFunction function,
		string includePath,
		int stableIndex,
		SortedSet<string> includes)
	{
		if (AngelscriptFunctionBindingPolicy.IsRpcNetFunction(function))
		{
			return new AngelscriptFunctionBindingAnalysisResult(moduleName, classObj.SourceName, function.SourceName, "", "rpc-net-function", null, null);
		}

		if (!AngelscriptHeaderSignatureResolver.TryBuildNativeModuleFunctionSignature(classObj, function, out AngelscriptFunctionSignature? signature, out string? signatureFailureReason))
		{
			return new AngelscriptFunctionBindingAnalysisResult(moduleName, classObj.SourceName, function.SourceName, "", signatureFailureReason ?? "signature-unresolved", null, null);
		}

		if (!AngelscriptFunctionBindingPolicy.IsSafeNativeModuleFunctionAddressSignature(signature!, classObj, function))
		{
			return new AngelscriptFunctionBindingAnalysisResult(moduleName, classObj.SourceName, function.SourceName, "", AngelscriptFunctionBindingPolicy.ClassifyUnsupportedNativeModuleFunctionBindingSignature(signature!, classObj, function), signature, null);
		}

		if (includePath.Length == 0)
		{
			return new AngelscriptFunctionBindingAnalysisResult(moduleName, classObj.SourceName, function.SourceName, "", "target-include-missing", signature, null);
		}

		SortedSet<string> bindingIncludes = new(StringComparer.Ordinal) { includePath };
		if (!TryCollectReferencedIncludes(factory, signature!, function, bindingIncludes, out string? includeFailureReason))
		{
			return new AngelscriptFunctionBindingAnalysisResult(moduleName, classObj.SourceName, function.SourceName, "", includeFailureReason, signature, null);
		}

		foreach (string bindingInclude in bindingIncludes)
		{
			includes.Add(bindingInclude);
		}

		AngelscriptNativeModuleFunctionBinding binding = new(
			moduleName,
			classObj.SourceName,
			function.SourceName,
			includePath,
			bindingIncludes.ToArray(),
			signature!.ReturnType,
			signature.ParameterTypes,
			signature.IsStatic,
			signature.IsConst,
			AngelscriptFunctionBindingPolicy.HasOutParams(function),
			AngelscriptFunctionBindingPolicy.HasWorldContext(function),
			AngelscriptFunctionBindingPolicy.HasReturnReference(signature, function),
			stableIndex);
		return new AngelscriptFunctionBindingAnalysisResult(moduleName, classObj.SourceName, function.SourceName, "NativeModuleFunctionAddress", null, signature, binding);
	}

	internal static AngelscriptFunctionBindingModuleConfiguration LoadFunctionBindingModuleConfiguration(IUhtExportFactory factory)
	{
		string runtimeBuildCsPath = AngelscriptFunctionBindingConfigurationResolver.ResolveRuntimeBuildCsPath(factory);
		factory.AddExternalDependency(runtimeBuildCsPath);
		BindingSettings settings = ReadBindingSettings(factory, runtimeBuildCsPath);
		HashSet<string> editorOnlyModules = new(StringComparer.OrdinalIgnoreCase) { "UMGEditor", "UnrealEd" };
		HashSet<string> configuredModules = settings.Settings.FunctionBindingMethod switch
		{
			AngelscriptFunctionBindingMethod.NativeRuntimeLinked => settings.Settings.NativeRuntimeLinkedModules,
			AngelscriptFunctionBindingMethod.NativeModuleFunctionAddress => settings.Settings.NativeModuleFunctionAddressModules,
			_ => new HashSet<string>(StringComparer.OrdinalIgnoreCase),
		};
		List<string> configuredModuleMisses = configuredModules
			.Where(moduleName => !factory.Session.Modules.Any(module => module.ShortName.Equals(moduleName, StringComparison.OrdinalIgnoreCase)))
			.OrderBy(static moduleName => moduleName, StringComparer.OrdinalIgnoreCase)
			.ToList();
		int configuredMissCount = configuredModuleMisses.Count;
		if (configuredModuleMisses.Count > 0)
		{
			Console.WriteLine("Warning: AngelscriptUHTTool configured function binding modules absent from the current UHT session: {0}.", string.Join(", ", configuredModuleMisses));
		}
		return new AngelscriptFunctionBindingModuleConfiguration(settings.Settings.FunctionBindingMethod, settings.Settings.NativeRuntimeLinkedModules, settings.Settings.NativeModuleFunctionAddressModules, editorOnlyModules, settings.CompileOptionsPath, settings.EngineDistribution, configuredMissCount, configuredModuleMisses);
	}

	private sealed record BindingSettings(
		AngelscriptFunctionBindingSettings Settings,
		string CompileOptionsPath,
		string EngineDistribution);

	private static BindingSettings ReadBindingSettings(IUhtExportFactory factory, string runtimeBuildCsPath)
	{
		foreach (string candidate in AngelscriptFunctionBindingConfigurationResolver.EnumerateCompileOptionsCandidates(runtimeBuildCsPath))
		{
			if (!File.Exists(candidate))
			{
				continue;
			}

			factory.AddExternalDependency(candidate);
			AngelscriptFunctionBindingSettings settings = AngelscriptFunctionBindingConfiguration.ReadFile(candidate);

			string? engineDirectory = AngelscriptFunctionBindingConfigurationResolver.ResolveEngineDirectory(factory);
			string engineDistribution = AngelscriptFunctionBindingConfigurationResolver.ClassifyEngineDistribution(engineDirectory);
			if (settings.FunctionBindingMethod == AngelscriptFunctionBindingMethod.NativeModuleFunctionAddress && !engineDistribution.Equals("source", StringComparison.OrdinalIgnoreCase))
			{
				throw new InvalidOperationException($"NativeModuleFunctionAddress compilation requires a source engine. Engine '{engineDirectory ?? "<unknown>"}' is classified as {engineDistribution}.");
			}
			return new BindingSettings(settings, candidate, engineDistribution);
		}

		return new BindingSettings(new AngelscriptFunctionBindingSettings(
			AngelscriptFunctionBindingMethod.NativeRuntimeLinked,
			new HashSet<string>(StringComparer.OrdinalIgnoreCase),
			new HashSet<string>(StringComparer.OrdinalIgnoreCase)), string.Empty, AngelscriptFunctionBindingConfigurationResolver.ClassifyEngineDistribution(AngelscriptFunctionBindingConfigurationResolver.ResolveEngineDirectory(factory)));
	}

	private static string LoadLayoutVersion(IUhtExportFactory factory)
	{
		string runtimeBuildCsPath = AngelscriptFunctionBindingConfigurationResolver.ResolveRuntimeBuildCsPath(factory);
		DirectoryInfo? sourceDirectory = Directory.GetParent(Path.GetDirectoryName(runtimeBuildCsPath)!);
		IEnumerable<string> candidates = sourceDirectory == null
			? Array.Empty<string>()
			: new[] { Path.Combine(sourceDirectory.FullName, "AngelscriptUHTTool", LayoutVersionFileName) };
		foreach (string candidate in candidates)
		{
			if (File.Exists(candidate))
			{
				factory.AddExternalDependency(candidate);
				string token = File.ReadAllLines(candidate).FirstOrDefault(static line => line.Trim().Length > 0 && !line.TrimStart().StartsWith('#'))?.Trim() ?? string.Empty;
				if (!Regex.IsMatch(token, "^0x[0-9A-Fa-f]{8}$"))
				{
					throw new InvalidDataException($"Invalid native module function binding layout version token '{token}' in {candidate}.");
				}
				return token;
			}
		}
		throw new FileNotFoundException($"Unable to locate {LayoutVersionFileName} for native module function binding generation.");
	}

	private static bool ShouldGenerate(UhtClass classObj, UhtFunction function)
	{
		return classObj.HeaderFile != null && AngelscriptFunctionBindingPolicy.IsEligible(classObj, function, IsSupportedHeader(classObj.HeaderFile.FilePath));
	}

	private static bool IsSupportedHeader(string headerPath)
	{
		string normalizedPath = headerPath.Replace('\\', '/');
		return !normalizedPath.Contains("/Private/", StringComparison.OrdinalIgnoreCase) && !normalizedPath.EndsWith("/Components/InstancedSkinnedMeshComponent.h", StringComparison.OrdinalIgnoreCase);
	}

	private static string? ResolveEditorOnlyGuard(UhtFunction function)
	{
		if (!function.FunctionFlags.HasAnyFlags(EFunctionFlags.EditorOnly))
		{
			return null;
		}
		bool withEditor = function.DefineScope.ToString().Split(',', StringSplitOptions.TrimEntries).Contains("Editor", StringComparer.Ordinal);
		bool withEditorOnlyData = (function.DefineScope & UhtDefineScope.EditorOnlyData) != 0;
		return withEditorOnlyData && !withEditor ? "WITH_EDITORONLY_DATA" : "WITH_EDITOR";
	}

	private static bool TryCollectReferencedIncludes(IUhtExportFactory factory, AngelscriptFunctionSignature signature, UhtFunction function, SortedSet<string> includes, out string? failureReason)
	{
		if (function.ReturnProperty is UhtProperty returnProperty && !TryAddReferencedInclude(factory, returnProperty, includes, out failureReason))
		{
			return false;
		}
		if (!TryAddStructIncludeByTypeName(factory, signature.ReturnType, includes, out failureReason))
		{
			return false;
		}
		int parameterIndex = 0;
		foreach (UhtType parameterType in function.ParameterProperties.Span)
		{
			if (parameterType is UhtProperty property && !TryAddReferencedInclude(factory, property, includes, out failureReason))
			{
				return false;
			}
			if (parameterIndex < signature.ParameterTypes.Count && !TryAddStructIncludeByTypeName(factory, signature.ParameterTypes[parameterIndex], includes, out failureReason))
			{
				return false;
			}
			parameterIndex++;
		}
		failureReason = null;
		return true;
	}

	private static bool TryAddReferencedInclude(IUhtExportFactory factory, UhtProperty property, SortedSet<string> includes, out string? failureReason)
	{
		if (property is not UhtStructProperty structProperty || structProperty.ScriptStruct.IsCoreType)
		{
			failureReason = null;
			return true;
		}
		string path = structProperty.ScriptStruct.HeaderFile.FilePath;
		if (string.IsNullOrEmpty(path) || !IsSupportedHeader(path))
		{
			failureReason = "native-module-function-binding-struct-header-unavailable";
			return false;
		}
		factory.AddExternalDependency(path);
		includes.Add(factory.GetModuleShortestIncludePath(structProperty.ScriptStruct.HeaderFile.Module, path).Replace('\\', '/'));
		failureReason = null;
		return true;
	}

	private static bool TryAddStructIncludeByTypeName(IUhtExportFactory factory, string typeName, SortedSet<string> includes, out string? failureReason)
	{
		string structName = ExtractStructTypeName(typeName);
		if (structName.Length == 0 || factory.Session.FindType(null, UhtFindOptions.SourceName | UhtFindOptions.ScriptStruct, structName) is not UhtScriptStruct scriptStruct || scriptStruct.IsCoreType)
		{
			failureReason = null;
			return true;
		}
		string path = scriptStruct.HeaderFile.FilePath;
		if (string.IsNullOrEmpty(path) || !IsSupportedHeader(path))
		{
			failureReason = "native-module-function-binding-struct-header-unavailable";
			return false;
		}
		factory.AddExternalDependency(path);
		includes.Add(factory.GetModuleShortestIncludePath(scriptStruct.HeaderFile.Module, path).Replace('\\', '/'));
		failureReason = null;
		return true;
	}

	private static string ExtractStructTypeName(string typeName)
	{
		string normalized = typeName.Replace("const ", string.Empty, StringComparison.Ordinal).Replace("&", string.Empty, StringComparison.Ordinal).Replace("*", string.Empty, StringComparison.Ordinal).Trim();
		if (normalized.StartsWith("struct ", StringComparison.Ordinal)) normalized = normalized["struct ".Length..].Trim();
		if (normalized.Contains('<', StringComparison.Ordinal)) return string.Empty;
		if (normalized.StartsWith("::", StringComparison.Ordinal)) normalized = normalized[2..];
		int scopeIndex = normalized.LastIndexOf("::", StringComparison.Ordinal);
		if (scopeIndex >= 0) normalized = normalized[(scopeIndex + 2)..];
		return normalized.StartsWith("F", StringComparison.Ordinal) ? normalized : string.Empty;
	}
}
