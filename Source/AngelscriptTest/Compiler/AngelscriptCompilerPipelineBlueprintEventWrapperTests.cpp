#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptTestMacros.h"

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

using namespace CompilerPipelineBlueprintEventWrapperTest;

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

		TestRunner->TestTrue(
			TEXT("BlueprintEvent wrapper test case should preprocess successfully"),
			bPreprocessSucceeded);
		TestRunner->TestEqual(
			TEXT("BlueprintEvent wrapper test case should keep preprocessing errors at zero"),
			PreprocessErrorCount,
			0);
		TestRunner->TestEqual(
			TEXT("BlueprintEvent wrapper test case should keep preprocessing diagnostics empty"),
			PreprocessMessages.Num(),
			0);
		TestRunner->TestEqual(
			TEXT("BlueprintEvent wrapper test case should emit exactly one module descriptor"),
			Modules.Num(),
			1);
		if (!bPreprocessSucceeded || Modules.Num() != 1)
		{
			return;
		}

		const TSharedRef<FAngelscriptModuleDesc> ModuleDesc = Modules[0];
		const TSharedPtr<FAngelscriptClassDesc> ClassDesc = ModuleDesc->GetClass(CompilerPipelineBlueprintEventWrapperTest::ClassName);
		if (!TestRunner->TestTrue(TEXT("BlueprintEvent wrapper test case should parse the annotated class descriptor"), ClassDesc.IsValid()))
		{
			return;
		}

		const TSharedPtr<FAngelscriptFunctionDesc> ComputeDesc = ClassDesc->GetMethod(CompilerPipelineBlueprintEventWrapperTest::ComputeFunctionName);
		const TSharedPtr<FAngelscriptFunctionDesc> EntryDesc = ClassDesc->GetMethod(CompilerPipelineBlueprintEventWrapperTest::EntryFunctionName);
		if (!TestRunner->TestTrue(TEXT("BlueprintEvent wrapper test case should parse the BlueprintEvent function descriptor"), ComputeDesc.IsValid())
			|| !TestRunner->TestTrue(TEXT("BlueprintEvent wrapper test case should parse the entry function descriptor"), EntryDesc.IsValid()))
		{
			return;
		}

		const FString& ProcessedCode = ModuleDesc->Code[0].Code;
		TestRunner->TestTrue(
			TEXT("BlueprintEvent wrapper test case should mark Compute as a BlueprintEvent during preprocessing"),
			ComputeDesc->bBlueprintEvent);
		TestRunner->TestTrue(
			TEXT("BlueprintEvent wrapper test case should leave Compute overridable during preprocessing"),
			ComputeDesc->bCanOverrideEvent);
		TestRunner->TestFalse(
			TEXT("BlueprintEvent wrapper test case should not make a plain BlueprintEvent callable from blueprint by default"),
			ComputeDesc->bBlueprintCallable);
		TestRunner->TestEqual(
			TEXT("BlueprintEvent wrapper test case should rename the backing script implementation"),
			ComputeDesc->ScriptFunctionName,
			FString(TEXT("Compute_Implementation")));
		TestRunner->TestTrue(
			TEXT("BlueprintEvent wrapper test case should inject an __Evt_Execute wrapper call into the processed code"),
			ProcessedCode.Contains(TEXT("__Evt_Execute(this, __STATIC_NAME(")));

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

		TestRunner->TestTrue(
			TEXT("BlueprintEvent wrapper test case should compile through the normal preprocessor pipeline"),
			bCompiled);
		TestRunner->TestTrue(
			TEXT("BlueprintEvent wrapper test case should record preprocessor usage in the compile summary"),
			Summary.bUsedPreprocessor);
		TestRunner->TestTrue(
			TEXT("BlueprintEvent wrapper test case should mark compile succeeded in the summary"),
			Summary.bCompileSucceeded);
		TestRunner->TestEqual(
			TEXT("BlueprintEvent wrapper test case should keep compile diagnostics empty"),
			Summary.Diagnostics.Num(),
			0);
		if (!bCompiled)
		{
			return;
		}

		UClass* GeneratedClass = FindGeneratedClass(&Engine, *CompilerPipelineBlueprintEventWrapperTest::ClassName);
		if (!TestRunner->TestNotNull(TEXT("BlueprintEvent wrapper test case should materialize the generated class"), GeneratedClass))
		{
			return;
		}

		UFunction* ComputeFunction = FindGeneratedFunction(GeneratedClass, *CompilerPipelineBlueprintEventWrapperTest::ComputeFunctionName);
		UFunction* EntryFunction = FindGeneratedFunction(GeneratedClass, *CompilerPipelineBlueprintEventWrapperTest::EntryFunctionName);
		if (!TestRunner->TestNotNull(TEXT("BlueprintEvent wrapper test case should materialize the generated event wrapper function"), ComputeFunction)
			|| !TestRunner->TestNotNull(TEXT("BlueprintEvent wrapper test case should materialize the generated entry function"), EntryFunction))
		{
			return;
		}

		TestRunner->TestTrue(
			TEXT("BlueprintEvent wrapper test case should surface Compute as a BlueprintEvent UFunction"),
			ComputeFunction->HasAnyFunctionFlags(FUNC_BlueprintEvent));
		TestRunner->TestFalse(
			TEXT("BlueprintEvent wrapper test case should keep Compute non-blueprint-callable without an explicit BlueprintCallable specifier"),
			ComputeFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable));

		UObject* RuntimeObject = NewObject<UObject>(GetTransientPackage(), GeneratedClass, TEXT("CompilerBlueprintEventWrapperCarrier"));
		if (!TestRunner->TestNotNull(TEXT("BlueprintEvent wrapper test case should instantiate the generated class"), RuntimeObject))
		{
			return;
		}

		int32 Result = 0;
		const bool bExecuted = ExecuteGeneratedIntEventOnGameThread(&Engine, RuntimeObject, EntryFunction, Result);
		TestRunner->TestTrue(
			TEXT("BlueprintEvent wrapper test case should execute the generated entry function"),
			bExecuted);
		if (bExecuted)
		{
			TestRunner->TestEqual(
				TEXT("BlueprintEvent wrapper test case should route through the wrapper and return the _Implementation result"),
				Result,
				42);
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

		TestRunner->TestTrue(
			TEXT("Mixed-push BlueprintEvent wrapper test case should preprocess successfully"),
			bPreprocessSucceeded);
		TestRunner->TestEqual(
			TEXT("Mixed-push BlueprintEvent wrapper test case should keep preprocessing errors at zero"),
			PreprocessErrorCount,
			0);
		TestRunner->TestEqual(
			TEXT("Mixed-push BlueprintEvent wrapper test case should keep preprocessing diagnostics empty"),
			PreprocessMessages.Num(),
			0);
		TestRunner->TestEqual(
			TEXT("Mixed-push BlueprintEvent wrapper test case should emit exactly one module descriptor"),
			Modules.Num(),
			1);
		if (!bPreprocessSucceeded || Modules.Num() != 1)
		{
			return;
		}

		const TSharedRef<FAngelscriptModuleDesc> ModuleDesc = Modules[0];
		const TSharedPtr<FAngelscriptClassDesc> ClassDesc = ModuleDesc->GetClass(CompilerPipelineBlueprintEventMixedPushTest::ClassName);
		if (!TestRunner->TestTrue(TEXT("Mixed-push BlueprintEvent wrapper test case should parse the annotated class descriptor"), ClassDesc.IsValid()))
		{
			return;
		}

		const TSharedPtr<FAngelscriptFunctionDesc> EventDesc = ClassDesc->GetMethod(CompilerPipelineBlueprintEventMixedPushTest::EvaluateFunctionName);
		const TSharedPtr<FAngelscriptFunctionDesc> EntryDesc = ClassDesc->GetMethod(CompilerPipelineBlueprintEventMixedPushTest::EntryFunctionName);
		if (!TestRunner->TestTrue(TEXT("Mixed-push BlueprintEvent wrapper test case should parse the BlueprintEvent function descriptor"), EventDesc.IsValid())
			|| !TestRunner->TestTrue(TEXT("Mixed-push BlueprintEvent wrapper test case should parse the entry function descriptor"), EntryDesc.IsValid()))
		{
			return;
		}

		const FString& ProcessedCode = ModuleDesc->Code[0].Code;
		TestRunner->TestTrue(
			TEXT("Mixed-push BlueprintEvent wrapper test case should mark EvaluateMixedPush as a BlueprintEvent during preprocessing"),
			EventDesc->bBlueprintEvent);
		TestRunner->TestEqual(
			TEXT("Mixed-push BlueprintEvent wrapper test case should rename the backing script implementation"),
			EventDesc->ScriptFunctionName,
			FString(TEXT("EvaluateMixedPush_Implementation")));
		TestRunner->TestTrue(
			TEXT("Mixed-push BlueprintEvent wrapper test case should use specialized ref push code for const FString arguments"),
			ProcessedCode.Contains(TEXT("__Evt_PushArgumentRef__FString(Label);")));
		TestRunner->TestTrue(
			TEXT("Mixed-push BlueprintEvent wrapper test case should use generic push code for TSubclassOf arguments"),
			ProcessedCode.Contains(TEXT("__Evt_PushArgument(TypeValue);")));

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

		TestRunner->TestTrue(
			TEXT("Mixed-push BlueprintEvent wrapper test case should compile through the normal preprocessor pipeline"),
			bCompiled);
		TestRunner->TestTrue(
			TEXT("Mixed-push BlueprintEvent wrapper test case should record preprocessor usage in the compile summary"),
			Summary.bUsedPreprocessor);
		TestRunner->TestTrue(
			TEXT("Mixed-push BlueprintEvent wrapper test case should mark compile succeeded in the summary"),
			Summary.bCompileSucceeded);
		TestRunner->TestEqual(
			TEXT("Mixed-push BlueprintEvent wrapper test case should keep compile diagnostics empty"),
			Summary.Diagnostics.Num(),
			0);
		if (!bCompiled)
		{
			return;
		}

		UClass* GeneratedClass = FindGeneratedClass(&Engine, *CompilerPipelineBlueprintEventMixedPushTest::ClassName);
		if (!TestRunner->TestNotNull(TEXT("Mixed-push BlueprintEvent wrapper test case should materialize the generated class"), GeneratedClass))
		{
			return;
		}

		UFunction* EventFunction = FindGeneratedFunction(GeneratedClass, *CompilerPipelineBlueprintEventMixedPushTest::EvaluateFunctionName);
		UFunction* EntryFunction = FindGeneratedFunction(GeneratedClass, *CompilerPipelineBlueprintEventMixedPushTest::EntryFunctionName);
		if (!TestRunner->TestNotNull(TEXT("Mixed-push BlueprintEvent wrapper test case should materialize the generated event wrapper function"), EventFunction)
			|| !TestRunner->TestNotNull(TEXT("Mixed-push BlueprintEvent wrapper test case should materialize the generated entry function"), EntryFunction))
		{
			return;
		}

		TestRunner->TestTrue(
			TEXT("Mixed-push BlueprintEvent wrapper test case should surface EvaluateMixedPush as a BlueprintEvent UFunction"),
			EventFunction->HasAnyFunctionFlags(FUNC_BlueprintEvent));

		UObject* RuntimeObject = NewObject<UObject>(GetTransientPackage(), GeneratedClass, TEXT("CompilerBlueprintEventMixedPushCarrier"));
		if (!TestRunner->TestNotNull(TEXT("Mixed-push BlueprintEvent wrapper test case should instantiate the generated class"), RuntimeObject))
		{
			return;
		}

		int32 Result = 0;
		const bool bExecuted = ExecuteGeneratedIntEventOnGameThread(&Engine, RuntimeObject, EntryFunction, Result);
		TestRunner->TestTrue(
			TEXT("Mixed-push BlueprintEvent wrapper test case should execute the generated entry function"),
			bExecuted);
		if (bExecuted)
		{
			TestRunner->TestEqual(
				TEXT("Mixed-push BlueprintEvent wrapper test case should route through the wrapper and preserve mixed push arguments"),
				Result,
				42);
		}

		}

	}

};

#endif
