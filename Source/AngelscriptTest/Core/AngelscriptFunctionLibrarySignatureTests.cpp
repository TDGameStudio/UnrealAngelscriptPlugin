#include "Core/AngelscriptBinds.h"
#include "Core/AngelscriptEngine.h"
#include "Binds/Helper_FunctionSignature.h"
#include "FunctionLibraries/AngelscriptFrameTimeMixinLibrary.h"
#include "FunctionLibraries/AngelscriptMathLibrary.h"
#include "FunctionLibraries/GameplayTagQueryMixinLibrary.h"
#include "FunctionLibraries/SubsystemLibrary.h"
#include "FunctionLibraries/WidgetBlueprintStatics.h"
#include "AngelscriptTestUtilities.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

class asIScriptGeneric;

namespace SubsystemGetterMetadataTest
{
	struct FSubsystemGetterExpectation
	{
		const TCHAR* FunctionName;
		const TCHAR* ExpectedNamespace;
		bool bExpectHiddenWorldContext = false;
		int32 ExpectedHiddenArgumentIndex = -1;
		bool bExpectWorldContextTrait = false;
		const TCHAR* RequiredDeclarationFragment = nullptr;
	};

	void CDECL NoOpGeneric(asIScriptGeneric* Generic)
	{
		(void)Generic;
	}

	void ResetIsolatedEnvironment()
	{
		DestroySharedTestEngine();
		FAngelscriptBinds::ResetBindState();

		if (FAngelscriptEngine::IsInitialized())
		{
			FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
		}
	}

	bool CheckSubsystemGetterSignature(
		FAutomationTestBase& Test,
		const TSharedRef<FAngelscriptType>& HostType,
		const FSubsystemGetterExpectation& Expectation)
	{
		UFunction* Function = USubsystemLibrary::StaticClass()->FindFunctionByName(Expectation.FunctionName);
		if (!Test.TestNotNull(
				FString::Printf(TEXT("SubsystemGetterMetadata should find reflected function %s"), Expectation.FunctionName),
				Function))
		{
			return false;
		}

		FAngelscriptFunctionSignature Signature(HostType, Function);
		const int FunctionId = FAngelscriptBinds::BindGlobalGenericFunction(Signature.Declaration, &NoOpGeneric);
		Signature.ModifyScriptFunction(FunctionId);

		auto* ScriptFunction = reinterpret_cast<asCScriptFunction*>(FAngelscriptEngine::Get().GetScriptEngine()->GetFunctionById(FunctionId));
		if (!Test.TestNotNull(
				FString::Printf(TEXT("SubsystemGetterMetadata should create script function %s"), Expectation.FunctionName),
				ScriptFunction))
		{
			return false;
		}

		bool bPassed = true;
		bPassed &= Test.TestEqual(
			FString::Printf(TEXT("%s should bind under the full subsystem library namespace"), Expectation.FunctionName),
			Signature.ClassName,
			FString(Expectation.ExpectedNamespace));
		bPassed &= Test.TestTrue(
			FString::Printf(TEXT("%s should remain a static script function"), Expectation.FunctionName),
			Signature.bStaticInScript);
		bPassed &= Test.TestTrue(
			FString::Printf(TEXT("%s should keep the generated declaration non-empty"), Expectation.FunctionName),
			!Signature.Declaration.IsEmpty());
		bPassed &= Test.TestTrue(
			FString::Printf(TEXT("%s should preserve the original function name in the declaration"), Expectation.FunctionName),
			Signature.Declaration.Contains(Expectation.FunctionName));
		bPassed &= Test.TestTrue(
			FString::Printf(TEXT("%s should append no_discard to the declaration"), Expectation.FunctionName),
			Signature.Declaration.Contains(TEXT("no_discard")));
		bPassed &= Test.TestEqual(
			FString::Printf(TEXT("%s should set the expected hidden world-context argument index"), Expectation.FunctionName),
			static_cast<int32>(ScriptFunction->hiddenArgumentIndex),
			Expectation.ExpectedHiddenArgumentIndex);
		bPassed &= Test.TestEqual(
			FString::Printf(TEXT("%s should set the expected world-context trait"), Expectation.FunctionName),
			ScriptFunction->traits.GetTrait(asTRAIT_USES_WORLDCONTEXT),
			Expectation.bExpectWorldContextTrait);

		if (Expectation.RequiredDeclarationFragment != nullptr)
		{
			bPassed &= Test.TestTrue(
				FString::Printf(TEXT("%s should preserve declaration fragment %s"), Expectation.FunctionName, Expectation.RequiredDeclarationFragment),
				Signature.Declaration.Contains(Expectation.RequiredDeclarationFragment));
		}

		if (Expectation.bExpectHiddenWorldContext)
		{
			bPassed &= Test.TestEqual(
				FString::Printf(TEXT("%s should record the same world-context argument before script-function mutation"), Expectation.FunctionName),
				static_cast<int32>(Signature.WorldContextArgument),
				Expectation.ExpectedHiddenArgumentIndex);
		}
		else
		{
			bPassed &= Test.TestEqual(
				FString::Printf(TEXT("%s should not record a hidden world-context argument in the signature"), Expectation.FunctionName),
				static_cast<int32>(Signature.WorldContextArgument),
				-1);
		}

		return bPassed;
	}
}

using namespace SubsystemGetterMetadataTest;

namespace MathReturnValueHelperMetadataTest
{
	struct FMathHelperExpectation
	{
		const TCHAR* FunctionName;
		const TCHAR* ExpectedScriptName;
		const TCHAR* ExpectedNamespace;
	};

	bool CheckMathHelperSignature(
		FAutomationTestBase& Test,
		const TSharedRef<FAngelscriptType>& HostType,
		const FMathHelperExpectation& Expectation)
	{
		UFunction* Function = UAngelscriptMathLibrary::StaticClass()->FindFunctionByName(Expectation.FunctionName);
		if (!Test.TestNotNull(
				FString::Printf(TEXT("MathReturnValueHelperMetadata should find reflected function %s"), Expectation.FunctionName),
				Function))
		{
			return false;
		}

		FAngelscriptFunctionSignature Signature(HostType, Function);
		const int FunctionId = FAngelscriptBinds::BindGlobalGenericFunction(Signature.Declaration, &SubsystemGetterMetadataTest::NoOpGeneric);
		Signature.ModifyScriptFunction(FunctionId);

		auto* ScriptFunction = reinterpret_cast<asCScriptFunction*>(FAngelscriptEngine::Get().GetScriptEngine()->GetFunctionById(FunctionId));
		if (!Test.TestNotNull(
				FString::Printf(TEXT("MathReturnValueHelperMetadata should create script function %s"), Expectation.FunctionName),
				ScriptFunction))
		{
			return false;
		}

		bool bPassed = true;
		bPassed &= Test.TestEqual(
			FString::Printf(TEXT("%s should bind under the full math library namespace"), Expectation.FunctionName),
			Signature.ClassName,
			FString(Expectation.ExpectedNamespace));
		bPassed &= Test.TestTrue(
			FString::Printf(TEXT("%s should remain a static script function"), Expectation.FunctionName),
			Signature.bStaticInScript);
		bPassed &= Test.TestEqual(
			FString::Printf(TEXT("%s should expose the expected script alias"), Expectation.FunctionName),
			Signature.ScriptName,
			FString(Expectation.ExpectedScriptName));
		bPassed &= Test.TestTrue(
			FString::Printf(TEXT("%s should append no_discard to the declaration"), Expectation.FunctionName),
			Signature.Declaration.Contains(TEXT("no_discard")));
		bPassed &= Test.TestTrue(
			FString::Printf(TEXT("%s should keep the expected alias in the declaration"), Expectation.FunctionName),
			Signature.Declaration.Contains(Expectation.ExpectedScriptName));
		bPassed &= Test.TestTrue(
			FString::Printf(TEXT("%s should remain a trivial bind"), Expectation.FunctionName),
			Signature.bTrivial);
		bPassed &= Test.TestEqual(
			FString::Printf(TEXT("%s should not hide a world-context argument"), Expectation.FunctionName),
			static_cast<int32>(Signature.WorldContextArgument),
			-1);
		bPassed &= Test.TestEqual(
			FString::Printf(TEXT("%s should not mark a determines-output-type argument"), Expectation.FunctionName),
			static_cast<int32>(Signature.DeterminesOutputTypeArgument),
			-1);
		bPassed &= Test.TestEqual(
			FString::Printf(TEXT("%s should keep the script function free of hidden arguments"), Expectation.FunctionName),
			static_cast<int32>(ScriptFunction->hiddenArgumentIndex),
			-1);
		bPassed &= Test.TestEqual(
			FString::Printf(TEXT("%s should keep the script function free of determines-output-type arguments"), Expectation.FunctionName),
			static_cast<int32>(ScriptFunction->determinesOutputTypeArgumentIndex),
			-1);
		if (FCString::Strcmp(Expectation.FunctionName, Expectation.ExpectedScriptName) != 0)
		{
			bPassed &= Test.TestFalse(
				FString::Printf(TEXT("%s should not leak the Unreal-only name into the declaration"), Expectation.FunctionName),
				Signature.Declaration.Contains(Expectation.FunctionName));
		}

		return bPassed;
	}
}

namespace ProductionScriptMixinSignatureTest
{
	struct FProductionScriptMixinExpectation
	{
		UClass* FunctionLibraryClass = nullptr;
		const TCHAR* FunctionName = nullptr;
		const TCHAR* ExpectedClassName = nullptr;
		int32 ExpectedPublicArgumentCount = 0;
		const TCHAR* RequiredDeclarationFragment = nullptr;
		const TCHAR* RequiredArgumentTypeFragment = nullptr;
		const TCHAR* ForbiddenDeclarationFragment = nullptr;
		bool bExpectConstMethod = false;
	};

	TSharedPtr<FAngelscriptType> ResolveHostTypeFromFirstParameter(UFunction* Function)
	{
		for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It)
		{
			FProperty* Property = *It;
			if (Property->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				continue;
			}

			if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
			{
				return FAngelscriptType::GetByClass(ObjectProperty->PropertyClass);
			}

			if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
			{
				return FAngelscriptTypeUsage::FromStruct(StructProperty->Struct).Type;
			}

			return FAngelscriptTypeUsage::FromProperty(Property).Type;
		}

		return nullptr;
	}

	int32 BindSignatureForInspection(const FAngelscriptFunctionSignature& Signature)
	{
		if (Signature.bStaticInScript)
		{
			return FAngelscriptBinds::BindGlobalGenericFunction(
				Signature.Declaration,
				&SubsystemGetterMetadataTest::NoOpGeneric);
		}

		return FAngelscriptBinds::BindMethodDirect(
			Signature.ClassName,
			Signature.Declaration,
			asFUNCTION(SubsystemGetterMetadataTest::NoOpGeneric),
			asCALL_GENERIC,
			ASAutoCaller::FunctionCaller::Make());
	}

	bool CheckProductionScriptMixinSignature(
		FAutomationTestBase& Test,
		const FProductionScriptMixinExpectation& Expectation)
	{
		UFunction* Function = Expectation.FunctionLibraryClass->FindFunctionByName(Expectation.FunctionName);
		if (!Test.TestNotNull(
				FString::Printf(TEXT("ProductionScriptMixinSignatures should find reflected function %s"), Expectation.FunctionName),
				Function))
		{
			return false;
		}

		TSharedPtr<FAngelscriptType> HostType = ResolveHostTypeFromFirstParameter(Function);
		if (!Test.TestTrue(
				FString::Printf(TEXT("ProductionScriptMixinSignatures should resolve the host type for %s from its first parameter"), Expectation.FunctionName),
				HostType.IsValid()))
		{
			return false;
		}

		const FString InspectName = FString::Printf(TEXT("%s_ProductionScriptMixinInspection"), Expectation.FunctionName);
		FAngelscriptFunctionSignature Signature(HostType.ToSharedRef(), Function, *InspectName);
		const int32 FunctionId = BindSignatureForInspection(Signature);
		Signature.ModifyScriptFunction(FunctionId);

		auto* ScriptFunction = reinterpret_cast<asCScriptFunction*>(FAngelscriptEngine::Get().GetScriptEngine()->GetFunctionById(FunctionId));
		if (!Test.TestNotNull(
				FString::Printf(TEXT("ProductionScriptMixinSignatures should create a script function for %s"), Expectation.FunctionName),
				ScriptFunction))
		{
			return false;
		}

		const FString ScriptDeclaration = ANSI_TO_TCHAR(ScriptFunction->GetDeclaration(true, false, true, true));

		bool bPassed = true;
		bPassed &= Test.TestEqual(
			FString::Printf(TEXT("%s should resolve the expected host type name"), Expectation.FunctionName),
			HostType->GetAngelscriptTypeName(),
			FString(Expectation.ExpectedClassName));
		bPassed &= Test.TestTrue(
			FString::Printf(TEXT("%s should keep the Unreal function static"), Expectation.FunctionName),
			Signature.bStaticInUnreal);
		bPassed &= Test.TestFalse(
			FString::Printf(TEXT("%s should bind production ScriptMixin functions as script members"), Expectation.FunctionName),
			Signature.bStaticInScript);
		bPassed &= Test.TestEqual(
			FString::Printf(TEXT("%s should expose the expected script member owner"), Expectation.FunctionName),
			Signature.ClassName,
			FString(Expectation.ExpectedClassName));
		bPassed &= Test.TestEqual(
			FString::Printf(TEXT("%s should expose the expected number of public parameters in the signature"), Expectation.FunctionName),
			Signature.ArgumentTypes.Num(),
			Expectation.ExpectedPublicArgumentCount);
		bPassed &= Test.TestEqual(
			FString::Printf(TEXT("%s should expose the expected number of public parameters in the script function"), Expectation.FunctionName),
			static_cast<int32>(ScriptFunction->GetParamCount()),
			Expectation.ExpectedPublicArgumentCount);
		bPassed &= Test.TestEqual(
			FString::Printf(TEXT("%s should not leave a hidden world-context argument in the signature"), Expectation.FunctionName),
			static_cast<int32>(Signature.WorldContextArgument),
			-1);
		bPassed &= Test.TestEqual(
			FString::Printf(TEXT("%s should not hide a world-context argument on the script function"), Expectation.FunctionName),
			static_cast<int32>(ScriptFunction->hiddenArgumentIndex),
			-1);
		bPassed &= Test.TestFalse(
			FString::Printf(TEXT("%s should not mark the script function with the world-context trait"), Expectation.FunctionName),
			ScriptFunction->traits.GetTrait(asTRAIT_USES_WORLDCONTEXT));

		if (Expectation.RequiredDeclarationFragment != nullptr)
		{
			bPassed &= Test.TestTrue(
				FString::Printf(TEXT("%s should preserve declaration fragment %s in the generated signature"), Expectation.FunctionName, Expectation.RequiredDeclarationFragment),
				Signature.Declaration.Contains(Expectation.RequiredDeclarationFragment));
			bPassed &= Test.TestTrue(
				FString::Printf(TEXT("%s should preserve declaration fragment %s on the script function"), Expectation.FunctionName, Expectation.RequiredDeclarationFragment),
				ScriptDeclaration.Contains(Expectation.RequiredDeclarationFragment));
		}

		if (Expectation.RequiredArgumentTypeFragment != nullptr)
		{
			bPassed &= Test.TestEqual(
				FString::Printf(TEXT("%s should expose exactly one explicit argument before checking its type"), Expectation.FunctionName),
				Signature.ArgumentTypes.Num(),
				1);
			if (Signature.ArgumentTypes.Num() == 1)
			{
				const FString ExposedArgumentType = Signature.ArgumentTypes[0].GetAngelscriptDeclaration();
				bPassed &= Test.TestTrue(
					FString::Printf(TEXT("%s should expose the expected explicit argument type"), Expectation.FunctionName),
					ExposedArgumentType.Contains(Expectation.RequiredArgumentTypeFragment));
				bPassed &= Test.TestTrue(
					FString::Printf(TEXT("%s should preserve the explicit argument name after mixin trimming"), Expectation.FunctionName),
					Signature.ArgumentNames[0] == TEXT("Tags"));
			}
		}

		if (Expectation.ForbiddenDeclarationFragment != nullptr)
		{
			bPassed &= Test.TestFalse(
				FString::Printf(TEXT("%s should not leak declaration fragment %s into the generated signature"), Expectation.FunctionName, Expectation.ForbiddenDeclarationFragment),
				Signature.Declaration.Contains(Expectation.ForbiddenDeclarationFragment));
			bPassed &= Test.TestFalse(
				FString::Printf(TEXT("%s should not leak declaration fragment %s into the script function"), Expectation.FunctionName, Expectation.ForbiddenDeclarationFragment),
				ScriptDeclaration.Contains(Expectation.ForbiddenDeclarationFragment));
		}

		if (Expectation.bExpectConstMethod)
		{
			bPassed &= Test.TestTrue(
				FString::Printf(TEXT("%s should generate a const member declaration"), Expectation.FunctionName),
				Signature.Declaration.Contains(TEXT("const")));
			bPassed &= Test.TestTrue(
				FString::Printf(TEXT("%s should keep the script declaration const"), Expectation.FunctionName),
				ScriptDeclaration.Contains(TEXT("const")));
		}

		return bPassed;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptFunctionLibrarySignatureTests,
	"Angelscript.TestModule.Engine.BindConfig",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(SubsystemGetterMetadata)
	{
		SubsystemGetterMetadataTest::ResetIsolatedEnvironment();
		ON_SCOPE_EXIT
		{
			SubsystemGetterMetadataTest::ResetIsolatedEnvironment();
		};

		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeFullEngineForTesting(FAngelscriptEngineConfig(), Dependencies);
		if (!TestRunner->TestTrue(TEXT("SubsystemGetterMetadata should create a testing engine"), Engine.IsValid()))
		{
			return;
		}

		FAngelscriptEngineScope EngineScope(*Engine);

		TSharedPtr<FAngelscriptType> HostType = FAngelscriptType::GetByClass(USubsystemLibrary::StaticClass());
		if (!TestRunner->TestTrue(TEXT("SubsystemGetterMetadata should resolve the subsystem library host type"), HostType.IsValid()))
		{
			return;
		}

		const TArray<SubsystemGetterMetadataTest::FSubsystemGetterExpectation> Expectations = {
			{
				TEXT("GetEngineSubsystem"),
				TEXT("USubsystemLibrary"),
				false,
				-1,
				false,
				TEXT("GetEngineSubsystem")
			},
			{
				TEXT("GetGameInstanceSubsystem"),
				TEXT("USubsystemLibrary"),
				true,
				0,
				true,
				nullptr
			},
			{
				TEXT("GetLocalPlayerSubsystem"),
				TEXT("USubsystemLibrary"),
				true,
				0,
				true,
				nullptr
			},
			{
				TEXT("GetWorldSubsystem"),
				TEXT("USubsystemLibrary"),
				true,
				0,
				true,
				nullptr
			},
			{
				TEXT("GetLocalPlayerSubsystemFromPlayerController"),
				TEXT("USubsystemLibrary"),
				false,
				-1,
				false,
				nullptr
			},
			{
				TEXT("GetLocalPlayerSubsystemFromLocalPlayer"),
				TEXT("USubsystemLibrary"),
				false,
				-1,
				false,
				TEXT("LocalPlayer")
			}
		};

		for (const SubsystemGetterMetadataTest::FSubsystemGetterExpectation& Expectation : Expectations)
		{
			SubsystemGetterMetadataTest::CheckSubsystemGetterSignature(
				*TestRunner,
				HostType.ToSharedRef(),
				Expectation);
		}
	}

	TEST_METHOD(SubsystemHelperNamespaceBinds)
	{
		SubsystemGetterMetadataTest::ResetIsolatedEnvironment();
		ON_SCOPE_EXIT
		{
			SubsystemGetterMetadataTest::ResetIsolatedEnvironment();
		};

		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeFullEngineForTesting(FAngelscriptEngineConfig(), Dependencies);
		if (!TestRunner->TestTrue(TEXT("SubsystemHelperNamespaceBinds should create a testing engine"), Engine.IsValid()))
		{
			return;
		}

		FAngelscriptEngineScope EngineScope(*Engine);
		asIScriptEngine* ScriptEngine = Engine->GetScriptEngine();
		if (!TestRunner->TestNotNull(TEXT("SubsystemHelperNamespaceBinds should expose the AS engine"), ScriptEngine))
		{
			return;
		}

		const FString PreviousNamespace = ANSI_TO_TCHAR(ScriptEngine->GetDefaultNamespace());
		if (!TestRunner->TestTrue(TEXT("Subsystem helper namespace should be selectable"), ScriptEngine->SetDefaultNamespace("USubsystemLibrary") >= 0))
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetDefaultNamespace(TCHAR_TO_ANSI(*PreviousNamespace));
		};

		const TArray<const ANSICHAR*> ExpectedNames = {
			"GetEngineSubsystem",
			"GetGameInstanceSubsystem",
			"GetLocalPlayerSubsystem",
			"GetWorldSubsystem",
			"GetLocalPlayerSubsystemFromLocalPlayer",
			"GetLocalPlayerSubsystemFromPlayerController"
		};

		for (const ANSICHAR* ExpectedName : ExpectedNames)
		{
			bool bFound = false;
			for (asUINT FunctionIndex = 0, FunctionCount = ScriptEngine->GetGlobalFunctionCount(); FunctionIndex < FunctionCount; ++FunctionIndex)
			{
				asIScriptFunction* Function = ScriptEngine->GetGlobalFunctionByIndex(FunctionIndex);
				if (Function != nullptr
					&& FCStringAnsi::Strcmp(Function->GetNamespace(), "USubsystemLibrary") == 0
					&& FCStringAnsi::Strcmp(Function->GetName(), ExpectedName) == 0)
				{
					bFound = true;
					break;
				}
			}

			TestRunner->TestTrue(
				FString::Printf(TEXT("Subsystem helper namespace should bind %hs"), ExpectedName),
				bFound);
		}

		for (asUINT FunctionIndex = 0, FunctionCount = ScriptEngine->GetGlobalFunctionCount(); FunctionIndex < FunctionCount; ++FunctionIndex)
		{
			asIScriptFunction* Function = ScriptEngine->GetGlobalFunctionByIndex(FunctionIndex);
			if (Function != nullptr
				&& FCStringAnsi::Strcmp(Function->GetNamespace(), "Subsystem") == 0)
			{
				TestRunner->AddError(FString::Printf(
					TEXT("Subsystem helper namespace should not leave old Subsystem::%hs binding"),
					Function->GetName()));
			}
		}
	}

	TEST_METHOD(MathReturnValueHelperMetadata)
	{
		SubsystemGetterMetadataTest::ResetIsolatedEnvironment();
		ON_SCOPE_EXIT
		{
			SubsystemGetterMetadataTest::ResetIsolatedEnvironment();
		};

		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeFullEngineForTesting(FAngelscriptEngineConfig(), Dependencies);
		if (!TestRunner->TestTrue(TEXT("MathReturnValueHelperMetadata should create a testing engine"), Engine.IsValid()))
		{
			return;
		}

		FAngelscriptEngineScope EngineScope(*Engine);

		TSharedPtr<FAngelscriptType> HostType = FAngelscriptType::GetByClass(UAngelscriptMathLibrary::StaticClass());
		if (!TestRunner->TestTrue(TEXT("MathReturnValueHelperMetadata should resolve the math library host type"), HostType.IsValid()))
		{
			return;
		}

		const TArray<MathReturnValueHelperMetadataTest::FMathHelperExpectation> Expectations = {
			{ TEXT("LerpShortestPath"), TEXT("LerpShortestPath"), TEXT("Math") },
			{ TEXT("RInterpShortestPathTo"), TEXT("RInterpShortestPathTo"), TEXT("Math") },
			{ TEXT("RInterpConstantShortestPathTo"), TEXT("RInterpConstantShortestPathTo"), TEXT("Math") },
			{ TEXT("TInterpTo"), TEXT("TInterpTo"), TEXT("Math") },
			{ TEXT("Modf_32"), TEXT("Modf"), TEXT("Math") },
			{ TEXT("Modf_64"), TEXT("Modf"), TEXT("Math") },
			{ TEXT("WrapDouble"), TEXT("Wrap"), TEXT("Math") },
			{ TEXT("WrapFloat"), TEXT("Wrap"), TEXT("Math") },
			{ TEXT("WrapInt"), TEXT("Wrap"), TEXT("Math") },
			{ TEXT("WrapIndex"), TEXT("WrapIndex"), TEXT("Math") }
		};

		for (const MathReturnValueHelperMetadataTest::FMathHelperExpectation& Expectation : Expectations)
		{
			MathReturnValueHelperMetadataTest::CheckMathHelperSignature(
				*TestRunner,
				HostType.ToSharedRef(),
				Expectation);
		}
	}

	TEST_METHOD(ProductionScriptMixinSignatures)
	{
		SubsystemGetterMetadataTest::ResetIsolatedEnvironment();
		ON_SCOPE_EXIT
		{
			SubsystemGetterMetadataTest::ResetIsolatedEnvironment();
		};

		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeFullEngineForTesting(FAngelscriptEngineConfig(), Dependencies);
		if (!TestRunner->TestTrue(TEXT("ProductionScriptMixinSignatures should create a testing engine"), Engine.IsValid()))
		{
			return;
		}

		FAngelscriptEngineScope EngineScope(*Engine);

		const TArray<ProductionScriptMixinSignatureTest::FProductionScriptMixinExpectation> Expectations = {
			{
				UAngelscriptFrameTimeMixinLibrary::StaticClass(),
				TEXT("AsSeconds"),
				TEXT("FQualifiedFrameTime"),
				0,
				TEXT("AsSeconds"),
				nullptr,
				nullptr,
				true
			},
			{
				UAngelscriptWidgetMixinLibrary::StaticClass(),
				TEXT("GetRenderTransform"),
				TEXT("UWidget"),
				0,
				TEXT("GetRenderTransform"),
				nullptr,
				TEXT("WorldContextObject"),
				false
			},
			{
				UGameplayTagQueryMixinLibrary::StaticClass(),
				TEXT("Matches"),
				TEXT("FGameplayTagQuery"),
				1,
				TEXT("Matches"),
				TEXT("FGameplayTagContainer"),
				nullptr,
				true
			}
		};

		for (const ProductionScriptMixinSignatureTest::FProductionScriptMixinExpectation& Expectation : Expectations)
		{
			ProductionScriptMixinSignatureTest::CheckProductionScriptMixinSignature(
				*TestRunner,
				Expectation);
		}
	}
};

#endif
