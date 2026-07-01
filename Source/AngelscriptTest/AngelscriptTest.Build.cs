using System.IO;
using System;
using UnrealBuildTool;

namespace UnrealBuildTool.Rules
{
	public class AngelscriptTest : ModuleRules
	{
		public AngelscriptTest(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
			bUseUnity = true;
			PrivateDefinitions.Add("AS_ENABLE_EDITOR_JITTED_CODE=1");

			bool bCompileAngelscriptUnitTests = ReadCompileUnitTestsSetting(Target);
			PublicDefinitions.Add("WITH_ANGELSCRIPT_UNITTESTS=" + (bCompileAngelscriptUnitTests ? "1" : "0"));
			ForceIncludeFiles.Add("AngelscriptCQTest.h");

			// Module root + subdirectories mirroring AngelscriptRuntime layout
			PublicIncludePaths.Add(ModuleDirectory);
			PrivateIncludePaths.Add(ModuleDirectory);
			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Core"));
			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Debugger"));
			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Dump"));
			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "AngelScriptSDK"));

			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Preprocessor"));
			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "ClassGenerator"));
			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Shared"));

			PublicDependencyModuleNames.AddRange(new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayTags",
				"Json",
				"JsonUtilities",
				"PropertyBindingUtils",
				"AngelscriptRuntime",
			});

			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"AIModule",
				"EnhancedInput",
				"InputCore",
				"Slate",
				"SlateCore",
				"UMG",
			});

			if (Target.bBuildEditor)
			{
				PrivateDependencyModuleNames.AddRange(new string[]
				{
					"BlueprintGraph",
					"CQTest",
					"LevelEditor",
					"Networking",
					"Sockets",
					"UnrealEd",
					"AngelscriptEditor",
				});
			}
		}

		private bool ReadCompileUnitTestsSetting(ReadOnlyTargetRules Target)
		{
			if (Target.ProjectFile == null)
			{
				return false;
			}

			string ConfigPath = Path.Combine(Path.GetDirectoryName(Target.ProjectFile.FullName), "Config", "DefaultAngelscriptCompileOptions.ini");
			ExternalDependencies.Add(ConfigPath);

			const string SettingSection = "/Script/AngelscriptRuntime.AngelscriptCompileOptions";
			const string SettingName = "bCompileAngelscriptUnitTests";
			if (!File.Exists(ConfigPath))
			{
				return false;
			}

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

				string Key = Line.Substring(0, SeparatorIndex).Trim();
				if (!string.Equals(Key, SettingName, StringComparison.OrdinalIgnoreCase))
				{
					continue;
				}

				string Value = Line.Substring(SeparatorIndex + 1).Trim();
				return Value.Equals("true", StringComparison.OrdinalIgnoreCase)
					|| Value.Equals("1", StringComparison.OrdinalIgnoreCase)
					|| Value.Equals("yes", StringComparison.OrdinalIgnoreCase);
			}

			return false;
		}
	}
}
