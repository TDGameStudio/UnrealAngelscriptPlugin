#include "Bind_FPaths.h"

#include "AngelscriptBinds.h"

#include "Misc/Paths.h"

/**
 * FPaths binding surface.
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                                                  | Purpose / parameter notes                                                                                            |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FPaths::RootDir();                                                                                                   | Returns the filesystem root containing the engine installation.                                                      |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FPaths::LaunchDir();                                                                                                 | Returns the process launch working directory.                                                                        |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FPaths::CombinePaths(const FString& FirstPath, const FString& SecondPath);                                           | Joins two path fragments with normalized separators.                                                                 |
 * |                                                                                                                              | @param FirstPath Base path fragment.                                                                                 |
 * |                                                                                                                              | @param SecondPath Fragment appended to FirstPath.                                                                    |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FPaths::EngineDir();                                                                                                 | Returns the engine installation directory.                                                                           |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FPaths::EngineContentDir();                                                                                          | Returns the engine Content directory.                                                                                |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FPaths::EngineConfigDir();                                                                                           | Returns the engine Config directory.                                                                                 |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FPaths::EngineEditorSettingsDir();                                                                                   | Returns the engine editor settings directory.                                                                        |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FPaths::EngineIntermediateDir();                                                                                     | Returns the engine Intermediate directory.                                                                           |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FPaths::EngineSavedDir();                                                                                            | Returns the engine Saved directory.                                                                                  |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FPaths::ProjectDir();                                                                                                | Returns the current project's root directory.                                                                        |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FPaths::ProjectUserDir();                                                                                            | Returns the current project's user-specific directory.                                                               |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FPaths::ProjectContentDir();                                                                                         | Returns the current project's Content directory.                                                                     |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FPaths::ProjectConfigDir();                                                                                          | Returns the current project's Config directory.                                                                      |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | const FString& FPaths::ProjectSavedDir();                                                                                    | Returns the current project's Saved directory.                                                                       |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FPaths::ProjectIntermediateDir();                                                                                    | Returns the current project's Intermediate directory.                                                                |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FPaths::ScreenShotDir();                                                                                             | Returns the directory used for screenshots.                                                                          |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FPaths::VideoCaptureDir();                                                                                           | Returns the directory used for captured video.                                                                       |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | const FString& FPaths::GetRelativePathToRoot();                                                                              | Returns the relative path from the launch directory to the engine root.                                              |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FPaths::GetExtension(const FString& InPath, bool bIncludeDot = false);                                               | Returns the final filename extension.                                                                                |
 * |                                                                                                                              | @param bIncludeDot Includes the leading period when true.                                                            |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FPaths::GetCleanFilename(const FString& InPath);                                                                     | Returns the filename and extension without directory components.                                                     |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FPaths::GetBaseFilename(const FString& InPath, bool bRemovePath = true);                                             | Returns the filename without its final extension.                                                                    |
 * |                                                                                                                              | @param bRemovePath Removes directory components when true.                                                           |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FPaths::GetPath(const FString& InPath);                                                                              | Returns the directory portion without the clean filename.                                                            |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FPaths::GetPathLeaf(const FString& InPath);                                                                          | Returns the final directory or filename component.                                                                   |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FPaths::ChangeExtension(const FString& InPath, const FString& InNewExtension);                                       | Replaces the final extension of InPath.                                                                              |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FPaths::SetExtension(const FString& InPath, const FString& InNewExtension);                                          | Returns InPath with its final extension set to InNewExtension.                                                       |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FPaths::Split(const FString& InPath, FString& PathPart, FString& FilenamePart, FString& ExtensionPart);                 | Splits a path into directory, base filename, and extension outputs.                                                  |
 * |                                                                                                                              | @param PathPart Receives the directory portion.                                                                      |
 * |                                                                                                                              | @param FilenamePart Receives the base filename without extension.                                                    |
 * |                                                                                                                              | @param ExtensionPart Receives the extension without its leading period.                                              |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FPaths::FileExists(const FString& InPath);                                                                              | Reports whether InPath names an existing file.                                                                       |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FPaths::DirectoryExists(const FString& InPath);                                                                         | Reports whether InPath names an existing directory.                                                                  |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FPaths::IsDrive(const FString& InPath);                                                                                 | Reports whether InPath represents a drive root.                                                                      |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FPaths::IsRelative(const FString& InPath);                                                                              | Reports whether InPath is relative rather than absolute.                                                             |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FPaths::IsRestrictedPath(const FString& InPath);                                                                        | Reports whether InPath lies beneath a configured restricted folder.                                                  |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FPaths::IsSamePath(const FString& PathA, const FString& PathB);                                                         | Compares two paths after platform-aware normalization.                                                               |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FPaths::IsUnderDirectory(const FString& InPath, const FString& InDirectory);                                            | Reports whether InPath is contained beneath InDirectory.                                                             |
 * |                                                                                                                              | @param InDirectory Candidate ancestor directory.                                                                     |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FPaths::NormalizeFilename(FString& InPath);                                                                             | Normalizes filename separators and removes redundant path syntax in place.                                           |
 * |                                                                                                                              | @param InPath Path string modified in place.                                                                         |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FPaths::NormalizeDirectoryName(FString& InPath);                                                                        | Normalizes a directory path and removes a trailing separator in place.                                               |
 * |                                                                                                                              | @param InPath Directory string modified in place.                                                                    |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FPaths::CollapseRelativeDirectories(FString& InPath);                                                                   | Collapses dot and parent-directory segments, returning false if traversal escapes the root.                          |
 * |                                                                                                                              | @param InPath Path string modified in place on success.                                                              |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FPaths::RemoveDuplicateSlashes(FString& InPath);                                                                        | Collapses repeated path separators in place.                                                                         |
 * |                                                                                                                              | @param InPath Path string modified in place.                                                                         |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FPaths::MakeStandardFilename(FString& InPath);                                                                          | Converts to the engine's standard relative filename form in place.                                                   |
 * |                                                                                                                              | @param InPath Filename modified in place.                                                                            |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FPaths::MakePlatformFilename(FString& InPath);                                                                          | Converts separators to the current platform's native filename form in place.                                         |
 * |                                                                                                                              | @param InPath Filename modified in place.                                                                            |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FPaths::ConvertRelativePathToFull(const FString& InPath);                                                            | Converts InPath to an absolute path using the process base directory.                                                |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FPaths::ConvertRelativePathToFull(const FString& BasePath, const FString& InPath);                                   | Converts InPath to an absolute path using BasePath.                                                                  |
 * |                                                                                                                              | @param BasePath Base directory used when InPath is relative.                                                         |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_FPaths(
	TEXT("FPaths"),
	EAngelscriptBindPhase::ManualBindings,
	[](FAngelscriptBinds& Binds)
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
	});
