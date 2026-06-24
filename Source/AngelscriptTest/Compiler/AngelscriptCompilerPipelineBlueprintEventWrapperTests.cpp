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
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CompilerPipelineBlueprintEventWrapperTest
{
	static const FName ModuleName(TEXT("Tests.Compiler.BlueprintEventWrapperExecutesImplementation"));
	static const FString RelativeScriptPath(TEXT("Tests/Compiler/BlueprintEventWrapperExecutesImplementation.as"));
	static const FString ClassName(TEXT("UCompilerBlueprintEventWrapperCarrier"));
	static const FString ComputeFunctionName(TEXT("Compute"));
	static const FString EntryFunctionName(TEXT("Entry"));

	FString GetFixtureRoot()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("CompilerBlueprintEventWrapperFixtures"));
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

namespace CompilerPipelineBlueprintEventMixedPushTest
{
	static const FName ModuleName(TEXT("Tests.Compiler.BlueprintEventWrapperUsesMixedPushPaths"));
	static const FString RelativeScriptPath(TEXT("Tests/Compiler/BlueprintEventWrapperUsesMixedPushPaths.as"));
	static const FString ClassName(TEXT("UCompilerBlueprintEventMixedPushCarrier"));
	static const FString EvaluateFunctionName(TEXT("EvaluateMixedPush"));
	static const FString EntryFunctionName(TEXT("Entry"));
}

TEST_CLASS_WITH_FLAGS(FCompilerPipelineBlueprintEventWrapperTests,
	"Angelscript.TestModule.Compiler.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(BlueprintEventWrapperExecutesImplementation)
	{


		const FString ScriptSource = TEXT(R"AS(
	UCLASS()
	class UCompilerBlueprintEventWrapperCarrier : UObject
	{
		UFUNCTION(BlueprintEvent)
		int Compute(int Value)
		{
			return Value + 21;
		}

		UFUNCTION()
		int Entry()
		{
			return Compute(21);
		}
	}
	)AS");

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		const FString AbsoluteScriptPath = CompilerPipelineBlueprintEventWrapperTest::WriteFixture(
			CompilerPipelineBlueprintEventWrapperTest::RelativeScriptPath,
			ScriptSource);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*CompilerPipelineBlueprintEventWrapperTest::ModuleName.ToString());
			IFileManager::Get().Delete(*AbsoluteScriptPath, false, true);
		};

		Engine.ResetDiagnostics();

		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddFile(CompilerPipelineBlueprintEventWrapperTest::RelativeScriptPath, AbsoluteScriptPath);

		const bool bPreprocessSucceeded = Preprocessor.Preprocess();
		const TArray<TSharedRef<FAngelscriptModuleDesc>> Modules = Preprocessor.GetModulesToCompile();

		int32 PreprocessErrorCount = 0;
		const TArray<FString> PreprocessMessages = CompilerPipelineBlueprintEventWrapperTest::CollectDiagnosticMessages(
			Engine,
			AbsoluteScriptPath,
			PreprocessErrorCount);

		ASSERT_THAT(IsTrue(
			bPreprocessSucceeded,
			TEXT("BlueprintEvent wrapper test case should preprocess successfully")));
		ASSERT_THAT(AreEqual(
			0,
			PreprocessErrorCount,
			TEXT("BlueprintEvent wrapper test case should keep preprocessing errors at zero")));
		ASSERT_THAT(AreEqual(
			0,
			PreprocessMessages.Num(),
			TEXT("BlueprintEvent wrapper test case should keep preprocessing diagnostics empty")));
		ASSERT_THAT(AreEqual(
			1,
			Modules.Num(),
			TEXT("BlueprintEvent wrapper test case should emit exactly one module descriptor")));
		if (!bPreprocessSucceeded || Modules.Num() != 1)
		{
			return;
		}

		const TSharedRef<FAngelscriptModuleDesc> ModuleDesc = Modules[0];
		const TSharedPtr<FAngelscriptClassDesc> ClassDesc = ModuleDesc->GetClass(CompilerPipelineBlueprintEventWrapperTest::ClassName);
		if (!this->Assert.IsTrue(ClassDesc.IsValid(), TEXT("BlueprintEvent wrapper test case should parse the annotated class descriptor")))
		{
			return;
		}

		const TSharedPtr<FAngelscriptFunctionDesc> ComputeDesc = ClassDesc->GetMethod(CompilerPipelineBlueprintEventWrapperTest::ComputeFunctionName);
		const TSharedPtr<FAngelscriptFunctionDesc> EntryDesc = ClassDesc->GetMethod(CompilerPipelineBlueprintEventWrapperTest::EntryFunctionName);
		if (!this->Assert.IsTrue(ComputeDesc.IsValid(), TEXT("BlueprintEvent wrapper test case should parse the BlueprintEvent function descriptor"))
			|| !this->Assert.IsTrue(EntryDesc.IsValid(), TEXT("BlueprintEvent wrapper test case should parse the entry function descriptor")))
		{
			return;
		}

		const FString& ProcessedCode = ModuleDesc->Code[0].Code;
		ASSERT_THAT(IsTrue(
			ComputeDesc->bBlueprintEvent,
			TEXT("BlueprintEvent wrapper test case should mark Compute as a BlueprintEvent during preprocessing")));
		ASSERT_THAT(IsTrue(
			ComputeDesc->bCanOverrideEvent,
			TEXT("BlueprintEvent wrapper test case should leave Compute overridable during preprocessing")));
		ASSERT_THAT(IsFalse(
			ComputeDesc->bBlueprintCallable,
			TEXT("BlueprintEvent wrapper test case should not make a plain BlueprintEvent callable from blueprint by default")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("Compute_Implementation")),
			ComputeDesc->ScriptFunctionName,
			TEXT("BlueprintEvent wrapper test case should rename the backing script implementation")));
		ASSERT_THAT(IsTrue(
			ProcessedCode.Contains(TEXT("__Evt_Execute(this, __STATIC_NAME(")),
			TEXT("BlueprintEvent wrapper test case should inject an __Evt_Execute wrapper call into the processed code")));

		Engine.ResetDiagnostics();

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			CompilerPipelineBlueprintEventWrapperTest::ModuleName,
			CompilerPipelineBlueprintEventWrapperTest::RelativeScriptPath,
			ScriptSource,
			true,
			Summary);

		ASSERT_THAT(IsTrue(
			bCompiled,
			TEXT("BlueprintEvent wrapper test case should compile through the normal preprocessor pipeline")));
		ASSERT_THAT(IsTrue(
			Summary.bUsedPreprocessor,
			TEXT("BlueprintEvent wrapper test case should record preprocessor usage in the compile summary")));
		ASSERT_THAT(IsTrue(
			Summary.bCompileSucceeded,
			TEXT("BlueprintEvent wrapper test case should mark compile succeeded in the summary")));
		ASSERT_THAT(AreEqual(
			0,
			Summary.Diagnostics.Num(),
			TEXT("BlueprintEvent wrapper test case should keep compile diagnostics empty")));
		if (!bCompiled)
		{
			return;
		}

		UClass* GeneratedClass = FindGeneratedClass(&Engine, *CompilerPipelineBlueprintEventWrapperTest::ClassName);
		if (!this->Assert.IsNotNull(GeneratedClass, TEXT("BlueprintEvent wrapper test case should materialize the generated class")))
		{
			return;
		}

		UFunction* ComputeFunction = FindGeneratedFunction(GeneratedClass, *CompilerPipelineBlueprintEventWrapperTest::ComputeFunctionName);
		UFunction* EntryFunction = FindGeneratedFunction(GeneratedClass, *CompilerPipelineBlueprintEventWrapperTest::EntryFunctionName);
		if (!this->Assert.IsNotNull(ComputeFunction, TEXT("BlueprintEvent wrapper test case should materialize the generated event wrapper function"))
			|| !this->Assert.IsNotNull(EntryFunction, TEXT("BlueprintEvent wrapper test case should materialize the generated entry function")))
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			ComputeFunction->HasAnyFunctionFlags(FUNC_BlueprintEvent),
			TEXT("BlueprintEvent wrapper test case should surface Compute as a BlueprintEvent UFunction")));
		ASSERT_THAT(IsFalse(
			ComputeFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("BlueprintEvent wrapper test case should keep Compute non-blueprint-callable without an explicit BlueprintCallable specifier")));

		UObject* RuntimeObject = NewObject<UObject>(GetTransientPackage(), GeneratedClass, TEXT("CompilerBlueprintEventWrapperCarrier"));
		if (!this->Assert.IsNotNull(RuntimeObject, TEXT("BlueprintEvent wrapper test case should instantiate the generated class")))
		{
			return;
		}

		int32 Result = 0;
		const bool bExecuted = ExecuteGeneratedIntEventOnGameThread(&Engine, RuntimeObject, EntryFunction, Result);
		ASSERT_THAT(IsTrue(
			bExecuted,
			TEXT("BlueprintEvent wrapper test case should execute the generated entry function")));
		if (bExecuted)
		{
			ASSERT_THAT(AreEqual(
				42,
				Result,
				TEXT("BlueprintEvent wrapper test case should route through the wrapper and return the _Implementation result")));
		}

		}

	}

	TEST_METHOD(BlueprintEventWrapperUsesMixedPushPaths)
	{


		const FString ScriptSource = TEXT(R"AS(
	UCLASS()
	class UCompilerBlueprintEventMixedPushCarrier : UObject
	{
		UFUNCTION(BlueprintEvent)
		int EvaluateMixedPush(const FString& Label, TSubclassOf<AActor> TypeValue)
		{
			return TypeValue == AActor::StaticClass() ? 42 : 0;
		}

		UFUNCTION()
		int Entry()
		{
			return EvaluateMixedPush("Alpha", AActor::StaticClass());
		}
	}
	)AS");

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		const FString AbsoluteScriptPath = CompilerPipelineBlueprintEventWrapperTest::WriteFixture(
			CompilerPipelineBlueprintEventMixedPushTest::RelativeScriptPath,
			ScriptSource);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*CompilerPipelineBlueprintEventMixedPushTest::ModuleName.ToString());
			IFileManager::Get().Delete(*AbsoluteScriptPath, false, true);
		};

		Engine.ResetDiagnostics();

		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddFile(CompilerPipelineBlueprintEventMixedPushTest::RelativeScriptPath, AbsoluteScriptPath);

		const bool bPreprocessSucceeded = Preprocessor.Preprocess();
		const TArray<TSharedRef<FAngelscriptModuleDesc>> Modules = Preprocessor.GetModulesToCompile();

		int32 PreprocessErrorCount = 0;
		const TArray<FString> PreprocessMessages = CompilerPipelineBlueprintEventWrapperTest::CollectDiagnosticMessages(
			Engine,
			AbsoluteScriptPath,
			PreprocessErrorCount);

		ASSERT_THAT(IsTrue(
			bPreprocessSucceeded,
			TEXT("Mixed-push BlueprintEvent wrapper test case should preprocess successfully")));
		ASSERT_THAT(AreEqual(
			0,
			PreprocessErrorCount,
			TEXT("Mixed-push BlueprintEvent wrapper test case should keep preprocessing errors at zero")));
		ASSERT_THAT(AreEqual(
			0,
			PreprocessMessages.Num(),
			TEXT("Mixed-push BlueprintEvent wrapper test case should keep preprocessing diagnostics empty")));
		ASSERT_THAT(AreEqual(
			1,
			Modules.Num(),
			TEXT("Mixed-push BlueprintEvent wrapper test case should emit exactly one module descriptor")));
		if (!bPreprocessSucceeded || Modules.Num() != 1)
		{
			return;
		}

		const TSharedRef<FAngelscriptModuleDesc> ModuleDesc = Modules[0];
		const TSharedPtr<FAngelscriptClassDesc> ClassDesc = ModuleDesc->GetClass(CompilerPipelineBlueprintEventMixedPushTest::ClassName);
		if (!this->Assert.IsTrue(ClassDesc.IsValid(), TEXT("Mixed-push BlueprintEvent wrapper test case should parse the annotated class descriptor")))
		{
			return;
		}

		const TSharedPtr<FAngelscriptFunctionDesc> EventDesc = ClassDesc->GetMethod(CompilerPipelineBlueprintEventMixedPushTest::EvaluateFunctionName);
		const TSharedPtr<FAngelscriptFunctionDesc> EntryDesc = ClassDesc->GetMethod(CompilerPipelineBlueprintEventMixedPushTest::EntryFunctionName);
		if (!this->Assert.IsTrue(EventDesc.IsValid(), TEXT("Mixed-push BlueprintEvent wrapper test case should parse the BlueprintEvent function descriptor"))
			|| !this->Assert.IsTrue(EntryDesc.IsValid(), TEXT("Mixed-push BlueprintEvent wrapper test case should parse the entry function descriptor")))
		{
			return;
		}

		const FString& ProcessedCode = ModuleDesc->Code[0].Code;
		ASSERT_THAT(IsTrue(
			EventDesc->bBlueprintEvent,
			TEXT("Mixed-push BlueprintEvent wrapper test case should mark EvaluateMixedPush as a BlueprintEvent during preprocessing")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EvaluateMixedPush_Implementation")),
			EventDesc->ScriptFunctionName,
			TEXT("Mixed-push BlueprintEvent wrapper test case should rename the backing script implementation")));
		ASSERT_THAT(IsTrue(
			ProcessedCode.Contains(TEXT("__Evt_PushArgumentRef__FString(Label);")),
			TEXT("Mixed-push BlueprintEvent wrapper test case should use specialized ref push code for const FString arguments")));
		ASSERT_THAT(IsTrue(
			ProcessedCode.Contains(TEXT("__Evt_PushArgument(TypeValue);")),
			TEXT("Mixed-push BlueprintEvent wrapper test case should use generic push code for TSubclassOf arguments")));

		Engine.ResetDiagnostics();

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			CompilerPipelineBlueprintEventMixedPushTest::ModuleName,
			CompilerPipelineBlueprintEventMixedPushTest::RelativeScriptPath,
			ScriptSource,
			true,
			Summary);

		ASSERT_THAT(IsTrue(
			bCompiled,
			TEXT("Mixed-push BlueprintEvent wrapper test case should compile through the normal preprocessor pipeline")));
		ASSERT_THAT(IsTrue(
			Summary.bUsedPreprocessor,
			TEXT("Mixed-push BlueprintEvent wrapper test case should record preprocessor usage in the compile summary")));
		ASSERT_THAT(IsTrue(
			Summary.bCompileSucceeded,
			TEXT("Mixed-push BlueprintEvent wrapper test case should mark compile succeeded in the summary")));
		ASSERT_THAT(AreEqual(
			0,
			Summary.Diagnostics.Num(),
			TEXT("Mixed-push BlueprintEvent wrapper test case should keep compile diagnostics empty")));
		if (!bCompiled)
		{
			return;
		}

		UClass* GeneratedClass = FindGeneratedClass(&Engine, *CompilerPipelineBlueprintEventMixedPushTest::ClassName);
		if (!this->Assert.IsNotNull(GeneratedClass, TEXT("Mixed-push BlueprintEvent wrapper test case should materialize the generated class")))
		{
			return;
		}

		UFunction* EventFunction = FindGeneratedFunction(GeneratedClass, *CompilerPipelineBlueprintEventMixedPushTest::EvaluateFunctionName);
		UFunction* EntryFunction = FindGeneratedFunction(GeneratedClass, *CompilerPipelineBlueprintEventMixedPushTest::EntryFunctionName);
		if (!this->Assert.IsNotNull(EventFunction, TEXT("Mixed-push BlueprintEvent wrapper test case should materialize the generated event wrapper function"))
			|| !this->Assert.IsNotNull(EntryFunction, TEXT("Mixed-push BlueprintEvent wrapper test case should materialize the generated entry function")))
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			EventFunction->HasAnyFunctionFlags(FUNC_BlueprintEvent),
			TEXT("Mixed-push BlueprintEvent wrapper test case should surface EvaluateMixedPush as a BlueprintEvent UFunction")));

		UObject* RuntimeObject = NewObject<UObject>(GetTransientPackage(), GeneratedClass, TEXT("CompilerBlueprintEventMixedPushCarrier"));
		if (!this->Assert.IsNotNull(RuntimeObject, TEXT("Mixed-push BlueprintEvent wrapper test case should instantiate the generated class")))
		{
			return;
		}

		int32 Result = 0;
		const bool bExecuted = ExecuteGeneratedIntEventOnGameThread(&Engine, RuntimeObject, EntryFunction, Result);
		ASSERT_THAT(IsTrue(
			bExecuted,
			TEXT("Mixed-push BlueprintEvent wrapper test case should execute the generated entry function")));
		if (bExecuted)
		{
			ASSERT_THAT(AreEqual(
				42,
				Result,
				TEXT("Mixed-push BlueprintEvent wrapper test case should route through the wrapper and preserve mixed push arguments")));
		}

		}

	}

};

#endif
