#include "Bind_FFileHelper_Functions.h"

#include "HAL/FileManager.h"

bool FAngelscriptFFileHelperBinds::LoadFileToString(
	FString& Result,
	const FString& Filename,
	FFileHelper::EHashOptions HashOptions,
	uint32 ReadFlags)
{
	return FFileHelper::LoadFileToString(Result, *Filename, HashOptions, ReadFlags);
}

bool FAngelscriptFFileHelperBinds::SaveStringToFile(
	const FString& String,
	const FString& Filename,
	FFileHelper::EEncodingOptions EncodingOptions,
	uint32 WriteFlags)
{
	return FFileHelper::SaveStringToFile(String, *Filename, EncodingOptions, &IFileManager::Get(), WriteFlags);
}
