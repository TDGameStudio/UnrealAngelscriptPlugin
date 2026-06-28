#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageUInterfaceTests
// -----------------------------------------------------------------------------
// Current fork boundary coverage for script-declared UINTERFACE/interface usage.
//
// The AS 2.33-based fork does not support script-level interface declarations or
// TScriptInterface<I> script types. These tests keep the coverage matrix honest
// by proving the unsupported forms fail at compile time with explicit diagnostics.
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageUInterfaceTest,
	"Angelscript.TestModule.Coverage.UInterface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool ExpectUInterfaceBoundaryRejected(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const TCHAR* ModuleName,
		const FString& Source,
		const TCHAR* Label,
		TArrayView<const FString> ExpectedDiagnostics)
	{
		return CompileAndExpectFailure(
			Test,
			Engine,
			ModuleName,
			*Source,
			Label,
			ExpectedDiagnostics);
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

	TEST_METHOD(ScriptInterfaceKeywordRejected)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("Virtual property syntax has been removed"));

		const FString ScriptSource = ASTEST_AS(R"AS(
			interface ICoverageUnsupportedInterface
			{
				void Execute();
			}
			)AS");
		ASSERT_THAT(IsTrue(ExpectUInterfaceBoundaryRejected(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUInterface_InterfaceKeywordUnsupported"),
			ScriptSource,
			TEXT("script-level interface keyword should remain unsupported in this fork"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	TEST_METHOD(UInterfaceMacroDeclarationRejected)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("Expected identifier"));
		ExpectedDiagnostics.Add(TEXT("Instead found '('"));

		const FString ScriptSource = ASTEST_AS(R"AS(
			UINTERFACE()
			interface ICoverageUnsupportedUInterface
			{
				void Execute();
			}
			)AS");
		ASSERT_THAT(IsTrue(ExpectUInterfaceBoundaryRejected(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUInterface_MacroDeclarationUnsupported"),
			ScriptSource,
			TEXT("UINTERFACE() script declarations should remain unsupported in this fork"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	TEST_METHOD(UInterfaceSpecifierDeclarationRejected)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("Expected identifier"));
		ExpectedDiagnostics.Add(TEXT("Instead found '('"));

		const FString ScriptSource = ASTEST_AS(R"AS(
			UINTERFACE(BlueprintType)
			interface ICoverageUnsupportedBlueprintTypeInterface
			{
				UFUNCTION(BlueprintCallable)
				int GetValue();
			}
			)AS");
		ASSERT_THAT(IsTrue(ExpectUInterfaceBoundaryRejected(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUInterface_SpecifierUnsupported"),
			ScriptSource,
			TEXT("UINTERFACE specifiers should remain unsupported in script declarations"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	TEST_METHOD(UInterfaceBlueprintableSpecifierRejected)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("Expected identifier"));
		ExpectedDiagnostics.Add(TEXT("Instead found '('"));

		const FString ScriptSource = ASTEST_AS(R"AS(
			UINTERFACE(Blueprintable)
			interface ICoverageUnsupportedBlueprintableInterface
			{
				void Execute();
			}
			)AS");
		ASSERT_THAT(IsTrue(ExpectUInterfaceBoundaryRejected(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUInterface_BlueprintableSpecifierUnsupported"),
			ScriptSource,
			TEXT("UINTERFACE(Blueprintable) should remain unsupported in script declarations"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	TEST_METHOD(GeneratedBodyInsideInterfaceRejected)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("Expected method or property"));
		ExpectedDiagnostics.Add(TEXT("Instead found identifier 'GENERATED_BODY'"));

		const FString ScriptSource = ASTEST_AS(R"AS(
			interface ICoverageUnsupportedGeneratedBodyInterface
			{
				GENERATED_BODY()
			}
			)AS");
		ASSERT_THAT(IsTrue(ExpectUInterfaceBoundaryRejected(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUInterface_GeneratedBodyUnsupported"),
			ScriptSource,
			TEXT("GENERATED_BODY() inside script interface declarations should remain unsupported"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	TEST_METHOD(TScriptInterfaceTypeRejected)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("Expected method or property"));
		ExpectedDiagnostics.Add(TEXT("Instead found identifier 'TScriptInterface'"));

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUnsupportedTScriptInterfaceActor : AActor
			{
				UPROPERTY()
				TScriptInterface<ICoverageUnsupportedInterface> InterfaceRef;
			}
			)AS");
		ASSERT_THAT(IsTrue(ExpectUInterfaceBoundaryRejected(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUInterface_TScriptInterfaceUnsupported"),
			ScriptSource,
			TEXT("TScriptInterface<I> script properties should remain unsupported without script interfaces"),
			MakeArrayView(ExpectedDiagnostics))));
	}

	TEST_METHOD(TScriptInterfaceArrayRejected)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("Expected method or property"));
		ExpectedDiagnostics.Add(TEXT("Instead found identifier 'TArray'"));

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUnsupportedTScriptInterfaceArrayActor : AActor
			{
				UPROPERTY()
				TArray<TScriptInterface<ICoverageUnsupportedInterface>> InterfaceRefs;
			}
			)AS");
		ASSERT_THAT(IsTrue(ExpectUInterfaceBoundaryRejected(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUInterface_TScriptInterfaceArrayUnsupported"),
			ScriptSource,
			TEXT("TArray<TScriptInterface<I>> should remain an explicit unsupported container boundary"),
			MakeArrayView(ExpectedDiagnostics))));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
