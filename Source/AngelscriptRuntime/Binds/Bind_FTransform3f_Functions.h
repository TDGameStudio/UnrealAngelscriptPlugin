#pragma once

#include "CoreMinimal.h"

struct FAngelscriptFTransform3fBinds
{
	static void ConstructDefault(FTransform3f* Address);
	static void ConstructCopy(FTransform3f* Address, const FTransform3f& Other);
	static void ConstructFromTranslation(FTransform3f* Address, const FVector3f& Translation);
	static void ConstructFromQuat(FTransform3f* Address, const FQuat4f& Rotation);
	static void ConstructFromRotator(FTransform3f* Address, const FRotator3f& Rotation);
	static void ConstructFromQuatTranslationScale(
		FTransform3f* Address,
		const FQuat4f& Rotation,
		const FVector3f& Translation,
		const FVector3f& Scale);
	static void ConstructFromRotatorTranslationScale(
		FTransform3f* Address,
		const FRotator3f& Rotation,
		const FVector3f& Translation,
		const FVector3f& Scale);
	static void ConstructFromAxes(
		FTransform3f* Address,
		const FVector3f& XAxis,
		const FVector3f& YAxis,
		const FVector3f& ZAxis,
		const FVector3f& Translation);
	static void ConstructFromTransform(FTransform3f* Address, const FTransform& Transform);
	static void AppendToString(void* Address, FString& OutString);
};
