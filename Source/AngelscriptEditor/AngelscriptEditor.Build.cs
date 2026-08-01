using System.IO;
using UnrealBuildTool;

namespace UnrealBuildTool.Rules
{
	public class AngelscriptEditor : ModuleRules
	{
		public AngelscriptEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

			PublicIncludePaths.Add(ModuleDirectory);
			PrivateIncludePaths.Add(ModuleDirectory);
			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "LearningTrace"));
			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "LearningTrace", "Core"));
			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "LearningTrace", "Phases"));
			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "LearningTrace", "Examples"));
			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "LearningTrace", "Exporter"));

			PublicDependencyModuleNames.AddRange(new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"UnrealEd",
				"EditorSubsystem",
				"AngelscriptRuntime",
				"BlueprintGraph",
				"Kismet",
				"DirectoryWatcher",
				"Slate",
				"SlateCore",
				"AssetTools",
				"AssetRegistry",
            });

			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"Projects",
				"Settings",
				"LevelEditor",
				"PlacementMode",
				"PropertyEditor",
				"ContentBrowser",
				"ContentBrowserData",
				"ToolMenus",
				"ToolWidgets",
				"Json",
				"JsonUtilities",
				"CQTest",
				"AutomationController",
            });
		}
	}
}
