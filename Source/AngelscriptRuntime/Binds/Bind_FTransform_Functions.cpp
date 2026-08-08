#include "Bind_FTransform.h"

void FAngelscriptFTransformBinds::ConstructDefault(FTransform* Address)
{
	new (Address) FTransform();
}

void FAngelscriptFTransformBinds::ConstructCopy(FTransform* Address, const FTransform& Other)
{
	new (Address) FTransform(Other);
}

void FAngelscriptFTransformBinds::ConstructFromTranslation(FTransform* Address, const FVector& Translation)
{
	new (Address) FTransform(Translation);
}

void FAngelscriptFTransformBinds::ConstructFromQuat(FTransform* Address, const FQuat& Rotation)
{
	new (Address) FTransform(Rotation);
}

void FAngelscriptFTransformBinds::ConstructFromRotator(FTransform* Address, const FRotator& Rotation)
{
	new (Address) FTransform(Rotation);
}

void FAngelscriptFTransformBinds::ConstructFromQuatTranslationScale(
	FTransform* Address,
	const FQuat& Rotation,
	const FVector& Translation,
	const FVector& Scale)
{
	new (Address) FTransform(Rotation, Translation, Scale);
}

void FAngelscriptFTransformBinds::ConstructFromRotatorTranslationScale(
	FTransform* Address,
	const FRotator& Rotation,
	const FVector& Translation,
	const FVector& Scale)
{
	new (Address) FTransform(Rotation, Translation, Scale);
}

void FAngelscriptFTransformBinds::ConstructFromAxes(
	FTransform* Address,
	const FVector& XAxis,
	const FVector& YAxis,
	const FVector& ZAxis,
	const FVector& Translation)
{
	new (Address) FTransform(XAxis, YAxis, ZAxis, Translation);
}

void FAngelscriptFTransformBinds::ConstructFromTransform3f(
	FTransform* Address,
	const FTransform3f& Transform)
{
	new (Address) FTransform(Transform);
}

void FAngelscriptFTransformBinds::AppendToString(void* Address, FString& OutString)
{
	OutString += static_cast<FTransform*>(Address)->ToString();
}
