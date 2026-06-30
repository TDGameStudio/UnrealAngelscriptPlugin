#include "AngelscriptBinds.h"

#include "Engine/Engine.h"
#include "Math/UnrealMathUtility.h"
#include "UObject/GarbageCollection.h"

#if WITH_DEV_AUTOMATION_TESTS

static void CoverageGCCollectGarbageNow()
{
	CollectGarbage(RF_NoFlags, true);
}

static void CoverageGCForceGarbageCollectionNow()
{
	if (GEngine == nullptr)
	{
		CoverageGCCollectGarbageNow();
		return;
	}

	GEngine->ForceGarbageCollection(true);

	const uint64 LastGFrameCounter = GFrameCounter;
	do
	{
		GFrameCounter = FMath::Rand();
	} while (GFrameCounter == LastGFrameCounter);

	GEngine->ConditionalCollectGarbage();
	GFrameCounter = LastGFrameCounter;
}

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_AngelscriptCoverageGCTestHelpers(
	TEXT("AngelscriptCoverageGCTestHelpers"),
	(int32)FAngelscriptBinds::EOrder::Late + 101,
	[]
	{
		FAngelscriptBinds::FNamespace Namespace("CoverageGC");

		FAngelscriptBinds::BindGlobalFunction(
			"void CollectGarbageNow()",
			FUNC_TRIVIAL(CoverageGCCollectGarbageNow));

		FAngelscriptBinds::BindGlobalFunction(
			"void ForceGarbageCollectionNow()",
			FUNC_TRIVIAL(CoverageGCForceGarbageCollectionNow));
	});

#endif // WITH_DEV_AUTOMATION_TESTS
