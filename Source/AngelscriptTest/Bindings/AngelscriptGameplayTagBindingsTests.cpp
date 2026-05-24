// ============================================================================
// AngelscriptGameplayTagBindingsTests.cpp
//
// Gameplay tag binding coverage — CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Bindings.GameplayTag.FAngelscriptGameplayTagBindingsTest.*
//
// Sections:
//   GameplayTagCompat                    — FGameplayTag construction, empty tag, request
//   GameplayTagContainerCompat           — FGameplayTagContainer add/remove/query
//   GameplayTagContainerEmptyContracts   — empty-vs-non-empty container semantics
//   GameplayTagQueryCompat               — FGameplayTagQuery factory methods and matching
//   GameplayTagExactQueryCompat          — exact tag query and tag name accessors
//   GameplayTagNamespaceGlobals          — GameplayTags:: namespace global bindings
//   GameplayTagHierarchySemantics        — parent/child tag matching helpers
//   GameplayTagContainerHierarchyFilters — container filter/leaf/parents operations
//
// CQTest adaptation notes:
//   - Tag names are runtime-dependent; C++ computes them and injects via ReplaceInline.
//   - Entry() functions split into individual 1/0 functions where the logic
//     allows independent evaluation; sequential-state tests keep a single function.
//   - Static helper functions (FindGameplayTagHierarchyFixture, etc.) preserved.
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestModuleScope.h"
#include "Shared/AngelscriptTestExecute.h"
#include "Shared/AngelscriptTestUtilities.h"

#include "Binds/Bind_FGameplayTag.h"
#include "Core/AngelscriptEngine.h"
#include "GameplayTagsManager.h"

#if WITH_DEV_AUTOMATION_TESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Static helpers
// ----------------------------------------------------------------------------

namespace
{
	bool FindGameplayTagHierarchyFixture(
		FGameplayTag& OutChildTag,
		FGameplayTag& OutParentTag,
		FGameplayTag& OutUnrelatedTag)
	{
		FGameplayTagContainer AllTags;
		UGameplayTagsManager::Get().RequestAllGameplayTags(AllTags, false);

		TArray<FGameplayTag> TagArray;
		AllTags.GetGameplayTagArray(TagArray);

		for (const FGameplayTag& CandidateChildTag : TagArray)
		{
			const FGameplayTag CandidateParentTag = CandidateChildTag.RequestDirectParent();
			if (!CandidateParentTag.IsValid())
			{
				continue;
			}

			for (const FGameplayTag& CandidateUnrelatedTag : TagArray)
			{
				if (!CandidateUnrelatedTag.IsValid())
				{
					continue;
				}
				if (CandidateUnrelatedTag == CandidateChildTag || CandidateUnrelatedTag == CandidateParentTag)
				{
					continue;
				}
				if (CandidateChildTag.MatchesTag(CandidateUnrelatedTag) || CandidateUnrelatedTag.MatchesTag(CandidateChildTag))
				{
					continue;
				}
				if (CandidateParentTag.MatchesTag(CandidateUnrelatedTag) || CandidateUnrelatedTag.MatchesTag(CandidateParentTag))
				{
					continue;
				}

				OutChildTag = CandidateChildTag;
				OutParentTag = CandidateParentTag;
				OutUnrelatedTag = CandidateUnrelatedTag;
				return true;
			}
		}

		return false;
	}

	struct FGameplayTagNamespaceGlobalFixture
	{
		FGameplayTag Tag;
		FString SanitizedTagIdentifier;
		FGameplayTag ParentTag;
		FString SanitizedParentIdentifier;
	};

	FString SanitizeGameplayTagIdentifier(const FString& TagName)
	{
		FString SanitizedIdentifier = TagName;
		for (TCHAR& Character : SanitizedIdentifier)
		{
			if (!FAngelscriptEngine::IsValidIdentifierCharacter(Character))
			{
				Character = TEXT('_');
			}
		}
		return SanitizedIdentifier;
	}

	bool IsValidScriptIdentifier(const FString& Identifier)
	{
		if (Identifier.IsEmpty())
		{
			return false;
		}
		if (!(FChar::IsAlpha(Identifier[0]) || Identifier[0] == TEXT('_')))
		{
			return false;
		}
		for (int32 Index = 1; Index < Identifier.Len(); ++Index)
		{
			if (!FAngelscriptEngine::IsValidIdentifierCharacter(Identifier[Index]))
			{
				return false;
			}
		}
		return true;
	}

	bool HasGameplayTagNamespaceGlobal(FAngelscriptEngine& Engine, const FString& Identifier)
	{
		asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
		if (ScriptEngine == nullptr)
		{
			return false;
		}

		const asUINT GlobalPropertyCount = ScriptEngine->GetGlobalPropertyCount();
		for (asUINT GlobalPropertyIndex = 0; GlobalPropertyIndex < GlobalPropertyCount; ++GlobalPropertyIndex)
		{
			const char* Name = nullptr;
			const char* NamespaceName = nullptr;
			if (ScriptEngine->GetGlobalPropertyByIndex(GlobalPropertyIndex, &Name, &NamespaceName) < 0)
			{
				continue;
			}

			if (Name != nullptr
				&& NamespaceName != nullptr
				&& FCStringAnsi::Strcmp(Name, TCHAR_TO_ANSI(*Identifier)) == 0
				&& FCStringAnsi::Strcmp(NamespaceName, "GameplayTags") == 0)
			{
				return true;
			}
		}

		return false;
	}

	bool FindGameplayTagNamespaceGlobalFixture(FAngelscriptEngine& Engine, FGameplayTagNamespaceGlobalFixture& OutFixture)
	{
		FGameplayTagContainer AllTags;
		UGameplayTagsManager::Get().RequestAllGameplayTags(AllTags, false);

		TArray<FGameplayTag> TagArray;
		AllTags.GetGameplayTagArray(TagArray);

		TMap<FString, int32> IdentifierCounts;
		for (const FGameplayTag& Tag : TagArray)
		{
			const FString Identifier = SanitizeGameplayTagIdentifier(Tag.ToString());
			IdentifierCounts.FindOrAdd(Identifier) += 1;
		}

		for (const FGameplayTag& CandidateTag : TagArray)
		{
			const FGameplayTag CandidateParentTag = CandidateTag.RequestDirectParent();
			if (!CandidateParentTag.IsValid())
			{
				continue;
			}

			const FString TagIdentifier = SanitizeGameplayTagIdentifier(CandidateTag.ToString());
			const FString ParentIdentifier = SanitizeGameplayTagIdentifier(CandidateParentTag.ToString());
			if (!IsValidScriptIdentifier(TagIdentifier) || !IsValidScriptIdentifier(ParentIdentifier))
			{
				continue;
			}

			if (IdentifierCounts.FindRef(TagIdentifier) != 1)
			{
				continue;
			}

			if (!HasGameplayTagNamespaceGlobal(Engine, TagIdentifier) || !HasGameplayTagNamespaceGlobal(Engine, ParentIdentifier))
			{
				continue;
			}

			OutFixture.Tag = CandidateTag;
			OutFixture.SanitizedTagIdentifier = TagIdentifier;
			OutFixture.ParentTag = CandidateParentTag;
			OutFixture.SanitizedParentIdentifier = ParentIdentifier;
			return true;
		}

		return false;
	}
}

// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptGameplayTagBindingsTest,
	"Angelscript.TestModule.Bindings.GameplayTag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: GameplayTagCompat
	// ====================================================================

	TEST_METHOD(GameplayTagCompat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FGameplayTagContainer AllTags;
		UGameplayTagsManager::Get().RequestAllGameplayTags(AllTags, false);
		if (!TestRunner->TestTrue(TEXT("GameplayTagsManager should have at least one registered tag"), AllTags.Num() > 0))
		{
			return;
		}

		const FString EscapedTagName = AllTags.First().ToString().ReplaceCharWithEscapedChar();
		FString Script = FString::Printf(TEXT(R"(
int GPTag_RequestValid()
{
	FGameplayTag GlobalTag = FGameplayTag::RequestGameplayTag(FName("__TAG_NAME__"), true);
	return GlobalTag.IsValid() ? 1 : 0;
}
int GPTag_EmptyNotValid()
{
	FGameplayTag EmptyDefault;
	return EmptyDefault.IsValid() ? 0 : 1;
}
int GPTag_EmptyEqualsEmptyTag()
{
	FGameplayTag EmptyDefault;
	return (EmptyDefault == FGameplayTag::EmptyTag) ? 1 : 0;
}
int GPTag_EmptyNameIsNone()
{
	FGameplayTag EmptyDefault;
	return EmptyDefault.GetTagName().IsNone() ? 1 : 0;
}
int GPTag_EmptyToStringMatchesEmptyTag()
{
	FGameplayTag EmptyDefault;
	return (EmptyDefault.ToString() == FGameplayTag::EmptyTag.ToString()) ? 1 : 0;
}
int GPTag_RequestNoneInvalid()
{
	FGameplayTag RequestedInvalid = FGameplayTag::RequestGameplayTag(NAME_None, false);
	return (RequestedInvalid == FGameplayTag::EmptyTag) ? 1 : 0;
}
)"));
		Script.ReplaceInline(TEXT("__TAG_NAME__"), *EscapedTagName);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASGameplayTag_TagCompat"), Script);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTag_RequestValid()"), TEXT("RequestGameplayTag with valid name should be valid"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTag_EmptyNotValid()"), TEXT("Default FGameplayTag should not be valid"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTag_EmptyEqualsEmptyTag()"), TEXT("Default tag should equal EmptyTag"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTag_EmptyNameIsNone()"), TEXT("Default tag name should be None"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTag_EmptyToStringMatchesEmptyTag()"), TEXT("Default tag ToString should match EmptyTag"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTag_RequestNoneInvalid()"), TEXT("RequestGameplayTag with None should equal EmptyTag"), 1);
	}

	// ====================================================================
	// Section: GameplayTagContainerCompat
	// ====================================================================

	TEST_METHOD(GameplayTagContainerCompat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FGameplayTagContainer AllTags;
		UGameplayTagsManager::Get().RequestAllGameplayTags(AllTags, false);
		if (!TestRunner->TestTrue(TEXT("GameplayTagsManager should have at least one registered tag"), AllTags.Num() > 0))
		{
			return;
		}

		const FString TagName = AllTags.First().ToString().ReplaceCharWithEscapedChar();
		FString Script = FString::Printf(TEXT(R"(
int GPTagContainer_EmptyIsEmpty()
{
	FGameplayTagContainer EmptyDefault;
	return EmptyDefault.IsEmpty() ? 1 : 0;
}
int GPTagContainer_EmptyEqualsEmptyContainer()
{
	FGameplayTagContainer EmptyDefault;
	return (EmptyDefault == FGameplayTagContainer::EmptyContainer) ? 1 : 0;
}
int GPTagContainer_AddTagMakesValid()
{
	FGameplayTag ValidTag = FGameplayTag::RequestGameplayTag(FName("__TAG_NAME__"), true);
	FGameplayTagContainer Tags;
	Tags.AddTag(ValidTag);
	return Tags.IsValid() ? 1 : 0;
}
int GPTagContainer_NumAfterAdd()
{
	FGameplayTag ValidTag = FGameplayTag::RequestGameplayTag(FName("__TAG_NAME__"), true);
	FGameplayTagContainer Tags;
	Tags.AddTag(ValidTag);
	return (Tags.Num() == 1) ? 1 : 0;
}
int GPTagContainer_HasTag()
{
	FGameplayTag ValidTag = FGameplayTag::RequestGameplayTag(FName("__TAG_NAME__"), true);
	FGameplayTagContainer Tags;
	Tags.AddTag(ValidTag);
	return Tags.HasTag(ValidTag) ? 1 : 0;
}
int GPTagContainer_HasTagExact()
{
	FGameplayTag ValidTag = FGameplayTag::RequestGameplayTag(FName("__TAG_NAME__"), true);
	FGameplayTagContainer Tags;
	Tags.AddTag(ValidTag);
	return Tags.HasTagExact(ValidTag) ? 1 : 0;
}
int GPTagContainer_First()
{
	FGameplayTag ValidTag = FGameplayTag::RequestGameplayTag(FName("__TAG_NAME__"), true);
	FGameplayTagContainer Tags;
	Tags.AddTag(ValidTag);
	return (Tags.First() == ValidTag) ? 1 : 0;
}
int GPTagContainer_HasAnyAndAll()
{
	FGameplayTag ValidTag = FGameplayTag::RequestGameplayTag(FName("__TAG_NAME__"), true);
	FGameplayTagContainer Tags;
	Tags.AddTag(ValidTag);
	FGameplayTagContainer Others;
	Others.AddTag(ValidTag);
	if (!Tags.HasAny(Others)) return 0;
	if (!Tags.HasAnyExact(Others)) return 0;
	if (!Tags.HasAll(Others)) return 0;
	if (!Tags.HasAllExact(Others)) return 0;
	return 1;
}
int GPTagContainer_AppendTags()
{
	FGameplayTag ValidTag = FGameplayTag::RequestGameplayTag(FName("__TAG_NAME__"), true);
	FGameplayTagContainer Tags;
	Tags.AddTag(ValidTag);
	FGameplayTagContainer Combined;
	Combined.AppendTags(Tags);
	return Combined.HasTag(ValidTag) ? 1 : 0;
}
int GPTagContainer_RemoveTag()
{
	FGameplayTag ValidTag = FGameplayTag::RequestGameplayTag(FName("__TAG_NAME__"), true);
	FGameplayTagContainer Combined;
	Combined.AddTag(ValidTag);
	if (!Combined.RemoveTag(ValidTag)) return 0;
	return Combined.IsEmpty() ? 1 : 0;
}
int GPTagContainer_Reset()
{
	FGameplayTag ValidTag = FGameplayTag::RequestGameplayTag(FName("__TAG_NAME__"), true);
	FGameplayTagContainer Combined;
	Combined.AddTag(ValidTag);
	Combined.Reset();
	return Combined.IsEmpty() ? 1 : 0;
}
)"), *TagName);
		Script.ReplaceInline(TEXT("__TAG_NAME__"), *TagName);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASGameplayTag_Container"), Script);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagContainer_EmptyIsEmpty()"), TEXT("Default container should be empty"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagContainer_EmptyEqualsEmptyContainer()"), TEXT("Default container should equal EmptyContainer"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagContainer_AddTagMakesValid()"), TEXT("Container with tag should be valid"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagContainer_NumAfterAdd()"), TEXT("Container Num after AddTag should be 1"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagContainer_HasTag()"), TEXT("Container should have added tag"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagContainer_HasTagExact()"), TEXT("Container should have exact added tag"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagContainer_First()"), TEXT("Container First should equal added tag"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagContainer_HasAnyAndAll()"), TEXT("Container HasAny/HasAll should find matching tags"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagContainer_AppendTags()"), TEXT("AppendTags should copy tags into target"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagContainer_RemoveTag()"), TEXT("RemoveTag should leave container empty"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagContainer_Reset()"), TEXT("Reset should clear container"), 1);
	}

	// ====================================================================
	// Section: GameplayTagContainerEmptyContracts
	// ====================================================================

	TEST_METHOD(GameplayTagContainerEmptyContracts)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FGameplayTagContainer AllTags;
		UGameplayTagsManager::Get().RequestAllGameplayTags(AllTags, false);
		if (!TestRunner->TestTrue(TEXT("Empty-contract test requires at least one registered tag"), AllTags.Num() > 0))
		{
			return;
		}

		const FGameplayTag ValidTag = AllTags.First();
		if (!TestRunner->TestTrue(TEXT("Empty-contract test should resolve a valid tag fixture"), ValidTag.IsValid()))
		{
			return;
		}

		// Compute the native reference mask
		FGameplayTagContainer NonEmptyContainer;
		NonEmptyContainer.AddTag(ValidTag);
		FGameplayTagContainer EmptyContainer;
		FGameplayTag EmptyTag;

		int32 NativeMask = 0;
		if (NonEmptyContainer.HasAll(EmptyContainer))      NativeMask |= (1 << 0);
		if (NonEmptyContainer.HasAllExact(EmptyContainer))  NativeMask |= (1 << 1);
		if (NonEmptyContainer.HasAny(EmptyContainer))       NativeMask |= (1 << 2);
		if (NonEmptyContainer.HasAnyExact(EmptyContainer))  NativeMask |= (1 << 3);
		if (EmptyContainer.HasAll(EmptyContainer))           NativeMask |= (1 << 4);
		if (EmptyContainer.HasAllExact(EmptyContainer))      NativeMask |= (1 << 5);
		if (EmptyContainer.HasAny(EmptyContainer))           NativeMask |= (1 << 6);
		if (EmptyContainer.HasAnyExact(EmptyContainer))      NativeMask |= (1 << 7);
		if (EmptyContainer.HasAll(NonEmptyContainer))         NativeMask |= (1 << 8);
		if (EmptyContainer.HasAllExact(NonEmptyContainer))    NativeMask |= (1 << 9);
		if (EmptyContainer.HasAny(NonEmptyContainer))         NativeMask |= (1 << 10);
		if (EmptyContainer.HasAnyExact(NonEmptyContainer))    NativeMask |= (1 << 11);
		if (NonEmptyContainer.HasTag(EmptyTag))              NativeMask |= (1 << 12);
		if (NonEmptyContainer.HasTagExact(EmptyTag))          NativeMask |= (1 << 13);

		const FString TagName = ValidTag.ToString().ReplaceCharWithEscapedChar();
		FString Script = FString::Printf(TEXT(R"(
int GPTagEmptyContract_ComputeMask()
{
	FGameplayTag ValidTag = FGameplayTag::RequestGameplayTag(FName("__TAG_NAME__"), true);
	if (!ValidTag.IsValid()) return -1;

	FGameplayTagContainer NonEmptyContainer;
	NonEmptyContainer.AddTag(ValidTag);
	FGameplayTagContainer EmptyContainer;
	FGameplayTag EmptyTag;

	int ResultMask = 0;
	if (NonEmptyContainer.HasAll(EmptyContainer))      ResultMask |= (1 << 0);
	if (NonEmptyContainer.HasAllExact(EmptyContainer))  ResultMask |= (1 << 1);
	if (NonEmptyContainer.HasAny(EmptyContainer))       ResultMask |= (1 << 2);
	if (NonEmptyContainer.HasAnyExact(EmptyContainer))  ResultMask |= (1 << 3);
	if (EmptyContainer.HasAll(EmptyContainer))           ResultMask |= (1 << 4);
	if (EmptyContainer.HasAllExact(EmptyContainer))      ResultMask |= (1 << 5);
	if (EmptyContainer.HasAny(EmptyContainer))           ResultMask |= (1 << 6);
	if (EmptyContainer.HasAnyExact(EmptyContainer))      ResultMask |= (1 << 7);
	if (EmptyContainer.HasAll(NonEmptyContainer))         ResultMask |= (1 << 8);
	if (EmptyContainer.HasAllExact(NonEmptyContainer))    ResultMask |= (1 << 9);
	if (EmptyContainer.HasAny(NonEmptyContainer))         ResultMask |= (1 << 10);
	if (EmptyContainer.HasAnyExact(NonEmptyContainer))    ResultMask |= (1 << 11);
	if (NonEmptyContainer.HasTag(EmptyTag))              ResultMask |= (1 << 12);
	if (NonEmptyContainer.HasTagExact(EmptyTag))          ResultMask |= (1 << 13);

	return ResultMask;
}
)"), *TagName);
		Script.ReplaceInline(TEXT("__TAG_NAME__"), *TagName);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASGameplayTag_EmptyContract"), Script);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		// Use direct invocation to compare mask values
		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, M, TEXT("int GPTagEmptyContract_ComputeMask()"));
		if (!Invoker.IsValid()) return;
		const int32 ScriptMask = Invoker.CallAndReturn<int32>(INDEX_NONE);
		TestRunner->TestEqual(TEXT("GameplayTagContainer empty contracts should match UE native semantics"), ScriptMask, NativeMask);
	}

	// ====================================================================
	// Section: GameplayTagQueryCompat
	// ====================================================================

	TEST_METHOD(GameplayTagQueryCompat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FGameplayTagContainer AllTags;
		UGameplayTagsManager::Get().RequestAllGameplayTags(AllTags, false);
		if (!TestRunner->TestTrue(TEXT("GameplayTagsManager should have at least one registered tag"), AllTags.Num() > 0))
		{
			return;
		}

		const FString TagName = AllTags.First().ToString().ReplaceCharWithEscapedChar();
		FString Script = FString::Printf(TEXT(R"(
int GPTagQuery_EmptyDefault()
{
	FGameplayTagQuery EmptyDefault;
	if (!EmptyDefault.IsEmpty()) return 0;
	return (EmptyDefault == FGameplayTagQuery::EmptyQuery) ? 1 : 0;
}
int GPTagQuery_MatchAnyNotEmpty()
{
	FGameplayTag ValidTag = FGameplayTag::RequestGameplayTag(FName("__TAG_NAME__"), true);
	FGameplayTagContainer Tags;
	Tags.AddTag(ValidTag);
	FGameplayTagQuery MatchAny = FGameplayTagQuery::MakeQuery_MatchAnyTags(Tags);
	return MatchAny.IsEmpty() ? 0 : 1;
}
int GPTagQuery_MatchAllNotEmpty()
{
	FGameplayTag ValidTag = FGameplayTag::RequestGameplayTag(FName("__TAG_NAME__"), true);
	FGameplayTagContainer Tags;
	Tags.AddTag(ValidTag);
	FGameplayTagQuery MatchAll = FGameplayTagQuery::MakeQuery_MatchAllTags(Tags);
	return MatchAll.IsEmpty() ? 0 : 1;
}
int GPTagQuery_MatchNoneNotEmpty()
{
	FGameplayTag ValidTag = FGameplayTag::RequestGameplayTag(FName("__TAG_NAME__"), true);
	FGameplayTagContainer Tags;
	Tags.AddTag(ValidTag);
	FGameplayTagQuery MatchNone = FGameplayTagQuery::MakeQuery_MatchNoTags(Tags);
	return MatchNone.IsEmpty() ? 0 : 1;
}
int GPTagQuery_MatchTagNotEmpty()
{
	FGameplayTag ValidTag = FGameplayTag::RequestGameplayTag(FName("__TAG_NAME__"), true);
	FGameplayTagQuery MatchTag = FGameplayTagQuery::MakeQuery_MatchTag(ValidTag);
	return MatchTag.IsEmpty() ? 0 : 1;
}
int GPTagQuery_DifferentQueriesNotEqual()
{
	FGameplayTag ValidTag = FGameplayTag::RequestGameplayTag(FName("__TAG_NAME__"), true);
	FGameplayTagContainer Tags;
	Tags.AddTag(ValidTag);
	FGameplayTagQuery MatchAny = FGameplayTagQuery::MakeQuery_MatchAnyTags(Tags);
	FGameplayTagQuery MatchAll = FGameplayTagQuery::MakeQuery_MatchAllTags(Tags);
	return (MatchAny == MatchAll) ? 0 : 1;
}
int GPTagQuery_CopyEquality()
{
	FGameplayTag ValidTag = FGameplayTag::RequestGameplayTag(FName("__TAG_NAME__"), true);
	FGameplayTagContainer Tags;
	Tags.AddTag(ValidTag);
	FGameplayTagQuery MatchAny = FGameplayTagQuery::MakeQuery_MatchAnyTags(Tags);
	FGameplayTagQuery Copy = MatchAny;
	return (Copy == MatchAny) ? 1 : 0;
}
int GPTagQuery_ExactMatchFactories()
{
	FGameplayTag ValidTag = FGameplayTag::RequestGameplayTag(FName("__TAG_NAME__"), true);
	FGameplayTagContainer Tags;
	Tags.AddTag(ValidTag);
	FGameplayTagQuery MatchAnyExact = FGameplayTagQuery::MakeQuery_ExactMatchAnyTags(Tags);
	FGameplayTagQuery MatchAllExact = FGameplayTagQuery::MakeQuery_ExactMatchAllTags(Tags);
	if (MatchAnyExact.IsEmpty()) return 0;
	if (MatchAllExact.IsEmpty()) return 0;
	return 1;
}
int GPTagQuery_MatchesQuery()
{
	FGameplayTag ValidTag = FGameplayTag::RequestGameplayTag(FName("__TAG_NAME__"), true);
	FGameplayTagContainer Tags;
	Tags.AddTag(ValidTag);
	FGameplayTagQuery MatchAny = FGameplayTagQuery::MakeQuery_MatchAnyTags(Tags);
	FGameplayTagQuery MatchAll = FGameplayTagQuery::MakeQuery_MatchAllTags(Tags);
	FGameplayTagQuery MatchAnyExact = FGameplayTagQuery::MakeQuery_ExactMatchAnyTags(Tags);
	FGameplayTagQuery MatchAllExact = FGameplayTagQuery::MakeQuery_ExactMatchAllTags(Tags);
	FGameplayTagQuery MatchNone = FGameplayTagQuery::MakeQuery_MatchNoTags(Tags);
	if (!Tags.MatchesQuery(MatchAny)) return 0;
	if (!Tags.MatchesQuery(MatchAll)) return 0;
	if (!Tags.MatchesQuery(MatchAnyExact)) return 0;
	if (!Tags.MatchesQuery(MatchAllExact)) return 0;
	if (Tags.MatchesQuery(MatchNone)) return 0;
	return 1;
}
)"), *TagName);
		Script.ReplaceInline(TEXT("__TAG_NAME__"), *TagName);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASGameplayTag_Query"), Script);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagQuery_EmptyDefault()"), TEXT("Default query should be empty and equal EmptyQuery"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagQuery_MatchAnyNotEmpty()"), TEXT("MakeQuery_MatchAnyTags should not be empty"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagQuery_MatchAllNotEmpty()"), TEXT("MakeQuery_MatchAllTags should not be empty"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagQuery_MatchNoneNotEmpty()"), TEXT("MakeQuery_MatchNoTags should not be empty"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagQuery_MatchTagNotEmpty()"), TEXT("MakeQuery_MatchTag should not be empty"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagQuery_DifferentQueriesNotEqual()"), TEXT("Different query types should not be equal"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagQuery_CopyEquality()"), TEXT("Query copy should equal original"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagQuery_ExactMatchFactories()"), TEXT("ExactMatch factory queries should not be empty"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagQuery_MatchesQuery()"), TEXT("Container should match expected queries"), 1);
	}

	// ====================================================================
	// Section: GameplayTagExactQueryCompat
	// ====================================================================

	TEST_METHOD(GameplayTagExactQueryCompat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FGameplayTagContainer AllTags;
		UGameplayTagsManager::Get().RequestAllGameplayTags(AllTags, false);
		if (!TestRunner->TestTrue(TEXT("Exact-query test requires at least one registered tag"), AllTags.Num() > 0))
		{
			return;
		}

		const FString TagName = AllTags.First().ToString().ReplaceCharWithEscapedChar();
		FString Script = FString::Printf(TEXT(R"(
int GPTagExact_GetTagName()
{
	FGameplayTag ValidTag = FGameplayTag::RequestGameplayTag(FName("__TAG_NAME__"), true);
	return (ValidTag.GetTagName() == FName("__TAG_NAME__")) ? 1 : 0;
}
int GPTagExact_ToString()
{
	FGameplayTag ValidTag = FGameplayTag::RequestGameplayTag(FName("__TAG_NAME__"), true);
	return (ValidTag.ToString() == "__TAG_NAME__") ? 1 : 0;
}
int GPTagExact_MatchTagQuery()
{
	FGameplayTag ValidTag = FGameplayTag::RequestGameplayTag(FName("__TAG_NAME__"), true);
	FGameplayTagQuery MatchTag = FGameplayTagQuery::MakeQuery_MatchTag(ValidTag);
	FGameplayTagContainer Tags;
	Tags.AddTag(ValidTag);
	return Tags.MatchesQuery(MatchTag) ? 1 : 0;
}
int GPTagExact_EmptyNotMatchQuery()
{
	FGameplayTag ValidTag = FGameplayTag::RequestGameplayTag(FName("__TAG_NAME__"), true);
	FGameplayTagQuery MatchTag = FGameplayTagQuery::MakeQuery_MatchTag(ValidTag);
	FGameplayTagContainer EmptyTags;
	return EmptyTags.MatchesQuery(MatchTag) ? 0 : 1;
}
int GPTagExact_RequestNoneEqualsEmpty()
{
	FGameplayTag RequestedInvalid = FGameplayTag::RequestGameplayTag(NAME_None, false);
	return (RequestedInvalid == FGameplayTag::EmptyTag) ? 1 : 0;
}
)"), *TagName);
		Script.ReplaceInline(TEXT("__TAG_NAME__"), *TagName);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASGameplayTag_ExactQuery"), Script);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagExact_GetTagName()"), TEXT("GetTagName should return the requested FName"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagExact_ToString()"), TEXT("ToString should return the tag string"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagExact_MatchTagQuery()"), TEXT("Container with tag should match MatchTag query"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagExact_EmptyNotMatchQuery()"), TEXT("Empty container should not match MatchTag query"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagExact_RequestNoneEqualsEmpty()"), TEXT("RequestGameplayTag(None) should equal EmptyTag"), 1);
	}

	// ====================================================================
	// Section: GameplayTagNamespaceGlobals
	// ====================================================================

	TEST_METHOD(GameplayTagNamespaceGlobals)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		AngelscriptReloadGameplayTags();
		AngelscriptRebindGameplayTagsToCurrentEngine();

		FGameplayTagNamespaceGlobalFixture Fixture;
		if (!TestRunner->TestTrue(
			TEXT("Namespace-global test requires a child tag with valid parent and bound globals"),
			FindGameplayTagNamespaceGlobalFixture(Engine, Fixture)))
		{
			return;
		}

		const FString TagName = Fixture.Tag.ToString().ReplaceCharWithEscapedChar();
		const FString ParentTagName = Fixture.ParentTag.ToString().ReplaceCharWithEscapedChar();
		FString Script = FString::Printf(TEXT(R"(
int GPTagNS_NamespaceTagValid()
{
	FGameplayTag NamespaceTag = GameplayTags::%s;
	return NamespaceTag.IsValid() ? 1 : 0;
}
int GPTagNS_NamespaceTagEqualsRequested()
{
	FGameplayTag NamespaceTag = GameplayTags::%s;
	FGameplayTag RequestedTag = FGameplayTag::RequestGameplayTag(FName("%s"), true);
	return (NamespaceTag == RequestedTag) ? 1 : 0;
}
int GPTagNS_NamespaceTagNameMatches()
{
	FGameplayTag NamespaceTag = GameplayTags::%s;
	FGameplayTag RequestedTag = FGameplayTag::RequestGameplayTag(FName("%s"), true);
	return (NamespaceTag.GetTagName() == RequestedTag.GetTagName()) ? 1 : 0;
}
int GPTagNS_NamespaceTagToStringMatches()
{
	FGameplayTag NamespaceTag = GameplayTags::%s;
	FGameplayTag RequestedTag = FGameplayTag::RequestGameplayTag(FName("%s"), true);
	return (NamespaceTag.ToString() == RequestedTag.ToString()) ? 1 : 0;
}
int GPTagNS_ParentTagValid()
{
	FGameplayTag NamespaceParentTag = GameplayTags::%s;
	return NamespaceParentTag.IsValid() ? 1 : 0;
}
int GPTagNS_ParentTagEqualsRequested()
{
	FGameplayTag NamespaceParentTag = GameplayTags::%s;
	FGameplayTag RequestedParentTag = FGameplayTag::RequestGameplayTag(FName("%s"), true);
	return (NamespaceParentTag == RequestedParentTag) ? 1 : 0;
}
int GPTagNS_DirectParentMatches()
{
	FGameplayTag NamespaceTag = GameplayTags::%s;
	FGameplayTag NamespaceParentTag = GameplayTags::%s;
	return (NamespaceTag.RequestDirectParent() == NamespaceParentTag) ? 1 : 0;
}
int GPTagNS_ParentChainContainsParent()
{
	FGameplayTag NamespaceTag = GameplayTags::%s;
	FGameplayTag NamespaceParentTag = GameplayTags::%s;
	FGameplayTagContainer ParentChain = NamespaceTag.GetGameplayTagParents();
	return ParentChain.HasTagExact(NamespaceParentTag) ? 1 : 0;
}
int GPTagNS_MatchesParent()
{
	FGameplayTag NamespaceTag = GameplayTags::%s;
	FGameplayTag NamespaceParentTag = GameplayTags::%s;
	return NamespaceTag.MatchesTag(NamespaceParentTag) ? 1 : 0;
}
int GPTagNS_NotMatchesExactParent()
{
	FGameplayTag NamespaceTag = GameplayTags::%s;
	FGameplayTag NamespaceParentTag = GameplayTags::%s;
	return NamespaceTag.MatchesTagExact(NamespaceParentTag) ? 0 : 1;
}
)"),
			*Fixture.SanitizedTagIdentifier,
			*Fixture.SanitizedTagIdentifier, *TagName,
			*Fixture.SanitizedTagIdentifier, *TagName,
			*Fixture.SanitizedTagIdentifier, *TagName,
			*Fixture.SanitizedParentIdentifier,
			*Fixture.SanitizedParentIdentifier, *ParentTagName,
			*Fixture.SanitizedTagIdentifier, *Fixture.SanitizedParentIdentifier,
			*Fixture.SanitizedTagIdentifier, *Fixture.SanitizedParentIdentifier,
			*Fixture.SanitizedTagIdentifier, *Fixture.SanitizedParentIdentifier,
			*Fixture.SanitizedTagIdentifier, *Fixture.SanitizedParentIdentifier);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASGameplayTag_Namespace"), Script);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagNS_NamespaceTagValid()"), TEXT("Namespace tag global should be valid"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagNS_NamespaceTagEqualsRequested()"), TEXT("Namespace tag should equal RequestGameplayTag result"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagNS_NamespaceTagNameMatches()"), TEXT("Namespace tag name should match requested"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagNS_NamespaceTagToStringMatches()"), TEXT("Namespace tag ToString should match requested"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagNS_ParentTagValid()"), TEXT("Namespace parent tag should be valid"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagNS_ParentTagEqualsRequested()"), TEXT("Namespace parent tag should equal requested"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagNS_DirectParentMatches()"), TEXT("RequestDirectParent should return namespace parent"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagNS_ParentChainContainsParent()"), TEXT("GetGameplayTagParents should contain parent tag"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagNS_MatchesParent()"), TEXT("Child tag should MatchesTag parent"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagNS_NotMatchesExactParent()"), TEXT("Child tag should not MatchesTagExact parent"), 1);
	}

	// ====================================================================
	// Section: GameplayTagHierarchySemantics
	// ====================================================================

	TEST_METHOD(GameplayTagHierarchySemantics)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FGameplayTag ChildTag;
		FGameplayTag ParentTag;
		FGameplayTag UnrelatedTag;
		if (!TestRunner->TestTrue(
			TEXT("Hierarchy semantics test requires child/parent/unrelated fixture"),
			FindGameplayTagHierarchyFixture(ChildTag, ParentTag, UnrelatedTag)))
		{
			return;
		}

		const FString ChildTagName = ChildTag.ToString().ReplaceCharWithEscapedChar();
		const FString ParentTagName = ParentTag.ToString().ReplaceCharWithEscapedChar();
		const FString UnrelatedTagName = UnrelatedTag.ToString().ReplaceCharWithEscapedChar();
		FString Script = FString::Printf(TEXT(R"(
int GPTagHier_ChildMatchesParent()
{
	FGameplayTag ChildTag = FGameplayTag::RequestGameplayTag(FName("__CHILD__"), true);
	FGameplayTag ParentTag = FGameplayTag::RequestGameplayTag(FName("__PARENT__"), true);
	return ChildTag.MatchesTag(ParentTag) ? 1 : 0;
}
int GPTagHier_ChildNotMatchesExactParent()
{
	FGameplayTag ChildTag = FGameplayTag::RequestGameplayTag(FName("__CHILD__"), true);
	FGameplayTag ParentTag = FGameplayTag::RequestGameplayTag(FName("__PARENT__"), true);
	return ChildTag.MatchesTagExact(ParentTag) ? 0 : 1;
}
int GPTagHier_MatchesTagDepthPositive()
{
	FGameplayTag ChildTag = FGameplayTag::RequestGameplayTag(FName("__CHILD__"), true);
	FGameplayTag ParentTag = FGameplayTag::RequestGameplayTag(FName("__PARENT__"), true);
	return (ChildTag.MatchesTagDepth(ParentTag) >= 1) ? 1 : 0;
}
int GPTagHier_MatchesAnyParentContainer()
{
	FGameplayTag ChildTag = FGameplayTag::RequestGameplayTag(FName("__CHILD__"), true);
	FGameplayTag ParentTag = FGameplayTag::RequestGameplayTag(FName("__PARENT__"), true);
	FGameplayTagContainer ParentContainer;
	ParentContainer.AddTag(ParentTag);
	return ChildTag.MatchesAny(ParentContainer) ? 1 : 0;
}
int GPTagHier_NotMatchesAnyExactParent()
{
	FGameplayTag ChildTag = FGameplayTag::RequestGameplayTag(FName("__CHILD__"), true);
	FGameplayTag ParentTag = FGameplayTag::RequestGameplayTag(FName("__PARENT__"), true);
	FGameplayTagContainer ParentContainer;
	ParentContainer.AddTag(ParentTag);
	return ChildTag.MatchesAnyExact(ParentContainer) ? 0 : 1;
}
int GPTagHier_DirectParentIsParent()
{
	FGameplayTag ChildTag = FGameplayTag::RequestGameplayTag(FName("__CHILD__"), true);
	FGameplayTag ParentTag = FGameplayTag::RequestGameplayTag(FName("__PARENT__"), true);
	return (ChildTag.RequestDirectParent() == ParentTag) ? 1 : 0;
}
int GPTagHier_SingleTagContainer()
{
	FGameplayTag ChildTag = FGameplayTag::RequestGameplayTag(FName("__CHILD__"), true);
	FGameplayTag ParentTag = FGameplayTag::RequestGameplayTag(FName("__PARENT__"), true);
	FGameplayTagContainer SingleTagContainer = ChildTag.GetSingleTagContainer();
	if (SingleTagContainer.Num() != 1) return 0;
	if (!SingleTagContainer.HasTagExact(ChildTag)) return 0;
	if (SingleTagContainer.HasTagExact(ParentTag)) return 0;
	return 1;
}
int GPTagHier_ParentChainContainsBoth()
{
	FGameplayTag ChildTag = FGameplayTag::RequestGameplayTag(FName("__CHILD__"), true);
	FGameplayTag ParentTag = FGameplayTag::RequestGameplayTag(FName("__PARENT__"), true);
	FGameplayTagContainer ParentChain = ChildTag.GetGameplayTagParents();
	if (!ParentChain.HasTagExact(ChildTag)) return 0;
	if (!ParentChain.HasTagExact(ParentTag)) return 0;
	return 1;
}
int GPTagHier_UnrelatedNotMatched()
{
	FGameplayTag ChildTag = FGameplayTag::RequestGameplayTag(FName("__CHILD__"), true);
	FGameplayTag UnrelatedTag = FGameplayTag::RequestGameplayTag(FName("__UNRELATED__"), true);
	if (ChildTag.MatchesTag(UnrelatedTag)) return 0;
	if (ChildTag.MatchesTagExact(UnrelatedTag)) return 0;
	if (ChildTag.MatchesTagDepth(UnrelatedTag) != 0) return 0;
	FGameplayTagContainer UnrelatedContainer;
	UnrelatedContainer.AddTag(UnrelatedTag);
	if (ChildTag.MatchesAny(UnrelatedContainer)) return 0;
	if (ChildTag.MatchesAnyExact(UnrelatedContainer)) return 0;
	FGameplayTagContainer ParentChain = ChildTag.GetGameplayTagParents();
	if (ParentChain.HasTagExact(UnrelatedTag)) return 0;
	return 1;
}
)"));
		Script.ReplaceInline(TEXT("__CHILD__"), *ChildTagName);
		Script.ReplaceInline(TEXT("__PARENT__"), *ParentTagName);
		Script.ReplaceInline(TEXT("__UNRELATED__"), *UnrelatedTagName);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASGameplayTag_Hierarchy"), Script);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagHier_ChildMatchesParent()"), TEXT("Child should MatchesTag parent"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagHier_ChildNotMatchesExactParent()"), TEXT("Child should not MatchesTagExact parent"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagHier_MatchesTagDepthPositive()"), TEXT("MatchesTagDepth child->parent should be >= 1"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagHier_MatchesAnyParentContainer()"), TEXT("Child should MatchesAny parent container"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagHier_NotMatchesAnyExactParent()"), TEXT("Child should not MatchesAnyExact parent container"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagHier_DirectParentIsParent()"), TEXT("RequestDirectParent should return the parent"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagHier_SingleTagContainer()"), TEXT("GetSingleTagContainer should contain only self"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagHier_ParentChainContainsBoth()"), TEXT("GetGameplayTagParents should contain child and parent"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagHier_UnrelatedNotMatched()"), TEXT("Unrelated tag should not match child in any way"), 1);
	}

	// ====================================================================
	// Section: GameplayTagContainerHierarchyFilters
	// ====================================================================

	TEST_METHOD(GameplayTagContainerHierarchyFilters)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FGameplayTag ChildTag;
		FGameplayTag ParentTag;
		FGameplayTag UnrelatedTag;
		if (!TestRunner->TestTrue(
			TEXT("Container hierarchy/filter test requires child/parent/unrelated fixture"),
			FindGameplayTagHierarchyFixture(ChildTag, ParentTag, UnrelatedTag)))
		{
			return;
		}

		const FString ChildTagName = ChildTag.ToString().ReplaceCharWithEscapedChar();
		const FString ParentTagName = ParentTag.ToString().ReplaceCharWithEscapedChar();
		const FString UnrelatedTagName = UnrelatedTag.ToString().ReplaceCharWithEscapedChar();
		FString Script = FString::Printf(TEXT(R"(
int GPTagFilter_AddTagFast()
{
	FGameplayTag ChildTag = FGameplayTag::RequestGameplayTag(FName("__CHILD__"), true);
	FGameplayTag UnrelatedTag = FGameplayTag::RequestGameplayTag(FName("__UNRELATED__"), true);
	FGameplayTagContainer ContainerA;
	ContainerA.AddTag(ChildTag);
	ContainerA.AddTagFast(UnrelatedTag);
	return ContainerA.HasTagExact(UnrelatedTag) ? 1 : 0;
}
int GPTagFilter_FilterIncludesMatchingTags()
{
	FGameplayTag ChildTag = FGameplayTag::RequestGameplayTag(FName("__CHILD__"), true);
	FGameplayTag ParentTag = FGameplayTag::RequestGameplayTag(FName("__PARENT__"), true);
	FGameplayTag UnrelatedTag = FGameplayTag::RequestGameplayTag(FName("__UNRELATED__"), true);
	FGameplayTagContainer ContainerA;
	ContainerA.AddTag(ChildTag);
	ContainerA.AddTagFast(UnrelatedTag);
	FGameplayTagContainer ParentContainer;
	ParentContainer.AddTag(ParentTag);
	ParentContainer.AddTagFast(UnrelatedTag);
	FGameplayTagContainer Filtered = ContainerA.Filter(ParentContainer);
	if (!Filtered.HasTagExact(ChildTag)) return 0;
	if (!Filtered.HasTagExact(UnrelatedTag)) return 0;
	return (Filtered.Num() == 2) ? 1 : 0;
}
int GPTagFilter_FilterExactExcludesChild()
{
	FGameplayTag ChildTag = FGameplayTag::RequestGameplayTag(FName("__CHILD__"), true);
	FGameplayTag ParentTag = FGameplayTag::RequestGameplayTag(FName("__PARENT__"), true);
	FGameplayTag UnrelatedTag = FGameplayTag::RequestGameplayTag(FName("__UNRELATED__"), true);
	FGameplayTagContainer ContainerA;
	ContainerA.AddTag(ChildTag);
	ContainerA.AddTagFast(UnrelatedTag);
	FGameplayTagContainer ParentContainer;
	ParentContainer.AddTag(ParentTag);
	ParentContainer.AddTagFast(UnrelatedTag);
	FGameplayTagContainer FilteredExact = ContainerA.FilterExact(ParentContainer);
	if (FilteredExact.HasTagExact(ChildTag)) return 0;
	if (!FilteredExact.HasTagExact(UnrelatedTag)) return 0;
	return (FilteredExact.Num() == 1) ? 1 : 0;
}
int GPTagFilter_AddLeafTag()
{
	FGameplayTag ChildTag = FGameplayTag::RequestGameplayTag(FName("__CHILD__"), true);
	FGameplayTag ParentTag = FGameplayTag::RequestGameplayTag(FName("__PARENT__"), true);
	FGameplayTag UnrelatedTag = FGameplayTag::RequestGameplayTag(FName("__UNRELATED__"), true);
	FGameplayTagContainer ParentContainer;
	ParentContainer.AddTag(ParentTag);
	ParentContainer.AddTagFast(UnrelatedTag);
	FGameplayTagContainer LeafContainer = ParentContainer;
	if (!LeafContainer.AddLeafTag(ChildTag)) return 0;
	if (!LeafContainer.HasTagExact(ChildTag)) return 0;
	if (LeafContainer.HasTagExact(ParentTag)) return 0;
	if (!LeafContainer.HasTag(ParentTag)) return 0;
	return (LeafContainer.Num() == 2) ? 1 : 0;
}
int GPTagFilter_LeafFilterExact()
{
	FGameplayTag ChildTag = FGameplayTag::RequestGameplayTag(FName("__CHILD__"), true);
	FGameplayTag ParentTag = FGameplayTag::RequestGameplayTag(FName("__PARENT__"), true);
	FGameplayTag UnrelatedTag = FGameplayTag::RequestGameplayTag(FName("__UNRELATED__"), true);
	FGameplayTagContainer ContainerA;
	ContainerA.AddTag(ChildTag);
	ContainerA.AddTagFast(UnrelatedTag);
	FGameplayTagContainer ParentContainer;
	ParentContainer.AddTag(ParentTag);
	ParentContainer.AddTagFast(UnrelatedTag);
	FGameplayTagContainer LeafContainer = ParentContainer;
	LeafContainer.AddLeafTag(ChildTag);
	FGameplayTagContainer LeafFilteredExact = ContainerA.FilterExact(LeafContainer);
	if (!LeafFilteredExact.HasTagExact(ChildTag)) return 0;
	if (!LeafFilteredExact.HasTagExact(UnrelatedTag)) return 0;
	return (LeafFilteredExact.Num() == 2) ? 1 : 0;
}
int GPTagFilter_ExpandedParents()
{
	FGameplayTag ChildTag = FGameplayTag::RequestGameplayTag(FName("__CHILD__"), true);
	FGameplayTag ParentTag = FGameplayTag::RequestGameplayTag(FName("__PARENT__"), true);
	FGameplayTag UnrelatedTag = FGameplayTag::RequestGameplayTag(FName("__UNRELATED__"), true);
	FGameplayTagContainer ContainerA;
	ContainerA.AddTag(ChildTag);
	ContainerA.AddTagFast(UnrelatedTag);
	FGameplayTagContainer ExpandedParents = ContainerA.GetGameplayTagParents();
	if (!ExpandedParents.HasTagExact(ChildTag)) return 0;
	if (!ExpandedParents.HasTagExact(ParentTag)) return 0;
	if (!ExpandedParents.HasTagExact(UnrelatedTag)) return 0;
	return 1;
}
int GPTagFilter_RemoveTags()
{
	FGameplayTag ChildTag = FGameplayTag::RequestGameplayTag(FName("__CHILD__"), true);
	FGameplayTag UnrelatedTag = FGameplayTag::RequestGameplayTag(FName("__UNRELATED__"), true);
	FGameplayTagContainer ContainerA;
	ContainerA.AddTag(ChildTag);
	ContainerA.AddTagFast(UnrelatedTag);
	FGameplayTagContainer RemoveContainer;
	RemoveContainer.AddTag(ChildTag);
	ContainerA.RemoveTags(RemoveContainer);
	if (ContainerA.HasTagExact(ChildTag)) return 0;
	if (!ContainerA.HasTagExact(UnrelatedTag)) return 0;
	return (ContainerA.Num() == 1) ? 1 : 0;
}
)"));
		Script.ReplaceInline(TEXT("__CHILD__"), *ChildTagName);
		Script.ReplaceInline(TEXT("__PARENT__"), *ParentTagName);
		Script.ReplaceInline(TEXT("__UNRELATED__"), *UnrelatedTagName);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASGameplayTag_Filter"), Script);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagFilter_AddTagFast()"), TEXT("AddTagFast should add tag to container"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagFilter_FilterIncludesMatchingTags()"), TEXT("Filter should include hierarchy-matching tags"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagFilter_FilterExactExcludesChild()"), TEXT("FilterExact should exclude child of parent"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagFilter_AddLeafTag()"), TEXT("AddLeafTag should replace parent with child"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagFilter_LeafFilterExact()"), TEXT("FilterExact against leaf container should match both"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagFilter_ExpandedParents()"), TEXT("GetGameplayTagParents should expand to include all ancestors"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GPTagFilter_RemoveTags()"), TEXT("RemoveTags should remove specified tag leaving the rest"), 1);
	}
};

#endif
