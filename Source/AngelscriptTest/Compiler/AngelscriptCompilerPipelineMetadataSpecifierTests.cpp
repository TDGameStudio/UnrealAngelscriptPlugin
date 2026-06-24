#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "Core/AngelscriptEngine.h"
#include "Preprocessor/AngelscriptPreprocessor.h"

#include "HAL/FileManager.h"
#include "CQTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CompilerPipelineMetadataSpecifierTest
{
	static const FName ModuleName(TEXT("Tests.Compiler.MacroMetadataStringsWithClosingParen"));
	static const FString RelativeScriptPath(TEXT("Tests/Compiler/MacroMetadataStringsWithClosingParen.as"));
	static const FString ClassName(TEXT("UCompilerMetadataParenCarrier"));
	static const FString MetaFunctionName(TEXT("GetClosingParenText"));
	static const FString EnumName(TEXT("ECompilerMetadataParenState"));

	static const FString ExpectedClassDisplayName(TEXT("Do (Test)"));
	static const FString ExpectedClassToolTip(TEXT("Class accepts ) text"));
	static const FString ExpectedFunctionDisplayName(TEXT("Run ) Now"));
	static const FString ExpectedFunctionToolTip(TEXT("Accepts ) in text"));
	static const FString ExpectedEnumToolTip(TEXT("Enum ) ToolTip"));
	static const FString ExpectedEnumValueDisplayName(TEXT("Alpha ) Value"));
	static const FString ExpectedEnumValueToolTip(TEXT("Alpha ) ToolTip"));

	FString GetFixtureRoot()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("CompilerMetadataSpecifierFixtures"));
	}

	FString WriteFixture(const FString& RelativePath, const FString& Contents)
	{
		const FString AbsolutePath = FPaths::Combine(GetFixtureRoot(), RelativePath);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(AbsolutePath), true);
		FFileHelper::SaveStringToFile(Contents, *AbsolutePath);
		return AbsolutePath;
	}

	TArray<FString> CollectDiagnosticMessages(
		const FAngelscriptEngine& Engine,
		const FString& AbsoluteFilename,
		int32& OutErrorCount)
	{
		OutErrorCount = 0;

		const FAngelscriptEngine::FDiagnostics* Diagnostics = Engine.Diagnostics.Find(AbsoluteFilename);
		if (Diagnostics == nullptr)
		{
			return {};
		}

		TArray<FString> Messages;
		for (const FAngelscriptEngine::FDiagnostic& Diagnostic : Diagnostics->Diagnostics)
		{
			Messages.Add(Diagnostic.Message);
			if (Diagnostic.bIsError)
			{
				++OutErrorCount;
			}
		}

		return Messages;
	}

	FString GetClassMeta(const TSharedPtr<FAngelscriptClassDesc>& ClassDesc, const TCHAR* Key)
	{
		if (!ClassDesc.IsValid())
		{
			return FString();
		}

		const FString* Value = ClassDesc->Meta.Find(FName(Key));
		return Value != nullptr ? *Value : FString();
	}

	FString GetFunctionMeta(const TSharedPtr<FAngelscriptFunctionDesc>& FunctionDesc, const TCHAR* Key)
	{
		if (!FunctionDesc.IsValid())
		{
			return FString();
		}

		const FString* Value = FunctionDesc->Meta.Find(FName(Key));
		return Value != nullptr ? *Value : FString();
	}

	FString GetEnumMeta(const TSharedPtr<FAngelscriptEnumDesc>& EnumDesc, const TCHAR* Key, int32 ValueIndex)
	{
		if (!EnumDesc.IsValid())
		{
			return FString();
		}

		const FString* Value = EnumDesc->Meta.Find(TPair<FName, int32>(FName(Key), ValueIndex));
		return Value != nullptr ? *Value : FString();
	}
}

TEST_CLASS_WITH_FLAGS(FCompilerPipelineMetadataSpecifierTests,
	"Angelscript.TestModule.Compiler.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(MacroMetadataStringsWithClosingParen)
	{


		const FString ScriptSource = TEXT(R"AS(
	UCLASS(meta=(DisplayName="Do (Test)", ToolTip="Class accepts ) text"))
	class UCompilerMetadataParenCarrier : UObject
	{
		UFUNCTION(meta=(DisplayName="Run ) Now", ToolTip="Accepts ) in text"))
		int GetClosingParenText()
		{
			return 7;
		}
	}

	UENUM(meta=(ToolTip="Enum ) ToolTip"))
	enum class ECompilerMetadataParenState : uint8
	{
		Alpha UMETA(DisplayName="Alpha ) Value", ToolTip="Alpha ) ToolTip"),
		Beta
	}
	)AS");

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		const FString AbsoluteScriptPath = CompilerPipelineMetadataSpecifierTest::WriteFixture(
			CompilerPipelineMetadataSpecifierTest::RelativeScriptPath,
			ScriptSource);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*CompilerPipelineMetadataSpecifierTest::ModuleName.ToString());
			IFileManager::Get().Delete(*AbsoluteScriptPath, false, true);
		};

		Engine.ResetDiagnostics();

		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddFile(CompilerPipelineMetadataSpecifierTest::RelativeScriptPath, AbsoluteScriptPath);

		const bool bPreprocessSucceeded = Preprocessor.Preprocess();
		const TArray<TSharedRef<FAngelscriptModuleDesc>> Modules = Preprocessor.GetModulesToCompile();

		int32 PreprocessErrorCount = 0;
		const TArray<FString> PreprocessMessages = CompilerPipelineMetadataSpecifierTest::CollectDiagnosticMessages(
			Engine,
			AbsoluteScriptPath,
			PreprocessErrorCount);

		ASSERT_THAT(IsTrue(
			bPreprocessSucceeded,
			TEXT("Metadata specifier test case should preprocess successfully")));
		ASSERT_THAT(AreEqual(
			0,
			PreprocessErrorCount,
			TEXT("Metadata specifier test case should not emit preprocessing errors")));
		ASSERT_THAT(AreEqual(
			0,
			PreprocessMessages.Num(),
			TEXT("Metadata specifier test case should keep preprocessing diagnostics empty")));
		ASSERT_THAT(AreEqual(
			1,
			Modules.Num(),
			TEXT("Metadata specifier test case should produce exactly one module descriptor")));
		if (!bPreprocessSucceeded || Modules.Num() != 1)
		{
			return;
		}

		const TSharedRef<FAngelscriptModuleDesc> ModuleDesc = Modules[0];
		ASSERT_THAT(AreEqual(
			CompilerPipelineMetadataSpecifierTest::ModuleName.ToString(),
			ModuleDesc->ModuleName,
			TEXT("Metadata specifier test case should preserve the expected module name")));

		const TSharedPtr<FAngelscriptClassDesc> ClassDesc = ModuleDesc->GetClass(CompilerPipelineMetadataSpecifierTest::ClassName);
		if (!this->Assert.IsTrue(ClassDesc.IsValid(), TEXT("Metadata specifier test case should parse the annotated class descriptor")))
		{
			return;
		}

		const TSharedPtr<FAngelscriptFunctionDesc> FunctionDesc = ClassDesc->GetMethod(CompilerPipelineMetadataSpecifierTest::MetaFunctionName);
		if (!this->Assert.IsTrue(FunctionDesc.IsValid(), TEXT("Metadata specifier test case should parse the annotated function descriptor")))
		{
			return;
		}

		const TSharedPtr<FAngelscriptEnumDesc> EnumDesc = ModuleDesc->GetEnum(CompilerPipelineMetadataSpecifierTest::EnumName);
		if (!this->Assert.IsTrue(EnumDesc.IsValid(), TEXT("Metadata specifier test case should parse the annotated enum descriptor")))
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			CompilerPipelineMetadataSpecifierTest::ExpectedClassDisplayName,
			CompilerPipelineMetadataSpecifierTest::GetClassMeta(ClassDesc, TEXT("DisplayName")),
			TEXT("Preprocessor should preserve the class DisplayName metadata that contains balanced parentheses")));
		ASSERT_THAT(AreEqual(
			CompilerPipelineMetadataSpecifierTest::ExpectedClassToolTip,
			CompilerPipelineMetadataSpecifierTest::GetClassMeta(ClassDesc, TEXT("ToolTip")),
			TEXT("Preprocessor should preserve the class ToolTip metadata that contains a closing parenthesis")));
		ASSERT_THAT(AreEqual(
			CompilerPipelineMetadataSpecifierTest::ExpectedFunctionDisplayName,
			CompilerPipelineMetadataSpecifierTest::GetFunctionMeta(FunctionDesc, TEXT("DisplayName")),
			TEXT("Preprocessor should preserve the function DisplayName metadata that contains a closing parenthesis")));
		ASSERT_THAT(AreEqual(
			CompilerPipelineMetadataSpecifierTest::ExpectedFunctionToolTip,
			CompilerPipelineMetadataSpecifierTest::GetFunctionMeta(FunctionDesc, TEXT("ToolTip")),
			TEXT("Preprocessor should preserve the function ToolTip metadata that contains a closing parenthesis")));
		ASSERT_THAT(AreEqual(
			CompilerPipelineMetadataSpecifierTest::ExpectedEnumToolTip,
			CompilerPipelineMetadataSpecifierTest::GetEnumMeta(EnumDesc, TEXT("ToolTip"), INDEX_NONE),
			TEXT("Preprocessor should preserve the enum ToolTip metadata that contains a closing parenthesis")));
		ASSERT_THAT(AreEqual(
			CompilerPipelineMetadataSpecifierTest::ExpectedEnumValueDisplayName,
			CompilerPipelineMetadataSpecifierTest::GetEnumMeta(EnumDesc, TEXT("DisplayName"), 0),
			TEXT("Preprocessor should preserve the enum value DisplayName metadata that contains a closing parenthesis")));
		ASSERT_THAT(AreEqual(
			CompilerPipelineMetadataSpecifierTest::ExpectedEnumValueToolTip,
			CompilerPipelineMetadataSpecifierTest::GetEnumMeta(EnumDesc, TEXT("ToolTip"), 0),
			TEXT("Preprocessor should preserve the enum value ToolTip metadata that contains a closing parenthesis")));

		Engine.ResetDiagnostics();

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			CompilerPipelineMetadataSpecifierTest::ModuleName,
			CompilerPipelineMetadataSpecifierTest::RelativeScriptPath,
			ScriptSource,
			true,
			Summary,
			true);

		ASSERT_THAT(IsTrue(
			bCompiled,
			TEXT("Metadata specifier test case should compile through the normal preprocessor pipeline")));
		ASSERT_THAT(IsTrue(
			Summary.bUsedPreprocessor,
			TEXT("Metadata specifier test case should report that it used the preprocessor")));
		ASSERT_THAT(IsTrue(
			Summary.bCompileSucceeded,
			TEXT("Metadata specifier test case should mark compile succeeded in the summary")));
		if (Summary.Diagnostics.Num() > 0)
		{
			TArray<FString> DiagnosticMessages;
			for (const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic : Summary.Diagnostics)
			{
				DiagnosticMessages.Add(FString::Printf(
					TEXT("[%s] %s"),
					Diagnostic.bIsError ? TEXT("Error") : TEXT("Warning"),
					*Diagnostic.Message));
			}

			TestRunner->AddInfo(FString::Printf(
				TEXT("Compile diagnostics: %s"),
				*FString::Join(DiagnosticMessages, TEXT(" | "))));
		}
		ASSERT_THAT(AreEqual(
			0,
			Summary.Diagnostics.Num(),
			TEXT("Metadata specifier test case should keep compile diagnostics empty")));
		if (!bCompiled)
		{
			return;
		}

		UClass* GeneratedClass = FindGeneratedClass(&Engine, *CompilerPipelineMetadataSpecifierTest::ClassName);
		if (!this->Assert.IsNotNull(GeneratedClass, TEXT("Metadata specifier test case should materialize the generated class")))
		{
			return;
		}

		UFunction* GeneratedFunction = FindGeneratedFunction(GeneratedClass, *CompilerPipelineMetadataSpecifierTest::MetaFunctionName);
		if (!this->Assert.IsNotNull(GeneratedFunction, TEXT("Metadata specifier test case should materialize the generated function")))
		{
			return;
		}

		const TSharedPtr<FAngelscriptEnumDesc> GeneratedEnumDesc = Engine.GetEnum(CompilerPipelineMetadataSpecifierTest::EnumName);
		if (!this->Assert.IsTrue(GeneratedEnumDesc.IsValid(), TEXT("Metadata specifier test case should register the generated enum descriptor")))
		{
			return;
		}
		if (!this->Assert.IsNotNull(GeneratedEnumDesc->Enum, TEXT("Metadata specifier test case should materialize the generated UEnum")))
		{
			return;
		}

		UEnum* GeneratedEnum = GeneratedEnumDesc->Enum;
		ASSERT_THAT(AreEqual(
			CompilerPipelineMetadataSpecifierTest::ExpectedClassDisplayName,
			GeneratedClass->GetMetaData(TEXT("DisplayName")),
			TEXT("Generated class should preserve DisplayName metadata with balanced parentheses")));
		ASSERT_THAT(AreEqual(
			CompilerPipelineMetadataSpecifierTest::ExpectedClassToolTip,
			GeneratedClass->GetMetaData(TEXT("ToolTip")),
			TEXT("Generated class should preserve ToolTip metadata with a closing parenthesis")));
		ASSERT_THAT(AreEqual(
			CompilerPipelineMetadataSpecifierTest::ExpectedFunctionDisplayName,
			GeneratedFunction->GetMetaData(TEXT("DisplayName")),
			TEXT("Generated function should preserve DisplayName metadata with a closing parenthesis")));
		ASSERT_THAT(AreEqual(
			CompilerPipelineMetadataSpecifierTest::ExpectedFunctionToolTip,
			GeneratedFunction->GetMetaData(TEXT("ToolTip")),
			TEXT("Generated function should preserve ToolTip metadata with a closing parenthesis")));
		ASSERT_THAT(AreEqual(
			CompilerPipelineMetadataSpecifierTest::ExpectedEnumToolTip,
			GeneratedEnum->GetMetaData(TEXT("ToolTip")),
			TEXT("Generated enum should preserve ToolTip metadata with a closing parenthesis")));
		ASSERT_THAT(AreEqual(
			CompilerPipelineMetadataSpecifierTest::ExpectedEnumValueDisplayName,
			GeneratedEnum->GetMetaData(TEXT("DisplayName"), 0),
			TEXT("Generated enum value should preserve DisplayName metadata with a closing parenthesis")));
		ASSERT_THAT(AreEqual(
			CompilerPipelineMetadataSpecifierTest::ExpectedEnumValueToolTip,
			GeneratedEnum->GetMetaData(TEXT("ToolTip"), 0),
			TEXT("Generated enum value should preserve ToolTip metadata with a closing parenthesis")));
		ASSERT_THAT(AreEqual(
			CompilerPipelineMetadataSpecifierTest::ExpectedEnumValueDisplayName,
			GeneratedEnum->GetDisplayNameTextByIndex(0).ToString(),
			TEXT("Generated enum display text should preserve the full DisplayName metadata")));

		}

	}

};

#endif
