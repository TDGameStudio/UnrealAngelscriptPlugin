#pragma once

#include "Shared/AngelscriptFunctionalTestUtils.h"
#include "Shared/AngelscriptReflectiveAccess.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestWorld.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptActorTestUtils
{
	using namespace AngelscriptFunctionalTestUtils;

	constexpr float DefaultActorTestDeltaTime = 0.016f;
	constexpr int32 DefaultActorTestTickCount = 3;

	inline void EnableActorTick(AActor& Actor)
	{
		Actor.PrimaryActorTick.bCanEverTick = true;
		Actor.SetActorTickEnabled(true);
		Actor.RegisterAllActorTickFunctions(true, false);
	}

	inline void TickWorldThroughTickManager(
		FAngelscriptEngine& Engine,
		UWorld& World,
		float DeltaTime,
		int32 NumTicks)
	{
		for (int32 TickIndex = 0; TickIndex < NumTicks; ++TickIndex)
		{
			FAngelscriptEngineScope WorldScope(Engine);
			World.Tick(ELevelTick::LEVELTICK_All, DeltaTime);
		}
	}

	inline FAngelscriptEngine& AcquireFreshActorEngine()
	{
		DestroySharedAndStrayGlobalTestEngine();
		return AcquireCleanSharedCloneEngine();
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
