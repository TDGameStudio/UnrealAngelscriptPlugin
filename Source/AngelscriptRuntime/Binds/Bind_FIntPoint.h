#pragma once

#include "CoreMinimal.h"

#include "Helper_StructType.h"

struct FIntPointType : TAngelscriptBaseStructType<FIntPoint>
{
	FString GetAngelscriptTypeName() const override;

	void ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override;

	bool NeedConstruct(const FAngelscriptTypeUsage& Usage) const override;

	bool NeedDestruct(const FAngelscriptTypeUsage& Usage) const override;

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
};

struct FAngelscriptFIntPointBinds
{
	static void ConstructXY(FIntPoint* Address, int32 X, int32 Y);
	static void ConstructZero(FIntPoint* Address);
	static void ConstructScalar(FIntPoint* Address, int32 Scalar);
	static void ConstructCopy(FIntPoint* Address, const FIntPoint& Other);
	static FIntPoint Negate(FIntPoint* Point);
	static void AppendToString(void* Address, FString& String);
};
