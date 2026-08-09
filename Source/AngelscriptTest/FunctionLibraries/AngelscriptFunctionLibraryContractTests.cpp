#include "CQTest.h"
#include "AngelscriptTestMacros.h"

#include "Binds/Helper_FunctionSignature.h"
#include "Core/AngelscriptBinds.h"
#include "Core/AngelscriptEngine.h"
#include "Core/AngelscriptType.h"
#include "FunctionLibraries/AngelscriptFunctionLibraryContractTestTypes.h"
#include "FunctionLibraries/AngelscriptComponentLibrary.h"
#include "FunctionLibraries/AngelscriptMathLibrary.h"
#include "FunctionLibraries/UAssetManagerMixinLibrary.h"
#include "FunctionLibraries/WidgetBlueprintStatics.h"

#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "StartAngelscriptHeaders.h"
#include "angelscript.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptFunctionLibraryContractTest,
	"Angelscript.TestModule.FunctionLibraries.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static const FName NAME_ModuleRelativePath;
	static const FName NAME_NotInAngelscript;
	static const FName NAME_ScriptCallable;
	static const FName NAME_ScriptMixin;
	static const FName NAME_WorldContext;

	static bool IsRuntimeFunctionLibraryClass(const UClass* Class)
	{
		if (Class == nullptr || Class->GetOutermost()->GetName() != TEXT("/Script/AngelscriptRuntime"))
		{
			return false;
		}

		FString ModuleRelativePath = Class->GetMetaData(NAME_ModuleRelativePath);
		ModuleRelativePath.ReplaceInline(TEXT("\\"), TEXT("/"));
		return ModuleRelativePath.StartsWith(TEXT("FunctionLibraries/"));
	}

	static TArray<UClass*> GetRuntimeFunctionLibraryClasses()
	{
		TArray<UClass*> Classes;
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (IsRuntimeFunctionLibraryClass(*It))
			{
				Classes.Add(*It);
			}
		}

		Classes.Sort([](const UClass& Left, const UClass& Right)
		{
			return Left.GetName() < Right.GetName();
		});
		return Classes;
	}

	static TArray<UFunction*> GetDeclaredFunctions(UClass* Class)
	{
		TArray<UFunction*> Functions;
		for (TFieldIterator<UFunction> It(Class, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			UFunction* Function = *It;
			if (Function != nullptr && Function->GetOuter() == Class)
			{
				Functions.Add(Function);
			}
		}

		Functions.Sort([](const UFunction& Left, const UFunction& Right)
		{
			return Left.GetName() < Right.GetName();
		});
		return Functions;
	}

	static bool IsCallableCandidate(const UFunction* Function)
	{
		return Function != nullptr
			&& (Function->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure)
				|| Function->HasMetaData(NAME_ScriptCallable));
	}

	static bool IsExplicitlyHidden(const UFunction* Function)
	{
		return Function != nullptr && Function->HasMetaData(NAME_NotInAngelscript);
	}

	static bool HasParameterNamed(const UFunction* Function, const FString& ParameterName)
	{
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			const FProperty* Property = *It;
			if (!Property->HasAnyPropertyFlags(CPF_ReturnParm) && Property->GetName() == ParameterName)
			{
				return true;
			}
		}
		return false;
	}

	static TSharedPtr<FAngelscriptType> GetFixtureHostType()
	{
		return FAngelscriptType::GetByClass(UObject::StaticClass());
	}

	static FAngelscriptFunctionSignature MakeFixtureSignature(UClass* FixtureClass, const FName FunctionName)
	{
		TSharedPtr<FAngelscriptType> HostType = GetFixtureHostType();
		check(HostType.IsValid());
		UFunction* Function = FixtureClass->FindFunctionByName(FunctionName);
		check(Function != nullptr);
		return FAngelscriptFunctionSignature(HostType.ToSharedRef(), Function);
	}

	static bool DoesFunctionMatchFinalShape(
		asIScriptFunction* Function,
		const FAngelscriptFunctionSignature& Signature)
	{
		if (Function == nullptr || Signature.Declaration.IsEmpty())
		{
			return false;
		}

		const FTCHARToUTF8 ScriptNameUtf8(*Signature.ScriptName);
		if (FCStringAnsi::Strcmp(Function->GetName(), ScriptNameUtf8.Get()) != 0
			|| (!Signature.bStaticInScript
				&& Function->IsReadOnly() != Signature.Declaration.Contains(TEXT(") const"))))
		{
			return false;
		}

		const FAngelscriptTypeUsage ExistingReturn = FAngelscriptTypeUsage::FromReturn(Function);
		if (ExistingReturn.IsValid() != Signature.ReturnType.IsValid()
			|| (ExistingReturn.IsValid() && !ExistingReturn.EqualsUnqualified(Signature.ReturnType)))
		{
			return false;
		}

		if (Function->GetParamCount() != static_cast<asUINT>(Signature.ArgumentTypes.Num()))
		{
			return false;
		}
		for (int32 Index = 0; Index < Signature.ArgumentTypes.Num(); ++Index)
		{
			const FAngelscriptTypeUsage ExistingArgument = FAngelscriptTypeUsage::FromParam(Function, Index);
			if (!ExistingArgument.IsValid()
				|| !ExistingArgument.EqualsUnqualified(Signature.ArgumentTypes[Index]))
			{
				return false;
			}
		}

		const asCScriptFunction* InternalFunction = static_cast<const asCScriptFunction*>(Function);
		const bool bExpectNoDiscard = Signature.Declaration.Contains(TEXT(" no_discard"));
		const bool bExpectAllowDiscard = Signature.Declaration.Contains(TEXT(" allow_discard"));
		const bool bExpectTemporaryThis = Signature.Declaration.Contains(TEXT(" accept_temporary_this"));
		if (InternalFunction->hiddenArgumentIndex != Signature.WorldContextArgument
			|| InternalFunction->determinesOutputTypeArgumentIndex != Signature.DeterminesOutputTypeArgument
			|| InternalFunction->traits.GetTrait(asTRAIT_NODISCARD) != bExpectNoDiscard
			|| InternalFunction->traits.GetTrait(asTRAIT_ALLOWDISCARD) != bExpectAllowDiscard
			|| InternalFunction->traits.GetTrait(asTRAIT_ACCEPT_TEMPORARY_OBJECT) != bExpectTemporaryThis
			|| (Signature.bNotAngelscriptProperty && Function->IsProperty())
			|| (Signature.bBlueprintProtected && !Function->IsProtected()))
		{
			return false;
		}

#if WITH_EDITOR
		if (InternalFunction->traits.GetTrait(asTRAIT_DEPRECATED) != Signature.bDeprecated)
		{
			return false;
		}
#endif

		return true;
	}

	static TArray<asIScriptFunction*> FindFinalFunctions(
		asIScriptEngine* ScriptEngine,
		const FAngelscriptFunctionSignature& Signature)
	{
		TArray<asIScriptFunction*> Matches;
		if (ScriptEngine == nullptr)
		{
			return Matches;
		}

		if (!Signature.bStaticInScript)
		{
			const FTCHARToUTF8 TypeNameUtf8(*Signature.ClassName);
			if (asITypeInfo* TypeInfo = ScriptEngine->GetTypeInfoByName(TypeNameUtf8.Get()))
			{
				for (asUINT Index = 0; Index < TypeInfo->GetMethodCount(); ++Index)
				{
					asIScriptFunction* Function = TypeInfo->GetMethodByIndex(Index);
					if (DoesFunctionMatchFinalShape(Function, Signature))
					{
						Matches.Add(Function);
					}
				}
			}
			return Matches;
		}

		for (asUINT Index = 0; Index < ScriptEngine->GetGlobalFunctionCount(); ++Index)
		{
			asIScriptFunction* Function = ScriptEngine->GetGlobalFunctionByIndex(Index);
			const FString FunctionNamespace = Function != nullptr
				? UTF8_TO_TCHAR(Function->GetNamespace())
				: FString();
			if (FunctionNamespace == Signature.ClassName
				&& DoesFunctionMatchFinalShape(Function, Signature))
			{
				Matches.Add(Function);
			}
		}
		return Matches;
	}

	static FString DescribeSameNameFunctions(
		asIScriptEngine* ScriptEngine,
		const FAngelscriptFunctionSignature& Signature)
	{
		TArray<FString> Declarations;
		const FTCHARToUTF8 ScriptNameUtf8(*Signature.ScriptName);
		auto AddIfSameName = [&](asIScriptFunction* Function)
		{
			if (Function != nullptr
				&& FCStringAnsi::Strcmp(Function->GetName(), ScriptNameUtf8.Get()) == 0)
			{
				Declarations.Add(UTF8_TO_TCHAR(Function->GetDeclaration(false, false, true, true)));
			}
		};

		if (!Signature.bStaticInScript)
		{
			const FTCHARToUTF8 TypeNameUtf8(*Signature.ClassName);
			if (asITypeInfo* TypeInfo = ScriptEngine->GetTypeInfoByName(TypeNameUtf8.Get()))
			{
				for (asUINT Index = 0; Index < TypeInfo->GetMethodCount(); ++Index)
				{
					AddIfSameName(TypeInfo->GetMethodByIndex(Index));
				}
			}
		}
		else
		{
			for (asUINT Index = 0; Index < ScriptEngine->GetGlobalFunctionCount(); ++Index)
			{
				asIScriptFunction* Function = ScriptEngine->GetGlobalFunctionByIndex(Index);
				if (Function != nullptr && Signature.ClassName == UTF8_TO_TCHAR(Function->GetNamespace()))
				{
					AddIfSameName(Function);
				}
			}
		}

		return Declarations.Num() > 0 ? FString::Join(Declarations, TEXT(" | ")) : TEXT("<none>");
	}

	static bool ExpectMethod(
		FAutomationTestBase& Test,
		asIScriptEngine* ScriptEngine,
		const ANSICHAR* TypeName,
		const ANSICHAR* Declaration)
	{
		FNoDiscardAsserter Assert(Test);
		asITypeInfo* TypeInfo = ScriptEngine != nullptr ? ScriptEngine->GetTypeInfoByName(TypeName) : nullptr;
		if (!Assert.IsNotNull(TypeInfo, FString::Printf(TEXT("%hs should be registered"), TypeName)))
		{
			return false;
		}

		FString Expected = UTF8_TO_TCHAR(Declaration);
		Expected.ReplaceInline(TEXT(" "), TEXT(""));
		for (asUINT Index = 0; Index < TypeInfo->GetMethodCount(); ++Index)
		{
			asIScriptFunction* Method = TypeInfo->GetMethodByIndex(Index);
			if (Method == nullptr)
			{
				continue;
			}

			FString Actual = UTF8_TO_TCHAR(Method->GetDeclaration(false, false, true, true));
			Actual.ReplaceInline(TEXT(" "), TEXT(""));
			if (Actual == Expected)
			{
				return true;
			}
		}

		return Assert.IsTrue(
			false,
			FString::Printf(TEXT("%hs should expose exact declaration %hs"), TypeName, Declaration));
	}

public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(RuntimeFunctionsHaveExplicitDisposition)
	{
		bool bPassed = true;
		for (UClass* Class : GetRuntimeFunctionLibraryClasses())
		{
			for (UFunction* Function : GetDeclaredFunctions(Class))
			{
				if (!IsCallableCandidate(Function) && !IsExplicitlyHidden(Function))
				{
					TestRunner->AddError(FString::Printf(
						TEXT("%s::%s is a bare UFUNCTION with no callable or explicit hidden disposition"),
						*Class->GetName(),
						*Function->GetName()));
					bPassed = false;
				}
			}
		}

		ASSERT_THAT(IsTrue(bPassed, TEXT("Every Runtime FunctionLibrary UFunction should have one explicit exposure disposition")));
	}

	TEST_METHOD(WorldContextMetadataNamesAParameter)
	{
		bool bPassed = true;
		for (UClass* Class : GetRuntimeFunctionLibraryClasses())
		{
			for (UFunction* Function : GetDeclaredFunctions(Class))
			{
				const FString WorldContextName = Function->GetMetaData(NAME_WorldContext);
				if (!WorldContextName.IsEmpty() && !HasParameterNamed(Function, WorldContextName))
				{
					TestRunner->AddError(FString::Printf(
						TEXT("%s::%s names missing WorldContext parameter '%s'"),
						*Class->GetName(),
						*Function->GetName(),
						*WorldContextName));
					bPassed = false;
				}
			}
		}

		ASSERT_THAT(IsTrue(bPassed, TEXT("WorldContext metadata should name an actual reflected parameter")));
	}

	TEST_METHOD(RemovedAssetManagerWrappersAreAbsent)
	{
		UClass* LibraryClass = UAssetManagerMixinLibrary::StaticClass();
		ASSERT_THAT(IsNotNull(LibraryClass));
		ASSERT_THAT(IsNull(LibraryClass->FindFunctionByName(TEXT("GetPrimaryAssetTypeInfo"))));
		ASSERT_THAT(IsNull(LibraryClass->FindFunctionByName(TEXT("GetPrimaryAssetTypeInfoList"))));
		ASSERT_THAT(IsNull(LibraryClass->FindFunctionByName(TEXT("GetPrimaryAssetRules"))));
	}

	TEST_METHOD(GetRenderTransformHasNoWorldContext)
	{
		UFunction* Function = UAngelscriptWidgetMixinLibrary::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(UAngelscriptWidgetMixinLibrary, GetRenderTransform));
		ASSERT_THAT(IsNotNull(Function));
		ASSERT_THAT(IsFalse(Function->HasMetaData(NAME_WorldContext),
			TEXT("GetRenderTransform should not synthesize a hidden WorldContext argument")));
	}

	TEST_METHOD(SceneComponentUsesRichEngineTransformSurface)
	{
		FAngelscriptEngine* Engine = RequireRunningProductionEngine(
			*TestRunner,
			TEXT("SceneComponent contract test requires the initialized production AS surface"));
		ASSERT_THAT(IsNotNull(Engine));
		FAngelscriptEngineScope Scope(*Engine);
		asIScriptEngine* ScriptEngine = Engine != nullptr ? Engine->GetScriptEngine() : nullptr;
		ASSERT_THAT(IsNotNull(ScriptEngine));

		TSharedPtr<FAngelscriptType> SceneComponentType = FAngelscriptType::GetByClass(USceneComponent::StaticClass());
		ASSERT_THAT(IsTrue(SceneComponentType.IsValid()));
		const FName EngineFunctionNames[] = {
			TEXT("K2_SetRelativeLocation"),
			TEXT("K2_SetRelativeRotation"),
			TEXT("K2_SetRelativeTransform"),
			TEXT("K2_SetRelativeLocationAndRotation"),
			TEXT("K2_AddRelativeLocation"),
			TEXT("K2_AddRelativeRotation"),
			TEXT("K2_AddLocalOffset"),
			TEXT("K2_AddLocalRotation"),
			TEXT("K2_AddLocalTransform"),
			TEXT("K2_SetWorldLocation"),
			TEXT("K2_SetWorldRotation"),
			TEXT("K2_SetWorldTransform"),
			TEXT("K2_SetWorldLocationAndRotation"),
			TEXT("K2_AddWorldOffset"),
			TEXT("K2_AddWorldRotation"),
			TEXT("K2_AddWorldTransform"),
		};
		for (const FName FunctionName : EngineFunctionNames)
		{
			UFunction* Function = USceneComponent::StaticClass()->FindFunctionByName(FunctionName);
			ASSERT_THAT(IsNotNull(Function, FString::Printf(TEXT("%s should be reflected by UE"), *FunctionName.ToString())));
			const FAngelscriptFunctionSignature Signature(SceneComponentType.ToSharedRef(), Function);
			ASSERT_THAT(IsTrue(Signature.bAllTypesValid));
			ASSERT_THAT(AreEqual(1, FindFinalFunctions(ScriptEngine, Signature).Num(),
				FString::Printf(TEXT("UE rich declaration '%s' should exist exactly once"), *Signature.Declaration)));
		}

		UClass* LibraryClass = UAngelscriptComponentLibrary::StaticClass();
		const FName RemovedWrapperNames[] = {
			TEXT("SetRelativeLocation"),
			TEXT("SetRelativeRotation"),
			TEXT("SetRelativeTransform"),
			TEXT("SetRelativeLocationAndRotation"),
			TEXT("AddRelativeLocation"),
			TEXT("AddRelativeRotation"),
			TEXT("AddLocalOffset"),
			TEXT("AddLocalRotation"),
			TEXT("AddLocalTransform"),
			TEXT("SetWorldLocation"),
			TEXT("SetWorldRotation"),
			TEXT("SetWorldTransform"),
			TEXT("SetWorldLocationAndRotation"),
			TEXT("AddWorldOffset"),
			TEXT("AddWorldRotation"),
			TEXT("AddWorldTransform"),
		};
		for (const FName FunctionName : RemovedWrapperNames)
		{
			ASSERT_THAT(IsNull(LibraryClass->FindFunctionByName(FunctionName),
				FString::Printf(TEXT("Simplified wrapper %s should be removed"), *FunctionName.ToString())));
		}

		TSharedPtr<FAngelscriptType> LibraryType = FAngelscriptType::GetByClass(LibraryClass);
		ASSERT_THAT(IsTrue(LibraryType.IsValid()));
		const FName UniqueWrapperNames[] = {
			TEXT("SetRelativeRotationQuat"),
			TEXT("SetRelativeLocationAndRotationQuat"),
			TEXT("AddRelativeRotationQuat"),
			TEXT("AddLocalRotationQuat"),
			TEXT("SetWorldRotationQuat"),
			TEXT("SetWorldLocationAndRotationQuat"),
			TEXT("AddWorldRotationQuat"),
			TEXT("SetComponentQuat"),
			TEXT("GetComponentQuat"),
			TEXT("GetSocketQuaternion"),
		};
		for (const FName FunctionName : UniqueWrapperNames)
		{
			UFunction* Function = LibraryClass->FindFunctionByName(FunctionName);
			ASSERT_THAT(IsNotNull(Function));
			const FAngelscriptFunctionSignature Signature(LibraryType.ToSharedRef(), Function);
			ASSERT_THAT(IsTrue(Signature.bAllTypesValid));
			ASSERT_THAT(AreEqual(1, FindFinalFunctions(ScriptEngine, Signature).Num(),
				FString::Printf(TEXT("Unique declaration '%s' should remain exactly once"), *Signature.Declaration)));
		}
	}

	TEST_METHOD(LevelStreamingEditorBoundaryStaysInRuntime)
	{
		const FString RuntimeRoot = FPaths::Combine(
			FPaths::ProjectDir(),
			TEXT("Plugins/Angelscript/Source/AngelscriptRuntime"));
		const FString HeaderPath = FPaths::Combine(
			RuntimeRoot,
			TEXT("FunctionLibraries/AngelscriptLevelStreamingLibrary.h"));
		const FString BindPath = FPaths::Combine(
			RuntimeRoot,
			TEXT("Binds/Bind_FunctionLibraryMixins.cpp"));
		FString HeaderContents;
		FString BindContents;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(HeaderContents, *HeaderPath)));
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(BindContents, *BindPath)));

		const int32 HeaderGuard = HeaderContents.Find(TEXT("#if WITH_EDITOR"));
		const int32 HeaderMethod = HeaderContents.Find(TEXT("GetShouldBeVisibleInEditor"));
		const int32 HeaderGuardEnd = HeaderContents.Find(TEXT("#endif"), ESearchCase::CaseSensitive, ESearchDir::FromStart, HeaderMethod);
		ASSERT_THAT(IsTrue(HeaderGuard != INDEX_NONE && HeaderGuard < HeaderMethod && HeaderMethod < HeaderGuardEnd,
			TEXT("LevelStreaming FunctionLibrary declaration should remain inside WITH_EDITOR")));

		const int32 BindGuard = BindContents.Find(TEXT("#if WITH_EDITOR"));
		const int32 BindMethod = BindContents.Find(TEXT("GetShouldBeVisibleInEditor"));
		const int32 BindGuardEnd = BindContents.Find(TEXT("#endif"), ESearchCase::CaseSensitive, ESearchDir::FromStart, BindMethod);
		ASSERT_THAT(IsTrue(BindGuard != INDEX_NONE && BindGuard < BindMethod && BindMethod < BindGuardEnd,
			TEXT("LevelStreaming supplement should remain inside a matching WITH_EDITOR guard")));

		const FString EditorFunctionLibraries = FPaths::Combine(
			FPaths::ProjectDir(),
			TEXT("Plugins/Angelscript/Source/AngelscriptEditor/FunctionLibraries"));
		TArray<FString> EditorLevelStreamingFiles;
		IFileManager::Get().FindFilesRecursive(
			EditorLevelStreamingFiles,
			*EditorFunctionLibraries,
			TEXT("*LevelStreaming*"),
			true,
			false);
		ASSERT_THAT(AreEqual(0, EditorLevelStreamingFiles.Num(),
			TEXT("LevelStreaming should not gain an AngelscriptEditor FunctionLibrary substitute")));
	}

	TEST_METHOD(FinalSurfaceMatchesMetadata)
	{
		FAngelscriptEngine* Engine = RequireRunningProductionEngine(
			*TestRunner,
			TEXT("FunctionLibrary contract tests require the initialized production AS surface"));
		ASSERT_THAT(IsNotNull(Engine));
		FAngelscriptEngineScope Scope(*Engine);
		asIScriptEngine* ScriptEngine = Engine != nullptr ? Engine->GetScriptEngine() : nullptr;
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Contract test should have an initialized AS engine")));

		bool bPassed = true;
		for (UClass* Class : GetRuntimeFunctionLibraryClasses())
		{
			TSharedPtr<FAngelscriptType> HostType = FAngelscriptType::GetByClass(Class);
			if (!HostType.IsValid())
			{
				TestRunner->AddError(FString::Printf(TEXT("No AS host type was registered for %s"), *Class->GetName()));
				bPassed = false;
				continue;
			}

			for (UFunction* Function : GetDeclaredFunctions(Class))
			{
				if (!IsCallableCandidate(Function))
				{
					continue;
				}

				FAngelscriptFunctionSignature Signature(HostType.ToSharedRef(), Function);
				if (!Signature.bAllTypesValid)
				{
					TestRunner->AddError(FString::Printf(
						TEXT("%s::%s could not construct a complete AS declaration"),
						*Class->GetName(),
						*Function->GetName()));
					bPassed = false;
					continue;
				}

				const TArray<asIScriptFunction*> Matches = FindFinalFunctions(ScriptEngine, Signature);
				const bool bExplicitlyHidden = IsExplicitlyHidden(Function);
				const int32 ExpectedCount = bExplicitlyHidden ? 0 : 1;
				const bool bCountMatches = bExplicitlyHidden ? Matches.Num() <= 1 : Matches.Num() == 1;
				if (!bCountMatches)
				{
					TestRunner->AddError(FString::Printf(
						TEXT("%s::%s expected %d final declaration(s) matching '%s' on '%s', found %d; same-name declarations: %s"),
						*Class->GetName(),
						*Function->GetName(),
						ExpectedCount,
						*Signature.Declaration,
						*Signature.ClassName,
						Matches.Num(),
						*DescribeSameNameFunctions(ScriptEngine, Signature)));
					bPassed = false;
				}
			}
		}

		ASSERT_THAT(IsTrue(bPassed, TEXT("Every callable Runtime FunctionLibrary declaration should exist in the final AS surface")));
	}

	TEST_METHOD(RejectsMissingReceiver)
	{
		const FAngelscriptFunctionSignature Signature = MakeFixtureSignature(
			UAngelscriptFunctionLibraryMissingReceiverFixture::StaticClass(),
			GET_FUNCTION_NAME_CHECKED(UAngelscriptFunctionLibraryMissingReceiverFixture, MissingReceiver));

		ASSERT_THAT(IsFalse(Signature.bAllTypesValid, TEXT("ScriptMixin without a first parameter should be rejected")));
		ASSERT_THAT(IsFalse(Signature.bStaticInScript, TEXT("Invalid ScriptMixin intent must not fall back to a static namespace")));
		ASSERT_THAT(IsTrue(Signature.ValidationError.Contains(TEXT("AngelscriptFunctionLibraryMissingReceiverFixture::MissingReceiver"))));
		ASSERT_THAT(IsTrue(Signature.ValidationError.Contains(TEXT("targets='FVector'"))));
		ASSERT_THAT(IsTrue(Signature.ValidationError.Contains(TEXT("first parameter='<missing>'"))));
	}

	TEST_METHOD(RejectsUnresolvedReceiver)
	{
		const FAngelscriptFunctionSignature Signature = MakeFixtureSignature(
			UAngelscriptFunctionLibraryUnresolvedReceiverFixture::StaticClass(),
			GET_FUNCTION_NAME_CHECKED(UAngelscriptFunctionLibraryUnresolvedReceiverFixture, UnresolvedReceiver));

		ASSERT_THAT(IsFalse(Signature.bAllTypesValid, TEXT("Unresolved ScriptMixin target should be rejected")));
		ASSERT_THAT(IsFalse(Signature.bStaticInScript, TEXT("Unresolved ScriptMixin target must not fall back to a static namespace")));
		ASSERT_THAT(IsTrue(Signature.ValidationError.Contains(TEXT("AngelscriptFunctionLibraryUnresolvedReceiverFixture::UnresolvedReceiver"))));
		ASSERT_THAT(IsTrue(Signature.ValidationError.Contains(TEXT("targets='FDoesNotExist'"))));
		ASSERT_THAT(IsTrue(Signature.ValidationError.Contains(TEXT("first parameter='FVector'"))));
	}

	TEST_METHOD(RejectsIncompatibleReceiver)
	{
		const FAngelscriptFunctionSignature Signature = MakeFixtureSignature(
			UAngelscriptFunctionLibraryIncompatibleReceiverFixture::StaticClass(),
			GET_FUNCTION_NAME_CHECKED(UAngelscriptFunctionLibraryIncompatibleReceiverFixture, IncompatibleReceiver));

		ASSERT_THAT(IsFalse(Signature.bAllTypesValid, TEXT("Incompatible ScriptMixin receiver should be rejected")));
		ASSERT_THAT(IsFalse(Signature.bStaticInScript, TEXT("Incompatible ScriptMixin receiver must not fall back to a static namespace")));
		ASSERT_THAT(IsTrue(Signature.ValidationError.Contains(TEXT("AngelscriptFunctionLibraryIncompatibleReceiverFixture::IncompatibleReceiver"))));
		ASSERT_THAT(IsTrue(Signature.ValidationError.Contains(TEXT("targets='FRotator'"))));
		ASSERT_THAT(IsTrue(Signature.ValidationError.Contains(TEXT("first parameter='FVector'"))));
	}

	TEST_METHOD(SelectsMatchingReceiverFromMultipleTargets)
	{
		const FAngelscriptFunctionSignature Signature = MakeFixtureSignature(
			UAngelscriptFunctionLibraryMultiTargetFixture::StaticClass(),
			GET_FUNCTION_NAME_CHECKED(UAngelscriptFunctionLibraryMultiTargetFixture, MatchingReceiver));

		ASSERT_THAT(IsTrue(Signature.bAllTypesValid, TEXT("A compatible receiver in a multi-target ScriptMixin should remain valid")));
		ASSERT_THAT(IsFalse(Signature.bStaticInScript, TEXT("A compatible receiver should become an instance method")));
		ASSERT_THAT(AreEqual(FString(TEXT("FVector")), Signature.ClassName, TEXT("The exact matching target should own the method")));
		ASSERT_THAT(AreEqual(0, Signature.ArgumentTypes.Num(), TEXT("The matching receiver should be removed from public arguments")));
	}

	TEST_METHOD(PreservesExplicitStaticFactoryNamespace)
	{
		TSharedPtr<FAngelscriptType> HostType = FAngelscriptType::GetByClass(UAngelscriptFQuatLibrary::StaticClass());
		ASSERT_THAT(IsTrue(HostType.IsValid()));
		UFunction* Function = UAngelscriptFQuatLibrary::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(UAngelscriptFQuatLibrary, MakeFromX));
		ASSERT_THAT(IsNotNull(Function));

		const FAngelscriptFunctionSignature Signature(HostType.ToSharedRef(), Function);
		ASSERT_THAT(IsTrue(Signature.bAllTypesValid, TEXT("An explicit class ScriptName should preserve static factory helpers")));
		ASSERT_THAT(IsTrue(Signature.bStaticInScript));
		ASSERT_THAT(AreEqual(FString(TEXT("FQuat")), Signature.ClassName));
		ASSERT_THAT(IsTrue(Signature.ValidationError.IsEmpty()));
	}

	TEST_METHOD(SupplementDeclarationsUseExactIdentity)
	{
		FAngelscriptEngine* Engine = RequireRunningProductionEngine(
			*TestRunner,
			TEXT("FunctionLibrary supplement contract tests require the initialized production AS surface"));
		ASSERT_THAT(IsNotNull(Engine));
		FAngelscriptEngineScope Scope(*Engine);
		asIScriptEngine* ScriptEngine = Engine != nullptr ? Engine->GetScriptEngine() : nullptr;
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Contract test should have an initialized AS engine")));

		ASSERT_THAT(IsTrue(ExpectMethod(
			*TestRunner,
			ScriptEngine,
			"FRuntimeCurveLinearColor",
			"void AddDefaultKey(float32 InTime, FLinearColor InColor)"),
			TEXT("RuntimeCurveLinearColor exact declaration should be present")));
		ASSERT_THAT(IsTrue(ExpectMethod(
			*TestRunner,
			ScriptEngine,
			"FRuntimeFloatCurve",
			"void AddDefaultKey(float32 InTime, float32 InValue)"),
			TEXT("RuntimeFloatCurve exact declaration should be present")));
		ASSERT_THAT(IsTrue(ExpectMethod(
			*TestRunner,
			ScriptEngine,
			"UCurveFloat",
			"FCurveKeyHandle AddAutoCurveKey(float32 InTime, float32 InValue)"),
			TEXT("UCurveFloat exact AddAutoCurveKey overload should be present")));
		ASSERT_THAT(IsTrue(ExpectMethod(
			*TestRunner,
			ScriptEngine,
			"UCurveFloat",
			"void SetKeyInterpMode(FCurveKeyHandle KeyHandle, ERichCurveInterpMode NewInterpMode, bool bAutoSetTangents)"),
			TEXT("UCurveFloat exact SetKeyInterpMode overload should be present")));
	}
};

const FName FAngelscriptFunctionLibraryContractTest::NAME_ModuleRelativePath(TEXT("ModuleRelativePath"));
const FName FAngelscriptFunctionLibraryContractTest::NAME_NotInAngelscript(TEXT("NotInAngelscript"));
const FName FAngelscriptFunctionLibraryContractTest::NAME_ScriptCallable(TEXT("ScriptCallable"));
const FName FAngelscriptFunctionLibraryContractTest::NAME_ScriptMixin(TEXT("ScriptMixin"));
const FName FAngelscriptFunctionLibraryContractTest::NAME_WorldContext(TEXT("WorldContext"));

#endif // WITH_ANGELSCRIPT_UNITTESTS
