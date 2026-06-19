#include "AngelscriptDocs.h"
#include "AngelscriptEngine.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Containers/StringConv.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_Core_AngelscriptDocsTests_Private
{
	FString MakeAutomationDocsSuffix()
	{
		return FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(12);
	}

	FString MakeDocsScriptSource(const FString& TypeName)
	{
		return FString::Printf(
			TEXT(R"ANGELSCRIPT(
class %s
{
	int EvaluateScore(int InValue) const
	{
		return InValue + 7;
	}
}
)ANGELSCRIPT"),
			*TypeName);
	}

	asITypeInfo* FindTypeInfoByDecl(
		FAutomationTestBase& Test,
		asIScriptModule& Module,
		const FString& Declaration)
	{
		FNoDiscardAsserter Assert(Test);
		const FTCHARToUTF8 DeclarationUtf8(*Declaration);
		asITypeInfo* TypeInfo = Module.GetTypeInfoByDecl(DeclarationUtf8.Get());
		if (!Assert.IsNotNull(
				TypeInfo,
				*FString::Printf(TEXT("Docs normalization test should resolve script type '%s'"), *Declaration)))
		{
			return nullptr;
		}
		return TypeInfo;
	}

	asIScriptFunction* FindMethodByDecl(
		FAutomationTestBase& Test,
		asITypeInfo& ScriptType,
		const FString& Declaration)
	{
		FNoDiscardAsserter Assert(Test);
		const FTCHARToUTF8 DeclarationUtf8(*Declaration);
		asIScriptFunction* Function = ScriptType.GetMethodByDecl(DeclarationUtf8.Get());
		if (Function == nullptr)
		{
			FString MethodName;
			int32 OpenParenIndex = INDEX_NONE;
			if (Declaration.FindChar(TEXT('('), OpenParenIndex))
			{
				const FString Prefix = Declaration.Left(OpenParenIndex).TrimStartAndEnd();
				int32 NameSeparatorIndex = INDEX_NONE;
				if (Prefix.FindLastChar(TEXT(' '), NameSeparatorIndex))
				{
					MethodName = Prefix.Mid(NameSeparatorIndex + 1).TrimStartAndEnd();
				}
			}

			if (!MethodName.IsEmpty())
			{
				const FTCHARToUTF8 MethodNameUtf8(*MethodName);
				const asUINT MethodCount = ScriptType.GetMethodCount();
				for (asUINT MethodIndex = 0; MethodIndex < MethodCount; ++MethodIndex)
				{
					asIScriptFunction* CandidateFunction = ScriptType.GetMethodByIndex(MethodIndex);
					if (CandidateFunction != nullptr && FCStringAnsi::Strcmp(CandidateFunction->GetName(), MethodNameUtf8.Get()) == 0)
					{
						Function = CandidateFunction;
						break;
					}
				}
			}
		}

		if (Function == nullptr)
		{
			FString AvailableMethods;
			const asUINT MethodCount = ScriptType.GetMethodCount();
			for (asUINT MethodIndex = 0; MethodIndex < MethodCount; ++MethodIndex)
			{
				asIScriptFunction* CandidateFunction = ScriptType.GetMethodByIndex(MethodIndex);
				if (CandidateFunction == nullptr)
				{
					continue;
				}

				if (!AvailableMethods.IsEmpty())
				{
					AvailableMethods += TEXT(", ");
				}

				AvailableMethods += UTF8_TO_TCHAR(CandidateFunction->GetDeclaration());
			}

			if (AvailableMethods.IsEmpty())
			{
				Test.AddError(FString::Printf(
					TEXT("Docs normalization test should resolve method '%s'; script type exposes no methods"),
					*Declaration));
			}
			else
			{
				Test.AddError(FString::Printf(
					TEXT("Docs normalization test should resolve method '%s'; available methods: %s"),
					*Declaration,
					*AvailableMethods));
			}
		}

		if (!Assert.IsNotNull(
				Function,
				*FString::Printf(TEXT("Docs normalization test should resolve method '%s'"), *Declaration)))
		{
			return nullptr;
		}
		return Function;
	}

	FString GetGeneratedDocsRootDir()
	{
		return FPaths::Combine(FPaths::ProjectDir(), TEXT("Docs"), TEXT("angelscript"), TEXT("generated"));
	}

	FString GetGeneratedDocsParentDir()
	{
		return FPaths::Combine(FPaths::ProjectDir(), TEXT("Docs"), TEXT("angelscript"));
	}

	FString GetGeneratedDocsFilePath(const FString& TypeName)
	{
		return FPaths::Combine(GetGeneratedDocsRootDir(), TypeName + TEXT(".hpp"));
	}

	struct FGeneratedDocsOutputGuard
	{
		explicit FGeneratedDocsOutputGuard(const FString& InTypeName)
			: RootDir(GetGeneratedDocsRootDir())
			, ParentDir(GetGeneratedDocsParentDir())
			, FilePath(GetGeneratedDocsFilePath(InTypeName))
			, bRootDirExisted(IFileManager::Get().DirectoryExists(*RootDir))
			, bParentDirExisted(IFileManager::Get().DirectoryExists(*ParentDir))
		{
		}

		bool Prepare(FAutomationTestBase& Test) const
		{
			FNoDiscardAsserter Assert(Test);
			return Assert.IsTrue(
				IFileManager::Get().MakeDirectory(*RootDir, true),
				TEXT("Docs normalization test should create the generated docs directory"));
		}

		void Cleanup() const
		{
			IFileManager::Get().Delete(*FilePath, false, true, true);

			if (!bRootDirExisted)
			{
				IFileManager::Get().DeleteDirectory(*RootDir, false, true);
			}

			if (!bParentDirExisted)
			{
				IFileManager::Get().DeleteDirectory(*ParentDir, false, false);
			}
		}

		FString RootDir;
		FString ParentDir;
		FString FilePath;
		bool bRootDirExisted = false;
		bool bParentDirExisted = false;
	};

	bool LoadGeneratedDocsFile(
		FAutomationTestBase& Test,
		const FString& FilePath,
		FString& OutContent)
	{
		FNoDiscardAsserter Assert(Test);
		OutContent.Reset();

		const bool bExists = IFileManager::Get().FileExists(*FilePath);
		if (!Assert.IsTrue(
				bExists,
				*FString::Printf(TEXT("Docs normalization test should create generated file '%s'"), *FilePath)))
		{
			TArray<FString> GeneratedFilenames;
			IFileManager::Get().FindFiles(
				GeneratedFilenames,
				*(GetGeneratedDocsRootDir() / TEXT("*.hpp")),
				true,
				false);

			GeneratedFilenames.Sort();
			const FString AvailableFiles = GeneratedFilenames.Num() > 0
				? FString::Join(GeneratedFilenames, TEXT(", "))
				: TEXT("<none>");
			Test.AddError(FString::Printf(
				TEXT("Docs normalization test missing expected file '%s'; generated dir '%s' currently contains %d hpp files: %s"),
				*FilePath,
				*GetGeneratedDocsRootDir(),
				GeneratedFilenames.Num(),
				*AvailableFiles));
			return false;
		}

		const bool bLoaded = FFileHelper::LoadFileToString(OutContent, *FilePath);
		return Assert.IsTrue(
			bLoaded,
			*FString::Printf(TEXT("Docs normalization test should load generated file '%s'"), *FilePath));
	}
}


TEST_CLASS_WITH_FLAGS(FAngelscriptDocsTests,
	"Angelscript.TestModule.Engine.Docs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(DumpDocumentationNormalizesSeeNoteAndReturnsAliases)
	{
		using namespace AngelscriptTest_Core_AngelscriptDocsTests_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		{
			FAngelscriptEngineScope _AutoEngineScope(Engine);
			ON_SCOPE_EXIT
			{
				const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
				for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
				{
					Engine.DiscardModule(*_Module->ModuleName);
				}
			};

		const FString UniqueSuffix = MakeAutomationDocsSuffix();
		const FString TypeName = FString::Printf(TEXT("FAutomationDocs_%s"), *UniqueSuffix);
		const FName ModuleName(*FString::Printf(TEXT("Automation.Docs.%s"), *UniqueSuffix));
		const FString ScriptFilename = FString::Printf(TEXT("Docs/%s.as"), *TypeName);
		const FString ScriptSource = MakeDocsScriptSource(TypeName);
		const FString GeneratedFilePath = GetGeneratedDocsFilePath(TypeName);
		const FGeneratedDocsOutputGuard OutputGuard(TypeName);
		int32 FunctionId = INDEX_NONE;

		ON_SCOPE_EXIT
		{
			if (FunctionId != INDEX_NONE)
			{
				FAngelscriptDocs::AddUnrealDocumentation(FunctionId, TEXT(""), TEXT(""), nullptr);
			}

			Engine.DiscardModule(*ModuleName.ToString());
			OutputGuard.Cleanup();
		};

		if (!OutputGuard.Prepare(*TestRunner))
		{
			return;
		}

		const bool bCompiled = CompileModuleFromMemory(
			&Engine,
			ModuleName,
			ScriptFilename,
			ScriptSource);
		ASSERT_THAT(IsTrue(bCompiled, TEXT("Docs normalization test should compile the automation docs module")));

		const TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByModuleName(ModuleName.ToString());
		ASSERT_THAT(IsTrue(ModuleDesc.IsValid(), TEXT("Docs normalization test should register the module by name")));

		ASSERT_THAT(IsNotNull(ModuleDesc->ScriptModule, TEXT("Docs normalization test should expose the compiled script module")));

		asITypeInfo* ScriptType = FindTypeInfoByDecl(*TestRunner, *ModuleDesc->ScriptModule, TypeName);
		if (ScriptType == nullptr)
		{
			return;
		}

		asIScriptFunction* EvaluateScore = FindMethodByDecl(*TestRunner, *ScriptType, TEXT("int EvaluateScore(int) const"));
		if (EvaluateScore == nullptr)
		{
			return;
		}

		FunctionId = EvaluateScore->GetId();
		const FString FunctionTooltip = TEXT(
			"Evaluates the score.\n"
			"@see RelatedScoreType\n"
			"@note Keep integer only\n"
			"@param InValue - first line\n"
			"  second line continues\n"
			"@returns final computed score");

		FAngelscriptDocs::AddUnrealDocumentation(FunctionId, FunctionTooltip, TEXT(""), nullptr);
		FAngelscriptDocs::DumpDocumentation(Engine.GetScriptEngine());

		FString GeneratedContent;
		if (!LoadGeneratedDocsFile(*TestRunner, GeneratedFilePath, GeneratedContent))
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			GeneratedContent.Contains(FString::Printf(TEXT("class %s"), *TypeName)),
			TEXT("Docs normalization test should emit the generated class declaration")));
		ASSERT_THAT(IsTrue(
			GeneratedContent.Contains(TEXT("See: RelatedScoreType")),
			TEXT("Docs normalization test should normalize @see into a See section")));
		ASSERT_THAT(IsTrue(
			GeneratedContent.Contains(TEXT("Note: Keep integer only")),
			TEXT("Docs normalization test should normalize @note into a Note section")));
		ASSERT_THAT(IsTrue(
			GeneratedContent.Contains(TEXT("Parameters:")),
			TEXT("Docs normalization test should emit a Parameters section")));
		ASSERT_THAT(IsTrue(
			GeneratedContent.Contains(TEXT("InValue - first line second line continues")),
			TEXT("Docs normalization test should fold multi-line parameter text into one readable line")));
		ASSERT_THAT(IsTrue(
			GeneratedContent.Contains(TEXT("Returns:")),
			TEXT("Docs normalization test should emit a Returns section for @returns")));
		ASSERT_THAT(IsTrue(
			GeneratedContent.Contains(TEXT("final computed score")),
			TEXT("Docs normalization test should preserve the @returns description text")));
		ASSERT_THAT(IsFalse(
			GeneratedContent.Contains(TEXT("@see")),
			TEXT("Docs normalization test should not leak raw @see tags into generated output")));
		ASSERT_THAT(IsFalse(
			GeneratedContent.Contains(TEXT("@note")),
			TEXT("Docs normalization test should not leak raw @note tags into generated output")));
		ASSERT_THAT(IsFalse(
			GeneratedContent.Contains(TEXT("@returns")),
			TEXT("Docs normalization test should not leak raw @returns tags into generated output")));

		}
	}
};

#endif
