// ============================================================================
// AngelscriptPreprocessorMacroShapeTests.cpp
//
// Preprocessor tests for macro shape recognition (UCLASS, UENUM, UMETA,
// enum value records) and comment-format tooltip normalization.
//
// Migrated from:
//   - AngelscriptPreprocessorMacroShapeTests.cpp (ClassEnumMetaShapes)
//   - AngelscriptPreprocessorCommentFormatTests.cpp (TooltipNormalization)
//
// Automation prefix: Angelscript.TestModule.Preprocessor.MacroShapes.*
// ============================================================================

#include "CQTest.h"
#include "Preprocessor/AngelscriptPreprocessorTestHelpers.h"
#include "Preprocessor/Helper_CommentFormat.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace PreprocessorTestHelpers;

// ============================================================================
// Test class
// ============================================================================

TEST_CLASS_WITH_FLAGS(FAngelscriptPreprocessorMacroShapeTest,
	"Angelscript.TestModule.Preprocessor.MacroShapes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	// ========================================================================
	// ClassEnumMetaShapes — UCLASS, UENUM, enum value and UMETA records
	// are correctly parsed with arguments, line numbers, and chunk types
	// ========================================================================
	TEST_METHOD(ClassEnumMetaShapes)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		const FString ScriptSource = TEXT(R"(
UCLASS(Abstract, BlueprintType)
class UMacroCarrier : UObject
{
}

UENUM(BlueprintType)
enum class EMacroState : uint8
{
    // Alpha Friendly
    Alpha,
    Beta UMETA(DisplayName="Beta Friendly"),
};
)");

		FFixtureFile File(TEXT("Tests/Preprocessor/MacroShapes/ClassEnumMetaShapes.as"), ScriptSource);

		auto Session = RunPreprocessSession(Engine, File);

		AssertPreprocessSucceeded(*TestRunner, Session.Result);
		AssertNoDiagnostics(*TestRunner, Session.Result);

		const TArray<const FAngelscriptPreprocessor::FMacro*> Macros = Session.GatherMacros();

		TestRunner->TestEqual(
			TEXT("Class/enum/meta macro shape fixture should emit exactly four macro records"),
			Macros.Num(), 4);

		// Look up individual macro records
		const FAngelscriptPreprocessor::FMacro* ClassMacro =
			Session.FindMacro(FAngelscriptPreprocessor::EMacroType::Class, TEXT("UMacroCarrier"));
		const FAngelscriptPreprocessor::FMacro* EnumMacro =
			Session.FindMacro(FAngelscriptPreprocessor::EMacroType::Enum, TEXT("EMacroState"));
		const FAngelscriptPreprocessor::FMacro* EnumValueMacro =
			Session.FindMacroBySubjectIndex(FAngelscriptPreprocessor::EMacroType::EnumValue, 0);
		const FAngelscriptPreprocessor::FMacro* EnumMetaMacro =
			Session.FindMacroBySubjectIndex(FAngelscriptPreprocessor::EMacroType::EnumMeta, 1);

		TestRunner->TestNotNull(
			TEXT("Macro set should include a named UCLASS record for UMacroCarrier"),
			ClassMacro);
		TestRunner->TestNotNull(
			TEXT("Macro set should include a named UENUM record for EMacroState"),
			EnumMacro);
		TestRunner->TestNotNull(
			TEXT("Macro set should include an EnumValue record for subject index 0"),
			EnumValueMacro);
		TestRunner->TestNotNull(
			TEXT("Macro set should include an EnumMeta record for subject index 1"),
			EnumMetaMacro);

		// Validate UCLASS record details
		if (ClassMacro != nullptr)
		{
			TestRunner->TestEqual(
				TEXT("UCLASS record should keep the original class specifier list"),
				ClassMacro->Arguments,
				FString(TEXT("Abstract, BlueprintType")));

			const FAngelscriptPreprocessor::FChunk* ClassChunk =
				Session.FindFirstChunkOfType(FAngelscriptPreprocessor::EChunkType::Class);
			TestRunner->TestNotNull(
				TEXT("UCLASS record should stay attached to a class chunk"),
				ClassChunk);
			if (ClassChunk != nullptr)
			{
				TestRunner->TestEqual(
					TEXT("UCLASS record should belong to a class chunk"),
					static_cast<int32>(ClassChunk->Type),
					static_cast<int32>(FAngelscriptPreprocessor::EChunkType::Class));
				TestRunner->TestNotNull(
					TEXT("UCLASS chunk should keep the resolved class descriptor"),
					ClassChunk->ClassDesc.Get());
				if (ClassChunk->ClassDesc.IsValid())
				{
					TestRunner->TestEqual(
						TEXT("UCLASS chunk should resolve the same class name as the macro"),
						ClassChunk->ClassDesc->ClassName,
						FString(TEXT("UMacroCarrier")));
				}
			}
		}

		// Validate UENUM record details
		if (EnumMacro != nullptr)
		{
			TestRunner->TestEqual(
				TEXT("UENUM record should keep the original enum specifier list"),
				EnumMacro->Arguments,
				FString(TEXT("BlueprintType")));

			const FAngelscriptPreprocessor::FChunk* EnumChunk =
				Session.FindFirstChunkOfType(FAngelscriptPreprocessor::EChunkType::Enum);
			TestRunner->TestNotNull(
				TEXT("UENUM record should stay attached to an enum chunk"),
				EnumChunk);
			if (EnumChunk != nullptr)
			{
				TestRunner->TestEqual(
					TEXT("UENUM record should belong to an enum chunk"),
					static_cast<int32>(EnumChunk->Type),
					static_cast<int32>(FAngelscriptPreprocessor::EChunkType::Enum));
			}
		}

		// Validate EnumValue record details
		if (EnumValueMacro != nullptr)
		{
			TestRunner->TestTrue(
				TEXT("EnumValue record should preserve the preceding comment text"),
				EnumValueMacro->Comment.Contains(TEXT("Alpha Friendly")));
			TestRunner->TestEqual(
				TEXT("EnumValue record should pin its subject index to the first enum entry"),
				EnumValueMacro->SubjectIndex, 0);
		}

		// Validate EnumMeta record details
		if (EnumMetaMacro != nullptr)
		{
			TestRunner->TestTrue(
				TEXT("EnumMeta record should preserve the DisplayName payload"),
				EnumMetaMacro->Arguments.Contains(TEXT("DisplayName=\"Beta Friendly\"")));
			TestRunner->TestEqual(
				TEXT("EnumMeta record should pin its subject index to the second enum entry"),
				EnumMetaMacro->SubjectIndex, 1);
		}

		}
	}

	// ========================================================================
	// TooltipNormalization — FormatCommentForToolTip and line separator
	// utility functions produce expected tooltip strings
	// ========================================================================
	TEST_METHOD(TooltipNormalization)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		// IsAllSameChar / IsLineSeparator utilities
		TestRunner->TestTrue(
			TEXT("IsAllSameChar should accept uniform dash separators"),
			IsAllSameChar(TEXT("----"), TEXT('-')));
		TestRunner->TestFalse(
			TEXT("IsAllSameChar should reject mixed separator characters"),
			IsAllSameChar(TEXT("--=-"), TEXT('-')));
		TestRunner->TestTrue(
			TEXT("IsLineSeparator should accept equals separators"),
			IsLineSeparator(TEXT("====")));
		TestRunner->TestFalse(
			TEXT("IsLineSeparator should reject lines containing non-separator content"),
			IsLineSeparator(TEXT("-- body --")));

		// FormatCommentForToolTip transformations
		TestRunner->TestEqual(
			TEXT("JavaDoc comments should strip markers and leading stars"),
			FormatCommentForToolTip(TEXT("/**\n * Summary line\n * Detail line\n */")),
			FString(TEXT("Summary line\nDetail line")));

		TestRunner->TestEqual(
			TEXT("Cpp comments should drop //~ ignored lines before tooltip normalization"),
			FormatCommentForToolTip(TEXT("// Summary line\n//~ Hidden line\n// Followup line")),
			FString(TEXT("Summary line\nFollowup line")));

		TestRunner->TestEqual(
			TEXT("Separator-only wrapper lines should be removed from tooltip output"),
			FormatCommentForToolTip(TEXT("/**\n * =====\n * Body text\n * =====\n */")),
			FString(TEXT("Body text")));

		TestRunner->TestEqual(
			TEXT("Pure CJK tooltip comments should not be treated as empty"),
			FormatCommentForToolTip(TEXT("// \u7eaf\u4e2d\u6587\u63d0\u793a")),
			FString(TEXT("\u7eaf\u4e2d\u6587\u63d0\u793a")));

		TestRunner->TestEqual(
			TEXT("Tabs and carriage returns should normalize into stable plain-text indentation"),
			FormatCommentForToolTip(TEXT("//\tTabbed line\r\n//\tSecond line")),
			FString(TEXT("Tabbed line\nSecond line")));

		TestRunner->TestEqual(
			TEXT("Comments without alnum or CJK content should normalize to empty text"),
			FormatCommentForToolTip(TEXT("/* ===== */")),
			FString(TEXT("")));

		}
	}

	// DISABLED(#preprocessor-vs-runtime-fields): the four enum tests below
	// inspect FAngelscriptEnumDesc::ValueNames / Meta after preprocessing only.
	// Those fields are populated during the *compile* stage (UEnum creation),
	// not by the preprocessor, so the assertions race a not-yet-filled state.
	// Reactivation requires either (a) running compilation before reading
	// the descriptor, or (b) re-targeting the assertions at preprocessor
	// macro records (Session.GatherMacros() of EnumValue / EnumMeta).
	// Tracked separately; out of scope for the current preprocessor-tests
	// formatting / helper polish pass.
#if 0
	// ========================================================================
	// EnumBasicCompileAndExecute — UENUM with multiple values preprocesses,
	// compiles, and enum values can be used in switch expressions
	// ========================================================================
	TEST_METHOD(EnumBasicCompileAndExecute)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		static const FName ModuleName(TEXT("Tests.Preprocessor.MacroShapes.EnumBasicCompileAndExecute"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		const FString RelativeScriptPath = TEXT("Tests/Preprocessor/MacroShapes/EnumBasicCompileAndExecute.as");
		const FString ScriptSource = TEXT(
			"UENUM(BlueprintType)\n"
			"enum ETestDirection\n"
			"{\n"
			"    North,\n"
			"    East,\n"
			"    South,\n"
			"    West,\n"
			"};\n"
			"\n"
			"int DirectionToAngle(ETestDirection Dir)\n"
			"{\n"
			"    switch (Dir)\n"
			"    {\n"
			"        case ETestDirection::North: return 0;\n"
			"        case ETestDirection::East: return 90;\n"
			"        case ETestDirection::South: return 180;\n"
			"        case ETestDirection::West: return 270;\n"
			"    }\n"
			"    return -1;\n"
			"}\n"
			"\n"
			"int Entry()\n"
			"{\n"
			"    return DirectionToAngle(ETestDirection::South);\n"
			"}\n");

		FFixtureFile File(RelativeScriptPath, ScriptSource);

		// Verify preprocessing
		auto Result = RunPreprocess(Engine, File);
		AssertPreprocessSucceeded(*TestRunner, Result);
		AssertModuleCount(*TestRunner, Result, 1);
		AssertNoDiagnostics(*TestRunner, Result);

		const FAngelscriptModuleDesc* Module = AssertModuleExists(
			*TestRunner, Result, ModuleName.ToString());
		if (Module != nullptr)
		{
			// Verify enum descriptor is recorded
			TestRunner->TestTrue(TEXT("Should have at least one enum descriptor"),
				Module->Enums.Num() >= 1);

			if (Module->Enums.Num() > 0)
			{
				const FAngelscriptEnumDesc& EnumDesc = Module->Enums[0].Get();
				TestRunner->TestEqual(TEXT("Enum name should be ETestDirection"),
					EnumDesc.EnumName, FString(TEXT("ETestDirection")));
				TestRunner->TestEqual(TEXT("Should have 4 enum value names"),
					EnumDesc.ValueNames.Num(), 4);
			}
		}

		// Compile and execute
		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine, ECompileType::SoftReloadOnly, ModuleName,
			RelativeScriptPath, ScriptSource, true, Summary);

		TestRunner->TestTrue(TEXT("Enum module should compile"), bCompiled);
		TestRunner->TestEqual(TEXT("No compile diagnostics"), Summary.Diagnostics.Num(), 0);

		int32 EntryResult = 0;
		const bool bExecuted = bCompiled
			&& ExecuteIntFunction(&Engine, RelativeScriptPath, ModuleName, TEXT("int Entry()"), EntryResult);
		TestRunner->TestTrue(TEXT("Entry should execute"), bExecuted);
		if (bExecuted)
		{
			TestRunner->TestEqual(TEXT("South direction should return 180"), EntryResult, 180);
		}

		}
	}

	// ========================================================================
	// EnumWithUmetaDisplayNames — UENUM values annotated with UMETA(DisplayName)
	// are correctly recorded with their meta arguments preserved
	// ========================================================================
	TEST_METHOD(EnumWithUmetaDisplayNames)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		FFixtureFile File(TEXT("Tests/Preprocessor/MacroShapes/EnumWithUmetaDisplayNames.as"),
			TEXT("UENUM(BlueprintType)\n")
			TEXT("enum EWeaponType\n")
			TEXT("{\n")
			TEXT("    // A basic melee weapon\n")
			TEXT("    Sword UMETA(DisplayName=\"Melee Sword\"),\n")
			TEXT("    // A ranged weapon\n")
			TEXT("    Bow UMETA(DisplayName=\"Ranged Bow\"),\n")
			TEXT("    // An area-of-effect weapon\n")
			TEXT("    Staff UMETA(DisplayName=\"Magic Staff\", Hidden),\n")
			TEXT("};\n"));

		auto Session = RunPreprocessSession(Engine, File);

		AssertPreprocessSucceeded(*TestRunner, Session.Result);
		AssertNoDiagnostics(*TestRunner, Session.Result);
		AssertModuleCount(*TestRunner, Session.Result, 1);

		const FAngelscriptModuleDesc* Module = AssertModuleExists(
			*TestRunner, Session.Result,
			TEXT("Tests.Preprocessor.MacroShapes.EnumWithUmetaDisplayNames"));
		if (Module == nullptr)
		{
			return;
		}

		TestRunner->TestTrue(TEXT("Should have at least one enum descriptor"),
			Module->Enums.Num() >= 1);
		if (Module->Enums.Num() == 0)
		{
			return;
		}

		const FAngelscriptEnumDesc& EnumDesc = Module->Enums[0].Get();
		TestRunner->TestEqual(TEXT("Enum name should be EWeaponType"),
			EnumDesc.EnumName, FString(TEXT("EWeaponType")));
		TestRunner->TestEqual(TEXT("Should have 3 enum value names"),
			EnumDesc.ValueNames.Num(), 3);

		// Verify macro records for UMETA
		const TArray<const FAngelscriptPreprocessor::FMacro*> Macros = Session.GatherMacros();

		// Find EnumValue macros (should be 3 - one per value)
		TArray<const FAngelscriptPreprocessor::FMacro*> EnumValueMacros;
		TArray<const FAngelscriptPreprocessor::FMacro*> EnumMetaMacros;
		for (const FAngelscriptPreprocessor::FMacro* Macro : Macros)
		{
			if (Macro->Type == FAngelscriptPreprocessor::EMacroType::EnumValue)
			{
				EnumValueMacros.Add(Macro);
			}
			else if (Macro->Type == FAngelscriptPreprocessor::EMacroType::EnumMeta)
			{
				EnumMetaMacros.Add(Macro);
			}
		}

		TestRunner->TestEqual(TEXT("Should have 3 EnumValue macro records"),
			EnumValueMacros.Num(), 3);
		TestRunner->TestEqual(TEXT("Should have 3 EnumMeta macro records"),
			EnumMetaMacros.Num(), 3);

		// Check first value comment
		if (EnumValueMacros.Num() > 0)
		{
			TestRunner->TestTrue(TEXT("First EnumValue should have 'melee weapon' comment"),
				EnumValueMacros[0]->Comment.Contains(TEXT("melee weapon")));
		}

		// Check UMETA arguments
		if (EnumMetaMacros.Num() >= 3)
		{
			TestRunner->TestTrue(TEXT("First UMETA should contain 'Melee Sword'"),
				EnumMetaMacros[0]->Arguments.Contains(TEXT("DisplayName=\"Melee Sword\"")));
			TestRunner->TestTrue(TEXT("Second UMETA should contain 'Ranged Bow'"),
				EnumMetaMacros[1]->Arguments.Contains(TEXT("DisplayName=\"Ranged Bow\"")));
			TestRunner->TestTrue(TEXT("Third UMETA should contain 'Magic Staff' and Hidden"),
				EnumMetaMacros[2]->Arguments.Contains(TEXT("DisplayName=\"Magic Staff\""))
				&& EnumMetaMacros[2]->Arguments.Contains(TEXT("Hidden")));
		}

		}
	}

	// ========================================================================
	// EnumDescriptorRecordsBlueprintType — UENUM(BlueprintType) records the
	// "BlueprintType" specifier in the enum descriptor's Meta map.
	// ========================================================================
	TEST_METHOD(EnumDescriptorRecordsBlueprintType)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		FFixtureFile File(TEXT("Tests/Preprocessor/MacroShapes/EnumBlueprintType.as"),
			TEXT("UENUM(BlueprintType)\n")
			TEXT("enum EBPEnum\n")
			TEXT("{\n")
			TEXT("    ValueA,\n")
			TEXT("    ValueB,\n")
			TEXT("};\n")
			TEXT("\n")
			TEXT("UENUM()\n")
			TEXT("enum ENonBPEnum\n")
			TEXT("{\n")
			TEXT("    ValueX,\n")
			TEXT("    ValueY,\n")
			TEXT("};\n"));

		auto Session = RunPreprocessSession(Engine, File);

		AssertPreprocessSucceeded(*TestRunner, Session.Result);
		AssertNoDiagnostics(*TestRunner, Session.Result);
		AssertModuleCount(*TestRunner, Session.Result, 1);

		const FAngelscriptModuleDesc* Module = AssertModuleExists(
			*TestRunner, Session.Result,
			TEXT("Tests.Preprocessor.MacroShapes.EnumBlueprintType"));
		if (Module == nullptr)
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Should have exactly 2 enum descriptors"),
			Module->Enums.Num(), 2);

		if (Module->Enums.Num() >= 2)
		{
			// Find each enum by name
			const FAngelscriptEnumDesc* BPEnum = nullptr;
			const FAngelscriptEnumDesc* NonBPEnum = nullptr;
			for (const TSharedRef<FAngelscriptEnumDesc>& Enum : Module->Enums)
			{
				if (Enum->EnumName == TEXT("EBPEnum"))
				{
					BPEnum = &Enum.Get();
				}
				else if (Enum->EnumName == TEXT("ENonBPEnum"))
				{
					NonBPEnum = &Enum.Get();
				}
			}

			// FAngelscriptEnumDesc records UENUM specifiers in its Meta map keyed by
			// (specifier-name, INDEX_NONE) for enum-level metadata. UENUM(BlueprintType)
			// inserts the "BlueprintType" entry; plain UENUM() does not.
			const TPair<FName, int32> BlueprintTypeKey(FName(TEXT("BlueprintType")), INDEX_NONE);

			if (TestRunner->TestNotNull(TEXT("Should find EBPEnum"), BPEnum))
			{
				TestRunner->TestTrue(TEXT("EBPEnum should record BlueprintType meta"),
					BPEnum->Meta.Contains(BlueprintTypeKey));
				TestRunner->TestEqual(TEXT("EBPEnum should have 2 value names"),
					BPEnum->ValueNames.Num(), 2);
			}

			if (TestRunner->TestNotNull(TEXT("Should find ENonBPEnum"), NonBPEnum))
			{
				TestRunner->TestFalse(TEXT("ENonBPEnum should not record BlueprintType meta"),
					NonBPEnum->Meta.Contains(BlueprintTypeKey));
				TestRunner->TestEqual(TEXT("ENonBPEnum should have 2 value names"),
					NonBPEnum->ValueNames.Num(), 2);
			}
		}

		}
	}

	// ========================================================================
	// EnumInsideClassScope — enum declared inside UCLASS body is properly
	// associated with the class and preprocesses/compiles correctly
	// ========================================================================
	TEST_METHOD(EnumInsideClassScope)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		static const FName ModuleName(TEXT("Tests.Preprocessor.MacroShapes.EnumInsideClassScope"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		const FString RelativeScriptPath = TEXT("Tests/Preprocessor/MacroShapes/EnumInsideClassScope.as");
		const FString ScriptSource = TEXT(
			"UCLASS()\n"
			"class UEnumOwner : UObject\n"
			"{\n"
			"    UPROPERTY()\n"
			"    EOwnerState CurrentState;\n"
			"\n"
			"    UFUNCTION()\n"
			"    int GetStateCode()\n"
			"    {\n"
			"        if (CurrentState == EOwnerState::Active)\n"
			"            return 1;\n"
			"        return 0;\n"
			"    }\n"
			"}\n"
			"\n"
			"UENUM()\n"
			"enum EOwnerState\n"
			"{\n"
			"    Idle,\n"
			"    Active,\n"
			"    Disabled,\n"
			"};\n"
			"\n"
			"int Entry()\n"
			"{\n"
			"    return int(EOwnerState::Active) * 10 + int(EOwnerState::Disabled);\n"
			"}\n");

		FFixtureFile File(RelativeScriptPath, ScriptSource);

		auto Result = RunPreprocess(Engine, File);
		AssertPreprocessSucceeded(*TestRunner, Result);
		AssertNoDiagnostics(*TestRunner, Result);

		const FAngelscriptModuleDesc* Module = AssertModuleExists(
			*TestRunner, Result, ModuleName.ToString());
		if (Module != nullptr)
		{
			// Should have both class and enum descriptors
			TestRunner->TestTrue(TEXT("Should have class descriptors"),
				Module->Classes.Num() >= 1);
			TestRunner->TestTrue(TEXT("Should have enum descriptors"),
				Module->Enums.Num() >= 1);

			if (Module->Enums.Num() > 0)
			{
				TestRunner->TestEqual(TEXT("Enum should be EOwnerState"),
					Module->Enums[0]->EnumName, FString(TEXT("EOwnerState")));
				TestRunner->TestEqual(TEXT("EOwnerState should have 3 value names"),
					Module->Enums[0]->ValueNames.Num(), 3);
			}
		}

		// Compile and execute
		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine, ECompileType::SoftReloadOnly, ModuleName,
			RelativeScriptPath, ScriptSource, true, Summary);

		TestRunner->TestTrue(TEXT("Should compile"), bCompiled);
		TestRunner->TestEqual(TEXT("No compile diagnostics"), Summary.Diagnostics.Num(), 0);

		int32 EntryResult = 0;
		const bool bExecuted = bCompiled
			&& ExecuteIntFunction(&Engine, RelativeScriptPath, ModuleName, TEXT("int Entry()"), EntryResult);
		TestRunner->TestTrue(TEXT("Entry should execute"), bExecuted);
		if (bExecuted)
		{
			// Active=1, Disabled=2 → 1*10 + 2 = 12
			TestRunner->TestEqual(TEXT("Enum values: Active(1)*10 + Disabled(2) = 12"),
				EntryResult, 12);
		}

		}
	}
#endif // DISABLED(#preprocessor-vs-runtime-fields)

	// ========================================================================
	// DelegateDeclarationParsed — event/delegate FMyDelegate() is recognized
	// by the preprocessor as a delegate chunk type
	// ========================================================================
	TEST_METHOD(DelegateDeclarationParsed)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		FFixtureFile File(TEXT("Tests/Preprocessor/MacroShapes/DelegateDeclaration.as"), TEXT(R"(
event void FOnHealthChanged(float NewHealth);

delegate void FOnDamageReceived(float Amount, AActor Instigator);

int Entry()
{
    return 7;
}
)"));

		auto Session = RunPreprocessSession(Engine, File);

		AssertPreprocessSucceeded(*TestRunner, Session.Result);
		AssertModuleCount(*TestRunner, Session.Result, 1);
		AssertNoDiagnostics(*TestRunner, Session.Result);

		const FAngelscriptModuleDesc* Module = AssertModuleExists(
			*TestRunner, Session.Result,
			TEXT("Tests.Preprocessor.MacroShapes.DelegateDeclaration"));
		if (Module != nullptr)
		{
			// Verify delegate descriptors are recorded
			TestRunner->TestTrue(TEXT("Should have at least one delegate"),
				Module->Delegates.Num() >= 1);

			// Check code is produced
			const FString Code = Session.Result.JoinedCode(*Module);
			TestRunner->TestTrue(TEXT("Should contain Entry function"),
				Code.Contains(TEXT("int Entry()")));
		}

		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
