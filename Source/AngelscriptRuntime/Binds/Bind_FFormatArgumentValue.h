#pragma once

#include "CoreMinimal.h"

#include "Helper_CppType.h"

struct FFormatArgumentValueType : TAngelscriptCppType<FFormatArgumentValue>
{
	FString GetAngelscriptTypeName() const override;

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
};

struct FAngelscriptFFormatArgumentValueBinds
{
	static void ConstructDefault(FFormatArgumentValue* Address);
	static void ConstructInt32(FFormatArgumentValue* Address, int32 Value);
	static void ConstructUInt32(FFormatArgumentValue* Address, uint32 Value);
	static void ConstructInt64(FFormatArgumentValue* Address, int64 Value);
	static void ConstructUInt64(FFormatArgumentValue* Address, uint64 Value);
	static void ConstructFloat(FFormatArgumentValue* Address, float Value);
	static void ConstructDouble(FFormatArgumentValue* Address, double Value);
	static void ConstructText(FFormatArgumentValue* Address, const FText& Value);
	static void ConstructGender(FFormatArgumentValue* Address, ETextGender Value);
};
