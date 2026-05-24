// ============================================================================
// AngelscriptCollisionParamsBindingsTests.cpp
//
// Collision params binding coverage — CQTest refactor. Automation ID:
//   Angelscript.TestModule.Bindings.CollisionParams.FAngelscriptCollisionParamsBindingsTest.*
//
// Sections:
//   CollisionQueryParamsBehaviour — full parity test for FCollisionQueryParams,
//     FComponentQueryParams, FCollisionObjectQueryParams, FCollisionResponseContainer
//
// CQTest adaptation notes:
//   One IMPLEMENT_SIMPLE_AUTOMATION_TEST merged into one TEST_CLASS.
//   This test retains its custom execution pattern (parameterised function with
//   out-ref arguments) because the script populates multiple struct outputs that
//   are compared against native equivalents. The original helper namespace is
//   preserved intact.
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestModuleScope.h"
#include "Shared/AngelscriptTestExecute.h"
#include "Bindings/AngelscriptWorldCollisionBindingsTestHelpers.h"

#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "Templates/Function.h"

#if WITH_DEV_AUTOMATION_TESTS



namespace AngelscriptTest_Bindings_AngelscriptCollisionParamsBindingsTests_Private
{
	static constexpr ANSICHAR CollisionParamsModuleName[] = "ASCollisionQueryParamsBehaviour";

	template <typename IdArrayType>
	TArray<uint32> CopyIgnoredIds(const IdArrayType& Source)
	{
		TArray<uint32> Result;
		Result.Reserve(Source.Num());
		for (uint32 Id : Source)
		{
			Result.Add(Id);
		}
		return Result;
	}

	FCollisionQueryParams BuildNativeCollisionQueryParams(const AActor* IgnoredActor, const UPrimitiveComponent* IgnoredComponent)
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

	FComponentQueryParams BuildNativeComponentQueryParams(const AActor* IgnoredActor, const UPrimitiveComponent* IgnoredComponent)
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

	FCollisionObjectQueryParams BuildNativeObjectQueryParams()
	{
		FCollisionObjectQueryParams QueryParams;
		QueryParams.IgnoreMask = 29;
		QueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
		QueryParams.AddObjectTypesToQuery(ECC_Camera);
		QueryParams.AddObjectTypesToQuery(ECC_Pawn);
		QueryParams.RemoveObjectTypesToQuery(ECC_Pawn);
		return QueryParams;
	}

	FCollisionResponseContainer BuildNativeResponseContainer()
	{
		FCollisionResponseContainer ResponseContainer(ECR_Ignore);
		ResponseContainer.SetResponse(ECC_Visibility, ECR_Block);
		ResponseContainer.SetResponse(ECC_Camera, ECR_Overlap);
		return ResponseContainer;
	}

	FCollisionResponseContainer BuildNativeMinResponseContainer()
	{
		FCollisionResponseContainer ResponseContainer = BuildNativeResponseContainer();

		FCollisionResponseContainer OtherContainer(ECR_Block);
		OtherContainer.SetResponse(ECC_Visibility, ECR_Overlap);
		OtherContainer.SetResponse(ECC_WorldStatic, ECR_Ignore);

		return FCollisionResponseContainer::CreateMinContainer(ResponseContainer, OtherContainer);
	}

	bool ExpectSingleIgnoredId(
		FAutomationTestBase& Test,
		const TCHAR* ContextLabel,
		const TArray<uint32>& ActualIds,
		const uint32 ExpectedId)
	{
		bool bPassed = true;
		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("%s should contain exactly one ignored entry"), ContextLabel),
			ActualIds.Num(),
			1);

		if (ActualIds.Num() == 1)
		{
			bPassed &= Test.TestEqual(
				*FString::Printf(TEXT("%s should preserve the ignored object ID"), ContextLabel),
				ActualIds[0],
				ExpectedId);
		}

		return bPassed;
	}

	bool ExpectCollisionQueryParamsParity(
		FAutomationTestBase& Test,
		const TCHAR* ContextLabel,
		const FCollisionQueryParams& ScriptParams,
		const FCollisionQueryParams& NativeParams,
		const uint32 ExpectedActorId,
		const uint32 ExpectedComponentId)
	{
		bool bPassed = true;
		bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s should preserve TraceTag"), ContextLabel), ScriptParams.TraceTag, NativeParams.TraceTag);
		bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s should preserve OwnerTag"), ContextLabel), ScriptParams.OwnerTag, NativeParams.OwnerTag);
		bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s should preserve bTraceComplex"), ContextLabel), ScriptParams.bTraceComplex, NativeParams.bTraceComplex);
		bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s should preserve bFindInitialOverlaps"), ContextLabel), ScriptParams.bFindInitialOverlaps, NativeParams.bFindInitialOverlaps);
		bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s should preserve bReturnFaceIndex"), ContextLabel), ScriptParams.bReturnFaceIndex, NativeParams.bReturnFaceIndex);
		bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s should preserve bReturnPhysicalMaterial"), ContextLabel), ScriptParams.bReturnPhysicalMaterial, NativeParams.bReturnPhysicalMaterial);
		bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s should preserve bIgnoreBlocks"), ContextLabel), ScriptParams.bIgnoreBlocks, NativeParams.bIgnoreBlocks);
		bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s should preserve bIgnoreTouches"), ContextLabel), ScriptParams.bIgnoreTouches, NativeParams.bIgnoreTouches);
		bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s should preserve bSkipNarrowPhase"), ContextLabel), ScriptParams.bSkipNarrowPhase, NativeParams.bSkipNarrowPhase);
		bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s should preserve MobilityType"), ContextLabel), ScriptParams.MobilityType, NativeParams.MobilityType);
		bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s should preserve IgnoreMask"), ContextLabel), ScriptParams.IgnoreMask, NativeParams.IgnoreMask);
		bPassed &= ExpectSingleIgnoredId(Test, *FString::Printf(TEXT("%s ignored actors"), ContextLabel), CopyIgnoredIds(ScriptParams.GetIgnoredSourceObjects()), ExpectedActorId);
		bPassed &= ExpectSingleIgnoredId(Test, *FString::Printf(TEXT("%s ignored components"), ContextLabel), CopyIgnoredIds(ScriptParams.GetIgnoredComponents()), ExpectedComponentId);
		return bPassed;
	}

	bool ExpectComponentQueryParamsParity(
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
		bPassed &= Test.TestEqual(
			TEXT("CollisionQueryParamsBehaviour should preserve ShapeCollisionMask.Bits"),
			ScriptParams.ShapeCollisionMask.Bits,
			NativeParams.ShapeCollisionMask.Bits);
		return bPassed;
	}
}


TEST_CLASS_WITH_FLAGS(FAngelscriptCollisionParamsBindingsTest, "Angelscript.TestModule.Bindings.CollisionParams",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	TEST_METHOD(CollisionQueryParamsBehaviour)
	{
		using namespace AngelscriptTest_Bindings_AngelscriptCollisionParamsBindingsTests_Private;
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASCollisionParams_Behaviour"), TEXT(R"AS(
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
		Failures |= 1;
	if (QueryParams.GetIgnoredComponents().Num() != 1)
		Failures |= 2;
	QueryParams.ClearIgnoredActors();
	QueryParams.ClearIgnoredComponents();
	if (QueryParams.GetIgnoredActors().Num() != 0)
		Failures |= 4;
	if (QueryParams.GetIgnoredComponents().Num() != 0)
		Failures |= 8;
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
		Failures |= 16;
	if (ComponentQueryParams.GetIgnoredComponents().Num() != 1)
		Failures |= 32;
	ComponentQueryParams.ClearIgnoredActors();
	ComponentQueryParams.ClearIgnoredComponents();
	if (ComponentQueryParams.GetIgnoredActors().Num() != 0)
		Failures |= 64;
	if (ComponentQueryParams.GetIgnoredComponents().Num() != 0)
		Failures |= 128;
	ComponentQueryParams.AddIgnoredActor(IgnoredActor);
	ComponentQueryParams.AddIgnoredComponent(IgnoredComponent);

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.IgnoreMask = 29;
	ObjectQueryParams.AddObjectTypesToQuery(ECollisionChannel::ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECollisionChannel::ECC_Camera);
	ObjectQueryParams.AddObjectTypesToQuery(ECollisionChannel::ECC_Pawn);
	ObjectQueryParams.RemoveObjectTypesToQuery(ECollisionChannel::ECC_Pawn);
	if (!ObjectQueryParams.IsValid())
		Failures |= 256;

	FCollisionResponseContainer ResponseContainer(ECollisionResponse::ECR_Ignore);
	if (!ResponseContainer.SetResponse(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block))
		Failures |= 512;
	if (!ResponseContainer.SetResponse(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Overlap))
		Failures |= 1024;

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
		if (!TestRunner->TestNotNull(TEXT("CollisionQueryParamsBehaviour should create a transient actor"), TestActor)
			|| !TestRunner->TestNotNull(TEXT("CollisionQueryParamsBehaviour should create a transient primitive component"), TestComponent))
		{
			return;
		}

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

		TestRunner->TestEqual(
			TEXT("CollisionQueryParamsBehaviour should preserve script-side ignored-list and response mutation checks"),
			ResultMask,
			0);

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

		TestRunner->TestEqual(
			TEXT("CollisionQueryParamsBehaviour should preserve the object-query bitfield"),
			ScriptObjectQueryParams.GetQueryBitfield(),
			NativeObjectQueryParams.GetQueryBitfield());
		TestRunner->TestEqual(
			TEXT("CollisionQueryParamsBehaviour should preserve ObjectTypesToQuery"),
			ScriptObjectQueryParams.ObjectTypesToQuery,
			NativeObjectQueryParams.ObjectTypesToQuery);
		TestRunner->TestEqual(
			TEXT("CollisionQueryParamsBehaviour should preserve object-query IgnoreMask"),
			ScriptObjectQueryParams.IgnoreMask,
			NativeObjectQueryParams.IgnoreMask);
		TestRunner->TestEqual(
			TEXT("CollisionQueryParamsBehaviour should preserve object-query validity"),
			ScriptObjectQueryParams.IsValid(),
			NativeObjectQueryParams.IsValid());

		TestRunner->TestTrue(
			TEXT("CollisionQueryParamsBehaviour should preserve the response container state"),
			ScriptResponseContainer == NativeResponseContainer);
		TestRunner->TestEqual(
			TEXT("CollisionQueryParamsBehaviour should preserve Visibility response"),
			ScriptResponseContainer.GetResponse(ECC_Visibility),
			NativeResponseContainer.GetResponse(ECC_Visibility));
		TestRunner->TestEqual(
			TEXT("CollisionQueryParamsBehaviour should preserve Camera response"),
			ScriptResponseContainer.GetResponse(ECC_Camera),
			NativeResponseContainer.GetResponse(ECC_Camera));
		TestRunner->TestTrue(
			TEXT("CollisionQueryParamsBehaviour should preserve CreateMinContainer results"),
			ScriptMinResponseContainer == NativeMinResponseContainer);
		TestRunner->TestEqual(
			TEXT("CollisionQueryParamsBehaviour should preserve min Visibility response"),
			ScriptMinResponseContainer.GetResponse(ECC_Visibility),
			NativeMinResponseContainer.GetResponse(ECC_Visibility));
		TestRunner->TestEqual(
			TEXT("CollisionQueryParamsBehaviour should preserve min Camera response"),
			ScriptMinResponseContainer.GetResponse(ECC_Camera),
			NativeMinResponseContainer.GetResponse(ECC_Camera));
		TestRunner->TestEqual(
			TEXT("CollisionQueryParamsBehaviour should preserve min WorldStatic response"),
			ScriptMinResponseContainer.GetResponse(ECC_WorldStatic),
			NativeMinResponseContainer.GetResponse(ECC_WorldStatic));
	}
};

#endif
