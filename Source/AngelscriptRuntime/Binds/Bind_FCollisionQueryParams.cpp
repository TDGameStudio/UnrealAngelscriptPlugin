#include "Bind_FCollisionQueryParams.h"

#include "Misc/DefaultValueHelper.h"

#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"

#include "CollisionQueryParams.h"

/**
 * Collision query, object filter, and response binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | enum EQueryMobilityType;                                                                   | Declares which component mobility classes a collision query may consider.                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EQueryMobilityType::Any;                                                                   | Allows static and movable components.                                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EQueryMobilityType::Static;                                                                | Restricts the query to static components.                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EQueryMobilityType::Dynamic;                                                               | Restricts the query to movable components.                                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | enum ECollisionObjectQueryInitType;                                                        | Declares preset object-channel bitfield initializers.                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ECollisionObjectQueryInitType::AllObjects;                                                 | Initializes the query for all object channels.                                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ECollisionObjectQueryInitType::AllStaticObjects;                                           | Initializes the query for static object channels.                                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ECollisionObjectQueryInitType::AllDynamicObjects;                                          | Initializes the query for dynamic object channels.                                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | struct FCollisionQueryParams;                                                              | Declares trace-query filtering and result options.                                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | struct FCollisionEnabledMask;                                                              | Declares a compact collision-enabled mode mask.                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | struct FComponentQueryParams;                                                              | Declares component-overlap query filtering and options.                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | struct FCollisionResponseParams;                                                           | Declares the per-channel response parameters used by scene queries.                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | struct FCollisionObjectQueryParams;                                                        | Declares the object-channel selection bitfield used by scene queries.                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FCollisionQueryParams Params();                                                            | Constructs default collision query parameters.                                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FCollisionQueryParams Params(const FCollisionQueryParams& Other);                          | Copy-constructs collision query parameters.                                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Params = Other;                                                                            | Assigns collision query parameters.                                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | const FCollisionQueryParams FCollisionQueryParams::DefaultQueryParam;                      | Exposes the engine default collision query parameters.                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FCollisionEnabledMask Mask();                                                              | Constructs an empty collision-enabled mode mask.                                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FCollisionEnabledMask Mask(ECollisionEnabled CollisionEnabled);                            | Constructs a mask containing CollisionEnabled.                                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int8 FCollisionEnabledMask.Bits;                                                           | Exposes the packed collision-enabled mode bits.                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FComponentQueryParams Params();                                                            | Constructs default component query parameters.                                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FComponentQueryParams Params(const FComponentQueryParams& Other);                          | Copy-constructs component query parameters.                                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Params = Other;                                                                            | Assigns component query parameters.                                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | const FComponentQueryParams FComponentQueryParams::DefaultComponentQueryParams;            | Exposes the engine default component query parameters.                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FCollisionResponseParams Params();                                                         | Constructs response parameters from the engine default response container.                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FCollisionResponseParams Params(ECollisionResponse DefaultResponse);                       | Constructs response parameters with one default response for every channel.                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FCollisionResponseParams Params(const FCollisionResponseContainer& ResponseContainer);     | Constructs response parameters from an explicit per-channel container.                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | const FCollisionResponseParams FCollisionResponseParams::DefaultResponseParam;             | Exposes the engine default collision response parameters.                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FCollisionObjectQueryParams Params();                                                      | Constructs an empty object-channel query.                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FCollisionObjectQueryParams Params(ECollisionChannel QueryChannel);                        | Constructs an object query containing QueryChannel.                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | const FCollisionObjectQueryParams FCollisionObjectQueryParams::DefaultObjectQueryParam;    | Exposes the engine default object-channel query.                                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FCollisionQueryParams Params(FName InTraceTag, bool bInTraceComplex, const AActor          | Constructs query parameters and optionally ignores one actor.                                                        |
 * |     InIgnoreActor);                                                                        | @param InTraceTag Diagnostic trace tag. @param bInTraceComplex Selects complex geometry.                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FName FCollisionQueryParams.TraceTag;                                                      | Exposes the diagnostic trace tag.                                                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FName FCollisionQueryParams.OwnerTag;                                                      | Exposes the diagnostic owner tag.                                                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FCollisionQueryParams.bTraceComplex;                                                  | Selects complex rather than simple collision geometry.                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FCollisionQueryParams.bFindInitialOverlaps;                                           | Requests overlaps already present at the trace start.                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FCollisionQueryParams.bReturnFaceIndex;                                               | Requests triangle face indices in hit results.                                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FCollisionQueryParams.bReturnPhysicalMaterial;                                        | Requests physical material handles in hit results.                                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FCollisionQueryParams.bIgnoreBlocks;                                                  | Filters blocking hits from results.                                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FCollisionQueryParams.bIgnoreTouches;                                                 | Filters touch/overlap hits from results.                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FCollisionQueryParams.bSkipNarrowPhase;                                               | Allows overlap queries to skip narrow-phase geometry tests.                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EQueryMobilityType FCollisionQueryParams.MobilityType;                                     | Restricts results by component mobility.                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | uint8 FCollisionQueryParams.IgnoreMask;                                                    | Exposes the extra collision mask filter bits.                                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | TArray<uint32> FCollisionQueryParams.GetIgnoredComponents() const;                         | Returns the stored ignored component unique IDs.                                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | TArray<uint32> FCollisionQueryParams.GetIgnoredActors() const;                             | Returns the stored ignored actor/source-object unique IDs.                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FCollisionQueryParams.ClearIgnoredComponents();                                       | Clears all ignored component IDs.                                                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FCollisionQueryParams.ClearIgnoredActors();                                           | Clears all ignored actor/source-object IDs.                                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FCollisionQueryParams.SetNumIgnoredComponents(int32 NewNum);                          | Sets the number of stored ignored component IDs.                                                                     |
 * |                                                                                            | @param NewNum New ignored-component array length.                                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FCollisionQueryParams.AddIgnoredActor(const AActor InIgnoreActor);                    | Adds an actor to the ignore filter when the handle is valid.                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FCollisionQueryParams.AddIgnoredActor(const uint32 InIgnoreActorID);                  | Adds an actor/source-object unique ID to the ignore filter.                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FCollisionQueryParams.AddIgnoredActors(const TArray<AActor>& InIgnoreActors);         | Adds the valid actor handles to the ignore filter.                                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FCollisionQueryParams.AddIgnoredActors(const TArray<const AActor>& InIgnoreActors);   | Adds the valid const actor handles to the ignore filter.                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FCollisionQueryParams.AddIgnoredComponent(const UPrimitiveComponent                   | Adds a primitive component to the ignore filter.                                                                     |
 * |     InIgnoreComponent);                                                                    |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FCollisionQueryParams.AddIgnoredComponents(const TArray<UPrimitiveComponent>&         | Adds the valid primitive components to the ignore filter.                                                            |
 * |     InIgnoreComponents);                                                                   |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FCollisionQueryParams.AddIgnoredComponent_LikelyDuplicatedRoot(const                  | Adds a likely duplicated root component using the engine duplicate-aware path.                                       |
 * |     UPrimitiveComponent InIgnoreComponent);                                                | @param InIgnoreComponent Candidate root component to ignore.                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FCollisionQueryParams.ToString() const;                                            | Returns the engine diagnostic representation of the query parameters.                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FComponentQueryParams Params(FName InTraceTag, const AActor InIgnoreActor,                 | Constructs component query parameters and optionally ignores one actor.                                              |
 * |     FCollisionEnabledMask CollisionEnabledMask);                                           | @param InTraceTag Diagnostic trace tag. @param CollisionEnabledMask Allowed collision modes.                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FName FComponentQueryParams.TraceTag;                                                      | Exposes the diagnostic trace tag.                                                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FName FComponentQueryParams.OwnerTag;                                                      | Exposes the diagnostic owner tag.                                                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FComponentQueryParams.bTraceComplex;                                                  | Selects complex rather than simple collision geometry.                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FComponentQueryParams.bFindInitialOverlaps;                                           | Requests overlaps already present at the trace start.                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FComponentQueryParams.bReturnFaceIndex;                                               | Requests triangle face indices in hit results.                                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FComponentQueryParams.bReturnPhysicalMaterial;                                        | Requests physical material handles in hit results.                                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FComponentQueryParams.bIgnoreBlocks;                                                  | Filters blocking hits from results.                                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FComponentQueryParams.bIgnoreTouches;                                                 | Filters touch/overlap hits from results.                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FComponentQueryParams.bSkipNarrowPhase;                                               | Allows overlap queries to skip narrow-phase geometry tests.                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EQueryMobilityType FComponentQueryParams.MobilityType;                                     | Restricts results by component mobility.                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | uint8 FComponentQueryParams.IgnoreMask;                                                    | Exposes the extra collision mask filter bits.                                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FCollisionEnabledMask FComponentQueryParams.ShapeCollisionMask;                            | Restricts results by collision-enabled state.                                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | TArray<uint32> FComponentQueryParams.GetIgnoredComponents() const;                         | Returns the stored ignored component unique IDs.                                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | TArray<uint32> FComponentQueryParams.GetIgnoredActors() const;                             | Returns the stored ignored actor/source-object unique IDs.                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FComponentQueryParams.ClearIgnoredComponents();                                       | Clears all ignored component IDs.                                                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FComponentQueryParams.ClearIgnoredActors();                                           | Clears all ignored actor/source-object IDs.                                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FComponentQueryParams.SetNumIgnoredComponents(int32 NewNum);                          | Sets the number of stored ignored component IDs.                                                                     |
 * |                                                                                            | @param NewNum New ignored-component array length.                                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FComponentQueryParams.AddIgnoredActor(const AActor InIgnoreActor);                    | Adds an actor to the ignore filter when the handle is valid.                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FComponentQueryParams.AddIgnoredActor(const uint32 InIgnoreActorID);                  | Adds an actor/source-object unique ID to the ignore filter.                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FComponentQueryParams.AddIgnoredActors(const TArray<AActor>& InIgnoreActors);         | Adds the valid actor handles to the ignore filter.                                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FComponentQueryParams.AddIgnoredActors(const TArray<const AActor>& InIgnoreActors);   | Adds the valid const actor handles to the ignore filter.                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FComponentQueryParams.AddIgnoredComponent(const UPrimitiveComponent                   | Adds a primitive component to the ignore filter.                                                                     |
 * |     InIgnoreComponent);                                                                    |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FComponentQueryParams.AddIgnoredComponents(const TArray<UPrimitiveComponent>&         | Adds the valid primitive components to the ignore filter.                                                            |
 * |     InIgnoreComponents);                                                                   |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FComponentQueryParams.AddIgnoredComponent_LikelyDuplicatedRoot(const                  | Adds a likely duplicated root component using the engine duplicate-aware path.                                       |
 * |     UPrimitiveComponent InIgnoreComponent);                                                | @param InIgnoreComponent Candidate root component to ignore.                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FComponentQueryParams.ToString() const;                                            | Returns the engine diagnostic representation of the query parameters.                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FCollisionObjectQueryParams Params(ECollisionObjectQueryInitType QueryType);               | Constructs an object query from a static/dynamic/all preset.                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FCollisionObjectQueryParams Params(int32 InObjectTypesToQuery);                            | Constructs an object query from packed object-channel bits.                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int32 FCollisionObjectQueryParams.ObjectTypesToQuery;                                      | Exposes the packed low object-channel query bits.                                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | uint8 FCollisionObjectQueryParams.IgnoreMask;                                              | Exposes the additional object-query mask filter bits.                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FCollisionObjectQueryParams.AddObjectTypesToQuery(ECollisionChannel QueryChannel);    | Adds one collision channel to the object-query bitfield.                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FCollisionObjectQueryParams.RemoveObjectTypesToQuery(ECollisionChannel QueryChannel); | Removes one collision channel from the object-query bitfield.                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int64 FCollisionObjectQueryParams.GetObjectTypesToQuery() const;                           | Returns the object-query bitfield.                                                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FCollisionObjectQueryParams.SetObjectTypesToQuery(int64 InObjectTypesToQuery);        | Replaces the object-query bitfield.                                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int64 FCollisionObjectQueryParams.GetQueryBitfield64() const;                              | Returns the complete 64-bit object-query representation.                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FCollisionObjectQueryParams.IsValid() const;                                          | Returns whether at least one valid object channel is selected.                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FCollisionObjectQueryParams.DoVerify() const;                                         | Runs the engine debug verification for the object-query bitfield.                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FCollisionObjectQueryParams::IsValidObjectQuery(ECollisionChannel QueryChannel);      | Returns whether QueryChannel can participate in an object query.                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ECollisionObjectQueryInitType                                                              | Maps an overlap filter option to the corresponding object-query initialization preset.                               |
 * |     FCollisionObjectQueryParams::GetCollisionChannelFromOverlapFilter(EOverlapFilterOption |                                                                                                                      |
 * |     Filter);                                                                               |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FCollisionResponseContainer Responses(ECollisionResponse DefaultResponse);                 | Constructs a per-channel container initialized to DefaultResponse.                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FCollisionResponseContainer.SetResponse(ECollisionChannel Channel, ECollisionResponse | Sets one channel response and reports whether the container changed.                                                 |
 * |     NewResponse);                                                                          |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FCollisionResponseContainer.SetAllChannels(ECollisionResponse NewResponse);           | Sets every channel response and reports whether the container changed.                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FCollisionResponseContainer.ReplaceChannels(ECollisionResponse OldResponse,           | Replaces matching channel responses and reports whether any changed.                                                 |
 * |     ECollisionResponse NewResponse);                                                       |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ECollisionResponse FCollisionResponseContainer.GetResponse(ECollisionChannel Channel)      | Returns the response configured for Channel.                                                                         |
 * |     const;                                                                                 |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Responses == Other;                                                                        | Compares every per-channel collision response.                                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | const FCollisionResponseContainer&                                                         | Returns the shared engine default response container.                                                                |
 * |     FCollisionResponseContainer::GetDefaultResponseContainer();                            |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FCollisionResponseContainer FCollisionResponseContainer::CreateMinContainer(const          | Returns the least blocking response from A and B for each channel.                                                   |
 * |     FCollisionResponseContainer& A, const FCollisionResponseContainer& B);                 |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

namespace
{


	void BindFCollisionQueryParamsEarly(FAngelscriptBinds& Binds)
	{
		auto CollisionQueryParams = Binds.ExistingClassForTarget("FCollisionQueryParams");
		CollisionQueryParams.Constructor("void f()", &FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionQueryParams)
			.NoDiscard()
			.NativeConstructor("FCollisionQueryParams", true);
		CollisionQueryParams.Constructor("void f(const FCollisionQueryParams& Other)", &FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionQueryParamsCopy)
			.NoDiscard()
			.NativeConstructor("FCollisionQueryParams", true);
		CollisionQueryParams.Method("FCollisionQueryParams& opAssign(const FCollisionQueryParams& Other)", METHODPR_TRIVIAL(FCollisionQueryParams&, FCollisionQueryParams, operator=, (const FCollisionQueryParams&)));

		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FCollisionQueryParams");
		Binds.BindGlobalVariableForTarget("const FCollisionQueryParams DefaultQueryParam", &FCollisionQueryParams::DefaultQueryParam);
	}

	void BindFCollisionEnabledMaskEarly(FAngelscriptBinds& Binds)
	{
		auto CollisionEnabledMask = Binds.ExistingClassForTarget("FCollisionEnabledMask");
		CollisionEnabledMask.Constructor("void f()", &FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionEnabledMask)
			.NoDiscard()
			.NativeConstructor("FCollisionEnabledMask", true);
		CollisionEnabledMask.Constructor("void f(ECollisionEnabled CollisionEnabled)", &FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionEnabledMaskFromCollisionEnabled)
			.NoDiscard()
			.NativeConstructor("FCollisionEnabledMask", true);
		CollisionEnabledMask.Property("int8 Bits", &FCollisionEnabledMask::Bits);
	}

	void BindFComponentQueryParamsEarly(FAngelscriptBinds& Binds)
	{
		auto ComponentQueryParams = Binds.ExistingClassForTarget("FComponentQueryParams");
		ComponentQueryParams.Constructor("void f()", &FAngelscriptFCollisionQueryParamsBinds::ConstructComponentQueryParams)
			.NoDiscard()
			.NativeConstructor("FComponentQueryParams", true);
		ComponentQueryParams.Constructor("void f(const FComponentQueryParams& Other)", &FAngelscriptFCollisionQueryParamsBinds::ConstructComponentQueryParamsCopy)
			.NoDiscard()
			.NativeConstructor("FComponentQueryParams", true);
		ComponentQueryParams.Method("FComponentQueryParams& opAssign(const FComponentQueryParams& Other)", METHODPR_TRIVIAL(FComponentQueryParams&, FComponentQueryParams, operator=, (const FComponentQueryParams&)));

		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FComponentQueryParams");
		Binds.BindGlobalVariableForTarget("const FComponentQueryParams DefaultComponentQueryParams", &FComponentQueryParams::DefaultComponentQueryParams);
	}

	void BindFCollisionResponseParamsEarly(FAngelscriptBinds& Binds)
	{
		auto CollisionResponseParams = Binds.ExistingClassForTarget("FCollisionResponseParams");
		CollisionResponseParams.Constructor("void f()", &FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionResponseParams)
			.NativeConstructor("FCollisionResponseParams", true);
		CollisionResponseParams.Constructor("void f(ECollisionResponse DefaultResponse)", &FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionResponseParamsFromDefaultResponse)
			.NativeConstructor("FCollisionResponseParams", true);

		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FCollisionResponseParams");
		Binds.BindGlobalVariableForTarget("const FCollisionResponseParams DefaultResponseParam", &FCollisionResponseParams::DefaultResponseParam);
	}

	void BindFCollisionObjectQueryParamsEarly(FAngelscriptBinds& Binds)
	{
		auto CollisionObjectQueryParams = Binds.ExistingClassForTarget("FCollisionObjectQueryParams");
		CollisionObjectQueryParams.Constructor("void f()", &FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionObjectQueryParams)
			.NativeConstructor("FCollisionObjectQueryParams", true);
		CollisionObjectQueryParams.Constructor("void f(ECollisionChannel QueryChannel)", &FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionObjectQueryParamsFromChannel)
			.NativeConstructor("FCollisionObjectQueryParams", true);

		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FCollisionObjectQueryParams");
		Binds.BindGlobalVariableForTarget("const FCollisionObjectQueryParams DefaultObjectQueryParam", &FCollisionObjectQueryParams::DefaultObjectQueryParam);
	}

	void BindFCollisionQueryParamsLate(FAngelscriptBinds& Binds)
	{
		auto CollisionQueryParams = Binds.ExistingClassForTarget("FCollisionQueryParams");
		CollisionQueryParams.Constructor("void f(FName InTraceTag, bool bInTraceComplex, const AActor InIgnoreActor)", &FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionQueryParamsFromTraceTag)
			.NoDiscard()
			.NativeConstructor("FCollisionQueryParams", true);
		CollisionQueryParams.Property("FName TraceTag", &FCollisionQueryParams::TraceTag);
		CollisionQueryParams.Property("FName OwnerTag", &FCollisionQueryParams::OwnerTag);
		CollisionQueryParams.Property("bool bTraceComplex", &FCollisionQueryParams::bTraceComplex);
		CollisionQueryParams.Property("bool bFindInitialOverlaps", &FCollisionQueryParams::bFindInitialOverlaps);
		CollisionQueryParams.Property("bool bReturnFaceIndex", &FCollisionQueryParams::bReturnFaceIndex);
		CollisionQueryParams.Property("bool bReturnPhysicalMaterial", &FCollisionQueryParams::bReturnPhysicalMaterial);
		CollisionQueryParams.Property("bool bIgnoreBlocks", &FCollisionQueryParams::bIgnoreBlocks);
		CollisionQueryParams.Property("bool bIgnoreTouches", &FCollisionQueryParams::bIgnoreTouches);
		CollisionQueryParams.Property("bool bSkipNarrowPhase", &FCollisionQueryParams::bSkipNarrowPhase);
		CollisionQueryParams.Property("EQueryMobilityType MobilityType", &FCollisionQueryParams::MobilityType);
		CollisionQueryParams.Property("uint8 IgnoreMask", &FCollisionQueryParams::IgnoreMask);
		CollisionQueryParams.Method("TArray<uint32> GetIgnoredComponents() const", &FAngelscriptFCollisionQueryParamsBinds::GetCollisionQueryParamsIgnoredComponents);
		CollisionQueryParams.Method("TArray<uint32> GetIgnoredActors() const", &FAngelscriptFCollisionQueryParamsBinds::GetCollisionQueryParamsIgnoredActors);
		CollisionQueryParams.Method("void ClearIgnoredComponents()", METHOD_TRIVIAL(FCollisionQueryParams, ClearIgnoredComponents));
		CollisionQueryParams.Method("void ClearIgnoredActors()", METHOD_TRIVIAL(FCollisionQueryParams, ClearIgnoredSourceObjects));
		CollisionQueryParams.Method("void SetNumIgnoredComponents(int32 NewNum)", METHOD_TRIVIAL(FCollisionQueryParams, SetNumIgnoredComponents));
		CollisionQueryParams.Method("void AddIgnoredActor(const AActor InIgnoreActor)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredActor, (const AActor*)));
		CollisionQueryParams.Method("void AddIgnoredActor(const uint32 InIgnoreActorID)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredActor, (const uint32)));
		CollisionQueryParams.Method("void AddIgnoredActors(const TArray<AActor>& InIgnoreActors)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredActors, (const TArray<AActor*>&)));
		CollisionQueryParams.Method("void AddIgnoredActors(const TArray<const AActor>& InIgnoreActors)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredActors, (const TArray<const AActor*>&)));
		CollisionQueryParams.Method("void AddIgnoredComponent(const UPrimitiveComponent InIgnoreComponent)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredComponent, (const UPrimitiveComponent*)));
		CollisionQueryParams.Method("void AddIgnoredComponents(const TArray<UPrimitiveComponent>& InIgnoreComponents)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredComponents, (const TArray<UPrimitiveComponent*>&)));
		CollisionQueryParams.Method("void AddIgnoredComponent_LikelyDuplicatedRoot(const UPrimitiveComponent InIgnoreComponent)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredComponent_LikelyDuplicatedRoot, (const UPrimitiveComponent*)));
		CollisionQueryParams.Method("FString ToString() const", METHOD_TRIVIAL(FCollisionQueryParams, ToString));
	}

	void BindFComponentQueryParamsLate(FAngelscriptBinds& Binds)
	{
		auto ComponentQueryParams = Binds.ExistingClassForTarget("FComponentQueryParams");
		ComponentQueryParams.Constructor("void f(FName InTraceTag, const AActor InIgnoreActor, FCollisionEnabledMask CollisionEnabledMask)", &FAngelscriptFCollisionQueryParamsBinds::ConstructComponentQueryParamsFromTraceTag)
			.NoDiscard()
			.NativeConstructor("FComponentQueryParams", true);
		ComponentQueryParams.Property("FName TraceTag", &FComponentQueryParams::TraceTag);
		ComponentQueryParams.Property("FName OwnerTag", &FComponentQueryParams::OwnerTag);
		ComponentQueryParams.Property("bool bTraceComplex", &FComponentQueryParams::bTraceComplex);
		ComponentQueryParams.Property("bool bFindInitialOverlaps", &FComponentQueryParams::bFindInitialOverlaps);
		ComponentQueryParams.Property("bool bReturnFaceIndex", &FComponentQueryParams::bReturnFaceIndex);
		ComponentQueryParams.Property("bool bReturnPhysicalMaterial", &FComponentQueryParams::bReturnPhysicalMaterial);
		ComponentQueryParams.Property("bool bIgnoreBlocks", &FComponentQueryParams::bIgnoreBlocks);
		ComponentQueryParams.Property("bool bIgnoreTouches", &FComponentQueryParams::bIgnoreTouches);
		ComponentQueryParams.Property("bool bSkipNarrowPhase", &FComponentQueryParams::bSkipNarrowPhase);
		ComponentQueryParams.Property("EQueryMobilityType MobilityType", &FComponentQueryParams::MobilityType);
		ComponentQueryParams.Property("uint8 IgnoreMask", &FComponentQueryParams::IgnoreMask);
		ComponentQueryParams.Property("FCollisionEnabledMask ShapeCollisionMask", &FComponentQueryParams::ShapeCollisionMask);
		ComponentQueryParams.Method("TArray<uint32> GetIgnoredComponents() const", &FAngelscriptFCollisionQueryParamsBinds::GetComponentQueryParamsIgnoredComponents);
		ComponentQueryParams.Method("TArray<uint32> GetIgnoredActors() const", &FAngelscriptFCollisionQueryParamsBinds::GetComponentQueryParamsIgnoredActors);
		ComponentQueryParams.Method("void ClearIgnoredComponents()", METHOD_TRIVIAL(FComponentQueryParams, ClearIgnoredComponents));
		ComponentQueryParams.Method("void ClearIgnoredActors()", METHOD_TRIVIAL(FComponentQueryParams, ClearIgnoredSourceObjects));
		ComponentQueryParams.Method("void SetNumIgnoredComponents(int32 NewNum)", METHOD_TRIVIAL(FComponentQueryParams, SetNumIgnoredComponents));
		ComponentQueryParams.Method("void AddIgnoredActor(const AActor InIgnoreActor)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredActor, (const AActor*)));
		ComponentQueryParams.Method("void AddIgnoredActor(const uint32 InIgnoreActorID)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredActor, (const uint32)));
		ComponentQueryParams.Method("void AddIgnoredActors(const TArray<AActor>& InIgnoreActors)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredActors, (const TArray<AActor*>&)));
		ComponentQueryParams.Method("void AddIgnoredActors(const TArray<const AActor>& InIgnoreActors)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredActors, (const TArray<const AActor*>&)));
		ComponentQueryParams.Method("void AddIgnoredComponent(const UPrimitiveComponent InIgnoreComponent)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredComponent, (const UPrimitiveComponent*)));
		ComponentQueryParams.Method("void AddIgnoredComponents(const TArray<UPrimitiveComponent>& InIgnoreComponents)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredComponents, (const TArray<UPrimitiveComponent*>&)));
		ComponentQueryParams.Method("void AddIgnoredComponent_LikelyDuplicatedRoot(const UPrimitiveComponent InIgnoreComponent)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredComponent_LikelyDuplicatedRoot, (const UPrimitiveComponent*)));
		ComponentQueryParams.Method("FString ToString() const", METHOD_TRIVIAL(FCollisionQueryParams, ToString));
	}

	void BindFCollisionObjectQueryParamsLate(FAngelscriptBinds& Binds)
	{
		auto CollisionObjectQueryParams = Binds.ExistingClassForTarget("FCollisionObjectQueryParams");
		CollisionObjectQueryParams.Constructor("void f(ECollisionObjectQueryInitType QueryType)", &FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionObjectQueryParamsFromInitType)
			.NativeConstructor("FCollisionObjectQueryParams", true);
		CollisionObjectQueryParams.Constructor("void f(int32 InObjectTypesToQuery)", &FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionObjectQueryParamsFromObjectTypes)
			.NativeConstructor("FCollisionObjectQueryParams", true);
		CollisionObjectQueryParams.Property("int32 ObjectTypesToQuery", &FCollisionObjectQueryParams::ObjectTypesToQuery);
		CollisionObjectQueryParams.Property("uint8 IgnoreMask", &FCollisionObjectQueryParams::IgnoreMask);
		CollisionObjectQueryParams.Method("void AddObjectTypesToQuery(ECollisionChannel QueryChannel)", METHOD_TRIVIAL(FCollisionObjectQueryParams, AddObjectTypesToQuery));
		CollisionObjectQueryParams.Method("void RemoveObjectTypesToQuery(ECollisionChannel QueryChannel)", METHOD_TRIVIAL(FCollisionObjectQueryParams, RemoveObjectTypesToQuery));
		CollisionObjectQueryParams.Method("int64 GetObjectTypesToQuery() const", METHOD_TRIVIAL(FCollisionObjectQueryParams, GetObjectTypesToQuery));
		CollisionObjectQueryParams.Method("void SetObjectTypesToQuery(int64 InObjectTypesToQuery)", METHOD_TRIVIAL(FCollisionObjectQueryParams, SetObjectTypesToQuery));
		CollisionObjectQueryParams.Method("int64 GetQueryBitfield64() const", METHOD_TRIVIAL(FCollisionObjectQueryParams, GetQueryBitfield64));
		CollisionObjectQueryParams.Method("bool IsValid() const", METHOD_TRIVIAL(FCollisionObjectQueryParams, IsValid));
		CollisionObjectQueryParams.Method("void DoVerify() const", METHOD_TRIVIAL(FCollisionObjectQueryParams, DoVerify));

		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FCollisionObjectQueryParams");
		Binds.BindGlobalFunctionForTarget("bool IsValidObjectQuery(ECollisionChannel QueryChannel) no_discard", FUNC_TRIVIAL(FCollisionObjectQueryParams::IsValidObjectQuery));
		Binds.BindGlobalFunctionForTarget("ECollisionObjectQueryInitType GetCollisionChannelFromOverlapFilter(EOverlapFilterOption Filter) no_discard", FUNC_TRIVIAL(FCollisionObjectQueryParams::GetCollisionChannelFromOverlapFilter));
	}

	void BindFCollisionResponseContainer(FAngelscriptBinds& Binds)
	{
		auto CollisionResponseContainer = Binds.ExistingClassForTarget("FCollisionResponseContainer");
		CollisionResponseContainer.Constructor("void f(ECollisionResponse DefaultResponse)", &FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionResponseContainer)
			.NativeConstructor("FCollisionResponseContainer", true);
		CollisionResponseContainer.Method("bool SetResponse(ECollisionChannel Channel, ECollisionResponse NewResponse)", METHOD_TRIVIAL(FCollisionResponseContainer, SetResponse));
		CollisionResponseContainer.Method("bool SetAllChannels(ECollisionResponse NewResponse)", METHOD_TRIVIAL(FCollisionResponseContainer, SetAllChannels));
		CollisionResponseContainer.Method("bool ReplaceChannels(ECollisionResponse OldResponse, ECollisionResponse NewResponse)", METHOD_TRIVIAL(FCollisionResponseContainer, ReplaceChannels));
		CollisionResponseContainer.Method("ECollisionResponse GetResponse(ECollisionChannel Channel) const", METHOD_TRIVIAL(FCollisionResponseContainer, GetResponse));
		CollisionResponseContainer.Method("bool opEquals(const FCollisionResponseContainer& Other) const", METHODPR_TRIVIAL(bool, FCollisionResponseContainer, operator==, (const FCollisionResponseContainer&) const));

		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FCollisionResponseContainer");
		Binds.BindGlobalFunctionForTarget("const FCollisionResponseContainer& GetDefaultResponseContainer()", FUNC_TRIVIAL(FCollisionResponseContainer::GetDefaultResponseContainer));
		Binds.BindGlobalFunctionForTarget("FCollisionResponseContainer CreateMinContainer(const FCollisionResponseContainer& A, const FCollisionResponseContainer& B)", FUNC_TRIVIAL(FCollisionResponseContainer::CreateMinContainer));
	}

	void BindFCollisionResponseParamsLate(FAngelscriptBinds& Binds)
	{
		auto CollisionResponseParams = Binds.ExistingClassForTarget("FCollisionResponseParams");
		CollisionResponseParams.Constructor("void f(const FCollisionResponseContainer& ResponseContainer)", &FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionResponseParamsFromContainer)
			.NativeConstructor("FCollisionResponseParams", true);
	}

}

AS_FORCE_LINK const FAngelscriptBind Bind_FCollisionQueryParams_TypeDeclarations(
	TEXT("FCollisionQueryParams.TypeDeclarations"),
	EAngelscriptBindPhase::TypeDeclarations,
	[](FAngelscriptBinds& Binds)
	{
		auto QueryMobilityType = Binds.EnumForTarget("EQueryMobilityType");
		QueryMobilityType["Any"] = EQueryMobilityType::Any;
		QueryMobilityType["Static"] = EQueryMobilityType::Static;
		QueryMobilityType["Dynamic"] = EQueryMobilityType::Dynamic;

		auto CollisionObjectQueryInitType = Binds.EnumForTarget("ECollisionObjectQueryInitType");
		CollisionObjectQueryInitType["AllObjects"] = FCollisionObjectQueryParams::InitType::AllObjects;
		CollisionObjectQueryInitType["AllStaticObjects"] = FCollisionObjectQueryParams::InitType::AllStaticObjects;
		CollisionObjectQueryInitType["AllDynamicObjects"] = FCollisionObjectQueryParams::InitType::AllDynamicObjects;

		FBindFlags QueryParamsFlags;
		Binds.ValueClassForTarget<FCollisionQueryParams>("FCollisionQueryParams", QueryParamsFlags);

		FBindFlags CollisionEnabledMaskFlags;
		CollisionEnabledMaskFlags.bPOD = true;
		Binds.ValueClassForTarget<FCollisionEnabledMask>("FCollisionEnabledMask", CollisionEnabledMaskFlags);

		FBindFlags ComponentQueryParamsFlags;
		Binds.ValueClassForTarget<FComponentQueryParams>("FComponentQueryParams", ComponentQueryParamsFlags);

		FBindFlags CollisionResponseParamsFlags;
		CollisionResponseParamsFlags.bPOD = true;
		Binds.ValueClassForTarget<FCollisionResponseParams>("FCollisionResponseParams", CollisionResponseParamsFlags);

		FBindFlags CollisionObjectQueryParamsFlags;
		CollisionObjectQueryParamsFlags.bPOD = true;
		Binds.ValueClassForTarget<FCollisionObjectQueryParams>("FCollisionObjectQueryParams", CollisionObjectQueryParamsFlags);
	});

AS_FORCE_LINK const FAngelscriptBind Bind_FCollisionQueryParams_TypeInfrastructure(
	TEXT("FCollisionQueryParams.TypeInfrastructure"),
	EAngelscriptBindPhase::TypeInfrastructure,
	[](FAngelscriptBinds& Binds)
	{
		Binds.RegisterTypeForTarget(MakeShared<FCollisionQueryParamsType>());
		Binds.RegisterTypeForTarget(MakeShared<FCollisionEnabledMaskType>());
		Binds.RegisterTypeForTarget(MakeShared<FComponentQueryParamsType>());
		Binds.RegisterTypeForTarget(MakeShared<FCollisionResponseParamsType>());
		Binds.RegisterTypeForTarget(MakeShared<FCollisionObjectQueryParamsType>());
	});

AS_FORCE_LINK const FAngelscriptBind Bind_FCollisionQueryParams_ManualBindings(
	TEXT("FCollisionQueryParams.ExplicitBindings"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		BindFCollisionQueryParamsEarly(Binds);
		BindFCollisionEnabledMaskEarly(Binds);
		BindFComponentQueryParamsEarly(Binds);
		BindFCollisionResponseParamsEarly(Binds);
		BindFCollisionObjectQueryParamsEarly(Binds);

		BindFCollisionQueryParamsLate(Binds);
		BindFComponentQueryParamsLate(Binds);
		BindFCollisionObjectQueryParamsLate(Binds);
		BindFCollisionResponseContainer(Binds);
		BindFCollisionResponseParamsLate(Binds);
	});
