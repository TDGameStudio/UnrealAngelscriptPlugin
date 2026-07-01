#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Core/AngelscriptEngine.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptDefaultStatementSafetyTests,
	"Angelscript.TestModule.ClassGenerator.DefaultStatementSafety",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
static bool SummaryContainsDiagnosticMessage(const FAngelscriptCompileTraceSummary& Summary, const FString& ExpectedMessage)
{
	for (const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic : Summary.Diagnostics)
	{
		if (Diagnostic.Message.Contains(ExpectedMessage))
		{
			return true;
		}
	}

	return false;
}

static bool CompileSafetyScript(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	const FName ModuleName,
	const FString& ClassName,
	const FString& Script,
	const bool bExpectedCompile,
	FAngelscriptCompileTraceSummary& OutSummary)
{
	const bool bCompiled = CompileModuleWithSummary(
		&Engine,
		ECompileType::FullReload,
		ModuleName,
		FString::Printf(TEXT("%s.as"), *ModuleName.ToString()),
		Script,
		true,
		OutSummary,
		!bExpectedCompile);

	FNoDiscardAsserter LocalAssert(Test);
	if (bExpectedCompile)
	{
		if (!LocalAssert.IsTrue(bCompiled, *FString::Printf(TEXT("%s should compile"), *ClassName)))
		{
			return false;
		}

		return LocalAssert.IsNotNull(FindGeneratedClass(&Engine, *ClassName), *FString::Printf(TEXT("%s should publish a generated class"), *ClassName));
	}

	return LocalAssert.IsFalse(bCompiled, *FString::Printf(TEXT("%s should fail compilation"), *ClassName));
}

public:
	TEST_METHOD(UnsafeDuringConstructionRejectsDefaultAndConstructor)
	{
FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(TEXT("ASUnsafeDefault"));
			Engine.DiscardModule(TEXT("ASUnsafeConstructor"));
			Engine.DiscardModule(TEXT("ASUnsafeOrdinary"));
			ASTEST_RESET_ENGINE(Engine);
		};

		const FString UnsafeDefaultScript = TEXT(R"AS(
UCLASS()
class UUnsafeDefaultTarget : UObject
{
	UPROPERTY()
	int Value = 0;

	int UnsafeValue() unsafe_during_construction
	{
		return 7;
	}

	default Value = UnsafeValue();
}
)AS");

		FAngelscriptCompileTraceSummary UnsafeDefaultSummary;
		if (!CompileSafetyScript(*TestRunner, Engine, TEXT("ASUnsafeDefault"), TEXT("UUnsafeDefaultTarget"), UnsafeDefaultScript, false, UnsafeDefaultSummary))
		{ return; }
		ASSERT_THAT(IsTrue(
			SummaryContainsDiagnosticMessage(UnsafeDefaultSummary, TEXT("unsafe during construction")),
			TEXT("Unsafe default call should report unsafe construction diagnostics")));

		const FString UnsafeConstructorScript = TEXT(R"AS(
class UnsafeConstructorCarrier
{
	int Value = 0;

	int UnsafeValue() unsafe_during_construction
	{
		return 7;
	}

	UnsafeConstructorCarrier()
	{
		Value = UnsafeValue();
	}
}

int Entry()
{
	UnsafeConstructorCarrier@ Carrier = UnsafeConstructorCarrier();
	return Carrier.Value;
}
)AS");

		FAngelscriptCompileTraceSummary UnsafeConstructorSummary;
		if (!CompileSafetyScript(*TestRunner, Engine, TEXT("ASUnsafeConstructor"), TEXT("UnsafeConstructorCarrier"), UnsafeConstructorScript, false, UnsafeConstructorSummary))
		{ return; }
		ASSERT_THAT(IsTrue(
			SummaryContainsDiagnosticMessage(UnsafeConstructorSummary, TEXT("unsafe during construction")),
			TEXT("Unsafe constructor call should report unsafe construction diagnostics")));

		const FString UnsafeOrdinaryScript = TEXT(R"AS(
UCLASS()
class UUnsafeOrdinaryTarget : UObject
{
	UPROPERTY()
	int Value = 0;

	int UnsafeValue() unsafe_during_construction
	{
		return 7;
	}

	UFUNCTION()
	int Entry()
	{
		return UnsafeValue();
	}
}
)AS");

		FAngelscriptCompileTraceSummary UnsafeOrdinarySummary;
		if (!CompileSafetyScript(*TestRunner, Engine, TEXT("ASUnsafeOrdinary"), TEXT("UUnsafeOrdinaryTarget"), UnsafeOrdinaryScript, true, UnsafeOrdinarySummary))
		{ return; }

		}
	}

	TEST_METHOD(DefaultsOnlyAccess)
	{
FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(TEXT("ASDefaultsOnlyOk"));
			Engine.DiscardModule(TEXT("ASDefaultsOnlyReject"));
			ASTEST_RESET_ENGINE(Engine);
		};

		const FString DefaultsOnlyOkScript = TEXT(R"AS(
UCLASS()
class UDefaultsOnlyOkTarget : UObject
{
	UPROPERTY()
	int Value = 0;

	int BuildDefaultValue() defaults
	{
		Value = 7;
		return Value;
	}

	default Value = BuildDefaultValue();
}
)AS");

		FAngelscriptCompileTraceSummary DefaultsOnlyOkSummary;
		if (!CompileSafetyScript(*TestRunner, Engine, TEXT("ASDefaultsOnlyOk"), TEXT("UDefaultsOnlyOkTarget"), DefaultsOnlyOkScript, true, DefaultsOnlyOkSummary))
		{ return; }

		const FString DefaultsOnlyRejectScript = TEXT(R"AS(
UCLASS()
class UDefaultsOnlyRejectTarget : UObject
{
	UPROPERTY()
	int Value = 0;

	int BuildDefaultValue() defaults
	{
		Value = 7;
		return Value;
	}

	UFUNCTION()
	int Entry()
	{
		return BuildDefaultValue();
	}
}
)AS");

		FAngelscriptCompileTraceSummary DefaultsOnlyRejectSummary;
		if (!CompileSafetyScript(*TestRunner, Engine, TEXT("ASDefaultsOnlyReject"), TEXT("UDefaultsOnlyRejectTarget"), DefaultsOnlyRejectScript, false, DefaultsOnlyRejectSummary))
		{ return; }
		ASSERT_THAT(IsTrue(
			SummaryContainsDiagnosticMessage(DefaultsOnlyRejectSummary, TEXT("only accessible from default statements")),
			TEXT("Defaults-only ordinary call should report an access diagnostic")));

		}
	}
};

#endif
