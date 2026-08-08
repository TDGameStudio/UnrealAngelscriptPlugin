#pragma once

#include "CoreMinimal.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Engine/OverlapResult.h"
#include "FunctionLibraries/WorldCollisionStatics.h"
#include "Helper_CppType.h"
#include "WorldCollision.h"

class UPrimitiveComponent;

struct FTraceHandleType : TAngelscriptCppType<FTraceHandle>
{
	FString GetAngelscriptTypeName() const override;
	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
	bool NeverRequiresGC(const FAngelscriptTypeUsage& Usage) const override;
};

struct FTraceDatumType : TAngelscriptCppType<FTraceDatum>
{
	FString GetAngelscriptTypeName() const override;
	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
	bool NeverRequiresGC(const FAngelscriptTypeUsage& Usage) const override;
};

struct FOverlapDatumType : TAngelscriptCppType<FOverlapDatum>
{
	FString GetAngelscriptTypeName() const override;
	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
	bool NeverRequiresGC(const FAngelscriptTypeUsage& Usage) const override;
};

struct FAngelscriptWorldCollisionBinds
{
	static void ConstructTraceHandle(FTraceHandle* Address);
	static void ConstructTraceHandleFromValue(FTraceHandle* Address, uint64 InHandle);
	static void ConstructTraceDatum(FTraceDatum* Address);
	static void ConstructOverlapDatum(FOverlapDatum* Address);

	static bool LineTraceTestByChannel(const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam);
	static bool LineTraceTestByObjectType(const FVector& Start, const FVector& End, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionQueryParams& Params);
	static bool LineTraceTestByProfile(const FVector& Start, const FVector& End, FName ProfileName, const FCollisionQueryParams& Params);
	static bool LineTraceSingleByChannel(FHitResult& OutHit, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam);
	static bool LineTraceSingleByObjectType(FHitResult& OutHit, const FVector& Start, const FVector& End, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionQueryParams& Params);
	static bool LineTraceSingleByProfile(FHitResult& OutHit, const FVector& Start, const FVector& End, FName ProfileName, const FCollisionQueryParams& Params);
	static bool LineTraceMultiByChannel(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam);
	static bool LineTraceMultiByObjectType(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionQueryParams& Params);
	static bool LineTraceMultiByProfile(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, FName ProfileName, const FCollisionQueryParams& Params);

	static bool SweepTestByChannel(const FVector& Start, const FVector& End, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam);
	static bool SweepTestByObjectType(const FVector& Start, const FVector& End, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params);
	static bool SweepTestByProfile(const FVector& Start, const FVector& End, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params);
	static bool SweepSingleByChannel(FHitResult& OutHit, const FVector& Start, const FVector& End, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam);
	static bool SweepSingleByObjectType(FHitResult& OutHit, const FVector& Start, const FVector& End, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params);
	static bool SweepSingleByProfile(FHitResult& OutHit, const FVector& Start, const FVector& End, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params);
	static bool SweepMultiByChannel(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam);
	static bool SweepMultiByObjectType(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params);
	static bool SweepMultiByProfile(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params);

	static bool OverlapBlockingTestByChannel(const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam);
	static bool OverlapAnyTestByChannel(const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam);
	static bool OverlapAnyTestByObjectType(const FVector& Pos, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params);
	static bool OverlapBlockingTestByProfile(const FVector& Pos, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params);
	static bool OverlapAnyTestByProfile(const FVector& Pos, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params);
	static bool OverlapMultiByChannel(TArray<FOverlapResult>& OutOverlaps, const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam);
	static bool OverlapMultiByObjectType(TArray<FOverlapResult>& OutOverlaps, const FVector& Pos, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params);
	static bool OverlapMultiByProfile(TArray<FOverlapResult>& OutOverlaps, const FVector& Pos, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params);

	static bool ComponentSweepMultiQuat(TArray<FHitResult>& OutHits, UPrimitiveComponent* PrimComp, const FVector& Start, const FVector& End, const FQuat& Rot, const FComponentQueryParams& Params);
	static bool ComponentSweepMultiRotator(TArray<FHitResult>& OutHits, UPrimitiveComponent* PrimComp, const FVector& Start, const FVector& End, const FRotator& Rot, const FComponentQueryParams& Params);
	static bool ComponentSweepMultiByChannelQuat(TArray<FHitResult>& OutHits, UPrimitiveComponent* PrimComp, const FVector& Start, const FVector& End, const FQuat& Rot, ECollisionChannel TraceChannel, const FComponentQueryParams& Params);
	static bool ComponentSweepMultiByChannelRotator(TArray<FHitResult>& OutHits, UPrimitiveComponent* PrimComp, const FVector& Start, const FVector& End, const FRotator& Rot, ECollisionChannel TraceChannel, const FComponentQueryParams& Params);
	static bool ComponentOverlapMultiQuat(TArray<FOverlapResult>& OutOverlaps, const UPrimitiveComponent* PrimComp, const FVector& Pos, const FQuat& Rot, const FComponentQueryParams& Params, const FCollisionObjectQueryParams& ObjectQueryParams);
	static bool ComponentOverlapMultiRotator(TArray<FOverlapResult>& OutOverlaps, const UPrimitiveComponent* PrimComp, const FVector& Pos, const FRotator& Rot, const FComponentQueryParams& Params, const FCollisionObjectQueryParams& ObjectQueryParams);
	static bool ComponentOverlapMultiByChannelQuat(TArray<FOverlapResult>& OutOverlaps, const UPrimitiveComponent* PrimComp, const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FComponentQueryParams& Params, const FCollisionObjectQueryParams& ObjectQueryParams);
	static bool ComponentOverlapMultiByChannelRotator(TArray<FOverlapResult>& OutOverlaps, const UPrimitiveComponent* PrimComp, const FVector& Pos, const FRotator& Rot, ECollisionChannel TraceChannel, const FComponentQueryParams& Params, const FCollisionObjectQueryParams& ObjectQueryParams);

	static FTraceHandle AsyncLineTraceByChannel(EAsyncTraceType InTraceType, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam, const FScriptTraceDelegate& InDelegate, uint32 UserData);
	static FTraceHandle AsyncLineTraceByObjectType(EAsyncTraceType InTraceType, const FVector& Start, const FVector& End, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionQueryParams& Params, const FScriptTraceDelegate& InDelegate, uint32 UserData);
	static FTraceHandle AsyncLineTraceByProfile(EAsyncTraceType InTraceType, const FVector& Start, const FVector& End, FName ProfileName, const FCollisionQueryParams& Params, const FScriptTraceDelegate& InDelegate, uint32 UserData);
	static FTraceHandle AsyncSweepByChannel(EAsyncTraceType InTraceType, const FVector& Start, const FVector& End, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam, const FScriptTraceDelegate& InDelegate, uint32 UserData);
	static FTraceHandle AsyncSweepByObjectType(EAsyncTraceType InTraceType, const FVector& Start, const FVector& End, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FScriptTraceDelegate& InDelegate, uint32 UserData);
	static FTraceHandle AsyncSweepByProfile(EAsyncTraceType InTraceType, const FVector& Start, const FVector& End, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FScriptTraceDelegate& InDelegate, uint32 UserData);
	static FTraceHandle AsyncOverlapByChannel(const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam, const FScriptOverlapDelegate& InDelegate, uint32 UserData);
	static FTraceHandle AsyncOverlapByObjectType(const FVector& Pos, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FScriptOverlapDelegate& InDelegate, uint32 UserData);
	static FTraceHandle AsyncOverlapByProfile(const FVector& Pos, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FScriptOverlapDelegate& InDelegate, uint32 UserData);
	static bool QueryTraceData(const FTraceHandle& Handle, FTraceDatum& OutData);
	static bool QueryOverlapData(const FTraceHandle& Handle, FOverlapDatum& OutData);
	static bool IsTraceHandleValid(const FTraceHandle& Handle, bool bOverlapTrace);
};
