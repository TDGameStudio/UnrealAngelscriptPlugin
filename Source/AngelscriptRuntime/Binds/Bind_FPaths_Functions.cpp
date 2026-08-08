#include "Bind_FPaths_Functions.h"

#include "Misc/Paths.h"

FString FAngelscriptFPathsBinds::CombinePaths(const FString& FirstPath, const FString& SecondPath)
{
	return FPaths::Combine(FirstPath, SecondPath);
}

FString FAngelscriptFPathsBinds::GetExtension(const FString& Path, bool bIncludeDot)
{
	return FPaths::GetExtension(Path, bIncludeDot);
}

FString FAngelscriptFPathsBinds::GetCleanFilename(const FString& Path)
{
	return FPaths::GetCleanFilename(Path);
}

FString FAngelscriptFPathsBinds::GetBaseFilename(const FString& Path, bool bRemovePath)
{
	return FPaths::GetBaseFilename(Path, bRemovePath);
}

FString FAngelscriptFPathsBinds::GetPath(const FString& Path)
{
	return FPaths::GetPath(Path);
}

FString FAngelscriptFPathsBinds::GetPathLeaf(const FString& Path)
{
	return FPaths::GetPathLeaf(Path);
}

FString FAngelscriptFPathsBinds::ConvertRelativePathToFull(const FString& Path)
{
	return FPaths::ConvertRelativePathToFull(Path);
}

FString FAngelscriptFPathsBinds::ConvertRelativePathToFullFromBase(const FString& BasePath, const FString& Path)
{
	return FPaths::ConvertRelativePathToFull(BasePath, Path);
}
