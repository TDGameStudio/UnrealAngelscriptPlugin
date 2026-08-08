#pragma once

#include "CoreMinimal.h"

#include "Helper_StructType.h"

struct FGetIntVector2
{
	static UScriptStruct* Get();
};

struct FIntVector2Type : TAngelscriptCoreStructType<FIntVector2, FGetIntVector2, false>
{
	FString GetAngelscriptTypeName() const override;

	void ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override;

	bool NeedConstruct(const FAngelscriptTypeUsage& Usage) const override;

	bool NeedDestruct(const FAngelscriptTypeUsage& Usage) const override;

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
};

struct FAngelscriptFIntVector2Binds
{
	static void ConstructXY(FIntVector2* Address, int32 X, int32 Y);
	static void ConstructZero(FIntVector2* Address);
	static void ConstructScalar(FIntVector2* Address, int32 Scalar);
	static void ConstructCopy(FIntVector2* Address, const FIntVector2& Other);
	static void AppendToString(void* Ptr, FString& Str);
};
