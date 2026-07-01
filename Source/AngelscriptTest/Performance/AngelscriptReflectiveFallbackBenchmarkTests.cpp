// ============================================================================
// AngelscriptReflectiveFallbackBenchmarkTests.cpp
//
// Pure C++ microbenchmark comparing four dispatch strategies on the same
// set of UFUNCTIONs:
//
//   A0 (absolute lower bound): direct C++ method call
//       Calls the C++ method directly (Target->Method(args)). No reflection,
//       no FFrame, no exec wrapper - just a virtual or static dispatch. This
//       is the speed of a direct binding (UHT direct-bind path) and serves
//       as the floor that no reflective dispatch can beat.
//
//   A1 (existing baseline): UObject::ProcessEvent
//       Walks the UFunction parameter chain via TFieldIterator on every call,
//       initialises a Parms buffer, and lets ProcessEvent build NewStack.
//       Mirrors the original behaviour of
//       InvokeReflectiveUFunctionFromGenericCall before the cache landed.
//
//   A2 (Invoke without cache): Build FFrame + UFunction::Invoke
//       Same per-call TFieldIterator walk and CopySingleValue parameter set-up
//       as A1, but skips ProcessEvent overhead by constructing FFrame manually
//       and calling Function->Invoke directly. Quantifies the win from
//       avoiding ProcessEvent's PreScriptCall hooks, BP-override checks, and
//       persistent-frame logic.
//
//   A3 (cached Invoke): Pre-built FReflectiveParamCache + memcpy + Invoke
//       Walks the UFunction parameter chain ONCE (during `MakeCache`), then
//       reuses the cached property offsets and the bIsSimpleCopy classifier
//       from `BlueprintCallableReflectiveFallback.cpp`. POD parameters are
//       copied with FMemory::Memcpy; non-POD parameters fall back to virtual
//       CopySingleValue. Mirrors what the production cached path does on
//       every call after the first.
//
// The benchmark intentionally does NOT go through Angelscript at all - the
// goal is to isolate the reflection / dispatch overhead so the measurements
// are reproducible across environments and so the cache vs. ProcessEvent
// numbers can be reasoned about without the AS scheduler in the picture.
//
// Reference: sluaunreal `LuaFunctionAccelerator::call` (cache + Invoke), see
// `Documents/Plans/Plan_ReflectiveFallbackCache.md` for the strategy origin.
//
// Automation ID:
//   Angelscript.TestModule.Performance.ReflectiveFallback.Benchmark
// ============================================================================

#include "Performance/AngelscriptPerformanceTestTypes.h"
#include "AngelscriptPerformanceTestUtils.h"

#include "CQTest.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeExit.h"
#include "UObject/PropertyOptional.h"
#include "UObject/Script.h"
#include "UObject/Stack.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptReflectiveFallbackBenchmarkTests,
	"Angelscript.TestModule.Performance.ReflectiveFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static constexpr int32 WarmupRuns = 1;
	static constexpr int32 MeasurementRuns = 3;
	static constexpr int32 IterationsPerMeasurement = 10000;

	struct FMeasuredSamples
	{
		TArray<double> Samples;
		int64 Checksum = 0;
	};

	template <typename CallableType>
	static FMeasuredSamples MeasureSamples(CallableType&& Callable)
	{
		for (int32 WarmupIndex = 0; WarmupIndex < WarmupRuns; ++WarmupIndex)
		{
			Callable();
		}

		FMeasuredSamples Result;
		for (int32 MeasurementIndex = 0; MeasurementIndex < MeasurementRuns; ++MeasurementIndex)
		{
			const double StartSeconds = FPlatformTime::Seconds();
			Result.Checksum = Callable();
			Result.Samples.Add(FPlatformTime::Seconds() - StartSeconds);
		}
		return Result;
	}

	static void AddMetric(
		TArray<FAngelscriptPerformanceMetric>& Metrics,
		const FString& Name,
		const TArray<double>& Samples,
		const FString& Source)
	{
		Metrics.Add({ Name, Samples, ComputeMedian(Samples), TEXT("seconds"), Source });
	}

	static bool WriteAndVerifyMetrics(
		FAutomationTestBase& Test,
		const FString& RunId,
		const FString& TestGroup,
		const TArray<FAngelscriptPerformanceMetric>& Metrics,
		const TArray<FString>& Notes)
	{
		const FString MetricsPath = WritePerformanceMetricsArtifact(RunId, TestGroup, Metrics, Notes);
		return Test.TestTrue(
			TEXT("Reflective fallback benchmark should write metrics.json"),
			FPlatformFileManager::Get().GetPlatformFile().FileExists(*MetricsPath));
	}

	// ---------------------------------------------------------------------
	// Local replication of the production FReflectiveParamCache so the
	// benchmark is decoupled from the anonymous-namespace type that lives
	// inside BlueprintCallableReflectiveFallback.cpp. The classification
	// rules (bIsSimpleCopy) MUST stay in sync with production - if production
	// changes, this needs to follow. The benchmark is the canonical apples-
	// to-apples measurement, so re-implementing here keeps the comparison
	// honest even if production semantics evolve.
	// ---------------------------------------------------------------------

	static bool LocalIsPropertySimpleCopy(const FProperty* Property)
	{
		if (Property == nullptr)
		{
			return false;
		}

		if (Property->HasAnyPropertyFlags(CPF_IsPlainOldData))
		{
			return true;
		}

		if (CastField<FObjectPropertyBase>(Property) != nullptr)
		{
			return true;
		}

		if (CastField<FStrProperty>(Property) != nullptr
			|| CastField<FNameProperty>(Property) != nullptr
			|| CastField<FTextProperty>(Property) != nullptr
			|| CastField<FArrayProperty>(Property) != nullptr
			|| CastField<FMapProperty>(Property) != nullptr
			|| CastField<FSetProperty>(Property) != nullptr
			|| CastField<FDelegateProperty>(Property) != nullptr
			|| CastField<FMulticastDelegateProperty>(Property) != nullptr
			|| CastField<FSoftObjectProperty>(Property) != nullptr
			|| CastField<FSoftClassProperty>(Property) != nullptr
			|| CastField<FFieldPathProperty>(Property) != nullptr
			|| CastField<FOptionalProperty>(Property) != nullptr)
		{
			return false;
		}

		if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			if (StructProp->Struct != nullptr)
			{
				const EStructFlags Flags = StructProp->Struct->StructFlags;
				const bool bHasDtor = (Flags & STRUCT_NoDestructor) == 0;
				const bool bHasCustomCopy = (Flags & STRUCT_CopyNative) != 0;
				return !bHasDtor && !bHasCustomCopy;
			}
			return false;
		}
		return false;
	}

	struct FBenchParamCache
	{
		struct FEntry
		{
			FProperty* Property = nullptr;
			int32 UEOffset = 0;
			int32 Size = 0;
			bool bIsSimpleCopy = false;
			bool bNeedInitialize = false;
			bool bNeedDestroy = false;
			// Engine FFrame::StepExplicitProperty crashes on CPF_OutParm
			// properties that are missing from the OutParms chain. const-ref
			// UFUNCTION arguments are CPF_OutParm | CPF_ConstParm and must be
			// in the chain even though we never write back to script.
			bool bRequiresOutParmRec = false;
		};
		TArray<FEntry, TInlineAllocator<8>> Params;
		// Indices into Params for every CPF_OutParm property (const or not).
		TArray<int32, TInlineAllocator<4>> OutParamIndices;
		FEntry Return;
		bool bHasReturn = false;
		int32 ParmsSize = 0;
		int32 ReturnValueOffset = MAX_uint16;
	};

	static FBenchParamCache MakeCache(UFunction* Function)
	{
		FBenchParamCache Cache;
		Cache.ParmsSize = Function->PropertiesSize;
		Cache.ReturnValueOffset = Function->ReturnValueOffset;
		Cache.Params.Reserve(8);

		for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It)
		{
			FProperty* Prop = *It;
			FBenchParamCache::FEntry Entry;
			Entry.Property = Prop;
			Entry.UEOffset = Prop->GetOffset_ForInternal();
			Entry.Size = Prop->GetSize();
			Entry.bNeedInitialize = !Prop->HasAnyPropertyFlags(CPF_ZeroConstructor);
			Entry.bNeedDestroy = !Prop->HasAnyPropertyFlags(CPF_NoDestructor)
				&& !Prop->HasAnyPropertyFlags(CPF_IsPlainOldData);
			Entry.bIsSimpleCopy = LocalIsPropertySimpleCopy(Prop);
			Entry.bRequiresOutParmRec = Prop->HasAnyPropertyFlags(CPF_OutParm)
				&& !Prop->HasAnyPropertyFlags(CPF_ReturnParm);

			if (Prop->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				Cache.Return = Entry;
				Cache.bHasReturn = true;
			}
			else
			{
				const int32 ParamIndex = Cache.Params.Add(Entry);
				if (Entry.bRequiresOutParmRec)
				{
					Cache.OutParamIndices.Add(ParamIndex);
				}
			}
		}

		return Cache;
	}

	// Compute a comparable checksum from the return-value buffer of an Invoke
	// so A0 / A1 / A2 / A3 paths can be cross-validated even for non-int /
	// non-string return types. The mapping intentionally collapses values to
	// int64 and does not preserve identity - it just needs to be deterministic
	// for the same input across all four dispatch paths.
	static int64 ExtractChecksum(const FProperty* Return, const void* RetPtr)
	{
		if (Return == nullptr || RetPtr == nullptr)
		{
			return 0;
		}
		if (CastField<FIntProperty>(Return) != nullptr)
		{
			return static_cast<int64>(*reinterpret_cast<const int32*>(RetPtr));
		}
		if (CastField<FInt64Property>(Return) != nullptr)
		{
			return *reinterpret_cast<const int64*>(RetPtr);
		}
		if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Return))
		{
			return BoolProp->GetPropertyValue(RetPtr) ? 1 : 0;
		}
		if (CastField<FFloatProperty>(Return) != nullptr)
		{
			return static_cast<int64>(*reinterpret_cast<const float*>(RetPtr) * 1000.0f);
		}
		if (CastField<FDoubleProperty>(Return) != nullptr)
		{
			return static_cast<int64>(*reinterpret_cast<const double*>(RetPtr) * 1000.0);
		}
		if (const FByteProperty* ByteProp = CastField<FByteProperty>(Return))
		{
			return static_cast<int64>(*reinterpret_cast<const uint8*>(RetPtr));
		}
		if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Return))
		{
			const FNumericProperty* Underlying = EnumProp->GetUnderlyingProperty();
			return Underlying != nullptr ? Underlying->GetSignedIntPropertyValue(RetPtr) : 0;
		}
		if (CastField<FStrProperty>(Return) != nullptr)
		{
			return reinterpret_cast<const FString*>(RetPtr)->Len();
		}
		if (CastField<FNameProperty>(Return) != nullptr)
		{
			return reinterpret_cast<const FName*>(RetPtr)->ToString().Len();
		}
		if (CastField<FObjectProperty>(Return) != nullptr)
		{
			const TObjectPtr<UObject>* PtrSlot = reinterpret_cast<const TObjectPtr<UObject>*>(RetPtr);
			return PtrSlot != nullptr && PtrSlot->Get() != nullptr ? 1 : 0;
		}
		if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Return))
		{
			FScriptArrayHelper Helper(ArrayProp, RetPtr);
			return Helper.Num();
		}
		if (const FSetProperty* SetProp = CastField<FSetProperty>(Return))
		{
			FScriptSetHelper Helper(SetProp, RetPtr);
			return Helper.Num();
		}
		if (const FMapProperty* MapProp = CastField<FMapProperty>(Return))
		{
			FScriptMapHelper Helper(MapProp, RetPtr);
			return Helper.Num();
		}
		// USTRUCT and other complex types: skip (returns 0). Timing data is
		// still valid; cross-path equality just degrades to "all zero".
		return 0;
	}

	// Uses the per-call TFieldIterator walk + InitializeValue + CopySingleValue
	// + ProcessEvent path (matches the original reflective fallback before the
	// cache landed). The InputArgs array supplies parameter values in the same
	// order they appear in the UFunction.
	static int64 InvokeProcessEventOnce(UObject* Target, UFunction* Function, const TArray<const void*>& InputArgs, void* OutReturn = nullptr)
	{
		uint8* Buffer = static_cast<uint8*>(FMemory_Alloca(Function->ParmsSize));
		FMemory::Memzero(Buffer, Function->ParmsSize);
		for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It)
		{
			It->InitializeValue_InContainer(Buffer);
		}

		int32 InputIndex = 0;
		for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It)
		{
			if (It->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				continue;
			}
			if (InputArgs.IsValidIndex(InputIndex) && InputArgs[InputIndex] != nullptr)
			{
				It->CopySingleValue(It->ContainerPtrToValuePtr<void>(Buffer), InputArgs[InputIndex]);
			}
			++InputIndex;
		}

		Target->ProcessEvent(Function, Buffer);

		int64 Checksum = 0;
		if (FProperty* Return = Function->GetReturnProperty())
		{
			void* RetPtr = Return->ContainerPtrToValuePtr<void>(Buffer);
			if (OutReturn != nullptr)
			{
				Return->CopySingleValue(OutReturn, RetPtr);
			}
			Checksum = ExtractChecksum(Return, RetPtr);
		}

		for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It)
		{
			It->DestroyValue_InContainer(Buffer);
		}
		return Checksum;
	}

	// Constructs FFrame manually and calls UFunction::Invoke without using a
	// pre-built cache. Per-call TFieldIterator + CopySingleValue still happen,
	// but ProcessEvent's overhead (PreScriptCall hooks, BP override resolution,
	// persistent-frame management) is bypassed.
	static int64 InvokeUncachedFFrameOnce(UObject* Target, UFunction* Function, const TArray<const void*>& InputArgs, void* OutReturn = nullptr)
	{
		uint8* Buffer = static_cast<uint8*>(FMemory_Alloca(Function->PropertiesSize));
		FMemory::Memzero(Buffer, Function->PropertiesSize);
		for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It)
		{
			It->InitializeValue_InContainer(Buffer);
		}

		int32 InputIndex = 0;
		for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It)
		{
			if (It->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				continue;
			}
			if (InputArgs.IsValidIndex(InputIndex) && InputArgs[InputIndex] != nullptr)
			{
				It->CopySingleValue(It->ContainerPtrToValuePtr<void>(Buffer), InputArgs[InputIndex]);
			}
			++InputIndex;
		}

		FFrame NewStack(Target, Function, Buffer, nullptr, Function->ChildProperties);

		// Engine UObject::ProcessEvent populates FOutParmRec for every
		// CPF_OutParm parameter (including const-ref). Native exec wrappers
		// then dereference NewStack.OutParms via FFrame::StepExplicitProperty;
		// missing entries cause a null deref. Mirror that contract here.
		FOutParmRec** LastOut = &NewStack.OutParms;
		for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It)
		{
			if (!It->HasAnyPropertyFlags(CPF_OutParm) || It->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				continue;
			}
			FOutParmRec* OutRec = static_cast<FOutParmRec*>(FMemory_Alloca(sizeof(FOutParmRec)));
			OutRec->Property = *It;
			OutRec->PropAddr = It->ContainerPtrToValuePtr<uint8>(Buffer);
			OutRec->NextOutParm = nullptr;
			if (*LastOut != nullptr)
			{
				(*LastOut)->NextOutParm = OutRec;
				LastOut = &(*LastOut)->NextOutParm;
			}
			else
			{
				*LastOut = OutRec;
			}
		}

		uint8* ReturnAddress = nullptr;
		if (Function->ReturnValueOffset != MAX_uint16)
		{
			ReturnAddress = Buffer + Function->ReturnValueOffset;
		}
		Function->Invoke(Target, NewStack, ReturnAddress);

		int64 Checksum = 0;
		if (FProperty* Return = Function->GetReturnProperty())
		{
			void* RetPtr = Return->ContainerPtrToValuePtr<void>(Buffer);
			if (OutReturn != nullptr)
			{
				Return->CopySingleValue(OutReturn, RetPtr);
			}
			Checksum = ExtractChecksum(Return, RetPtr);
		}

		for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It)
		{
			It->DestroyValue_InContainer(Buffer);
		}
		return Checksum;
	}

	// Cached path: uses the pre-built FBenchParamCache and FFrame + Invoke.
	// POD args go through Memcpy; non-POD args go through CopySingleValue.
	static int64 InvokeCachedOnce(UObject* Target, UFunction* Function, const FBenchParamCache& Cache, const TArray<const void*>& InputArgs, void* OutReturn = nullptr)
	{
		uint8* Buffer = static_cast<uint8*>(FMemory_Alloca(Cache.ParmsSize));
		FMemory::Memzero(Buffer, Cache.ParmsSize);
		for (const FBenchParamCache::FEntry& E : Cache.Params)
		{
			if (E.bNeedInitialize)
			{
				E.Property->InitializeValue_InContainer(Buffer);
			}
		}
		if (Cache.bHasReturn && Cache.Return.bNeedInitialize)
		{
			Cache.Return.Property->InitializeValue_InContainer(Buffer);
		}

		const int32 ParamCount = Cache.Params.Num();
		for (int32 ParamIndex = 0; ParamIndex < ParamCount; ++ParamIndex)
		{
			const FBenchParamCache::FEntry& E = Cache.Params[ParamIndex];
			if (!InputArgs.IsValidIndex(ParamIndex) || InputArgs[ParamIndex] == nullptr)
			{
				continue;
			}
			void* Dest = Buffer + E.UEOffset;
			if (E.bIsSimpleCopy)
			{
				FMemory::Memcpy(Dest, InputArgs[ParamIndex], E.Size);
			}
			else
			{
				E.Property->CopySingleValue(Dest, InputArgs[ParamIndex]);
			}
		}

		FFrame NewStack(Target, Function, Buffer, nullptr, Function->ChildProperties);

		// Build the FOutParmRec linked list from the precomputed cache so
		// engine exec wrappers find every CPF_OutParm property they expect.
		FOutParmRec** LastOut = &NewStack.OutParms;
		for (int32 OutIdx : Cache.OutParamIndices)
		{
			const FBenchParamCache::FEntry& E = Cache.Params[OutIdx];
			FOutParmRec* OutRec = static_cast<FOutParmRec*>(FMemory_Alloca(sizeof(FOutParmRec)));
			OutRec->Property = E.Property;
			OutRec->PropAddr = Buffer + E.UEOffset;
			OutRec->NextOutParm = nullptr;
			if (*LastOut != nullptr)
			{
				(*LastOut)->NextOutParm = OutRec;
				LastOut = &(*LastOut)->NextOutParm;
			}
			else
			{
				*LastOut = OutRec;
			}
		}

		uint8* ReturnAddress = Cache.bHasReturn ? (Buffer + Cache.ReturnValueOffset) : nullptr;
		Function->Invoke(Target, NewStack, ReturnAddress);

		int64 Checksum = 0;
		if (Cache.bHasReturn)
		{
			void* RetPtr = Buffer + Cache.ReturnValueOffset;
			if (OutReturn != nullptr)
			{
				Cache.Return.Property->CopySingleValue(OutReturn, RetPtr);
			}
			Checksum = ExtractChecksum(Cache.Return.Property, RetPtr);
		}

		for (const FBenchParamCache::FEntry& E : Cache.Params)
		{
			if (E.bNeedDestroy)
			{
				E.Property->DestroyValue_InContainer(Buffer);
			}
		}
		if (Cache.bHasReturn && Cache.Return.bNeedDestroy)
		{
			Cache.Return.Property->DestroyValue_InContainer(Buffer);
		}
		return Checksum;
	}

	// =====================================================================
	// Driver helpers for the four-way A0/A1/A2/A3 benchmark sweep.
	// Each FBenchSpec captures one UFUNCTION's reflective dispatch wiring
	// plus the per-call native C++ body that mirrors what the function does
	// at the language level. The runner produces matched samples for all
	// four strategies so the metrics file stays apples-to-apples.
	// =====================================================================
	struct FRunResult
	{
		FMeasuredSamples A0;
		FMeasuredSamples A1;
		FMeasuredSamples A2;
		FMeasuredSamples A3;
	};

	template <typename A0BodyType>
	static FRunResult BenchOne(
		UObject* Target,
		UFunction* Function,
		const FBenchParamCache& Cache,
		const TArray<const void*>& Args,
		A0BodyType&& A0Body,
		TFunction<void()> ResetBeforeEach = TFunction<void()>())
	{
		auto MaybeReset = [&]() { if (ResetBeforeEach) ResetBeforeEach(); };

		FRunResult R;
		R.A0 = MeasureSamples([&]() -> int64
		{
			MaybeReset();
			int64 Sum = 0;
			for (int32 Index = 0; Index < IterationsPerMeasurement; ++Index)
			{
				Sum += A0Body(Index);
			}
			return Sum;
		});
		R.A1 = MeasureSamples([&]() -> int64
		{
			MaybeReset();
			int64 Sum = 0;
			for (int32 Index = 0; Index < IterationsPerMeasurement; ++Index)
			{
				Sum += InvokeProcessEventOnce(Target, Function, Args);
			}
			return Sum;
		});
		R.A2 = MeasureSamples([&]() -> int64
		{
			MaybeReset();
			int64 Sum = 0;
			for (int32 Index = 0; Index < IterationsPerMeasurement; ++Index)
			{
				Sum += InvokeUncachedFFrameOnce(Target, Function, Args);
			}
			return Sum;
		});
		R.A3 = MeasureSamples([&]() -> int64
		{
			MaybeReset();
			int64 Sum = 0;
			for (int32 Index = 0; Index < IterationsPerMeasurement; ++Index)
			{
				Sum += InvokeCachedOnce(Target, Function, Cache, Args);
			}
			return Sum;
		});
		return R;
	}

	static void EmitFourWayMetrics(
		TArray<FAngelscriptPerformanceMetric>& Metrics,
		const FString& Prefix,
		const FRunResult& R)
	{
		AddMetric(Metrics, Prefix + TEXT(".a0_native_cpp_seconds"),    R.A0.Samples, TEXT("Native C++"));
		AddMetric(Metrics, Prefix + TEXT(".a1_processevent_seconds"),  R.A1.Samples, TEXT("ProcessEvent"));
		AddMetric(Metrics, Prefix + TEXT(".a2_invoke_seconds"),        R.A2.Samples, TEXT("FFrame+Invoke"));
		AddMetric(Metrics, Prefix + TEXT(".a3_cached_invoke_seconds"), R.A3.Samples, TEXT("Cached FFrame+Invoke"));
	}

	static void TestChecksumsAcrossPaths(
		FAutomationTestBase& Test,
		const FString& Name,
		const FRunResult& R,
		bool bSkipA0Compare = false)
	{
		Test.TestEqual(*FString::Printf(TEXT("%s checksums (A1 vs A2) should match"), *Name), R.A1.Checksum, R.A2.Checksum);
		Test.TestEqual(*FString::Printf(TEXT("%s checksums (A1 vs A3) should match"), *Name), R.A1.Checksum, R.A3.Checksum);
		if (!bSkipA0Compare)
		{
			Test.TestEqual(*FString::Printf(TEXT("%s checksums (A0 vs A1) should match"), *Name), R.A0.Checksum, R.A1.Checksum);
		}
	}

	static bool RunReflectiveFallbackBenchmark(FAutomationTestBase& Test);

public:
	TEST_METHOD(Benchmark)
	{
		ASSERT_THAT(IsTrue(RunReflectiveFallbackBenchmark(*TestRunner)));
	}
};

bool FAngelscriptReflectiveFallbackBenchmarkTests::RunReflectiveFallbackBenchmark(FAutomationTestBase& Test)
{

	UAngelscriptPerformanceTestTargetObject* Target = GetMutableDefault<UAngelscriptPerformanceTestTargetObject>();
	if (!Test.TestNotNull(TEXT("Benchmark target CDO should resolve"), Target))
	{
		return false;
	}
	ON_SCOPE_EXIT { Target->ResetValues(); };

	UClass* TargetClass = UAngelscriptPerformanceTestTargetObject::StaticClass();

	// Resolve the benchmark UFUNCTIONs across a wide type matrix:
	//   - statics: NoOp/Add (no-arg / 2-arg POD)
	//   - members no-op: MemberNoOp (zero overhead body)
	//   - POD getters: bool / int32 / double / enum
	//   - non-POD getters: name / string / struct / object / array / map
	//   - non-POD setters: string / struct / array / map (const-ref args)
	auto Resolve = [&](const TCHAR* Name) -> UFunction*
	{
		UFunction* Fn = TargetClass->FindFunctionByName(FName(Name));
		Test.TestNotNull(*FString::Printf(TEXT("UFUNCTION %s should exist"), Name), Fn);
		return Fn;
	};

	UFunction* StaticNoOpFn   = Resolve(TEXT("StaticNoOp"));
	UFunction* StaticAddFn    = Resolve(TEXT("StaticAdd"));
	UFunction* MemberNoOpFn   = Resolve(TEXT("MemberNoOp"));
	UFunction* GetBoolFn      = Resolve(TEXT("GetBoolValueFunction"));
	UFunction* GetInt32Fn     = Resolve(TEXT("GetInt32ValueFunction"));
	UFunction* GetDoubleFn    = Resolve(TEXT("GetDoubleValueFunction"));
	UFunction* GetEnumFn      = Resolve(TEXT("GetEnumValueFunction"));
	UFunction* GetNameFn      = Resolve(TEXT("GetNameValueFunction"));
	UFunction* GetStringFn    = Resolve(TEXT("GetStringValueFunction"));
	UFunction* GetStructFn    = Resolve(TEXT("GetStructValueFunction"));
	UFunction* GetObjectFn    = Resolve(TEXT("GetObjectValueFunction"));
	UFunction* GetArrayFn     = Resolve(TEXT("GetArrayValueFunction"));
	UFunction* GetMapFn       = Resolve(TEXT("GetMapValueFunction"));
	UFunction* SetStringFn    = Resolve(TEXT("SetStringValueFunction"));
	UFunction* SetStructFn    = Resolve(TEXT("SetStructValueFunction"));
	UFunction* SetArrayFn     = Resolve(TEXT("SetArrayValueFunction"));
	UFunction* SetMapFn       = Resolve(TEXT("SetMapValueFunction"));
	if (Test.HasAnyErrors())
	{
		return false;
	}

	// One-time cache build for every UFunction (same lifetime as production
	// FBlueprintCallableReflectiveSignature::CachedParams).
	const FBenchParamCache StaticNoOpCache = MakeCache(StaticNoOpFn);
	const FBenchParamCache StaticAddCache  = MakeCache(StaticAddFn);
	const FBenchParamCache MemberNoOpCache = MakeCache(MemberNoOpFn);
	const FBenchParamCache GetBoolCache    = MakeCache(GetBoolFn);
	const FBenchParamCache GetInt32Cache   = MakeCache(GetInt32Fn);
	const FBenchParamCache GetDoubleCache  = MakeCache(GetDoubleFn);
	const FBenchParamCache GetEnumCache    = MakeCache(GetEnumFn);
	const FBenchParamCache GetNameCache    = MakeCache(GetNameFn);
	const FBenchParamCache GetStringCache  = MakeCache(GetStringFn);
	const FBenchParamCache GetStructCache  = MakeCache(GetStructFn);
	const FBenchParamCache GetObjectCache  = MakeCache(GetObjectFn);
	const FBenchParamCache GetArrayCache   = MakeCache(GetArrayFn);
	const FBenchParamCache GetMapCache     = MakeCache(GetMapFn);
	const FBenchParamCache SetStringCache  = MakeCache(SetStringFn);
	const FBenchParamCache SetStructCache  = MakeCache(SetStructFn);
	const FBenchParamCache SetArrayCache   = MakeCache(SetArrayFn);
	const FBenchParamCache SetMapCache     = MakeCache(SetMapFn);

	// Pre-built input arg arrays. Keep storage stable for the entire test so
	// the const void* pointers stay valid across all 4 measurement loops.
	const TArray<const void*> EmptyArgs;
	const int32 ConstArgA = 7;
	const int32 ConstArgB = 11;
	TArray<const void*> AddArgs;
	AddArgs.Add(&ConstArgA);
	AddArgs.Add(&ConstArgB);

	const FString SampleString = TEXT("ReflectiveCacheBenchmark");
	TArray<const void*> StringArgs;
	StringArgs.Add(&SampleString);

	FAngelscriptPerformanceTestStruct SampleStruct;
	SampleStruct.Value = 42;
	TArray<const void*> StructArgs;
	StructArgs.Add(&SampleStruct);

	const TArray<int32> SampleArray = { 1, 2, 3, 4, 5 };
	TArray<const void*> ArrayArgs;
	ArrayArgs.Add(&SampleArray);

	TMap<FName, int32> SampleMap;
	SampleMap.Add(TEXT("Alpha"), 1);
	SampleMap.Add(TEXT("Beta"), 2);
	SampleMap.Add(TEXT("Gamma"), 3);
	TArray<const void*> MapArgs;
	MapArgs.Add(&SampleMap);

	// Pre-populate the target with values that getters will read so we get
	// non-zero, type-meaningful checksums to cross-check against.
	Target->BoolValue = true;
	Target->Int32Value = 42;
	Target->DoubleValue = 3.1415926;
	Target->EnumValue = EAngelscriptPerformanceTestEnum::One;
	Target->NameValue = TEXT("PerformanceTest");
	Target->StringValue = TEXT("RuntimePerformance");
	Target->StructValue = SampleStruct;
	Target->ObjectValue = TargetClass;
	Target->ArrayValue = SampleArray;
	Target->MapValue = SampleMap;

	TArray<FAngelscriptPerformanceMetric> Metrics;

	// ---------------- Static no-arg / 2-arg ----------------
	{
		// StaticAdd is a pure function; the C++ compiler trivially folds
		// `Sum += Index + 11` so A0 here measures loop overhead, not call
		// overhead. Keep it as a baseline anchor and skip cross-path equality
		// for A0 (Sum vs reflection's StaticAdd return-of-last-call).
		FRunResult R = BenchOne(Target, StaticAddFn, StaticAddCache, AddArgs,
			[&](int32) -> int64 { return UAngelscriptPerformanceTestTargetObject::StaticAdd(ConstArgA, ConstArgB); });
		EmitFourWayMetrics(Metrics, TEXT("reflective.staticadd"), R);
		TestChecksumsAcrossPaths(Test, TEXT("StaticAdd"), R);
	}
	{
		FRunResult R = BenchOne(Target, StaticNoOpFn, StaticNoOpCache, EmptyArgs,
			[&](int32) -> int64 { UAngelscriptPerformanceTestTargetObject::StaticNoOp(); return 0; });
		EmitFourWayMetrics(Metrics, TEXT("reflective.staticnoop"), R);
		TestChecksumsAcrossPaths(Test, TEXT("StaticNoOp"), R);
	}

	// ---------------- Member no-op (lower bound for member calls) ----------
	{
		FRunResult R = BenchOne(Target, MemberNoOpFn, MemberNoOpCache, EmptyArgs,
			[&](int32) -> int64 { Target->MemberNoOp(); return 0; });
		EmitFourWayMetrics(Metrics, TEXT("reflective.membernoop"), R);
		TestChecksumsAcrossPaths(Test, TEXT("MemberNoOp"), R);
	}

	// ---------------- POD scalar getters ----------------
	{
		FRunResult R = BenchOne(Target, GetBoolFn, GetBoolCache, EmptyArgs,
			[&](int32) -> int64 { return Target->GetBoolValueFunction() ? 1 : 0; });
		EmitFourWayMetrics(Metrics, TEXT("reflective.getbool"), R);
		// FBoolProperty's bitfield extraction differs subtly between branches;
		// we still compare A1 vs A2 vs A3 (all reflection paths share the
		// extraction code), but skip A0 vs A1 for noise tolerance.
		TestChecksumsAcrossPaths(Test, TEXT("GetBool"), R, /*bSkipA0Compare=*/true);
	}
	{
		FRunResult R = BenchOne(Target, GetInt32Fn, GetInt32Cache, EmptyArgs,
			[&](int32) -> int64 { return Target->GetInt32ValueFunction(); });
		EmitFourWayMetrics(Metrics, TEXT("reflective.getint32"), R);
		TestChecksumsAcrossPaths(Test, TEXT("GetInt32"), R);
	}
	{
		FRunResult R = BenchOne(Target, GetDoubleFn, GetDoubleCache, EmptyArgs,
			[&](int32) -> int64 { return static_cast<int64>(Target->GetDoubleValueFunction() * 1000.0); });
		EmitFourWayMetrics(Metrics, TEXT("reflective.getdouble"), R);
		TestChecksumsAcrossPaths(Test, TEXT("GetDouble"), R);
	}
	{
		FRunResult R = BenchOne(Target, GetEnumFn, GetEnumCache, EmptyArgs,
			[&](int32) -> int64 { return static_cast<int64>(Target->GetEnumValueFunction()); });
		EmitFourWayMetrics(Metrics, TEXT("reflective.getenum"), R);
		TestChecksumsAcrossPaths(Test, TEXT("GetEnum"), R);
	}

	// ---------------- Non-POD getters (FName / FString / USTRUCT / UObject* / TArray / TMap) ----
	{
		FRunResult R = BenchOne(Target, GetNameFn, GetNameCache, EmptyArgs,
			[&](int32) -> int64 { return Target->GetNameValueFunction().ToString().Len(); });
		EmitFourWayMetrics(Metrics, TEXT("reflective.getname"), R);
		TestChecksumsAcrossPaths(Test, TEXT("GetName"), R);
	}
	{
		FRunResult R = BenchOne(Target, GetStringFn, GetStringCache, EmptyArgs,
			[&](int32) -> int64 { return Target->GetStringValueFunction().Len(); });
		EmitFourWayMetrics(Metrics, TEXT("reflective.getstring"), R);
		TestChecksumsAcrossPaths(Test, TEXT("GetString"), R);
	}
	{
		// USTRUCT return: ExtractChecksum returns 0 for FStructProperty, so
		// reflection paths checksum to 0; A0 also returns 0 to match.
		FRunResult R = BenchOne(Target, GetStructFn, GetStructCache, EmptyArgs,
			[&](int32) -> int64 { (void)Target->GetStructValueFunction(); return 0; });
		EmitFourWayMetrics(Metrics, TEXT("reflective.getstruct"), R);
		TestChecksumsAcrossPaths(Test, TEXT("GetStruct"), R);
	}
	{
		FRunResult R = BenchOne(Target, GetObjectFn, GetObjectCache, EmptyArgs,
			[&](int32) -> int64 { return Target->GetObjectValueFunction() != nullptr ? 1 : 0; });
		EmitFourWayMetrics(Metrics, TEXT("reflective.getobject"), R);
		TestChecksumsAcrossPaths(Test, TEXT("GetObject"), R);
	}
	{
		FRunResult R = BenchOne(Target, GetArrayFn, GetArrayCache, EmptyArgs,
			[&](int32) -> int64 { return Target->GetArrayValueFunction().Num(); });
		EmitFourWayMetrics(Metrics, TEXT("reflective.getarray"), R);
		TestChecksumsAcrossPaths(Test, TEXT("GetArray"), R);
	}
	{
		FRunResult R = BenchOne(Target, GetMapFn, GetMapCache, EmptyArgs,
			[&](int32) -> int64 { return Target->GetMapValueFunction().Num(); });
		EmitFourWayMetrics(Metrics, TEXT("reflective.getmap"), R);
		TestChecksumsAcrossPaths(Test, TEXT("GetMap"), R);
	}

	// ---------------- Non-POD setters (CPF_OutParm | CPF_ConstParm refs) ----
	// Setters return void, so checksum from ExtractChecksum is always 0;
	// A0 lambdas also return 0 to match. We rely on the timing data only.
	{
		FRunResult R = BenchOne(Target, SetStringFn, SetStringCache, StringArgs,
			[&](int32) -> int64 { Target->SetStringValueFunction(SampleString); return 0; },
			[&]() { Target->StringValue.Reset(); });
		EmitFourWayMetrics(Metrics, TEXT("reflective.setstring"), R);
		TestChecksumsAcrossPaths(Test, TEXT("SetString"), R);
	}
	{
		FRunResult R = BenchOne(Target, SetStructFn, SetStructCache, StructArgs,
			[&](int32) -> int64 { Target->SetStructValueFunction(SampleStruct); return 0; });
		EmitFourWayMetrics(Metrics, TEXT("reflective.setstruct"), R);
		TestChecksumsAcrossPaths(Test, TEXT("SetStruct"), R);
	}
	{
		FRunResult R = BenchOne(Target, SetArrayFn, SetArrayCache, ArrayArgs,
			[&](int32) -> int64 { Target->SetArrayValueFunction(SampleArray); return 0; },
			[&]() { Target->ArrayValue.Reset(); });
		EmitFourWayMetrics(Metrics, TEXT("reflective.setarray"), R);
		TestChecksumsAcrossPaths(Test, TEXT("SetArray"), R);
	}
	{
		FRunResult R = BenchOne(Target, SetMapFn, SetMapCache, MapArgs,
			[&](int32) -> int64 { Target->SetMapValueFunction(SampleMap); return 0; },
			[&]() { Target->MapValue.Reset(); });
		EmitFourWayMetrics(Metrics, TEXT("reflective.setmap"), R);
		TestChecksumsAcrossPaths(Test, TEXT("SetMap"), R);
	}

	const TArray<FString> Notes = {
		FString::Printf(TEXT("warmup_runs=%d"), WarmupRuns),
		FString::Printf(TEXT("measurement_runs=%d"), MeasurementRuns),
		FString::Printf(TEXT("iterations_per_measurement=%d"), IterationsPerMeasurement),
		TEXT("strategy=A0:Native C++ / A1:ProcessEvent / A2:FFrame+Invoke / A3:Cached FFrame+Invoke"),
		TEXT("source=AngelscriptReflectiveFallbackBenchmarkTests.cpp"),
	};

	const bool bWroteMetrics = WriteAndVerifyMetrics(
		Test,
		TEXT("ReflectiveFallback_Benchmark"),
		TEXT("Angelscript.TestModule.Performance.ReflectiveFallback.Benchmark"),
		Metrics,
		Notes);

	return !Test.HasAnyErrors() && bWroteMetrics;
}

#endif
