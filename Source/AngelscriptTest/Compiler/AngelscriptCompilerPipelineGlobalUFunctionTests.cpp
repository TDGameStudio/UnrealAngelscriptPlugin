#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "ClassGenerator/ASClass.h"
#include "Core/AngelscriptEngine.h"
#include "Preprocessor/AngelscriptPreprocessor.h"

#include "HAL/FileManager.h"
#include "CQTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CompilerPipelineGlobalUFunctionTest
{
	static const FName ModuleName(TEXT("Tests.Compiler.GlobalUFunctionCreatesStaticsClass"));
	static const FString RelativeScriptPath(TEXT("Tests/Compiler/GlobalUFunctionCreatesStaticsClass.as"));
	static const FName GlobalFunctionName(TEXT("GetGlobalValue"));

	FString GetFixtureRoot()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("CompilerGlobalUFunctionFixtures"));
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

	FString MakeExpectedStaticsClassName(const FString& ModuleNameString)
	{
		FString Identifier;
		Identifier.Reserve(ModuleNameString.Len());
		for (const TCHAR Character : ModuleNameString)
		{
			if (FAngelscriptEngine::IsValidIdentifierCharacter(Character))
			{
				Identifier += Character;
			}
			else
			{
				Identifier += TEXT('_');
			}
		}

		return FString::Printf(TEXT("Module_%sStatics"), *Identifier);
	}

	TSharedPtr<FAngelscriptClassDesc> FindStaticsClassDesc(const TSharedRef<FAngelscriptModuleDesc>& ModuleDesc, int32& OutStaticsClassCount)
	{
		OutStaticsClassCount = 0;
		TSharedPtr<FAngelscriptClassDesc> FoundClass;
		for (const TSharedRef<FAngelscriptClassDesc>& ClassDesc : ModuleDesc->Classes)
		{
			if (ClassDesc->bIsStaticsClass)
			{
				++OutStaticsClassCount;
				FoundClass = ClassDesc;
			}
		}

		return FoundClass;
	}

	bool ExecuteGeneratedStaticIntFunction(
		FAutomationTestBase& Test,
		UClass* OwnerClass,
		UASFunction* Function,
		int32& OutResult)
	{
		FNoDiscardAsserter LocalAssert(Test);

		FIntProperty* ReturnProperty = FindFProperty<FIntProperty>(Function, TEXT("ReturnValue"));
		if (!LocalAssert.IsNotNull(ReturnProperty, TEXT("Global UFUNCTION statics-class test case should expose a ReturnValue property")))
		{
			return false;
		}

		FStructOnScope Params(Function);
		void* ParamsMemory = Params.GetStructMemory();
		if (!LocalAssert.IsNotNull(ParamsMemory, TEXT("Global UFUNCTION statics-class test case should allocate a reflected parameter buffer")))
		{
			return false;
		}

		UObject* DefaultObject = OwnerClass != nullptr ? OwnerClass->GetDefaultObject() : nullptr;
		if (!LocalAssert.IsNotNull(DefaultObject, TEXT("Global UFUNCTION statics-class test case should expose a default object for the generated statics class")))
		{
			return false;
		}

		if (Function->bIsWorldContextGenerated)
		{
			*(UObject**)((SIZE_T)ParamsMemory + Function->WorldContextOffsetInParms) = DefaultObject;
		}

		Function->RuntimeCallEvent(DefaultObject, ParamsMemory);
		OutResult = ReturnProperty->GetPropertyValue_InContainer(ParamsMemory);
		return true;
	}
}

namespace CompilerPipelineGlobalUFunctionSanitizedModuleTest
{
	static const FName ModuleName(TEXT("Tests.Compiler.GlobalUFunction-Foo+Bar"));
	static const FString RelativeScriptPath(TEXT("Tests/Compiler/GlobalUFunction-Foo+Bar.as"));
	static const FName SanitizedGlobalFunctionName(TEXT("GetSanitizedGlobalValue"));
}

TEST_CLASS_WITH_FLAGS(FCompilerPipelineGlobalUFunctionTests,
	"Angelscript.TestModule.Compiler.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(GlobalUFunctionCreatesStaticsClass)
	{


		const FString ScriptSource = TEXT(R"AS(
	UFUNCTION(BlueprintCallable)
	int GetGlobalValue()
	{
		return 42;
	}
	)AS");

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		const FString AbsoluteScriptPath = CompilerPipelineGlobalUFunctionTest::WriteFixture(
			CompilerPipelineGlobalUFunctionTest::RelativeScriptPath,
			ScriptSource);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*CompilerPipelineGlobalUFunctionTest::ModuleName.ToString());
			IFileManager::Get().Delete(*AbsoluteScriptPath, false, true);
		};

		Engine.ResetDiagnostics();

		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddFile(CompilerPipelineGlobalUFunctionTest::RelativeScriptPath, AbsoluteScriptPath);

		const bool bPreprocessSucceeded = Preprocessor.Preprocess();
		const TArray<TSharedRef<FAngelscriptModuleDesc>> Modules = Preprocessor.GetModulesToCompile();

		int32 PreprocessErrorCount = 0;
		const TArray<FString> PreprocessMessages = CompilerPipelineGlobalUFunctionTest::CollectDiagnosticMessages(
			Engine,
			AbsoluteScriptPath,
			PreprocessErrorCount);

		ASSERT_THAT(IsTrue(
			bPreprocessSucceeded,
			TEXT("Global UFUNCTION statics-class test case should preprocess successfully")));
		ASSERT_THAT(AreEqual(
			0,
			PreprocessErrorCount,
			TEXT("Global UFUNCTION statics-class test case should keep preprocessing errors at zero")));
		ASSERT_THAT(AreEqual(
			0,
			PreprocessMessages.Num(),
			TEXT("Global UFUNCTION statics-class test case should keep preprocessing diagnostics empty")));
		ASSERT_THAT(AreEqual(
			1,
			Modules.Num(),
			TEXT("Global UFUNCTION statics-class test case should emit exactly one module descriptor")));
		if (!bPreprocessSucceeded || Modules.Num() != 1)
		{
			return;
		}

		const TSharedRef<FAngelscriptModuleDesc> ModuleDesc = Modules[0];
		const FString ExpectedStaticsClassName = CompilerPipelineGlobalUFunctionTest::MakeExpectedStaticsClassName(ModuleDesc->ModuleName);
		int32 StaticsClassCount = 0;
		const TSharedPtr<FAngelscriptClassDesc> StaticsClassDesc = CompilerPipelineGlobalUFunctionTest::FindStaticsClassDesc(
			ModuleDesc,
			StaticsClassCount);
		if (!this->Assert.IsTrue(StaticsClassDesc.IsValid(), TEXT("Global UFUNCTION statics-class test case should emit a statics class descriptor")))
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			1,
			StaticsClassCount,
			TEXT("Global UFUNCTION statics-class test case should emit exactly one statics class descriptor")));
		ASSERT_THAT(AreEqual(
			ExpectedStaticsClassName,
			StaticsClassDesc->ClassName,
			TEXT("Global UFUNCTION statics-class test case should normalize the generated statics class name from the module name")));

		const TSharedPtr<FAngelscriptFunctionDesc> FunctionDesc = StaticsClassDesc->GetMethod(
			CompilerPipelineGlobalUFunctionTest::GlobalFunctionName.ToString());
		if (!this->Assert.IsTrue(FunctionDesc.IsValid(), TEXT("Global UFUNCTION statics-class test case should attach GetGlobalValue to the generated statics class descriptor")))
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			FunctionDesc->bIsStatic,
			TEXT("Global UFUNCTION statics-class test case should mark the generated descriptor method as static")));
		ASSERT_THAT(IsTrue(
			FunctionDesc->bBlueprintCallable,
			TEXT("Global UFUNCTION statics-class test case should preserve BlueprintCallable on the generated descriptor method")));

		Engine.ResetDiagnostics();

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			CompilerPipelineGlobalUFunctionTest::ModuleName,
			CompilerPipelineGlobalUFunctionTest::RelativeScriptPath,
			ScriptSource,
			true,
			Summary);

		ASSERT_THAT(IsTrue(
			bCompiled,
			TEXT("Global UFUNCTION statics-class test case should compile through the normal preprocessor pipeline")));
		ASSERT_THAT(IsTrue(
			Summary.bUsedPreprocessor,
			TEXT("Global UFUNCTION statics-class test case should record preprocessor usage in the compile summary")));
		ASSERT_THAT(IsTrue(
			Summary.bCompileSucceeded,
			TEXT("Global UFUNCTION statics-class test case should mark compile succeeded in the summary")));
		ASSERT_THAT(AreEqual(
			0,
			Summary.Diagnostics.Num(),
			TEXT("Global UFUNCTION statics-class test case should keep compile diagnostics empty")));
		if (!bCompiled)
		{
			return;
		}

		const FName GeneratedStaticsClassName(*FString::Printf(TEXT("U%s"), *ExpectedStaticsClassName));
		UClass* GeneratedClass = FindGeneratedClass(&Engine, GeneratedStaticsClassName);
		if (!this->Assert.IsNotNull(GeneratedClass, TEXT("Global UFUNCTION statics-class test case should materialize the generated statics class")))
		{
			return;
		}

		UFunction* GeneratedFunction = FindGeneratedFunction(GeneratedClass, CompilerPipelineGlobalUFunctionTest::GlobalFunctionName);
		UASFunction* ScriptFunction = Cast<UASFunction>(GeneratedFunction);
		if (!this->Assert.IsNotNull(GeneratedFunction, TEXT("Global UFUNCTION statics-class test case should materialize the generated static function"))
			|| !this->Assert.IsNotNull(ScriptFunction, TEXT("Global UFUNCTION statics-class test case should expose the generated function as a UASFunction")))
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			GeneratedFunction->HasAnyFunctionFlags(FUNC_Static),
			TEXT("Global UFUNCTION statics-class test case should surface the reflected function as static")));
		ASSERT_THAT(IsTrue(
			GeneratedFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("Global UFUNCTION statics-class test case should surface the reflected function as BlueprintCallable")));
		ASSERT_THAT(IsTrue(
			ScriptFunction->bIsWorldContextGenerated,
			TEXT("Global UFUNCTION statics-class test case should synthesize a hidden world-context parameter for the reflected static function")));
		ASSERT_THAT(AreEqual(
			ScriptFunction->Arguments.Num(),
			ScriptFunction->WorldContextIndex,
			TEXT("Global UFUNCTION statics-class test case should append the hidden world-context parameter after the declared script arguments")));
		ASSERT_THAT(IsTrue(
			ScriptFunction->WorldContextOffsetInParms >= 0,
			TEXT("Global UFUNCTION statics-class test case should record a valid world-context offset for reflective execution")));

		int32 RuntimeResult = 0;
		const bool bExecuted = CompilerPipelineGlobalUFunctionTest::ExecuteGeneratedStaticIntFunction(
			*TestRunner,
			GeneratedClass,
			ScriptFunction,
			RuntimeResult);
		ASSERT_THAT(IsTrue(
			bExecuted,
			TEXT("Global UFUNCTION statics-class test case should execute the generated statics function through RuntimeCallEvent")));
		if (bExecuted)
		{
			ASSERT_THAT(AreEqual(
				42,
				RuntimeResult,
				TEXT("Global UFUNCTION statics-class test case should return the original global function value through the generated statics class")));
		}

		}

	}

	TEST_METHOD(GlobalUFunctionSanitizesModuleNameForStaticsClass)
	{


		const FString ScriptSource = TEXT(R"AS(
	UFUNCTION(BlueprintCallable)
	int GetSanitizedGlobalValue()
	{
		return 77;
	}
	)AS");

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		const FString AbsoluteScriptPath = CompilerPipelineGlobalUFunctionTest::WriteFixture(
			CompilerPipelineGlobalUFunctionSanitizedModuleTest::RelativeScriptPath,
			ScriptSource);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*CompilerPipelineGlobalUFunctionSanitizedModuleTest::ModuleName.ToString());
			IFileManager::Get().Delete(*AbsoluteScriptPath, false, true);
		};

		Engine.ResetDiagnostics();

		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddFile(CompilerPipelineGlobalUFunctionSanitizedModuleTest::RelativeScriptPath, AbsoluteScriptPath);

		const bool bPreprocessSucceeded = Preprocessor.Preprocess();
		const TArray<TSharedRef<FAngelscriptModuleDesc>> Modules = Preprocessor.GetModulesToCompile();

		int32 PreprocessErrorCount = 0;
		const TArray<FString> PreprocessMessages = CompilerPipelineGlobalUFunctionTest::CollectDiagnosticMessages(
			Engine,
			AbsoluteScriptPath,
			PreprocessErrorCount);

		ASSERT_THAT(IsTrue(
			bPreprocessSucceeded,
			TEXT("Sanitized-module global UFUNCTION test case should preprocess successfully")));
		ASSERT_THAT(AreEqual(
			0,
			PreprocessErrorCount,
			TEXT("Sanitized-module global UFUNCTION test case should keep preprocessing errors at zero")));
		ASSERT_THAT(AreEqual(
			0,
			PreprocessMessages.Num(),
			TEXT("Sanitized-module global UFUNCTION test case should keep preprocessing diagnostics empty")));
		ASSERT_THAT(AreEqual(
			1,
			Modules.Num(),
			TEXT("Sanitized-module global UFUNCTION test case should emit exactly one module descriptor")));
		if (!bPreprocessSucceeded || Modules.Num() != 1)
		{
			return;
		}

		const TSharedRef<FAngelscriptModuleDesc> ModuleDesc = Modules[0];
		const FString ExpectedStaticsClassName(TEXT("Module_Tests_Compiler_GlobalUFunction_Foo_BarStatics"));
		const FString ExpectedDisplayName(TEXT("GlobalUFunction-Foo+Bar"));
		int32 StaticsClassCount = 0;
		const TSharedPtr<FAngelscriptClassDesc> StaticsClassDesc = CompilerPipelineGlobalUFunctionTest::FindStaticsClassDesc(
			ModuleDesc,
			StaticsClassCount);
		if (!this->Assert.IsTrue(StaticsClassDesc.IsValid(), TEXT("Sanitized-module global UFUNCTION test case should emit a statics class descriptor")))
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			ModuleDesc->ModuleName.Contains(TEXT("-")),
			TEXT("Sanitized-module global UFUNCTION test case should preserve '-' in the raw module name before statics-class sanitization")));
		ASSERT_THAT(IsTrue(
			ModuleDesc->ModuleName.Contains(TEXT("+")),
			TEXT("Sanitized-module global UFUNCTION test case should preserve '+' in the raw module name before statics-class sanitization")));
		ASSERT_THAT(AreEqual(
			1,
			StaticsClassCount,
			TEXT("Sanitized-module global UFUNCTION test case should emit exactly one statics class descriptor")));
		ASSERT_THAT(AreEqual(
			ExpectedStaticsClassName,
			StaticsClassDesc->ClassName,
			TEXT("Sanitized-module global UFUNCTION test case should sanitize invalid module-name characters when generating the statics class name")));
		ASSERT_THAT(IsFalse(
			StaticsClassDesc->ClassName.Contains(TEXT("-")),
			TEXT("Sanitized-module global UFUNCTION test case should not leave '-' in the generated statics class name")));
		ASSERT_THAT(IsFalse(
			StaticsClassDesc->ClassName.Contains(TEXT("+")),
			TEXT("Sanitized-module global UFUNCTION test case should not leave '+' in the generated statics class name")));
		const FString* DisplayName = StaticsClassDesc->Meta.Find(FName(TEXT("DisplayName")));
		ASSERT_THAT(IsNotNull(
			DisplayName,
			TEXT("Sanitized-module global UFUNCTION test case should set DisplayName metadata from the unsanitized base filename")));
		if (DisplayName != nullptr)
		{
			ASSERT_THAT(AreEqual(
				ExpectedDisplayName,
				*DisplayName,
				TEXT("Sanitized-module global UFUNCTION test case should preserve the original base filename in DisplayName metadata")));
		}

		const TSharedPtr<FAngelscriptFunctionDesc> FunctionDesc = StaticsClassDesc->GetMethod(
			CompilerPipelineGlobalUFunctionSanitizedModuleTest::SanitizedGlobalFunctionName.ToString());
		if (!this->Assert.IsTrue(FunctionDesc.IsValid(), TEXT("Sanitized-module global UFUNCTION test case should attach GetSanitizedGlobalValue to the generated statics class descriptor")))
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			FunctionDesc->bIsStatic,
			TEXT("Sanitized-module global UFUNCTION test case should mark the generated descriptor method as static")));
		ASSERT_THAT(IsTrue(
			FunctionDesc->bBlueprintCallable,
			TEXT("Sanitized-module global UFUNCTION test case should preserve BlueprintCallable on the generated descriptor method")));

		Engine.ResetDiagnostics();

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			CompilerPipelineGlobalUFunctionSanitizedModuleTest::ModuleName,
			CompilerPipelineGlobalUFunctionSanitizedModuleTest::RelativeScriptPath,
			ScriptSource,
			true,
			Summary);

		ASSERT_THAT(IsTrue(
			bCompiled,
			TEXT("Sanitized-module global UFUNCTION test case should compile through the normal preprocessor pipeline")));
		ASSERT_THAT(IsTrue(
			Summary.bUsedPreprocessor,
			TEXT("Sanitized-module global UFUNCTION test case should record preprocessor usage in the compile summary")));
		ASSERT_THAT(IsTrue(
			Summary.bCompileSucceeded,
			TEXT("Sanitized-module global UFUNCTION test case should mark compile succeeded in the summary")));
		ASSERT_THAT(AreEqual(
			0,
			Summary.Diagnostics.Num(),
			TEXT("Sanitized-module global UFUNCTION test case should keep compile diagnostics empty")));
		if (!bCompiled)
		{
			return;
		}

		const FName GeneratedStaticsClassName(*FString::Printf(TEXT("U%s"), *ExpectedStaticsClassName));
		UClass* GeneratedClass = FindGeneratedClass(&Engine, GeneratedStaticsClassName);
		if (!this->Assert.IsNotNull(GeneratedClass, TEXT("Sanitized-module global UFUNCTION test case should materialize the sanitized statics class")))
		{
			return;
		}

		UFunction* GeneratedFunction = FindGeneratedFunction(GeneratedClass, CompilerPipelineGlobalUFunctionSanitizedModuleTest::SanitizedGlobalFunctionName);
		UASFunction* ScriptFunction = Cast<UASFunction>(GeneratedFunction);
		if (!this->Assert.IsNotNull(GeneratedFunction, TEXT("Sanitized-module global UFUNCTION test case should materialize the generated static function"))
			|| !this->Assert.IsNotNull(ScriptFunction, TEXT("Sanitized-module global UFUNCTION test case should expose the generated function as a UASFunction")))
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			GeneratedFunction->HasAnyFunctionFlags(FUNC_Static),
			TEXT("Sanitized-module global UFUNCTION test case should surface the reflected function as static")));
		ASSERT_THAT(IsTrue(
			GeneratedFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("Sanitized-module global UFUNCTION test case should surface the reflected function as BlueprintCallable")));

		int32 RuntimeResult = 0;
		const bool bExecuted = CompilerPipelineGlobalUFunctionTest::ExecuteGeneratedStaticIntFunction(
			*TestRunner,
			GeneratedClass,
			ScriptFunction,
			RuntimeResult);
		ASSERT_THAT(IsTrue(
			bExecuted,
			TEXT("Sanitized-module global UFUNCTION test case should execute the generated statics function through RuntimeCallEvent")));
		if (bExecuted)
		{
			ASSERT_THAT(AreEqual(
				77,
				RuntimeResult,
				TEXT("Sanitized-module global UFUNCTION test case should return the original global function value through the sanitized statics class")));
		}

		}

	}

};

#endif
