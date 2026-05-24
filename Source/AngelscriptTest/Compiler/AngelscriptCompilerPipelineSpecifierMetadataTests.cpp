#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CompilerPipelineSpecifierMetadataTest
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

TEST_CLASS_WITH_FLAGS(FCompilerPipelineSpecifierMetadataTests,
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
			Engine.DiscardModule(*CompilerPipelineSpecifierMetadataTest::ModuleName.ToString());
		};

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			CompilerPipelineSpecifierMetadataTest::ModuleName,
			CompilerPipelineSpecifierMetadataTest::RelativeScriptPath,
			TestScriptSource,
			true,
			Summary,
			true);

		if (Summary.Diagnostics.Num() > 0)
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("Specifier metadata diagnostics: %s"),
				*CompilerPipelineSpecifierMetadataTest::JoinDiagnostics(Summary.Diagnostics)));
		}

		TestRunner->TestTrue(
			TEXT("Specifier string metadata round-trip should compile successfully"),
			bCompiled);
		TestRunner->TestTrue(
			TEXT("Specifier string metadata round-trip should report preprocessor usage"),
			Summary.bUsedPreprocessor);
		TestRunner->TestTrue(
			TEXT("Specifier string metadata round-trip should mark compile succeeded in the summary"),
			Summary.bCompileSucceeded);
		TestRunner->TestEqual(
			TEXT("Specifier string metadata round-trip should report FullyHandled"),
			Summary.CompileResult,
			ECompileResult::FullyHandled);
		TestRunner->TestEqual(
			TEXT("Specifier string metadata round-trip should produce exactly one module descriptor"),
			Summary.ModuleDescCount,
			1);
		TestRunner->TestEqual(
			TEXT("Specifier string metadata round-trip should compile exactly one module"),
			Summary.CompiledModuleCount,
			1);
		TestRunner->TestEqual(
			TEXT("Specifier string metadata round-trip should keep diagnostics empty"),
			Summary.Diagnostics.Num(),
			0);
		TestRunner->TestEqual(
			TEXT("Specifier string metadata round-trip should record exactly one module name"),
			Summary.ModuleNames.Num(),
			1);
		if (Summary.ModuleNames.Num() > 0)
		{
			TestRunner->TestEqual(
				TEXT("Specifier string metadata round-trip should normalize the module name from the relative script path"),
				Summary.ModuleNames[0],
				CompilerPipelineSpecifierMetadataTest::ModuleName.ToString());
		}
		if (!bCompiled)
		{
			return;
		}

		UClass* GeneratedClass = FindGeneratedClass(&Engine, *CompilerPipelineSpecifierMetadataTest::ClassName);
		if (!TestRunner->TestNotNull(TEXT("Specifier string metadata round-trip should materialize the generated class"), GeneratedClass))
		{
			return;
		}

		FProperty* CountProperty = FindFProperty<FProperty>(GeneratedClass, *CompilerPipelineSpecifierMetadataTest::PropertyName);
		UFunction* ComputeFunction = FindGeneratedFunction(GeneratedClass, *CompilerPipelineSpecifierMetadataTest::SpecifierFunctionName);
		if (!TestRunner->TestNotNull(TEXT("Specifier string metadata round-trip should materialize the generated property"), CountProperty)
			|| !TestRunner->TestNotNull(TEXT("Specifier string metadata round-trip should materialize the generated function"), ComputeFunction))
		{
			return;
		}

		TestRunner->TestEqual(
			TEXT("Generated class should preserve DisplayName metadata with embedded comma"),
			GeneratedClass->GetMetaData(TEXT("DisplayName")),
			CompilerPipelineSpecifierMetadataTest::ExpectedClassDisplayName);
		TestRunner->TestEqual(
			TEXT("Generated class should preserve ToolTip metadata with embedded quotes"),
			GeneratedClass->GetMetaData(TEXT("ToolTip")),
			CompilerPipelineSpecifierMetadataTest::ExpectedClassToolTip);
		TestRunner->TestEqual(
			TEXT("Generated property should preserve DisplayName metadata with embedded comma"),
			CountProperty->GetMetaData(TEXT("DisplayName")),
			CompilerPipelineSpecifierMetadataTest::ExpectedPropertyDisplayName);
		TestRunner->TestEqual(
			TEXT("Generated property should preserve ToolTip metadata with embedded quotes"),
			CountProperty->GetMetaData(TEXT("ToolTip")),
			CompilerPipelineSpecifierMetadataTest::ExpectedPropertyToolTip);
		TestRunner->TestEqual(
			TEXT("Generated function should preserve DisplayName metadata with embedded comma"),
			ComputeFunction->GetMetaData(TEXT("DisplayName")),
			CompilerPipelineSpecifierMetadataTest::ExpectedFunctionDisplayName);
		TestRunner->TestEqual(
			TEXT("Generated function should preserve ToolTip metadata with embedded quotes"),
			ComputeFunction->GetMetaData(TEXT("ToolTip")),
			CompilerPipelineSpecifierMetadataTest::ExpectedFunctionToolTip);

		int32 EntryResult = 0;
		const bool bExecuted = ExecuteIntFunction(
			&Engine,
			CompilerPipelineSpecifierMetadataTest::RelativeScriptPath,
			CompilerPipelineSpecifierMetadataTest::ModuleName,
			CompilerPipelineSpecifierMetadataTest::EntryFunctionDeclaration,
			EntryResult);
		TestRunner->TestTrue(
			TEXT("Specifier string metadata round-trip should execute the compiled entry function"),
			bExecuted);
		if (bExecuted)
		{
			TestRunner->TestEqual(
				TEXT("Specifier string metadata round-trip should preserve execution after metadata parsing"),
				EntryResult,
				CompilerPipelineSpecifierMetadataTest::ExpectedEntryValue);
		}

		}

	}

};

#endif
