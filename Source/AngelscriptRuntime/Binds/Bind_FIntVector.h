#pragma once

#include "CoreMinimal.h"

#include "Helper_StructType.h"

struct FIntVectorType : TAngelscriptBaseStructType<FIntVector>
{
	FString GetAngelscriptTypeName() const override;

	void ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override;

	bool NeedConstruct(const FAngelscriptTypeUsage& Usage) const override;

	bool NeedDestruct(const FAngelscriptTypeUsage& Usage) const override;

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
};

struct FAngelscriptFIntVectorBinds
{
	static void ConstructXYZ(FIntVector* Address, int32 X, int32 Y, int32 Z);
	static void ConstructZero(FIntVector* Address);
	static void ConstructScalar(FIntVector* Address, int32 Scalar);
	static void ConstructCopy(FIntVector* Address, const FIntVector& Other);
	static FIntVector Negate(const FIntVector* Vector);
	static void AppendToString(void* Ptr, FString& Str);
};
