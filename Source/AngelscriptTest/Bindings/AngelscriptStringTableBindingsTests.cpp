// ============================================================================
// AngelscriptStringTableBindingsTests.cpp
//
// StringTable LOCTABLE binding coverage — CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Bindings.StringTable.FAngelscriptStringTableBindingsTest.*
//
// Sections:
//   LocTableCompat — LOCTABLE_NEW, LOCTABLE_SETSTRING, LOCTABLE_SETMETA,
//                    LOCTABLE read-back, FStringTableRegistry verification,
//                    source string and metadata payload parity
//
// CQTest adaptation notes:
//   The string table ID is unique per run (GUID-based). C++ helpers create
//   the table, run script, then verify registry state. Token substitution
//   injects the runtime table ID into script source via ReplaceInline.
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptBindingsModuleBuilder.h"
#include "Shared/AngelscriptBindingsAssertions.h"

#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"
#include "Misc/Guid.h"
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

namespace AngelscriptStringTableBindingsTests_Private
{
	static const FString GreetingKeyString(TEXT("Greeting"));
	static const FTextKey GreetingKey(TEXT("Greeting"));
	static const FName CommentMetaDataId(TEXT("Comment"));
	static const FString ExpectedNamespace(TEXT("AS.Test.Namespace"));
	static const FString ExpectedGreeting(TEXT("Hello"));
	static const FString ExpectedComment(TEXT("Doc"));

	FName MakeUniqueStringTableId()
	{
		return FName(*FString::Printf(
			TEXT("Angelscript.Test.StringTable.%s"),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	}

	void UnregisterStringTableIfPresent(const FName TableId)
	{
		FStringTableRegistry::Get().UnregisterStringTable(TableId);
	}

	FString BuildLocTableScript(const FName TableId)
	{
		FString Script = TEXT(R"(
int LocTable_ReadBack()
{
	const FName TableId = n"__TABLE_ID__";
	LOCTABLE_NEW(TableId, "__TABLE_NAMESPACE__");
	LOCTABLE_SETSTRING(TableId, "__GREETING_KEY__", "__GREETING_VALUE__");
	LOCTABLE_SETMETA(TableId, "__GREETING_KEY__", n"__COMMENT_META_ID__", "__COMMENT_VALUE__");

	FText Greeting = LOCTABLE(TableId, "__GREETING_KEY__");
	if (Greeting.IsEmpty())
		return 0;
	if (Greeting.ToString() != "__GREETING_VALUE__")
		return 0;
	return 1;
}
)");
		Script.ReplaceInline(TEXT("__TABLE_ID__"), *TableId.ToString(), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__TABLE_NAMESPACE__"), *ExpectedNamespace, ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__GREETING_KEY__"), *GreetingKeyString, ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__GREETING_VALUE__"), *ExpectedGreeting, ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__COMMENT_META_ID__"), *CommentMetaDataId.ToString(), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__COMMENT_VALUE__"), *ExpectedComment, ESearchCase::CaseSensitive);
		return Script;
	}
}


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptStringTableBindingsTest,
	"Angelscript.TestModule.Bindings.StringTable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: LocTableCompat
	// ====================================================================

	TEST_METHOD(LocTableCompat)
	{
		using namespace AngelscriptStringTableBindingsTests_Private;
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FName TableId = MakeUniqueStringTableId();
		ON_SCOPE_EXIT { UnregisterStringTableIfPresent(TableId); };
		UnregisterStringTableIfPresent(TableId);

		const FString Script = BuildLocTableScript(TableId);

		FCoverageModuleScope Mod(*TestRunner, Engine, TEXT("ASStringTable_LocTableCompat"), Script);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		// Verify script can read back the localized text
		AngelscriptTestBindings::ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int LocTable_ReadBack()"), TEXT("LOCTABLE reads back expected localized text"), 1);

		// Verify C++ registry state after script execution
		FStringTableConstPtr StringTable = FStringTableRegistry::Get().FindStringTable(TableId);
		TestRunner->TestNotNull(
			TEXT("[StringTable] registered string table exists in FStringTableRegistry"),
			StringTable.Get());

		if (StringTable.Get() != nullptr)
		{
			FString SourceString;
			const bool bHasSourceString = StringTable->GetSourceString(GreetingKey, SourceString);
			TestRunner->TestTrue(
				TEXT("[StringTable] Greeting source string is addressable in registry"),
				bHasSourceString);
			TestRunner->TestEqual(
				TEXT("[StringTable] Greeting source string contents match"),
				SourceString,
				ExpectedGreeting);

			const FString MetaData = StringTable->GetMetaData(GreetingKey, CommentMetaDataId);
			TestRunner->TestEqual(
				TEXT("[StringTable] Greeting metadata payload matches"),
				MetaData,
				ExpectedComment);
		}

		// Verify table survives module discard (Mod destructor hasn't run yet,
		// but we can check it's still registered before scope exit)
		TestRunner->TestNotNull(
			TEXT("[StringTable] registered table alive before explicit registry cleanup"),
			FStringTableRegistry::Get().FindStringTable(TableId).Get());
	}
};

#endif
