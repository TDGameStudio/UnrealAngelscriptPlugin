#pragma once

#include "Misc/FileHelper.h"

struct FAngelscriptFFileHelperBinds
{
	static bool LoadFileToString(
		FString& Result,
		const FString& Filename,
		FFileHelper::EHashOptions HashOptions,
		uint32 ReadFlags);
	static bool SaveStringToFile(
		const FString& String,
		const FString& Filename,
		FFileHelper::EEncodingOptions EncodingOptions,
		uint32 WriteFlags);
};
