using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;
using EpicGames.Core;
using EpicGames.UHT.Tables;
using EpicGames.UHT.Types;
using EpicGames.UHT.Utils;

namespace AngelscriptUHTTool;

internal enum AngelscriptFunctionBindingMethod
{
	None,
	NativeRuntimeLinked,
	NativeModuleFunctionAddress,
}

internal sealed record AngelscriptFunctionBindingAnalysisResult(
	string ModuleName,
	string ClassName,
	string FunctionName,
	string FunctionBindingCategory,
	string? FailureReason,
	AngelscriptFunctionSignature? Signature,
	AngelscriptNativeModuleFunctionBinding? NativeModuleFunctionBinding);

internal sealed record AngelscriptGeneratedFunctionRegistration(
	string ClassName,
	string FunctionName,
	string EraseMacro,
	string FunctionBindingCategory,
	string? EditorOnlyGuard = null)
{
	public string BuildBindingRegistrationLine()
	{
		return $"\tFAngelscriptBinds::RegisterFunctionBinding({ClassName}::StaticClass(), \"{FunctionName}\", {{ {EraseMacro} }});";
	}
}

internal sealed record AngelscriptNativeModuleFunctionBinding(
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

internal sealed record AngelscriptFunctionBindingModuleConfiguration(
	AngelscriptFunctionBindingMethod FunctionBindingMethod,
	HashSet<string> RuntimeLinkedModules,
	HashSet<string> NativeModuleFunctionAddressModules,
	HashSet<string> EditorOnlyModules,
	string CompileOptionsPath,
	string EngineDistribution,
	int ConfiguredModuleMissCount);

internal sealed record AngelscriptFunctionBindingModuleStatistics(
	string ModuleName,
	bool EditorOnly,
	int TotalAnalyzedFunctions,
	int NativeRuntimeLinkedCount,
	int ReflectiveFallbackCount,
	int NativeModuleFunctionAddressCount,
	int SkippedFunctionCount,
	int ShardCount);

internal sealed record AngelscriptFunctionBindingDiagnosticRow(
	string ModuleName,
	bool EditorOnly,
	string ClassName,
	string FunctionName,
	string FunctionBindingCategory,
	string EraseMacro,
	int ShardIndex);

internal sealed record AngelscriptFunctionBindingStatistics(
	int TotalAnalyzedFunctions,
	int NativeRuntimeLinkedCount,
	int ReflectiveFallbackCount,
	int NativeModuleFunctionAddressCount,
	int SkippedFunctionCount,
	int ConfiguredModuleMissCount,
	int TotalShardCount);

internal sealed record AngelscriptSkippedFunctionDiagnostic(
	string ModuleName,
	string ClassName,
	string FunctionName,
	string FailureReason);

internal static class AngelscriptFunctionBindingCodeGenerator
{
	private const int MaxEntriesPerShard = 256;
	private const int MaxRuntimeWrapperShardCount = 64;
	private const string LayoutVersionFileName = "native-module-function-binding-layout-version.txt";
	private const string CompileOptionsFileName = "DefaultAngelscriptCompileOptions.ini";
	private const string CompileOptionsSectionName = "/Script/AngelscriptRuntime.AngelscriptCompileOptions";
	private const string FunctionBindingMethodSettingName = "FunctionBindingMethod";
	private const string NativeRuntimeLinkedModulesSettingName = "NativeRuntimeLinkedModules";
	private const string NativeModuleFunctionAddressModulesSettingName = "NativeModuleFunctionAddressModules";

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
			factory.CommitOutput(outputPath, BuildRuntimeShard(module.ShortName, editorOnly, includes, runtimeBindings, startIndex, entryCount, shardIndex, runtimeShardCount));
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
					binding.EraseMacro,
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
					"ERASE_NO_FUNCTION()",
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
						analysis.Signature?.BuildEraseMacro() ?? "ERASE_NO_FUNCTION()",
						analysis.FunctionBindingCategory,
						ResolveEditorOnlyGuard(function)));
					continue;
				}

				if (!emitNativeModuleFunctionAddress || classObj.ClassType is UhtClassType.Interface or UhtClassType.NativeInterface)
				{
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
		if (classObj.ClassType is UhtClassType.Interface or UhtClassType.NativeInterface)
		{
			return new AngelscriptFunctionBindingAnalysisResult(moduleName, classObj.SourceName, function.SourceName, "ReflectiveFallback", "interface-function", null, null);
		}

		if (IsRpcNetFunction(function))
		{
			return new AngelscriptFunctionBindingAnalysisResult(moduleName, classObj.SourceName, function.SourceName, "ReflectiveFallback", "rpc-net-function", null, null);
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
		if (IsRpcNetFunction(function))
		{
			return new AngelscriptFunctionBindingAnalysisResult(moduleName, classObj.SourceName, function.SourceName, "", "rpc-net-function", null, null);
		}

		if (!AngelscriptHeaderSignatureResolver.TryBuildNativeModuleFunctionSignature(classObj, function, out AngelscriptFunctionSignature? signature, out string? signatureFailureReason))
		{
			return new AngelscriptFunctionBindingAnalysisResult(moduleName, classObj.SourceName, function.SourceName, "", signatureFailureReason ?? "signature-unresolved", null, null);
		}

		if (!IsSafeNativeModuleFunctionAddressSignature(signature!, classObj, function))
		{
			return new AngelscriptFunctionBindingAnalysisResult(moduleName, classObj.SourceName, function.SourceName, "", ClassifyUnsupportedNativeModuleFunctionBindingSignature(signature!, classObj, function), signature, null);
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
			HasOutParams(function),
			HasWorldContext(function),
			HasReturnReference(signature, function),
			stableIndex);
		return new AngelscriptFunctionBindingAnalysisResult(moduleName, classObj.SourceName, function.SourceName, "NativeModuleFunctionAddress", null, signature, binding);
	}

	private static StringBuilder BuildRuntimeShard(string moduleName, bool editorOnly, SortedSet<string> includes, List<AngelscriptGeneratedFunctionRegistration> bindings, int startIndex, int entryCount, int shardIndex, int shardCount)
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
		builder.Append("AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_AS_FunctionBinding_").Append(moduleName).Append('_').Append(shardIndex.ToString("D3", CultureInfo.InvariantCulture)).AppendLine("((int32)FAngelscriptBinds::EOrder::Late + 50, []()");
		builder.AppendLine("{");
		builder.AppendLine("\tconst double GeneratedFunctionBindingStartSeconds = FPlatformTime::Seconds();");
		for (int bindingIndex = startIndex; bindingIndex < startIndex + entryCount; bindingIndex++)
		{
			AngelscriptGeneratedFunctionRegistration binding = bindings[bindingIndex];
			bool needsEditorGuard = !editorOnly && binding.EditorOnlyGuard != null;
			if (needsEditorGuard)
			{
				builder.Append("#if ").AppendLine(binding.EditorOnlyGuard);
			}
			builder.AppendLine(binding.BuildBindingRegistrationLine());
			if (needsEditorGuard)
			{
				builder.AppendLine("#endif");
			}
		}

		builder.AppendLine("\tconst double GeneratedFunctionBindingElapsedMilliseconds = (FPlatformTime::Seconds() - GeneratedFunctionBindingStartSeconds) * 1000.0;");
		builder.Append("\tFAngelscriptBinds::RecordGeneratedFunctionBindingShardTiming(TEXT(\"").Append(moduleName).Append("\"), ").Append(shardIndex + 1).Append(", ").Append(shardCount).Append(", ").Append(entryCount).AppendLine(", GeneratedFunctionBindingElapsedMilliseconds);");
		builder.Append("\tUE_LOG(Angelscript, Log, TEXT(\"[UHT] Registered %d generated AS-callable bindings for module %s shard %d/%d in %.3f ms\"), ").Append(entryCount).Append(", TEXT(\"").Append(moduleName).Append("\"), ").Append(shardIndex + 1).Append(", ").Append(shardCount).AppendLine(", GeneratedFunctionBindingElapsedMilliseconds);");
		builder.AppendLine("});");
		builder.AppendLine("PRAGMA_ENABLE_DEPRECATION_WARNINGS");
		if (editorOnly)
		{
			builder.AppendLine("#endif");
		}
		return builder;
	}

	private static StringBuilder BuildNativeModuleFunctionAddressShard(string moduleName, List<AngelscriptNativeModuleFunctionBinding> bindings, int startIndex, int entryCount, int shardIndex, int shardCount, string layoutVersion)
	{
		SortedSet<string> includes = new(StringComparer.Ordinal);
		for (int bindingIndex = startIndex; bindingIndex < startIndex + entryCount; bindingIndex++)
		{
			foreach (string include in bindings[bindingIndex].IncludePaths)
			{
				includes.Add(include);
			}
		}

		StringBuilder builder = new();
		builder.AppendLine("#include \"CoreMinimal.h\"");
		builder.AppendLine("#include \"Features/IModularFeatures.h\"");
		builder.AppendLine("#include \"Misc/CoreDelegates.h\"");
		foreach (string include in includes)
		{
			builder.Append("#include \"").Append(include).AppendLine("\"");
		}

		builder.AppendLine();
		builder.Append("namespace AngelscriptNativeModuleFunctionAddress_").Append(SanitizeIdentifier(moduleName)).Append('_').Append(shardIndex.ToString("D3", CultureInfo.InvariantCulture)).AppendLine();
		builder.AppendLine("{");
		builder.AppendLine("\tstruct FAngelscriptNativeModuleFunctionBindingCallFrame { void** ArgSlots; uint16 ArgCount; uint16 Reserved0; void* ReturnSlot; UObject* ScriptSelf; UObject* WorldContext; uint32 Flags; uint32 Reserved1; };");
		builder.AppendLine("\tstruct FAngelscriptNativeModuleFunctionBinding { const TCHAR* ClassName; const TCHAR* FunctionName; void (*Thunk)(UObject* Self, FAngelscriptNativeModuleFunctionBindingCallFrame* Frame); uint16 ArgCount; uint16 RetSize; uint32 Flags; };");
		builder.AppendLine("\tstruct FAngelscriptNativeModuleFunctionBindingView { const FAngelscriptNativeModuleFunctionBinding* Table; int32 Count; const TCHAR* ModuleName; uint32 LayoutVersion; };");
		builder.AppendLine($"\tconstexpr uint32 GNativeModuleFunctionBindingLayoutVersion = {layoutVersion}u;");
		builder.AppendLine("\tconstexpr uint32 GNativeModuleFunctionBindingFlagNone = 0u;");
		builder.AppendLine("\tconstexpr uint32 GNativeModuleFunctionBindingFlagStatic = 1u << 0;");
		builder.AppendLine("\tconstexpr uint32 GNativeModuleFunctionBindingFlagConst = 1u << 1;");
		builder.AppendLine("\tconstexpr uint32 GNativeModuleFunctionBindingFlagWorldContext = 1u << 2;");
		builder.AppendLine("\tconstexpr uint32 GNativeModuleFunctionBindingFlagHasOutParams = 1u << 3;");
		builder.AppendLine("\tconstexpr uint32 GNativeModuleFunctionBindingFlagReturnByRef = 1u << 4;");
		builder.AppendLine("\tbool bNativeModuleFunctionBindingShuttingDown = false;");
		builder.AppendLine();
		builder.AppendLine("\ttemplate <typename T>");
		builder.AppendLine("\tdecltype(auto) PassNativeModuleFunctionBindingArg(FAngelscriptNativeModuleFunctionBindingCallFrame* Frame, uint16 Index)");
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
		builder.AppendLine("\tvoid BuildNativeModuleFunctionBindingReturn(FAngelscriptNativeModuleFunctionBindingCallFrame* Frame, T&& Value)");
		builder.AppendLine("\t{");
		builder.AppendLine("\t\tif (Frame == nullptr || Frame->ReturnSlot == nullptr)");
		builder.AppendLine("\t\t{");
		builder.AppendLine("\t\t\treturn;");
		builder.AppendLine("\t\t}");
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

		for (int bindingIndex = startIndex; bindingIndex < startIndex + entryCount; bindingIndex++)
		{
			AngelscriptNativeModuleFunctionBinding binding = bindings[bindingIndex];
			string thunkName = BuildNativeModuleFunctionBindingThunkName(binding);
			string callArguments = BuildNativeModuleFunctionBindingCallArguments(binding);
			builder.Append("\tvoid ").Append(thunkName).AppendLine("(UObject* Self, FAngelscriptNativeModuleFunctionBindingCallFrame* Frame)");
			builder.AppendLine("\t{");
			if (binding.ParameterTypes.Count > 0)
			{
				builder.Append("\t\tif (Frame == nullptr || Frame->ArgSlots == nullptr || Frame->ArgCount < ").Append(binding.ParameterTypes.Count).AppendLine(")");
				builder.AppendLine("\t\t{");
				builder.AppendLine("\t\t\treturn;");
				builder.AppendLine("\t\t}");
			}
			if (!binding.IsStatic)
			{
				builder.AppendLine("\t\tif (Self == nullptr)");
				builder.AppendLine("\t\t{");
				builder.AppendLine("\t\t\treturn;");
				builder.AppendLine("\t\t}");
			}
			builder.Append("\t\t");
			if (binding.ReturnType != "void")
			{
				builder.Append("BuildNativeModuleFunctionBindingReturn<").Append(binding.ReturnType).Append(">(Frame, ");
			}
			if (binding.IsStatic)
			{
				builder.Append(binding.ClassName).Append("::").Append(binding.FunctionName);
			}
			else
			{
				builder.Append("static_cast<").Append(binding.IsConst ? "const " : string.Empty).Append(binding.ClassName).Append("*>(Self)->").Append(binding.FunctionName);
			}
			builder.Append('(').Append(callArguments).Append(')');
			builder.AppendLine(binding.ReturnType == "void" ? ";" : ");");
			builder.AppendLine("\t}");
			builder.AppendLine();
		}

		builder.AppendLine("\tstatic const FAngelscriptNativeModuleFunctionBinding GNativeModuleFunctionBindingTable[] =");
		builder.AppendLine("\t{");
		for (int bindingIndex = startIndex; bindingIndex < startIndex + entryCount; bindingIndex++)
		{
			AngelscriptNativeModuleFunctionBinding binding = bindings[bindingIndex];
			builder.Append("\t\t{ TEXT(\"").Append(binding.ClassName).Append("\"), TEXT(\"").Append(binding.FunctionName).Append("\"), &").Append(BuildNativeModuleFunctionBindingThunkName(binding)).Append(", ").Append(binding.ParameterTypes.Count).Append(", ").Append(GetReturnSizeExpression(binding.ReturnType)).Append(", ").Append(BuildNativeModuleFunctionBindingFlagsExpression(binding)).AppendLine(" },");
		}
		builder.AppendLine("\t};");
		builder.AppendLine();
		builder.AppendLine("\tstruct FNativeModuleFunctionBindingFeature : public IModularFeature");
		builder.AppendLine("\t{");
		builder.AppendLine("\t\tconst FAngelscriptNativeModuleFunctionBinding* Table;");
		builder.AppendLine("\t\tint32 Count;");
		builder.AppendLine("\t\tconst TCHAR* ModuleName;");
		builder.AppendLine("\t\tuint32 LayoutVersion;");
		builder.AppendLine("\t};");
		builder.AppendLine("\tstatic_assert(sizeof(FAngelscriptNativeModuleFunctionBindingCallFrame) == 48, \"Native module function binding call frame ABI layout changed.\");");
		builder.AppendLine("\tstatic_assert(sizeof(FAngelscriptNativeModuleFunctionBinding) == 32, \"Native module function binding ABI layout changed.\");");
		builder.AppendLine("\tstatic_assert(sizeof(FAngelscriptNativeModuleFunctionBindingView) == 32, \"Native module function binding view ABI layout changed.\");");
		builder.AppendLine("\tstatic FNativeModuleFunctionBindingFeature GNativeModuleFunctionBindingFeature = { GNativeModuleFunctionBindingTable, UE_ARRAY_COUNT(GNativeModuleFunctionBindingTable), TEXT(\"" + moduleName + "\"), GNativeModuleFunctionBindingLayoutVersion };");
		builder.AppendLine("\tstruct FNativeModuleFunctionBindingAutoRegistration");
		builder.AppendLine("\t{");
		builder.AppendLine("\t\tFNativeModuleFunctionBindingAutoRegistration()");
		builder.AppendLine("\t\t{");
		builder.AppendLine("\t\t\tIModularFeatures::Get().RegisterModularFeature(FName(TEXT(\"AngelscriptNativeModuleFunctionBinding\")), &GNativeModuleFunctionBindingFeature);");
		builder.AppendLine("\t\t\tFCoreDelegates::OnPreExit.AddStatic([]() { bNativeModuleFunctionBindingShuttingDown = true; });");
		builder.AppendLine("\t\t}");
		builder.AppendLine("\t\t~FNativeModuleFunctionBindingAutoRegistration()");
		builder.AppendLine("\t\t{");
		builder.AppendLine("\t\t\tif (!bNativeModuleFunctionBindingShuttingDown)");
		builder.AppendLine("\t\t\t{");
		builder.AppendLine("\t\t\t\tIModularFeatures::Get().UnregisterModularFeature(FName(TEXT(\"AngelscriptNativeModuleFunctionBinding\")), &GNativeModuleFunctionBindingFeature);");
		builder.AppendLine("\t\t\t}");
		builder.AppendLine("\t\t}");
		builder.AppendLine("\t};");
		builder.Append("\tstatic FNativeModuleFunctionBindingAutoRegistration GNativeModuleFunctionBindingAutoRegistration_").Append(shardIndex.ToString("D3", CultureInfo.InvariantCulture)).AppendLine(";");
		builder.AppendLine("}");
		return builder;
	}

	private static string BuildNativeModuleFunctionBindingThunkName(AngelscriptNativeModuleFunctionBinding binding)
	{
		return "Call_" + SanitizeIdentifier(binding.ClassName) + "_" + SanitizeIdentifier(binding.FunctionName) + "_" + binding.StableIndex.ToString("D5", CultureInfo.InvariantCulture);
	}

	private static string BuildNativeModuleFunctionBindingCallArguments(AngelscriptNativeModuleFunctionBinding binding)
	{
		return string.Join(", ", binding.ParameterTypes.Select((type, index) => $"PassNativeModuleFunctionBindingArg<{type}>(Frame, {index})"));
	}

	private static string BuildNativeModuleFunctionBindingFlagsExpression(AngelscriptNativeModuleFunctionBinding binding)
	{
		List<string> flags = new();
		if (binding.IsStatic) flags.Add("GNativeModuleFunctionBindingFlagStatic");
		if (binding.IsConst) flags.Add("GNativeModuleFunctionBindingFlagConst");
		if (binding.HasWorldContext) flags.Add("GNativeModuleFunctionBindingFlagWorldContext");
		if (binding.HasOutParams) flags.Add("GNativeModuleFunctionBindingFlagHasOutParams");
		if (binding.ReturnsByRef) flags.Add("GNativeModuleFunctionBindingFlagReturnByRef");
		return flags.Count == 0 ? "GNativeModuleFunctionBindingFlagNone" : string.Join(" | ", flags);
	}

	private static string GetReturnSizeExpression(string returnType)
	{
		return returnType == "void" ? "0" : $"sizeof({returnType})";
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

	private static bool TryEmitNativeModuleFunctionBindingBridgeProbe(IUhtExportFactory factory, HashSet<string> generatedPaths, string layoutVersion)
	{
		UhtModule? engineModule = factory.Session.Modules.FirstOrDefault(static module => module.ShortName.Equals("Engine", StringComparison.Ordinal));
		if (engineModule == null)
		{
			return false;
		}

		string outputPath = Path.Combine(engineModule.Module.OutputDirectory, "AS_FunctionBinding_Engine_NativeModuleFunctionBindingBridgeProbe.cpp");
		StringBuilder builder = new();
		builder.AppendLine("#include \"Features/IModularFeatures.h\"");
		builder.AppendLine("#include \"Misc/CoreDelegates.h\"");
		builder.AppendLine("namespace");
		builder.AppendLine("{");
		builder.AppendLine($"\tconstexpr uint32 GProbeLayoutVersion = {layoutVersion}u;");
		builder.AppendLine("\tstruct FProbeFeature : public IModularFeature { const void* Table; int32 Count; const TCHAR* ModuleName; uint32 LayoutVersion; };");
		builder.AppendLine("\tstatic FProbeFeature GProbeFeature = { nullptr, 0, TEXT(\"Engine\"), GProbeLayoutVersion };");
		builder.AppendLine("\tstruct FProbeRegistration");
		builder.AppendLine("\t{");
		builder.AppendLine("\t\tFProbeRegistration() { IModularFeatures::Get().RegisterModularFeature(FName(TEXT(\"AngelscriptNativeModuleFunctionBindingBridgeProbe\")), &GProbeFeature); }");
		builder.AppendLine("\t\t~FProbeRegistration() { IModularFeatures::Get().UnregisterModularFeature(FName(TEXT(\"AngelscriptNativeModuleFunctionBindingBridgeProbe\")), &GProbeFeature); }");
		builder.AppendLine("\t};");
		builder.AppendLine("\tstatic FProbeRegistration GProbeRegistration;");
		builder.AppendLine("}");
		factory.CommitOutput(outputPath, builder);
		generatedPaths.Add(outputPath);
		return true;
	}

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
		StringBuilder builder = new("ModuleName,EditorOnly,ClassName,FunctionName,FunctionBindingCategory,EraseMacro,ShardIndex\r\n");
		foreach (AngelscriptFunctionBindingDiagnosticRow diagnostic in diagnostics)
		{
			builder.Append(EscapeCsv(diagnostic.ModuleName)).Append(',').Append(diagnostic.EditorOnly ? "true" : "false").Append(',').Append(EscapeCsv(diagnostic.ClassName)).Append(',').Append(EscapeCsv(diagnostic.FunctionName)).Append(',').Append(EscapeCsv(diagnostic.FunctionBindingCategory)).Append(',').Append(EscapeCsv(diagnostic.EraseMacro)).Append(',').Append(diagnostic.ShardIndex).Append("\r\n");
		}
		File.WriteAllText(path, builder.ToString(), Encoding.UTF8);
	}

	private static void WriteSkippedDiagnosticsCsv(IUhtExportFactory factory, List<AngelscriptSkippedFunctionDiagnostic> diagnostics)
	{
		string path = factory.MakePath("AS_FunctionBindingSkippedFunctions", ".csv");
		StringBuilder builder = new("ModuleName,ClassName,FunctionName,FailureReason\r\n");
		foreach (AngelscriptSkippedFunctionDiagnostic diagnostic in diagnostics)
		{
			builder.Append(EscapeCsv(diagnostic.ModuleName)).Append(',').Append(EscapeCsv(diagnostic.ClassName)).Append(',').Append(EscapeCsv(diagnostic.FunctionName)).Append(',').Append(EscapeCsv(diagnostic.FailureReason)).Append("\r\n");
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

	internal static AngelscriptFunctionBindingModuleConfiguration LoadFunctionBindingModuleConfiguration(IUhtExportFactory factory)
	{
		string runtimeBuildCsPath = ResolveRuntimeBuildCsPath(factory);
		factory.AddExternalDependency(runtimeBuildCsPath);
		BindingSettings settings = ReadBindingSettings(factory, runtimeBuildCsPath);
		HashSet<string> editorOnlyModules = new(StringComparer.OrdinalIgnoreCase) { "UMGEditor", "UnrealEd" };
		HashSet<string> configuredModules = settings.FunctionBindingMethod == AngelscriptFunctionBindingMethod.NativeRuntimeLinked
			? settings.RuntimeLinkedModules
			: settings.NativeModuleFunctionAddressModules;
		int configuredMissCount = configuredModules.Count(moduleName => !factory.Session.Modules.Any(module => module.ShortName.Equals(moduleName, StringComparison.OrdinalIgnoreCase)));
		if (configuredMissCount > 0)
		{
			Console.WriteLine("Warning: AngelscriptUHTTool configured {0} function binding modules that are absent from the current UHT session.", configuredMissCount);
		}
		return new AngelscriptFunctionBindingModuleConfiguration(settings.FunctionBindingMethod, settings.RuntimeLinkedModules, settings.NativeModuleFunctionAddressModules, editorOnlyModules, settings.CompileOptionsPath, settings.EngineDistribution, configuredMissCount);
	}

	private sealed record BindingSettings(
		AngelscriptFunctionBindingMethod FunctionBindingMethod,
		HashSet<string> RuntimeLinkedModules,
		HashSet<string> NativeModuleFunctionAddressModules,
		string CompileOptionsPath,
		string EngineDistribution);

	private static BindingSettings ReadBindingSettings(IUhtExportFactory factory, string runtimeBuildCsPath)
	{
		AngelscriptFunctionBindingMethod method = AngelscriptFunctionBindingMethod.NativeRuntimeLinked;
		HashSet<string> runtimeLinkedModules = new(StringComparer.OrdinalIgnoreCase);
		HashSet<string> nativeModuleFunctionAddressModules = new(StringComparer.OrdinalIgnoreCase);
		foreach (string candidate in EnumerateCompileOptionsCandidates(runtimeBuildCsPath))
		{
			if (!File.Exists(candidate))
			{
				continue;
			}

			factory.AddExternalDependency(candidate);
			bool inSection = false;
			foreach (string rawLine in File.ReadAllLines(candidate))
			{
				string line = rawLine.Trim();
				if (line.Length == 0 || line.StartsWith(';') || line.StartsWith('#'))
				{
					continue;
				}
				if (line.StartsWith('[') && line.EndsWith(']'))
				{
					inSection = string.Equals(line[1..^1], CompileOptionsSectionName, StringComparison.Ordinal);
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
				string key = line[..separatorIndex].Trim().TrimStart('+');
				string value = line[(separatorIndex + 1)..].Trim();
				if (key.Equals(FunctionBindingMethodSettingName, StringComparison.OrdinalIgnoreCase))
				{
					method = ParseFunctionBindingMethod(value, candidate);
				}
				else if (key.Equals(NativeRuntimeLinkedModulesSettingName, StringComparison.OrdinalIgnoreCase))
				{
					AddConfiguredModule(runtimeLinkedModules, value, candidate, key);
				}
				else if (key.Equals(NativeModuleFunctionAddressModulesSettingName, StringComparison.OrdinalIgnoreCase))
				{
					AddConfiguredModule(nativeModuleFunctionAddressModules, value, candidate, key);
				}
			}

			string? engineDirectory = ResolveEngineDirectory(factory);
			string engineDistribution = ClassifyEngineDistribution(engineDirectory);
			if (method == AngelscriptFunctionBindingMethod.NativeModuleFunctionAddress && !engineDistribution.Equals("source", StringComparison.OrdinalIgnoreCase))
			{
				throw new InvalidOperationException($"NativeModuleFunctionAddress compilation requires a source engine. Engine '{engineDirectory ?? "<unknown>"}' is classified as {engineDistribution}.");
			}
			return new BindingSettings(method, runtimeLinkedModules, nativeModuleFunctionAddressModules, candidate, engineDistribution);
		}

		return new BindingSettings(method, runtimeLinkedModules, nativeModuleFunctionAddressModules, string.Empty, ClassifyEngineDistribution(ResolveEngineDirectory(factory)));
	}

	private static AngelscriptFunctionBindingMethod ParseFunctionBindingMethod(string value, string configPath)
	{
		return value switch
		{
			"None" => AngelscriptFunctionBindingMethod.None,
			"NativeRuntimeLinked" => AngelscriptFunctionBindingMethod.NativeRuntimeLinked,
			"NativeModuleFunctionAddress" => AngelscriptFunctionBindingMethod.NativeModuleFunctionAddress,
			_ => throw new InvalidDataException($"Unknown FunctionBindingMethod '{value}' in {configPath}.")
		};
	}

	private static void AddConfiguredModule(HashSet<string> modules, string value, string configPath, string key)
	{
		if (value.Length == 0)
		{
			throw new InvalidDataException($"{key} contains an empty module name in {configPath}.");
		}
		modules.Add(value);
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

	private static string ClassifyEngineDistribution(string? engineDirectory)
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
		if (File.Exists(Path.Combine(normalizedDirectory, "Build", "SourceDistribution.txt")) || Directory.Exists(Path.Combine(normalizedDirectory, ".git")) || (Directory.GetParent(normalizedDirectory) is DirectoryInfo parent && Directory.Exists(Path.Combine(parent.FullName, ".git"))))
		{
			return "source";
		}
		return "unknown";
	}

	private static string LoadLayoutVersion(IUhtExportFactory factory)
	{
		string runtimeBuildCsPath = ResolveRuntimeBuildCsPath(factory);
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
				DeleteStaleFiles(outputDirectory, "AS_FunctionTable_Engine_LinkProbe.cpp", livePaths);
				DeleteStaleFiles(outputDirectory, "AS_FunctionTable_Engine_ModuleBinding_LinkProbe.cpp", livePaths);
			}
		}
	}

	private static void DeleteStaleFiles(string outputDirectory, string pattern, HashSet<string> livePaths)
	{
		if (!Directory.Exists(outputDirectory))
		{
			return;
		}
		foreach (string existingFile in Directory.EnumerateFiles(outputDirectory, pattern))
		{
			if (!livePaths.Contains(Path.GetFullPath(existingFile)))
			{
				File.Delete(existingFile);
			}
		}
	}

	private static bool ShouldGenerate(UhtClass classObj, UhtFunction function)
	{
		if (classObj.HeaderFile == null || !IsSupportedHeader(classObj.HeaderFile.FilePath) || !AngelscriptFunctionBindingExporter.IsAngelscriptCallable(function))
		{
			return false;
		}
		if (function.MetaData.ContainsKey("NotInAngelscript") || (function.MetaData.ContainsKey("BlueprintInternalUseOnly") && !function.MetaData.ContainsKey("UsableInAngelscript")))
		{
			return false;
		}
		if (classObj.SourceName == "UUniversalObjectLocatorScriptingExtensions" && (function.SourceName == "MakeUniversalObjectLocator" || function.SourceName == "UniversalObjectLocatorFromString"))
		{
			return false;
		}
		return !function.FunctionExportFlags.ToString().Contains("CustomThunk", StringComparison.Ordinal);
	}

	private static bool IsSupportedHeader(string headerPath)
	{
		string normalizedPath = headerPath.Replace('\\', '/');
		return !normalizedPath.Contains("/Private/", StringComparison.OrdinalIgnoreCase) && !normalizedPath.EndsWith("/Components/InstancedSkinnedMeshComponent.h", StringComparison.OrdinalIgnoreCase);
	}

	private static bool IsRpcNetFunction(UhtFunction function)
	{
		return function.FunctionFlags.HasAnyFlags(EFunctionFlags.Net | EFunctionFlags.NetServer | EFunctionFlags.NetClient | EFunctionFlags.NetMulticast);
	}

	private static string? ResolveEditorOnlyGuard(UhtFunction function)
	{
		if (!function.FunctionFlags.HasAnyFlags(EFunctionFlags.EditorOnly))
		{
			return null;
		}
		bool withEditor = (function.DefineScope & UhtDefineScope.Editor) != 0;
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

	private static bool IsSafeNativeModuleFunctionAddressSignature(AngelscriptFunctionSignature signature, UhtClass classObj, UhtFunction function)
	{
		return IsSafeReturn(signature, function) && HasOnlySafeParameters(function) && !HasOutParams(function) && !HasWorldContext(function) && !HasReturnReference(signature, function) && !HasScriptMethodMixinProjection(signature, classObj, function);
	}

	private static string ClassifyUnsupportedNativeModuleFunctionBindingSignature(AngelscriptFunctionSignature signature, UhtClass classObj, UhtFunction function)
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
		return returnProperty is UhtBoolProperty or UhtNumericProperty or UhtEnumProperty or UhtStructProperty or UhtStrProperty or UhtNameProperty or UhtTextProperty || returnProperty is UhtObjectProperty && signature.ReturnType.EndsWith("*", StringComparison.Ordinal);
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

	private static bool HasWorldContext(UhtFunction function) => function.MetaData.ContainsKey("WorldContext");
	private static bool HasOutParams(UhtFunction function) => HasParameter(function, static type => type is UhtProperty property && property.PropertyFlags.ToString().Contains("OutParm", StringComparison.Ordinal));
	private static bool HasReferenceParameters(UhtFunction function) => HasParameter(function, static type => type is UhtProperty property && property.PropertyFlags.ToString().Contains("ReferenceParm", StringComparison.Ordinal));
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
	private static bool ReturnsByRef(UhtFunction function) => function.ReturnProperty is UhtProperty property && property.PropertyFlags.ToString().Contains("ReferenceParm", StringComparison.Ordinal);
	private static bool HasReturnReference(AngelscriptFunctionSignature signature, UhtFunction function) => ReturnsByRef(function) || signature.ReturnType.Contains("&", StringComparison.Ordinal);

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
