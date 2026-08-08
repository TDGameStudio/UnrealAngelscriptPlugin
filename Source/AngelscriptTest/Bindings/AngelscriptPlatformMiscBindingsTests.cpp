// =============================================================================
// AngelscriptPlatformMiscBindingsTests.cpp
//
// CQTest coverage for FGenericPlatformMisc, CoreGlobals, SystemTimers,
// ConfigEnums bindings.
// Automation IDs: Angelscript.TestModule.Bindings.PlatformMisc.*
// =============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#include "Engine/CollisionProfile.h"
#include "Engine/EngineTypes.h"

#if WITH_ANGELSCRIPT_UNITTESTS



TEST_CLASS_WITH_FLAGS(FAngelscriptPlatformMiscBindingsTest,
	"Angelscript.TestModule.Bindings.PlatformMisc",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(CoreGlobals)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ExpectedFragments[] = {
			TEXT("GIsEditor"),
		};
		const FString CoreGlobalsMissingSource = ASTEST_AS(R"AS(
			int IsEditor()
			{
				return GIsEditor ? 1 : 0;
			}
			)AS");
		ExpectBindingCompileFailure(
			*TestRunner,
			Engine,
			TEXT("ASPlatformMisc_PlatMiscCoreGlobalsMissing"),
			*CoreGlobalsMissingSource,
			TEXT("GIsEditor is not bound as a script-visible global on this branch"),
			MakeArrayView(ExpectedFragments));
	}

	TEST_METHOD(PlatformMisc)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ExpectedFragments[] = {
			TEXT("NumberOfCores"),
			TEXT("FGenericPlatformMisc"),
		};
		const FString PlatformMiscMissingSource = ASTEST_AS(R"AS(
			int GetNumCores()
			{
				return FGenericPlatformMisc::NumberOfCores();
			}

			int GetNumCoresIncludingHyperthreads()
			{
				return FGenericPlatformMisc::NumberOfCoresIncludingHyperthreads();
			}
			)AS");
		ExpectBindingCompileFailure(
			*TestRunner,
			Engine,
			TEXT("ASPlatformMisc_PlatMiscNumberOfCoresMissing"),
			*PlatformMiscMissingSource,
			TEXT("FGenericPlatformMisc currently exposes RequestExit only; core-count helpers remain an explicit unsupported binding boundary"),
			MakeArrayView(ExpectedFragments));
	}

	TEST_METHOD(ConfigEnums)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		UCollisionProfile* CollisionProfile = UCollisionProfile::Get();
		ASSERT_THAT(IsNotNull(CollisionProfile, TEXT("Collision profile singleton should be available")));

		const int32 VisibilityTraceType = static_cast<int32>(CollisionProfile->ConvertToTraceType(ECC_Visibility));
		const int32 WorldStaticObjectType = static_cast<int32>(CollisionProfile->ConvertToObjectType(ECC_WorldStatic));
		const FString Source = ASTEST_AS(R"AS(
			int ConfigEnum_Visibility()
			{
				return int(ETraceTypeQuery::Visibility);
			}

			int ConfigEnum_WorldStatic()
			{
				return int(EObjectTypeQuery::WorldStatic);
			}
			)AS");

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASPlatformMisc_ConfigEnums"), Source);
		if (!Module.IsValid())
		{
			return;
		}

		auto& ScriptModule = Module.GetModule();
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, ScriptModule, TEXT("int ConfigEnum_Visibility()"), TEXT("ETraceTypeQuery::Visibility should match the project collision profile"), VisibilityTraceType),
			TEXT("Visibility config enum should execute")));
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, ScriptModule, TEXT("int ConfigEnum_WorldStatic()"), TEXT("EObjectTypeQuery::WorldStatic should match the project collision profile"), WorldStaticObjectType),
			TEXT("WorldStatic config enum should execute")));
	}

	TEST_METHOD(SystemTimers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString ExpectedFragments[] = {
			TEXT("GetCurrentTime"),
			TEXT("GetDeltaTime"),
			TEXT("FApp"),
		};
		const FString TimersMissingSource = ASTEST_AS(R"AS(
			int Seconds_Positive()
			{
				double S = FApp::GetCurrentTime();
				return (S > 0.0) ? 1 : 0;
			}

			int DeltaTime_NonNegative()
			{
				float DT = FApp::GetDeltaTime();
				return (DT >= 0.0) ? 1 : 0;
			}
			)AS");
		ExpectBindingCompileFailure(
			*TestRunner,
			Engine,
			TEXT("ASPlatformMisc_TimersMissing"),
			*TimersMissingSource,
			TEXT("FApp time helpers are not bound on the current branch"),
			MakeArrayView(ExpectedFragments));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
