#pragma once

#include "CoreMinimal.h"

#include "Helper_StructType.h"

struct FTransformType : TAngelscriptBaseStructType<FTransform>
{
	FString GetAngelscriptTypeName() const override;

	bool CanCompare(const FAngelscriptTypeUsage& Usage) const override;

	bool IsValueEqual(const FAngelscriptTypeUsage& Usage, void* SourcePtr, void* DestinationPtr) const override;

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
};

struct FAngelscriptFTransformBinds
{
	static void ConstructDefault(FTransform* Address);
	static void ConstructCopy(FTransform* Address, const FTransform& Other);
	static void ConstructFromTranslation(FTransform* Address, const FVector& Translation);
	static void ConstructFromQuat(FTransform* Address, const FQuat& Rotation);
	static void ConstructFromRotator(FTransform* Address, const FRotator& Rotation);
	static void ConstructFromQuatTranslationScale(
		FTransform* Address,
		const FQuat& Rotation,
		const FVector& Translation,
		const FVector& Scale);
	static void ConstructFromRotatorTranslationScale(
		FTransform* Address,
		const FRotator& Rotation,
		const FVector& Translation,
		const FVector& Scale);
	static void ConstructFromAxes(
		FTransform* Address,
		const FVector& XAxis,
		const FVector& YAxis,
		const FVector& ZAxis,
		const FVector& Translation);
	static void ConstructFromTransform3f(FTransform* Address, const FTransform3f& Transform);
	static void AppendToString(void* Address, FString& OutString);
};
