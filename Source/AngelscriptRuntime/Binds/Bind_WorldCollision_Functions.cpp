#include "Bind_WorldCollision.h"

#include "AngelscriptEngine.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace
{
	UWorld* GetWorldForCollisionBinding()
	{
		return GEngine->GetWorldFromContextObject(
			FAngelscriptEngine::TryGetCurrentWorldContextObject(),
			EGetWorldErrorMode::LogAndReturnNull);
	}

	FTraceDelegate MakeTraceDelegate(const FScriptTraceDelegate& ScriptDelegate)
	{
		if (!ScriptDelegate.IsBound())
		{
			return FTraceDelegate();
		}

		return FTraceDelegate::CreateWeakLambda(
			const_cast<UObject*>(ScriptDelegate.GetUObject()),
			[ScriptDelegate](const FTraceHandle& TraceHandle, FTraceDatum& TraceDatum)
			{
				ScriptDelegate.ExecuteIfBound(TraceHandle._Handle, TraceDatum.OutHits, TraceDatum.UserData);
			});
	}

	FOverlapDelegate MakeOverlapDelegate(const FScriptOverlapDelegate& ScriptDelegate)
	{
		if (!ScriptDelegate.IsBound())
		{
			return FOverlapDelegate();
		}

		return FOverlapDelegate::CreateWeakLambda(
			const_cast<UObject*>(ScriptDelegate.GetUObject()),
			[ScriptDelegate](const FTraceHandle& TraceHandle, FOverlapDatum& OverlapDatum)
			{
				ScriptDelegate.ExecuteIfBound(TraceHandle._Handle, OverlapDatum.OutOverlaps, OverlapDatum.UserData);
			});
	}
}

void FAngelscriptWorldCollisionBinds::ConstructTraceHandle(FTraceHandle* Address)
{
	new (Address) FTraceHandle();
}

void FAngelscriptWorldCollisionBinds::ConstructTraceHandleFromValue(FTraceHandle* Address, uint64 InHandle)
{
	FTraceHandle* TraceHandle = new (Address) FTraceHandle();
	TraceHandle->_Handle = InHandle;
}

void FAngelscriptWorldCollisionBinds::ConstructTraceDatum(FTraceDatum* Address)
{
	new (Address) FTraceDatum();
}

void FAngelscriptWorldCollisionBinds::ConstructOverlapDatum(FOverlapDatum* Address)
{
	new (Address) FOverlapDatum();
}

bool FAngelscriptWorldCollisionBinds::LineTraceTestByChannel(const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam)
{
	return GetWorldForCollisionBinding()->LineTraceTestByChannel(Start, End, TraceChannel, Params, ResponseParam);
}

bool FAngelscriptWorldCollisionBinds::LineTraceTestByObjectType(const FVector& Start, const FVector& End, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionQueryParams& Params)
{
	return GetWorldForCollisionBinding()->LineTraceTestByObjectType(Start, End, ObjectQueryParams, Params);
}

bool FAngelscriptWorldCollisionBinds::LineTraceTestByProfile(const FVector& Start, const FVector& End, FName ProfileName, const FCollisionQueryParams& Params)
{
	return GetWorldForCollisionBinding()->LineTraceTestByProfile(Start, End, ProfileName, Params);
}

bool FAngelscriptWorldCollisionBinds::LineTraceSingleByChannel(FHitResult& OutHit, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam)
{
	return GetWorldForCollisionBinding()->LineTraceSingleByChannel(OutHit, Start, End, TraceChannel, Params, ResponseParam);
}

bool FAngelscriptWorldCollisionBinds::LineTraceSingleByObjectType(FHitResult& OutHit, const FVector& Start, const FVector& End, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionQueryParams& Params)
{
	return GetWorldForCollisionBinding()->LineTraceSingleByObjectType(OutHit, Start, End, ObjectQueryParams, Params);
}

bool FAngelscriptWorldCollisionBinds::LineTraceSingleByProfile(FHitResult& OutHit, const FVector& Start, const FVector& End, FName ProfileName, const FCollisionQueryParams& Params)
{
	return GetWorldForCollisionBinding()->LineTraceSingleByProfile(OutHit, Start, End, ProfileName, Params);
}

bool FAngelscriptWorldCollisionBinds::LineTraceMultiByChannel(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam)
{
	return GetWorldForCollisionBinding()->LineTraceMultiByChannel(OutHits, Start, End, TraceChannel, Params, ResponseParam);
}

bool FAngelscriptWorldCollisionBinds::LineTraceMultiByObjectType(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionQueryParams& Params)
{
	return GetWorldForCollisionBinding()->LineTraceMultiByObjectType(OutHits, Start, End, ObjectQueryParams, Params);
}

bool FAngelscriptWorldCollisionBinds::LineTraceMultiByProfile(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, FName ProfileName, const FCollisionQueryParams& Params)
{
	return GetWorldForCollisionBinding()->LineTraceMultiByProfile(OutHits, Start, End, ProfileName, Params);
}

bool FAngelscriptWorldCollisionBinds::SweepTestByChannel(const FVector& Start, const FVector& End, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam)
{
	return GetWorldForCollisionBinding()->SweepTestByChannel(Start, End, Rot, TraceChannel, CollisionShape, Params, ResponseParam);
}

bool FAngelscriptWorldCollisionBinds::SweepTestByObjectType(const FVector& Start, const FVector& End, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params)
{
	return GetWorldForCollisionBinding()->SweepTestByObjectType(Start, End, Rot, ObjectQueryParams, CollisionShape, Params);
}

bool FAngelscriptWorldCollisionBinds::SweepTestByProfile(const FVector& Start, const FVector& End, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params)
{
	return GetWorldForCollisionBinding()->SweepTestByProfile(Start, End, Rot, ProfileName, CollisionShape, Params);
}

bool FAngelscriptWorldCollisionBinds::SweepSingleByChannel(FHitResult& OutHit, const FVector& Start, const FVector& End, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam)
{
	return GetWorldForCollisionBinding()->SweepSingleByChannel(OutHit, Start, End, Rot, TraceChannel, CollisionShape, Params, ResponseParam);
}

bool FAngelscriptWorldCollisionBinds::SweepSingleByObjectType(FHitResult& OutHit, const FVector& Start, const FVector& End, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params)
{
	return GetWorldForCollisionBinding()->SweepSingleByObjectType(OutHit, Start, End, Rot, ObjectQueryParams, CollisionShape, Params);
}

bool FAngelscriptWorldCollisionBinds::SweepSingleByProfile(FHitResult& OutHit, const FVector& Start, const FVector& End, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params)
{
	return GetWorldForCollisionBinding()->SweepSingleByProfile(OutHit, Start, End, Rot, ProfileName, CollisionShape, Params);
}

bool FAngelscriptWorldCollisionBinds::SweepMultiByChannel(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam)
{
	return GetWorldForCollisionBinding()->SweepMultiByChannel(OutHits, Start, End, Rot, TraceChannel, CollisionShape, Params, ResponseParam);
}

bool FAngelscriptWorldCollisionBinds::SweepMultiByObjectType(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params)
{
	return GetWorldForCollisionBinding()->SweepMultiByObjectType(OutHits, Start, End, Rot, ObjectQueryParams, CollisionShape, Params);
}

bool FAngelscriptWorldCollisionBinds::SweepMultiByProfile(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params)
{
	return GetWorldForCollisionBinding()->SweepMultiByProfile(OutHits, Start, End, Rot, ProfileName, CollisionShape, Params);
}

bool FAngelscriptWorldCollisionBinds::OverlapBlockingTestByChannel(const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam)
{
	return GetWorldForCollisionBinding()->OverlapBlockingTestByChannel(Pos, Rot, TraceChannel, CollisionShape, Params, ResponseParam);
}

bool FAngelscriptWorldCollisionBinds::OverlapAnyTestByChannel(const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam)
{
	return GetWorldForCollisionBinding()->OverlapAnyTestByChannel(Pos, Rot, TraceChannel, CollisionShape, Params, ResponseParam);
}

bool FAngelscriptWorldCollisionBinds::OverlapAnyTestByObjectType(const FVector& Pos, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params)
{
	return GetWorldForCollisionBinding()->OverlapAnyTestByObjectType(Pos, Rot, ObjectQueryParams, CollisionShape, Params);
}

bool FAngelscriptWorldCollisionBinds::OverlapBlockingTestByProfile(const FVector& Pos, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params)
{
	return GetWorldForCollisionBinding()->OverlapBlockingTestByProfile(Pos, Rot, ProfileName, CollisionShape, Params);
}

bool FAngelscriptWorldCollisionBinds::OverlapAnyTestByProfile(const FVector& Pos, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params)
{
	return GetWorldForCollisionBinding()->OverlapAnyTestByProfile(Pos, Rot, ProfileName, CollisionShape, Params);
}

bool FAngelscriptWorldCollisionBinds::OverlapMultiByChannel(TArray<FOverlapResult>& OutOverlaps, const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam)
{
	return GetWorldForCollisionBinding()->OverlapMultiByChannel(OutOverlaps, Pos, Rot, TraceChannel, CollisionShape, Params, ResponseParam);
}

bool FAngelscriptWorldCollisionBinds::OverlapMultiByObjectType(TArray<FOverlapResult>& OutOverlaps, const FVector& Pos, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params)
{
	return GetWorldForCollisionBinding()->OverlapMultiByObjectType(OutOverlaps, Pos, Rot, ObjectQueryParams, CollisionShape, Params);
}

bool FAngelscriptWorldCollisionBinds::OverlapMultiByProfile(TArray<FOverlapResult>& OutOverlaps, const FVector& Pos, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params)
{
	return GetWorldForCollisionBinding()->OverlapMultiByProfile(OutOverlaps, Pos, Rot, ProfileName, CollisionShape, Params);
}

bool FAngelscriptWorldCollisionBinds::ComponentSweepMultiQuat(TArray<FHitResult>& OutHits, UPrimitiveComponent* PrimComp, const FVector& Start, const FVector& End, const FQuat& Rot, const FComponentQueryParams& Params)
{
	if (PrimComp == nullptr)
	{
		OutHits.Reset();
		return false;
	}

	return GetWorldForCollisionBinding()->ComponentSweepMulti(OutHits, PrimComp, Start, End, Rot, Params);
}

bool FAngelscriptWorldCollisionBinds::ComponentSweepMultiRotator(TArray<FHitResult>& OutHits, UPrimitiveComponent* PrimComp, const FVector& Start, const FVector& End, const FRotator& Rot, const FComponentQueryParams& Params)
{
	if (PrimComp == nullptr)
	{
		OutHits.Reset();
		return false;
	}

	return GetWorldForCollisionBinding()->ComponentSweepMulti(OutHits, PrimComp, Start, End, Rot, Params);
}

bool FAngelscriptWorldCollisionBinds::ComponentSweepMultiByChannelQuat(TArray<FHitResult>& OutHits, UPrimitiveComponent* PrimComp, const FVector& Start, const FVector& End, const FQuat& Rot, ECollisionChannel TraceChannel, const FComponentQueryParams& Params)
{
	return GetWorldForCollisionBinding()->ComponentSweepMultiByChannel(OutHits, PrimComp, Start, End, Rot, TraceChannel, Params);
}

bool FAngelscriptWorldCollisionBinds::ComponentSweepMultiByChannelRotator(TArray<FHitResult>& OutHits, UPrimitiveComponent* PrimComp, const FVector& Start, const FVector& End, const FRotator& Rot, ECollisionChannel TraceChannel, const FComponentQueryParams& Params)
{
	return GetWorldForCollisionBinding()->ComponentSweepMultiByChannel(OutHits, PrimComp, Start, End, Rot, TraceChannel, Params);
}

bool FAngelscriptWorldCollisionBinds::ComponentOverlapMultiQuat(TArray<FOverlapResult>& OutOverlaps, const UPrimitiveComponent* PrimComp, const FVector& Pos, const FQuat& Rot, const FComponentQueryParams& Params, const FCollisionObjectQueryParams& ObjectQueryParams)
{
	if (PrimComp == nullptr)
	{
		OutOverlaps.Reset();
		return false;
	}

	return GetWorldForCollisionBinding()->ComponentOverlapMulti(OutOverlaps, PrimComp, Pos, Rot, Params, ObjectQueryParams);
}

bool FAngelscriptWorldCollisionBinds::ComponentOverlapMultiRotator(TArray<FOverlapResult>& OutOverlaps, const UPrimitiveComponent* PrimComp, const FVector& Pos, const FRotator& Rot, const FComponentQueryParams& Params, const FCollisionObjectQueryParams& ObjectQueryParams)
{
	if (PrimComp == nullptr)
	{
		OutOverlaps.Reset();
		return false;
	}

	return GetWorldForCollisionBinding()->ComponentOverlapMulti(OutOverlaps, PrimComp, Pos, Rot, Params, ObjectQueryParams);
}

bool FAngelscriptWorldCollisionBinds::ComponentOverlapMultiByChannelQuat(TArray<FOverlapResult>& OutOverlaps, const UPrimitiveComponent* PrimComp, const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FComponentQueryParams& Params, const FCollisionObjectQueryParams& ObjectQueryParams)
{
	return GetWorldForCollisionBinding()->ComponentOverlapMultiByChannel(OutOverlaps, PrimComp, Pos, Rot, TraceChannel, Params, ObjectQueryParams);
}

bool FAngelscriptWorldCollisionBinds::ComponentOverlapMultiByChannelRotator(TArray<FOverlapResult>& OutOverlaps, const UPrimitiveComponent* PrimComp, const FVector& Pos, const FRotator& Rot, ECollisionChannel TraceChannel, const FComponentQueryParams& Params, const FCollisionObjectQueryParams& ObjectQueryParams)
{
	return GetWorldForCollisionBinding()->ComponentOverlapMultiByChannel(OutOverlaps, PrimComp, Pos, Rot, TraceChannel, Params, ObjectQueryParams);
}

FTraceHandle FAngelscriptWorldCollisionBinds::AsyncLineTraceByChannel(EAsyncTraceType InTraceType, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam, const FScriptTraceDelegate& InDelegate, uint32 UserData)
{
	FTraceDelegate TraceDelegate = MakeTraceDelegate(InDelegate);
	return GetWorldForCollisionBinding()->AsyncLineTraceByChannel(InTraceType, Start, End, TraceChannel, Params, ResponseParam, TraceDelegate.IsBound() ? &TraceDelegate : nullptr, UserData);
}

FTraceHandle FAngelscriptWorldCollisionBinds::AsyncLineTraceByObjectType(EAsyncTraceType InTraceType, const FVector& Start, const FVector& End, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionQueryParams& Params, const FScriptTraceDelegate& InDelegate, uint32 UserData)
{
	FTraceDelegate TraceDelegate = MakeTraceDelegate(InDelegate);
	return GetWorldForCollisionBinding()->AsyncLineTraceByObjectType(InTraceType, Start, End, ObjectQueryParams, Params, TraceDelegate.IsBound() ? &TraceDelegate : nullptr, UserData);
}

FTraceHandle FAngelscriptWorldCollisionBinds::AsyncLineTraceByProfile(EAsyncTraceType InTraceType, const FVector& Start, const FVector& End, FName ProfileName, const FCollisionQueryParams& Params, const FScriptTraceDelegate& InDelegate, uint32 UserData)
{
	FTraceDelegate TraceDelegate = MakeTraceDelegate(InDelegate);
	return GetWorldForCollisionBinding()->AsyncLineTraceByProfile(InTraceType, Start, End, ProfileName, Params, TraceDelegate.IsBound() ? &TraceDelegate : nullptr, UserData);
}

FTraceHandle FAngelscriptWorldCollisionBinds::AsyncSweepByChannel(EAsyncTraceType InTraceType, const FVector& Start, const FVector& End, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam, const FScriptTraceDelegate& InDelegate, uint32 UserData)
{
	FTraceDelegate TraceDelegate = MakeTraceDelegate(InDelegate);
	return GetWorldForCollisionBinding()->AsyncSweepByChannel(InTraceType, Start, End, Rot, TraceChannel, CollisionShape, Params, ResponseParam, TraceDelegate.IsBound() ? &TraceDelegate : nullptr, UserData);
}

FTraceHandle FAngelscriptWorldCollisionBinds::AsyncSweepByObjectType(EAsyncTraceType InTraceType, const FVector& Start, const FVector& End, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FScriptTraceDelegate& InDelegate, uint32 UserData)
{
	FTraceDelegate TraceDelegate = MakeTraceDelegate(InDelegate);
	return GetWorldForCollisionBinding()->AsyncSweepByObjectType(InTraceType, Start, End, Rot, ObjectQueryParams, CollisionShape, Params, TraceDelegate.IsBound() ? &TraceDelegate : nullptr, UserData);
}

FTraceHandle FAngelscriptWorldCollisionBinds::AsyncSweepByProfile(EAsyncTraceType InTraceType, const FVector& Start, const FVector& End, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FScriptTraceDelegate& InDelegate, uint32 UserData)
{
	FTraceDelegate TraceDelegate = MakeTraceDelegate(InDelegate);
	return GetWorldForCollisionBinding()->AsyncSweepByProfile(InTraceType, Start, End, Rot, ProfileName, CollisionShape, Params, TraceDelegate.IsBound() ? &TraceDelegate : nullptr, UserData);
}

FTraceHandle FAngelscriptWorldCollisionBinds::AsyncOverlapByChannel(const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam, const FScriptOverlapDelegate& InDelegate, uint32 UserData)
{
	FOverlapDelegate OverlapDelegate = MakeOverlapDelegate(InDelegate);
	return GetWorldForCollisionBinding()->AsyncOverlapByChannel(Pos, Rot, TraceChannel, CollisionShape, Params, ResponseParam, OverlapDelegate.IsBound() ? &OverlapDelegate : nullptr, UserData);
}

FTraceHandle FAngelscriptWorldCollisionBinds::AsyncOverlapByObjectType(const FVector& Pos, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FScriptOverlapDelegate& InDelegate, uint32 UserData)
{
	FOverlapDelegate OverlapDelegate = MakeOverlapDelegate(InDelegate);
	return GetWorldForCollisionBinding()->AsyncOverlapByObjectType(Pos, Rot, ObjectQueryParams, CollisionShape, Params, OverlapDelegate.IsBound() ? &OverlapDelegate : nullptr, UserData);
}

FTraceHandle FAngelscriptWorldCollisionBinds::AsyncOverlapByProfile(const FVector& Pos, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FScriptOverlapDelegate& InDelegate, uint32 UserData)
{
	FOverlapDelegate OverlapDelegate = MakeOverlapDelegate(InDelegate);
	return GetWorldForCollisionBinding()->AsyncOverlapByProfile(Pos, Rot, ProfileName, CollisionShape, Params, OverlapDelegate.IsBound() ? &OverlapDelegate : nullptr, UserData);
}

bool FAngelscriptWorldCollisionBinds::QueryTraceData(const FTraceHandle& Handle, FTraceDatum& OutData)
{
	return GetWorldForCollisionBinding()->QueryTraceData(Handle, OutData);
}

bool FAngelscriptWorldCollisionBinds::QueryOverlapData(const FTraceHandle& Handle, FOverlapDatum& OutData)
{
	return GetWorldForCollisionBinding()->QueryOverlapData(Handle, OutData);
}

bool FAngelscriptWorldCollisionBinds::IsTraceHandleValid(const FTraceHandle& Handle, bool bOverlapTrace)
{
	return GetWorldForCollisionBinding()->IsTraceHandleValid(Handle, bOverlapTrace);
}
