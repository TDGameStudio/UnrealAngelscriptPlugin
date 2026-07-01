#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "Preprocessor/AngelscriptPreprocessor.h"

#include "HAL/FileManager.h"
#include "CQTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace CompilerClassHierarchyTest
{
	static const FString BaseRelativeScriptPath(TEXT("Tests/Compiler/ClassHierarchy/Base.as"));
	static const FString ChildRelativeScriptPath(TEXT("Tests/Compiler/ClassHierarchy/Child.as"));
	static const FName BaseModuleName(TEXT("Tests.Compiler.ClassHierarchy.Base"));
	static const FName ChildModuleName(TEXT("Tests.Compiler.ClassHierarchy.Child"));
	static const FName BaseClassName(TEXT("UHierarchyBase"));
	static const FName ChildClassName(TEXT("UHierarchyChild"));
	static const FName DerivedFunctionName(TEXT("GetDerivedValue"));

	FString GetFixtureRoot()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("CompilerClassHierarchyFixtures"));
	}

	FString WriteFixture(const FString& InRelativeScriptPath, const FString& Contents)
	{
		const FString AbsolutePath = FPaths::Combine(GetFixtureRoot(), InRelativeScriptPath);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(AbsolutePath), true);
		FFileHelper::SaveStringToFile(Contents, *AbsolutePath);
		return AbsolutePath;
	}

	FAngelscriptModuleDesc* FindModuleByName(
		const TArray<TSharedRef<FAngelscriptModuleDesc>>& Modules,
		const FString& InModuleName)
	{
		for (const TSharedRef<FAngelscriptModuleDesc>& Module : Modules)
		{
			if (Module->ModuleName == InModuleName)
			{
				return &Module.Get();
			}
		}

		return nullptr;
	}

	TArray<FString> CollectDiagnosticMessages(
		FAngelscriptEngine& Engine,
		const TArray<FString>& AbsoluteFilenames,
		int32& OutErrorCount)
	{
		TArray<FString> Messages;
		OutErrorCount = 0;

		for (const FString& AbsoluteFilename : AbsoluteFilenames)
		{
			const FAngelscriptEngine::FDiagnostics* Diagnostics = Engine.Diagnostics.Find(AbsoluteFilename);
			if (Diagnostics == nullptr)
			{
				continue;
			}

			for (const FAngelscriptEngine::FDiagnostic& Diagnostic : Diagnostics->Diagnostics)
			{
				Messages.Add(Diagnostic.Message);
				if (Diagnostic.bIsError)
				{
					++OutErrorCount;
				}
			}
		}

		return Messages;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCompilerClassHierarchyTests,
	"Angelscript.TestModule.Compiler.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ScriptSuperclassRoundTrip)
	{


		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		const FString BaseAbsoluteScriptPath = CompilerClassHierarchyTest::WriteFixture(
			CompilerClassHierarchyTest::BaseRelativeScriptPath,
			TEXT(
				"UCLASS()\n"
				"class UHierarchyBase : UObject\n"
				"{\n"
				"    UFUNCTION()\n"
				"    int GetBaseValue()\n"
				"    {\n"
				"        return 7;\n"
				"    }\n"
				"}\n"));
		const FString ChildAbsoluteScriptPath = CompilerClassHierarchyTest::WriteFixture(
			CompilerClassHierarchyTest::ChildRelativeScriptPath,
			TEXT(
				"UCLASS()\n"
				"class UHierarchyChild : UHierarchyBase\n"
				"{\n"
				"    UFUNCTION()\n"
				"    int GetDerivedValue()\n"
				"    {\n"
				"        return GetBaseValue();\n"
				"    }\n"
				"}\n"));

		ON_SCOPE_EXIT
		{
			IFileManager::Get().Delete(*BaseAbsoluteScriptPath, false, true);
			IFileManager::Get().Delete(*ChildAbsoluteScriptPath, false, true);
			Engine.DiscardModule(*CompilerClassHierarchyTest::BaseModuleName.ToString());
			Engine.DiscardModule(*CompilerClassHierarchyTest::ChildModuleName.ToString());
		};

		Engine.ResetDiagnostics();
		Engine.LastEmittedDiagnostics.Empty();

		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddFile(CompilerClassHierarchyTest::BaseRelativeScriptPath, BaseAbsoluteScriptPath);
		Preprocessor.AddFile(CompilerClassHierarchyTest::ChildRelativeScriptPath, ChildAbsoluteScriptPath);

		const bool bPreprocessSucceeded = Preprocessor.Preprocess();
		TArray<TSharedRef<FAngelscriptModuleDesc>> ModulesToCompile = Preprocessor.GetModulesToCompile();

		int32 PreprocessErrorCount = 0;
		const FString PreprocessDiagnostics = FString::Join(
			CompilerClassHierarchyTest::CollectDiagnosticMessages(
				Engine,
				{BaseAbsoluteScriptPath, ChildAbsoluteScriptPath},
				PreprocessErrorCount),
			TEXT("\n"));

		ASSERT_THAT(IsTrue(
			bPreprocessSucceeded,
			TEXT("Script superclass round-trip should preprocess successfully")));
		ASSERT_THAT(AreEqual(
			0,
			PreprocessErrorCount,
			TEXT("Script superclass round-trip should keep preprocessing diagnostics empty")));
		ASSERT_THAT(IsTrue(
			PreprocessDiagnostics.IsEmpty(),
			TEXT("Script superclass round-trip should not accumulate preprocessing messages")));
		ASSERT_THAT(AreEqual(
			2,
			ModulesToCompile.Num(),
			TEXT("Script superclass round-trip should emit two module descriptors")));
		if (!bPreprocessSucceeded || ModulesToCompile.Num() != 2)
		{
			return;
		}

		FAngelscriptModuleDesc* BaseModuleDesc = CompilerClassHierarchyTest::FindModuleByName(
			ModulesToCompile,
			CompilerClassHierarchyTest::BaseModuleName.ToString());
		FAngelscriptModuleDesc* ChildModuleDesc = CompilerClassHierarchyTest::FindModuleByName(
			ModulesToCompile,
			CompilerClassHierarchyTest::ChildModuleName.ToString());
		if (!this->Assert.IsNotNull(BaseModuleDesc, TEXT("Script superclass round-trip should emit the base module descriptor"))
			|| !this->Assert.IsNotNull(ChildModuleDesc, TEXT("Script superclass round-trip should emit the child module descriptor")))
		{
			return;
		}

		TSharedPtr<FAngelscriptClassDesc> ChildClassDesc = ChildModuleDesc->GetClass(CompilerClassHierarchyTest::ChildClassName.ToString());
		if (!this->Assert.IsNotNull(ChildClassDesc.Get(), TEXT("Script superclass round-trip should preserve the child class descriptor after preprocessing")))
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			CompilerClassHierarchyTest::BaseClassName.ToString(),
			ChildClassDesc->SuperClass,
			TEXT("Script superclass round-trip should keep the child descriptor super class text stable")));

		Engine.ResetDiagnostics();

		TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
		const ECompileResult CompileResult = Engine.CompileModules(
			ECompileType::FullReload,
			ModulesToCompile,
			CompiledModules);

		int32 CompileErrorCount = 0;
		const FString CompileDiagnostics = FString::Join(
			CompilerClassHierarchyTest::CollectDiagnosticMessages(
				Engine,
				{BaseAbsoluteScriptPath, ChildAbsoluteScriptPath},
				CompileErrorCount),
			TEXT("\n"));

		ASSERT_THAT(AreEqual(
			ECompileResult::FullyHandled,
			CompileResult,
			TEXT("Script superclass round-trip should compile as FullyHandled")));
		ASSERT_THAT(AreEqual(
			0,
			CompileErrorCount,
			TEXT("Script superclass round-trip should keep compile diagnostics empty")));
		ASSERT_THAT(IsTrue(
			CompileDiagnostics.IsEmpty(),
			TEXT("Script superclass round-trip should not accumulate compile messages")));
		ASSERT_THAT(AreEqual(
			2,
			CompiledModules.Num(),
			TEXT("Script superclass round-trip should materialize exactly two compiled modules")));
		if (CompileResult != ECompileResult::FullyHandled || CompiledModules.Num() != 2)
		{
			return;
		}

		UClass* GeneratedBaseClass = FindGeneratedClass(&Engine, CompilerClassHierarchyTest::BaseClassName);
		UClass* GeneratedChildClass = FindGeneratedClass(&Engine, CompilerClassHierarchyTest::ChildClassName);
		if (!this->Assert.IsNotNull(GeneratedBaseClass, TEXT("Script superclass round-trip should generate the base class"))
			|| !this->Assert.IsNotNull(GeneratedChildClass, TEXT("Script superclass round-trip should generate the child class")))
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			GeneratedChildClass->GetSuperClass() == GeneratedBaseClass,
			TEXT("Script superclass round-trip should keep the generated child super chain pointing at the generated base")));

		UFunction* DerivedFunction = FindGeneratedFunction(GeneratedChildClass, CompilerClassHierarchyTest::DerivedFunctionName);
		if (!this->Assert.IsNotNull(DerivedFunction, TEXT("Script superclass round-trip should expose GetDerivedValue on the generated child class")))
		{
			return;
		}

		UObject* RuntimeObject = GeneratedChildClass->GetDefaultObject();
		if (!this->Assert.IsNotNull(RuntimeObject, TEXT("Script superclass round-trip should materialize the child default object")))
		{
			return;
		}

		int32 Result = 0;
		const bool bExecuted = ExecuteGeneratedIntEventOnGameThread(&Engine, RuntimeObject, DerivedFunction, Result);
		ASSERT_THAT(IsTrue(
			bExecuted,
			TEXT("Script superclass round-trip should execute the generated child method")));
		if (bExecuted)
		{
			ASSERT_THAT(AreEqual(
				7,
				Result,
				TEXT("Script superclass round-trip should let the child method call through to the scripted base implementation")));
		}

		}

	}

};

#endif
