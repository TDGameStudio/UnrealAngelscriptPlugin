#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"

#include "Misc/Paths.h"

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FPaths(FAngelscriptBinds::EOrder::Late, []
{
	FAngelscriptBinds::FNamespace ns("FPaths");

	// global paths
	FAngelscriptBinds::BindGlobalFunction("FString RootDir()", &FPaths::RootDir);
	FAngelscriptBinds::BindGlobalFunction("FString LaunchDir()", &FPaths::LaunchDir);

	FAngelscriptBinds::BindGlobalFunction("FString CombinePaths(const FString& FirstPath, const FString& SecondPath)", [](const FString& FirstPath, const FString& SecondPath) { return FPaths::Combine(FirstPath, SecondPath); });

	FAngelscriptBinds::BindGlobalFunction("FString EngineDir()", &FPaths::EngineDir);
	FAngelscriptBinds::BindGlobalFunction("FString EngineContentDir()", &FPaths::EngineContentDir);
	FAngelscriptBinds::BindGlobalFunction("FString EngineConfigDir()", &FPaths::EngineConfigDir);
	FAngelscriptBinds::BindGlobalFunction("FString EngineEditorSettingsDir()", &FPaths::EngineEditorSettingsDir);
	FAngelscriptBinds::BindGlobalFunction("FString EngineIntermediateDir()", &FPaths::EngineIntermediateDir);
	FAngelscriptBinds::BindGlobalFunction("FString EngineSavedDir()", &FPaths::EngineSavedDir);

	FAngelscriptBinds::BindGlobalFunction("FString ProjectDir()", &FPaths::ProjectDir);
	FAngelscriptBinds::BindGlobalFunction("FString ProjectUserDir()", &FPaths::ProjectUserDir);
	FAngelscriptBinds::BindGlobalFunction("FString ProjectContentDir()", &FPaths::ProjectContentDir);
	FAngelscriptBinds::BindGlobalFunction("FString ProjectConfigDir()", &FPaths::ProjectConfigDir);
	FAngelscriptBinds::BindGlobalFunction("const FString& ProjectSavedDir()", &FPaths::ProjectSavedDir);
	FAngelscriptBinds::BindGlobalFunction("FString ProjectIntermediateDir()", &FPaths::ProjectIntermediateDir);

	FAngelscriptBinds::BindGlobalFunction("FString ScreenShotDir()", &FPaths::ScreenShotDir);
	FAngelscriptBinds::BindGlobalFunction("FString VideoCaptureDir()", &FPaths::VideoCaptureDir);

	FAngelscriptBinds::BindGlobalFunction("const FString& GetRelativePathToRoot()", &FPaths::GetRelativePathToRoot);

	// filename & path functions
	FAngelscriptBinds::BindGlobalFunction("FString GetExtension(const FString& InPath, bool bIncludeDot = false)",
		[](const FString& InPath, bool bIncludeDot) -> FString { return FPaths::GetExtension(InPath, bIncludeDot); });
	FAngelscriptBinds::BindGlobalFunction("FString GetCleanFilename(const FString& InPath)",
		[](const FString& InPath) -> FString { return FPaths::GetCleanFilename(InPath); });
	FAngelscriptBinds::BindGlobalFunction("FString GetBaseFilename(const FString& InPath, bool bRemovePath = true)",
		[](const FString& InPath, bool bRemovePath) -> FString { return FPaths::GetBaseFilename(InPath, bRemovePath); });
	FAngelscriptBinds::BindGlobalFunction("FString GetPath(const FString& InPath)",
		[](const FString& InPath) -> FString { return FPaths::GetPath(InPath); });
	FAngelscriptBinds::BindGlobalFunction("FString GetPathLeaf(const FString& InPath)",
		[](const FString& InPath) -> FString { return FPaths::GetPathLeaf(InPath); });
	FAngelscriptBinds::BindGlobalFunction("FString ChangeExtension(const FString& InPath, const FString& InNewExtension)", &FPaths::ChangeExtension);
	FAngelscriptBinds::BindGlobalFunction("FString SetExtension(const FString& InPath, const FString& InNewExtension)", &FPaths::SetExtension);
	FAngelscriptBinds::BindGlobalFunction("void Split(const FString& InPath, FString& PathPart, FString& FilenamePart, FString& ExtensionPart)", &FPaths::Split);

	FAngelscriptBinds::BindGlobalFunction("bool FileExists(const FString& InPath)", &FPaths::FileExists);
	FAngelscriptBinds::BindGlobalFunction("bool DirectoryExists(const FString& InPath)", &FPaths::DirectoryExists);
	FAngelscriptBinds::BindGlobalFunction("bool IsDrive(const FString& InPath)", &FPaths::IsDrive);
	FAngelscriptBinds::BindGlobalFunction("bool IsRelative(const FString& InPath)", &FPaths::IsRelative);
	FAngelscriptBinds::BindGlobalFunction("bool IsRestrictedPath(const FString& InPath)", &FPaths::IsRestrictedPath);
	FAngelscriptBinds::BindGlobalFunction("bool IsSamePath(const FString& PathA, const FString& PathB)", &FPaths::IsSamePath);
	FAngelscriptBinds::BindGlobalFunction("bool IsUnderDirectory(const FString& InPath, const FString& InDirectory)", &FPaths::IsUnderDirectory);

	FAngelscriptBinds::BindGlobalFunction("void NormalizeFilename(FString& InPath)", &FPaths::NormalizeFilename);
	FAngelscriptBinds::BindGlobalFunction("void NormalizeDirectoryName(FString& InPath)", &FPaths::NormalizeDirectoryName);
	FAngelscriptBinds::BindGlobalFunction("bool CollapseRelativeDirectories(FString& InPath)", &FPaths::CollapseRelativeDirectories);
	FAngelscriptBinds::BindGlobalFunction("void RemoveDuplicateSlashes(FString& InPath)", FUNCPR(void, FPaths::RemoveDuplicateSlashes, (FString&)));
	FAngelscriptBinds::BindGlobalFunction("void MakeStandardFilename(FString& InPath)", FPaths::MakeStandardFilename);
	FAngelscriptBinds::BindGlobalFunction("void MakePlatformFilename(FString& InPath)", &FPaths::MakePlatformFilename);
	FAngelscriptBinds::BindGlobalFunction("FString ConvertRelativePathToFull(const FString& InPath)",
		[](const FString& InPath) -> FString { return FPaths::ConvertRelativePathToFull(InPath); });
	FAngelscriptBinds::BindGlobalFunction("FString ConvertRelativePathToFull(const FString& BasePath, const FString& InPath)",
		[](const FString& BasePath, const FString& InPath) -> FString {return FPaths::ConvertRelativePathToFull(BasePath, InPath);});
});
