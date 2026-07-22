using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnrealBuildTool;

namespace UnrealBuildTool.Rules
{
	public class AngelscriptRuntime : ModuleRules
	{
		private const string FunctionBindingSettingsSection = "/Script/AngelscriptRuntime.AngelscriptCompileOptions";
		private const string FunctionBindingMethodKey = "FunctionBindingMethod";
		private const string NativeRuntimeLinkedModulesKey = "NativeRuntimeLinkedModules";
		private const string NativeModuleFunctionAddressModulesKey = "NativeModuleFunctionAddressModules";

		public AngelscriptRuntime(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
			NumIncludedBytesPerUnityCPPOverride = 131072;
			FunctionBindingSettings BindingSettings = ReadFunctionBindingSettings(Target);
			PrivateDefinitions.Add("ANGELSCRIPT_EXPORT=1");
			PublicDefinitions.Add("WITH_ANGELSCRIPT=1");
			PublicDefinitions.Add("WITH_ANGELSCRIPT_NATIVE_MODULE_FUNCTION_ADDRESS=" + (BindingSettings.Method == FunctionBindingMethod.NativeModuleFunctionAddress ? "1" : "0"));
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

			if (BindingSettings.Method == FunctionBindingMethod.NativeRuntimeLinked)
			{
				AddConfiguredRuntimeLinkedDependencies(BindingSettings.RuntimeLinkedModules, Target);
				AddGeneratedFunctionBindingWrappers(BindingSettings.RuntimeLinkedModules);
			}

            //var PluginPath = "../Plugins/Angelscript";
            //var PluginPath = "./Plugins/Angelscript";
            //var PluginPath = "./";

			/* Link to Angelscript */
			//PublicIncludePaths.Add(PluginPath + "/ThirdParty/include");
			//PublicIncludePaths.Add(PluginPath + "/ThirdParty/source");
		}

		private void AddGeneratedFunctionBindingWrappers(HashSet<string> ModuleNames)
		{
			foreach (string ModuleName in ModuleNames.OrderBy(static Name => Name, StringComparer.OrdinalIgnoreCase))
			{
				AddGeneratedFunctionBindingModuleWrappers(ModuleName);
			}
		}

		private void AddGeneratedFunctionBindingModuleWrappers(string ModuleName)
		{
			string GeneratedSourceName = $"AS_FunctionBinding_{ModuleName}";
			string[] AggregatorSource =
			{
				$"#if __has_include(\"{GeneratedSourceName}.gen.cpp\")",
				$"#include UE_INLINE_GENERATED_CPP_BY_NAME({GeneratedSourceName})",
				"#endif",
			};

			FilesToGenerate.Add(
				$"AngelscriptGeneratedFunctionBindingWrappers/AS_FunctionBinding_{ModuleName}_Aggregator.cpp",
				AggregatorSource);
		}

		private void AddConfiguredRuntimeLinkedDependencies(HashSet<string> ModuleNames, ReadOnlyTargetRules Target)
		{
			HashSet<string> ExistingDependencies = new(StringComparer.OrdinalIgnoreCase);
			foreach (string Dependency in PublicDependencyModuleNames)
			{
				ExistingDependencies.Add(Dependency);
			}
			foreach (string Dependency in PrivateDependencyModuleNames)
			{
				ExistingDependencies.Add(Dependency);
			}

			HashSet<string> EditorOnlyModules = new(StringComparer.OrdinalIgnoreCase)
			{
				"UMGEditor",
				"UnrealEd",
			};
			foreach (string ModuleName in ModuleNames)
			{
				if (ModuleName.Length == 0 || ModuleName.Equals("AngelscriptRuntime", StringComparison.OrdinalIgnoreCase) || (!Target.bBuildEditor && EditorOnlyModules.Contains(ModuleName)))
				{
					continue;
				}

				if (!ExistingDependencies.Contains(ModuleName))
				{
					PrivateDependencyModuleNames.Add(ModuleName);
					ExistingDependencies.Add(ModuleName);
				}
			}
		}

		private sealed record FunctionBindingSettings(
			FunctionBindingMethod Method,
			HashSet<string> RuntimeLinkedModules,
			HashSet<string> NativeModuleFunctionAddressModules);

		private enum FunctionBindingMethod
		{
			None,
			NativeRuntimeLinked,
			NativeModuleFunctionAddress,
		}

		private FunctionBindingSettings ReadFunctionBindingSettings(ReadOnlyTargetRules Target)
		{
			HashSet<string> RuntimeLinkedModules = new(StringComparer.OrdinalIgnoreCase);
			HashSet<string> NativeModuleFunctionAddressModules = new(StringComparer.OrdinalIgnoreCase);
			if (Target.ProjectFile == null)
			{
				return new FunctionBindingSettings(FunctionBindingMethod.NativeRuntimeLinked, RuntimeLinkedModules, NativeModuleFunctionAddressModules);
			}

			string? ProjectDirectory = Path.GetDirectoryName(Target.ProjectFile.FullName);
			if (string.IsNullOrEmpty(ProjectDirectory))
			{
				return new FunctionBindingSettings(FunctionBindingMethod.NativeRuntimeLinked, RuntimeLinkedModules, NativeModuleFunctionAddressModules);
			}

			string ConfigPath = Path.Combine(ProjectDirectory, "Config", "DefaultAngelscriptCompileOptions.ini");
			ExternalDependencies.Add(ConfigPath);

			const string SettingSection = FunctionBindingSettingsSection;
			if (!File.Exists(ConfigPath))
			{
				return new FunctionBindingSettings(FunctionBindingMethod.NativeRuntimeLinked, RuntimeLinkedModules, NativeModuleFunctionAddressModules);
			}

			FunctionBindingMethod Method = FunctionBindingMethod.NativeRuntimeLinked;
			bool bInSection = false;
			foreach (string RawLine in File.ReadAllLines(ConfigPath))
			{
				string Line = RawLine.Trim();
				if (Line.Length == 0 || Line.StartsWith(";") || Line.StartsWith("#"))
				{
					continue;
				}

				if (Line.StartsWith("[") && Line.EndsWith("]"))
				{
					bInSection = string.Equals(Line.Substring(1, Line.Length - 2), SettingSection, StringComparison.Ordinal);
					continue;
				}

				if (!bInSection)
				{
					continue;
				}

				int SeparatorIndex = Line.IndexOf('=');
				if (SeparatorIndex <= 0)
				{
					continue;
				}

				string RawKey = Line.Substring(0, SeparatorIndex).Trim();
				char Operation = RawKey.Length > 0 && (RawKey[0] == '+' || RawKey[0] == '-' || RawKey[0] == '!') ? RawKey[0] : '\0';
				string Key = Operation == '\0' ? RawKey : RawKey.Substring(1).Trim();
				string Value = Line.Substring(SeparatorIndex + 1).Trim();
				if (Key.Equals(FunctionBindingMethodKey, StringComparison.OrdinalIgnoreCase))
				{
					if (Operation != '\0')
					{
					throw new BuildException("{0} does not support array operation '{1}' in '{2}'.", FunctionBindingMethodKey, Operation, ConfigPath);
					}
					Method = ParseFunctionBindingMethod(Value);
				}
				else if (Key.Equals(NativeRuntimeLinkedModulesKey, StringComparison.OrdinalIgnoreCase))
				{
					ApplyModuleArrayOperation(RuntimeLinkedModules, Operation, Value, Key, ConfigPath);
				}
				else if (Key.Equals(NativeModuleFunctionAddressModulesKey, StringComparison.OrdinalIgnoreCase))
				{
					ApplyModuleArrayOperation(NativeModuleFunctionAddressModules, Operation, Value, Key, ConfigPath);
				}
			}

			if (Method == FunctionBindingMethod.NativeModuleFunctionAddress && !IsSourceEngine())
			{
				throw new BuildException(
					"NativeModuleFunctionAddress compilation requires a source engine. Engine '{0}' is installed, binary, or unknown; select None or NativeRuntimeLinked, or use a source engine.",
					EngineDirectory);
			}

			return new FunctionBindingSettings(Method, RuntimeLinkedModules, NativeModuleFunctionAddressModules);
		}

		private static void ApplyModuleArrayOperation(HashSet<string> Modules, char Operation, string Value, string Key, string ConfigPath)
		{
			if (Operation == '!')
			{
				if (Value.Length > 0 && !Value.Equals("ClearArray", StringComparison.OrdinalIgnoreCase))
				{
					throw new BuildException("{0} uses '!'; expected an empty value or ClearArray in '{1}'.", Key, ConfigPath);
				}
				Modules.Clear();
				return;
			}

			if (Value.Length == 0)
			{
				throw new BuildException("{0} contains an empty module name in '{1}'.", Key, ConfigPath);
			}

			switch (Operation)
			{
				case '+':
					Modules.Add(Value);
					break;
				case '-':
					Modules.Remove(Value);
					break;
				default:
					Modules.Clear();
					Modules.Add(Value);
					break;
			}
		}

		private static FunctionBindingMethod ParseFunctionBindingMethod(string Value)
		{
			return Value switch
			{
				"None" => FunctionBindingMethod.None,
				"NativeRuntimeLinked" => FunctionBindingMethod.NativeRuntimeLinked,
				"NativeModuleFunctionAddress" => FunctionBindingMethod.NativeModuleFunctionAddress,
				_ => throw new BuildException("Unknown FunctionBindingMethod '{0}'. Expected None, NativeRuntimeLinked, or NativeModuleFunctionAddress.", Value),
			};
		}

		private bool IsSourceEngine()
		{
			string NormalizedEngineDirectory = EngineDirectory.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
			if (File.Exists(Path.Combine(NormalizedEngineDirectory, "Build", "InstalledBuild.txt")))
			{
				return false;
			}

			if (File.Exists(Path.Combine(NormalizedEngineDirectory, "Build", "SourceDistribution.txt")))
			{
				return true;
			}

			for (DirectoryInfo? Directory = new DirectoryInfo(NormalizedEngineDirectory); Directory != null; Directory = Directory.Parent)
			{
				if (File.Exists(Path.Combine(Directory.FullName, ".git")) || System.IO.Directory.Exists(Path.Combine(Directory.FullName, ".git")))
				{
					return true;
				}
			}

			return false;
		}
	}
}
