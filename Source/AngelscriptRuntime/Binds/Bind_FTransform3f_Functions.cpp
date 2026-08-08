#include "Bind_FTransform3f.h"

void FAngelscriptFTransform3fBinds::ConstructDefault(FTransform3f* Address)
{
	new (Address) FTransform3f();
}

void FAngelscriptFTransform3fBinds::ConstructCopy(FTransform3f* Address, const FTransform3f& Other)
{
	new (Address) FTransform3f(Other);
}

void FAngelscriptFTransform3fBinds::ConstructFromTranslation(
	FTransform3f* Address,
	const FVector3f& Translation)
{
	new (Address) FTransform3f(Translation);
}

void FAngelscriptFTransform3fBinds::ConstructFromQuat(FTransform3f* Address, const FQuat4f& Rotation)
{
	new (Address) FTransform3f(Rotation);
}

void FAngelscriptFTransform3fBinds::ConstructFromRotator(
	FTransform3f* Address,
	const FRotator3f& Rotation)
{
	new (Address) FTransform3f(Rotation);
}

void FAngelscriptFTransform3fBinds::ConstructFromQuatTranslationScale(
	FTransform3f* Address,
	const FQuat4f& Rotation,
	const FVector3f& Translation,
	const FVector3f& Scale)
{
	new (Address) FTransform3f(Rotation, Translation, Scale);
}

void FAngelscriptFTransform3fBinds::ConstructFromRotatorTranslationScale(
	FTransform3f* Address,
	const FRotator3f& Rotation,
	const FVector3f& Translation,
	const FVector3f& Scale)
{
	new (Address) FTransform3f(Rotation, Translation, Scale);
}

void FAngelscriptFTransform3fBinds::ConstructFromAxes(
	FTransform3f* Address,
	const FVector3f& XAxis,
	const FVector3f& YAxis,
	const FVector3f& ZAxis,
	const FVector3f& Translation)
{
	new (Address) FTransform3f(XAxis, YAxis, ZAxis, Translation);
}

void FAngelscriptFTransform3fBinds::ConstructFromTransform(
	FTransform3f* Address,
	const FTransform& Transform)
{
	new (Address) FTransform3f(Transform);
}

void FAngelscriptFTransform3fBinds::AppendToString(void* Address, FString& OutString)
{
	OutString += static_cast<FTransform3f*>(Address)->ToString();
}
