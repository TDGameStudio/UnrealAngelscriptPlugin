using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnrealBuildTool;

namespace UnrealBuildTool.Rules
{
	public class AngelscriptRuntime : ModuleRules
	{
		public AngelscriptRuntime(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
			NumIncludedBytesPerUnityCPPOverride = 131072;
			FunctionBindingSettings bindingSettings = ReadFunctionBindingSettings(Target);
			PrivateDefinitions.Add("ANGELSCRIPT_EXPORT=1");
			PublicDefinitions.Add("WITH_ANGELSCRIPT=1");
			PublicDefinitions.Add("WITH_ANGELSCRIPT_NATIVE_MODULE_FUNCTION_ADDRESS=" + (bindingSettings.Method == FunctionBindingMethod.NativeModuleFunctionAddress ? "1" : "0"));
			PublicDefinitions.Add("ANGELSCRIPT_DLL_LIBRARY_IMPORT=1");

			PublicIncludePaths.Add(ModuleDirectory);
			PrivateIncludePaths.Add(ModuleDirectory);
			PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Core"));
			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Core"));
			PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Core", "Commandlets"));
			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Core", "Commandlets"));

			var AngelscriptThirdPartyPath = Path.Combine(ModuleDirectory, "ThirdParty", "angelscript");
			PublicIncludePaths.Add(Path.Combine(AngelscriptThirdPartyPath, "source"));
			PublicIncludePaths.Add(AngelscriptThirdPartyPath);

			if (Target.Configuration == UnrealTargetConfiguration.Debug || Target.Configuration == UnrealTargetConfiguration.DebugGame)
			{
				OptimizeCode = CodeOptimization.Never;
			}

			/* Link to libraries used in core angelscript code */
			PublicDependencyModuleNames.AddRange(new string[]
			{
				"ApplicationCore",
				"Core",
				"CoreUObject",
				"Engine",
				"EngineSettings",
				"DeveloperSettings",
				"Json",
				"JsonUtilities",
            });

			/* Link to libraries used in bindings */
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"AIModule",
				"NavigationSystem",
				"NetCore",
				"Landscape",
				"Networking",
				"Sockets",
				"InputCore",
				"SlateCore",
				"Slate",
				"UMG",
				"TraceLog",
				"AssetRegistry",
				"Projects",
				"PhysicsCore",
				"CoreOnline",
				"EnhancedInput",
            });

			if (Target.bBuildEditor)
			{
				PublicDependencyModuleNames.AddRange(new string[] 
				{
					"UnrealEd",
					"EditorSubsystem",
				});

				PrivateDependencyModuleNames.AddRange(new string[]
				{
					"UMGEditor",
				});
			}

			if (bindingSettings.Method == FunctionBindingMethod.NativeRuntimeLinked)
			{
				AddConfiguredRuntimeLinkedDependencies(bindingSettings.RuntimeLinkedModules, Target);
				AddGeneratedFunctionBindingWrappers(bindingSettings.RuntimeLinkedModules);
			}

            //var PluginPath = "../Plugins/Angelscript";
            //var PluginPath = "./Plugins/Angelscript";
            //var PluginPath = "./";

			/* Link to Angelscript */
			//PublicIncludePaths.Add(PluginPath + "/ThirdParty/include");
			//PublicIncludePaths.Add(PluginPath + "/ThirdParty/source");
		}

		private void AddGeneratedFunctionBindingWrappers(HashSet<string> moduleNames)
		{
			foreach (string moduleName in moduleNames.OrderBy(static name => name, StringComparer.OrdinalIgnoreCase))
			{
				AddGeneratedFunctionBindingModuleWrappers(moduleName);
			}
		}

		private void AddGeneratedFunctionBindingModuleWrappers(string moduleName)
		{
			const int MaxShardCount = 64;
			for (int shardIndex = 0; shardIndex < MaxShardCount; shardIndex++)
			{
				string shardName = $"AS_FunctionBinding_{moduleName}_{shardIndex:D3}";
				FilesToGenerate.Add(
					$"AngelscriptGeneratedFunctionBindingWrappers/{shardName}.cpp",
					new[]
					{
						$"#if __has_include(\"{shardName}.gen.cpp\")",
						$"#include UE_INLINE_GENERATED_CPP_BY_NAME({shardName})",
						"#endif",
					});
			}
		}

		private void AddConfiguredRuntimeLinkedDependencies(HashSet<string> moduleNames, ReadOnlyTargetRules target)
		{
			HashSet<string> existingDependencies = new(StringComparer.OrdinalIgnoreCase);
			foreach (string dependency in PublicDependencyModuleNames)
			{
				existingDependencies.Add(dependency);
			}
			foreach (string dependency in PrivateDependencyModuleNames)
			{
				existingDependencies.Add(dependency);
			}

			HashSet<string> editorOnlyModules = new(StringComparer.OrdinalIgnoreCase)
			{
				"UMGEditor",
				"UnrealEd",
			};
			foreach (string moduleName in moduleNames)
			{
				if (moduleName.Length == 0 || moduleName.Equals("AngelscriptRuntime", StringComparison.OrdinalIgnoreCase) || (!target.bBuildEditor && editorOnlyModules.Contains(moduleName)))
				{
					continue;
				}

				if (!existingDependencies.Contains(moduleName))
				{
					PrivateDependencyModuleNames.Add(moduleName);
					existingDependencies.Add(moduleName);
				}
			}
		}

		private sealed record FunctionBindingSettings(FunctionBindingMethod Method, HashSet<string> RuntimeLinkedModules);

		private enum FunctionBindingMethod
		{
			None,
			NativeRuntimeLinked,
			NativeModuleFunctionAddress,
		}

		private FunctionBindingSettings ReadFunctionBindingSettings(ReadOnlyTargetRules target)
		{
			HashSet<string> runtimeLinkedModules = new(StringComparer.OrdinalIgnoreCase);
			if (target.ProjectFile == null)
			{
				return new FunctionBindingSettings(FunctionBindingMethod.NativeRuntimeLinked, runtimeLinkedModules);
			}

			string? projectDirectory = Path.GetDirectoryName(target.ProjectFile.FullName);
			if (string.IsNullOrEmpty(projectDirectory))
			{
				return new FunctionBindingSettings(FunctionBindingMethod.NativeRuntimeLinked, runtimeLinkedModules);
			}

			string configPath = Path.Combine(projectDirectory, "Config", "DefaultAngelscriptCompileOptions.ini");
			ExternalDependencies.Add(configPath);

			const string settingSection = "/Script/AngelscriptRuntime.AngelscriptCompileOptions";
			if (!File.Exists(configPath))
			{
				return new FunctionBindingSettings(FunctionBindingMethod.NativeRuntimeLinked, runtimeLinkedModules);
			}

			FunctionBindingMethod method = FunctionBindingMethod.NativeRuntimeLinked;
			bool inSection = false;
			foreach (string rawLine in File.ReadAllLines(configPath))
			{
				string line = rawLine.Trim();
				if (line.Length == 0 || line.StartsWith(";") || line.StartsWith("#"))
				{
					continue;
				}

				if (line.StartsWith("[") && line.EndsWith("]"))
				{
					inSection = string.Equals(line.Substring(1, line.Length - 2), settingSection, StringComparison.Ordinal);
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

				string key = line.Substring(0, separatorIndex).Trim().TrimStart('+');
				string value = line.Substring(separatorIndex + 1).Trim();
				if (key.Equals("FunctionBindingMethod", StringComparison.OrdinalIgnoreCase))
				{
					method = ParseFunctionBindingMethod(value);
				}
				else if (key.Equals("NativeRuntimeLinkedModules", StringComparison.OrdinalIgnoreCase))
				{
					if (value.Length == 0)
					{
						throw new BuildException("NativeRuntimeLinkedModules contains an empty module name in '{0}'.", configPath);
					}
					runtimeLinkedModules.Add(value);
				}
			}

			if (method == FunctionBindingMethod.NativeModuleFunctionAddress && !IsSourceEngine())
			{
				throw new BuildException(
					"NativeModuleFunctionAddress compilation requires a source engine. Engine '{0}' is installed, binary, or unknown; select None or NativeRuntimeLinked, or use a source engine.",
					EngineDirectory);
			}

			return new FunctionBindingSettings(method, runtimeLinkedModules);
		}

		private static FunctionBindingMethod ParseFunctionBindingMethod(string value)
		{
			return value switch
			{
				"None" => FunctionBindingMethod.None,
				"NativeRuntimeLinked" => FunctionBindingMethod.NativeRuntimeLinked,
				"NativeModuleFunctionAddress" => FunctionBindingMethod.NativeModuleFunctionAddress,
				_ => throw new BuildException("Unknown FunctionBindingMethod '{0}'. Expected None, NativeRuntimeLinked, or NativeModuleFunctionAddress.", value),
			};
		}

		private static bool IsSourceEngine()
		{
			string NormalizedEngineDirectory = EngineDirectory.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
			if (File.Exists(Path.Combine(NormalizedEngineDirectory, "Build", "InstalledBuild.txt")))
			{
				return false;
			}

			if (File.Exists(Path.Combine(NormalizedEngineDirectory, "Build", "SourceDistribution.txt")) ||
				Directory.Exists(Path.Combine(NormalizedEngineDirectory, ".git")))
			{
				return true;
			}

			DirectoryInfo? EngineParent = Directory.GetParent(NormalizedEngineDirectory);
			return EngineParent != null && Directory.Exists(Path.Combine(EngineParent.FullName, ".git"));
		}
	}
}
