#include "Bind_WorldCollision.h"

#include "AngelscriptBinds.h"
#include "AngelscriptType.h"
#include "WorldCollision.h"

/**
 * World collision query and asynchronous result binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | enum EAsyncTraceType;                                                                      | Declares the result mode requested for an asynchronous collision query.                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EAsyncTraceType::Test;                                                                     | Requests only whether the query produced a qualifying result.                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EAsyncTraceType::Single;                                                                   | Requests one qualifying hit result.                                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EAsyncTraceType::Multi;                                                                    | Requests all qualifying hit results.                                                                                 |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | struct FTraceHandle;                                                                       | Declares the frame-scoped handle returned by asynchronous collision queries.                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTraceHandle Handle();                                                                     | Constructs an invalid trace handle.                                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTraceHandle Handle(uint64 InHandle);                                                      | Constructs a trace handle from its packed native value.                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Handle == Other;                                                                           | Compares packed trace-handle identity.                                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FTraceHandle.IsValid() const;                                                         | Returns whether the handle contains a non-zero trace identity.                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | uint64 FTraceHandle._Handle;                                                               | Exposes the packed native handle value.                                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | uint32 FTraceHandle._FrameNumber;                                                          | Exposes the frame number encoded in the handle.                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | uint32 FTraceHandle._Index;                                                                | Exposes the per-frame query index encoded in the handle.                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | struct FTraceDatum;                                                                        | Declares completed asynchronous line/sweep query data.                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTraceDatum Data();                                                                        | Constructs empty trace result data.                                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector FTraceDatum.Start;                                                                 | Stores the world-space query start in Unreal units.                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector FTraceDatum.End;                                                                   | Stores the world-space query end in Unreal units.                                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FQuat FTraceDatum.Rot;                                                                     | Stores the swept shape world-space orientation.                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | TArray<FHitResult> FTraceDatum.OutHits;                                                    | Stores the completed hit results.                                                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EAsyncTraceType FTraceDatum.TraceType;                                                     | Stores the requested asynchronous result mode.                                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ECollisionChannel FTraceDatum.TraceChannel;                                                | Stores the collision trace channel.                                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | uint32 FTraceDatum.UserData;                                                               | Stores caller-provided data copied through the async query.                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | struct FOverlapDatum;                                                                      | Declares completed asynchronous overlap query data.                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FOverlapDatum Data();                                                                      | Constructs empty overlap result data.                                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector FOverlapDatum.Pos;                                                                 | Stores the world-space overlap position in Unreal units.                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FQuat FOverlapDatum.Rot;                                                                   | Stores the overlap shape world-space orientation.                                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | TArray<FOverlapResult> FOverlapDatum.OutOverlaps;                                          | Stores the completed overlap results.                                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | ECollisionChannel FOverlapDatum.TraceChannel;                                              | Stores the collision trace channel.                                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | uint32 FOverlapDatum.UserData;                                                             | Stores caller-provided data copied through the async query.                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::LineTraceTestByChannel(const FVector& Start, const FVector& End,              | Tests whether the world-space line segment produces a qualifying collision.                                          |
 * |     ECollisionChannel TraceChannel, const FCollisionQueryParams& Params =                  | @param Start, End World-space endpoints in Unreal units.                                                             |
 * |     FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams&              | @param TraceChannel Channel used for response filtering.                                                             |
 * |     ResponseParam = FCollisionResponseParams::DefaultResponseParam);                       | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * |                                                                                            | @param ResponseParam Supplies per-channel collision responses.                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::LineTraceTestByObjectType(const FVector& Start, const FVector& End, const     | Tests whether the world-space line segment produces a qualifying collision.                                          |
 * |     FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionQueryParams& Params =  | @param Start, End World-space endpoints in Unreal units.                                                             |
 * |     FCollisionQueryParams::DefaultQueryParam);                                             | @param ObjectQueryParams Selects object channels included by the query.                                              |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::LineTraceTestByProfile(const FVector& Start, const FVector& End, FName        | Tests whether the world-space line segment produces a qualifying collision.                                          |
 * |     ProfileName, const FCollisionQueryParams& Params =                                     | @param Start, End World-space endpoints in Unreal units.                                                             |
 * |     FCollisionQueryParams::DefaultQueryParam);                                             | @param ProfileName Collision profile supplying channel responses.                                                    |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::LineTraceSingleByChannel(FHitResult& OutHit, const FVector& Start, const      | Finds one qualifying collision along the world-space line segment.                                                   |
 * |     FVector& End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params =    | @param OutHit Receives the selected hit result.                                                                      |
 * |     FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams&              | @param Start, End World-space endpoints in Unreal units.                                                             |
 * |     ResponseParam = FCollisionResponseParams::DefaultResponseParam);                       | @param TraceChannel Channel used for response filtering.                                                             |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * |                                                                                            | @param ResponseParam Supplies per-channel collision responses.                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::LineTraceSingleByObjectType(FHitResult& OutHit, const FVector& Start, const   | Finds one qualifying collision along the world-space line segment.                                                   |
 * |     FVector& End, const FCollisionObjectQueryParams& ObjectQueryParams, const              | @param OutHit Receives the selected hit result.                                                                      |
 * |     FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam);             | @param Start, End World-space endpoints in Unreal units.                                                             |
 * |                                                                                            | @param ObjectQueryParams Selects object channels included by the query.                                              |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::LineTraceSingleByProfile(FHitResult& OutHit, const FVector& Start, const      | Finds one qualifying collision along the world-space line segment.                                                   |
 * |     FVector& End, FName ProfileName, const FCollisionQueryParams& Params =                 | @param OutHit Receives the selected hit result.                                                                      |
 * |     FCollisionQueryParams::DefaultQueryParam);                                             | @param Start, End World-space endpoints in Unreal units.                                                             |
 * |                                                                                            | @param ProfileName Collision profile supplying channel responses.                                                    |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::LineTraceMultiByChannel(TArray<FHitResult>& OutHits, const FVector& Start,    | Collects qualifying collisions along the world-space line segment.                                                   |
 * |     const FVector& End, ECollisionChannel TraceChannel, const FCollisionQueryParams&       | @param OutHits Receives hit results in engine query order.                                                           |
 * |     Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams&     | @param Start, End World-space endpoints in Unreal units.                                                             |
 * |     ResponseParam = FCollisionResponseParams::DefaultResponseParam);                       | @param TraceChannel Channel used for response filtering.                                                             |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * |                                                                                            | @param ResponseParam Supplies per-channel collision responses.                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::LineTraceMultiByObjectType(TArray<FHitResult>& OutHits, const FVector& Start, | Collects qualifying collisions along the world-space line segment.                                                   |
 * |     const FVector& End, const FCollisionObjectQueryParams& ObjectQueryParams, const        | @param OutHits Receives hit results in engine query order.                                                           |
 * |     FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam);             | @param Start, End World-space endpoints in Unreal units.                                                             |
 * |                                                                                            | @param ObjectQueryParams Selects object channels included by the query.                                              |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::LineTraceMultiByProfile(TArray<FHitResult>& OutHits, const FVector& Start,    | Collects qualifying collisions along the world-space line segment.                                                   |
 * |     const FVector& End, FName ProfileName, const FCollisionQueryParams& Params =           | @param OutHits Receives hit results in engine query order.                                                           |
 * |     FCollisionQueryParams::DefaultQueryParam);                                             | @param Start, End World-space endpoints in Unreal units.                                                             |
 * |                                                                                            | @param ProfileName Collision profile supplying channel responses.                                                    |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::SweepTestByChannel(const FVector& Start, const FVector& End, const FQuat&     | Tests whether sweeping CollisionShape produces a qualifying collision.                                               |
 * |     Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const      | @param Start, End World-space endpoints in Unreal units.                                                             |
 * |     FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const        | @param Rot World-space shape or component orientation.                                                               |
 * |     FCollisionResponseParams& ResponseParam =                                              | @param TraceChannel Channel used for response filtering.                                                             |
 * |     FCollisionResponseParams::DefaultResponseParam);                                       | @param CollisionShape Shape dimensions used by the sweep or overlap.                                                 |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * |                                                                                            | @param ResponseParam Supplies per-channel collision responses.                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::SweepTestByObjectType(const FVector& Start, const FVector& End, const FQuat&  | Tests whether sweeping CollisionShape produces a qualifying collision.                                               |
 * |     Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape&      | @param Start, End World-space endpoints in Unreal units.                                                             |
 * |     CollisionShape, const FCollisionQueryParams& Params =                                  | @param Rot World-space shape or component orientation.                                                               |
 * |     FCollisionQueryParams::DefaultQueryParam);                                             | @param ObjectQueryParams Selects object channels included by the query.                                              |
 * |                                                                                            | @param CollisionShape Shape dimensions used by the sweep or overlap.                                                 |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::SweepTestByProfile(const FVector& Start, const FVector& End, const FQuat&     | Tests whether sweeping CollisionShape produces a qualifying collision.                                               |
 * |     Rot, FName ProfileName, const FCollisionShape& CollisionShape, const                   | @param Start, End World-space endpoints in Unreal units.                                                             |
 * |     FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam);             | @param Rot World-space shape or component orientation.                                                               |
 * |                                                                                            | @param ProfileName Collision profile supplying channel responses.                                                    |
 * |                                                                                            | @param CollisionShape Shape dimensions used by the sweep or overlap.                                                 |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::SweepSingleByChannel(FHitResult& OutHit, const FVector& Start, const FVector& | Finds one qualifying collision while sweeping CollisionShape.                                                        |
 * |     End, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape&          | @param OutHit Receives the selected hit result.                                                                      |
 * |     CollisionShape, const FCollisionQueryParams& Params =                                  | @param Start, End World-space endpoints in Unreal units.                                                             |
 * |     FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams&              | @param Rot World-space shape or component orientation.                                                               |
 * |     ResponseParam = FCollisionResponseParams::DefaultResponseParam);                       | @param TraceChannel Channel used for response filtering.                                                             |
 * |                                                                                            | @param CollisionShape Shape dimensions used by the sweep or overlap.                                                 |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * |                                                                                            | @param ResponseParam Supplies per-channel collision responses.                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::SweepSingleByObjectType(FHitResult& OutHit, const FVector& Start, const       | Finds one qualifying collision while sweeping CollisionShape.                                                        |
 * |     FVector& End, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams,  | @param OutHit Receives the selected hit result.                                                                      |
 * |     const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params =           | @param Start, End World-space endpoints in Unreal units.                                                             |
 * |     FCollisionQueryParams::DefaultQueryParam);                                             | @param Rot World-space shape or component orientation.                                                               |
 * |                                                                                            | @param ObjectQueryParams Selects object channels included by the query.                                              |
 * |                                                                                            | @param CollisionShape Shape dimensions used by the sweep or overlap.                                                 |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::SweepSingleByProfile(FHitResult& OutHit, const FVector& Start, const FVector& | Finds one qualifying collision while sweeping CollisionShape.                                                        |
 * |     End, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const | @param OutHit Receives the selected hit result.                                                                      |
 * |     FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam);             | @param Start, End World-space endpoints in Unreal units.                                                             |
 * |                                                                                            | @param Rot World-space shape or component orientation.                                                               |
 * |                                                                                            | @param ProfileName Collision profile supplying channel responses.                                                    |
 * |                                                                                            | @param CollisionShape Shape dimensions used by the sweep or overlap.                                                 |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::SweepMultiByChannel(TArray<FHitResult>& OutHits, const FVector& Start, const  | Collects qualifying collisions while sweeping CollisionShape.                                                        |
 * |     FVector& End, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& | @param OutHits Receives hit results in engine query order.                                                           |
 * |     CollisionShape, const FCollisionQueryParams& Params =                                  | @param Start, End World-space endpoints in Unreal units.                                                             |
 * |     FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams&              | @param Rot World-space shape or component orientation.                                                               |
 * |     ResponseParam = FCollisionResponseParams::DefaultResponseParam);                       | @param TraceChannel Channel used for response filtering.                                                             |
 * |                                                                                            | @param CollisionShape Shape dimensions used by the sweep or overlap.                                                 |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * |                                                                                            | @param ResponseParam Supplies per-channel collision responses.                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::SweepMultiByObjectType(TArray<FHitResult>& OutHits, const FVector& Start,     | Collects qualifying collisions while sweeping CollisionShape.                                                        |
 * |     const FVector& End, const FQuat& Rot, const FCollisionObjectQueryParams&               | @param OutHits Receives hit results in engine query order.                                                           |
 * |     ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& | @param Start, End World-space endpoints in Unreal units.                                                             |
 * |     Params = FCollisionQueryParams::DefaultQueryParam);                                    | @param Rot World-space shape or component orientation.                                                               |
 * |                                                                                            | @param ObjectQueryParams Selects object channels included by the query.                                              |
 * |                                                                                            | @param CollisionShape Shape dimensions used by the sweep or overlap.                                                 |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::SweepMultiByProfile(TArray<FHitResult>& OutHits, const FVector& Start, const  | Collects qualifying collisions while sweeping CollisionShape.                                                        |
 * |     FVector& End, const FQuat& Rot, FName ProfileName, const FCollisionShape&              | @param OutHits Receives hit results in engine query order.                                                           |
 * |     CollisionShape, const FCollisionQueryParams& Params =                                  | @param Start, End World-space endpoints in Unreal units.                                                             |
 * |     FCollisionQueryParams::DefaultQueryParam);                                             | @param Rot World-space shape or component orientation.                                                               |
 * |                                                                                            | @param ProfileName Collision profile supplying channel responses.                                                    |
 * |                                                                                            | @param CollisionShape Shape dimensions used by the sweep or overlap.                                                 |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::OverlapBlockingTestByChannel(const FVector& Pos, const FQuat& Rot,            | Tests whether CollisionShape has a blocking overlap at Pos.                                                          |
 * |     ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const           | @param Pos World-space query position in Unreal units.                                                               |
 * |     FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const        | @param Rot World-space shape or component orientation.                                                               |
 * |     FCollisionResponseParams& ResponseParam =                                              | @param TraceChannel Channel used for response filtering.                                                             |
 * |     FCollisionResponseParams::DefaultResponseParam);                                       | @param CollisionShape Shape dimensions used by the sweep or overlap.                                                 |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * |                                                                                            | @param ResponseParam Supplies per-channel collision responses.                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::OverlapAnyTestByChannel(const FVector& Pos, const FQuat& Rot,                 | Tests whether CollisionShape has any qualifying overlap at Pos.                                                      |
 * |     ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const           | @param Pos World-space query position in Unreal units.                                                               |
 * |     FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const        | @param Rot World-space shape or component orientation.                                                               |
 * |     FCollisionResponseParams& ResponseParam =                                              | @param TraceChannel Channel used for response filtering.                                                             |
 * |     FCollisionResponseParams::DefaultResponseParam);                                       | @param CollisionShape Shape dimensions used by the sweep or overlap.                                                 |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * |                                                                                            | @param ResponseParam Supplies per-channel collision responses.                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::OverlapAnyTestByObjectType(const FVector& Pos, const FQuat& Rot, const        | Tests whether CollisionShape has any qualifying overlap at Pos.                                                      |
 * |     FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, | @param Pos World-space query position in Unreal units.                                                               |
 * |     const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam);       | @param Rot World-space shape or component orientation.                                                               |
 * |                                                                                            | @param ObjectQueryParams Selects object channels included by the query.                                              |
 * |                                                                                            | @param CollisionShape Shape dimensions used by the sweep or overlap.                                                 |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::OverlapBlockingTestByProfile(const FVector& Pos, const FQuat& Rot, FName      | Tests whether CollisionShape has a blocking overlap at Pos.                                                          |
 * |     ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams&       | @param Pos World-space query position in Unreal units.                                                               |
 * |     Params = FCollisionQueryParams::DefaultQueryParam);                                    | @param Rot World-space shape or component orientation.                                                               |
 * |                                                                                            | @param ProfileName Collision profile supplying channel responses.                                                    |
 * |                                                                                            | @param CollisionShape Shape dimensions used by the sweep or overlap.                                                 |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::OverlapAnyTestByProfile(const FVector& Pos, const FQuat& Rot, FName           | Tests whether CollisionShape has any qualifying overlap at Pos.                                                      |
 * |     ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams&       | @param Pos World-space query position in Unreal units.                                                               |
 * |     Params = FCollisionQueryParams::DefaultQueryParam);                                    | @param Rot World-space shape or component orientation.                                                               |
 * |                                                                                            | @param ProfileName Collision profile supplying channel responses.                                                    |
 * |                                                                                            | @param CollisionShape Shape dimensions used by the sweep or overlap.                                                 |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::OverlapMultiByChannel(TArray<FOverlapResult>& OutOverlaps, const FVector&     | Collects qualifying overlaps for CollisionShape at Pos.                                                              |
 * |     Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape&          | @param OutOverlaps Receives matching overlap results.                                                                |
 * |     CollisionShape, const FCollisionQueryParams& Params =                                  | @param Pos World-space query position in Unreal units.                                                               |
 * |     FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams&              | @param Rot World-space shape or component orientation.                                                               |
 * |     ResponseParam = FCollisionResponseParams::DefaultResponseParam);                       | @param TraceChannel Channel used for response filtering.                                                             |
 * |                                                                                            | @param CollisionShape Shape dimensions used by the sweep or overlap.                                                 |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * |                                                                                            | @param ResponseParam Supplies per-channel collision responses.                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::OverlapMultiByObjectType(TArray<FOverlapResult>& OutOverlaps, const FVector&  | Collects qualifying overlaps for CollisionShape at Pos.                                                              |
 * |     Pos, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const     | @param OutOverlaps Receives matching overlap results.                                                                |
 * |     FCollisionShape& CollisionShape, const FCollisionQueryParams& Params =                 | @param Pos World-space query position in Unreal units.                                                               |
 * |     FCollisionQueryParams::DefaultQueryParam);                                             | @param Rot World-space shape or component orientation.                                                               |
 * |                                                                                            | @param ObjectQueryParams Selects object channels included by the query.                                              |
 * |                                                                                            | @param CollisionShape Shape dimensions used by the sweep or overlap.                                                 |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::OverlapMultiByProfile(TArray<FOverlapResult>& OutOverlaps, const FVector&     | Collects qualifying overlaps for CollisionShape at Pos.                                                              |
 * |     Pos, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const | @param OutOverlaps Receives matching overlap results.                                                                |
 * |     FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam);             | @param Pos World-space query position in Unreal units.                                                               |
 * |                                                                                            | @param Rot World-space shape or component orientation.                                                               |
 * |                                                                                            | @param ProfileName Collision profile supplying channel responses.                                                    |
 * |                                                                                            | @param CollisionShape Shape dimensions used by the sweep or overlap.                                                 |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::ComponentSweepMulti(TArray<FHitResult>& OutHits, UPrimitiveComponent          | Sweeps PrimComp and collects matching hit results.                                                                   |
 * |     PrimComp, const FVector& Start, const FVector& End, const FQuat& Rot, const            | @param OutHits Receives hit results in engine query order.                                                           |
 * |     FComponentQueryParams& Params);                                                        | @param Start, End World-space endpoints in Unreal units.                                                             |
 * |                                                                                            | @param Rot World-space shape or component orientation.                                                               |
 * |                                                                                            | @param PrimComp Primitive component whose collision geometry is queried.                                             |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::ComponentSweepMulti(TArray<FHitResult>& OutHits, UPrimitiveComponent          | Sweeps PrimComp and collects matching hit results.                                                                   |
 * |     PrimComp, const FVector& Start, const FVector& End, const FRotator& Rot, const         | @param OutHits Receives hit results in engine query order.                                                           |
 * |     FComponentQueryParams& Params);                                                        | @param Start, End World-space endpoints in Unreal units.                                                             |
 * |                                                                                            | @param Rot World-space shape or component orientation.                                                               |
 * |                                                                                            | @param PrimComp Primitive component whose collision geometry is queried.                                             |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::ComponentSweepMultiByChannel(TArray<FHitResult>& OutHits, UPrimitiveComponent | Sweeps PrimComp and collects channel-filtered hit results.                                                           |
 * |     PrimComp, const FVector& Start, const FVector& End, const FQuat& Rot,                  | @param OutHits Receives hit results in engine query order.                                                           |
 * |     ECollisionChannel TraceChannel, const FComponentQueryParams& Params);                  | @param Start, End World-space endpoints in Unreal units.                                                             |
 * |                                                                                            | @param Rot World-space shape or component orientation.                                                               |
 * |                                                                                            | @param PrimComp Primitive component whose collision geometry is queried.                                             |
 * |                                                                                            | @param TraceChannel Channel used for response filtering.                                                             |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::ComponentSweepMultiByChannel(TArray<FHitResult>& OutHits, UPrimitiveComponent | Sweeps PrimComp and collects channel-filtered hit results.                                                           |
 * |     PrimComp, const FVector& Start, const FVector& End, const FRotator& Rot,               | @param OutHits Receives hit results in engine query order.                                                           |
 * |     ECollisionChannel TraceChannel, const FComponentQueryParams& Params);                  | @param Start, End World-space endpoints in Unreal units.                                                             |
 * |                                                                                            | @param Rot World-space shape or component orientation.                                                               |
 * |                                                                                            | @param PrimComp Primitive component whose collision geometry is queried.                                             |
 * |                                                                                            | @param TraceChannel Channel used for response filtering.                                                             |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::ComponentOverlapMulti(TArray<FOverlapResult>& OutOverlaps, const              | Tests PrimComp at Pos and collects matching overlaps.                                                                |
 * |     UPrimitiveComponent PrimComp, const FVector& Pos, const FQuat& Rot, const              | @param OutOverlaps Receives matching overlap results.                                                                |
 * |     FComponentQueryParams& Params = FComponentQueryParams::DefaultComponentQueryParams,    | @param Pos World-space query position in Unreal units.                                                               |
 * |     const FCollisionObjectQueryParams& ObjectQueryParams =                                 | @param Rot World-space shape or component orientation.                                                               |
 * |     FCollisionObjectQueryParams::DefaultObjectQueryParam);                                 | @param PrimComp Primitive component whose collision geometry is queried.                                             |
 * |                                                                                            | @param ObjectQueryParams Selects object channels included by the query.                                              |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::ComponentOverlapMulti(TArray<FOverlapResult>& OutOverlaps, const              | Tests PrimComp at Pos and collects matching overlaps.                                                                |
 * |     UPrimitiveComponent PrimComp, const FVector& Pos, const FRotator& Rot, const           | @param OutOverlaps Receives matching overlap results.                                                                |
 * |     FComponentQueryParams& Params = FComponentQueryParams::DefaultComponentQueryParams,    | @param Pos World-space query position in Unreal units.                                                               |
 * |     const FCollisionObjectQueryParams& ObjectQueryParams =                                 | @param Rot World-space shape or component orientation.                                                               |
 * |     FCollisionObjectQueryParams::DefaultObjectQueryParam);                                 | @param PrimComp Primitive component whose collision geometry is queried.                                             |
 * |                                                                                            | @param ObjectQueryParams Selects object channels included by the query.                                              |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::ComponentOverlapMultiByChannel(TArray<FOverlapResult>& OutOverlaps, const     | Tests PrimComp at Pos and collects channel-filtered overlaps.                                                        |
 * |     UPrimitiveComponent PrimComp, const FVector& Pos, const FQuat& Rot, ECollisionChannel  | @param OutOverlaps Receives matching overlap results.                                                                |
 * |     TraceChannel, const FComponentQueryParams& Params =                                    | @param Pos World-space query position in Unreal units.                                                               |
 * |     FComponentQueryParams::DefaultComponentQueryParams, const FCollisionObjectQueryParams& | @param Rot World-space shape or component orientation.                                                               |
 * |     ObjectQueryParams = FCollisionObjectQueryParams::DefaultObjectQueryParam);             | @param PrimComp Primitive component whose collision geometry is queried.                                             |
 * |                                                                                            | @param TraceChannel Channel used for response filtering.                                                             |
 * |                                                                                            | @param ObjectQueryParams Selects object channels included by the query.                                              |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::ComponentOverlapMultiByChannel(TArray<FOverlapResult>& OutOverlaps, const     | Tests PrimComp at Pos and collects channel-filtered overlaps.                                                        |
 * |     UPrimitiveComponent PrimComp, const FVector& Pos, const FRotator& Rot,                 | @param OutOverlaps Receives matching overlap results.                                                                |
 * |     ECollisionChannel TraceChannel, const FComponentQueryParams& Params =                  | @param Pos World-space query position in Unreal units.                                                               |
 * |     FComponentQueryParams::DefaultComponentQueryParams, const FCollisionObjectQueryParams& | @param Rot World-space shape or component orientation.                                                               |
 * |     ObjectQueryParams = FCollisionObjectQueryParams::DefaultObjectQueryParam);             | @param PrimComp Primitive component whose collision geometry is queried.                                             |
 * |                                                                                            | @param TraceChannel Channel used for response filtering.                                                             |
 * |                                                                                            | @param ObjectQueryParams Selects object channels included by the query.                                              |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTraceHandle System::AsyncLineTraceByChannel(EAsyncTraceType InTraceType, const FVector&   | Queues an asynchronous line trace and returns its frame-scoped handle.                                               |
 * |     Start,const FVector& End, ECollisionChannel TraceChannel, const FCollisionQueryParams& | @param Start, End World-space endpoints in Unreal units.                                                             |
 * |     Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams&     | @param TraceChannel Channel used for response filtering.                                                             |
 * |     ResponseParam = FCollisionResponseParams::DefaultResponseParam, const                  | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * |     FScriptTraceDelegate& InDelegate = FScriptTraceDelegate(), uint32 UserData = 0);       | @param ResponseParam Supplies per-channel collision responses.                                                       |
 * |                                                                                            | @param InTraceType Selects test, single-hit, or multi-hit async results.                                             |
 * |                                                                                            | @param InDelegate Optional completion callback. @param UserData Copied to completed data.                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTraceHandle System::AsyncLineTraceByObjectType(EAsyncTraceType InTraceType, const         | Queues an asynchronous line trace and returns its frame-scoped handle.                                               |
 * |     FVector& Start,const FVector& End, const FCollisionObjectQueryParams&                  | @param Start, End World-space endpoints in Unreal units.                                                             |
 * |     ObjectQueryParams, const FCollisionQueryParams& Params =                               | @param ObjectQueryParams Selects object channels included by the query.                                              |
 * |     FCollisionQueryParams::DefaultQueryParam, const FScriptTraceDelegate& InDelegate =     | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * |     FScriptTraceDelegate(), uint32 UserData = 0 );                                         | @param InTraceType Selects test, single-hit, or multi-hit async results.                                             |
 * |                                                                                            | @param InDelegate Optional completion callback. @param UserData Copied to completed data.                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTraceHandle System::AsyncLineTraceByProfile(EAsyncTraceType InTraceType, const FVector&   | Queues an asynchronous line trace and returns its frame-scoped handle.                                               |
 * |     Start, const FVector& End, FName ProfileName, const FCollisionQueryParams& Params =    | @param Start, End World-space endpoints in Unreal units.                                                             |
 * |     FCollisionQueryParams::DefaultQueryParam, const FScriptTraceDelegate& InDelegate =     | @param ProfileName Collision profile supplying channel responses.                                                    |
 * |     FScriptTraceDelegate(), uint32 UserData = 0);                                          | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * |                                                                                            | @param InTraceType Selects test, single-hit, or multi-hit async results.                                             |
 * |                                                                                            | @param InDelegate Optional completion callback. @param UserData Copied to completed data.                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTraceHandle System::AsyncSweepByChannel(EAsyncTraceType InTraceType, const FVector&       | Queues an asynchronous shape sweep and returns its frame-scoped handle.                                              |
 * |     Start, const FVector& End, const FQuat& Rot, ECollisionChannel TraceChannel, const     | @param Start, End World-space endpoints in Unreal units.                                                             |
 * |     FCollisionShape& CollisionShape, const FCollisionQueryParams& Params =                 | @param Rot World-space shape or component orientation.                                                               |
 * |     FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams&              | @param TraceChannel Channel used for response filtering.                                                             |
 * |     ResponseParam = FCollisionResponseParams::DefaultResponseParam, const                  | @param CollisionShape Shape dimensions used by the sweep or overlap.                                                 |
 * |     FScriptTraceDelegate& InDelegate = FScriptTraceDelegate(), uint32 UserData = 0);       | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * |                                                                                            | @param ResponseParam Supplies per-channel collision responses.                                                       |
 * |                                                                                            | @param InTraceType Selects test, single-hit, or multi-hit async results.                                             |
 * |                                                                                            | @param InDelegate Optional completion callback. @param UserData Copied to completed data.                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTraceHandle System::AsyncSweepByObjectType(EAsyncTraceType InTraceType, const FVector&    | Queues an asynchronous shape sweep and returns its frame-scoped handle.                                              |
 * |     Start, const FVector& End, const FQuat& Rot, const FCollisionObjectQueryParams&        | @param Start, End World-space endpoints in Unreal units.                                                             |
 * |     ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& | @param Rot World-space shape or component orientation.                                                               |
 * |     Params = FCollisionQueryParams::DefaultQueryParam, const FScriptTraceDelegate&         | @param ObjectQueryParams Selects object channels included by the query.                                              |
 * |     InDelegate = FScriptTraceDelegate(), uint32 UserData = 0);                             | @param CollisionShape Shape dimensions used by the sweep or overlap.                                                 |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * |                                                                                            | @param InTraceType Selects test, single-hit, or multi-hit async results.                                             |
 * |                                                                                            | @param InDelegate Optional completion callback. @param UserData Copied to completed data.                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTraceHandle System::AsyncSweepByProfile(EAsyncTraceType InTraceType, const FVector&       | Queues an asynchronous shape sweep and returns its frame-scoped handle.                                              |
 * |     Start, const FVector& End, const FQuat& Rot, FName ProfileName, const FCollisionShape& | @param Start, End World-space endpoints in Unreal units.                                                             |
 * |     CollisionShape, const FCollisionQueryParams& Params =                                  | @param Rot World-space shape or component orientation.                                                               |
 * |     FCollisionQueryParams::DefaultQueryParam, const FScriptTraceDelegate& InDelegate =     | @param ProfileName Collision profile supplying channel responses.                                                    |
 * |     FScriptTraceDelegate(), uint32 UserData = 0);                                          | @param CollisionShape Shape dimensions used by the sweep or overlap.                                                 |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * |                                                                                            | @param InTraceType Selects test, single-hit, or multi-hit async results.                                             |
 * |                                                                                            | @param InDelegate Optional completion callback. @param UserData Copied to completed data.                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTraceHandle System::AsyncOverlapByChannel(const FVector& Pos, const FQuat& Rot,           | Queues an asynchronous overlap query and returns its frame-scoped handle.                                            |
 * |     ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const           | @param Pos World-space query position in Unreal units.                                                               |
 * |     FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const        | @param Rot World-space shape or component orientation.                                                               |
 * |     FCollisionResponseParams& ResponseParam =                                              | @param TraceChannel Channel used for response filtering.                                                             |
 * |     FCollisionResponseParams::DefaultResponseParam, const FScriptOverlapDelegate&          | @param CollisionShape Shape dimensions used by the sweep or overlap.                                                 |
 * |     InDelegate = FScriptOverlapDelegate(), uint32 UserData = 0);                           | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * |                                                                                            | @param ResponseParam Supplies per-channel collision responses.                                                       |
 * |                                                                                            | @param InDelegate Optional completion callback. @param UserData Copied to completed data.                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTraceHandle System::AsyncOverlapByObjectType(const FVector& Pos, const FQuat& Rot, const  | Queues an asynchronous overlap query and returns its frame-scoped handle.                                            |
 * |     FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, | @param Pos World-space query position in Unreal units.                                                               |
 * |     const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const  | @param Rot World-space shape or component orientation.                                                               |
 * |     FScriptOverlapDelegate& InDelegate = FScriptOverlapDelegate(), uint32 UserData = 0);   | @param ObjectQueryParams Selects object channels included by the query.                                              |
 * |                                                                                            | @param CollisionShape Shape dimensions used by the sweep or overlap.                                                 |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * |                                                                                            | @param InDelegate Optional completion callback. @param UserData Copied to completed data.                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTraceHandle System::AsyncOverlapByProfile(const FVector& Pos, const FQuat& Rot, FName     | Queues an asynchronous overlap query and returns its frame-scoped handle.                                            |
 * |     ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams&       | @param Pos World-space query position in Unreal units.                                                               |
 * |     Params = FCollisionQueryParams::DefaultQueryParam, const FScriptOverlapDelegate&       | @param Rot World-space shape or component orientation.                                                               |
 * |     InDelegate = FScriptOverlapDelegate(), uint32 UserData = 0);                           | @param ProfileName Collision profile supplying channel responses.                                                    |
 * |                                                                                            | @param CollisionShape Shape dimensions used by the sweep or overlap.                                                 |
 * |                                                                                            | @param Params Controls complexity, ignored objects, result fields, and filtering.                                    |
 * |                                                                                            | @param InDelegate Optional completion callback. @param UserData Copied to completed data.                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::QueryTraceData(const FTraceHandle& Handle, FTraceDatum& OutData);             | Copies completed line/sweep data for Handle when it is ready.                                                        |
 * |                                                                                            | @param OutData Receives completed data when the function returns true.                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::QueryOverlapData(const FTraceHandle& Handle, FOverlapDatum& OutData);         | Copies completed overlap data for Handle when it is ready.                                                           |
 * |                                                                                            | @param OutData Receives completed data when the function returns true.                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::IsTraceHandleValid(const FTraceHandle& Handle, bool bOverlapTrace);           | Returns whether Handle is valid for the current frame and query kind.                                                |
 * |                                                                                            | @param bOverlapTrace Selects overlap-handle validation when true.                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

namespace
{
	void BindAsyncTraceTypeDeclarations(FAngelscriptBinds& Binds)
	{
		auto TraceType = Binds.EnumForTarget("EAsyncTraceType");
		TraceType["Test"] = EAsyncTraceType::Test;
		TraceType["Single"] = EAsyncTraceType::Single;
		TraceType["Multi"] = EAsyncTraceType::Multi;
	}

	void BindTraceHandleTypeDeclarations(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Binds.ValueClassForTarget<FTraceHandle>("FTraceHandle", Flags);
	}

	void BindTraceHandleTypeInfrastructure(FAngelscriptBinds& Binds)
	{
		Binds.RegisterTypeForTarget(MakeShared<FTraceHandleType>());
	}

	void BindTraceHandle(FAngelscriptBinds& Binds)
	{
		auto TraceHandle = Binds.ExistingClassForTarget("FTraceHandle");
		TraceHandle.Constructor("void f()", &FAngelscriptWorldCollisionBinds::ConstructTraceHandle)
			.NativeConstructor("FTraceHandle", true);
		TraceHandle.Constructor("void f(uint64 InHandle)", &FAngelscriptWorldCollisionBinds::ConstructTraceHandleFromValue)
			.NativeConstructor("FTraceHandle", true);
		TraceHandle.Method("bool opEquals(const FTraceHandle& Other) const", METHODPR_TRIVIAL(bool, FTraceHandle, operator==, (const FTraceHandle&) const));
		TraceHandle.Method("bool IsValid() const", METHOD_TRIVIAL(FTraceHandle, IsValid));
		TraceHandle.Property("uint64 _Handle", &FTraceHandle::_Handle);
		TraceHandle.Property("uint32 _FrameNumber", (size_t)&(((FTraceHandle*)nullptr)->_Data.FrameNumber));
		TraceHandle.Property("uint32 _Index", (size_t)&(((FTraceHandle*)nullptr)->_Data.Index));
	}

	void BindTraceDatumTypeDeclarations(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Binds.ValueClassForTarget<FTraceDatum>("FTraceDatum", Flags);
	}

	void BindTraceDatumTypeInfrastructure(FAngelscriptBinds& Binds)
	{
		Binds.RegisterTypeForTarget(MakeShared<FTraceDatumType>());
	}

	void BindTraceDatum(FAngelscriptBinds& Binds)
	{
		auto TraceDatum = Binds.ExistingClassForTarget("FTraceDatum");
		TraceDatum.Constructor("void f()", &FAngelscriptWorldCollisionBinds::ConstructTraceDatum)
			.NativeConstructor("FTraceDatum", true);
		TraceDatum.Property("FVector Start", &FTraceDatum::Start);
		TraceDatum.Property("FVector End", &FTraceDatum::End);
		TraceDatum.Property("FQuat Rot", &FTraceDatum::Rot);
		TraceDatum.Property("TArray<FHitResult> OutHits", &FTraceDatum::OutHits);
		TraceDatum.Property("EAsyncTraceType TraceType", &FTraceDatum::TraceType);
		TraceDatum.Property("ECollisionChannel TraceChannel", &FOverlapDatum::TraceChannel);
		TraceDatum.Property("uint32 UserData", &FTraceDatum::UserData);
	}

	void BindOverlapDatumTypeDeclarations(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Binds.ValueClassForTarget<FOverlapDatum>("FOverlapDatum", Flags);
	}

	void BindOverlapDatumTypeInfrastructure(FAngelscriptBinds& Binds)
	{
		Binds.RegisterTypeForTarget(MakeShared<FOverlapDatumType>());
	}

	void BindOverlapDatum(FAngelscriptBinds& Binds)
	{
		auto OverlapDatum = Binds.ExistingClassForTarget("FOverlapDatum");
		OverlapDatum.Constructor("void f()", &FAngelscriptWorldCollisionBinds::ConstructOverlapDatum)
			.NativeConstructor("FOverlapDatum", true);
		OverlapDatum.Property("FVector Pos", &FOverlapDatum::Pos);
		OverlapDatum.Property("FQuat Rot", &FOverlapDatum::Rot);
		OverlapDatum.Property("TArray<FOverlapResult> OutOverlaps", &FOverlapDatum::OutOverlaps);
		OverlapDatum.Property("ECollisionChannel TraceChannel", &FOverlapDatum::TraceChannel);
		OverlapDatum.Property("uint32 UserData", &FOverlapDatum::UserData);
	}

	void BindSyncQueries(FAngelscriptBinds& Binds)
	{
		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "System");

		Binds.BindGlobalFunctionForTarget("bool LineTraceTestByChannel(const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam)", &FAngelscriptWorldCollisionBinds::LineTraceTestByChannel);
		Binds.BindGlobalFunctionForTarget("bool LineTraceTestByObjectType(const FVector& Start, const FVector& End, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam)", &FAngelscriptWorldCollisionBinds::LineTraceTestByObjectType);
		Binds.BindGlobalFunctionForTarget("bool LineTraceTestByProfile(const FVector& Start, const FVector& End, FName ProfileName, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam)", &FAngelscriptWorldCollisionBinds::LineTraceTestByProfile);
		Binds.BindGlobalFunctionForTarget("bool LineTraceSingleByChannel(FHitResult& OutHit, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam)", &FAngelscriptWorldCollisionBinds::LineTraceSingleByChannel);
		Binds.BindGlobalFunctionForTarget("bool LineTraceSingleByObjectType(FHitResult& OutHit, const FVector& Start, const FVector& End, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam)", &FAngelscriptWorldCollisionBinds::LineTraceSingleByObjectType);
		Binds.BindGlobalFunctionForTarget("bool LineTraceSingleByProfile(FHitResult& OutHit, const FVector& Start, const FVector& End, FName ProfileName, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam)", &FAngelscriptWorldCollisionBinds::LineTraceSingleByProfile);
		Binds.BindGlobalFunctionForTarget("bool LineTraceMultiByChannel(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam)", &FAngelscriptWorldCollisionBinds::LineTraceMultiByChannel);
		Binds.BindGlobalFunctionForTarget("bool LineTraceMultiByObjectType(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam)", &FAngelscriptWorldCollisionBinds::LineTraceMultiByObjectType);
		Binds.BindGlobalFunctionForTarget("bool LineTraceMultiByProfile(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, FName ProfileName, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam)", &FAngelscriptWorldCollisionBinds::LineTraceMultiByProfile);

		Binds.BindGlobalFunctionForTarget("bool SweepTestByChannel(const FVector& Start, const FVector& End, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam)", &FAngelscriptWorldCollisionBinds::SweepTestByChannel);
		Binds.BindGlobalFunctionForTarget("bool SweepTestByObjectType(const FVector& Start, const FVector& End, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam)", &FAngelscriptWorldCollisionBinds::SweepTestByObjectType);
		Binds.BindGlobalFunctionForTarget("bool SweepTestByProfile(const FVector& Start, const FVector& End, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam)", &FAngelscriptWorldCollisionBinds::SweepTestByProfile);
		Binds.BindGlobalFunctionForTarget("bool SweepSingleByChannel(FHitResult& OutHit, const FVector& Start, const FVector& End, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam)", &FAngelscriptWorldCollisionBinds::SweepSingleByChannel);
		Binds.BindGlobalFunctionForTarget("bool SweepSingleByObjectType(FHitResult& OutHit, const FVector& Start, const FVector& End, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam)", &FAngelscriptWorldCollisionBinds::SweepSingleByObjectType);
		Binds.BindGlobalFunctionForTarget("bool SweepSingleByProfile(FHitResult& OutHit, const FVector& Start, const FVector& End, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam)", &FAngelscriptWorldCollisionBinds::SweepSingleByProfile);
		Binds.BindGlobalFunctionForTarget("bool SweepMultiByChannel(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam)", &FAngelscriptWorldCollisionBinds::SweepMultiByChannel);
		Binds.BindGlobalFunctionForTarget("bool SweepMultiByObjectType(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam)", &FAngelscriptWorldCollisionBinds::SweepMultiByObjectType);
		Binds.BindGlobalFunctionForTarget("bool SweepMultiByProfile(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam)", &FAngelscriptWorldCollisionBinds::SweepMultiByProfile);

		Binds.BindGlobalFunctionForTarget("bool OverlapBlockingTestByChannel(const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam)", &FAngelscriptWorldCollisionBinds::OverlapBlockingTestByChannel);
		Binds.BindGlobalFunctionForTarget("bool OverlapAnyTestByChannel(const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam)", &FAngelscriptWorldCollisionBinds::OverlapAnyTestByChannel);
		Binds.BindGlobalFunctionForTarget("bool OverlapAnyTestByObjectType(const FVector& Pos, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam)", &FAngelscriptWorldCollisionBinds::OverlapAnyTestByObjectType);
		Binds.BindGlobalFunctionForTarget("bool OverlapBlockingTestByProfile(const FVector& Pos, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam)", &FAngelscriptWorldCollisionBinds::OverlapBlockingTestByProfile);
		Binds.BindGlobalFunctionForTarget("bool OverlapAnyTestByProfile(const FVector& Pos, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam)", &FAngelscriptWorldCollisionBinds::OverlapAnyTestByProfile);
		Binds.BindGlobalFunctionForTarget("bool OverlapMultiByChannel(TArray<FOverlapResult>& OutOverlaps, const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam)", &FAngelscriptWorldCollisionBinds::OverlapMultiByChannel);
		Binds.BindGlobalFunctionForTarget("bool OverlapMultiByObjectType(TArray<FOverlapResult>& OutOverlaps, const FVector& Pos, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam)", &FAngelscriptWorldCollisionBinds::OverlapMultiByObjectType);
		Binds.BindGlobalFunctionForTarget("bool OverlapMultiByProfile(TArray<FOverlapResult>& OutOverlaps, const FVector& Pos, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam)", &FAngelscriptWorldCollisionBinds::OverlapMultiByProfile);

		Binds.BindGlobalFunctionForTarget("bool ComponentSweepMulti(TArray<FHitResult>& OutHits, UPrimitiveComponent PrimComp, const FVector& Start, const FVector& End, const FQuat& Rot, const FComponentQueryParams& Params)", &FAngelscriptWorldCollisionBinds::ComponentSweepMultiQuat);
		Binds.BindGlobalFunctionForTarget("bool ComponentSweepMulti(TArray<FHitResult>& OutHits, UPrimitiveComponent PrimComp, const FVector& Start, const FVector& End, const FRotator& Rot, const FComponentQueryParams& Params)", &FAngelscriptWorldCollisionBinds::ComponentSweepMultiRotator);
		Binds.BindGlobalFunctionForTarget("bool ComponentSweepMultiByChannel(TArray<FHitResult>& OutHits, UPrimitiveComponent PrimComp, const FVector& Start, const FVector& End, const FQuat& Rot, ECollisionChannel TraceChannel, const FComponentQueryParams& Params)", &FAngelscriptWorldCollisionBinds::ComponentSweepMultiByChannelQuat);
		Binds.BindGlobalFunctionForTarget("bool ComponentSweepMultiByChannel(TArray<FHitResult>& OutHits, UPrimitiveComponent PrimComp, const FVector& Start, const FVector& End, const FRotator& Rot, ECollisionChannel TraceChannel, const FComponentQueryParams& Params)", &FAngelscriptWorldCollisionBinds::ComponentSweepMultiByChannelRotator);
		Binds.BindGlobalFunctionForTarget("bool ComponentOverlapMulti(TArray<FOverlapResult>& OutOverlaps, const UPrimitiveComponent PrimComp, const FVector& Pos, const FQuat& Rot, const FComponentQueryParams& Params = FComponentQueryParams::DefaultComponentQueryParams, const FCollisionObjectQueryParams& ObjectQueryParams = FCollisionObjectQueryParams::DefaultObjectQueryParam)", &FAngelscriptWorldCollisionBinds::ComponentOverlapMultiQuat);
		Binds.BindGlobalFunctionForTarget("bool ComponentOverlapMulti(TArray<FOverlapResult>& OutOverlaps, const UPrimitiveComponent PrimComp, const FVector& Pos, const FRotator& Rot, const FComponentQueryParams& Params = FComponentQueryParams::DefaultComponentQueryParams, const FCollisionObjectQueryParams& ObjectQueryParams = FCollisionObjectQueryParams::DefaultObjectQueryParam)", &FAngelscriptWorldCollisionBinds::ComponentOverlapMultiRotator);
		Binds.BindGlobalFunctionForTarget("bool ComponentOverlapMultiByChannel(TArray<FOverlapResult>& OutOverlaps, const UPrimitiveComponent PrimComp, const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FComponentQueryParams& Params = FComponentQueryParams::DefaultComponentQueryParams, const FCollisionObjectQueryParams& ObjectQueryParams = FCollisionObjectQueryParams::DefaultObjectQueryParam)", &FAngelscriptWorldCollisionBinds::ComponentOverlapMultiByChannelQuat);
		Binds.BindGlobalFunctionForTarget("bool ComponentOverlapMultiByChannel(TArray<FOverlapResult>& OutOverlaps, const UPrimitiveComponent PrimComp, const FVector& Pos, const FRotator& Rot, ECollisionChannel TraceChannel, const FComponentQueryParams& Params = FComponentQueryParams::DefaultComponentQueryParams, const FCollisionObjectQueryParams& ObjectQueryParams = FCollisionObjectQueryParams::DefaultObjectQueryParam)", &FAngelscriptWorldCollisionBinds::ComponentOverlapMultiByChannelRotator);
	}

	void BindAsyncQueries(FAngelscriptBinds& Binds)
	{
		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "System");

		Binds.BindGlobalFunctionForTarget("FTraceHandle AsyncLineTraceByChannel(EAsyncTraceType InTraceType, const FVector& Start,const FVector& End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam, const FScriptTraceDelegate& InDelegate = FScriptTraceDelegate(), uint32 UserData = 0)", &FAngelscriptWorldCollisionBinds::AsyncLineTraceByChannel);
		Binds.BindGlobalFunctionForTarget("FTraceHandle AsyncLineTraceByObjectType(EAsyncTraceType InTraceType, const FVector& Start,const FVector& End, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FScriptTraceDelegate& InDelegate = FScriptTraceDelegate(), uint32 UserData = 0 )", &FAngelscriptWorldCollisionBinds::AsyncLineTraceByObjectType);
		Binds.BindGlobalFunctionForTarget("FTraceHandle AsyncLineTraceByProfile(EAsyncTraceType InTraceType, const FVector& Start, const FVector& End, FName ProfileName, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FScriptTraceDelegate& InDelegate = FScriptTraceDelegate(), uint32 UserData = 0)", &FAngelscriptWorldCollisionBinds::AsyncLineTraceByProfile);
		Binds.BindGlobalFunctionForTarget("FTraceHandle AsyncSweepByChannel(EAsyncTraceType InTraceType, const FVector& Start, const FVector& End, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam, const FScriptTraceDelegate& InDelegate = FScriptTraceDelegate(), uint32 UserData = 0)", &FAngelscriptWorldCollisionBinds::AsyncSweepByChannel);
		Binds.BindGlobalFunctionForTarget("FTraceHandle AsyncSweepByObjectType(EAsyncTraceType InTraceType, const FVector& Start, const FVector& End, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FScriptTraceDelegate& InDelegate = FScriptTraceDelegate(), uint32 UserData = 0)", &FAngelscriptWorldCollisionBinds::AsyncSweepByObjectType);
		Binds.BindGlobalFunctionForTarget("FTraceHandle AsyncSweepByProfile(EAsyncTraceType InTraceType, const FVector& Start, const FVector& End, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FScriptTraceDelegate& InDelegate = FScriptTraceDelegate(), uint32 UserData = 0)", &FAngelscriptWorldCollisionBinds::AsyncSweepByProfile);
		Binds.BindGlobalFunctionForTarget("FTraceHandle AsyncOverlapByChannel(const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam, const FScriptOverlapDelegate& InDelegate = FScriptOverlapDelegate(), uint32 UserData = 0)", &FAngelscriptWorldCollisionBinds::AsyncOverlapByChannel);
		Binds.BindGlobalFunctionForTarget("FTraceHandle AsyncOverlapByObjectType(const FVector& Pos, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FScriptOverlapDelegate& InDelegate = FScriptOverlapDelegate(), uint32 UserData = 0)", &FAngelscriptWorldCollisionBinds::AsyncOverlapByObjectType);
		Binds.BindGlobalFunctionForTarget("FTraceHandle AsyncOverlapByProfile(const FVector& Pos, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FScriptOverlapDelegate& InDelegate = FScriptOverlapDelegate(), uint32 UserData = 0)", &FAngelscriptWorldCollisionBinds::AsyncOverlapByProfile);
		Binds.BindGlobalFunctionForTarget("bool QueryTraceData(const FTraceHandle& Handle, FTraceDatum& OutData)", &FAngelscriptWorldCollisionBinds::QueryTraceData);
		Binds.BindGlobalFunctionForTarget("bool QueryOverlapData(const FTraceHandle& Handle, FOverlapDatum& OutData)", &FAngelscriptWorldCollisionBinds::QueryOverlapData);
		Binds.BindGlobalFunctionForTarget("bool IsTraceHandleValid(const FTraceHandle& Handle, bool bOverlapTrace) no_discard", &FAngelscriptWorldCollisionBinds::IsTraceHandleValid);
	}



}

AS_FORCE_LINK const FAngelscriptBind Bind_WorldCollision_TypeDeclarations(
	TEXT("WorldCollision.TypeDeclarations"),
	EAngelscriptBindPhase::TypeDeclarations,
	[](FAngelscriptBinds& Binds)
	{
		BindAsyncTraceTypeDeclarations(Binds);
		BindTraceHandleTypeDeclarations(Binds);
		BindTraceDatumTypeDeclarations(Binds);
		BindOverlapDatumTypeDeclarations(Binds);
	});

AS_FORCE_LINK const FAngelscriptBind Bind_WorldCollision_TypeInfrastructure(
	TEXT("WorldCollision.TypeInfrastructure"),
	EAngelscriptBindPhase::TypeInfrastructure,
	[](FAngelscriptBinds& Binds)
	{
		BindTraceHandleTypeInfrastructure(Binds);
		BindTraceDatumTypeInfrastructure(Binds);
		BindOverlapDatumTypeInfrastructure(Binds);
	});

AS_FORCE_LINK const FAngelscriptBind Bind_WorldCollision_ManualBindings(
	TEXT("WorldCollision.ManualBindings"),
	EAngelscriptBindPhase::ManualBindings,
	[](FAngelscriptBinds& Binds)
	{
		BindTraceHandle(Binds);
		BindTraceDatum(Binds);
		BindOverlapDatum(Binds);
		BindSyncQueries(Binds);
		BindAsyncQueries(Binds);
	});
