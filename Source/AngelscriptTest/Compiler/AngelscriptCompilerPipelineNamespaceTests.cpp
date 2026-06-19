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

namespace CompilerPipelineNamespaceTest
{
	static const FName ModuleName(TEXT("Tests.Compiler.NamespacedAnnotatedClassStaticHelperRoundTrip"));
	static const FString RelativeScriptPath(TEXT("Tests/Compiler/NamespacedAnnotatedClassStaticHelperRoundTrip.as"));
	static const FString ClassName(TEXT("UNamespaceCarrier"));
	static const FString EntryDecl(TEXT("int Entry()"));
	static const FString MethodName(TEXT("GetValue"));

	FString GetFixtureRoot()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("CompilerNamespaceFixtures"));
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
}

TEST_CLASS_WITH_FLAGS(FCompilerPipelineNamespaceTests,
	"Angelscript.TestModule.Compiler.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(NamespacedAnnotatedClassStaticHelperRoundTrip)
	{
	using namespace CompilerPipelineNamespaceTest;


		const FString ScriptSource = TEXT(R"AS(
	namespace Gameplay
	{
		UCLASS()
		class UNamespaceCarrier : UObject
		{
			UFUNCTION()
			int GetValue()
			{
				return 42;
			}
		}
	}

	int Entry()
	{
		return Gameplay::UNamespaceCarrier::StaticClass() != nullptr ? 42 : 0;
	}
	)AS");

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		const FString AbsoluteScriptPath = CompilerPipelineNamespaceTest::WriteFixture(
			CompilerPipelineNamespaceTest::RelativeScriptPath,
			ScriptSource);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*CompilerPipelineNamespaceTest::ModuleName.ToString());
			IFileManager::Get().Delete(*AbsoluteScriptPath, false, true);
		};

		Engine.ResetDiagnostics();

		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddFile(CompilerPipelineNamespaceTest::RelativeScriptPath, AbsoluteScriptPath);

		const bool bPreprocessSucceeded = Preprocessor.Preprocess();
		const TArray<TSharedRef<FAngelscriptModuleDesc>> Modules = Preprocessor.GetModulesToCompile();

		int32 PreprocessErrorCount = 0;
		const TArray<FString> PreprocessMessages = CompilerPipelineNamespaceTest::CollectDiagnosticMessages(
			Engine,
			AbsoluteScriptPath,
			PreprocessErrorCount);

		ASSERT_THAT(IsTrue(
			bPreprocessSucceeded,
			TEXT("Namespaced annotated class test case should preprocess successfully")));
		ASSERT_THAT(AreEqual(
			0,
			PreprocessErrorCount,
			TEXT("Namespaced annotated class test case should keep preprocessing errors at zero")));
		ASSERT_THAT(AreEqual(
			0,
			PreprocessMessages.Num(),
			TEXT("Namespaced annotated class test case should keep preprocessing diagnostics empty")));
		ASSERT_THAT(AreEqual(
			1,
			Modules.Num(),
			TEXT("Namespaced annotated class test case should emit exactly one module descriptor")));
		if (!bPreprocessSucceeded || Modules.Num() != 1)
		{
			return;
		}

		const TSharedRef<FAngelscriptModuleDesc> ModuleDesc = Modules[0];
		const TSharedPtr<FAngelscriptClassDesc> ClassDesc = ModuleDesc->GetClass(CompilerPipelineNamespaceTest::ClassName);
		if (!this->Assert.IsTrue(ClassDesc.IsValid(), TEXT("Namespaced annotated class test case should parse the annotated class descriptor")))
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			ClassDesc->Namespace.IsSet(),
			TEXT("Namespaced annotated class test case should record the class namespace during preprocessing")));
		if (ClassDesc->Namespace.IsSet())
		{
			ASSERT_THAT(AreEqual(
				FString(TEXT("Gameplay")),
				ClassDesc->Namespace.GetValue(),
				TEXT("Namespaced annotated class test case should preserve the Gameplay namespace")));
		}

		if (!this->Assert.IsTrue(ModuleDesc->Code.Num() == 1, TEXT("Namespaced annotated class test case should keep one processed code section")))
		{
			return;
		}

		const FString& ProcessedCode = ModuleDesc->Code[0].Code;
		ASSERT_THAT(IsTrue(
			ProcessedCode.Contains(TEXT("namespace Gameplay")),
			TEXT("Namespaced annotated class test case should keep the generated helper inside the Gameplay namespace")));
		ASSERT_THAT(IsTrue(
			ProcessedCode.Contains(TEXT("__StaticType_UNamespaceCarrier")),
			TEXT("Namespaced annotated class test case should emit the __StaticType global for the namespaced class")));
		ASSERT_THAT(IsTrue(
			ProcessedCode.Contains(TEXT("namespace UNamespaceCarrier { UClass StaticClass()")),
			TEXT("Namespaced annotated class test case should emit the nested StaticClass helper wrapper")));

		Engine.ResetDiagnostics();

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			CompilerPipelineNamespaceTest::ModuleName,
			CompilerPipelineNamespaceTest::RelativeScriptPath,
			ScriptSource,
			true,
			Summary);

		ASSERT_THAT(IsTrue(
			bCompiled,
			TEXT("Namespaced annotated class test case should compile through the normal preprocessor pipeline")));
		ASSERT_THAT(IsTrue(
			Summary.bUsedPreprocessor,
			TEXT("Namespaced annotated class test case should record preprocessor usage in the compile summary")));
		ASSERT_THAT(IsTrue(
			Summary.bCompileSucceeded,
			TEXT("Namespaced annotated class test case should mark compile succeeded in the summary")));
		ASSERT_THAT(AreEqual(
			ECompileResult::FullyHandled,
			Summary.CompileResult,
			TEXT("Namespaced annotated class test case should compile as fully handled")));
		ASSERT_THAT(AreEqual(
			0,
			Summary.Diagnostics.Num(),
			TEXT("Namespaced annotated class test case should keep compile diagnostics empty")));
		if (!bCompiled)
		{
			return;
		}

		UClass* GeneratedClass = FindGeneratedClass(&Engine, *CompilerPipelineNamespaceTest::ClassName);
		if (!this->Assert.IsNotNull(GeneratedClass, TEXT("Namespaced annotated class test case should materialize the generated class")))
		{
			return;
		}

		ASSERT_THAT(IsNotNull(
			FindGeneratedFunction(GeneratedClass, *CompilerPipelineNamespaceTest::MethodName),
			TEXT("Namespaced annotated class test case should materialize the generated class method")));

		int32 Result = 0;
		const bool bExecuted = ExecuteIntFunction(
			&Engine,
			CompilerPipelineNamespaceTest::ModuleName,
			CompilerPipelineNamespaceTest::EntryDecl,
			Result);
		ASSERT_THAT(IsTrue(
			bExecuted,
			TEXT("Namespaced annotated class test case should execute the entry point")));
		if (bExecuted)
		{
			ASSERT_THAT(AreEqual(
				42,
				Result,
				TEXT("Namespaced annotated class test case should resolve Gameplay::UNamespaceCarrier::StaticClass() at runtime")));
		}

		}

	}

};

#endif
