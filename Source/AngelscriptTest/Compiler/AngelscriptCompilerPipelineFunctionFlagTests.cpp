#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptTestMacros.h"

#include "Core/AngelscriptSettings.h"
#include "Preprocessor/AngelscriptPreprocessor.h"

#include "HAL/FileManager.h"
#include "CQTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CompilerPipelineFunctionFlagTest
{
	struct FExpectedFunctionFlags
	{
		const TCHAR* FunctionName = nullptr;
		bool bBlueprintCallable = false;
		bool bBlueprintPure = false;
	};

	struct FTestCase
	{
		FName ModuleName;
		FString RelativeScriptPath;
		FString ClassName;
		bool bDefaultFunctionBlueprintCallable = false;
		FString ScriptSource;
		TArray<FExpectedFunctionFlags> ExpectedFunctions;
	};

	TArray<FTestCase> BuildTestCases()
	{
		return {
			{
				TEXT("Tests.Compiler.FunctionBlueprintCallableDefaultsFalse"),
				TEXT("Tests/Compiler/FunctionBlueprintCallableDefaultsFalse.as"),
				TEXT("UCompilerBlueprintCallableDefaultFalseCarrier"),
				false,
				TEXT(R"AS(
UCLASS()
class UCompilerBlueprintCallableDefaultFalseCarrier : UObject
{
	UFUNCTION()
	int ImplicitDefault()
	{
		return 1;
	}

	UFUNCTION(BlueprintCallable)
	int ExplicitCallable()
	{
		return 2;
	}

	UFUNCTION(BlueprintPure)
	int PureValue()
	{
		return 3;
	}
}
)AS"),
				{
					{ TEXT("ImplicitDefault"), false, false },
					{ TEXT("ExplicitCallable"), true, false },
					{ TEXT("PureValue"), true, true },
				}
			},
			{
				TEXT("Tests.Compiler.FunctionBlueprintCallableDefaultsTrue"),
				TEXT("Tests/Compiler/FunctionBlueprintCallableDefaultsTrue.as"),
				TEXT("UCompilerBlueprintCallableDefaultTrueCarrier"),
				true,
				TEXT(R"AS(
UCLASS()
class UCompilerBlueprintCallableDefaultTrueCarrier : UObject
{
	UFUNCTION()
	int ImplicitCallable()
	{
		return 4;
	}

	UFUNCTION(NotBlueprintCallable)
	int HiddenValue()
	{
		return 5;
	}
}
)AS"),
				{
					{ TEXT("ImplicitCallable"), true, false },
					{ TEXT("HiddenValue"), false, false },
				}
			}
		};
	}

	FString GetFixtureRoot()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("CompilerFunctionFlagFixtures"));
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

	bool ExpectDescriptorFlags(
		FAutomationTestBase& Test,
		const FString& TestCaseLabel,
		const TSharedPtr<FAngelscriptFunctionDesc>& FunctionDesc,
		const FExpectedFunctionFlags& Expected)
	{
		if (!Test.TestTrue(
				*FString::Printf(TEXT("%s should parse function descriptor %s"), *TestCaseLabel, Expected.FunctionName),
				FunctionDesc.IsValid()))
		{
			return false;
		}

		bool bPassed = true;
		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("%s should set bBlueprintCallable for %s during preprocessing"), *TestCaseLabel, Expected.FunctionName),
			FunctionDesc->bBlueprintCallable,
			Expected.bBlueprintCallable);
		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("%s should set bBlueprintPure for %s during preprocessing"), *TestCaseLabel, Expected.FunctionName),
			FunctionDesc->bBlueprintPure,
			Expected.bBlueprintPure);
		return bPassed;
	}

	bool ExpectGeneratedFlags(
		FAutomationTestBase& Test,
		const FString& TestCaseLabel,
		UClass* GeneratedClass,
		const FExpectedFunctionFlags& Expected)
	{
		UFunction* GeneratedFunction = FindGeneratedFunction(GeneratedClass, Expected.FunctionName);
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("%s should materialize generated function %s"), *TestCaseLabel, Expected.FunctionName),
				GeneratedFunction))
		{
			return false;
		}

		bool bPassed = true;
		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("%s should set FUNC_BlueprintCallable for %s"), *TestCaseLabel, Expected.FunctionName),
			GeneratedFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			Expected.bBlueprintCallable);
		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("%s should set FUNC_BlueprintPure for %s"), *TestCaseLabel, Expected.FunctionName),
			GeneratedFunction->HasAnyFunctionFlags(FUNC_BlueprintPure),
			Expected.bBlueprintPure);
		return bPassed;
	}
}

using namespace CompilerPipelineFunctionFlagTest;

TEST_CLASS_WITH_FLAGS(FCompilerPipelineFunctionFlagTests,
	"Angelscript.TestModule.Compiler.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(FunctionBlueprintCallableDefaultsAndOverrides)
	{


		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		UAngelscriptSettings* Settings = GetMutableDefault<UAngelscriptSettings>();
		if (!TestRunner->TestNotNull(TEXT("Function blueprint callable defaults test should access mutable angelscript settings"), Settings))
		{
			return;
		}

		const bool PreviousDefaultCallable = Settings->bDefaultFunctionBlueprintCallable;
		ON_SCOPE_EXIT
		{
			Settings->bDefaultFunctionBlueprintCallable = PreviousDefaultCallable;
		};

		for (const CompilerPipelineFunctionFlagTest::FTestCase& TestCase : CompilerPipelineFunctionFlagTest::BuildTestCases())
		{
			{
				const FString AbsoluteScriptPath = CompilerPipelineFunctionFlagTest::WriteFixture(
					TestCase.RelativeScriptPath,
					TestCase.ScriptSource);
				const FString ModuleNameString = TestCase.ModuleName.ToString();
				ON_SCOPE_EXIT
				{
					Engine.DiscardModule(*ModuleNameString);
					IFileManager::Get().Delete(*AbsoluteScriptPath, false, true);
				};

				Settings->bDefaultFunctionBlueprintCallable = TestCase.bDefaultFunctionBlueprintCallable;
				Engine.ResetDiagnostics();

				FAngelscriptPreprocessor Preprocessor;
				Preprocessor.AddFile(TestCase.RelativeScriptPath, AbsoluteScriptPath);

				const bool bPreprocessSucceeded = Preprocessor.Preprocess();
				const TArray<TSharedRef<FAngelscriptModuleDesc>> Modules = Preprocessor.GetModulesToCompile();

				int32 PreprocessErrorCount = 0;
				const TArray<FString> PreprocessMessages = CompilerPipelineFunctionFlagTest::CollectDiagnosticMessages(
					Engine,
					AbsoluteScriptPath,
					PreprocessErrorCount);

				const FString TestCaseLabel = FString::Printf(
					TEXT("Function blueprint callable defaults test case (%s, default=%s)"),
					*TestCase.ClassName,
					TestCase.bDefaultFunctionBlueprintCallable ? TEXT("true") : TEXT("false"));

				TestRunner->TestTrue(
					*FString::Printf(TEXT("%s should preprocess successfully"), *TestCaseLabel),
					bPreprocessSucceeded);
				TestRunner->TestEqual(
					*FString::Printf(TEXT("%s should keep preprocessing error count at zero"), *TestCaseLabel),
					PreprocessErrorCount,
					0);
				TestRunner->TestEqual(
					*FString::Printf(TEXT("%s should keep preprocessing diagnostics empty"), *TestCaseLabel),
					PreprocessMessages.Num(),
					0);
				TestRunner->TestEqual(
					*FString::Printf(TEXT("%s should emit exactly one module descriptor"), *TestCaseLabel),
					Modules.Num(),
					1);
				if (!bPreprocessSucceeded || Modules.Num() != 1)
				{
					return;
				}

				const TSharedRef<FAngelscriptModuleDesc> ModuleDesc = Modules[0];
				const TSharedPtr<FAngelscriptClassDesc> ClassDesc = ModuleDesc->GetClass(TestCase.ClassName);
				if (!TestRunner->TestTrue(
						*FString::Printf(TEXT("%s should parse the annotated class descriptor"), *TestCaseLabel),
						ClassDesc.IsValid()))
				{
					return;
				}

				for (const CompilerPipelineFunctionFlagTest::FExpectedFunctionFlags& ExpectedFunction : TestCase.ExpectedFunctions)
				{
					const TSharedPtr<FAngelscriptFunctionDesc> FunctionDesc = ClassDesc->GetMethod(ExpectedFunction.FunctionName);
					CompilerPipelineFunctionFlagTest::ExpectDescriptorFlags(
						*TestRunner,
						TestCaseLabel,
						FunctionDesc,
						ExpectedFunction);
				}

				Engine.ResetDiagnostics();

				FAngelscriptCompileTraceSummary Summary;
				const bool bCompiled = CompileModuleWithSummary(
					&Engine,
					ECompileType::FullReload,
					TestCase.ModuleName,
					TestCase.RelativeScriptPath,
					TestCase.ScriptSource,
					true,
					Summary,
					true);

				TestRunner->TestTrue(
					*FString::Printf(TEXT("%s should compile through the full preprocessor pipeline"), *TestCaseLabel),
					bCompiled);
				TestRunner->TestTrue(
					*FString::Printf(TEXT("%s should record preprocessor usage in the compile summary"), *TestCaseLabel),
					Summary.bUsedPreprocessor);
				TestRunner->TestTrue(
					*FString::Printf(TEXT("%s should mark compile succeeded in the summary"), *TestCaseLabel),
					Summary.bCompileSucceeded);
				TestRunner->TestEqual(
					*FString::Printf(TEXT("%s should report FullyHandled compile result"), *TestCaseLabel),
					Summary.CompileResult,
					ECompileResult::FullyHandled);
				TestRunner->TestEqual(
					*FString::Printf(TEXT("%s should keep compile diagnostics empty"), *TestCaseLabel),
					Summary.Diagnostics.Num(),
					0);
				if (!bCompiled)
				{
					return;
				}

				UClass* GeneratedClass = FindGeneratedClass(&Engine, *TestCase.ClassName);
				if (!TestRunner->TestNotNull(
						*FString::Printf(TEXT("%s should materialize the generated class"), *TestCaseLabel),
						GeneratedClass))
				{
					return;
				}

				for (const CompilerPipelineFunctionFlagTest::FExpectedFunctionFlags& ExpectedFunction : TestCase.ExpectedFunctions)
				{
					CompilerPipelineFunctionFlagTest::ExpectGeneratedFlags(
						*TestRunner,
						TestCaseLabel,
						GeneratedClass,
						ExpectedFunction);
				}
			}
		}

		}

	}

};

#endif
