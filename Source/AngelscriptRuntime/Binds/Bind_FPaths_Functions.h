#pragma once

#include "CoreMinimal.h"

struct FAngelscriptFPathsBinds
{
	static FString CombinePaths(const FString& FirstPath, const FString& SecondPath);
	static FString GetExtension(const FString& Path, bool bIncludeDot);
	static FString GetCleanFilename(const FString& Path);
	static FString GetBaseFilename(const FString& Path, bool bRemovePath);
	static FString GetPath(const FString& Path);
	static FString GetPathLeaf(const FString& Path);
	static FString ConvertRelativePathToFull(const FString& Path);
	static FString ConvertRelativePathToFullFromBase(const FString& BasePath, const FString& Path);
};
