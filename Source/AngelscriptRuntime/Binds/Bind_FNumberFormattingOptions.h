#pragma once

#include "Helper_CppType.h"
#include "Internationalization/Text.h"

struct FNumberFormattingOptionsType : TAngelscriptCppType<FNumberFormattingOptions>
{
	FString GetAngelscriptTypeName() const override;
	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
};

struct FAngelscriptFNumberFormattingOptionsBinds
{
	static void Construct(FNumberFormattingOptions* Address);
	static uint32 Hash(const FNumberFormattingOptions& Options);
};
