using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using EpicGames.UHT.Types;
using EpicGames.UHT.Utils;

namespace AngelscriptUHTTool;

internal static partial class AngelscriptFunctionBindingCodeGenerator
{
	private static StringBuilder BuildGeneratedFunctionBindingShard(string moduleName, bool editorOnly, List<AngelscriptGeneratedFunctionRegistration> bindings, int startIndex, int bindingCount, int shardIndex, int shardCount)
	{
		SortedSet<string> includes = new(StringComparer.Ordinal);
		for (int bindingIndex = startIndex; bindingIndex < startIndex + bindingCount; bindingIndex++)
		{
			if (bindings[bindingIndex].IncludePath.Length > 0)
			{
				includes.Add(bindings[bindingIndex].IncludePath);
			}
		}

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
		for (int bindingIndex = startIndex; bindingIndex < startIndex + bindingCount; bindingIndex++)
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
		builder.Append("\tFAngelscriptBinds::RecordGeneratedFunctionBindingShardTiming(TEXT(\"").Append(moduleName).Append("\"), ").Append(shardIndex + 1).Append(", ").Append(shardCount).Append(", ").Append(bindingCount).AppendLine(", GeneratedFunctionBindingElapsedMilliseconds);");
		builder.Append("\tUE_LOG(Angelscript, Log, TEXT(\"[UHT] Registered %d generated AS-callable bindings for module %s shard %d/%d in %.3f ms\"), ").Append(bindingCount).Append(", TEXT(\"").Append(moduleName).Append("\"), ").Append(shardIndex + 1).Append(", ").Append(shardCount).AppendLine(", GeneratedFunctionBindingElapsedMilliseconds);");
		builder.AppendLine("});");
		builder.AppendLine("PRAGMA_ENABLE_DEPRECATION_WARNINGS");
		if (editorOnly)
		{
			builder.AppendLine("#endif");
		}
		return builder;
	}

	private static StringBuilder BuildNativeModuleFunctionAddressShard(string moduleName, List<AngelscriptNativeModuleFunctionBinding> bindings, int startIndex, int bindingCount, int shardIndex, int shardCount, string layoutVersion)
	{
		SortedSet<string> includes = new(StringComparer.Ordinal);
		for (int bindingIndex = startIndex; bindingIndex < startIndex + bindingCount; bindingIndex++)
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

		for (int bindingIndex = startIndex; bindingIndex < startIndex + bindingCount; bindingIndex++)
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
		for (int bindingIndex = startIndex; bindingIndex < startIndex + bindingCount; bindingIndex++)
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
}
