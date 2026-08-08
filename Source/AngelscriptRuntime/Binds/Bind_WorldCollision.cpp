#include "AngelscriptBinds.h"
#include "AngelscriptType.h"
#include "Bind_WorldCollision_Functions.h"
#include "Helper_CppType.h"

#include "WorldCollision.h"

namespace
{
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

	void BindWorldCollisionTypeDeclarations(FAngelscriptBinds& Binds)
	{
		BindAsyncTraceTypeDeclarations(Binds);
		BindTraceHandleTypeDeclarations(Binds);
		BindTraceDatumTypeDeclarations(Binds);
		BindOverlapDatumTypeDeclarations(Binds);
	}

	void BindWorldCollisionTypeInfrastructure(FAngelscriptBinds& Binds)
	{
		BindTraceHandleTypeInfrastructure(Binds);
		BindTraceDatumTypeInfrastructure(Binds);
		BindOverlapDatumTypeInfrastructure(Binds);
	}

	void BindWorldCollisionManualBindings(FAngelscriptBinds& Binds)
	{
		BindTraceHandle(Binds);
		BindTraceDatum(Binds);
		BindOverlapDatum(Binds);
		BindSyncQueries(Binds);
		BindAsyncQueries(Binds);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_WorldCollision_TypeDeclarations(
	TEXT("WorldCollision.TypeDeclarations"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindWorldCollisionTypeDeclarations);

AS_FORCE_LINK const FAngelscriptBind Bind_WorldCollision_TypeInfrastructure(
	TEXT("WorldCollision.TypeInfrastructure"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindWorldCollisionTypeInfrastructure);

AS_FORCE_LINK const FAngelscriptBind Bind_WorldCollision_ManualBindings(
	TEXT("WorldCollision.ManualBindings"),
	EAngelscriptBindPhase::ManualBindings,
	&BindWorldCollisionManualBindings);
