#include "Bind_FVector2f_Functions.h"

void FAngelscriptFVector2fBinds::Construct(FVector2f* Address, const float X, const float Y)
{
	new (Address) FVector2f(X, Y);
}

void FAngelscriptFVector2fBinds::ConstructZero(FVector2f* Address)
{
	new (Address) FVector2f(0.f, 0.f);
}

void FAngelscriptFVector2fBinds::ConstructCopy(FVector2f* Address, const FVector2f& Other)
{
	new (Address) FVector2f(Other);
}

void FAngelscriptFVector2fBinds::ConstructFromVector3f(FVector2f* Address, const FVector3f& Other)
{
	new (Address) FVector2f(Other);
}

void FAngelscriptFVector2fBinds::ConstructFromVector2D(FVector2f* Address, const FVector2D& Other)
{
	new (Address) FVector2f(Other);
}

FVector2f FAngelscriptFVector2fBinds::GetClampedToMaxSize(FVector2f& Vector, const float MaxSize)
{
	if (MaxSize < KINDA_SMALL_NUMBER)
	{
		return FVector2f(0.f, 0.f);
	}

	const float VSq = Vector.SizeSquared();
	if (VSq > FMath::Square(MaxSize))
	{
		const float Scale = MaxSize * FMath::InvSqrt(VSq);
		return FVector2f(Vector.X * Scale, Vector.Y * Scale);
	}
	return Vector;
}

void FAngelscriptFVector2fBinds::AppendToString(void* Ptr, FString& Str)
{
	Str += static_cast<FVector2f*>(Ptr)->ToString();
}
