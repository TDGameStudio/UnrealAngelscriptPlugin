#include "Bind_FVector2D.h"

void FAngelscriptFVector2DBinds::Construct(FVector2D* Address, const double X, const double Y)
{
	new (Address) FVector2D(X, Y);
}

void FAngelscriptFVector2DBinds::ConstructZero(FVector2D* Address)
{
	new (Address) FVector2D(0.f, 0.f);
}

void FAngelscriptFVector2DBinds::ConstructCopy(FVector2D* Address, const FVector2D& Other)
{
	new (Address) FVector2D(Other);
}

void FAngelscriptFVector2DBinds::ConstructFromVector2f(FVector2D* Address, const FVector2f& Other)
{
	new (Address) FVector2D(Other);
}

FVector2D FAngelscriptFVector2DBinds::GetClampedToMaxSize(FVector2D& Vector, const double MaxSize)
{
	if (MaxSize < KINDA_SMALL_NUMBER)
	{
		return FVector2D(0.f, 0.f);
	}

	const double VSq = Vector.SizeSquared();
	if (VSq > FMath::Square(MaxSize))
	{
		const double Scale = MaxSize * FMath::InvSqrt(VSq);
		return FVector2D(Vector.X * Scale, Vector.Y * Scale);
	}
	return Vector;
}

void FAngelscriptFVector2DBinds::AppendToString(void* Ptr, FString& Str)
{
	Str += static_cast<FVector2D*>(Ptr)->ToString();
}
