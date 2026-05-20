#include "Binds/Bind_FGameplayTag.h"
#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "../Debugging/AngelscriptDebugServer.h"

#include "Binds/Helper_StructType.h"
#include "Binds/Helper_ToString.h"

#include "GameplayTagContainer.h"
#include "GameplayTagsManager.h"
#include "GameplayTagsSettings.h"

// These are global variables because they are meant
// to be static, persistent memory, and they are used
// in multiple functions in this file (for organization reasons).
// 25 is an arbitrary number chosen for how many tags
// fit in one chunk before a new chunk is allocated.
TChunkedArray<FGameplayTag, sizeof(FGameplayTag) * 25> AngelscriptGameplayTags;

// This is a duplication of data for checking tag containment.
// A TSet gives us a O(1) lookup rather than O(n) TArray gives.
// Reallocation of this variable as tags are added is acceptable
// since it doesn't break bound pointers used in AngelscriptGameplayTags
TSet<FName> AngelscriptGameplayTagsLookup;

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FGameplayTagQuery(FAngelscriptBinds::EOrder::Late, [] {
	auto FGameplayTagQuery_ = FAngelscriptBinds::ExistingClass("FGameplayTagQuery");

	{
		FAngelscriptBinds::FNamespace ns("FGameplayTagQuery");
		FAngelscriptBinds::BindGlobalVariable("FGameplayTagQuery EmptyQuery", &FGameplayTagQuery::EmptyQuery);

		FAngelscriptBinds::BindGlobalFunction("FGameplayTagQuery MakeQuery_MatchAnyTags(const FGameplayTagContainer& InTags) no_discard", &FGameplayTagQuery::MakeQuery_MatchAnyTags);
		FAngelscriptBinds::BindGlobalFunction("FGameplayTagQuery MakeQuery_MatchAllTags(const FGameplayTagContainer& InTags) no_discard", &FGameplayTagQuery::MakeQuery_MatchAllTags);
		FAngelscriptBinds::BindGlobalFunction("FGameplayTagQuery MakeQuery_MatchNoTags(const FGameplayTagContainer& InTags) no_discard", &FGameplayTagQuery::MakeQuery_MatchNoTags);
		FAngelscriptBinds::BindGlobalFunction("FGameplayTagQuery MakeQuery_ExactMatchAnyTags(const FGameplayTagContainer& InTags) no_discard", &FGameplayTagQuery::MakeQuery_ExactMatchAnyTags);
		FAngelscriptBinds::BindGlobalFunction("FGameplayTagQuery MakeQuery_ExactMatchAllTags(const FGameplayTagContainer& InTags) no_discard", &FGameplayTagQuery::MakeQuery_ExactMatchAllTags);
		FAngelscriptBinds::BindGlobalFunction("FGameplayTagQuery MakeQuery_MatchTag(const FGameplayTag& InTag) no_discard", &FGameplayTagQuery::MakeQuery_MatchTag);
	}

	FGameplayTagQuery_.Method("bool IsEmpty() const", METHOD_TRIVIAL(FGameplayTagQuery, IsEmpty));
	FGameplayTagQuery_.Method("bool opEquals(const FGameplayTagQuery& Other) const", METHODPR_TRIVIAL(bool, FGameplayTagQuery, operator==, (const FGameplayTagQuery& Other) const));
});

void Bind_AddNewGameplayTag(int32 GameplayTagIndex)
{
	FAngelscriptBinds::FNamespace ns("GameplayTags");

	const FGameplayTag& GameplayTag = AngelscriptGameplayTags[GameplayTagIndex];

	FString TagAsString = GameplayTag.ToString();

	// Exchange dots and other invalid characters in the tag for underscores
	for (int32 Index = TagAsString.Len() - 1; Index >= 0; --Index)
	{
		if (!FAngelscriptEngine::IsValidIdentifierCharacter(TagAsString[Index]))
		{
			TagAsString[Index] = '_';
		}
	}

	const FString Declaration = TEXT("const FGameplayTag ") + TagAsString;
	FAngelscriptBinds::BindGlobalVariable(TCHAR_TO_ANSI(*Declaration), &GameplayTag);
}

bool Bind_ProcessGameplayTag(const FName TagName)
{
	bool Result = false;
	const FGameplayTag Tag = UGameplayTagsManager::Get().RequestGameplayTag(TagName, false);

	if (Tag.IsValid())
	{
		// Both the chunked array and its dedup lookup are process-level
		// containers (see AngelscriptEngine.cpp::ReleaseOwnedSharedStateResources
		// for the rationale on why these are not cleared on shutdown).
		// Tagging the inserts so the LLM report attributes the residual size
		// to "Angelscript/GlobalState/GameplayTags" rather than the default bucket.

		if (!AngelscriptGameplayTagsLookup.Contains(TagName))
		{
			Result = true;
			int32 Index = AngelscriptGameplayTags.AddElement(Tag);
			AngelscriptGameplayTagsLookup.Add(TagName);

			// AddElement makes a copy, so we need to use the
			// returned index to get the pointer to the persistent
			// value
			Bind_AddNewGameplayTag(Index);
		}
		for (auto& ParentTag : Tag.GetGameplayTagParents())
		{
			FName ParentTagName = ParentTag.GetTagName();
			if (!AngelscriptGameplayTagsLookup.Contains(ParentTagName))
			{
				Result = true;
				int32 Index = AngelscriptGameplayTags.AddElement(ParentTag);
				AngelscriptGameplayTagsLookup.Add(ParentTagName);

				// AddElement makes a copy, so we need to use the
				// returned index to get the pointer to the persistent
				// value
				Bind_AddNewGameplayTag(Index);
			}
		}
	}

	return Result;
}

bool Bind_AddNewGameplayTags()
{
	bool Result = false;
	TArray<const FGameplayTagSource*> Sources;
	UGameplayTagsManager::Get().FindTagSourcesWithType(EGameplayTagSourceType::Native, Sources);
	UGameplayTagsManager::Get().FindTagSourcesWithType(EGameplayTagSourceType::DefaultTagList, Sources);
	UGameplayTagsManager::Get().FindTagSourcesWithType(EGameplayTagSourceType::TagList, Sources);
	UGameplayTagsManager::Get().FindTagSourcesWithType(EGameplayTagSourceType::RestrictedTagList, Sources);
	UGameplayTagsManager::Get().FindTagSourcesWithType(EGameplayTagSourceType::DataTable, Sources);

	for (const FGameplayTagSource* Source : Sources)
	{
		if (Source->SourceTagList != nullptr)
		{
			for (const auto& RowTag : Source->SourceTagList->GameplayTagList)
			{
				Result |= Bind_ProcessGameplayTag(RowTag.Tag);
			}
		}

		if (Source->SourceRestrictedTagList != nullptr)
		{
			for (const auto& RowTag : Source->SourceRestrictedTagList->RestrictedGameplayTagList)
			{
				Result |= Bind_ProcessGameplayTag(RowTag.Tag);
			}
		}
	}

	return Result;
}

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FGameplayTag(FAngelscriptBinds::EOrder::Late, [] {
	auto FGameplayTag_ = FAngelscriptBinds::ExistingClass("FGameplayTag");

	{
		FAngelscriptBinds::FNamespace ns("FGameplayTag");
		FAngelscriptBinds::BindGlobalVariable("FGameplayTag EmptyTag", &FGameplayTag::EmptyTag);
		FAngelscriptBinds::BindGlobalFunction("FGameplayTag RequestGameplayTag(const FName& TagName, bool ErrorIfNotFound = true) no_discard", FUNC_TRIVIAL(FGameplayTag::RequestGameplayTag));
	}

	FGameplayTag_.Method("bool opEquals(const FGameplayTag& Other) const", METHODPR_TRIVIAL(bool, FGameplayTag, operator==, (FGameplayTag const& Other) const));
	FGameplayTag_.Method("bool IsValid() const", METHOD_TRIVIAL(FGameplayTag, IsValid));
	FGameplayTag_.Method("FName GetTagName() const", METHOD_TRIVIAL(FGameplayTag, GetTagName));
	FGameplayTag_.Method("bool MatchesTag(const FGameplayTag& TagToCheck) const", METHOD_TRIVIAL(FGameplayTag, MatchesTag));
	FGameplayTag_.Method("bool MatchesTagExact(const FGameplayTag& TagToCheck) const", METHOD_TRIVIAL(FGameplayTag, MatchesTagExact));
	FGameplayTag_.Method("int MatchesTagDepth(const FGameplayTag& TagToCheck) const", METHOD_TRIVIAL(FGameplayTag, MatchesTagDepth));
	FGameplayTag_.Method("bool MatchesAny(const FGameplayTagContainer& ContainerToCheck) const", METHOD_TRIVIAL(FGameplayTag, MatchesAny));
	FGameplayTag_.Method("bool MatchesAnyExact(const FGameplayTagContainer& ContainerToCheck) const", METHOD_TRIVIAL(FGameplayTag, MatchesAnyExact));
	FGameplayTag_.Method("FGameplayTagContainer GetSingleTagContainer() const", METHOD_TRIVIAL(FGameplayTag, GetSingleTagContainer));
	FGameplayTag_.Method("FGameplayTag RequestDirectParent() const", METHOD_TRIVIAL(FGameplayTag, RequestDirectParent));
	FGameplayTag_.Method("FGameplayTagContainer GetGameplayTagParents() const", METHOD_TRIVIAL(FGameplayTag, GetGameplayTagParents));

	auto FGameplayTagContainer_ = FAngelscriptBinds::ExistingClass("FGameplayTagContainer");
	{
		FAngelscriptBinds::FNamespace ns("FGameplayTagContainer");
		FAngelscriptBinds::BindGlobalVariable("FGameplayTagContainer EmptyContainer", &FGameplayTagContainer::EmptyContainer);
	}
	FGameplayTagContainer_.Method("bool opEquals(const FGameplayTagContainer& Other) const", METHODPR_TRIVIAL(bool, FGameplayTagContainer, operator==, (FGameplayTagContainer const& Other) const));
	FGameplayTagContainer_.Method("bool IsEmpty() const", METHOD_TRIVIAL(FGameplayTagContainer, IsEmpty));
	FGameplayTagContainer_.Method("bool IsValid() const", METHOD_TRIVIAL(FGameplayTagContainer, IsValid));
	FGameplayTagContainer_.Method("int Num() const", METHOD_TRIVIAL(FGameplayTagContainer, Num));
	FGameplayTagContainer_.Method("FGameplayTag First() const", METHOD_TRIVIAL(FGameplayTagContainer, First));
	FGameplayTagContainer_.Method("void AddTag(const FGameplayTag& TagToAdd)", METHOD_TRIVIAL(FGameplayTagContainer, AddTag));
	FGameplayTagContainer_.Method("void AddTagFast(const FGameplayTag& TagToAdd)", METHOD_TRIVIAL(FGameplayTagContainer, AddTagFast));
	FGameplayTagContainer_.Method("bool AddLeafTag(const FGameplayTag& TagToAdd)", METHOD_TRIVIAL(FGameplayTagContainer, AddLeafTag));
	FGameplayTagContainer_.Method("bool RemoveTag(const FGameplayTag& TagToRemove)", [](FGameplayTagContainer& Container, const FGameplayTag& TagToRemove) -> bool
	{
		return Container.RemoveTag(TagToRemove, false);
	});
	FGameplayTagContainer_.Method("void RemoveTags(const FGameplayTagContainer& TagsToRemove)", METHOD_TRIVIAL(FGameplayTagContainer, RemoveTags));
	FGameplayTagContainer_.Method("void AppendTags(const FGameplayTagContainer& Other)", METHOD_TRIVIAL(FGameplayTagContainer, AppendTags));
	FGameplayTagContainer_.Method("void Reset(int Slack = 0)", METHOD_TRIVIAL(FGameplayTagContainer, Reset));
	FGameplayTagContainer_.Method("bool HasTag(const FGameplayTag& TagToCheck) const", METHOD_TRIVIAL(FGameplayTagContainer, HasTag));
	FGameplayTagContainer_.Method("bool HasTagExact(const FGameplayTag& TagToCheck) const", METHOD_TRIVIAL(FGameplayTagContainer, HasTagExact));
	FGameplayTagContainer_.Method("bool HasAny(const FGameplayTagContainer& ContainerToCheck) const", METHOD_TRIVIAL(FGameplayTagContainer, HasAny));
	FGameplayTagContainer_.Method("bool HasAnyExact(const FGameplayTagContainer& ContainerToCheck) const", METHOD_TRIVIAL(FGameplayTagContainer, HasAnyExact));
	FGameplayTagContainer_.Method("bool HasAll(const FGameplayTagContainer& ContainerToCheck) const", METHOD_TRIVIAL(FGameplayTagContainer, HasAll));
	FGameplayTagContainer_.Method("bool HasAllExact(const FGameplayTagContainer& ContainerToCheck) const", METHOD_TRIVIAL(FGameplayTagContainer, HasAllExact));
	FGameplayTagContainer_.Method("FGameplayTagContainer GetGameplayTagParents() const", METHOD_TRIVIAL(FGameplayTagContainer, GetGameplayTagParents));
	FGameplayTagContainer_.Method("FGameplayTagContainer Filter(const FGameplayTagContainer& OtherContainer) const", METHOD_TRIVIAL(FGameplayTagContainer, Filter));
	FGameplayTagContainer_.Method("FGameplayTagContainer FilterExact(const FGameplayTagContainer& OtherContainer) const", METHOD_TRIVIAL(FGameplayTagContainer, FilterExact));
	FGameplayTagContainer_.Method("bool MatchesQuery(const FGameplayTagQuery& Query) const", METHOD_TRIVIAL(FGameplayTagContainer, MatchesQuery));

	FToStringHelper::Register(TEXT("FGameplayTag"), [](void *Ptr, FString &Str) {
		Str += ((FGameplayTag *)Ptr)->ToString();
	});

	Bind_AddNewGameplayTags();
});

void AngelscriptReloadGameplayTags()
{
	if (Bind_AddNewGameplayTags())
	{
#if WITH_AS_DEBUGSERVER
		FAngelscriptDebugServer* DebugServer = FAngelscriptEngine::Get().DebugServer;
		if (DebugServer != nullptr)
		{
			DebugServer->BroadcastDebugDatabase();
		}
#endif
	}
}

void AngelscriptRebindGameplayTagsToCurrentEngine()
{
	// Re-bind every cached tag to the currently scoped engine. This intentionally
	// bypasses AngelscriptGameplayTagsLookup — that lookup is a process-level guard
	// to avoid duplicate slots in the AngelscriptGameplayTags chunked array, but
	// for a clone/test engine that missed the initial bind pass we need to replay
	// the RegisterGlobalProperty calls onto its own asIScriptEngine.
	for (int32 TagIndex = 0; TagIndex < AngelscriptGameplayTags.Num(); ++TagIndex)
	{
		Bind_AddNewGameplayTag(TagIndex);
	}
}
