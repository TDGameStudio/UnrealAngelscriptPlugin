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
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace CompilerPropertyMetadataTest
{
	static const FName ModuleName(TEXT("Tests.Compiler.PropertyCallbackMetadataRoundTrip"));
	static const FString RelativeScriptPath(TEXT("Tests/Compiler/PropertyCallbackMetadataRoundTrip.as"));
	static const FString ClassName(TEXT("UPropertyCallbackCarrier"));
	static const FString PropertyName(TEXT("TrackedValue"));
	static const FString EntryFunctionDeclaration(TEXT("int Entry()"));
	static const FString OnRepFunctionName(TEXT("OnRep_TrackedValue"));
	static const FString GetterFunctionName(TEXT("GetTrackedValue"));
	static const FString SetterFunctionName(TEXT("SetTrackedValue"));
	static const int32 ExpectedEntryValue = 42;

	struct FPropertyCallbackValidationTestCase
	{
		const TCHAR* Label;
		FName ModuleName;
		FString RelativeScriptPath;
		const TCHAR* ScriptSource;
		const TCHAR* ExpectedMessageFragment;
		int32 ExpectedRow;
	};

	FString GetFixtureRoot()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("CompilerPropertyMetadataFixtures"));
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

	FString GetPropertyMeta(const TSharedPtr<FAngelscriptPropertyDesc>& PropertyDesc, const TCHAR* Key)
	{
		if (!PropertyDesc.IsValid())
		{
			return FString();
		}

		const FString* Value = PropertyDesc->Meta.Find(FName(Key));
		return Value != nullptr ? *Value : FString();
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

	const FAngelscriptCompileTraceDiagnosticSummary* FindMatchingErrorDiagnostic(
		const TArray<FAngelscriptCompileTraceDiagnosticSummary>& Diagnostics,
		const FString& MessageFragment)
	{
		return Diagnostics.FindByPredicate(
			[&MessageFragment](const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic)
			{
				return Diagnostic.bIsError && Diagnostic.Message.Contains(MessageFragment);
			});
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCompilerPropertyMetadataTests,
	"Angelscript.TestModule.Compiler.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(PropertyCallbackMetadataRoundTrip)
	{


		const FString TestScriptSource = TEXT(R"AS(
	UCLASS()
	class UPropertyCallbackCarrier : UObject
	{
		UPROPERTY(ReplicatedUsing=OnRep_TrackedValue, BlueprintGetter=GetTrackedValue, BlueprintSetter=SetTrackedValue)
		int TrackedValue;

		UFUNCTION()
		void OnRep_TrackedValue()
		{
		}

		UFUNCTION(BlueprintPure)
		int GetTrackedValue() const
		{
			return TrackedValue;
		}

		UFUNCTION()
		void SetTrackedValue(int Value)
		{
			TrackedValue = Value;
		}
	}

	int Entry()
	{
		return 42;
	}
	)AS");

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		const FString AbsoluteScriptPath = CompilerPropertyMetadataTest::WriteFixture(
			CompilerPropertyMetadataTest::RelativeScriptPath,
			TestScriptSource);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*CompilerPropertyMetadataTest::ModuleName.ToString());
			IFileManager::Get().Delete(*AbsoluteScriptPath, false, true);
		};

		Engine.ResetDiagnostics();

		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddFile(CompilerPropertyMetadataTest::RelativeScriptPath, AbsoluteScriptPath);

		const bool bPreprocessSucceeded = Preprocessor.Preprocess();
		const TArray<TSharedRef<FAngelscriptModuleDesc>> Modules = Preprocessor.GetModulesToCompile();

		int32 PreprocessErrorCount = 0;
		const TArray<FString> PreprocessMessages = CompilerPropertyMetadataTest::CollectDiagnosticMessages(
			Engine,
			AbsoluteScriptPath,
			PreprocessErrorCount);

		ASSERT_THAT(IsTrue(
			bPreprocessSucceeded,
			TEXT("Property callback metadata test case should preprocess successfully")));
		ASSERT_THAT(AreEqual(
			0,
			PreprocessErrorCount,
			TEXT("Property callback metadata test case should not emit preprocessing errors")));
		ASSERT_THAT(AreEqual(
			0,
			PreprocessMessages.Num(),
			TEXT("Property callback metadata test case should keep preprocessing diagnostics empty")));
		ASSERT_THAT(AreEqual(
			1,
			Modules.Num(),
			TEXT("Property callback metadata test case should produce exactly one module descriptor")));
		if (!bPreprocessSucceeded || Modules.Num() != 1)
		{
			return;
		}

		const TSharedRef<FAngelscriptModuleDesc> ModuleDesc = Modules[0];
		ASSERT_THAT(AreEqual(
			CompilerPropertyMetadataTest::ModuleName.ToString(),
			ModuleDesc->ModuleName,
			TEXT("Property callback metadata test case should preserve the expected module name")));

		const TSharedPtr<FAngelscriptClassDesc> ClassDesc = ModuleDesc->GetClass(CompilerPropertyMetadataTest::ClassName);
		if (!this->Assert.IsTrue(ClassDesc.IsValid(), TEXT("Property callback metadata test case should parse the annotated class descriptor")))
		{
			return;
		}

		const TSharedPtr<FAngelscriptPropertyDesc> PropertyDesc = ClassDesc->GetProperty(CompilerPropertyMetadataTest::PropertyName);
		const TSharedPtr<FAngelscriptFunctionDesc> OnRepDesc = ClassDesc->GetMethod(CompilerPropertyMetadataTest::OnRepFunctionName);
		const TSharedPtr<FAngelscriptFunctionDesc> GetterDesc = ClassDesc->GetMethod(CompilerPropertyMetadataTest::GetterFunctionName);
		const TSharedPtr<FAngelscriptFunctionDesc> SetterDesc = ClassDesc->GetMethod(CompilerPropertyMetadataTest::SetterFunctionName);
		if (!this->Assert.IsTrue(PropertyDesc.IsValid(), TEXT("Property callback metadata test case should parse the annotated property descriptor"))
			|| !this->Assert.IsTrue(OnRepDesc.IsValid(), TEXT("Property callback metadata test case should parse the RepNotify callback descriptor"))
			|| !this->Assert.IsTrue(GetterDesc.IsValid(), TEXT("Property callback metadata test case should parse the BlueprintGetter descriptor"))
			|| !this->Assert.IsTrue(SetterDesc.IsValid(), TEXT("Property callback metadata test case should parse the BlueprintSetter descriptor")))
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			PropertyDesc->bReplicated,
			TEXT("Preprocessor should mark ReplicatedUsing properties as replicated")));
		ASSERT_THAT(IsTrue(
			PropertyDesc->bRepNotify,
			TEXT("Preprocessor should mark ReplicatedUsing properties as rep-notify")));
		ASSERT_THAT(AreEqual(
			CompilerPropertyMetadataTest::OnRepFunctionName,
			CompilerPropertyMetadataTest::GetPropertyMeta(PropertyDesc, TEXT("ReplicatedUsing")),
			TEXT("Preprocessor should preserve the ReplicatedUsing callback name")));
		ASSERT_THAT(AreEqual(
			CompilerPropertyMetadataTest::GetterFunctionName,
			CompilerPropertyMetadataTest::GetPropertyMeta(PropertyDesc, TEXT("BlueprintGetter")),
			TEXT("Preprocessor should preserve the BlueprintGetter callback name")));
		ASSERT_THAT(AreEqual(
			CompilerPropertyMetadataTest::SetterFunctionName,
			CompilerPropertyMetadataTest::GetPropertyMeta(PropertyDesc, TEXT("BlueprintSetter")),
			TEXT("Preprocessor should preserve the BlueprintSetter callback name")));
		ASSERT_THAT(IsTrue(
			GetterDesc->bBlueprintPure,
			TEXT("Preprocessor should keep the BlueprintGetter callback marked BlueprintPure")));

		Engine.ResetDiagnostics();

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			CompilerPropertyMetadataTest::ModuleName,
			CompilerPropertyMetadataTest::RelativeScriptPath,
			TestScriptSource,
			true,
			Summary,
			true);

		if (Summary.Diagnostics.Num() > 0)
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("Compile diagnostics: %s"),
				*CompilerPropertyMetadataTest::JoinDiagnostics(Summary.Diagnostics)));
		}

		ASSERT_THAT(IsTrue(
			bCompiled,
			TEXT("Property callback metadata test case should compile through the normal preprocessor pipeline")));
		ASSERT_THAT(IsTrue(
			Summary.bUsedPreprocessor,
			TEXT("Property callback metadata test case should record preprocessor usage in the compile summary")));
		ASSERT_THAT(IsTrue(
			Summary.bCompileSucceeded,
			TEXT("Property callback metadata test case should mark compile succeeded in the summary")));
		ASSERT_THAT(AreEqual(
			0,
			Summary.Diagnostics.Num(),
			TEXT("Property callback metadata test case should keep compile diagnostics empty")));
		if (!bCompiled)
		{
			return;
		}

		int32 EntryResult = 0;
		const bool bExecuted = ExecuteIntFunction(
			&Engine,
			CompilerPropertyMetadataTest::RelativeScriptPath,
			CompilerPropertyMetadataTest::ModuleName,
			CompilerPropertyMetadataTest::EntryFunctionDeclaration,
			EntryResult);
		ASSERT_THAT(IsTrue(
			bExecuted,
			TEXT("Property callback metadata test case should execute the compiled entry function")));
		if (bExecuted)
		{
			ASSERT_THAT(AreEqual(
				CompilerPropertyMetadataTest::ExpectedEntryValue,
				EntryResult,
				TEXT("Property callback metadata test case should preserve module execution after metadata propagation")));
		}

		UClass* GeneratedClass = FindGeneratedClass(&Engine, *CompilerPropertyMetadataTest::ClassName);
		if (!this->Assert.IsNotNull(GeneratedClass, TEXT("Property callback metadata test case should materialize the generated class")))
		{
			return;
		}

		FIntProperty* TrackedValueProperty = FindFProperty<FIntProperty>(GeneratedClass, *CompilerPropertyMetadataTest::PropertyName);
		UFunction* OnRepFunction = FindGeneratedFunction(GeneratedClass, *CompilerPropertyMetadataTest::OnRepFunctionName);
		UFunction* GetterFunction = FindGeneratedFunction(GeneratedClass, *CompilerPropertyMetadataTest::GetterFunctionName);
		UFunction* SetterFunction = FindGeneratedFunction(GeneratedClass, *CompilerPropertyMetadataTest::SetterFunctionName);
		if (!this->Assert.IsNotNull(TrackedValueProperty, TEXT("Property callback metadata test case should materialize the generated property"))
			|| !this->Assert.IsNotNull(OnRepFunction, TEXT("Property callback metadata test case should materialize the generated RepNotify callback"))
			|| !this->Assert.IsNotNull(GetterFunction, TEXT("Property callback metadata test case should materialize the generated BlueprintGetter callback"))
			|| !this->Assert.IsNotNull(SetterFunction, TEXT("Property callback metadata test case should materialize the generated BlueprintSetter callback")))
		{
			return;
		}

		FIntProperty* GetterReturnProperty = CastField<FIntProperty>(GetterFunction->GetReturnProperty());
		FIntProperty* SetterValueProperty = FindFProperty<FIntProperty>(SetterFunction, TEXT("Value"));

		ASSERT_THAT(IsTrue(
			TrackedValueProperty->HasAnyPropertyFlags(CPF_Net),
			TEXT("Generated property should carry CPF_Net")));
		ASSERT_THAT(IsTrue(
			TrackedValueProperty->HasAnyPropertyFlags(CPF_RepNotify),
			TEXT("Generated property should carry CPF_RepNotify")));
		ASSERT_THAT(AreEqual(
			FName(*CompilerPropertyMetadataTest::OnRepFunctionName),
			TrackedValueProperty->RepNotifyFunc,
			TEXT("Generated property should preserve the RepNotify callback name")));
		ASSERT_THAT(AreEqual(
			CompilerPropertyMetadataTest::GetterFunctionName,
			TrackedValueProperty->GetMetaData(TEXT("BlueprintGetter")),
			TEXT("Generated property should preserve BlueprintGetter metadata")));
		ASSERT_THAT(AreEqual(
			CompilerPropertyMetadataTest::SetterFunctionName,
			TrackedValueProperty->GetMetaData(TEXT("BlueprintSetter")),
			TEXT("Generated property should preserve BlueprintSetter metadata")));
		ASSERT_THAT(AreEqual(
			0,
			OnRepFunction->NumParms,
			TEXT("Generated RepNotify callback should not expose parameters")));
		ASSERT_THAT(IsNotNull(
			GetterReturnProperty,
			TEXT("Generated BlueprintGetter callback should return int")));
		ASSERT_THAT(AreEqual(
			1,
			SetterFunction->NumParms,
			TEXT("Generated BlueprintSetter callback should expose exactly one parameter")));
		ASSERT_THAT(IsNotNull(
			SetterValueProperty,
			TEXT("Generated BlueprintSetter callback should expose an int Value parameter")));

		}

	}

	TEST_METHOD(PropertyCallbackSignatureValidationReportsDiagnostics)
	{


		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		const TArray<CompilerPropertyMetadataTest::FPropertyCallbackValidationTestCase> TestCases = {
			{
				TEXT("ReplicatedUsing callback should reject more than one argument"),
				FName(TEXT("Tests.Compiler.PropertyCallbackValidation.RepNotifyTooManyArgs")),
				TEXT("Tests/Compiler/PropertyCallbackValidation/RepNotifyTooManyArgs.as"),
				TEXT(R"AS(
	UCLASS()
	class UPropertyCallbackCarrier : UObject
	{
		UPROPERTY(ReplicatedUsing=OnRep_TrackedValue)
		int TrackedValue;

		UFUNCTION()
		void OnRep_TrackedValue(int OldValue, int NewValue)
		{
		}
	}
	)AS"),
				TEXT("can not have more than 1 argument."),
				9
			},
			{
				TEXT("BlueprintSetter callback should reject a mismatched value type"),
				FName(TEXT("Tests.Compiler.PropertyCallbackValidation.SetterTypeMismatch")),
				TEXT("Tests/Compiler/PropertyCallbackValidation/SetterTypeMismatch.as"),
				TEXT(R"AS(
	UCLASS()
	class UPropertyCallbackCarrier : UObject
	{
		UPROPERTY(BlueprintSetter=SetTrackedValue)
		int TrackedValue;

		UFUNCTION()
		void SetTrackedValue(float Value)
		{
		}
	}
	)AS"),
				TEXT("takes an argument of type 'float', but the value written is of type 'int'."),
				9
			},
			{
				TEXT("BlueprintGetter callback should require BlueprintPure"),
				FName(TEXT("Tests.Compiler.PropertyCallbackValidation.GetterNeedsBlueprintPure")),
				TEXT("Tests/Compiler/PropertyCallbackValidation/GetterNeedsBlueprintPure.as"),
				TEXT(R"AS(
	UCLASS()
	class UPropertyCallbackCarrier : UObject
	{
		UPROPERTY(BlueprintGetter=GetTrackedValue)
		int TrackedValue;

		UFUNCTION()
		int GetTrackedValue() const
		{
			return TrackedValue;
		}
	}
	)AS"),
				TEXT("needs to be marked as BlueprintPure."),
				5
			}
		};

		for (const CompilerPropertyMetadataTest::FPropertyCallbackValidationTestCase& TestCase : TestCases)
		{
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

			if (Summary.Diagnostics.Num() > 0)
			{
				TestRunner->AddInfo(FString::Printf(TEXT("%s diagnostics: %s"), TestCase.Label, *CompilerPropertyMetadataTest::JoinDiagnostics(Summary.Diagnostics)));
			}

			const FAngelscriptCompileTraceDiagnosticSummary* MatchingDiagnostic =
				CompilerPropertyMetadataTest::FindMatchingErrorDiagnostic(Summary.Diagnostics, TestCase.ExpectedMessageFragment);

			ASSERT_THAT(IsFalse(bCompiled, FString::Printf(TEXT("%s should fail compile"), TestCase.Label)));
			ASSERT_THAT(IsFalse(Summary.bCompileSucceeded, FString::Printf(TEXT("%s should keep bCompileSucceeded false"), TestCase.Label)));
			ASSERT_THAT(IsTrue(Summary.bUsedPreprocessor, FString::Printf(TEXT("%s should record preprocessor usage"), TestCase.Label)));
			ASSERT_THAT(IsNotNull(MatchingDiagnostic, FString::Printf(TEXT("%s should emit the expected callback diagnostic"), TestCase.Label)));
			if (MatchingDiagnostic != nullptr)
			{
				ASSERT_THAT(AreEqual(
					TestCase.ExpectedRow,
					MatchingDiagnostic->Row,
					FString::Printf(TEXT("%s should pin the diagnostic row to the callback/property declaration"), TestCase.Label)));
			}

			Engine.DiscardModule(*TestCase.ModuleName.ToString());
		}

		}

	}

};

#endif
