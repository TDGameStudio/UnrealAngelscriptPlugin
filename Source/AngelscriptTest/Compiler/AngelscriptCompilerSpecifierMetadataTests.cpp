#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace CompilerSpecifierMetadataTest
{
	static const FName ModuleName(TEXT("Tests.Compiler.SpecifierStringMetadataRoundTrip"));
	static const FString RelativeScriptPath(TEXT("Tests/Compiler/SpecifierStringMetadataRoundTrip.as"));
	static const FString ClassName(TEXT("USpecifierCarrier"));
	static const FString PropertyName(TEXT("Count"));
	static const FString SpecifierFunctionName(TEXT("Compute"));
	static const FString EntryFunctionDeclaration(TEXT("int Entry()"));
	static const int32 ExpectedEntryValue = 7;
	static const FString ExpectedClassDisplayName(TEXT("Alpha, Beta"));
	static const FString ExpectedClassToolTip(TEXT("He said \\\"Hi\\\""));
	static const FString ExpectedPropertyDisplayName(TEXT("Count, Total"));
	static const FString ExpectedPropertyToolTip(TEXT("Quoted \\\"Value\\\""));
	static const FString ExpectedFunctionDisplayName(TEXT("Call, Verify"));
	static const FString ExpectedFunctionToolTip(TEXT("Escaped \\\"quote\\\""));

	FString JoinDiagnostics(const TArray<FAngelscriptCompileTraceDiagnosticSummary>& Diagnostics)
	{
		TArray<FString> Lines;
		for (const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic : Diagnostics)
		{
			Lines.Add(FString::Printf(
				TEXT("[%s] %s(%d:%d) %s"),
				Diagnostic.bIsError ? TEXT("Error") : (Diagnostic.bIsInfo ? TEXT("Info") : TEXT("Warning")),
				*Diagnostic.Section,
				Diagnostic.Row,
				Diagnostic.Column,
				*Diagnostic.Message));
		}

		return FString::Join(Lines, TEXT(" | "));
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCompilerSpecifierMetadataTests,
	"Angelscript.TestModule.Compiler.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(SpecifierStringMetadataRoundTrip)
	{


		const FString TestScriptSource = TEXT(R"AS(
	UCLASS(meta=(DisplayName="Alpha, Beta", ToolTip="He said \"Hi\""))
	class USpecifierCarrier : UObject
	{
		UPROPERTY(meta=(DisplayName="Count, Total", ToolTip="Quoted \"Value\""))
		int Count;

		UFUNCTION(meta=(DisplayName="Call, Verify", ToolTip="Escaped \"quote\""))
		int Compute()
		{
			return 7;
		}
	}

	int Entry()
	{
		return 7;
	}
	)AS");

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*CompilerSpecifierMetadataTest::ModuleName.ToString());
		};

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			CompilerSpecifierMetadataTest::ModuleName,
			CompilerSpecifierMetadataTest::RelativeScriptPath,
			TestScriptSource,
			true,
			Summary,
			true);

		if (Summary.Diagnostics.Num() > 0)
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("Specifier metadata diagnostics: %s"),
				*CompilerSpecifierMetadataTest::JoinDiagnostics(Summary.Diagnostics)));
		}

		ASSERT_THAT(IsTrue(
			bCompiled,
			TEXT("Specifier string metadata round-trip should compile successfully")));
		ASSERT_THAT(IsTrue(
			Summary.bUsedPreprocessor,
			TEXT("Specifier string metadata round-trip should report preprocessor usage")));
		ASSERT_THAT(IsTrue(
			Summary.bCompileSucceeded,
			TEXT("Specifier string metadata round-trip should mark compile succeeded in the summary")));
		ASSERT_THAT(AreEqual(
			ECompileResult::FullyHandled,
			Summary.CompileResult,
			TEXT("Specifier string metadata round-trip should report FullyHandled")));
		ASSERT_THAT(AreEqual(
			1,
			Summary.ModuleDescCount,
			TEXT("Specifier string metadata round-trip should produce exactly one module descriptor")));
		ASSERT_THAT(AreEqual(
			1,
			Summary.CompiledModuleCount,
			TEXT("Specifier string metadata round-trip should compile exactly one module")));
		ASSERT_THAT(AreEqual(
			0,
			Summary.Diagnostics.Num(),
			TEXT("Specifier string metadata round-trip should keep diagnostics empty")));
		ASSERT_THAT(AreEqual(
			1,
			Summary.ModuleNames.Num(),
			TEXT("Specifier string metadata round-trip should record exactly one module name")));
		if (Summary.ModuleNames.Num() > 0)
		{
			ASSERT_THAT(AreEqual(
				CompilerSpecifierMetadataTest::ModuleName.ToString(),
				Summary.ModuleNames[0],
				TEXT("Specifier string metadata round-trip should normalize the module name from the relative script path")));
		}
		if (!bCompiled)
		{
			return;
		}

		UClass* GeneratedClass = FindGeneratedClass(&Engine, *CompilerSpecifierMetadataTest::ClassName);
		if (!this->Assert.IsNotNull(GeneratedClass, TEXT("Specifier string metadata round-trip should materialize the generated class")))
		{
			return;
		}

		FProperty* CountProperty = FindFProperty<FProperty>(GeneratedClass, *CompilerSpecifierMetadataTest::PropertyName);
		UFunction* ComputeFunction = FindGeneratedFunction(GeneratedClass, *CompilerSpecifierMetadataTest::SpecifierFunctionName);
		if (!this->Assert.IsNotNull(CountProperty, TEXT("Specifier string metadata round-trip should materialize the generated property"))
			|| !this->Assert.IsNotNull(ComputeFunction, TEXT("Specifier string metadata round-trip should materialize the generated function")))
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			CompilerSpecifierMetadataTest::ExpectedClassDisplayName,
			GeneratedClass->GetMetaData(TEXT("DisplayName")),
			TEXT("Generated class should preserve DisplayName metadata with embedded comma")));
		ASSERT_THAT(AreEqual(
			CompilerSpecifierMetadataTest::ExpectedClassToolTip,
			GeneratedClass->GetMetaData(TEXT("ToolTip")),
			TEXT("Generated class should preserve ToolTip metadata with embedded quotes")));
		ASSERT_THAT(AreEqual(
			CompilerSpecifierMetadataTest::ExpectedPropertyDisplayName,
			CountProperty->GetMetaData(TEXT("DisplayName")),
			TEXT("Generated property should preserve DisplayName metadata with embedded comma")));
		ASSERT_THAT(AreEqual(
			CompilerSpecifierMetadataTest::ExpectedPropertyToolTip,
			CountProperty->GetMetaData(TEXT("ToolTip")),
			TEXT("Generated property should preserve ToolTip metadata with embedded quotes")));
		ASSERT_THAT(AreEqual(
			CompilerSpecifierMetadataTest::ExpectedFunctionDisplayName,
			ComputeFunction->GetMetaData(TEXT("DisplayName")),
			TEXT("Generated function should preserve DisplayName metadata with embedded comma")));
		ASSERT_THAT(AreEqual(
			CompilerSpecifierMetadataTest::ExpectedFunctionToolTip,
			ComputeFunction->GetMetaData(TEXT("ToolTip")),
			TEXT("Generated function should preserve ToolTip metadata with embedded quotes")));

		int32 EntryResult = 0;
		const bool bExecuted = ExecuteIntFunction(
			&Engine,
			CompilerSpecifierMetadataTest::RelativeScriptPath,
			CompilerSpecifierMetadataTest::ModuleName,
			CompilerSpecifierMetadataTest::EntryFunctionDeclaration,
			EntryResult);
		ASSERT_THAT(IsTrue(
			bExecuted,
			TEXT("Specifier string metadata round-trip should execute the compiled entry function")));
		if (bExecuted)
		{
			ASSERT_THAT(AreEqual(
				CompilerSpecifierMetadataTest::ExpectedEntryValue,
				EntryResult,
				TEXT("Specifier string metadata round-trip should preserve execution after metadata parsing")));
		}

		}

	}

};

#endif
