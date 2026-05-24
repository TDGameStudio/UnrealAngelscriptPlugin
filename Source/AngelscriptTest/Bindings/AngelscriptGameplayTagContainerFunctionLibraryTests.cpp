// ============================================================================
// AngelscriptGameplayTagContainerFunctionLibraryTests.cpp
//
// FGameplayTagContainer.RemoveTag binding coverage — CQTest refactor.
// Automation IDs:
//   Angelscript.TestModule.FunctionLibraries.GameplayTagContainer.FAngelscriptGameplayTagContainerFunctionLibraryTest.*
//
// Sections:
//   RemoveTagMiss — RemoveTag with missing tag, empty tag, and present tag;
//                   verifies return value, Num, HasTagExact, and IsEmpty parity
//                   against native C++ reference values
//
// CQTest adaptation notes:
//   Native C++ reference values are captured at test time from two unrelated
//   gameplay tags and substituted into script via FString::Format.
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestModuleScope.h"
#include "Shared/AngelscriptBindingsAssertions.h"

#include "GameplayTagsManager.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------

namespace AngelscriptGameplayTagContainerFunctionLibraryTests_Private
{
	int32 BoolToScriptInt(const bool bValue)
	{
		return bValue ? 1 : 0;
	}

	bool FindPresentAndMissingTags(FGameplayTag& OutPresentTag, FGameplayTag& OutMissingTag)
	{
		FGameplayTagContainer AllTags;
		UGameplayTagsManager::Get().RequestAllGameplayTags(AllTags, false);

		TArray<FGameplayTag> TagArray;
		AllTags.GetGameplayTagArray(TagArray);

		for (const FGameplayTag& CandidatePresentTag : TagArray)
		{
			if (!CandidatePresentTag.IsValid())
			{
				continue;
			}

			for (const FGameplayTag& CandidateMissingTag : TagArray)
			{
				if (!CandidateMissingTag.IsValid() || CandidateMissingTag == CandidatePresentTag)
				{
					continue;
				}

				if (CandidatePresentTag.MatchesTag(CandidateMissingTag) || CandidateMissingTag.MatchesTag(CandidatePresentTag))
				{
					continue;
				}

				OutPresentTag = CandidatePresentTag;
				OutMissingTag = CandidateMissingTag;
				return true;
			}
		}

		return false;
	}

	struct FRemoveTagReference
	{
		int32 RemoveMissing = 0;
		int32 NumAfterMissing = 0;
		int32 HasPresentAfterMissing = 0;
		int32 RemoveEmpty = 0;
		int32 NumAfterEmpty = 0;
		int32 HasPresentAfterEmpty = 0;
		int32 RemovePresent = 0;
		int32 NumAfterPresent = 0;
		int32 HasPresentAfterPresent = 0;
	};

	FRemoveTagReference CaptureReference(const FGameplayTag& PresentTag, const FGameplayTag& MissingTag)
	{
		FGameplayTagContainer Container;
		Container.AddTag(PresentTag);

		FRemoveTagReference Reference;
		Reference.RemoveMissing = BoolToScriptInt(Container.RemoveTag(MissingTag));
		Reference.NumAfterMissing = Container.Num();
		Reference.HasPresentAfterMissing = BoolToScriptInt(Container.HasTagExact(PresentTag));

		Reference.RemoveEmpty = BoolToScriptInt(Container.RemoveTag(FGameplayTag::EmptyTag));
		Reference.NumAfterEmpty = Container.Num();
		Reference.HasPresentAfterEmpty = BoolToScriptInt(Container.HasTagExact(PresentTag));

		Reference.RemovePresent = BoolToScriptInt(Container.RemoveTag(PresentTag));
		Reference.NumAfterPresent = Container.Num();
		Reference.HasPresentAfterPresent = BoolToScriptInt(Container.HasTagExact(PresentTag));
		return Reference;
	}

	FString BuildRemoveTagScript(
		const FString& PresentTagName,
		const FString& MissingTagName,
		const FRemoveTagReference& Ref)
	{
		FStringFormatOrderedArguments Args;
		Args.Add(PresentTagName);   // {0}
		Args.Add(MissingTagName);   // {1}
		Args.Add(Ref.RemoveMissing);            // {2}
		Args.Add(Ref.NumAfterMissing);          // {3}
		Args.Add(Ref.HasPresentAfterMissing);   // {4}
		Args.Add(Ref.RemoveEmpty);              // {5}
		Args.Add(Ref.NumAfterEmpty);            // {6}
		Args.Add(Ref.HasPresentAfterEmpty);     // {7}
		Args.Add(Ref.RemovePresent);            // {8}
		Args.Add(Ref.NumAfterPresent);          // {9}
		Args.Add(Ref.HasPresentAfterPresent);   // {10}

		return FString::Format(TEXT(R"(
int RemoveTag_MissingParity()
{
	FGameplayTag PresentTag = FGameplayTag::RequestGameplayTag(FName("{0}"), true);
	FGameplayTag MissingTag = FGameplayTag::RequestGameplayTag(FName("{1}"), true);
	if (!PresentTag.IsValid()) return 0;
	if (!MissingTag.IsValid()) return 0;
	if (PresentTag == MissingTag) return 0;

	FGameplayTagContainer Container;
	Container.AddTag(PresentTag);

	if ((Container.RemoveTag(MissingTag) ? 1 : 0) != {2}) return 0;
	if (Container.Num() != {3}) return 0;
	if ((Container.HasTagExact(PresentTag) ? 1 : 0) != {4}) return 0;
	return 1;
}

int RemoveTag_EmptyParity()
{
	FGameplayTag PresentTag = FGameplayTag::RequestGameplayTag(FName("{0}"), true);
	FGameplayTag MissingTag = FGameplayTag::RequestGameplayTag(FName("{1}"), true);

	FGameplayTagContainer Container;
	Container.AddTag(PresentTag);
	Container.RemoveTag(MissingTag);

	if ((Container.RemoveTag(FGameplayTag::EmptyTag) ? 1 : 0) != {5}) return 0;
	if (Container.Num() != {6}) return 0;
	if ((Container.HasTagExact(PresentTag) ? 1 : 0) != {7}) return 0;
	return 1;
}

int RemoveTag_PresentParity()
{
	FGameplayTag PresentTag = FGameplayTag::RequestGameplayTag(FName("{0}"), true);
	FGameplayTag MissingTag = FGameplayTag::RequestGameplayTag(FName("{1}"), true);

	FGameplayTagContainer Container;
	Container.AddTag(PresentTag);
	Container.RemoveTag(MissingTag);
	Container.RemoveTag(FGameplayTag::EmptyTag);

	if ((Container.RemoveTag(PresentTag) ? 1 : 0) != {8}) return 0;
	if (Container.Num() != {9}) return 0;
	if ((Container.HasTagExact(PresentTag) ? 1 : 0) != {10}) return 0;
	if (!Container.IsEmpty()) return 0;
	return 1;
}
)"), Args);
	}
}


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptGameplayTagContainerFunctionLibraryTest,
	"Angelscript.TestModule.FunctionLibraries.GameplayTagContainer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: RemoveTagMiss
	// ====================================================================

	TEST_METHOD(RemoveTagMiss)
	{
		using namespace AngelscriptGameplayTagContainerFunctionLibraryTests_Private;
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FGameplayTag PresentTag;
		FGameplayTag MissingTag;
		if (!TestRunner->TestTrue(
			TEXT("[GPTagContainer] requires two unrelated registered gameplay tags"),
			FindPresentAndMissingTags(PresentTag, MissingTag)))
		{
			return;
		}

		const FRemoveTagReference Ref = CaptureReference(PresentTag, MissingTag);
		const FString Script = BuildRemoveTagScript(
			PresentTag.ToString().ReplaceCharWithEscapedChar(),
			MissingTag.ToString().ReplaceCharWithEscapedChar(),
			Ref);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASGameplayTagContainer_RemoveTagMiss"), Script);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int RemoveTag_MissingParity()"), TEXT("RemoveTag missing tag preserves container"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int RemoveTag_EmptyParity()"), TEXT("RemoveTag empty tag preserves container"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int RemoveTag_PresentParity()"), TEXT("RemoveTag present tag empties container"), 1);
	}
};

#endif
