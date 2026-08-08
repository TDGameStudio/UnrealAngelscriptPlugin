#include "AngelscriptBinds.h"

#include "Misc/Paths.h"

#include "Bind_FPaths_Functions.h"

namespace
{
	void BindFPaths(FAngelscriptBinds& Binds)
	{
		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FPaths");

		Binds.BindGlobalFunctionForTarget("FString RootDir()", &FPaths::RootDir);
		Binds.BindGlobalFunctionForTarget("FString LaunchDir()", &FPaths::LaunchDir);
		Binds.BindGlobalFunctionForTarget("FString CombinePaths(const FString& FirstPath, const FString& SecondPath)", &FAngelscriptFPathsBinds::CombinePaths);
		Binds.BindGlobalFunctionForTarget("FString EngineDir()", &FPaths::EngineDir);
		Binds.BindGlobalFunctionForTarget("FString EngineContentDir()", &FPaths::EngineContentDir);
		Binds.BindGlobalFunctionForTarget("FString EngineConfigDir()", &FPaths::EngineConfigDir);
		Binds.BindGlobalFunctionForTarget("FString EngineEditorSettingsDir()", &FPaths::EngineEditorSettingsDir);
		Binds.BindGlobalFunctionForTarget("FString EngineIntermediateDir()", &FPaths::EngineIntermediateDir);
		Binds.BindGlobalFunctionForTarget("FString EngineSavedDir()", &FPaths::EngineSavedDir);
		Binds.BindGlobalFunctionForTarget("FString ProjectDir()", &FPaths::ProjectDir);
		Binds.BindGlobalFunctionForTarget("FString ProjectUserDir()", &FPaths::ProjectUserDir);
		Binds.BindGlobalFunctionForTarget("FString ProjectContentDir()", &FPaths::ProjectContentDir);
		Binds.BindGlobalFunctionForTarget("FString ProjectConfigDir()", &FPaths::ProjectConfigDir);
		Binds.BindGlobalFunctionForTarget("const FString& ProjectSavedDir()", &FPaths::ProjectSavedDir);
		Binds.BindGlobalFunctionForTarget("FString ProjectIntermediateDir()", &FPaths::ProjectIntermediateDir);
		Binds.BindGlobalFunctionForTarget("FString ScreenShotDir()", &FPaths::ScreenShotDir);
		Binds.BindGlobalFunctionForTarget("FString VideoCaptureDir()", &FPaths::VideoCaptureDir);
		Binds.BindGlobalFunctionForTarget("const FString& GetRelativePathToRoot()", &FPaths::GetRelativePathToRoot);

		Binds.BindGlobalFunctionForTarget("FString GetExtension(const FString& InPath, bool bIncludeDot = false)", &FAngelscriptFPathsBinds::GetExtension);
		Binds.BindGlobalFunctionForTarget("FString GetCleanFilename(const FString& InPath)", &FAngelscriptFPathsBinds::GetCleanFilename);
		Binds.BindGlobalFunctionForTarget("FString GetBaseFilename(const FString& InPath, bool bRemovePath = true)", &FAngelscriptFPathsBinds::GetBaseFilename);
		Binds.BindGlobalFunctionForTarget("FString GetPath(const FString& InPath)", &FAngelscriptFPathsBinds::GetPath);
		Binds.BindGlobalFunctionForTarget("FString GetPathLeaf(const FString& InPath)", &FAngelscriptFPathsBinds::GetPathLeaf);
		Binds.BindGlobalFunctionForTarget("FString ChangeExtension(const FString& InPath, const FString& InNewExtension)", &FPaths::ChangeExtension);
		Binds.BindGlobalFunctionForTarget("FString SetExtension(const FString& InPath, const FString& InNewExtension)", &FPaths::SetExtension);
		Binds.BindGlobalFunctionForTarget("void Split(const FString& InPath, FString& PathPart, FString& FilenamePart, FString& ExtensionPart)", &FPaths::Split);
		Binds.BindGlobalFunctionForTarget("bool FileExists(const FString& InPath)", &FPaths::FileExists);
		Binds.BindGlobalFunctionForTarget("bool DirectoryExists(const FString& InPath)", &FPaths::DirectoryExists);
		Binds.BindGlobalFunctionForTarget("bool IsDrive(const FString& InPath)", &FPaths::IsDrive);
		Binds.BindGlobalFunctionForTarget("bool IsRelative(const FString& InPath)", &FPaths::IsRelative);
		Binds.BindGlobalFunctionForTarget("bool IsRestrictedPath(const FString& InPath)", &FPaths::IsRestrictedPath);
		Binds.BindGlobalFunctionForTarget("bool IsSamePath(const FString& PathA, const FString& PathB)", &FPaths::IsSamePath);
		Binds.BindGlobalFunctionForTarget("bool IsUnderDirectory(const FString& InPath, const FString& InDirectory)", &FPaths::IsUnderDirectory);
		Binds.BindGlobalFunctionForTarget("void NormalizeFilename(FString& InPath)", &FPaths::NormalizeFilename);
		Binds.BindGlobalFunctionForTarget("void NormalizeDirectoryName(FString& InPath)", &FPaths::NormalizeDirectoryName);
		Binds.BindGlobalFunctionForTarget("bool CollapseRelativeDirectories(FString& InPath)", &FPaths::CollapseRelativeDirectories);
		Binds.BindGlobalFunctionForTarget("void RemoveDuplicateSlashes(FString& InPath)", FUNCPR(void, FPaths::RemoveDuplicateSlashes, (FString&)));
		Binds.BindGlobalFunctionForTarget("void MakeStandardFilename(FString& InPath)", FPaths::MakeStandardFilename);
		Binds.BindGlobalFunctionForTarget("void MakePlatformFilename(FString& InPath)", &FPaths::MakePlatformFilename);
		Binds.BindGlobalFunctionForTarget("FString ConvertRelativePathToFull(const FString& InPath)", &FAngelscriptFPathsBinds::ConvertRelativePathToFull);
		Binds.BindGlobalFunctionForTarget("FString ConvertRelativePathToFull(const FString& BasePath, const FString& InPath)", &FAngelscriptFPathsBinds::ConvertRelativePathToFullFromBase);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FPaths(
	TEXT("FPaths"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFPaths);
