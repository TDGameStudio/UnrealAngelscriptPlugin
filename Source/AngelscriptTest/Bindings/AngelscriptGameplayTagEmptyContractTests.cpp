// ============================================================================
// AngelscriptGameplayTagEmptyContractTests.cpp
//
// FGameplayTag empty-tag contract coverage — CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Bindings.GameplayTagEmptyContract.FAngelscriptGameplayTagEmptyContractTest.*
//
// Sections:
//   ValidTagSanity     — ValidTag self-consistency (MatchesTagExact, MatchesAnyExact,
//                        GetSingleTagContainer)
//   EmptyTagMatching   — EmptyTag.MatchesTag/MatchesTagExact/MatchesTagDepth/
//                        MatchesAny/MatchesAnyExact vs valid tag
//   EmptyTagSingle     — EmptyTag.GetSingleTagContainer state parity
//   EmptyTagParent     — EmptyTag.RequestDirectParent and GetGameplayTagParents parity
//
// CQTest adaptation notes:
//   Native C++ reference values are captured from the first valid gameplay tag
//   at test time and substituted into script via FString::Format.
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptBindingsModuleBuilder.h"
#include "Shared/AngelscriptBindingsAssertions.h"

#include "GameplayTagsManager.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptTestBindings;

// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------

namespace AngelscriptGameplayTagEmptyContractTests_Private
{
	int32 BoolToScriptInt(const bool bValue)
	{
		return bValue ? 1 : 0;
	}

	bool FindValidGameplayTag(FGameplayTag& OutValidTag)
	{
		FGameplayTagContainer AllTags;
		UGameplayTagsManager::Get().RequestAllGameplayTags(AllTags, false);
		if (AllTags.Num() == 0)
		{
			return false;
		}

		OutValidTag = AllTags.First();
		return OutValidTag.IsValid();
	}

	struct FEmptyTagReference
	{
		int32 MatchesTag = 0;
		int32 MatchesTagExact = 0;
		int32 MatchesTagDepth = 0;
		int32 MatchesAny = 0;
		int32 MatchesAnyExact = 0;
		int32 SingleIsEmpty = 0;
		int32 SingleNum = 0;
		int32 SingleHasEmpty = 0;
		int32 SingleEqualsEmptyContainer = 0;
		int32 ParentIsValid = 0;
		int32 ParentEqualsEmpty = 0;
		int32 ParentsIsEmpty = 0;
		int32 ParentsNum = 0;
		int32 ParentsHasEmpty = 0;
		int32 ParentsHasValid = 0;
		int32 ParentsEqualsEmptyContainer = 0;
	};

	FEmptyTagReference CaptureEmptyTagReference(const FGameplayTag& ValidTag)
	{
		FGameplayTagContainer ValidContainer;
		ValidContainer.AddTag(ValidTag);

		const FGameplayTag EmptyTag;
		const FGameplayTagContainer SingleContainer = EmptyTag.GetSingleTagContainer();
		const FGameplayTag DirectParent = EmptyTag.RequestDirectParent();
		const FGameplayTagContainer ParentChain = EmptyTag.GetGameplayTagParents();

		FEmptyTagReference Ref;
		Ref.MatchesTag = BoolToScriptInt(EmptyTag.MatchesTag(ValidTag));
		Ref.MatchesTagExact = BoolToScriptInt(EmptyTag.MatchesTagExact(ValidTag));
		Ref.MatchesTagDepth = EmptyTag.MatchesTagDepth(ValidTag);
		Ref.MatchesAny = BoolToScriptInt(EmptyTag.MatchesAny(ValidContainer));
		Ref.MatchesAnyExact = BoolToScriptInt(EmptyTag.MatchesAnyExact(ValidContainer));
		Ref.SingleIsEmpty = BoolToScriptInt(SingleContainer.IsEmpty());
		Ref.SingleNum = SingleContainer.Num();
		Ref.SingleHasEmpty = BoolToScriptInt(SingleContainer.HasTagExact(FGameplayTag::EmptyTag));
		Ref.SingleEqualsEmptyContainer = BoolToScriptInt(SingleContainer == FGameplayTagContainer::EmptyContainer);
		Ref.ParentIsValid = BoolToScriptInt(DirectParent.IsValid());
		Ref.ParentEqualsEmpty = BoolToScriptInt(DirectParent == FGameplayTag::EmptyTag);
		Ref.ParentsIsEmpty = BoolToScriptInt(ParentChain.IsEmpty());
		Ref.ParentsNum = ParentChain.Num();
		Ref.ParentsHasEmpty = BoolToScriptInt(ParentChain.HasTagExact(FGameplayTag::EmptyTag));
		Ref.ParentsHasValid = BoolToScriptInt(ParentChain.HasTagExact(ValidTag));
		Ref.ParentsEqualsEmptyContainer = BoolToScriptInt(ParentChain == FGameplayTagContainer::EmptyContainer);
		return Ref;
	}

	FString BuildEmptyContractScript(const FString& ValidTagName, const FEmptyTagReference& Ref)
	{
		FStringFormatOrderedArguments Args;
		Args.Add(ValidTagName);                     // {0}
		Args.Add(Ref.MatchesTag);                   // {1}
		Args.Add(Ref.MatchesTagExact);              // {2}
		Args.Add(Ref.MatchesTagDepth);              // {3}
		Args.Add(Ref.MatchesAny);                   // {4}
		Args.Add(Ref.MatchesAnyExact);              // {5}
		Args.Add(Ref.SingleIsEmpty);                // {6}
		Args.Add(Ref.SingleNum);                    // {7}
		Args.Add(Ref.SingleHasEmpty);               // {8}
		Args.Add(Ref.SingleEqualsEmptyContainer);   // {9}
		Args.Add(Ref.ParentIsValid);                // {10}
		Args.Add(Ref.ParentEqualsEmpty);            // {11}
		Args.Add(Ref.ParentsIsEmpty);               // {12}
		Args.Add(Ref.ParentsNum);                   // {13}
		Args.Add(Ref.ParentsHasEmpty);              // {14}
		Args.Add(Ref.ParentsHasValid);              // {15}
		Args.Add(Ref.ParentsEqualsEmptyContainer);  // {16}

		return FString::Format(TEXT(R"(
int ValidTag_SelfConsistency()
{
	FGameplayTag ValidTag = FGameplayTag::RequestGameplayTag(FName("{0}"), true);
	if (!ValidTag.IsValid()) return 0;

	FGameplayTagContainer ValidContainer;
	ValidContainer.AddTag(ValidTag);

	if (!ValidTag.MatchesTagExact(ValidTag)) return 0;
	if (!ValidTag.MatchesAnyExact(ValidContainer)) return 0;
	if (!ValidTag.GetSingleTagContainer().HasTagExact(ValidTag)) return 0;
	return 1;
}

int EmptyTag_MatchesParity()
{
	FGameplayTag EmptyTag;
	FGameplayTag ValidTag = FGameplayTag::RequestGameplayTag(FName("{0}"), true);
	if (!ValidTag.IsValid()) return 0;

	FGameplayTagContainer ValidContainer;
	ValidContainer.AddTag(ValidTag);

	if ((EmptyTag.MatchesTag(ValidTag) ? 1 : 0) != {1}) return 0;
	if ((EmptyTag.MatchesTagExact(ValidTag) ? 1 : 0) != {2}) return 0;
	if (EmptyTag.MatchesTagDepth(ValidTag) != {3}) return 0;
	if ((EmptyTag.MatchesAny(ValidContainer) ? 1 : 0) != {4}) return 0;
	if ((EmptyTag.MatchesAnyExact(ValidContainer) ? 1 : 0) != {5}) return 0;
	return 1;
}

int EmptyTag_SingleContainerParity()
{
	FGameplayTag EmptyTag;
	FGameplayTag ValidTag = FGameplayTag::RequestGameplayTag(FName("{0}"), true);

	FGameplayTagContainer SingleContainer = EmptyTag.GetSingleTagContainer();
	if ((SingleContainer.IsEmpty() ? 1 : 0) != {6}) return 0;
	if (SingleContainer.Num() != {7}) return 0;
	if ((SingleContainer.HasTagExact(FGameplayTag::EmptyTag) ? 1 : 0) != {8}) return 0;
	if ((SingleContainer == FGameplayTagContainer::EmptyContainer ? 1 : 0) != {9}) return 0;
	if (SingleContainer.HasTagExact(ValidTag)) return 0;
	return 1;
}

int EmptyTag_ParentParity()
{
	FGameplayTag EmptyTag;
	FGameplayTag ValidTag = FGameplayTag::RequestGameplayTag(FName("{0}"), true);

	FGameplayTag DirectParent = EmptyTag.RequestDirectParent();
	if ((DirectParent.IsValid() ? 1 : 0) != {10}) return 0;
	if ((DirectParent == FGameplayTag::EmptyTag ? 1 : 0) != {11}) return 0;
	if (DirectParent == ValidTag) return 0;

	FGameplayTagContainer ParentChain = EmptyTag.GetGameplayTagParents();
	if ((ParentChain.IsEmpty() ? 1 : 0) != {12}) return 0;
	if (ParentChain.Num() != {13}) return 0;
	if ((ParentChain.HasTagExact(FGameplayTag::EmptyTag) ? 1 : 0) != {14}) return 0;
	if ((ParentChain.HasTagExact(ValidTag) ? 1 : 0) != {15}) return 0;
	if ((ParentChain == FGameplayTagContainer::EmptyContainer ? 1 : 0) != {16}) return 0;
	return 1;
}
)"), Args);
	}
}


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptGameplayTagEmptyContractTest,
	"Angelscript.TestModule.Bindings.GameplayTagEmptyContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: EmptyTagContracts
	// ====================================================================

	TEST_METHOD(EmptyTagContracts)
	{
		using namespace AngelscriptGameplayTagEmptyContractTests_Private;
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FGameplayTag ValidTag;
		if (!TestRunner->TestTrue(
			TEXT("[GPTagEmpty] requires at least one registered gameplay tag"),
			FindValidGameplayTag(ValidTag)))
		{
			return;
		}

		const FEmptyTagReference Ref = CaptureEmptyTagReference(ValidTag);
		const FString Script = BuildEmptyContractScript(
			ValidTag.ToString().ReplaceCharWithEscapedChar(),
			Ref);

		FCoverageModuleScope Mod(*TestRunner, Engine, TEXT("ASGameplayTagEmpty_EmptyTagContracts"), Script);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		AngelscriptTestBindings::ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int ValidTag_SelfConsistency()"), TEXT("valid tag self-consistency sanity check"), 1);
		AngelscriptTestBindings::ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int EmptyTag_MatchesParity()"), TEXT("empty tag Matches* parity with native"), 1);
		AngelscriptTestBindings::ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int EmptyTag_SingleContainerParity()"), TEXT("empty tag GetSingleTagContainer parity"), 1);
		AngelscriptTestBindings::ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int EmptyTag_ParentParity()"), TEXT("empty tag parent chain parity"), 1);
	}
};

#endif
