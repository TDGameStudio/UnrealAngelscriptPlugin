#include "Bind_UCollisionProfile.h"

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

namespace
{
	void BindUCollisionProfile(FAngelscriptBinds& Binds)
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
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_UCollisionProfile(
	TEXT("UCollisionProfile"),
	EAngelscriptBindPhase::ManualBindings,
	&BindUCollisionProfile);
