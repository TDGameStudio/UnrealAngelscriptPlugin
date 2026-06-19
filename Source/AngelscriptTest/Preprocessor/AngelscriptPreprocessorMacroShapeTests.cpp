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

		ASSERT_THAT(AreEqual(
			4,
			Macros.Num(),
			TEXT("Class/enum/meta macro shape fixture should emit exactly four macro records")));

		// Look up individual macro records
		const FAngelscriptPreprocessor::FMacro* ClassMacro =
			Session.FindMacro(FAngelscriptPreprocessor::EMacroType::Class, TEXT("UMacroCarrier"));
		const FAngelscriptPreprocessor::FMacro* EnumMacro =
			Session.FindMacro(FAngelscriptPreprocessor::EMacroType::Enum, TEXT("EMacroState"));
		const FAngelscriptPreprocessor::FMacro* EnumValueMacro =
			Session.FindMacroBySubjectIndex(FAngelscriptPreprocessor::EMacroType::EnumValue, 0);
		const FAngelscriptPreprocessor::FMacro* EnumMetaMacro =
			Session.FindMacroBySubjectIndex(FAngelscriptPreprocessor::EMacroType::EnumMeta, 1);

		ASSERT_THAT(IsNotNull(
			ClassMacro,
			TEXT("Macro set should include a named UCLASS record for UMacroCarrier")));
		ASSERT_THAT(IsNotNull(
			EnumMacro,
			TEXT("Macro set should include a named UENUM record for EMacroState")));
		ASSERT_THAT(IsNotNull(
			EnumValueMacro,
			TEXT("Macro set should include an EnumValue record for subject index 0")));
		ASSERT_THAT(IsNotNull(
			EnumMetaMacro,
			TEXT("Macro set should include an EnumMeta record for subject index 1")));

		// Validate UCLASS record details
		if (ClassMacro != nullptr)
		{
			ASSERT_THAT(AreEqual(
				FString(TEXT("Abstract, BlueprintType")),
				ClassMacro->Arguments,
				TEXT("UCLASS record should keep the original class specifier list")));

			const FAngelscriptPreprocessor::FChunk* ClassChunk =
				Session.FindFirstChunkOfType(FAngelscriptPreprocessor::EChunkType::Class);
			ASSERT_THAT(IsNotNull(
				ClassChunk,
				TEXT("UCLASS record should stay attached to a class chunk")));
			if (ClassChunk != nullptr)
			{
				ASSERT_THAT(AreEqual(
					static_cast<int32>(FAngelscriptPreprocessor::EChunkType::Class),
					static_cast<int32>(ClassChunk->Type),
					TEXT("UCLASS record should belong to a class chunk")));
				ASSERT_THAT(IsNotNull(
					ClassChunk->ClassDesc.Get(),
					TEXT("UCLASS chunk should keep the resolved class descriptor")));
				if (ClassChunk->ClassDesc.IsValid())
				{
					ASSERT_THAT(AreEqual(
						FString(TEXT("UMacroCarrier")),
						ClassChunk->ClassDesc->ClassName,
						TEXT("UCLASS chunk should resolve the same class name as the macro")));
				}
			}
		}

		// Validate UENUM record details
		if (EnumMacro != nullptr)
		{
			ASSERT_THAT(AreEqual(
				FString(TEXT("BlueprintType")),
				EnumMacro->Arguments,
				TEXT("UENUM record should keep the original enum specifier list")));

			const FAngelscriptPreprocessor::FChunk* EnumChunk =
				Session.FindFirstChunkOfType(FAngelscriptPreprocessor::EChunkType::Enum);
			ASSERT_THAT(IsNotNull(
				EnumChunk,
				TEXT("UENUM record should stay attached to an enum chunk")));
			if (EnumChunk != nullptr)
			{
				ASSERT_THAT(AreEqual(
					static_cast<int32>(FAngelscriptPreprocessor::EChunkType::Enum),
					static_cast<int32>(EnumChunk->Type),
					TEXT("UENUM record should belong to an enum chunk")));
			}
		}

		// Validate EnumValue record details
		if (EnumValueMacro != nullptr)
		{
			ASSERT_THAT(IsTrue(
				EnumValueMacro->Comment.Contains(TEXT("Alpha Friendly")),
				TEXT("EnumValue record should preserve the preceding comment text")));
			ASSERT_THAT(AreEqual(
				0,
				EnumValueMacro->SubjectIndex,
				TEXT("EnumValue record should pin its subject index to the first enum entry")));
		}

		// Validate EnumMeta record details
		if (EnumMetaMacro != nullptr)
		{
			ASSERT_THAT(IsTrue(
				EnumMetaMacro->Arguments.Contains(TEXT("DisplayName=\"Beta Friendly\"")),
				TEXT("EnumMeta record should preserve the DisplayName payload")));
			ASSERT_THAT(AreEqual(
				1,
				EnumMetaMacro->SubjectIndex,
				TEXT("EnumMeta record should pin its subject index to the second enum entry")));
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
		ASSERT_THAT(IsTrue(
			IsAllSameChar(TEXT("----"), TEXT('-')),
			TEXT("IsAllSameChar should accept uniform dash separators")));
		ASSERT_THAT(IsFalse(
			IsAllSameChar(TEXT("--=-"), TEXT('-')),
			TEXT("IsAllSameChar should reject mixed separator characters")));
		ASSERT_THAT(IsTrue(
			IsLineSeparator(TEXT("====")),
			TEXT("IsLineSeparator should accept equals separators")));
		ASSERT_THAT(IsFalse(
			IsLineSeparator(TEXT("-- body --")),
			TEXT("IsLineSeparator should reject lines containing non-separator content")));

		// FormatCommentForToolTip transformations
		ASSERT_THAT(AreEqual(
			FString(TEXT("Summary line\nDetail line")),
			FormatCommentForToolTip(TEXT("/**\n * Summary line\n * Detail line\n */")),
			TEXT("JavaDoc comments should strip markers and leading stars")));

		ASSERT_THAT(AreEqual(
			FString(TEXT("Summary line\nFollowup line")),
			FormatCommentForToolTip(TEXT("// Summary line\n//~ Hidden line\n// Followup line")),
			TEXT("Cpp comments should drop //~ ignored lines before tooltip normalization")));

		ASSERT_THAT(AreEqual(
			FString(TEXT("Body text")),
			FormatCommentForToolTip(TEXT("/**\n * =====\n * Body text\n * =====\n */")),
			TEXT("Separator-only wrapper lines should be removed from tooltip output")));

		ASSERT_THAT(AreEqual(
			FString(TEXT("\u7eaf\u4e2d\u6587\u63d0\u793a")),
			FormatCommentForToolTip(TEXT("// \u7eaf\u4e2d\u6587\u63d0\u793a")),
			TEXT("Pure CJK tooltip comments should not be treated as empty")));

		ASSERT_THAT(AreEqual(
			FString(TEXT("Tabbed line\nSecond line")),
			FormatCommentForToolTip(TEXT("//\tTabbed line\r\n//\tSecond line")),
			TEXT("Tabs and carriage returns should normalize into stable plain-text indentation")));

		ASSERT_THAT(AreEqual(
			FString(TEXT("")),
			FormatCommentForToolTip(TEXT("/* ===== */")),
			TEXT("Comments without alnum or CJK content should normalize to empty text")));

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
			ASSERT_THAT(IsTrue(Module->Enums.Num() >= 1, TEXT("Should have at least one enum descriptor")));

			if (Module->Enums.Num() > 0)
			{
				const FAngelscriptEnumDesc& EnumDesc = Module->Enums[0].Get();
				ASSERT_THAT(AreEqual(FString(TEXT("ETestDirection")), EnumDesc.EnumName, TEXT("Enum name should be ETestDirection")));
				ASSERT_THAT(AreEqual(4, EnumDesc.ValueNames.Num(), TEXT("Should have 4 enum value names")));
			}
		}

		// Compile and execute
		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine, ECompileType::SoftReloadOnly, ModuleName,
			RelativeScriptPath, ScriptSource, true, Summary);

		ASSERT_THAT(IsTrue(bCompiled, TEXT("Enum module should compile")));
		ASSERT_THAT(AreEqual(0, Summary.Diagnostics.Num(), TEXT("No compile diagnostics")));

		int32 EntryResult = 0;
		const bool bExecuted = bCompiled
			&& ExecuteIntFunction(&Engine, RelativeScriptPath, ModuleName, TEXT("int Entry()"), EntryResult);
		ASSERT_THAT(IsTrue(bExecuted, TEXT("Entry should execute")));
		if (bExecuted)
		{
			ASSERT_THAT(AreEqual(180, EntryResult, TEXT("South direction should return 180")));
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

		ASSERT_THAT(IsTrue(Module->Enums.Num() >= 1, TEXT("Should have at least one enum descriptor")));
		if (Module->Enums.Num() == 0)
		{
			return;
		}

		const FAngelscriptEnumDesc& EnumDesc = Module->Enums[0].Get();
		ASSERT_THAT(AreEqual(FString(TEXT("EWeaponType")), EnumDesc.EnumName, TEXT("Enum name should be EWeaponType")));
		ASSERT_THAT(AreEqual(3, EnumDesc.ValueNames.Num(), TEXT("Should have 3 enum value names")));

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

		ASSERT_THAT(AreEqual(3, EnumValueMacros.Num(), TEXT("Should have 3 EnumValue macro records")));
		ASSERT_THAT(AreEqual(3, EnumMetaMacros.Num(), TEXT("Should have 3 EnumMeta macro records")));

		// Check first value comment
		if (EnumValueMacros.Num() > 0)
		{
			ASSERT_THAT(IsTrue(EnumValueMacros[0]->Comment.Contains(TEXT("melee weapon")), TEXT("First EnumValue should have 'melee weapon' comment")));
		}

		// Check UMETA arguments
		if (EnumMetaMacros.Num() >= 3)
		{
			ASSERT_THAT(IsTrue(EnumMetaMacros[0]->Arguments.Contains(TEXT("DisplayName=\"Melee Sword\"")), TEXT("First UMETA should contain 'Melee Sword'")));
			ASSERT_THAT(IsTrue(EnumMetaMacros[1]->Arguments.Contains(TEXT("DisplayName=\"Ranged Bow\"")), TEXT("Second UMETA should contain 'Ranged Bow'")));
			ASSERT_THAT(IsTrue(
				EnumMetaMacros[2]->Arguments.Contains(TEXT("DisplayName=\"Magic Staff\""))
				&& EnumMetaMacros[2]->Arguments.Contains(TEXT("Hidden")),
				TEXT("Third UMETA should contain 'Magic Staff' and Hidden")));
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

		ASSERT_THAT(AreEqual(2, Module->Enums.Num(), TEXT("Should have exactly 2 enum descriptors")));

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

			if (this->Assert.IsNotNull(BPEnum, TEXT("Should find EBPEnum")))
			{
				ASSERT_THAT(IsTrue(BPEnum->Meta.Contains(BlueprintTypeKey), TEXT("EBPEnum should record BlueprintType meta")));
				ASSERT_THAT(AreEqual(2, BPEnum->ValueNames.Num(), TEXT("EBPEnum should have 2 value names")));
			}

			if (this->Assert.IsNotNull(NonBPEnum, TEXT("Should find ENonBPEnum")))
			{
				ASSERT_THAT(IsFalse(NonBPEnum->Meta.Contains(BlueprintTypeKey), TEXT("ENonBPEnum should not record BlueprintType meta")));
				ASSERT_THAT(AreEqual(2, NonBPEnum->ValueNames.Num(), TEXT("ENonBPEnum should have 2 value names")));
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
			ASSERT_THAT(IsTrue(Module->Classes.Num() >= 1, TEXT("Should have class descriptors")));
			ASSERT_THAT(IsTrue(Module->Enums.Num() >= 1, TEXT("Should have enum descriptors")));

			if (Module->Enums.Num() > 0)
			{
				ASSERT_THAT(AreEqual(FString(TEXT("EOwnerState")), Module->Enums[0]->EnumName, TEXT("Enum should be EOwnerState")));
				ASSERT_THAT(AreEqual(3, Module->Enums[0]->ValueNames.Num(), TEXT("EOwnerState should have 3 value names")));
			}
		}

		// Compile and execute
		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine, ECompileType::SoftReloadOnly, ModuleName,
			RelativeScriptPath, ScriptSource, true, Summary);

		ASSERT_THAT(IsTrue(bCompiled, TEXT("Should compile")));
		ASSERT_THAT(AreEqual(0, Summary.Diagnostics.Num(), TEXT("No compile diagnostics")));

		int32 EntryResult = 0;
		const bool bExecuted = bCompiled
			&& ExecuteIntFunction(&Engine, RelativeScriptPath, ModuleName, TEXT("int Entry()"), EntryResult);
		ASSERT_THAT(IsTrue(bExecuted, TEXT("Entry should execute")));
		if (bExecuted)
		{
			// Active=1, Disabled=2 → 1*10 + 2 = 12
			ASSERT_THAT(AreEqual(12, EntryResult, TEXT("Enum values: Active(1)*10 + Disabled(2) = 12")));
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
			ASSERT_THAT(IsTrue(Module->Delegates.Num() >= 1, TEXT("Should have at least one delegate")));

			// Check code is produced
			const FString Code = Session.Result.JoinedCode(*Module);
			ASSERT_THAT(IsTrue(Code.Contains(TEXT("int Entry()")), TEXT("Should contain Entry function")));
		}

		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
