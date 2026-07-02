// ============================================================================
// AngelscriptStringTableBindingsTests.cpp
//
// StringTable LOCTABLE binding coverage �?CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Bindings.StringTable.FAngelscriptStringTableBindingsTest.*
//
// Sections:
//   LocTableCompat �?LOCTABLE_NEW, LOCTABLE_SETSTRING, LOCTABLE_SETMETA,
//                    LOCTABLE read-back, FStringTableRegistry verification,
//                    source string and metadata payload parity
//
// CQTest adaptation notes:
//   The string table ID is unique per run (GUID-based). C++ helpers create
//   the table, run script, then verify registry state. Token substitution
//   injects the runtime table ID into script source via ReplaceInline.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptStringTableBindingsTest,
	"Angelscript.TestModule.Bindings.StringTable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FString GreetingKeyString = FString(TEXT("Greeting"));
	inline static const FTextKey GreetingKey = FTextKey(TEXT("Greeting"));
	inline static const FName CommentMetaDataId = FName(TEXT("Comment"));
	inline static const FString ExpectedNamespace = FString(TEXT("AS.Test.Namespace"));
	inline static const FString ExpectedGreeting = FString(TEXT("Hello"));
	inline static const FString ExpectedComment = FString(TEXT("Doc"));

	static FName MakeUniqueStringTableId()
	{
		return FName(*FString::Printf(
			TEXT("Angelscript.Test.StringTable.%s"),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	}

	static void UnregisterStringTableIfPresent(const FName TableId)
	{
		FStringTableRegistry::Get().UnregisterStringTable(TableId);
	}

	static FString BuildLocTableScript(const FName TableId)
	{
		FString LocTableSource = ASTEST_AS(R"AS(
			int LocTable_ReadBack()
			{
				const FName TableId = n"__TABLE_ID__";
				LOCTABLE_NEW(TableId, "__TABLE_NAMESPACE__");
				LOCTABLE_SETSTRING(TableId, "__GREETING_KEY__", "__GREETING_VALUE__");
				LOCTABLE_SETMETA(TableId, "__GREETING_KEY__", n"__COMMENT_META_ID__", "__COMMENT_VALUE__");

				FText Greeting = LOCTABLE(TableId, "__GREETING_KEY__");
				if (Greeting.IsEmpty())
				{
					return 0;
				}
				if (Greeting.ToString() != "__GREETING_VALUE__")
				{
					return 0;
				}
				return 1;
			}
			)AS");
		LocTableSource.ReplaceInline(TEXT("__TABLE_ID__"), *TableId.ToString(), ESearchCase::CaseSensitive);
		LocTableSource.ReplaceInline(TEXT("__TABLE_NAMESPACE__"), *ExpectedNamespace, ESearchCase::CaseSensitive);
		LocTableSource.ReplaceInline(TEXT("__GREETING_KEY__"), *GreetingKeyString, ESearchCase::CaseSensitive);
		LocTableSource.ReplaceInline(TEXT("__GREETING_VALUE__"), *ExpectedGreeting, ESearchCase::CaseSensitive);
		LocTableSource.ReplaceInline(TEXT("__COMMENT_META_ID__"), *CommentMetaDataId.ToString(), ESearchCase::CaseSensitive);
		LocTableSource.ReplaceInline(TEXT("__COMMENT_VALUE__"), *ExpectedComment, ESearchCase::CaseSensitive);
		return LocTableSource;
	}

public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	// ====================================================================
	// Section: LocTableCompat
	// ====================================================================

	TEST_METHOD(LocTableLookup)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FName TableId = MakeUniqueStringTableId();
		ON_SCOPE_EXIT { UnregisterStringTableIfPresent(TableId); };
		UnregisterStringTableIfPresent(TableId);

		const FString LocTableSource = BuildLocTableScript(TableId);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASStringTable_LocTableCompat"), LocTableSource);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		// Verify script can read back the localized text
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int LocTable_ReadBack()"), TEXT("LOCTABLE reads back expected localized text"), 1), TEXT("LOCTABLE reads back expected localized text")));

		// Verify C++ registry state after script execution
		FStringTableConstPtr StringTable = FStringTableRegistry::Get().FindStringTable(TableId);
		ASSERT_THAT(IsNotNull(
			StringTable.Get(),
			TEXT("[StringTable] registered string table exists in FStringTableRegistry")));

		if (StringTable.Get() != nullptr)
		{
			FString SourceString;
			const bool bHasSourceString = StringTable->GetSourceString(GreetingKey, SourceString);
			ASSERT_THAT(IsTrue(
				bHasSourceString,
				TEXT("[StringTable] Greeting source string is addressable in registry")));
			ASSERT_THAT(AreEqual(
				ExpectedGreeting,
				SourceString,
				TEXT("[StringTable] Greeting source string contents match")));

			const FString MetaData = StringTable->GetMetaData(GreetingKey, CommentMetaDataId);
			ASSERT_THAT(AreEqual(
				ExpectedComment,
				MetaData,
				TEXT("[StringTable] Greeting metadata payload matches")));
		}

		// Verify table survives module discard (Mod destructor hasn't run yet,
		// but we can check it's still registered before scope exit)
		ASSERT_THAT(IsNotNull(
			FStringTableRegistry::Get().FindStringTable(TableId).Get(),
			TEXT("[StringTable] registered table alive before explicit registry cleanup")));
	}
};

#endif
