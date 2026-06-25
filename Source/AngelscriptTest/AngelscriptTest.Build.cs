using System.IO;
using UnrealBuildTool;

namespace UnrealBuildTool.Rules
{
	public class AngelscriptTest : ModuleRules
	{
		public AngelscriptTest(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
			bUseUnity = false;
			PrivateDefinitions.Add("AS_ENABLE_EDITOR_JITTED_CODE=1");

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
				"UMG",
			});

			if (Target.bBuildEditor)
			{
				PrivateDependencyModuleNames.AddRange(new string[]
				{
					"BlueprintGraph",
					"CQTest",
					"Networking",
					"Sockets",
					"UnrealEd",
					"AngelscriptEditor",
				});
			}
		}
	}
}
