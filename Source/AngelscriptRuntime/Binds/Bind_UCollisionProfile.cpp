#include "Engine/EngineTypes.h"

struct FAngelscriptUCollisionProfileBinds
{
	static ECollisionChannel ConvertToCollisionChannel(bool bTraceType, int32 Index);
	static EObjectTypeQuery ConvertToObjectType(ECollisionChannel CollisionChannel);
	static ETraceTypeQuery ConvertToTraceType(ECollisionChannel CollisionChannel);
};

#include "AngelscriptBinds.h"

/**
 * UCollisionProfile namespace binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ECollisionChannel UCollisionProfile::ConvertToCollisionChannel(bool TraceType, int32       | Converts a trace-type or object-type query index to its collision channel. @param TraceType Selects trace-query      |
 * | Index);                                                                                    | mapping when true and object-query mapping when false.                                                               |
 * |                                                                                            | @param Index Query index in the selected trace-type or object-type mapping.                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EObjectTypeQuery UCollisionProfile::ConvertToObjectType(ECollisionChannel                  | Converts a collision channel to its object-query value.                                                              |
 * | CollisionChannel);                                                                         |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ETraceTypeQuery UCollisionProfile::ConvertToTraceType(ECollisionChannel CollisionChannel); | Converts a collision channel to its trace-query value.                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_UCollisionProfile(
	TEXT("UCollisionProfile"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "UCollisionProfile");
		Binds.BindGlobalFunctionForTarget(
			"ECollisionChannel ConvertToCollisionChannel(bool TraceType, int32 Index)",
			&FAngelscriptUCollisionProfileBinds::ConvertToCollisionChannel);
		Binds.BindGlobalFunctionForTarget(
			"EObjectTypeQuery ConvertToObjectType(ECollisionChannel CollisionChannel)",
			&FAngelscriptUCollisionProfileBinds::ConvertToObjectType);
		Binds.BindGlobalFunctionForTarget(
			"ETraceTypeQuery ConvertToTraceType(ECollisionChannel CollisionChannel)",
			&FAngelscriptUCollisionProfileBinds::ConvertToTraceType);
	});

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
