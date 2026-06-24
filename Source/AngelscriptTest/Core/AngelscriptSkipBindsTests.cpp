#include "AngelscriptBinds.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"

#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptSkipBindsTests,
	"Angelscript.TestModule.Engine.BindConfig.SkipBinds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
struct FSkipEntryExpectation
{
	const TCHAR* ClassName;
	const TCHAR* FunctionName;
	bool bExpectedSkipped;
};

struct FSkipClassExpectation
{
	const TCHAR* ClassName;
	bool bExpectedSkipped;
};

public:
	TEST_METHOD(DefaultSkipListIsRegistered)
	{
FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		{
			FAngelscriptEngineScope _AutoEngineScope(Engine);
			ON_SCOPE_EXIT
			{
				const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
				for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
				{
					Engine.DiscardModule(*_Module->ModuleName);
				}
			};

		const FSkipEntryExpectation EntryCases[] =
		{
			{ TEXT("StaticMesh"), TEXT("GetMinLODForQualityLevels"), true },
			{ TEXT("StaticMesh"), TEXT("SetMinLODForQualityLevels"), true },
			{ TEXT("SkeletalMesh"), TEXT("GetMinLODForQualityLevels"), true },
			{ TEXT("SkeletalMesh"), TEXT("SetMinLODForQualityLevels"), true },
			{ TEXT("SourceEffectEQPreset"), TEXT("SetSettings"), true },
			{ TEXT("StaticMesh"), TEXT("BuildNanite"), false },
		};

		const FSkipClassExpectation ClassCases[] =
		{
			{ TEXT("ClothingSimulationInteractorNv"), true },
			{ TEXT("NiagaraPreviewGrid"), true },
			{ TEXT("GameplayCamerasSubsystem"), true },
			{ TEXT("AsyncAction_PerformTargeting"), true },
			{ TEXT("Actor"), false },
		};

		for (const FSkipEntryExpectation& EntryCase : EntryCases)
		{
			const bool bFirstRead = FAngelscriptBinds::CheckForSkipEntry(EntryCase.ClassName, EntryCase.FunctionName);
			const bool bSecondRead = FAngelscriptBinds::CheckForSkipEntry(EntryCase.ClassName, EntryCase.FunctionName);
			const FString EntryLabel = FString::Printf(TEXT("%s.%s"), EntryCase.ClassName, EntryCase.FunctionName);

			ASSERT_THAT(AreEqual(
				EntryCase.bExpectedSkipped,
				bFirstRead,
				*FString::Printf(TEXT("SkipBinds default list should return the expected registration state for %s"), *EntryLabel)));
			ASSERT_THAT(AreEqual(
				bFirstRead,
				bSecondRead,
				*FString::Printf(TEXT("SkipBinds repeated reads should keep the same result for %s"), *EntryLabel)));
		}

		for (const FSkipClassExpectation& ClassCase : ClassCases)
		{
			const bool bFirstRead = FAngelscriptBinds::CheckForSkipClass(ClassCase.ClassName);
			const bool bSecondRead = FAngelscriptBinds::CheckForSkipClass(ClassCase.ClassName);

			ASSERT_THAT(AreEqual(
				ClassCase.bExpectedSkipped,
				bFirstRead,
				*FString::Printf(TEXT("SkipBinds default list should return the expected registration state for class %s"), ClassCase.ClassName)));
			ASSERT_THAT(AreEqual(
				bFirstRead,
				bSecondRead,
				*FString::Printf(TEXT("SkipBinds repeated reads should keep the same result for class %s"), ClassCase.ClassName)));
		}

		}
	}
};

#endif
