#include "CQTest.h"

#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestEngineAcquisition.h"

#include "EnhancedInputComponent.h"
#include "StaticJIT/StaticJITBinds.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_objecttype.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS && AS_CAN_GENERATE_JIT

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITNativeFormTests,
	"Angelscript.TestModule.StaticJIT.NativeForms",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static constexpr TCHAR SourceFilename[] = TEXT("StaticJITTArrayIndexCustomCall.as");
	inline static const FName ModuleName = FName(TEXT("ASStaticJITTArrayIndexCustomCall"));

	static FString MakeScriptSource()
	{
		return
			TEXT("int ReadMiddle()\n")
			TEXT("{\n")
			TEXT("    TArray<int> Values;\n")
			TEXT("    Values.Add(11);\n")
			TEXT("    Values.Add(22);\n")
			TEXT("    Values.Add(33);\n")
			TEXT("    return Values[1];\n")
			TEXT("}\n")
			TEXT("\n")
			TEXT("int ReadInvalid()\n")
			TEXT("{\n")
			TEXT("    TArray<int> Values;\n")
			TEXT("    Values.Add(11);\n")
			TEXT("    return Values[5];\n")
			TEXT("}\n");
	}

	static asITypeInfo* FindArrayTypeInfo(FAutomationTestBase& Test, asIScriptEngine& ScriptEngine)
	{
		static constexpr const ANSICHAR* CandidateDecls[] =
		{
			"TArray<int>",
			"array<int>",
		};

		for (const ANSICHAR* CandidateDecl : CandidateDecls)
		{
			if (asITypeInfo* TypeInfo = ScriptEngine.GetTypeInfoByDecl(CandidateDecl))
			{
				return TypeInfo;
			}
		}

		Test.AddError(TEXT("StaticJIT TArray index native form test could not resolve the array<int>/TArray<int> script type."));
		return nullptr;
	}

	static bool GeneratedSourceUsesExpectedIndexFastPath(const FString& GeneratedSource)
	{
		return GeneratedSource.Contains(TEXT("FArrayOperations::OpIndex_Template_Unchecked"))
			|| GeneratedSource.Contains(TEXT("FArrayOperations::OpIndex_Stride_Unchecked"))
			|| GeneratedSource.Contains(TEXT("FArrayOperations::OpIndex_Unchecked"));
	}

	static FScriptFunctionNativeForm* FindNativeFormForMethodName(
		FAutomationTestBase& Test,
		asITypeInfo& TypeInfo,
		const ANSICHAR* MethodName,
		FString* OutResolvedDeclaration = nullptr)
	{
		TArray<FString> MatchingDeclarations;

		for (asUINT MethodIndex = 0; MethodIndex < TypeInfo.GetMethodCount(); ++MethodIndex)
		{
			asIScriptFunction* Candidate = TypeInfo.GetMethodByIndex(MethodIndex);
			if (Candidate == nullptr || FCStringAnsi::Strcmp(Candidate->GetName(), MethodName) != 0)
			{
				continue;
			}

			const FString Declaration = UTF8_TO_TCHAR(Candidate->GetDeclaration(true, true, true, true));
			MatchingDeclarations.Add(Declaration);

			if (FScriptFunctionNativeForm* NativeForm = FScriptFunctionNativeForm::GetNativeForm(Candidate))
			{
				if (OutResolvedDeclaration != nullptr)
				{
					*OutResolvedDeclaration = Declaration;
				}
				return NativeForm;
			}
		}

		if (MatchingDeclarations.IsEmpty())
		{
			Test.AddError(FString::Printf(
				TEXT("StaticJIT TArray index native form test could not find any '%hs' overloads on the resolved array type."),
				MethodName));
		}
		else
		{
			Test.AddInfo(FString::Printf(
				TEXT("Resolved '%hs' overloads without native forms: %s"),
				MethodName,
				*FString::Join(MatchingDeclarations, TEXT(" | "))));
		}

		return nullptr;
	}

	static bool RunTArrayIndexCustomCall(FAutomationTestBase& Test);
	static bool RunEnhancedInputGetHandleNoJit(FAutomationTestBase& Test);

public:
	TEST_METHOD(TArrayIndexCustomCall)
	{
		ASSERT_THAT(IsTrue(RunTArrayIndexCustomCall(*TestRunner)));
	}

	TEST_METHOD(EnhancedInputGetHandleNoJit)
	{
		ASSERT_THAT(IsTrue(RunEnhancedInputGetHandleNoJit(*TestRunner)));
	}
};

bool FAngelscriptStaticJITNativeFormTests::RunEnhancedInputGetHandleNoJit(FAutomationTestBase& Test)
{
	FAngelscriptEngineConfig Config;
	Config.bCollectStaticJITCompatibilityBinds = true;

	FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> OwnedEngine = CreateScriptScanFreeFullEngineForTesting(Config, Dependencies);
	if (!Test.TestNotNull(TEXT("StaticJIT.NativeForms.EnhancedInputGetHandleNoJit should create a dedicated engine"), OwnedEngine.Get()))
	{
		return false;
	}

	FAngelscriptEngine& Engine = *OwnedEngine;
	asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
	if (!Test.TestNotNull(TEXT("StaticJIT.NativeForms.EnhancedInputGetHandleNoJit should expose the script engine"), ScriptEngine))
	{
		return false;
	}

	asITypeInfo* TypeInfo = ScriptEngine->GetTypeInfoByDecl("FEnhancedInputActionEventBinding");
	if (!Test.TestNotNull(TEXT("StaticJIT.NativeForms.EnhancedInputGetHandleNoJit should resolve FEnhancedInputActionEventBinding"), TypeInfo))
	{
		return false;
	}

	bool bFoundHandle = false;
	for (asUINT MethodIndex = 0; MethodIndex < TypeInfo->GetMethodCount(); ++MethodIndex)
	{
		asIScriptFunction* Candidate = TypeInfo->GetMethodByIndex(MethodIndex);
		if (Candidate != nullptr && FCStringAnsi::Strcmp(Candidate->GetName(), "GetHandle") == 0)
		{
			bFoundHandle = true;
			break;
		}
	}

	if (!Test.TestTrue(TEXT("StaticJIT.NativeForms.EnhancedInputGetHandleNoJit should expose GetHandle"), bFoundHandle))
	{
		return false;
	}

	FString ResolvedDeclaration;
	FScriptFunctionNativeForm* NativeForm = FindNativeFormForMethodName(
		Test,
		*TypeInfo,
		"GetHandle",
		&ResolvedDeclaration);
	return Test.TestNull(
		TEXT("StaticJIT.NativeForms.EnhancedInputGetHandleNoJit should not create a trivial native form"),
		NativeForm);
}

bool FAngelscriptStaticJITNativeFormTests::RunTArrayIndexCustomCall(FAutomationTestBase& Test)
{
	bool bPassed = false;
	FAngelscriptEngineConfig Config;
	Config.bCollectStaticJITCompatibilityBinds = true;

	FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> OwnedEngine = CreateScriptScanFreeFullEngineForTesting(Config, Dependencies);
	if (!Test.TestNotNull(
		TEXT("StaticJIT.NativeForms.TArrayIndexCustomCall should create a dedicated engine with precompiled-data generation enabled"),
		OwnedEngine.Get()))
	{
		return false;
	}

	FAngelscriptEngine& Engine = *OwnedEngine;

	do
	{
		const bool bCompiled = CompileModuleFromMemory(
			&Engine,
			ModuleName,
			SourceFilename,
			MakeScriptSource());
		if (!Test.TestTrue(
			TEXT("StaticJIT.NativeForms.TArrayIndexCustomCall should compile the fixture module"),
			bCompiled))
		{
			break;
		}

		int32 ReadMiddleResult = 0;
		const bool bExecutedReadMiddle = ExecuteIntFunction(
			&Engine,
			ModuleName,
			TEXT("int ReadMiddle()"),
			ReadMiddleResult);
		if (!Test.TestTrue(
			TEXT("StaticJIT.NativeForms.TArrayIndexCustomCall should keep ReadMiddle executable through the normal script runtime"),
			bExecutedReadMiddle))
		{
			break;
		}

		if (!Test.TestEqual(
			TEXT("StaticJIT.NativeForms.TArrayIndexCustomCall should keep ReadMiddle returning the indexed middle element"),
			ReadMiddleResult,
			22))
		{
			break;
		}

		asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
		if (!Test.TestNotNull(
			TEXT("StaticJIT.NativeForms.TArrayIndexCustomCall should have a live script engine"),
			ScriptEngine))
		{
			break;
		}

		asITypeInfo* ArrayTypeInfo = FindArrayTypeInfo(Test, *ScriptEngine);
		if (!Test.TestNotNull(
			TEXT("StaticJIT.NativeForms.TArrayIndexCustomCall should resolve the bound array<int> type"),
			ArrayTypeInfo))
		{
			break;
		}

		FString ResolvedOpIndexDeclaration;
		FScriptFunctionNativeForm* NativeForm = FindNativeFormForMethodName(
			Test,
			*ArrayTypeInfo,
			"opIndex",
			&ResolvedOpIndexDeclaration);
		if (!Test.TestNotNull(
			TEXT("StaticJIT.NativeForms.TArrayIndexCustomCall should resolve a native form for array<int>.opIndex"),
			NativeForm))
		{
			break;
		}

		Test.AddInfo(FString::Printf(
			TEXT("Resolved array<int>.opIndex native form overload: %s"),
			*ResolvedOpIndexDeclaration));

		FString GeneratedSource;
		FString GenerateError;
		const bool bGenerated = GenerateStaticJITSourceText(
			&Engine,
			ModuleName,
			GeneratedSource,
			/*bEmitDebugMetadata=*/false,
			&GenerateError);
		if (!Test.TestTrue(
			TEXT("StaticJIT.NativeForms.TArrayIndexCustomCall should generate StaticJIT source text for the compiled module"),
			bGenerated))
		{
			if (!GenerateError.IsEmpty())
			{
				Test.AddError(GenerateError);
			}
			break;
		}

		if (!Test.TestTrue(
			TEXT("StaticJIT.NativeForms.TArrayIndexCustomCall should emit FArrayOperations::IsValidIndex in the generated custom call"),
			GeneratedSource.Contains(TEXT("FArrayOperations::IsValidIndex"))))
		{
			break;
		}

		if (!Test.TestTrue(
			TEXT("StaticJIT.NativeForms.TArrayIndexCustomCall should emit ThrowOutOfBounds in the generated custom call"),
			GeneratedSource.Contains(TEXT("FArrayOperations::ThrowOutOfBounds();"))))
		{
			break;
		}

		if (!Test.TestTrue(
			TEXT("StaticJIT.NativeForms.TArrayIndexCustomCall should emit one of the unchecked opIndex fast paths instead of a generic call"),
			GeneratedSourceUsesExpectedIndexFastPath(GeneratedSource)))
		{
			break;
		}

		bPassed = true;
	}
	while (false);

	return bPassed;
}

#endif
