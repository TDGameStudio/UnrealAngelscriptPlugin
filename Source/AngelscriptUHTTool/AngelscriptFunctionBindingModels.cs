using System.Collections.Generic;

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
	string IncludePath,
	string EraseMacro,
	string FunctionBindingCategory,
	string? EditorOnlyGuard = null,
	string? FailureReason = null)
{
	public string BuildBindingRegistrationLine()
	{
		return $"\tFAngelscriptBinds::RegisterGeneratedFunctionBinding({ClassName}::StaticClass(), \"{FunctionName}\", {{ {EraseMacro} }});";
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
	int ConfiguredModuleMissCount,
	IReadOnlyList<string> ConfiguredModuleMisses);

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
	string? FailureReason,
	string EraseMacro,
	string ArtifactName,
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
	string FailureReason,
	string Result = "Skipped",
	string ArtifactName = "");
