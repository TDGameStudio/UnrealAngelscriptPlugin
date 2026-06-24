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
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(
				Property,
				*FString::Printf(TEXT("%s should expose the reflected FString property"), Context))
			|| !LocalAssert.IsNotNull(
				Object,
				*FString::Printf(TEXT("%s should expose the target object"), Context)))
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
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.AreEqual(ExpectedValue, ActualValue, Context);
	}
}

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

		ASSERT_THAT(IsTrue(
			bPreprocessSucceeded,
			TEXT("String default literal test case should preprocess successfully")));
		ASSERT_THAT(AreEqual(
			0,
			PreprocessErrorCount,
			TEXT("String default literal test case should not emit preprocessing errors")));
		ASSERT_THAT(AreEqual(
			0,
			PreprocessMessages.Num(),
			TEXT("String default literal test case should keep preprocessing diagnostics empty")));
		ASSERT_THAT(AreEqual(
			1,
			Modules.Num(),
			TEXT("String default literal test case should emit exactly one module descriptor")));
		if (!bPreprocessSucceeded || Modules.Num() != 1)
		{
			return;
		}

		const TSharedRef<FAngelscriptModuleDesc> ModuleDesc = Modules[0];
		ASSERT_THAT(AreEqual(
			CompilerPipelinePropertyDefaultTest::ModuleName.ToString(),
			ModuleDesc->ModuleName,
			TEXT("String default literal test case should preserve the expected module name")));

		const TSharedPtr<FAngelscriptClassDesc> ClassDesc = ModuleDesc->GetClass(CompilerPipelinePropertyDefaultTest::ClassName);
		if (!this->Assert.IsTrue(ClassDesc.IsValid(), TEXT("String default literal test case should parse the annotated class descriptor")))
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			CompilerPipelinePropertyDefaultTest::ExpectedDefaultsCode,
			ClassDesc->DefaultsCode,
			TEXT("String default literal test case should preserve the exact defaults code text")));
		ASSERT_THAT(IsTrue(
			ClassDesc->DefaultsCode.Contains(TEXT("//not a comment")),
			TEXT("String default literal test case should keep the line-comment marker inside the defaults code")));
		ASSERT_THAT(IsTrue(
			ClassDesc->DefaultsCode.Contains(TEXT("/*literal*/")),
			TEXT("String default literal test case should keep the block-comment marker inside the defaults code")));

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

		ASSERT_THAT(IsTrue(
			bCompiled,
			TEXT("String default literal test case should compile through the normal preprocessor pipeline")));
		ASSERT_THAT(IsTrue(
			Summary.bUsedPreprocessor,
			TEXT("String default literal test case should record preprocessor usage in the compile summary")));
		ASSERT_THAT(IsTrue(
			Summary.bCompileSucceeded,
			TEXT("String default literal test case should mark compile succeeded in the summary")));
		ASSERT_THAT(AreEqual(
			ECompileResult::FullyHandled,
			Summary.CompileResult,
			TEXT("String default literal test case should finish with a fully handled compile result")));
		ASSERT_THAT(AreEqual(
			0,
			Summary.Diagnostics.Num(),
			TEXT("String default literal test case should keep compile diagnostics empty")));
		if (!bCompiled || !Summary.bCompileSucceeded)
		{
			return;
		}

		UClass* GeneratedClass = FindGeneratedClass(&Engine, *CompilerPipelinePropertyDefaultTest::ClassName);
		if (!this->Assert.IsNotNull(GeneratedClass, TEXT("String default literal test case should materialize the generated class")))
		{
			return;
		}

		FStrProperty* MessageProperty = FindFProperty<FStrProperty>(GeneratedClass, *CompilerPipelinePropertyDefaultTest::MessagePropertyName);
		FStrProperty* BlockTextProperty = FindFProperty<FStrProperty>(GeneratedClass, *CompilerPipelinePropertyDefaultTest::BlockTextPropertyName);
		UFunction* VerifyDefaultsFunction = FindGeneratedFunction(GeneratedClass, CompilerPipelinePropertyDefaultTest::VerifyFunctionName);
		if (!this->Assert.IsNotNull(MessageProperty, TEXT("String default literal test case should materialize the Message property"))
			|| !this->Assert.IsNotNull(BlockTextProperty, TEXT("String default literal test case should materialize the BlockText property"))
			|| !this->Assert.IsNotNull(VerifyDefaultsFunction, TEXT("String default literal test case should materialize the verification function")))
		{
			return;
		}

		UObject* DefaultObject = GeneratedClass->GetDefaultObject();
		UObject* RuntimeObject = NewObject<UObject>(GetTransientPackage(), GeneratedClass, TEXT("CompilerStringDefaultCarrier"));
		if (!this->Assert.IsNotNull(DefaultObject, TEXT("String default literal test case should expose the generated CDO"))
			|| !this->Assert.IsNotNull(RuntimeObject, TEXT("String default literal test case should instantiate the generated class")))
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
		ASSERT_THAT(IsTrue(
			bExecuted,
			TEXT("String default literal test case should execute the generated verification function")));
		if (bExecuted)
		{
			ASSERT_THAT(AreEqual(
				CompilerPipelinePropertyDefaultTest::ExpectedVerifyResult,
				Observation.VerifyResult,
				TEXT("String default literal test case should keep comment markers and escaped quotes visible to script runtime code")));
		}

		}

	}

};

#endif
