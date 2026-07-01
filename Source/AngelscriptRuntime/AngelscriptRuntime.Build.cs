using System.IO;
using UnrealBuildTool;

namespace UnrealBuildTool.Rules
{
	public class AngelscriptRuntime : ModuleRules
	{
		public AngelscriptRuntime(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
			NumIncludedBytesPerUnityCPPOverride = 131072;
			PrivateDefinitions.Add("ANGELSCRIPT_EXPORT=1");
			PublicDefinitions.Add("WITH_ANGELSCRIPT=1");
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

			AddGeneratedFunctionTableWrappers();

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

            //var PluginPath = "../Plugins/Angelscript";
            //var PluginPath = "./Plugins/Angelscript";
            //var PluginPath = "./";

			/* Link to Angelscript */
			//PublicIncludePaths.Add(PluginPath + "/ThirdParty/include");
			//PublicIncludePaths.Add(PluginPath + "/ThirdParty/source");
		}

		private void AddGeneratedFunctionTableWrappers()
		{
			AddGeneratedFunctionTableModuleWrappers("AIModule", 2);
			AddGeneratedFunctionTableModuleWrappers("AngelscriptRuntime", 2);
			AddGeneratedFunctionTableModuleWrappers("AssetRegistry", 2);
			AddGeneratedFunctionTableModuleWrappers("Engine", 32);
			AddGeneratedFunctionTableModuleWrappers("EngineSettings", 2);
			AddGeneratedFunctionTableModuleWrappers("EnhancedInput", 2);
			AddGeneratedFunctionTableModuleWrappers("Landscape", 2);
			AddGeneratedFunctionTableModuleWrappers("NavigationSystem", 2);
			AddGeneratedFunctionTableModuleWrappers("UMG", 8);
			AddGeneratedFunctionTableModuleWrappers("UMGEditor", 2);
			AddGeneratedFunctionTableModuleWrappers("UnrealEd", 4);
		}

		private void AddGeneratedFunctionTableModuleWrappers(string ModuleName, int MaxShardCount)
		{
			for (int ShardIndex = 0; ShardIndex < MaxShardCount; ShardIndex++)
			{
				string ShardName = $"AS_FunctionTable_{ModuleName}_{ShardIndex:D3}";
				FilesToGenerate.Add(
					$"AngelscriptGeneratedFunctionTableWrappers/{ShardName}.cpp",
					new[]
					{
						$"#if __has_include(\"{ShardName}.gen.cpp\")",
						$"#include UE_INLINE_GENERATED_CPP_BY_NAME({ShardName})",
						"#endif",
					});
			}
		}
	}
}
