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
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CompilerPipelinePropertyDefaultTest
{
	static const FName ModuleName(TEXT("Tests.Compiler.StringDefaultPreservesCommentMarkersInsideLiteral"));
	static const FString RelativeScriptPath(TEXT("Tests/Compiler/StringDefaultPreservesCommentMarkersInsideLiteral.as"));
	static const FString ClassName(TEXT("UCompilerStringDefaultCarrier"));
	static const FString MessagePropertyName(TEXT("Message"));
	static const FString BlockTextPropertyName(TEXT("BlockText"));
	static const FName VerifyFunctionName(TEXT("VerifyDefaults"));
	static const FString ExpectedDefaultsCode(TEXT("Message = \"He said \\\"//not a comment\\\"\";BlockText = \"/*literal*/\";"));
	static const FString ExpectedMessage(TEXT("He said \"//not a comment\""));
	static const FString ExpectedBlockText(TEXT("/*literal*/"));
	static const int32 ExpectedVerifyResult = 42;

	struct FRuntimeDefaultObservation
	{
		FString DefaultMessage;
		FString DefaultBlockText;
		FString RuntimeMessage;
		FString RuntimeBlockText;
		int32 VerifyResult = INDEX_NONE;
	};

	FString GetFixtureRoot()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("CompilerPropertyDefaultFixtures"));
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

	FString JoinMessages(const TArray<FString>& Messages)
	{
		return FString::Join(Messages, TEXT(" | "));
	}

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

	bool ReadStringPropertyValue(
		FAutomationTestBase& Test,
		const TCHAR* Context,
		FStrProperty* Property,
		UObject* Object,
		FString& OutValue)
	{
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("%s should expose the reflected FString property"), Context),
				Property)
			|| !Test.TestNotNull(
				*FString::Printf(TEXT("%s should expose the target object"), Context),
				Object))
		{
			return false;
		}

		OutValue = Property->GetPropertyValue_InContainer(Object);
		return true;
	}

	bool VerifyStringValue(
		FAutomationTestBase& Test,
		const TCHAR* Context,
		const FString& ActualValue,
		const FString& ExpectedValue)
	{
		return Test.TestEqual(Context, ActualValue, ExpectedValue);
	}
}

using namespace CompilerPipelinePropertyDefaultTest;

TEST_CLASS_WITH_FLAGS(FCompilerPipelinePropertyDefaultTests,
	"Angelscript.TestModule.Compiler.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(StringDefaultPreservesCommentMarkersInsideLiteral)
	{


		const FString TestScriptSource = TEXT(R"AS(
	UCLASS()
	class UCompilerStringDefaultCarrier : UObject
	{
		UPROPERTY()
		FString Message;

		UPROPERTY()
		FString BlockText;

		default Message = "He said \"//not a comment\"";
		default BlockText = "/*literal*/";

		UFUNCTION()
		int VerifyDefaults()
		{
			if (!(Message == "He said \"//not a comment\""))
				return 10;

			if (!(BlockText == "/*literal*/"))
				return 20;

			return 42;
		}
	}
	)AS");

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		const FString AbsoluteScriptPath = CompilerPipelinePropertyDefaultTest::WriteFixture(
			CompilerPipelinePropertyDefaultTest::RelativeScriptPath,
			TestScriptSource);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*CompilerPipelinePropertyDefaultTest::ModuleName.ToString());
			IFileManager::Get().Delete(*AbsoluteScriptPath, false, true);
		};

		Engine.ResetDiagnostics();

		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddFile(CompilerPipelinePropertyDefaultTest::RelativeScriptPath, AbsoluteScriptPath);

		const bool bPreprocessSucceeded = Preprocessor.Preprocess();
		const TArray<TSharedRef<FAngelscriptModuleDesc>> Modules = Preprocessor.GetModulesToCompile();

		int32 PreprocessErrorCount = 0;
		const TArray<FString> PreprocessMessages = CompilerPipelinePropertyDefaultTest::CollectDiagnosticMessages(
			Engine,
			AbsoluteScriptPath,
			PreprocessErrorCount);

		if (PreprocessMessages.Num() > 0)
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("Preprocess diagnostics: %s"),
				*CompilerPipelinePropertyDefaultTest::JoinMessages(PreprocessMessages)));
		}

		TestRunner->TestTrue(
			TEXT("String default literal test case should preprocess successfully"),
			bPreprocessSucceeded);
		TestRunner->TestEqual(
			TEXT("String default literal test case should not emit preprocessing errors"),
			PreprocessErrorCount,
			0);
		TestRunner->TestEqual(
			TEXT("String default literal test case should keep preprocessing diagnostics empty"),
			PreprocessMessages.Num(),
			0);
		TestRunner->TestEqual(
			TEXT("String default literal test case should emit exactly one module descriptor"),
			Modules.Num(),
			1);
		if (!bPreprocessSucceeded || Modules.Num() != 1)
		{
			return;
		}

		const TSharedRef<FAngelscriptModuleDesc> ModuleDesc = Modules[0];
		TestRunner->TestEqual(
			TEXT("String default literal test case should preserve the expected module name"),
			ModuleDesc->ModuleName,
			CompilerPipelinePropertyDefaultTest::ModuleName.ToString());

		const TSharedPtr<FAngelscriptClassDesc> ClassDesc = ModuleDesc->GetClass(CompilerPipelinePropertyDefaultTest::ClassName);
		if (!TestRunner->TestTrue(TEXT("String default literal test case should parse the annotated class descriptor"), ClassDesc.IsValid()))
		{
			return;
		}

		TestRunner->TestEqual(
			TEXT("String default literal test case should preserve the exact defaults code text"),
			ClassDesc->DefaultsCode,
			CompilerPipelinePropertyDefaultTest::ExpectedDefaultsCode);
		TestRunner->TestTrue(
			TEXT("String default literal test case should keep the line-comment marker inside the defaults code"),
			ClassDesc->DefaultsCode.Contains(TEXT("//not a comment")));
		TestRunner->TestTrue(
			TEXT("String default literal test case should keep the block-comment marker inside the defaults code"),
			ClassDesc->DefaultsCode.Contains(TEXT("/*literal*/")));

		Engine.ResetDiagnostics();

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			CompilerPipelinePropertyDefaultTest::ModuleName,
			CompilerPipelinePropertyDefaultTest::RelativeScriptPath,
			TestScriptSource,
			true,
			Summary,
			true);

		if (Summary.Diagnostics.Num() > 0)
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("Compile diagnostics: %s"),
				*CompilerPipelinePropertyDefaultTest::JoinDiagnostics(Summary.Diagnostics)));
		}

		TestRunner->TestTrue(
			TEXT("String default literal test case should compile through the normal preprocessor pipeline"),
			bCompiled);
		TestRunner->TestTrue(
			TEXT("String default literal test case should record preprocessor usage in the compile summary"),
			Summary.bUsedPreprocessor);
		TestRunner->TestTrue(
			TEXT("String default literal test case should mark compile succeeded in the summary"),
			Summary.bCompileSucceeded);
		TestRunner->TestEqual(
			TEXT("String default literal test case should finish with a fully handled compile result"),
			Summary.CompileResult,
			ECompileResult::FullyHandled);
		TestRunner->TestEqual(
			TEXT("String default literal test case should keep compile diagnostics empty"),
			Summary.Diagnostics.Num(),
			0);
		if (!bCompiled || !Summary.bCompileSucceeded)
		{
			return;
		}

		UClass* GeneratedClass = FindGeneratedClass(&Engine, *CompilerPipelinePropertyDefaultTest::ClassName);
		if (!TestRunner->TestNotNull(TEXT("String default literal test case should materialize the generated class"), GeneratedClass))
		{
			return;
		}

		FStrProperty* MessageProperty = FindFProperty<FStrProperty>(GeneratedClass, *CompilerPipelinePropertyDefaultTest::MessagePropertyName);
		FStrProperty* BlockTextProperty = FindFProperty<FStrProperty>(GeneratedClass, *CompilerPipelinePropertyDefaultTest::BlockTextPropertyName);
		UFunction* VerifyDefaultsFunction = FindGeneratedFunction(GeneratedClass, CompilerPipelinePropertyDefaultTest::VerifyFunctionName);
		if (!TestRunner->TestNotNull(TEXT("String default literal test case should materialize the Message property"), MessageProperty)
			|| !TestRunner->TestNotNull(TEXT("String default literal test case should materialize the BlockText property"), BlockTextProperty)
			|| !TestRunner->TestNotNull(TEXT("String default literal test case should materialize the verification function"), VerifyDefaultsFunction))
		{
			return;
		}

		UObject* DefaultObject = GeneratedClass->GetDefaultObject();
		UObject* RuntimeObject = NewObject<UObject>(GetTransientPackage(), GeneratedClass, TEXT("CompilerStringDefaultCarrier"));
		if (!TestRunner->TestNotNull(TEXT("String default literal test case should expose the generated CDO"), DefaultObject)
			|| !TestRunner->TestNotNull(TEXT("String default literal test case should instantiate the generated class"), RuntimeObject))
		{
			return;
		}

		CompilerPipelinePropertyDefaultTest::FRuntimeDefaultObservation Observation;
		CompilerPipelinePropertyDefaultTest::ReadStringPropertyValue(
			*TestRunner,
			TEXT("String default literal test case should read Message from the CDO"),
			MessageProperty,
			DefaultObject,
			Observation.DefaultMessage);
		CompilerPipelinePropertyDefaultTest::ReadStringPropertyValue(
			*TestRunner,
			TEXT("String default literal test case should read BlockText from the CDO"),
			BlockTextProperty,
			DefaultObject,
			Observation.DefaultBlockText);
		CompilerPipelinePropertyDefaultTest::ReadStringPropertyValue(
			*TestRunner,
			TEXT("String default literal test case should read Message from a runtime instance"),
			MessageProperty,
			RuntimeObject,
			Observation.RuntimeMessage);
		CompilerPipelinePropertyDefaultTest::ReadStringPropertyValue(
			*TestRunner,
			TEXT("String default literal test case should read BlockText from a runtime instance"),
			BlockTextProperty,
			RuntimeObject,
			Observation.RuntimeBlockText);

		CompilerPipelinePropertyDefaultTest::VerifyStringValue(
			*TestRunner,
			TEXT("String default literal test case should preserve Message on the CDO"),
			Observation.DefaultMessage,
			CompilerPipelinePropertyDefaultTest::ExpectedMessage);
		CompilerPipelinePropertyDefaultTest::VerifyStringValue(
			*TestRunner,
			TEXT("String default literal test case should preserve BlockText on the CDO"),
			Observation.DefaultBlockText,
			CompilerPipelinePropertyDefaultTest::ExpectedBlockText);
		CompilerPipelinePropertyDefaultTest::VerifyStringValue(
			*TestRunner,
			TEXT("String default literal test case should preserve Message on runtime instances"),
			Observation.RuntimeMessage,
			CompilerPipelinePropertyDefaultTest::ExpectedMessage);
		CompilerPipelinePropertyDefaultTest::VerifyStringValue(
			*TestRunner,
			TEXT("String default literal test case should preserve BlockText on runtime instances"),
			Observation.RuntimeBlockText,
			CompilerPipelinePropertyDefaultTest::ExpectedBlockText);

		const bool bExecuted = ExecuteGeneratedIntEventOnGameThread(
			&Engine,
			RuntimeObject,
			VerifyDefaultsFunction,
			Observation.VerifyResult);
		TestRunner->TestTrue(
			TEXT("String default literal test case should execute the generated verification function"),
			bExecuted);
		if (bExecuted)
		{
			TestRunner->TestEqual(
				TEXT("String default literal test case should keep comment markers and escaped quotes visible to script runtime code"),
				Observation.VerifyResult,
				CompilerPipelinePropertyDefaultTest::ExpectedVerifyResult);
		}

		}

	}

};

#endif
