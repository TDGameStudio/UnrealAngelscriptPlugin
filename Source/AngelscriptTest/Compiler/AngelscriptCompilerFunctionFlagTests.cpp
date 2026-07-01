#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "Core/AngelscriptSettings.h"
#include "Preprocessor/AngelscriptPreprocessor.h"

#include "HAL/FileManager.h"
#include "CQTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace CompilerFunctionFlagTest
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
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsTrue(
				FunctionDesc.IsValid(),
				*FString::Printf(TEXT("%s should parse function descriptor %s"), *TestCaseLabel, Expected.FunctionName)))
		{
			return false;
		}

		bool bPassed = true;
		bPassed &= LocalAssert.AreEqual(
			Expected.bBlueprintCallable,
			FunctionDesc->bBlueprintCallable,
			*FString::Printf(TEXT("%s should set bBlueprintCallable for %s during preprocessing"), *TestCaseLabel, Expected.FunctionName));
		bPassed &= LocalAssert.AreEqual(
			Expected.bBlueprintPure,
			FunctionDesc->bBlueprintPure,
			*FString::Printf(TEXT("%s should set bBlueprintPure for %s during preprocessing"), *TestCaseLabel, Expected.FunctionName));
		return bPassed;
	}

	bool ExpectGeneratedFlags(
		FAutomationTestBase& Test,
		const FString& TestCaseLabel,
		UClass* GeneratedClass,
		const FExpectedFunctionFlags& Expected)
	{
		UFunction* GeneratedFunction = FindGeneratedFunction(GeneratedClass, Expected.FunctionName);
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(
				GeneratedFunction,
				*FString::Printf(TEXT("%s should materialize generated function %s"), *TestCaseLabel, Expected.FunctionName)))
		{
			return false;
		}

		bool bPassed = true;
		bPassed &= LocalAssert.AreEqual(
			Expected.bBlueprintCallable,
			GeneratedFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			*FString::Printf(TEXT("%s should set FUNC_BlueprintCallable for %s"), *TestCaseLabel, Expected.FunctionName));
		bPassed &= LocalAssert.AreEqual(
			Expected.bBlueprintPure,
			GeneratedFunction->HasAnyFunctionFlags(FUNC_BlueprintPure),
			*FString::Printf(TEXT("%s should set FUNC_BlueprintPure for %s"), *TestCaseLabel, Expected.FunctionName));
		return bPassed;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCompilerFunctionFlagTests,
	"Angelscript.TestModule.Compiler.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(FunctionBlueprintCallableDefaultsAndOverrides)
	{


		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		UAngelscriptSettings* Settings = GetMutableDefault<UAngelscriptSettings>();
		if (!this->Assert.IsNotNull(Settings, TEXT("Function blueprint callable defaults test should access mutable angelscript settings")))
		{
			return;
		}

		const bool PreviousDefaultCallable = Settings->bDefaultFunctionBlueprintCallable;
		ON_SCOPE_EXIT
		{
			Settings->bDefaultFunctionBlueprintCallable = PreviousDefaultCallable;
		};

		for (const CompilerFunctionFlagTest::FTestCase& TestCase : CompilerFunctionFlagTest::BuildTestCases())
		{
			{
				const FString AbsoluteScriptPath = CompilerFunctionFlagTest::WriteFixture(
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
				const TArray<FString> PreprocessMessages = CompilerFunctionFlagTest::CollectDiagnosticMessages(
					Engine,
					AbsoluteScriptPath,
					PreprocessErrorCount);

				const FString TestCaseLabel = FString::Printf(
					TEXT("Function blueprint callable defaults test case (%s, default=%s)"),
					*TestCase.ClassName,
					TestCase.bDefaultFunctionBlueprintCallable ? TEXT("true") : TEXT("false"));

				ASSERT_THAT(IsTrue(
					bPreprocessSucceeded,
					*FString::Printf(TEXT("%s should preprocess successfully"), *TestCaseLabel)));
				ASSERT_THAT(AreEqual(
					0,
					PreprocessErrorCount,
					*FString::Printf(TEXT("%s should keep preprocessing error count at zero"), *TestCaseLabel)));
				ASSERT_THAT(AreEqual(
					0,
					PreprocessMessages.Num(),
					*FString::Printf(TEXT("%s should keep preprocessing diagnostics empty"), *TestCaseLabel)));
				ASSERT_THAT(AreEqual(
					1,
					Modules.Num(),
					*FString::Printf(TEXT("%s should emit exactly one module descriptor"), *TestCaseLabel)));
				if (!bPreprocessSucceeded || Modules.Num() != 1)
				{
					return;
				}

				const TSharedRef<FAngelscriptModuleDesc> ModuleDesc = Modules[0];
				const TSharedPtr<FAngelscriptClassDesc> ClassDesc = ModuleDesc->GetClass(TestCase.ClassName);
				if (!this->Assert.IsTrue(
						ClassDesc.IsValid(),
						*FString::Printf(TEXT("%s should parse the annotated class descriptor"), *TestCaseLabel)))
				{
					return;
				}

				for (const CompilerFunctionFlagTest::FExpectedFunctionFlags& ExpectedFunction : TestCase.ExpectedFunctions)
				{
					const TSharedPtr<FAngelscriptFunctionDesc> FunctionDesc = ClassDesc->GetMethod(ExpectedFunction.FunctionName);
					CompilerFunctionFlagTest::ExpectDescriptorFlags(
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

				ASSERT_THAT(IsTrue(
					bCompiled,
					*FString::Printf(TEXT("%s should compile through the full preprocessor pipeline"), *TestCaseLabel)));
				ASSERT_THAT(IsTrue(
					Summary.bUsedPreprocessor,
					*FString::Printf(TEXT("%s should record preprocessor usage in the compile summary"), *TestCaseLabel)));
				ASSERT_THAT(IsTrue(
					Summary.bCompileSucceeded,
					*FString::Printf(TEXT("%s should mark compile succeeded in the summary"), *TestCaseLabel)));
				ASSERT_THAT(AreEqual(
					ECompileResult::FullyHandled,
					Summary.CompileResult,
					*FString::Printf(TEXT("%s should report FullyHandled compile result"), *TestCaseLabel)));
				ASSERT_THAT(AreEqual(
					0,
					Summary.Diagnostics.Num(),
					*FString::Printf(TEXT("%s should keep compile diagnostics empty"), *TestCaseLabel)));
				if (!bCompiled)
				{
					return;
				}

				UClass* GeneratedClass = FindGeneratedClass(&Engine, *TestCase.ClassName);
				if (!this->Assert.IsNotNull(
						GeneratedClass,
						*FString::Printf(TEXT("%s should materialize the generated class"), *TestCaseLabel)))
				{
					return;
				}

				for (const CompilerFunctionFlagTest::FExpectedFunctionFlags& ExpectedFunction : TestCase.ExpectedFunctions)
				{
					CompilerFunctionFlagTest::ExpectGeneratedFlags(
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
