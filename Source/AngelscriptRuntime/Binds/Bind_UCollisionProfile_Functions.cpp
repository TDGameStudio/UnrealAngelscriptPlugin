#include "Bind_UCollisionProfile.h"

#include "Engine/CollisionProfile.h"

ECollisionChannel FAngelscriptUCollisionProfileBinds::ConvertToCollisionChannel(bool bTraceType, int32 Index)
{
	return UCollisionProfile::Get()->ConvertToCollisionChannel(bTraceType, Index);
}

EObjectTypeQuery FAngelscriptUCollisionProfileBinds::ConvertToObjectType(ECollisionChannel CollisionChannel)
{
	return UCollisionProfile::Get()->ConvertToObjectType(CollisionChannel);
}

ETraceTypeQuery FAngelscriptUCollisionProfileBinds::ConvertToTraceType(ECollisionChannel CollisionChannel)
{
	return UCollisionProfile::Get()->ConvertToTraceType(CollisionChannel);
}
