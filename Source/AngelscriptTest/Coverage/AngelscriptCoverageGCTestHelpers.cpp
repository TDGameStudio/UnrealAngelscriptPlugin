#include "AngelscriptBinds.h"

#include "Engine/Engine.h"
#include "Math/UnrealMathUtility.h"
#include "UObject/GarbageCollection.h"

#if WITH_ANGELSCRIPT_UNITTESTS

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

static void BindAngelscriptCoverageGCTestHelpers(FAngelscriptBinds& Binds)
{
	FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "CoverageGC");

	Binds.BindGlobalFunctionForTarget(
		"void CollectGarbageNow()",
		FUNC_TRIVIAL(CoverageGCCollectGarbageNow));

	Binds.BindGlobalFunctionForTarget(
		"void ForceGarbageCollectionNow()",
		FUNC_TRIVIAL(CoverageGCForceGarbageCollectionNow));
}

AS_FORCE_LINK const FAngelscriptBind Bind_AngelscriptCoverageGCTestHelpers(
	TEXT("AngelscriptCoverageGCTestHelpers.PostReflection"),
	EAngelscriptBindPhase::PostReflectionBindings,
	&BindAngelscriptCoverageGCTestHelpers);

#endif // WITH_ANGELSCRIPT_UNITTESTS
