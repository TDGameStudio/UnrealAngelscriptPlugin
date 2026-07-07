#include "Engine/Engine.h"
#include "Engine/World.h"

#include "FunctionLibraries/WorldCollisionStatics.h"

#include "AngelscriptEngine.h"
#include "AngelscriptType.h"
#include "AngelscriptBinds.h"

#include "Helper_CppType.h"

namespace WorldCollision
{
	UWorld* GetWorld()
	{
		return GEngine->GetWorldFromContextObject(FAngelscriptEngine::TryGetCurrentWorldContextObject(), EGetWorldErrorMode::LogAndReturnNull);
	}
};

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_AsyncTraceType((int32)FAngelscriptBinds::EOrder::Early, []
{
	auto TraceType_ = FAngelscriptBinds::Enum("EAsyncTraceType");
	TraceType_["Test"]	 = EAsyncTraceType::Test;
	TraceType_["Single"] = EAsyncTraceType::Single;
	TraceType_["Multi"]	 = EAsyncTraceType::Multi;
});

struct FTraceHandleType : TAngelscriptCppType<FTraceHandle>
{
	FString GetAngelscriptTypeName() const override
	{
		return TEXT("FTraceHandle");
	}

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		OutCppForm.CppType = GetAngelscriptTypeName();
		return true;
	}

	bool NeverRequiresGC(const FAngelscriptTypeUsage& Usage) const override { return true; }
};

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FTraceHandle(FAngelscriptBinds::EOrder::Early, []
{
	FBindFlags Flags;
	Flags.bPOD = true;

	auto FTraceHandle_ = FAngelscriptBinds::ValueClass<FTraceHandle>("FTraceHandle", Flags);
	FAngelscriptType::Register(MakeShared<FTraceHandleType>());

	FTraceHandle_.Constructor("void f()", [](FTraceHandle* Address)
	{
		new(Address) FTraceHandle();
	});
	SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(FTraceHandle_, "FTraceHandle");

	FTraceHandle_.Constructor("void f(uint64 InHandle)", [](FTraceHandle* Address, uint64 InHandle)
	{
		FTraceHandle* TraceHandle = new(Address) FTraceHandle();
		TraceHandle->_Handle = InHandle;
	});
	SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(FTraceHandle_, "FTraceHandle");

	FTraceHandle_.Method("bool opEquals(const FTraceHandle& Other) const", METHODPR_TRIVIAL(bool, FTraceHandle, operator==, (const FTraceHandle&) const));
	FTraceHandle_.Method("bool IsValid() const", METHOD_TRIVIAL(FTraceHandle, IsValid));
	FTraceHandle_.Property("uint64 _Handle", &FTraceHandle::_Handle);
	FTraceHandle_.Property("uint32 _FrameNumber", (size_t)&(((FTraceHandle*)nullptr)->_Data.FrameNumber));
	FTraceHandle_.Property("uint32 _Index", (size_t)& (((FTraceHandle*)nullptr)->_Data.Index));
});

struct FTraceDatumType : TAngelscriptCppType<FTraceDatum>
{
	FString GetAngelscriptTypeName() const override
	{
		return TEXT("FTraceDatum");
	}

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		OutCppForm.CppType = GetAngelscriptTypeName();
		return true;
	}

	bool NeverRequiresGC(const FAngelscriptTypeUsage& Usage) const override { return true; }
};

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FTraceDatum_Early(FAngelscriptBinds::EOrder::Early, []
{
	FBindFlags Flags;
	auto FTraceDatum_ = FAngelscriptBinds::ValueClass<FTraceDatum>("FTraceDatum", Flags);
	FAngelscriptType::Register(MakeShared<FTraceDatumType>());

	FTraceDatum_.Constructor("void f()", [](FTraceDatum* Address)
	{
		new(Address) FTraceDatum();
	});
	SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(FTraceDatum_, "FTraceDatum");
});

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FTraceDatum_Late(FAngelscriptBinds::EOrder::Late, []
{
	auto FTraceDatum_ = FAngelscriptBinds::ExistingClass("FTraceDatum");

	FTraceDatum_.Property("FVector Start", &FTraceDatum::Start);
	FTraceDatum_.Property("FVector End", &FTraceDatum::End);
	FTraceDatum_.Property("FQuat Rot", &FTraceDatum::Rot);
	FTraceDatum_.Property("TArray<FHitResult> OutHits", &FTraceDatum::OutHits);
	FTraceDatum_.Property("EAsyncTraceType TraceType", &FTraceDatum::TraceType);
	FTraceDatum_.Property("ECollisionChannel TraceChannel", &FOverlapDatum::TraceChannel);
	FTraceDatum_.Property("uint32 UserData", &FTraceDatum::UserData);
});

struct FOverlapDatumType : TAngelscriptCppType<FOverlapDatum>
{
	FString GetAngelscriptTypeName() const override
	{
		return TEXT("FOverlapDatum");
	}

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		OutCppForm.CppType = GetAngelscriptTypeName();
		return true;
	}

	bool NeverRequiresGC(const FAngelscriptTypeUsage& Usage) const override { return true; }
};

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FOverlapDatum_Early(FAngelscriptBinds::EOrder::Early, []
{
	FBindFlags Flags;
	auto FOverlapDatum_ = FAngelscriptBinds::ValueClass<FOverlapDatum>("FOverlapDatum", Flags);
	FAngelscriptType::Register(MakeShared<FOverlapDatumType>());

	FOverlapDatum_.Constructor("void f()", [](FOverlapDatum* Address)
	{
		new(Address) FOverlapDatum();
	});
	SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(FOverlapDatum_, "FOverlapDatum");
});

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FOverlapDatum_Late(FAngelscriptBinds::EOrder::Late, []
{
	auto FOverlapDatum_ = FAngelscriptBinds::ExistingClass("FOverlapDatum");

	FOverlapDatum_.Property("FVector Pos", &FOverlapDatum::Pos);
	FOverlapDatum_.Property("FQuat Rot", &FOverlapDatum::Rot);
	FOverlapDatum_.Property("TArray<FOverlapResult> OutOverlaps", &FOverlapDatum::OutOverlaps);
	FOverlapDatum_.Property("ECollisionChannel TraceChannel", &FOverlapDatum::TraceChannel);
	FOverlapDatum_.Property("uint32 UserData", &FOverlapDatum::UserData);
});

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_Traces((int32)FAngelscriptBinds::EOrder::Late, []
{
	FAngelscriptBinds::FNamespace ns("System");

	FAngelscriptBinds::BindGlobalFunction("bool LineTraceTestByChannel(const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam)",
		[](const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam)
		{
			return WorldCollision::GetWorld()->LineTraceTestByChannel(Start, End, TraceChannel, Params, ResponseParam);
		});
	FAngelscriptBinds::BindGlobalFunction("bool LineTraceTestByObjectType(const FVector& Start, const FVector& End, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam)",
		[](const FVector& Start, const FVector& End, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionQueryParams& Params)
		{
			return WorldCollision::GetWorld()->LineTraceTestByObjectType(Start, End, ObjectQueryParams, Params);
		});
	FAngelscriptBinds::BindGlobalFunction("bool LineTraceTestByProfile(const FVector& Start, const FVector& End, FName ProfileName, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam)",
		[](const FVector& Start, const FVector& End, FName ProfileName, const FCollisionQueryParams& Params)
		{
			return WorldCollision::GetWorld()->LineTraceTestByProfile(Start, End, ProfileName, Params);
		});
	FAngelscriptBinds::BindGlobalFunction("bool LineTraceSingleByChannel(FHitResult& OutHit, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam)",
		[](FHitResult& OutHit, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam)
		{
			return WorldCollision::GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, TraceChannel, Params, ResponseParam);
		});
	FAngelscriptBinds::BindGlobalFunction("bool LineTraceSingleByObjectType(FHitResult& OutHit, const FVector& Start, const FVector& End, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam)",
		[](FHitResult& OutHit, const FVector& Start, const FVector& End, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionQueryParams& Params)
		{
			return WorldCollision::GetWorld()->LineTraceSingleByObjectType(OutHit, Start, End, ObjectQueryParams, Params);
		});
	FAngelscriptBinds::BindGlobalFunction("bool LineTraceSingleByProfile(FHitResult& OutHit, const FVector& Start, const FVector& End, FName ProfileName, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam)",
		[](FHitResult& OutHit, const FVector& Start, const FVector& End, FName ProfileName, const FCollisionQueryParams& Params)
		{
			return WorldCollision::GetWorld()->LineTraceSingleByProfile(OutHit, Start, End, ProfileName, Params);
		});
	FAngelscriptBinds::BindGlobalFunction("bool LineTraceMultiByChannel(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam)",
		[](TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam)
		{
			return WorldCollision::GetWorld()->LineTraceMultiByChannel(OutHits, Start, End, TraceChannel, Params, ResponseParam);
		});
	FAngelscriptBinds::BindGlobalFunction("bool LineTraceMultiByObjectType(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam)",
		[](TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionQueryParams& Params)
		{
			return WorldCollision::GetWorld()->LineTraceMultiByObjectType(OutHits, Start, End, ObjectQueryParams, Params);
		});
	FAngelscriptBinds::BindGlobalFunction("bool LineTraceMultiByProfile(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, FName ProfileName, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam)",
		[](TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, FName ProfileName, const FCollisionQueryParams& Params)
		{
			return WorldCollision::GetWorld()->LineTraceMultiByProfile(OutHits, Start, End, ProfileName, Params);
		});
	FAngelscriptBinds::BindGlobalFunction("bool SweepTestByChannel(const FVector& Start, const FVector& End, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam)",
		[](const FVector& Start, const FVector& End, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam)
		{
			return WorldCollision::GetWorld()->SweepTestByChannel(Start, End, Rot, TraceChannel, CollisionShape, Params, ResponseParam);
		});
	FAngelscriptBinds::BindGlobalFunction("bool SweepTestByObjectType(const FVector& Start, const FVector& End, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam)",
		[](const FVector& Start, const FVector& End, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params)
		{
			return WorldCollision::GetWorld()->SweepTestByObjectType(Start, End, Rot, ObjectQueryParams, CollisionShape, Params);
		});
	FAngelscriptBinds::BindGlobalFunction("bool SweepTestByProfile(const FVector& Start, const FVector& End, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam)",
		[](const FVector& Start, const FVector& End, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params)
		{
			return WorldCollision::GetWorld()->SweepTestByProfile(Start, End, Rot, ProfileName, CollisionShape, Params);
		});
	FAngelscriptBinds::BindGlobalFunction("bool SweepSingleByChannel(FHitResult& OutHit, const FVector& Start, const FVector& End, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam)",
		[](FHitResult& OutHit, const FVector& Start, const FVector& End, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam)
		{
			return WorldCollision::GetWorld()->SweepSingleByChannel(OutHit, Start, End, Rot, TraceChannel, CollisionShape, Params, ResponseParam);
		});
	FAngelscriptBinds::BindGlobalFunction("bool SweepSingleByObjectType(FHitResult& OutHit, const FVector& Start, const FVector& End, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam)",
		[](FHitResult& OutHit, const FVector& Start, const FVector& End, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params)
		{
			return WorldCollision::GetWorld()->SweepSingleByObjectType(OutHit, Start, End, Rot, ObjectQueryParams, CollisionShape, Params);
		});
	FAngelscriptBinds::BindGlobalFunction("bool SweepSingleByProfile(FHitResult& OutHit, const FVector& Start, const FVector& End, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam)",
		[](FHitResult& OutHit, const FVector& Start, const FVector& End, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params)
		{
			return WorldCollision::GetWorld()->SweepSingleByProfile(OutHit, Start, End, Rot, ProfileName, CollisionShape, Params);
		});
	FAngelscriptBinds::BindGlobalFunction("bool SweepMultiByChannel(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam)",
		[](TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam)
		{
			return WorldCollision::GetWorld()->SweepMultiByChannel(OutHits, Start, End, Rot, TraceChannel, CollisionShape, Params, ResponseParam);
		});
	FAngelscriptBinds::BindGlobalFunction("bool SweepMultiByObjectType(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam)",
		[](TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params)
		{
			return WorldCollision::GetWorld()->SweepMultiByObjectType(OutHits, Start, End, Rot, ObjectQueryParams, CollisionShape, Params);
		});
	FAngelscriptBinds::BindGlobalFunction("bool SweepMultiByProfile(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam)",
		[](TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params)
		{
			return WorldCollision::GetWorld()->SweepMultiByProfile(OutHits, Start, End, Rot, ProfileName, CollisionShape, Params);
		});
	FAngelscriptBinds::BindGlobalFunction("bool OverlapBlockingTestByChannel(const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam)",
		[](const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam)
		{
			return WorldCollision::GetWorld()->OverlapBlockingTestByChannel(Pos, Rot, TraceChannel, CollisionShape, Params, ResponseParam);
		});
	FAngelscriptBinds::BindGlobalFunction("bool OverlapAnyTestByChannel(const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam)",
		[](const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam)
		{
			return WorldCollision::GetWorld()->OverlapAnyTestByChannel(Pos, Rot, TraceChannel, CollisionShape, Params, ResponseParam);
		});
	FAngelscriptBinds::BindGlobalFunction("bool OverlapAnyTestByObjectType(const FVector& Pos, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam)",
		[](const FVector& Pos, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params)
		{
			return WorldCollision::GetWorld()->OverlapAnyTestByObjectType(Pos, Rot, ObjectQueryParams, CollisionShape, Params);
		});
	FAngelscriptBinds::BindGlobalFunction("bool OverlapBlockingTestByProfile(const FVector& Pos, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam)",
		[](const FVector& Pos, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params)
		{
			return WorldCollision::GetWorld()->OverlapBlockingTestByProfile(Pos, Rot, ProfileName, CollisionShape, Params);
		});
	FAngelscriptBinds::BindGlobalFunction("bool OverlapAnyTestByProfile(const FVector& Pos, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam)",
		[](const FVector& Pos, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params)
		{
			return WorldCollision::GetWorld()->OverlapAnyTestByProfile(Pos, Rot, ProfileName, CollisionShape, Params);
		});
	FAngelscriptBinds::BindGlobalFunction("bool OverlapMultiByChannel(TArray<FOverlapResult>& OutOverlaps, const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam)",
		[](TArray<FOverlapResult>& OutOverlaps, const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam)
		{
			return WorldCollision::GetWorld()->OverlapMultiByChannel(OutOverlaps, Pos, Rot, TraceChannel, CollisionShape, Params, ResponseParam);
		});
	FAngelscriptBinds::BindGlobalFunction("bool OverlapMultiByObjectType(TArray<FOverlapResult>& OutOverlaps, const FVector& Pos, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam)",
		[](TArray<FOverlapResult>& OutOverlaps, const FVector& Pos, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params)
		{
			return WorldCollision::GetWorld()->OverlapMultiByObjectType(OutOverlaps, Pos, Rot, ObjectQueryParams, CollisionShape, Params);
		});
	FAngelscriptBinds::BindGlobalFunction("bool OverlapMultiByProfile(TArray<FOverlapResult>& OutOverlaps, const FVector& Pos, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam)",
		[](TArray<FOverlapResult>& OutOverlaps, const FVector& Pos, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params)
		{
			return WorldCollision::GetWorld()->OverlapMultiByProfile(OutOverlaps, Pos, Rot, ProfileName, CollisionShape, Params);
		});

	FAngelscriptBinds::BindGlobalFunction("bool ComponentSweepMulti(TArray<FHitResult>& OutHits, UPrimitiveComponent PrimComp, const FVector& Start, const FVector& End, const FQuat& Rot, const FComponentQueryParams& Params)",
		[](TArray<FHitResult>& OutHits, UPrimitiveComponent* PrimComp, const FVector& Start, const FVector& End, const FQuat& Rot, const FComponentQueryParams& Params)
		{
			if (PrimComp == nullptr)
			{
				OutHits.Reset();
				return false;
			}

			return WorldCollision::GetWorld()->ComponentSweepMulti(OutHits, PrimComp, Start, End, Rot, Params);
		});
	FAngelscriptBinds::BindGlobalFunction("bool ComponentSweepMulti(TArray<FHitResult>& OutHits, UPrimitiveComponent PrimComp, const FVector& Start, const FVector& End, const FRotator& Rot, const FComponentQueryParams& Params)",
		[](TArray<FHitResult>& OutHits, UPrimitiveComponent* PrimComp, const FVector& Start, const FVector& End, const FRotator& Rot, const FComponentQueryParams& Params)
		{
			if (PrimComp == nullptr)
			{
				OutHits.Reset();
				return false;
			}

			return WorldCollision::GetWorld()->ComponentSweepMulti(OutHits, PrimComp, Start, End, Rot, Params);
		});
	FAngelscriptBinds::BindGlobalFunction("bool ComponentSweepMultiByChannel(TArray<FHitResult>& OutHits, UPrimitiveComponent PrimComp, const FVector& Start, const FVector& End, const FQuat& Rot, ECollisionChannel TraceChannel, const FComponentQueryParams& Params)",
		[](TArray<FHitResult>& OutHits, UPrimitiveComponent* PrimComp, const FVector& Start, const FVector& End, const FQuat& Rot, ECollisionChannel TraceChannel, const FComponentQueryParams& Params)
		{
			return WorldCollision::GetWorld()->ComponentSweepMultiByChannel(OutHits, PrimComp, Start, End, Rot, TraceChannel, Params);
		});
	FAngelscriptBinds::BindGlobalFunction("bool ComponentSweepMultiByChannel(TArray<FHitResult>& OutHits, UPrimitiveComponent PrimComp, const FVector& Start, const FVector& End, const FRotator& Rot, ECollisionChannel TraceChannel, const FComponentQueryParams& Params)",
		[](TArray<FHitResult>& OutHits, UPrimitiveComponent* PrimComp, const FVector& Start, const FVector& End, const FRotator& Rot, ECollisionChannel TraceChannel, const FComponentQueryParams& Params)
		{
			return WorldCollision::GetWorld()->ComponentSweepMultiByChannel(OutHits, PrimComp, Start, End, Rot, TraceChannel, Params);
		});

	FAngelscriptBinds::BindGlobalFunction("bool ComponentOverlapMulti(TArray<FOverlapResult>& OutOverlaps, const UPrimitiveComponent PrimComp, const FVector& Pos, const FQuat& Rot, const FComponentQueryParams& Params = FComponentQueryParams::DefaultComponentQueryParams, const FCollisionObjectQueryParams& ObjectQueryParams = FCollisionObjectQueryParams::DefaultObjectQueryParam)",
		[](TArray<FOverlapResult>& OutOverlaps, const UPrimitiveComponent* PrimComp, const FVector& Pos, const FQuat& Rot, const FComponentQueryParams& Params, const FCollisionObjectQueryParams& ObjectQueryParams)
		{
			if (PrimComp == nullptr)
			{
				OutOverlaps.Reset();
				return false;
			}

			return WorldCollision::GetWorld()->ComponentOverlapMulti(OutOverlaps, PrimComp, Pos, Rot, Params, ObjectQueryParams);
		});
	FAngelscriptBinds::BindGlobalFunction("bool ComponentOverlapMulti(TArray<FOverlapResult>& OutOverlaps, const UPrimitiveComponent PrimComp, const FVector& Pos, const FRotator& Rot, const FComponentQueryParams& Params = FComponentQueryParams::DefaultComponentQueryParams, const FCollisionObjectQueryParams& ObjectQueryParams = FCollisionObjectQueryParams::DefaultObjectQueryParam)",
		[](TArray<FOverlapResult>& OutOverlaps, const UPrimitiveComponent* PrimComp, const FVector& Pos, const FRotator& Rot, const FComponentQueryParams& Params, const FCollisionObjectQueryParams& ObjectQueryParams)
		{
			if (PrimComp == nullptr)
			{
				OutOverlaps.Reset();
				return false;
			}

			return WorldCollision::GetWorld()->ComponentOverlapMulti(OutOverlaps, PrimComp, Pos, Rot, Params, ObjectQueryParams);
		});
	FAngelscriptBinds::BindGlobalFunction("bool ComponentOverlapMultiByChannel(TArray<FOverlapResult>& OutOverlaps, const UPrimitiveComponent PrimComp, const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FComponentQueryParams& Params = FComponentQueryParams::DefaultComponentQueryParams, const FCollisionObjectQueryParams& ObjectQueryParams = FCollisionObjectQueryParams::DefaultObjectQueryParam)",
		[](TArray<FOverlapResult>& OutOverlaps, const UPrimitiveComponent* PrimComp, const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FComponentQueryParams& Params, const FCollisionObjectQueryParams& ObjectQueryParams)
		{
			return WorldCollision::GetWorld()->ComponentOverlapMultiByChannel(OutOverlaps, PrimComp, Pos, Rot, TraceChannel, Params, ObjectQueryParams);
		});
	FAngelscriptBinds::BindGlobalFunction("bool ComponentOverlapMultiByChannel(TArray<FOverlapResult>& OutOverlaps, const UPrimitiveComponent PrimComp, const FVector& Pos, const FRotator& Rot, ECollisionChannel TraceChannel, const FComponentQueryParams& Params = FComponentQueryParams::DefaultComponentQueryParams, const FCollisionObjectQueryParams& ObjectQueryParams = FCollisionObjectQueryParams::DefaultObjectQueryParam)",
		[](TArray<FOverlapResult>& OutOverlaps, const UPrimitiveComponent* PrimComp, const FVector& Pos, const FRotator& Rot, ECollisionChannel TraceChannel, const FComponentQueryParams& Params, const FCollisionObjectQueryParams& ObjectQueryParams)
		{
			return WorldCollision::GetWorld()->ComponentOverlapMultiByChannel(OutOverlaps, PrimComp, Pos, Rot, TraceChannel, Params, ObjectQueryParams);
		});
});

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_AsyncTraces((int32)FAngelscriptBinds::EOrder::Late, []
{
	FAngelscriptBinds::FNamespace ns("System");

	FAngelscriptBinds::BindGlobalFunction("FTraceHandle AsyncLineTraceByChannel(EAsyncTraceType InTraceType, const FVector& Start,const FVector& End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam, const FScriptTraceDelegate& InDelegate = FScriptTraceDelegate(), uint32 UserData = 0)",
		[](EAsyncTraceType InTraceType, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, 
			const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam, const FScriptTraceDelegate& InDelegate, uint32 UserData) -> FTraceHandle
		{
			FTraceDelegate TraceDelegate;
			if (InDelegate.IsBound())
			{
				TraceDelegate = FTraceDelegate::CreateWeakLambda(const_cast<UObject*>(InDelegate.GetUObject()), [InDelegate](const FTraceHandle& TraceHandle, FTraceDatum& TraceDatum)
				{
					InDelegate.ExecuteIfBound(TraceHandle._Handle, TraceDatum.OutHits, TraceDatum.UserData);
				});
			}
			return WorldCollision::GetWorld()->AsyncLineTraceByChannel(InTraceType, Start, End, TraceChannel, Params, ResponseParam, TraceDelegate.IsBound() ? &TraceDelegate : nullptr, UserData);
		});

	FAngelscriptBinds::BindGlobalFunction("FTraceHandle AsyncLineTraceByObjectType(EAsyncTraceType InTraceType, const FVector& Start,const FVector& End, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FScriptTraceDelegate& InDelegate = FScriptTraceDelegate(), uint32 UserData = 0 )",
		[](EAsyncTraceType InTraceType, const FVector& Start, const FVector& End, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionQueryParams& Params, const FScriptTraceDelegate& InDelegate, uint32 UserData) -> FTraceHandle
		{
			FTraceDelegate TraceDelegate;
			if (InDelegate.IsBound())
			{
				TraceDelegate = FTraceDelegate::CreateWeakLambda(const_cast<UObject*>(InDelegate.GetUObject()), [InDelegate](const FTraceHandle& TraceHandle, FTraceDatum& TraceDatum)
				{
					InDelegate.ExecuteIfBound(TraceHandle._Handle, TraceDatum.OutHits, TraceDatum.UserData);
				});
			}
			return WorldCollision::GetWorld()->AsyncLineTraceByObjectType(InTraceType, Start, End, ObjectQueryParams, Params, TraceDelegate.IsBound() ? &TraceDelegate : nullptr, UserData);
		});

	FAngelscriptBinds::BindGlobalFunction("FTraceHandle AsyncLineTraceByProfile(EAsyncTraceType InTraceType, const FVector& Start, const FVector& End, FName ProfileName, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FScriptTraceDelegate& InDelegate = FScriptTraceDelegate(), uint32 UserData = 0)",
		[](EAsyncTraceType InTraceType, const FVector& Start, const FVector& End, FName ProfileName, const FCollisionQueryParams& Params, const FScriptTraceDelegate& InDelegate, uint32 UserData) -> FTraceHandle
		{
			FTraceDelegate TraceDelegate;
			if (InDelegate.IsBound())
			{
				TraceDelegate = FTraceDelegate::CreateWeakLambda(const_cast<UObject*>(InDelegate.GetUObject()), [InDelegate](const FTraceHandle& TraceHandle, FTraceDatum& TraceDatum)
				{
					InDelegate.ExecuteIfBound(TraceHandle._Handle, TraceDatum.OutHits, TraceDatum.UserData);
				});
			}
			return WorldCollision::GetWorld()->AsyncLineTraceByProfile(InTraceType, Start, End, ProfileName, Params, TraceDelegate.IsBound() ? &TraceDelegate : nullptr, UserData);
		});

	FAngelscriptBinds::BindGlobalFunction("FTraceHandle AsyncSweepByChannel(EAsyncTraceType InTraceType, const FVector& Start, const FVector& End, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam, const FScriptTraceDelegate& InDelegate = FScriptTraceDelegate(), uint32 UserData = 0)",
		[](EAsyncTraceType InTraceType, const FVector& Start, const FVector& End, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam, const FScriptTraceDelegate& InDelegate, uint32 UserData) -> FTraceHandle
		{
			FTraceDelegate TraceDelegate;
			if (InDelegate.IsBound())
			{
				TraceDelegate = FTraceDelegate::CreateWeakLambda(const_cast<UObject*>(InDelegate.GetUObject()), [InDelegate](const FTraceHandle& TraceHandle, FTraceDatum& TraceDatum)
				{
					InDelegate.ExecuteIfBound(TraceHandle._Handle, TraceDatum.OutHits, TraceDatum.UserData);
				});
			}
			return WorldCollision::GetWorld()->AsyncSweepByChannel(InTraceType, Start, End, Rot, TraceChannel, CollisionShape, Params, ResponseParam, TraceDelegate.IsBound() ? &TraceDelegate : nullptr, UserData);
		});

	FAngelscriptBinds::BindGlobalFunction("FTraceHandle AsyncSweepByObjectType(EAsyncTraceType InTraceType, const FVector& Start, const FVector& End, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FScriptTraceDelegate& InDelegate = FScriptTraceDelegate(), uint32 UserData = 0)",
		[](EAsyncTraceType InTraceType, const FVector& Start, const FVector& End, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FScriptTraceDelegate& InDelegate, uint32 UserData) -> FTraceHandle
		{
			FTraceDelegate TraceDelegate;
			if (InDelegate.IsBound())
			{
				TraceDelegate = FTraceDelegate::CreateWeakLambda(const_cast<UObject*>(InDelegate.GetUObject()), [InDelegate](const FTraceHandle& TraceHandle, FTraceDatum& TraceDatum)
				{
					InDelegate.ExecuteIfBound(TraceHandle._Handle, TraceDatum.OutHits, TraceDatum.UserData);
				});
			}
			return WorldCollision::GetWorld()->AsyncSweepByObjectType(InTraceType, Start, End, Rot, ObjectQueryParams, CollisionShape, Params, TraceDelegate.IsBound() ? &TraceDelegate : nullptr, UserData);
		});

	FAngelscriptBinds::BindGlobalFunction("FTraceHandle AsyncSweepByProfile(EAsyncTraceType InTraceType, const FVector& Start, const FVector& End, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FScriptTraceDelegate& InDelegate = FScriptTraceDelegate(), uint32 UserData = 0)",
		[](EAsyncTraceType InTraceType, const FVector& Start, const FVector& End, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FScriptTraceDelegate& InDelegate, uint32 UserData) -> FTraceHandle
		{
			FTraceDelegate TraceDelegate;
			if (InDelegate.IsBound())
			{
				TraceDelegate = FTraceDelegate::CreateWeakLambda(const_cast<UObject*>(InDelegate.GetUObject()), [InDelegate](const FTraceHandle& TraceHandle, FTraceDatum& TraceDatum)
				{
					InDelegate.ExecuteIfBound(TraceHandle._Handle, TraceDatum.OutHits, TraceDatum.UserData);
				});
			}
			return WorldCollision::GetWorld()->AsyncSweepByProfile(InTraceType, Start, End, Rot, ProfileName, CollisionShape, Params, TraceDelegate.IsBound() ? &TraceDelegate : nullptr, UserData);
		});

	FAngelscriptBinds::BindGlobalFunction("FTraceHandle AsyncOverlapByChannel(const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam, const FScriptOverlapDelegate& InDelegate = FScriptOverlapDelegate(), uint32 UserData = 0)",
		[](const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam, const FScriptOverlapDelegate& InDelegate, uint32 UserData) -> FTraceHandle
		{
			FOverlapDelegate OverlapDelegate;
			if (InDelegate.IsBound())
			{
				OverlapDelegate = FOverlapDelegate::CreateWeakLambda(const_cast<UObject*>(InDelegate.GetUObject()), [InDelegate](const FTraceHandle& TraceHandle, FOverlapDatum& OverlapDatum)
				{
					InDelegate.ExecuteIfBound(TraceHandle._Handle, OverlapDatum.OutOverlaps, OverlapDatum.UserData);
				});
			}
			return WorldCollision::GetWorld()->AsyncOverlapByChannel(Pos, Rot, TraceChannel, CollisionShape, Params, ResponseParam, OverlapDelegate.IsBound() ? &OverlapDelegate : nullptr, UserData);
		});

	FAngelscriptBinds::BindGlobalFunction("FTraceHandle AsyncOverlapByObjectType(const FVector& Pos, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FScriptOverlapDelegate& InDelegate = FScriptOverlapDelegate(), uint32 UserData = 0)",
		[](const FVector& Pos, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FScriptOverlapDelegate& InDelegate, uint32 UserData) -> FTraceHandle
		{
			FOverlapDelegate OverlapDelegate;
			if (InDelegate.IsBound())
			{
				OverlapDelegate = FOverlapDelegate::CreateWeakLambda(const_cast<UObject*>(InDelegate.GetUObject()), [InDelegate](const FTraceHandle& TraceHandle, FOverlapDatum& OverlapDatum)
				{
					InDelegate.ExecuteIfBound(TraceHandle._Handle, OverlapDatum.OutOverlaps, OverlapDatum.UserData);
				});
			}
			return WorldCollision::GetWorld()->AsyncOverlapByObjectType(Pos, Rot, ObjectQueryParams, CollisionShape, Params, OverlapDelegate.IsBound() ? &OverlapDelegate : nullptr, UserData);
		});

	FAngelscriptBinds::BindGlobalFunction("FTraceHandle AsyncOverlapByProfile(const FVector& Pos, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FScriptOverlapDelegate& InDelegate = FScriptOverlapDelegate(), uint32 UserData = 0)",
		[](const FVector& Pos, const FQuat& Rot, FName ProfileName, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FScriptOverlapDelegate& InDelegate, uint32 UserData) -> FTraceHandle
		{
			FOverlapDelegate OverlapDelegate;
			if (InDelegate.IsBound())
			{
				OverlapDelegate = FOverlapDelegate::CreateWeakLambda(const_cast<UObject*>(InDelegate.GetUObject()), [InDelegate](const FTraceHandle& TraceHandle, FOverlapDatum& OverlapDatum)
				{
					InDelegate.ExecuteIfBound(TraceHandle._Handle, OverlapDatum.OutOverlaps, OverlapDatum.UserData);
				});
			}
			return WorldCollision::GetWorld()->AsyncOverlapByProfile(Pos, Rot, ProfileName, CollisionShape, Params, OverlapDelegate.IsBound() ? &OverlapDelegate : nullptr, UserData);
		});

	FAngelscriptBinds::BindGlobalFunction("bool QueryTraceData(const FTraceHandle& Handle, FTraceDatum& OutData)", [](const FTraceHandle& Handle, FTraceDatum& OutData) -> bool
		{
			return WorldCollision::GetWorld()->QueryTraceData(Handle, OutData);
		});

	FAngelscriptBinds::BindGlobalFunction("bool QueryOverlapData(const FTraceHandle& Handle, FOverlapDatum& OutData)", [](const FTraceHandle& Handle, FOverlapDatum& OutData) -> bool
		{
			return WorldCollision::GetWorld()->QueryOverlapData(Handle, OutData);
		});

	FAngelscriptBinds::BindGlobalFunction("bool IsTraceHandleValid(const FTraceHandle& Handle, bool bOverlapTrace) no_discard", [](const FTraceHandle& Handle, bool bOverlapTrace) -> bool
		{
			return WorldCollision::GetWorld()->IsTraceHandleValid(Handle, bOverlapTrace);
		});
});
