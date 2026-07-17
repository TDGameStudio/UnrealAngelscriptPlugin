#include "Core/AngelscriptBinds.h"
#include "Core/AngelscriptEngine.h"
#include "Binds/Helper_FunctionSignature.h"
#include "FunctionLibraries/AngelscriptFrameTimeMixinLibrary.h"
#include "FunctionLibraries/AngelscriptMathLibrary.h"
#include "FunctionLibraries/SubsystemLibrary.h"
#include "FunctionLibraries/WidgetBlueprintStatics.h"
#include "AngelscriptTestUtilities.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

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
		FNoDiscardAsserter LocalAssert(Test);
		UFunction* Function = USubsystemLibrary::StaticClass()->FindFunctionByName(Expectation.FunctionName);
		if (!LocalAssert.IsNotNull(
				Function,
				FString::Printf(TEXT("SubsystemGetterMetadata should find reflected function %s"), Expectation.FunctionName)))
		{
			return false;
		}

		FAngelscriptFunctionSignature Signature(HostType, Function);
		const int FunctionId = FAngelscriptBinds::BindGlobalGenericFunction(Signature.Declaration, &NoOpGeneric);
		Signature.ModifyScriptFunction(FunctionId);

		auto* ScriptFunction = reinterpret_cast<asCScriptFunction*>(FAngelscriptEngine::Get().GetScriptEngine()->GetFunctionById(FunctionId));
		if (!LocalAssert.IsNotNull(
				ScriptFunction,
				FString::Printf(TEXT("SubsystemGetterMetadata should create script function %s"), Expectation.FunctionName)))
		{
			return false;
		}

		bool bPassed = true;
		bPassed &= LocalAssert.AreEqual(
			FString(Expectation.ExpectedNamespace),
			Signature.ClassName,
			FString::Printf(TEXT("%s should bind under the full subsystem library namespace"), Expectation.FunctionName));
		bPassed &= LocalAssert.IsTrue(
			Signature.bStaticInScript,
			FString::Printf(TEXT("%s should remain a static script function"), Expectation.FunctionName));
		bPassed &= LocalAssert.IsTrue(
			!Signature.Declaration.IsEmpty(),
			FString::Printf(TEXT("%s should keep the generated declaration non-empty"), Expectation.FunctionName));
		bPassed &= LocalAssert.IsTrue(
			Signature.Declaration.Contains(Expectation.FunctionName),
			FString::Printf(TEXT("%s should preserve the original function name in the declaration"), Expectation.FunctionName));
		bPassed &= LocalAssert.IsTrue(
			Signature.Declaration.Contains(TEXT("no_discard")),
			FString::Printf(TEXT("%s should append no_discard to the declaration"), Expectation.FunctionName));
		bPassed &= LocalAssert.AreEqual(
			Expectation.ExpectedHiddenArgumentIndex,
			static_cast<int32>(ScriptFunction->hiddenArgumentIndex),
			FString::Printf(TEXT("%s should set the expected hidden world-context argument index"), Expectation.FunctionName));
		bPassed &= LocalAssert.AreEqual(
			Expectation.bExpectWorldContextTrait,
			ScriptFunction->traits.GetTrait(asTRAIT_USES_WORLDCONTEXT),
			FString::Printf(TEXT("%s should set the expected world-context trait"), Expectation.FunctionName));

		if (Expectation.RequiredDeclarationFragment != nullptr)
		{
			bPassed &= LocalAssert.IsTrue(
				Signature.Declaration.Contains(Expectation.RequiredDeclarationFragment),
				FString::Printf(TEXT("%s should preserve declaration fragment %s"), Expectation.FunctionName, Expectation.RequiredDeclarationFragment));
		}

		if (Expectation.bExpectHiddenWorldContext)
		{
			bPassed &= LocalAssert.AreEqual(
				Expectation.ExpectedHiddenArgumentIndex,
				static_cast<int32>(Signature.WorldContextArgument),
				FString::Printf(TEXT("%s should record the same world-context argument before script-function mutation"), Expectation.FunctionName));
		}
		else
		{
			bPassed &= LocalAssert.AreEqual(
				-1,
				static_cast<int32>(Signature.WorldContextArgument),
				FString::Printf(TEXT("%s should not record a hidden world-context argument in the signature"), Expectation.FunctionName));
		}

		return bPassed;
	}
}

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
		FNoDiscardAsserter LocalAssert(Test);
		UFunction* Function = UAngelscriptMathLibrary::StaticClass()->FindFunctionByName(Expectation.FunctionName);
		if (!LocalAssert.IsNotNull(
				Function,
				FString::Printf(TEXT("MathReturnValueHelperMetadata should find reflected function %s"), Expectation.FunctionName)))
		{
			return false;
		}

		FAngelscriptFunctionSignature Signature(HostType, Function);
		const int FunctionId = FAngelscriptBinds::BindGlobalGenericFunction(Signature.Declaration, &SubsystemGetterMetadataTest::NoOpGeneric);
		Signature.ModifyScriptFunction(FunctionId);

		auto* ScriptFunction = reinterpret_cast<asCScriptFunction*>(FAngelscriptEngine::Get().GetScriptEngine()->GetFunctionById(FunctionId));
		if (!LocalAssert.IsNotNull(
				ScriptFunction,
				FString::Printf(TEXT("MathReturnValueHelperMetadata should create script function %s"), Expectation.FunctionName)))
		{
			return false;
		}

		bool bPassed = true;
		bPassed &= LocalAssert.AreEqual(
			FString(Expectation.ExpectedNamespace),
			Signature.ClassName,
			FString::Printf(TEXT("%s should bind under the full math library namespace"), Expectation.FunctionName));
		bPassed &= LocalAssert.IsTrue(
			Signature.bStaticInScript,
			FString::Printf(TEXT("%s should remain a static script function"), Expectation.FunctionName));
		bPassed &= LocalAssert.AreEqual(
			FString(Expectation.ExpectedScriptName),
			Signature.ScriptName,
			FString::Printf(TEXT("%s should expose the expected script alias"), Expectation.FunctionName));
		bPassed &= LocalAssert.IsTrue(
			Signature.Declaration.Contains(TEXT("no_discard")),
			FString::Printf(TEXT("%s should append no_discard to the declaration"), Expectation.FunctionName));
		bPassed &= LocalAssert.IsTrue(
			Signature.Declaration.Contains(Expectation.ExpectedScriptName),
			FString::Printf(TEXT("%s should keep the expected alias in the declaration"), Expectation.FunctionName));
		bPassed &= LocalAssert.IsTrue(
			Signature.bTrivial,
			FString::Printf(TEXT("%s should remain a trivial bind"), Expectation.FunctionName));
		bPassed &= LocalAssert.AreEqual(
			-1,
			static_cast<int32>(Signature.WorldContextArgument),
			FString::Printf(TEXT("%s should not hide a world-context argument"), Expectation.FunctionName));
		bPassed &= LocalAssert.AreEqual(
			-1,
			static_cast<int32>(Signature.DeterminesOutputTypeArgument),
			FString::Printf(TEXT("%s should not mark a determines-output-type argument"), Expectation.FunctionName));
		bPassed &= LocalAssert.AreEqual(
			-1,
			static_cast<int32>(ScriptFunction->hiddenArgumentIndex),
			FString::Printf(TEXT("%s should keep the script function free of hidden arguments"), Expectation.FunctionName));
		bPassed &= LocalAssert.AreEqual(
			-1,
			static_cast<int32>(ScriptFunction->determinesOutputTypeArgumentIndex),
			FString::Printf(TEXT("%s should keep the script function free of determines-output-type arguments"), Expectation.FunctionName));
		if (FCString::Strcmp(Expectation.FunctionName, Expectation.ExpectedScriptName) != 0)
		{
			bPassed &= LocalAssert.IsFalse(
				Signature.Declaration.Contains(Expectation.FunctionName),
				FString::Printf(TEXT("%s should not leak the Unreal-only name into the declaration"), Expectation.FunctionName));
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
		FNoDiscardAsserter LocalAssert(Test);
		UFunction* Function = Expectation.FunctionLibraryClass->FindFunctionByName(Expectation.FunctionName);
		if (!LocalAssert.IsNotNull(
				Function,
				FString::Printf(TEXT("ProductionScriptMixinSignatures should find reflected function %s"), Expectation.FunctionName)))
		{
			return false;
		}

		TSharedPtr<FAngelscriptType> HostType = ResolveHostTypeFromFirstParameter(Function);
		if (!LocalAssert.IsTrue(
				HostType.IsValid(),
				FString::Printf(TEXT("ProductionScriptMixinSignatures should resolve the host type for %s from its first parameter"), Expectation.FunctionName)))
		{
			return false;
		}

		const FString InspectName = FString::Printf(TEXT("%s_ProductionScriptMixinInspection"), Expectation.FunctionName);
		FAngelscriptFunctionSignature Signature(HostType.ToSharedRef(), Function, *InspectName);
		const int32 FunctionId = BindSignatureForInspection(Signature);
		Signature.ModifyScriptFunction(FunctionId);

		auto* ScriptFunction = reinterpret_cast<asCScriptFunction*>(FAngelscriptEngine::Get().GetScriptEngine()->GetFunctionById(FunctionId));
		if (!LocalAssert.IsNotNull(
				ScriptFunction,
				FString::Printf(TEXT("ProductionScriptMixinSignatures should create a script function for %s"), Expectation.FunctionName)))
		{
			return false;
		}

		const FString ScriptDeclaration = ANSI_TO_TCHAR(ScriptFunction->GetDeclaration(true, false, true, true));

		bool bPassed = true;
		bPassed &= LocalAssert.AreEqual(
			FString(Expectation.ExpectedClassName),
			HostType->GetAngelscriptTypeName(),
			FString::Printf(TEXT("%s should resolve the expected host type name"), Expectation.FunctionName));
		bPassed &= LocalAssert.IsTrue(
			Signature.bStaticInUnreal,
			FString::Printf(TEXT("%s should keep the Unreal function static"), Expectation.FunctionName));
		bPassed &= LocalAssert.IsFalse(
			Signature.bStaticInScript,
			FString::Printf(TEXT("%s should bind production ScriptMixin functions as script members"), Expectation.FunctionName));
		bPassed &= LocalAssert.AreEqual(
			FString(Expectation.ExpectedClassName),
			Signature.ClassName,
			FString::Printf(TEXT("%s should expose the expected script member owner"), Expectation.FunctionName));
		bPassed &= LocalAssert.AreEqual(
			Expectation.ExpectedPublicArgumentCount,
			Signature.ArgumentTypes.Num(),
			FString::Printf(TEXT("%s should expose the expected number of public parameters in the signature"), Expectation.FunctionName));
		bPassed &= LocalAssert.AreEqual(
			Expectation.ExpectedPublicArgumentCount,
			static_cast<int32>(ScriptFunction->GetParamCount()),
			FString::Printf(TEXT("%s should expose the expected number of public parameters in the script function"), Expectation.FunctionName));
		bPassed &= LocalAssert.AreEqual(
			-1,
			static_cast<int32>(Signature.WorldContextArgument),
			FString::Printf(TEXT("%s should not leave a hidden world-context argument in the signature"), Expectation.FunctionName));
		bPassed &= LocalAssert.AreEqual(
			-1,
			static_cast<int32>(ScriptFunction->hiddenArgumentIndex),
			FString::Printf(TEXT("%s should not hide a world-context argument on the script function"), Expectation.FunctionName));
		bPassed &= LocalAssert.IsFalse(
			ScriptFunction->traits.GetTrait(asTRAIT_USES_WORLDCONTEXT),
			FString::Printf(TEXT("%s should not mark the script function with the world-context trait"), Expectation.FunctionName));

		if (Expectation.RequiredDeclarationFragment != nullptr)
		{
			bPassed &= LocalAssert.IsTrue(
				Signature.Declaration.Contains(Expectation.RequiredDeclarationFragment),
				FString::Printf(TEXT("%s should preserve declaration fragment %s in the generated signature"), Expectation.FunctionName, Expectation.RequiredDeclarationFragment));
			bPassed &= LocalAssert.IsTrue(
				ScriptDeclaration.Contains(Expectation.RequiredDeclarationFragment),
				FString::Printf(TEXT("%s should preserve declaration fragment %s on the script function"), Expectation.FunctionName, Expectation.RequiredDeclarationFragment));
		}

		if (Expectation.RequiredArgumentTypeFragment != nullptr)
		{
			bPassed &= LocalAssert.AreEqual(
				1,
				Signature.ArgumentTypes.Num(),
				FString::Printf(TEXT("%s should expose exactly one explicit argument before checking its type"), Expectation.FunctionName));
			if (Signature.ArgumentTypes.Num() == 1)
			{
				const FString ExposedArgumentType = Signature.ArgumentTypes[0].GetAngelscriptDeclaration();
				bPassed &= LocalAssert.IsTrue(
					ExposedArgumentType.Contains(Expectation.RequiredArgumentTypeFragment),
					FString::Printf(TEXT("%s should expose the expected explicit argument type"), Expectation.FunctionName));
				bPassed &= LocalAssert.IsTrue(
					Signature.ArgumentNames[0] == TEXT("Tags"),
					FString::Printf(TEXT("%s should preserve the explicit argument name after mixin trimming"), Expectation.FunctionName));
			}
		}

		if (Expectation.ForbiddenDeclarationFragment != nullptr)
		{
			bPassed &= LocalAssert.IsFalse(
				Signature.Declaration.Contains(Expectation.ForbiddenDeclarationFragment),
				FString::Printf(TEXT("%s should not leak declaration fragment %s into the generated signature"), Expectation.FunctionName, Expectation.ForbiddenDeclarationFragment));
			bPassed &= LocalAssert.IsFalse(
				ScriptDeclaration.Contains(Expectation.ForbiddenDeclarationFragment),
				FString::Printf(TEXT("%s should not leak declaration fragment %s into the script function"), Expectation.FunctionName, Expectation.ForbiddenDeclarationFragment));
		}

		if (Expectation.bExpectConstMethod)
		{
			bPassed &= LocalAssert.IsTrue(
				Signature.Declaration.Contains(TEXT("const")),
				FString::Printf(TEXT("%s should generate a const member declaration"), Expectation.FunctionName));
			bPassed &= LocalAssert.IsTrue(
				ScriptDeclaration.Contains(TEXT("const")),
				FString::Printf(TEXT("%s should keep the script declaration const"), Expectation.FunctionName));
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
		if (!this->Assert.IsTrue(Engine.IsValid(), TEXT("SubsystemGetterMetadata should create a testing engine")))
		{
			return;
		}

		FAngelscriptEngineScope EngineScope(*Engine);

		TSharedPtr<FAngelscriptType> HostType = FAngelscriptType::GetByClass(USubsystemLibrary::StaticClass());
		if (!this->Assert.IsTrue(HostType.IsValid(), TEXT("SubsystemGetterMetadata should resolve the subsystem library host type")))
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
		if (!this->Assert.IsTrue(Engine.IsValid(), TEXT("SubsystemHelperNamespaceBinds should create a testing engine")))
		{
			return;
		}

		FAngelscriptEngineScope EngineScope(*Engine);
		asIScriptEngine* ScriptEngine = Engine->GetScriptEngine();
		if (!this->Assert.IsNotNull(ScriptEngine, TEXT("SubsystemHelperNamespaceBinds should expose the AS engine")))
		{
			return;
		}

		const FString PreviousNamespace = ANSI_TO_TCHAR(ScriptEngine->GetDefaultNamespace());
		if (!this->Assert.IsTrue(ScriptEngine->SetDefaultNamespace("USubsystemLibrary") >= 0, TEXT("Subsystem helper namespace should be selectable")))
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

			(void)this->Assert.IsTrue(
				bFound,
				FString::Printf(TEXT("Subsystem helper namespace should bind %hs"), ExpectedName));
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
		if (!this->Assert.IsTrue(Engine.IsValid(), TEXT("MathReturnValueHelperMetadata should create a testing engine")))
		{
			return;
		}

		FAngelscriptEngineScope EngineScope(*Engine);

		TSharedPtr<FAngelscriptType> HostType = FAngelscriptType::GetByClass(UAngelscriptMathLibrary::StaticClass());
		if (!this->Assert.IsTrue(HostType.IsValid(), TEXT("MathReturnValueHelperMetadata should resolve the math library host type")))
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

	TEST_METHOD(WidgetBlueprintCreateWidgetMetadata)
	{
		SubsystemGetterMetadataTest::ResetIsolatedEnvironment();
		ON_SCOPE_EXIT
		{
			SubsystemGetterMetadataTest::ResetIsolatedEnvironment();
		};

		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeFullEngineForTesting(FAngelscriptEngineConfig(), Dependencies);
		if (!this->Assert.IsTrue(Engine.IsValid(), TEXT("WidgetBlueprintCreateWidgetMetadata should create a testing engine")))
		{
			return;
		}

		FAngelscriptEngineScope EngineScope(*Engine);
		UFunction* Function = UWidgetBlueprintStatics::StaticClass()->FindFunctionByName(TEXT("CreateWidget"));
		if (!this->Assert.IsNotNull(Function, TEXT("WidgetBlueprintCreateWidgetMetadata should find CreateWidget")))
		{
			return;
		}

		TSharedPtr<FAngelscriptType> HostType = FAngelscriptType::GetByClass(UWidgetBlueprintStatics::StaticClass());
		if (!this->Assert.IsTrue(HostType.IsValid(), TEXT("WidgetBlueprintCreateWidgetMetadata should resolve the widget library type")))
		{
			return;
		}

		FAngelscriptFunctionSignature Signature(HostType.ToSharedRef(), Function);
		const int FunctionId = FAngelscriptBinds::BindGlobalGenericFunction(Signature.Declaration, &SubsystemGetterMetadataTest::NoOpGeneric);
		Signature.ModifyScriptFunction(FunctionId);

		auto* ScriptFunction = reinterpret_cast<asCScriptFunction*>(Engine->GetScriptEngine()->GetFunctionById(FunctionId));
		if (!this->Assert.IsNotNull(ScriptFunction, TEXT("WidgetBlueprintCreateWidgetMetadata should create a script function")))
		{
			return;
		}

		(void)this->Assert.AreEqual(1, static_cast<int32>(Signature.DeterminesOutputTypeArgument), TEXT("CreateWidget should determine output type from WidgetType"));
		(void)this->Assert.AreEqual(1, static_cast<int32>(ScriptFunction->determinesOutputTypeArgumentIndex), TEXT("CreateWidget script function should retain determined output type metadata"));
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
		if (!this->Assert.IsTrue(Engine.IsValid(), TEXT("ProductionScriptMixinSignatures should create a testing engine")))
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
