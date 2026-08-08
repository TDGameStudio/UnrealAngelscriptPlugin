#pragma once

#include "CoreMinimal.h"

#include "Helper_StructType.h"

struct FIntVector4Type : TAngelscriptCoreStructType<FIntVector4, TBaseStructure<FIntVector4>, false>
{
	FString GetAngelscriptTypeName() const override;

	void ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override;

	bool NeedConstruct(const FAngelscriptTypeUsage& Usage) const override;

	bool NeedDestruct(const FAngelscriptTypeUsage& Usage) const override;

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
};

struct FAngelscriptFIntVector4Binds
{
	static void ConstructXYZW(FIntVector4* Address, int32 X, int32 Y, int32 Z, int32 W);
	static void ConstructZero(FIntVector4* Address);
	static void ConstructScalar(FIntVector4* Address, int32 Scalar);
	static void ConstructCopy(FIntVector4* Address, const FIntVector4& Other);
	static FIntVector4 Negate(const FIntVector4* Vector);
	static void AppendToString(void* Ptr, FString& Str);
};
