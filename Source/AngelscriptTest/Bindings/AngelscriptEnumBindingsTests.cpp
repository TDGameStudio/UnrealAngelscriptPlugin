// ============================================================================
// AngelscriptEnumBindingsTests.cpp
//
// UEnum binding coverage — CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Bindings.Enum.FAngelscriptEnumBindingsTest.*
//
// Sections:
//   NameAndIndex      — GetNameByIndex, GetNameByValue, GetIndexByName, GetValueByName
//   StringAndDisplay  — GetNameStringByIndex, GetNameStringByValue, GetIndexByNameString,
//                        GetValueByNameString, GetDisplayNameTextByValue, GenerateEnumPrefix
//   Validation        — IsValidEnumValue (valid/invalid), IsValidEnumName (valid/invalid),
//                        GetValueByName(missing), GetIndexByName(missing),
//                        GetIndexByNameString(missing)
//
// CQTest adaptation notes:
//   Native baseline values (UEnum lookups on EAttachmentRule) are computed in
//   each TEST_METHOD and injected into the AS source via ReplaceInline before
//   passing to FScopedAngelscriptModule.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#include "Engine/EngineTypes.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Helpers — compute and inject native baselines
// ----------------------------------------------------------------------------

namespace AngelscriptEnumBindingsTests_Private
{
	struct FEnumBaselines
	{
		UEnum* Enum = nullptr;
		int64 WorldValue = 0;
		FName WorldName;
		FString WorldNameString;
		int32 WorldIndex = 0;
		FString WorldDisplay;
		FString EnumPrefix;
		FName MissingName;
		FString MissingNameString;
		int64 MissingValue = 0;
		int32 MissingIndex = 0;
		int32 MissingIndexString = 0;
		FString EnumPath;

		bool Init(FAutomationTestBase& Test)
		{
			FNoDiscardAsserter Assert(Test);
			Enum = StaticEnum<EAttachmentRule>();
			if (!Assert.IsNotNull(Enum, TEXT("Enum baseline should resolve native UEnum")))
			{
				return false;
			}
			WorldValue = static_cast<int64>(EAttachmentRule::KeepWorld);
			WorldName = Enum->GetNameByValue(WorldValue);
			WorldNameString = Enum->GetNameStringByValue(WorldValue);
			WorldIndex = Enum->GetIndexByName(WorldName);
			WorldDisplay = Enum->GetDisplayNameTextByValue(WorldValue).ToString();
			EnumPrefix = Enum->GenerateEnumPrefix();
			MissingName = FName(TEXT("EAttachmentRule::DefinitelyMissing"));
			MissingNameString = TEXT("DefinitelyMissing");
			MissingValue = Enum->GetValueByName(MissingName);
			MissingIndex = Enum->GetIndexByName(MissingName);
			MissingIndexString = Enum->GetIndexByNameString(MissingNameString);
			EnumPath = Enum->GetPathName().ReplaceCharWithEscapedChar();
			return true;
		}

		void Substitute(FString& Script) const
		{
			Script.ReplaceInline(TEXT("$ENUM_PATH$"), *EnumPath);
			Script.ReplaceInline(TEXT("$WORLD_NAME$"), *WorldName.ToString().ReplaceCharWithEscapedChar());
			Script.ReplaceInline(TEXT("$WORLD_NAME_STRING$"), *WorldNameString.ReplaceCharWithEscapedChar());
			Script.ReplaceInline(TEXT("$WORLD_DISPLAY$"), *WorldDisplay.ReplaceCharWithEscapedChar());
			Script.ReplaceInline(TEXT("$ENUM_PREFIX$"), *EnumPrefix.ReplaceCharWithEscapedChar());
			Script.ReplaceInline(TEXT("$MISSING_NAME$"), *MissingName.ToString());
			Script.ReplaceInline(TEXT("$MISSING_NAME_STRING$"), *MissingNameString);
			Script.ReplaceInline(TEXT("$WORLD_INDEX$"), *LexToString(WorldIndex));
			Script.ReplaceInline(TEXT("$WORLD_VALUE$"), *LexToString(WorldValue));
			Script.ReplaceInline(TEXT("$MISSING_VALUE$"), *LexToString(MissingValue));
			Script.ReplaceInline(TEXT("$MISSING_INDEX$"), *LexToString(MissingIndex));
			Script.ReplaceInline(TEXT("$MISSING_INDEX_STRING$"), *LexToString(MissingIndexString));
		}
	};
}

// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptEnumBindingsTest,
	"Angelscript.TestModule.Bindings.Enum",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: NameAndIndex
	// ====================================================================

	TEST_METHOD(NameAndIndex)
	{
		using namespace AngelscriptEnumBindingsTests_Private;

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FEnumBaselines B;
		if (!B.Init(*TestRunner)) return;

		FString Script = TEXT(R"(
UEnum GetTestEnum()
{
	UObject EnumObject = FindObject("$ENUM_PATH$");
	return Cast<UEnum>(EnumObject);
}
int GetNameByIndex()
{
	UEnum E = GetTestEnum();
	if (!IsValid(E)) return 0;
	return (E.GetNameByIndex($WORLD_INDEX$) == FName("$WORLD_NAME$")) ? 1 : 0;
}
int GetNameByValue()
{
	UEnum E = GetTestEnum();
	if (!IsValid(E)) return 0;
	return (E.GetNameByValue($WORLD_VALUE$) == FName("$WORLD_NAME$")) ? 1 : 0;
}
int GetIndexByName()
{
	UEnum E = GetTestEnum();
	if (!IsValid(E)) return 0;
	return (E.GetIndexByName(FName("$WORLD_NAME$")) == $WORLD_INDEX$) ? 1 : 0;
}
int GetValueByName()
{
	UEnum E = GetTestEnum();
	if (!IsValid(E)) return 0;
	return (E.GetValueByName(FName("$WORLD_NAME$")) == $WORLD_VALUE$) ? 1 : 0;
}
)");
		B.Substitute(Script);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASEnum_NameAndIndex"), Script);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GetNameByIndex()"), TEXT("GetNameByIndex should return correct FName"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GetNameByValue()"), TEXT("GetNameByValue should return correct FName"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GetIndexByName()"), TEXT("GetIndexByName should return correct index"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GetValueByName()"), TEXT("GetValueByName should return correct value"), 1);
	}

	// ====================================================================
	// Section: StringAndDisplay
	// ====================================================================

	TEST_METHOD(StringAndDisplay)
	{
		using namespace AngelscriptEnumBindingsTests_Private;

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FEnumBaselines B;
		if (!B.Init(*TestRunner)) return;

		FString Script = TEXT(R"(
UEnum GetTestEnum()
{
	UObject EnumObject = FindObject("$ENUM_PATH$");
	return Cast<UEnum>(EnumObject);
}
int GetNameStringByIndex()
{
	UEnum E = GetTestEnum();
	if (!IsValid(E)) return 0;
	return (E.GetNameStringByIndex($WORLD_INDEX$) == "$WORLD_NAME_STRING$") ? 1 : 0;
}
int GetNameStringByValue()
{
	UEnum E = GetTestEnum();
	if (!IsValid(E)) return 0;
	return (E.GetNameStringByValue($WORLD_VALUE$) == "$WORLD_NAME_STRING$") ? 1 : 0;
}
int GetIndexByNameString()
{
	UEnum E = GetTestEnum();
	if (!IsValid(E)) return 0;
	return (E.GetIndexByNameString("$WORLD_NAME_STRING$") == $WORLD_INDEX$) ? 1 : 0;
}
int GetValueByNameString()
{
	UEnum E = GetTestEnum();
	if (!IsValid(E)) return 0;
	return (E.GetValueByNameString("$WORLD_NAME_STRING$") == $WORLD_VALUE$) ? 1 : 0;
}
int GetDisplayNameTextByValue()
{
	UEnum E = GetTestEnum();
	if (!IsValid(E)) return 0;
	return (E.GetDisplayNameTextByValue($WORLD_VALUE$).ToString() == "$WORLD_DISPLAY$") ? 1 : 0;
}
int GenerateEnumPrefix()
{
	UEnum E = GetTestEnum();
	if (!IsValid(E)) return 0;
	return (E.GenerateEnumPrefix() == "$ENUM_PREFIX$") ? 1 : 0;
}
)");
		B.Substitute(Script);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASEnum_StringAndDisplay"), Script);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GetNameStringByIndex()"), TEXT("GetNameStringByIndex should match native"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GetNameStringByValue()"), TEXT("GetNameStringByValue should match native"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GetIndexByNameString()"), TEXT("GetIndexByNameString should match native"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GetValueByNameString()"), TEXT("GetValueByNameString should match native"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GetDisplayNameTextByValue()"), TEXT("GetDisplayNameTextByValue should match native"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GenerateEnumPrefix()"), TEXT("GenerateEnumPrefix should match native"), 1);
	}

	// ====================================================================
	// Section: Validation
	// ====================================================================

	TEST_METHOD(Validation)
	{
		using namespace AngelscriptEnumBindingsTests_Private;

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FEnumBaselines B;
		if (!B.Init(*TestRunner)) return;

		FString Script = TEXT(R"(
UEnum GetTestEnum()
{
	UObject EnumObject = FindObject("$ENUM_PATH$");
	return Cast<UEnum>(EnumObject);
}
int IsValidEnumValue_Valid()
{
	UEnum E = GetTestEnum();
	if (!IsValid(E)) return 0;
	return E.IsValidEnumValue($WORLD_VALUE$) ? 1 : 0;
}
int IsValidEnumValue_Invalid()
{
	UEnum E = GetTestEnum();
	if (!IsValid(E)) return 0;
	return E.IsValidEnumValue(99) ? 0 : 1;
}
int IsValidEnumName_Valid()
{
	UEnum E = GetTestEnum();
	if (!IsValid(E)) return 0;
	return E.IsValidEnumName(FName("$WORLD_NAME$")) ? 1 : 0;
}
int IsValidEnumName_Invalid()
{
	UEnum E = GetTestEnum();
	if (!IsValid(E)) return 0;
	return E.IsValidEnumName(FName("$MISSING_NAME$")) ? 0 : 1;
}
int GetValueByName_Missing()
{
	UEnum E = GetTestEnum();
	if (!IsValid(E)) return 0;
	return (E.GetValueByName(FName("$MISSING_NAME$")) == $MISSING_VALUE$) ? 1 : 0;
}
int GetIndexByName_Missing()
{
	UEnum E = GetTestEnum();
	if (!IsValid(E)) return 0;
	return (E.GetIndexByName(FName("$MISSING_NAME$")) == $MISSING_INDEX$) ? 1 : 0;
}
int GetIndexByNameString_Missing()
{
	UEnum E = GetTestEnum();
	if (!IsValid(E)) return 0;
	return (E.GetIndexByNameString("$MISSING_NAME_STRING$") == $MISSING_INDEX_STRING$) ? 1 : 0;
}
)");
		B.Substitute(Script);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASEnum_Validation"), Script);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IsValidEnumValue_Valid()"), TEXT("IsValidEnumValue should accept valid value"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IsValidEnumValue_Invalid()"), TEXT("IsValidEnumValue should reject invalid value"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IsValidEnumName_Valid()"), TEXT("IsValidEnumName should accept valid name"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IsValidEnumName_Invalid()"), TEXT("IsValidEnumName should reject missing name"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GetValueByName_Missing()"), TEXT("GetValueByName with missing name should return sentinel"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GetIndexByName_Missing()"), TEXT("GetIndexByName with missing name should return sentinel"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GetIndexByNameString_Missing()"), TEXT("GetIndexByNameString with missing name should return sentinel"), 1);
	}
};

#endif
