#include "AngelscriptNativeInterfaceTestTypes.h"

ATestNativeParentInterfaceActor::ATestNativeParentInterfaceActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

int32 ATestNativeParentInterfaceActor::GetNativeValue_Implementation() const
{
	return NativeValue;
}

void ATestNativeParentInterfaceActor::SetNativeMarker_Implementation(FName Marker)
{
	NativeMarker = Marker;
}

void ATestNativeParentInterfaceActor::AdjustNativeValue_Implementation(int32 Delta, int32& Value)
{
	Value += Delta;
	LastAdjustmentDelta = Delta;
	LastAdjustedValue = Value;
}

ATestNativeMultiInterfaceActor::ATestNativeMultiInterfaceActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

int32 ATestNativeMultiInterfaceActor::GetNativeValue_Implementation() const
{
	return NativeValue;
}

void ATestNativeMultiInterfaceActor::SetNativeMarker_Implementation(FName Marker)
{
	NativeMarker = Marker;
}

void ATestNativeMultiInterfaceActor::AdjustNativeValue_Implementation(int32 Delta, int32& Value)
{
	Value += Delta;
	LastAdjustmentDelta = Delta;
	LastAdjustedValue = Value;
}

int32 ATestNativeMultiInterfaceActor::GetSecondaryValue_Implementation() const
{
	return SecondaryValue;
}

void ATestNativeMultiInterfaceActor::SetSecondaryLabel_Implementation(const FString& NewLabel)
{
	SecondaryLabel = NewLabel;
}
