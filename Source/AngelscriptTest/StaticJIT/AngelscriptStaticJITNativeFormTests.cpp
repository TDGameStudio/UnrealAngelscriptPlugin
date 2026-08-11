#include "CQTest.h"

#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestEngineAcquisition.h"

#include "AngelscriptBinds.h"
#include "AngelscriptType.h"
#include "Binds/Helper_FunctionSignature.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/BlueprintPathsLibrary.h"
#include "StaticJIT/StaticJITBinds.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_callfunc.h"
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

	TEST_METHOD(ReflectedBlueprintCallableRetainsExactUFunctionGenericRoute)
	{
		FAngelscriptEngineConfig Config;
		Config.bCollectStaticJITCompatibilityBinds = true;

		FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> OwnedEngine = CreateScriptScanFreeFullEngineForTesting(Config, Dependencies);
		ASSERT_THAT(IsNotNull(OwnedEngine.Get(),
			TEXT("StaticJIT.NativeForms.ReflectedBlueprintCallable should create a dedicated engine")));

		FAngelscriptEngine& Engine = *OwnedEngine;
		FAngelscriptEngineScope EngineScope(Engine);

		UClass* LibraryClass = UBlueprintPathsLibrary::StaticClass();
		UFunction* BaseFilenameFunction = LibraryClass->FindFunctionByName(TEXT("GetBaseFilename"));
		ASSERT_THAT(IsNotNull(BaseFilenameFunction,
			TEXT("StaticJIT.NativeForms.ReflectedBlueprintCallable should resolve GetBaseFilename")));

		FAngelscriptBindState* BindState = Engine.GetBindState();
		ASSERT_THAT(IsNotNull(BindState,
			TEXT("StaticJIT.NativeForms.ReflectedBlueprintCallable should expose engine-local bind state")));

		const TMap<FString, FAngelscriptFunctionBinding>* LibraryBindings =
			BindState->ClassFunctionBindings.Find(LibraryClass);
		ASSERT_THAT(IsNotNull(LibraryBindings,
			TEXT("StaticJIT.NativeForms.ReflectedBlueprintCallable should retain BlueprintPathsLibrary bindings")));

		const FAngelscriptFunctionBinding* FunctionBinding = LibraryBindings->Find(TEXT("GetBaseFilename"));
		ASSERT_THAT(IsNotNull(FunctionBinding,
			TEXT("StaticJIT.NativeForms.ReflectedBlueprintCallable should retain the exact GetBaseFilename binding")));
		ASSERT_THAT(IsTrue(FunctionBinding->bReflectiveFallbackBound,
			TEXT("StaticJIT.NativeForms.ReflectedBlueprintCallable should remain on reflective fallback")));
		ASSERT_THAT(IsTrue(FunctionBinding->bUsesGenericCall,
			TEXT("StaticJIT.NativeForms.ReflectedBlueprintCallable should record the generic call route")));
		ASSERT_THAT(AreEqual(
			EAngelscriptFunctionBindingOrigin::Reflective,
			FunctionBinding->Origin,
			TEXT("StaticJIT.NativeForms.ReflectedBlueprintCallable should retain reflective provenance")));
		ASSERT_THAT(IsNotNull(FunctionBinding->UserData,
			TEXT("StaticJIT.NativeForms.ReflectedBlueprintCallable should retain reflective signature userdata")));

		FAngelscriptTypeDatabase* TypeDatabase = Engine.GetTypeDatabase();
		ASSERT_THAT(IsNotNull(TypeDatabase,
			TEXT("StaticJIT.NativeForms.ReflectedBlueprintCallable should expose the engine-local type database")));
		const TSharedRef<FAngelscriptType>* LibraryType = TypeDatabase->TypesByClass.Find(LibraryClass);
		ASSERT_THAT(IsNotNull(LibraryType,
			TEXT("StaticJIT.NativeForms.ReflectedBlueprintCallable should resolve the engine-local library script type")));

		FAngelscriptFunctionSignature Signature(
			*TypeDatabase,
			*LibraryType,
			BaseFilenameFunction);
		ASSERT_THAT(IsTrue(Signature.bAllTypesValid,
			TEXT("StaticJIT.NativeForms.ReflectedBlueprintCallable should retain a valid exact signature")));
		ASSERT_THAT(IsTrue(Signature.bStaticInScript,
			TEXT("StaticJIT.NativeForms.ReflectedBlueprintCallable should remain namespaced in script")));

		asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("StaticJIT.NativeForms.ReflectedBlueprintCallable should expose the script engine")));

		asIScriptFunction* ScriptFunction = nullptr;
		const FTCHARToUTF8 ExpectedNamespace(*Signature.ClassName);
		const FTCHARToUTF8 ExpectedScriptName(*Signature.ScriptName);
		for (asUINT FunctionIndex = 0; FunctionIndex < ScriptEngine->GetGlobalFunctionCount(); ++FunctionIndex)
		{
			asIScriptFunction* Candidate = ScriptEngine->GetGlobalFunctionByIndex(FunctionIndex);
			if (Candidate == nullptr)
			{
				continue;
			}

			const char* CandidateNamespace = Candidate->GetNamespace();
			const char* CandidateName = Candidate->GetName();
			if (FCStringAnsi::Strcmp(CandidateNamespace != nullptr ? CandidateNamespace : "", ExpectedNamespace.Get()) == 0
				&& FCStringAnsi::Strcmp(CandidateName != nullptr ? CandidateName : "", ExpectedScriptName.Get()) == 0
				&& Candidate->GetUserData() == FunctionBinding->UserData)
			{
				ScriptFunction = Candidate;
				break;
			}
		}
		ASSERT_THAT(IsNotNull(ScriptFunction,
			TEXT("StaticJIT.NativeForms.ReflectedBlueprintCallable should resolve the namespaced primary by exact userdata")));
		ASSERT_THAT(IsTrue(ScriptFunction->GetUserData() == FunctionBinding->UserData,
			TEXT("StaticJIT.NativeForms.ReflectedBlueprintCallable should preserve exact userdata identity")));

		auto* InternalFunction = static_cast<asCScriptFunction*>(ScriptFunction);
		ASSERT_THAT(IsNotNull(InternalFunction->sysFuncIntf,
			TEXT("StaticJIT.NativeForms.ReflectedBlueprintCallable should remain a registered system function")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(ICC_GENERIC_FUNC),
			static_cast<int32>(InternalFunction->sysFuncIntf->callConv),
			TEXT("StaticJIT.NativeForms.ReflectedBlueprintCallable should retain generic function callconv")));

		FScriptFunctionNativeForm* NativeForm = FScriptFunctionNativeForm::GetNativeForm(ScriptFunction);
		ASSERT_THAT(IsNotNull(NativeForm,
			TEXT("StaticJIT.NativeForms.ReflectedBlueprintCallable should retain its native form")));

		const FAngelscriptNativeFormDebugInfo NativeFormInfo = NativeForm->GetDebugInfoForTesting();
		ASSERT_THAT(AreEqual(
			EAngelscriptNativeFormKind::UFunction,
			NativeFormInfo.Kind,
			TEXT("StaticJIT.NativeForms.ReflectedBlueprintCallable should retain UFunction native-form kind")));
		ASSERT_THAT(IsTrue(NativeFormInfo.UnrealFunction == BaseFilenameFunction,
			TEXT("StaticJIT.NativeForms.ReflectedBlueprintCallable should retain the exact UFunction")));
		ASSERT_THAT(AreEqual(
			BaseFilenameFunction->GetName(),
			NativeFormInfo.Name,
			TEXT("StaticJIT.NativeForms.ReflectedBlueprintCallable should retain the exact native name")));
		ASSERT_THAT(IsFalse(NativeFormInfo.bTrivial,
			TEXT("StaticJIT.NativeForms.ReflectedBlueprintCallable should remain nontrivial")));
	}

	TEST_METHOD(NetUFunctionRetainsReflectiveGenericMethodRoute)
	{
		FAngelscriptEngineConfig Config;
		Config.bCollectStaticJITCompatibilityBinds = true;

		FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> OwnedEngine = CreateScriptScanFreeFullEngineForTesting(Config, Dependencies);
		ASSERT_THAT(IsNotNull(OwnedEngine.Get(),
			TEXT("StaticJIT.NativeForms.NetUFunction should create a dedicated engine")));

		FAngelscriptEngine& Engine = *OwnedEngine;
		FAngelscriptEngineScope EngineScope(Engine);

		UClass* ControllerClass = APlayerController::StaticClass();
		UFunction* ClientSetHUDFunction = ControllerClass->FindFunctionByName(TEXT("ClientSetHUD"));
		ASSERT_THAT(IsNotNull(ClientSetHUDFunction,
			TEXT("StaticJIT.NativeForms.NetUFunction should resolve APlayerController.ClientSetHUD")));
		ASSERT_THAT(IsTrue(ClientSetHUDFunction->HasAnyFunctionFlags(FUNC_Net),
			TEXT("StaticJIT.NativeForms.NetUFunction fixture must remain an RPC")));
		ASSERT_THAT(IsTrue(ClientSetHUDFunction->HasAnyFunctionFlags(FUNC_NetClient),
			TEXT("StaticJIT.NativeForms.NetUFunction fixture must remain a client RPC")));
		ASSERT_THAT(IsTrue(ClientSetHUDFunction->HasAnyFunctionFlags(FUNC_NetReliable),
			TEXT("StaticJIT.NativeForms.NetUFunction fixture must remain reliable")));

		FAngelscriptBindState* BindState = Engine.GetBindState();
		ASSERT_THAT(IsNotNull(BindState,
			TEXT("StaticJIT.NativeForms.NetUFunction should expose engine-local bind state")));

		const TMap<FString, FAngelscriptFunctionBinding>* ControllerBindings =
			BindState->ClassFunctionBindings.Find(ControllerClass);
		ASSERT_THAT(IsNotNull(ControllerBindings,
			TEXT("StaticJIT.NativeForms.NetUFunction should retain APlayerController bindings")));

		const FAngelscriptFunctionBinding* FunctionBinding = ControllerBindings->Find(TEXT("ClientSetHUD"));
		ASSERT_THAT(IsNotNull(FunctionBinding,
			TEXT("StaticJIT.NativeForms.NetUFunction should retain the exact ClientSetHUD binding")));
		ASSERT_THAT(IsFalse(FunctionBinding->FunctionPointer.IsBound(),
			TEXT("StaticJIT.NativeForms.NetUFunction must not expose a raw direct-call pointer")));

		FAngelscriptTypeDatabase* TypeDatabase = Engine.GetTypeDatabase();
		ASSERT_THAT(IsNotNull(TypeDatabase,
			TEXT("StaticJIT.NativeForms.NetUFunction should expose the engine-local type database")));
		const TSharedRef<FAngelscriptType>* ControllerType = TypeDatabase->TypesByClass.Find(ControllerClass);
		ASSERT_THAT(IsNotNull(ControllerType,
			TEXT("StaticJIT.NativeForms.NetUFunction should resolve the engine-local controller script type")));

		FAngelscriptFunctionSignature Signature(
			*TypeDatabase,
			*ControllerType,
			ClientSetHUDFunction);
		ASSERT_THAT(IsTrue(Signature.bAllTypesValid,
			TEXT("StaticJIT.NativeForms.NetUFunction should retain a valid exact signature")));
		ASSERT_THAT(IsFalse(Signature.bStaticInScript,
			TEXT("StaticJIT.NativeForms.NetUFunction should remain an instance method")));
		ASSERT_THAT(IsFalse(Signature.bStaticInUnreal,
			TEXT("StaticJIT.NativeForms.NetUFunction should remain an Unreal instance method")));

		asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("StaticJIT.NativeForms.NetUFunction should expose the script engine")));

		const FTCHARToUTF8 ControllerScriptName(*(*ControllerType)->GetAngelscriptTypeName());
		asITypeInfo* ControllerTypeInfo = ScriptEngine->GetTypeInfoByDecl(ControllerScriptName.Get());
		ASSERT_THAT(IsNotNull(ControllerTypeInfo,
			TEXT("StaticJIT.NativeForms.NetUFunction should resolve the controller script type info")));

		asIScriptFunction* ScriptFunction = nullptr;
		const FTCHARToUTF8 ExpectedScriptName(*Signature.ScriptName);
		for (asUINT MethodIndex = 0; MethodIndex < ControllerTypeInfo->GetMethodCount(); ++MethodIndex)
		{
			asIScriptFunction* Candidate = ControllerTypeInfo->GetMethodByIndex(MethodIndex);
			if (Candidate == nullptr || FCStringAnsi::Strcmp(Candidate->GetName(), ExpectedScriptName.Get()) != 0)
			{
				continue;
			}

			FScriptFunctionNativeForm* CandidateNativeForm = FScriptFunctionNativeForm::GetNativeForm(Candidate);
			if (CandidateNativeForm != nullptr
				&& CandidateNativeForm->GetDebugInfoForTesting().UnrealFunction == ClientSetHUDFunction)
			{
				ScriptFunction = Candidate;
				break;
			}
		}
		ASSERT_THAT(IsNotNull(ScriptFunction,
			TEXT("StaticJIT.NativeForms.NetUFunction should resolve ClientSetHUD by exact native form")));
		ASSERT_THAT(IsNotNull(ScriptFunction->GetUserData(),
			TEXT("StaticJIT.NativeForms.NetUFunction should retain event-signature userdata")));

		auto* InternalFunction = static_cast<asCScriptFunction*>(ScriptFunction);
		ASSERT_THAT(IsNotNull(InternalFunction->sysFuncIntf,
			TEXT("StaticJIT.NativeForms.NetUFunction should remain a registered system method")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(ICC_GENERIC_METHOD),
			static_cast<int32>(InternalFunction->sysFuncIntf->callConv),
			TEXT("StaticJIT.NativeForms.NetUFunction should retain generic method callconv")));

		FScriptFunctionNativeForm* NativeForm = FScriptFunctionNativeForm::GetNativeForm(ScriptFunction);
		ASSERT_THAT(IsNotNull(NativeForm,
			TEXT("StaticJIT.NativeForms.NetUFunction should retain its native form")));

		const FAngelscriptNativeFormDebugInfo NativeFormInfo = NativeForm->GetDebugInfoForTesting();
		ASSERT_THAT(AreEqual(
			EAngelscriptNativeFormKind::UFunction,
			NativeFormInfo.Kind,
			TEXT("StaticJIT.NativeForms.NetUFunction should retain UFunction native-form kind")));
		ASSERT_THAT(IsTrue(NativeFormInfo.UnrealFunction == ClientSetHUDFunction,
			TEXT("StaticJIT.NativeForms.NetUFunction should retain the exact RPC UFunction")));
		ASSERT_THAT(AreEqual(
			ClientSetHUDFunction->GetName(),
			NativeFormInfo.Name,
			TEXT("StaticJIT.NativeForms.NetUFunction should retain the exact native name")));
		ASSERT_THAT(IsFalse(NativeFormInfo.bTrivial,
			TEXT("StaticJIT.NativeForms.NetUFunction should remain nontrivial")));
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
