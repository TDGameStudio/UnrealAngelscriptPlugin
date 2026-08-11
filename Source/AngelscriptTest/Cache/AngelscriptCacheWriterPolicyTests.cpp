#include "Cache/AngelscriptCacheManifestPack.h"
#include "Cache/AngelscriptCacheService.h"
#include "Core/AngelscriptEngine.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(
	FAngelscriptCacheWriterPolicyTests,
	"Angelscript.TestModule.Cache.WriterPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(DefaultProjectPolicyResolvesTo64MiBBoundedFourWorkers)
	{
		const FAngelscriptEngineConfig Config;
		const FAngelscriptCachePackPolicy Policy =
			ResolveAngelscriptCacheWriterPolicy(Config, 64, true, 4);
		ASSERT_THAT(AreEqual(UINT64_C(64) * 1024 * 1024,
			Policy.TargetRawBytesPerPack));
		ASSERT_THAT(AreEqual(
			EAngelscriptCachePreparationExecutionMode::BoundedParallel,
			Policy.ExecutionMode));
		ASSERT_THAT(AreEqual(4u, Policy.MaxWorkerCount));
	}

	TEST_METHOD(ProcessOverridesClampTargetWorkersAndForceSerial)
	{
		FAngelscriptEngineConfig Config;
		Config.CacheV2PackTargetMiBOverride = 4096;
		Config.CacheV2PreparationWorkerCountOverride = 4096;
		Config.bForceSerialCacheV2Preparation = true;
		const FAngelscriptCachePackPolicy Policy =
			ResolveAngelscriptCacheWriterPolicy(Config, 16, true, 8);
		ASSERT_THAT(AreEqual(UINT64_C(256) * 1024 * 1024,
			Policy.TargetRawBytesPerPack));
		ASSERT_THAT(AreEqual(
			EAngelscriptCachePreparationExecutionMode::ForcedSerial,
			Policy.ExecutionMode));
		ASSERT_THAT(AreEqual(1u, Policy.MaxWorkerCount));
	}

	TEST_METHOD(ServiceOwnsAnImmutablePolicyCopy)
	{
		FAngelscriptCachePackPolicy Configured;
		Configured.TargetRawBytesPerPack = UINT64_C(4) * 1024 * 1024;
		Configured.ExecutionMode =
			EAngelscriptCachePreparationExecutionMode::BoundedParallel;
		Configured.MaxWorkerCount = 2;
		FAngelscriptCacheService Service;
		Service.ConfigureWriterPolicy(Configured);
		Configured.TargetRawBytesPerPack = 1;
		Configured.MaxWorkerCount = 1;

		const FAngelscriptCachePackPolicy Captured =
			Service.CaptureWriterPolicy();
		ASSERT_THAT(AreEqual(UINT64_C(4) * 1024 * 1024,
			Captured.TargetRawBytesPerPack));
		ASSERT_THAT(AreEqual(
			EAngelscriptCachePreparationExecutionMode::BoundedParallel,
			Captured.ExecutionMode));
		ASSERT_THAT(AreEqual(2u, Captured.MaxWorkerCount));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
