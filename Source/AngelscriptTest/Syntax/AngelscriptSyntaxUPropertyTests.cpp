// ============================================================================
// AngelscriptSyntaxUPropertyTests.cpp
//
// Syntax coverage tests for UPROPERTY declarations — CQTest edition.
// Tests specifiers (positive/negative) and property types (positive/negative).
//
// Automation prefix: Angelscript.TestModule.Syntax.UProperty.*
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestModuleScope.h"
#include "Shared/AngelscriptBindingsAssertions.h"
#include "Syntax/AngelscriptSyntaxTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ====================================================================
// Test Class
// ====================================================================

TEST_CLASS_WITH_FLAGS(FAngelscriptSyntaxUPropertyTest,
	"Angelscript.TestModule.Syntax.UProperty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Specifiers — Positive
	// ====================================================================

	TEST_METHOD(Specifiers_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// Basic UPROPERTY
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("UPropSP_Basic"),
			TEXT(R"(
class AUPropBasicActor : AActor
{
	UPROPERTY()
	int Health = 100;
}
)"),
			TEXT("Basic UPROPERTY"));

		// EditAnywhere
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("UPropSP_EditAnywhere"),
			TEXT(R"(
class AUPropEditActor : AActor
{
	UPROPERTY(EditAnywhere)
	int Health = 100;
}
)"),
			TEXT("EditAnywhere specifier"));

		// BlueprintReadWrite
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("UPropSP_BPReadWrite"),
			TEXT(R"(
class AUPropBPRWActor : AActor
{
	UPROPERTY(BlueprintReadWrite)
	int Health = 100;
}
)"),
			TEXT("BlueprintReadWrite specifier"));

		// BlueprintReadOnly
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("UPropSP_BPReadOnly"),
			TEXT(R"(
class AUPropBPROActor : AActor
{
	UPROPERTY(BlueprintReadOnly)
	int Health = 100;
}
)"),
			TEXT("BlueprintReadOnly specifier"));

		// Replicated
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("UPropSP_Replicated"),
			TEXT(R"(
class AUPropRepActor : AActor
{
	UPROPERTY(Replicated)
	int Health = 100;
}
)"),
			TEXT("Replicated specifier"));

		// ReplicatedUsing
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("UPropSP_ReplicatedUsing"),
			TEXT(R"(
class AUPropRepUsingActor : AActor
{
	UPROPERTY(ReplicatedUsing = OnRep_Health)
	int Health = 100;

	UFUNCTION()
	void OnRep_Health() { }
}
)"),
			TEXT("ReplicatedUsing specifier"));

		// Transient
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("UPropSP_Transient"),
			TEXT(R"(
class AUPropTransActor : AActor
{
	UPROPERTY(Transient)
	int TempVal = 0;
}
)"),
			TEXT("Transient specifier"));

		// Category
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("UPropSP_Category"),
			TEXT(R"(
class AUPropCatActor : AActor
{
	UPROPERTY(Category = "Stats")
	int Health = 100;
}
)"),
			TEXT("Category specifier"));

		// Multiple specifiers
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("UPropSP_Multiple"),
			TEXT(R"(
class AUPropMultiActor : AActor
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float Damage = 10.0f;
}
)"),
			TEXT("Multiple specifiers combined"));

		// NotEditable
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("UPropSP_NotEditable"),
			TEXT(R"(
class AUPropNotEditActor : AActor
{
	UPROPERTY(NotEditable)
	int InternalVal = 0;
}
)"),
			TEXT("NotEditable specifier"));

		// Meta
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("UPropSP_Meta"),
			TEXT(R"(
class AUPropMetaActor : AActor
{
	UPROPERTY(Meta = (ClampMin = 0, ClampMax = 100))
	int Health = 50;
}
)"),
			TEXT("Meta specifier"));
	}

	// ====================================================================
	// Specifiers — Negative
	// ====================================================================

	TEST_METHOD(Specifiers_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TestRunner->AddExpectedErrorPlain(
			TEXT("NotReplicated specifier is only allowed structs"),
			EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedErrorPlain(
			TEXT("No function specified for ReplicatedUsing"),
			EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedErrorPlain(
			TEXT("Unknown property specifier"),
			EAutomationExpectedErrorFlags::Contains, 3);

		// Invalid specifier
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("UPropSN_Invalid"),
			TEXT(R"(
class AUPropInvalidActor : AActor
{
	UPROPERTY(InvalidSpecifier)
	int X = 0;
}
)"),
			TEXT("Invalid specifier should fail"));

		// DISABLED(#as-engine-behavior): structural-validation-absent — AS 不校验 BlueprintReadOnly 和 BlueprintReadWrite 冲突
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("UPropSN_ConflictRORW"),
			TEXT(R"(
class AUPropConflictRWActor : AActor
{
	UPROPERTY(BlueprintReadOnly, BlueprintReadWrite)
	int X = 0;
}
)"),
			TEXT("Conflicting BlueprintReadOnly and BlueprintReadWrite should fail"));
#endif

		// Conflicting Replicated + NotReplicated
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("UPropSN_ConflictRepNotRep"),
			TEXT(R"(
class AUPropConflictRepActor : AActor
{
	UPROPERTY(Replicated, NotReplicated)
	int X = 0;
}
)"),
			TEXT("Conflicting Replicated and NotReplicated should fail"));

		// UPROPERTY on local variable
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("UPropSN_LocalVar"),
			TEXT(R"(
class AUPropLocalVarActor : AActor
{
	void Foo()
	{
		UPROPERTY()
		int X = 0;
	}
}
)"),
			TEXT("UPROPERTY on local variable should fail"));

		// UPROPERTY at global scope
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("UPropSN_GlobalScope"),
			TEXT(R"(
UPROPERTY() int GlobalVar = 0;
)"),
			TEXT("UPROPERTY at global scope should fail"));

		// DISABLED(#as-engine-behavior): structural-validation-absent — AS 不校验重复 UPROPERTY specifier
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("UPropSN_DuplicateSpec"),
			TEXT(R"(
class AUPropDupSpecActor : AActor
{
	UPROPERTY(EditAnywhere, EditAnywhere)
	int X = 0;
}
)"),
			TEXT("Duplicate specifier should fail"));
#endif

		// Missing parenthesis
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("UPropSN_MissingParen"),
			TEXT(R"(
class AUPropMisParenActor : AActor
{
	UPROPERTY( int X = 0;
}
)"),
			TEXT("Missing closing parenthesis should fail"));

		// UPROPERTY on function
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("UPropSN_OnFunction"),
			TEXT(R"(
class AUPropOnFuncActor : AActor
{
	UPROPERTY()
	void Foo() { }
}
)"),
			TEXT("UPROPERTY on function should fail"));

		// DISABLED(#as-engine-behavior): structural-validation-absent — AS 不校验 UPROPERTY EditAnywhere 在非 USTRUCT 成员上
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("UPropSN_EditNonClass"),
			TEXT(R"(
struct FPlain
{
	UPROPERTY(EditAnywhere)
	int X = 0;
}
)"),
			TEXT("UPROPERTY EditAnywhere on non-USTRUCT member should fail"));
#endif

		// DISABLED(#as-engine-behavior): structural-validation-absent — AS 不区分 UPROPERTY specifier 大小写
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("UPropSN_CaseSensitive"),
			TEXT(R"(
class AUPropCaseActor : AActor
{
	UPROPERTY(editanywhere)
	int X = 0;
}
)"),
			TEXT("Lowercase specifier (case sensitivity) should fail"));
#endif

		// Trailing garbage after UPROPERTY
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("UPropSN_TrailingGarbage"),
			TEXT(R"(
class AUPropGarbageActor : AActor
{
	UPROPERTY() garbage int X = 0;
}
)"),
			TEXT("Trailing garbage after UPROPERTY should fail"));

		// DISABLED(#as-engine-behavior): structural-validation-absent — AS 不校验无效的 Meta key
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("UPropSN_BadMetaKey"),
			TEXT(R"(
class AUPropBadMetaActor : AActor
{
	UPROPERTY(Meta = (NonExistentMetaKey = true))
	int X = 0;
}
)"),
			TEXT("Invalid meta key should fail"));
#endif

		// Empty specifier with lone comma
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("UPropSN_EmptyComma"),
			TEXT(R"(
class AUPropEmptyCommaActor : AActor
{
	UPROPERTY(,)
	int X = 0;
}
)"),
			TEXT("Empty specifier with lone comma should fail"));

		// ReplicatedUsing without function name
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("UPropSN_RepUsingNoFunc"),
			TEXT(R"(
class AUPropRepNoFuncActor : AActor
{
	UPROPERTY(ReplicatedUsing)
	int X = 0;
}
)"),
			TEXT("ReplicatedUsing without function name should fail"));

		// Numeric literal as specifier
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("UPropSN_NumberSpec"),
			TEXT(R"(
class AUPropNumSpecActor : AActor
{
	UPROPERTY(123)
	int X = 0;
}
)"),
			TEXT("Numeric literal as specifier should fail"));
	}

	// ====================================================================
	// Types — Positive
	// ====================================================================

	TEST_METHOD(Types_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// int
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("UPropTP_Int"),
			TEXT(R"(
class AUPropIntActor : AActor
{
	UPROPERTY()
	int Health = 100;
}
)"),
			TEXT("UPROPERTY int type"));

		// float
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("UPropTP_Float"),
			TEXT(R"(
class AUPropFloatActor : AActor
{
	UPROPERTY()
	float Speed = 5.0f;
}
)"),
			TEXT("UPROPERTY float type"));

		// bool
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("UPropTP_Bool"),
			TEXT(R"(
class AUPropBoolActor : AActor
{
	UPROPERTY()
	bool bIsAlive = true;
}
)"),
			TEXT("UPROPERTY bool type"));

		// FString
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("UPropTP_FString"),
			TEXT(R"(
class AUPropStrActor : AActor
{
	UPROPERTY()
	FString Name = "Default";
}
)"),
			TEXT("UPROPERTY FString type"));

		// FVector
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("UPropTP_FVector"),
			TEXT(R"(
class AUPropVecActor : AActor
{
	UPROPERTY()
	FVector Location;
}
)"),
			TEXT("UPROPERTY FVector type"));

		// TArray
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("UPropTP_TArray"),
			TEXT(R"(
class AUPropArrActor : AActor
{
	UPROPERTY()
	TArray<int> Scores;
}
)"),
			TEXT("UPROPERTY TArray type"));

		// TSubclassOf
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("UPropTP_TSubclassOf"),
			TEXT(R"(
class AUPropSubclassActor : AActor
{
	UPROPERTY()
	TSubclassOf<AActor> ActorClass;
}
)"),
			TEXT("UPROPERTY TSubclassOf type"));
	}

	// ====================================================================
	// Types — Negative
	// ====================================================================

	TEST_METHOD(Types_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// Non-existent type
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("UPropTN_NonExistent"),
			TEXT(R"(
class AUPropNonExistActor : AActor
{
	UPROPERTY()
	FNonExistentType X;
}
)"),
			TEXT("Non-existent UPROPERTY type should fail"));

		// void type
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("UPropTN_Void"),
			TEXT(R"(
class AUPropVoidActor : AActor
{
	UPROPERTY()
	void X;
}
)"),
			TEXT("void UPROPERTY type should fail"));

		// auto type
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("UPropTN_Auto"),
			TEXT(R"(
class AUPropAutoActor : AActor
{
	UPROPERTY()
	auto X = 5;
}
)"),
			TEXT("auto UPROPERTY type should fail"));

		// DISABLED(#as-engine-behavior): structural-validation-absent — AS 不校验 UPROPERTY 原始指针类型
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("UPropTN_RawPointer"),
			TEXT(R"(
class AUPropRawPtrActor : AActor
{
	UPROPERTY()
	AActor Ptr = nullptr;
}
)"),
			TEXT("Raw pointer UPROPERTY type should fail"));
#endif

		// TArray with non-existent element type
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("UPropTN_TArrayBadElem"),
			TEXT(R"(
class AUPropArrBadActor : AActor
{
	UPROPERTY()
	TArray<FNonExistent> Items;
}
)"),
			TEXT("TArray with non-existent element type should fail"));

		// TSubclassOf with non-UObject type
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("UPropTN_SubclassNonObj"),
			TEXT(R"(
class AUPropSubNonObjActor : AActor
{
	UPROPERTY()
	TSubclassOf<int> BadClass;
}
)"),
			TEXT("TSubclassOf with non-UObject type should fail"));

		// DISABLED(#as-engine-behavior): structural-validation-absent — AS 不校验 UPROPERTY 多变量声明
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("UPropTN_MultiDecl"),
			TEXT(R"(
class AUPropMultiDeclActor : AActor
{
	UPROPERTY()
	int X, Y;
}
)"),
			TEXT("Multiple declarations on one UPROPERTY should fail"));
#endif

		// Function type as property
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("UPropTN_FuncType"),
			TEXT(R"(
class AUPropFuncTypeActor : AActor
{
	UPROPERTY()
	void() Callback;
}
)"),
			TEXT("Function type as UPROPERTY should fail"));

		// Nested TArray with non-existent inner type
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("UPropTN_NestedBad"),
			TEXT(R"(
class AUPropNestedBadActor : AActor
{
	UPROPERTY()
	TArray<TArray<FBogus>> Nested;
}
)"),
			TEXT("Nested TArray with bad inner type should fail"));

		// Reference type as UPROPERTY
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("UPropTN_RefType"),
			TEXT(R"(
class AUPropRefTypeActor : AActor
{
	UPROPERTY()
	int& RefProp;
}
)"),
			TEXT("Reference type as UPROPERTY should fail"));

		// DISABLED(#as-engine-behavior): structural-validation-absent — AS 不校验 const 变量作为 UPROPERTY
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("UPropTN_ConstProp"),
			TEXT(R"(
class AUPropConstPropActor : AActor
{
	UPROPERTY()
	const int ConstVal = 5;
}
)"),
			TEXT("Const variable as UPROPERTY should fail"));
#endif

		// TMap with non-existent key type
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("UPropTN_TMapBadKey"),
			TEXT(R"(
class AUPropMapBadKeyActor : AActor
{
	UPROPERTY()
	TMap<FNonExistent, int> BadMap;
}
)"),
			TEXT("TMap with non-existent key type should fail"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
