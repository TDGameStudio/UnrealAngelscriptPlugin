// ============================================================================
// AngelscriptCollisionParamsBindingsTests.cpp
//
// Collision params binding coverage �?CQTest refactor. Automation ID:
//   Angelscript.TestModule.Bindings.CollisionParams.FAngelscriptCollisionParamsBindingsTest.*
//
// Sections:
//   CollisionQueryParamsBehaviour �?full parity test for FCollisionQueryParams,
//     FComponentQueryParams, FCollisionObjectQueryParams, FCollisionResponseContainer
//
// CQTest adaptation notes:
//   One legacy automation test merged into one TEST_CLASS.
//   This test retains its custom execution pattern (parameterised function with
//   out-ref arguments) because the script populates multiple struct outputs that
//   are compared against native equivalents. The original helper namespace is
//   preserved intact.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"
#include "Bindings/AngelscriptWorldCollisionBindingsTestHelpers.h"

#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "Templates/Function.h"

#if WITH_ANGELSCRIPT_UNITTESTS




TEST_CLASS_WITH_FLAGS(FAngelscriptCollisionParamsBindingsTest, "Angelscript.TestModule.Bindings.CollisionParams",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static constexpr ANSICHAR CollisionParamsModuleName[] = "ASCollisionQueryParamsBehaviour";

	template <typename IdArrayType>
	static TArray<uint32> CopyIgnoredIds(const IdArrayType& Source)
	{
		TArray<uint32> Result;
		Result.Reserve(Source.Num());
		for (uint32 Id : Source)
		{
			Result.Add(Id);
		}
		return Result;
	}

	static FCollisionQueryParams BuildNativeCollisionQueryParams(const AActor* IgnoredActor, const UPrimitiveComponent* IgnoredComponent)
	{
		FCollisionQueryParams QueryParams;
		QueryParams.TraceTag = TEXT("TraceTag");
		QueryParams.OwnerTag = TEXT("OwnerTag");
		QueryParams.bTraceComplex = true;
		QueryParams.bFindInitialOverlaps = true;
		QueryParams.bReturnFaceIndex = true;
		QueryParams.bReturnPhysicalMaterial = true;
		QueryParams.bIgnoreBlocks = true;
		QueryParams.bIgnoreTouches = true;
		QueryParams.bSkipNarrowPhase = true;
		QueryParams.MobilityType = EQueryMobilityType::Dynamic;
		QueryParams.IgnoreMask = 17;
		QueryParams.AddIgnoredActor(IgnoredActor);
		QueryParams.AddIgnoredComponent(IgnoredComponent);
		QueryParams.ClearIgnoredSourceObjects();
		QueryParams.ClearIgnoredComponents();
		QueryParams.AddIgnoredActor(IgnoredActor);
		QueryParams.AddIgnoredComponent(IgnoredComponent);
		return QueryParams;
	}

	static FComponentQueryParams BuildNativeComponentQueryParams(const AActor* IgnoredActor, const UPrimitiveComponent* IgnoredComponent)
	{
		FComponentQueryParams QueryParams;
		QueryParams.TraceTag = TEXT("ComponentTrace");
		QueryParams.OwnerTag = TEXT("ComponentOwner");
		QueryParams.bTraceComplex = true;
		QueryParams.bReturnFaceIndex = true;
		QueryParams.MobilityType = EQueryMobilityType::Static;
		QueryParams.IgnoreMask = 23;
		QueryParams.ShapeCollisionMask.Bits = 3;
		QueryParams.AddIgnoredActor(IgnoredActor);
		QueryParams.AddIgnoredComponent(IgnoredComponent);
		QueryParams.ClearIgnoredSourceObjects();
		QueryParams.ClearIgnoredComponents();
		QueryParams.AddIgnoredActor(IgnoredActor);
		QueryParams.AddIgnoredComponent(IgnoredComponent);
		return QueryParams;
	}

	static FCollisionObjectQueryParams BuildNativeObjectQueryParams()
	{
		FCollisionObjectQueryParams QueryParams;
		QueryParams.IgnoreMask = 29;
		QueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
		QueryParams.AddObjectTypesToQuery(ECC_Camera);
		QueryParams.AddObjectTypesToQuery(ECC_Pawn);
		QueryParams.RemoveObjectTypesToQuery(ECC_Pawn);
		return QueryParams;
	}

	static FCollisionResponseContainer BuildNativeResponseContainer()
	{
		FCollisionResponseContainer ResponseContainer(ECR_Ignore);
		ResponseContainer.SetResponse(ECC_Visibility, ECR_Block);
		ResponseContainer.SetResponse(ECC_Camera, ECR_Overlap);
		return ResponseContainer;
	}

	static FCollisionResponseContainer BuildNativeMinResponseContainer()
	{
		FCollisionResponseContainer ResponseContainer = BuildNativeResponseContainer();

		FCollisionResponseContainer OtherContainer(ECR_Block);
		OtherContainer.SetResponse(ECC_Visibility, ECR_Overlap);
		OtherContainer.SetResponse(ECC_WorldStatic, ECR_Ignore);

		return FCollisionResponseContainer::CreateMinContainer(ResponseContainer, OtherContainer);
	}

	static bool ExpectSingleIgnoredId(
		FAutomationTestBase& Test,
		const TCHAR* ContextLabel,
		const TArray<uint32>& ActualIds,
		const uint32 ExpectedId)
	{
		FNoDiscardAsserter LocalAssert(Test);
		bool bPassed = true;
		bPassed &= LocalAssert.AreEqual(
			1,
			ActualIds.Num(),
			*FString::Printf(TEXT("%s should contain exactly one ignored entry"), ContextLabel));

		if (ActualIds.Num() == 1)
		{
			bPassed &= LocalAssert.AreEqual(
				ExpectedId,
				ActualIds[0],
				*FString::Printf(TEXT("%s should preserve the ignored object ID"), ContextLabel));
		}

		return bPassed;
	}

	static bool ExpectCollisionQueryParamsParity(
		FAutomationTestBase& Test,
		const TCHAR* ContextLabel,
		const FCollisionQueryParams& ScriptParams,
		const FCollisionQueryParams& NativeParams,
		const uint32 ExpectedActorId,
		const uint32 ExpectedComponentId)
	{
		FNoDiscardAsserter LocalAssert(Test);
		bool bPassed = true;
		bPassed &= LocalAssert.AreEqual(NativeParams.TraceTag, ScriptParams.TraceTag, *FString::Printf(TEXT("%s should preserve TraceTag"), ContextLabel));
		bPassed &= LocalAssert.AreEqual(NativeParams.OwnerTag, ScriptParams.OwnerTag, *FString::Printf(TEXT("%s should preserve OwnerTag"), ContextLabel));
		bPassed &= LocalAssert.AreEqual(NativeParams.bTraceComplex, ScriptParams.bTraceComplex, *FString::Printf(TEXT("%s should preserve bTraceComplex"), ContextLabel));
		bPassed &= LocalAssert.AreEqual(NativeParams.bFindInitialOverlaps, ScriptParams.bFindInitialOverlaps, *FString::Printf(TEXT("%s should preserve bFindInitialOverlaps"), ContextLabel));
		bPassed &= LocalAssert.AreEqual(NativeParams.bReturnFaceIndex, ScriptParams.bReturnFaceIndex, *FString::Printf(TEXT("%s should preserve bReturnFaceIndex"), ContextLabel));
		bPassed &= LocalAssert.AreEqual(NativeParams.bReturnPhysicalMaterial, ScriptParams.bReturnPhysicalMaterial, *FString::Printf(TEXT("%s should preserve bReturnPhysicalMaterial"), ContextLabel));
		bPassed &= LocalAssert.AreEqual(NativeParams.bIgnoreBlocks, ScriptParams.bIgnoreBlocks, *FString::Printf(TEXT("%s should preserve bIgnoreBlocks"), ContextLabel));
		bPassed &= LocalAssert.AreEqual(NativeParams.bIgnoreTouches, ScriptParams.bIgnoreTouches, *FString::Printf(TEXT("%s should preserve bIgnoreTouches"), ContextLabel));
		bPassed &= LocalAssert.AreEqual(NativeParams.bSkipNarrowPhase, ScriptParams.bSkipNarrowPhase, *FString::Printf(TEXT("%s should preserve bSkipNarrowPhase"), ContextLabel));
		bPassed &= LocalAssert.AreEqual(NativeParams.MobilityType, ScriptParams.MobilityType, *FString::Printf(TEXT("%s should preserve MobilityType"), ContextLabel));
		bPassed &= LocalAssert.AreEqual(NativeParams.IgnoreMask, ScriptParams.IgnoreMask, *FString::Printf(TEXT("%s should preserve IgnoreMask"), ContextLabel));
		bPassed &= ExpectSingleIgnoredId(Test, *FString::Printf(TEXT("%s ignored actors"), ContextLabel), CopyIgnoredIds(ScriptParams.GetIgnoredSourceObjects()), ExpectedActorId);
		bPassed &= ExpectSingleIgnoredId(Test, *FString::Printf(TEXT("%s ignored components"), ContextLabel), CopyIgnoredIds(ScriptParams.GetIgnoredComponents()), ExpectedComponentId);
		return bPassed;
	}

	static bool ExpectComponentQueryParamsParity(
		FAutomationTestBase& Test,
		const FComponentQueryParams& ScriptParams,
		const FComponentQueryParams& NativeParams,
		const uint32 ExpectedActorId,
		const uint32 ExpectedComponentId)
	{
		bool bPassed = ExpectCollisionQueryParamsParity(
			Test,
			TEXT("CollisionQueryParamsBehaviour component query params"),
			static_cast<const FCollisionQueryParams&>(ScriptParams),
			static_cast<const FCollisionQueryParams&>(NativeParams),
			ExpectedActorId,
			ExpectedComponentId);
		FNoDiscardAsserter LocalAssert(Test);
		bPassed &= LocalAssert.AreEqual(
			NativeParams.ShapeCollisionMask.Bits,
			ScriptParams.ShapeCollisionMask.Bits,
			TEXT("CollisionQueryParamsBehaviour should preserve ShapeCollisionMask.Bits"));
		return bPassed;
	}

public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(CollisionQueryParamsBehaviour)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASCollisionParams_Behaviour"), ASTEST_AS(R"AS(
			int PopulateCollisionBindings(
			AActor IgnoredActor,
			UPrimitiveComponent IgnoredComponent,
			FCollisionQueryParams& OutQueryParams,
			FComponentQueryParams& OutComponentQueryParams,
			FCollisionObjectQueryParams& OutObjectQueryParams,
			FCollisionResponseContainer& OutResponseContainer,
			FCollisionResponseContainer& OutMinResponseContainer)
			{
				int Failures = 0;

				FCollisionQueryParams QueryParams;
				QueryParams.TraceTag = n"TraceTag";
				QueryParams.OwnerTag = n"OwnerTag";
				QueryParams.bTraceComplex = true;
				QueryParams.bFindInitialOverlaps = true;
				QueryParams.bReturnFaceIndex = true;
				QueryParams.bReturnPhysicalMaterial = true;
				QueryParams.bIgnoreBlocks = true;
				QueryParams.bIgnoreTouches = true;
				QueryParams.bSkipNarrowPhase = true;
				QueryParams.MobilityType = EQueryMobilityType::Dynamic;
				QueryParams.IgnoreMask = 17;
				QueryParams.AddIgnoredActor(IgnoredActor);
				QueryParams.AddIgnoredComponent(IgnoredComponent);
				if (QueryParams.GetIgnoredActors().Num() != 1)
				{
					Failures |= 1;
				}
				if (QueryParams.GetIgnoredComponents().Num() != 1)
				{
					Failures |= 2;
				}
				QueryParams.ClearIgnoredActors();
				QueryParams.ClearIgnoredComponents();
				if (QueryParams.GetIgnoredActors().Num() != 0)
				{
					Failures |= 4;
				}
				if (QueryParams.GetIgnoredComponents().Num() != 0)
				{
					Failures |= 8;
				}
				QueryParams.AddIgnoredActor(IgnoredActor);
				QueryParams.AddIgnoredComponent(IgnoredComponent);

				FComponentQueryParams ComponentQueryParams;
				ComponentQueryParams.TraceTag = n"ComponentTrace";
				ComponentQueryParams.OwnerTag = n"ComponentOwner";
				ComponentQueryParams.bTraceComplex = true;
				ComponentQueryParams.bReturnFaceIndex = true;
				ComponentQueryParams.MobilityType = EQueryMobilityType::Static;
				ComponentQueryParams.IgnoreMask = 23;
				ComponentQueryParams.ShapeCollisionMask.Bits = 3;
				ComponentQueryParams.AddIgnoredActor(IgnoredActor);
				ComponentQueryParams.AddIgnoredComponent(IgnoredComponent);
				if (ComponentQueryParams.GetIgnoredActors().Num() != 1)
				{
					Failures |= 16;
				}
				if (ComponentQueryParams.GetIgnoredComponents().Num() != 1)
				{
					Failures |= 32;
				}
				ComponentQueryParams.ClearIgnoredActors();
				ComponentQueryParams.ClearIgnoredComponents();
				if (ComponentQueryParams.GetIgnoredActors().Num() != 0)
				{
					Failures |= 64;
				}
				if (ComponentQueryParams.GetIgnoredComponents().Num() != 0)
				{
					Failures |= 128;
				}
				ComponentQueryParams.AddIgnoredActor(IgnoredActor);
				ComponentQueryParams.AddIgnoredComponent(IgnoredComponent);

				FCollisionObjectQueryParams ObjectQueryParams;
				ObjectQueryParams.IgnoreMask = 29;
				ObjectQueryParams.AddObjectTypesToQuery(ECollisionChannel::ECC_WorldStatic);
				ObjectQueryParams.AddObjectTypesToQuery(ECollisionChannel::ECC_Camera);
				ObjectQueryParams.AddObjectTypesToQuery(ECollisionChannel::ECC_Pawn);
				ObjectQueryParams.RemoveObjectTypesToQuery(ECollisionChannel::ECC_Pawn);
				if (!ObjectQueryParams.IsValid())
				{
					Failures |= 256;
				}

				FCollisionResponseContainer ResponseContainer(ECollisionResponse::ECR_Ignore);
				if (!ResponseContainer.SetResponse(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block))
				{
					Failures |= 512;
				}
				if (!ResponseContainer.SetResponse(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Overlap))
				{
					Failures |= 1024;
				}

				FCollisionResponseContainer OtherContainer(ECollisionResponse::ECR_Block);
				OtherContainer.SetResponse(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Overlap);
				OtherContainer.SetResponse(ECollisionChannel::ECC_WorldStatic, ECollisionResponse::ECR_Ignore);

				OutQueryParams = QueryParams;
				OutComponentQueryParams = ComponentQueryParams;
				OutObjectQueryParams = ObjectQueryParams;
				OutResponseContainer = ResponseContainer;
				OutMinResponseContainer = FCollisionResponseContainer::CreateMinContainer(ResponseContainer, OtherContainer);

				return Failures;
			}
			)AS"));
		if (!Mod.IsValid()) return;

		AActor* TestActor = NewObject<AActor>(GetTransientPackage(), NAME_None, RF_Transient);
		UBoxComponent* TestComponent = NewObject<UBoxComponent>(TestActor, NAME_None, RF_Transient);
		ASSERT_THAT(IsNotNull(TestActor, TEXT("CollisionQueryParamsBehaviour should create a transient actor")));
		ASSERT_THAT(IsNotNull(TestComponent, TEXT("CollisionQueryParamsBehaviour should create a transient primitive component")));

		FCollisionQueryParams ScriptQueryParams;
		FComponentQueryParams ScriptComponentQueryParams;
		FCollisionObjectQueryParams ScriptObjectQueryParams;
		FCollisionResponseContainer ScriptResponseContainer;
		FCollisionResponseContainer ScriptMinResponseContainer;
		int32 ResultMask = INDEX_NONE;

		auto& M = Mod.GetModule();
		if (!WorldCollisionExecuteIntFunction(
			*TestRunner,
			Engine,
			M,
			TEXT("int PopulateCollisionBindings(AActor IgnoredActor, UPrimitiveComponent IgnoredComponent, FCollisionQueryParams& OutQueryParams, FComponentQueryParams& OutComponentQueryParams, FCollisionObjectQueryParams& OutObjectQueryParams, FCollisionResponseContainer& OutResponseContainer, FCollisionResponseContainer& OutMinResponseContainer)"),
			[this, TestActor, TestComponent, &ScriptQueryParams, &ScriptComponentQueryParams, &ScriptObjectQueryParams, &ScriptResponseContainer, &ScriptMinResponseContainer](asIScriptContext& Context)
			{
				return WorldCollisionSetArgObjectChecked(*TestRunner, Context, 0, TestActor, TEXT("PopulateCollisionBindings"))
					&& WorldCollisionSetArgObjectChecked(*TestRunner, Context, 1, TestComponent, TEXT("PopulateCollisionBindings"))
					&& WorldCollisionSetArgAddressChecked(*TestRunner, Context, 2, &ScriptQueryParams, TEXT("PopulateCollisionBindings"))
					&& WorldCollisionSetArgAddressChecked(*TestRunner, Context, 3, &ScriptComponentQueryParams, TEXT("PopulateCollisionBindings"))
					&& WorldCollisionSetArgAddressChecked(*TestRunner, Context, 4, &ScriptObjectQueryParams, TEXT("PopulateCollisionBindings"))
					&& WorldCollisionSetArgAddressChecked(*TestRunner, Context, 5, &ScriptResponseContainer, TEXT("PopulateCollisionBindings"))
					&& WorldCollisionSetArgAddressChecked(*TestRunner, Context, 6, &ScriptMinResponseContainer, TEXT("PopulateCollisionBindings"));
			},
			TEXT("PopulateCollisionBindings"),
			ResultMask))
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			0,
			ResultMask,
			TEXT("CollisionQueryParamsBehaviour should preserve script-side ignored-list and response mutation checks")));

		const uint32 ExpectedActorId = TestActor->GetUniqueID();
		const uint32 ExpectedComponentId = TestComponent->GetUniqueID();
		const FCollisionQueryParams NativeQueryParams = BuildNativeCollisionQueryParams(TestActor, TestComponent);
		const FComponentQueryParams NativeComponentQueryParams = BuildNativeComponentQueryParams(TestActor, TestComponent);
		const FCollisionObjectQueryParams NativeObjectQueryParams = BuildNativeObjectQueryParams();
		const FCollisionResponseContainer NativeResponseContainer = BuildNativeResponseContainer();
		const FCollisionResponseContainer NativeMinResponseContainer = BuildNativeMinResponseContainer();

		ExpectCollisionQueryParamsParity(
			*TestRunner,
			TEXT("CollisionQueryParamsBehaviour query params"),
			ScriptQueryParams,
			NativeQueryParams,
			ExpectedActorId,
			ExpectedComponentId);
		ExpectComponentQueryParamsParity(
			*TestRunner,
			ScriptComponentQueryParams,
			NativeComponentQueryParams,
			ExpectedActorId,
			ExpectedComponentId);

		ASSERT_THAT(AreEqual(
			NativeObjectQueryParams.GetQueryBitfield64(),
			ScriptObjectQueryParams.GetQueryBitfield64(),
			TEXT("CollisionQueryParamsBehaviour should preserve the object-query bitfield")));
		ASSERT_THAT(AreEqual(
			NativeObjectQueryParams.GetObjectTypesToQuery(),
			ScriptObjectQueryParams.GetObjectTypesToQuery(),
			TEXT("CollisionQueryParamsBehaviour should preserve ObjectTypesToQuery")));
		ASSERT_THAT(AreEqual(
			NativeObjectQueryParams.IgnoreMask,
			ScriptObjectQueryParams.IgnoreMask,
			TEXT("CollisionQueryParamsBehaviour should preserve object-query IgnoreMask")));
		ASSERT_THAT(AreEqual(
			NativeObjectQueryParams.IsValid(),
			ScriptObjectQueryParams.IsValid(),
			TEXT("CollisionQueryParamsBehaviour should preserve object-query validity")));

		ASSERT_THAT(IsTrue(
			ScriptResponseContainer == NativeResponseContainer,
			TEXT("CollisionQueryParamsBehaviour should preserve the response container state")));
		ASSERT_THAT(AreEqual(
			NativeResponseContainer.GetResponse(ECC_Visibility),
			ScriptResponseContainer.GetResponse(ECC_Visibility),
			TEXT("CollisionQueryParamsBehaviour should preserve Visibility response")));
		ASSERT_THAT(AreEqual(
			NativeResponseContainer.GetResponse(ECC_Camera),
			ScriptResponseContainer.GetResponse(ECC_Camera),
			TEXT("CollisionQueryParamsBehaviour should preserve Camera response")));
		ASSERT_THAT(IsTrue(
			ScriptMinResponseContainer == NativeMinResponseContainer,
			TEXT("CollisionQueryParamsBehaviour should preserve CreateMinContainer results")));
		ASSERT_THAT(AreEqual(
			NativeMinResponseContainer.GetResponse(ECC_Visibility),
			ScriptMinResponseContainer.GetResponse(ECC_Visibility),
			TEXT("CollisionQueryParamsBehaviour should preserve min Visibility response")));
		ASSERT_THAT(AreEqual(
			NativeMinResponseContainer.GetResponse(ECC_Camera),
			ScriptMinResponseContainer.GetResponse(ECC_Camera),
			TEXT("CollisionQueryParamsBehaviour should preserve min Camera response")));
		ASSERT_THAT(AreEqual(
			NativeMinResponseContainer.GetResponse(ECC_WorldStatic),
			ScriptMinResponseContainer.GetResponse(ECC_WorldStatic),
			TEXT("CollisionQueryParamsBehaviour should preserve min WorldStatic response")));
	}
};

#endif
