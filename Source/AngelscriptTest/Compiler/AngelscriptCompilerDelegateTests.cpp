#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "Core/AngelscriptEngine.h"
#include "Core/AngelscriptType.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CompilerDelegateMetadataTest
{
	static const FName ModuleName(TEXT("CompilerDelegateSignatureMetadataRoundTrip"));
	static const FString ScriptFilename(TEXT("CompilerDelegateSignatureMetadataRoundTrip.as"));
	static const FString SingleDelegateName(TEXT("FCompilerSingleMetadataRoundTrip"));
	static const FString MultiDelegateName(TEXT("FCompilerMultiMetadataRoundTrip"));

	enum class EExpectedPropertyKind : uint8
	{
		Int,
		String,
		Class,
	};

	struct FExpectedArgument
	{
		FName Name;
		FString TypeDeclaration;
		EExpectedPropertyKind PropertyKind;
		UClass* ExpectedMetaClass = nullptr;
	};

	static TArray<FProperty*> GetOrderedParameterProperties(UFunction* Function)
	{
		TArray<FProperty*> ParameterProperties;
		if (Function == nullptr)
		{
			return ParameterProperties;
		}

		for (TFieldIterator<FProperty> It(Function); It; ++It)
		{
			FProperty* Property = *It;
			if (Property == nullptr)
			{
				continue;
			}

			if (Property->HasAnyPropertyFlags(CPF_Parm) && !Property->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				ParameterProperties.Add(Property);
			}
		}

		ParameterProperties.Sort([](const FProperty& Left, const FProperty& Right)
		{
			return Left.GetOffset_ForUFunction() < Right.GetOffset_ForUFunction();
		});

		return ParameterProperties;
	}

	static bool VerifyReflectedPropertyKind(
		FAutomationTestBase& Test,
		const FString& Context,
		FProperty* Property,
		const FExpectedArgument& Expected)
	{
		FNoDiscardAsserter LocalAssert(Test);
		switch (Expected.PropertyKind)
		{
		case EExpectedPropertyKind::Int:
			return LocalAssert.IsNotNull(CastField<FIntProperty>(Property), *FString::Printf(TEXT("%s should materialize '%s' as FIntProperty"), *Context, *Expected.Name.ToString()));

		case EExpectedPropertyKind::String:
			return LocalAssert.IsNotNull(CastField<FStrProperty>(Property), *FString::Printf(TEXT("%s should materialize '%s' as FStrProperty"), *Context, *Expected.Name.ToString()));

		case EExpectedPropertyKind::Class:
		{
			FClassProperty* ClassProperty = CastField<FClassProperty>(Property);
			const bool bHasClassProperty = LocalAssert.IsNotNull(
				ClassProperty,
				*FString::Printf(TEXT("%s should materialize '%s' as FClassProperty"), *Context, *Expected.Name.ToString()));
			if (!bHasClassProperty || Expected.ExpectedMetaClass == nullptr)
			{
				return bHasClassProperty;
			}

			return LocalAssert.IsTrue(
				ClassProperty->MetaClass == Expected.ExpectedMetaClass,
				*FString::Printf(TEXT("%s should materialize '%s' with the expected MetaClass"), *Context, *Expected.Name.ToString()));
		}
		}

		return false;
	}

	static bool VerifyDelegateMetadata(
		FAutomationTestBase& Test,
		const FString& Context,
		const TSharedPtr<FAngelscriptDelegateDesc>& DelegateDesc,
		const bool bExpectedMulticast,
		const TArray<FExpectedArgument>& ExpectedArguments)
	{
		bool bPassed = true;
		FNoDiscardAsserter LocalAssert(Test);

		bPassed &= LocalAssert.IsTrue(
			DelegateDesc.IsValid(),
			*FString::Printf(TEXT("%s should expose delegate metadata"), *Context));
		if (!DelegateDesc.IsValid())
		{
			return false;
		}

		bPassed &= LocalAssert.AreEqual(
			bExpectedMulticast,
			DelegateDesc->bIsMulticast,
			*FString::Printf(TEXT("%s should preserve the multicast flag"), *Context));
		bPassed &= LocalAssert.IsTrue(
			DelegateDesc->Signature.IsValid(),
			*FString::Printf(TEXT("%s should keep a signature description"), *Context));
		bPassed &= LocalAssert.IsNotNull(
			DelegateDesc->Function,
			*FString::Printf(TEXT("%s should materialize a UDelegateFunction"), *Context));
		if (!DelegateDesc->Signature.IsValid() || DelegateDesc->Function == nullptr)
		{
			return false;
		}

		bPassed &= LocalAssert.AreEqual(
			FString(TEXT("void")),
			DelegateDesc->Signature->ReturnType.GetAngelscriptDeclaration(),
			*FString::Printf(TEXT("%s should preserve a void return type in the signature description"), *Context));
		bPassed &= LocalAssert.IsNull(
			DelegateDesc->Function->GetReturnProperty(),
			*FString::Printf(TEXT("%s should not generate a reflected return property for void"), *Context));
		bPassed &= LocalAssert.AreEqual(
			ExpectedArguments.Num(),
			DelegateDesc->Signature->Arguments.Num(),
			*FString::Printf(TEXT("%s should preserve the expected argument count in the signature description"), *Context));

		TArray<FProperty*> ParameterProperties = GetOrderedParameterProperties(DelegateDesc->Function);
		bPassed &= LocalAssert.AreEqual(
			ExpectedArguments.Num(),
			ParameterProperties.Num(),
			*FString::Printf(TEXT("%s should materialize the expected reflected parameter count"), *Context));
		if (DelegateDesc->Signature->Arguments.Num() != ExpectedArguments.Num() || ParameterProperties.Num() != ExpectedArguments.Num())
		{
			return false;
		}

		for (int32 ArgumentIndex = 0; ArgumentIndex < ExpectedArguments.Num(); ++ArgumentIndex)
		{
			const FExpectedArgument& Expected = ExpectedArguments[ArgumentIndex];
			const FAngelscriptArgumentDesc& ActualArgument = DelegateDesc->Signature->Arguments[ArgumentIndex];
			FProperty* ParameterProperty = ParameterProperties[ArgumentIndex];

			bPassed &= LocalAssert.AreEqual(
				Expected.Name,
				FName(*ActualArgument.ArgumentName),
				*FString::Printf(TEXT("%s should preserve signature argument %d name"), *Context, ArgumentIndex));
			bPassed &= LocalAssert.AreEqual(
				Expected.TypeDeclaration,
				ActualArgument.Type.GetAngelscriptDeclaration(),
				*FString::Printf(TEXT("%s should preserve signature argument %d type"), *Context, ArgumentIndex));
			bPassed &= LocalAssert.IsNotNull(
				ParameterProperty,
				*FString::Printf(TEXT("%s should expose reflected parameter %d"), *Context, ArgumentIndex));
			if (ParameterProperty == nullptr)
			{
				continue;
			}

			bPassed &= LocalAssert.AreEqual(
				Expected.Name,
				ParameterProperty->GetFName(),
				*FString::Printf(TEXT("%s should preserve reflected parameter %d name order"), *Context, ArgumentIndex));
			bPassed &= LocalAssert.IsTrue(
				ActualArgument.Type.MatchesProperty(ParameterProperty, FAngelscriptType::EPropertyMatchType::OverrideArgument),
				*FString::Printf(TEXT("%s should keep signature argument '%s' compatible with the reflected property"), *Context, *Expected.Name.ToString()));
			bPassed &= VerifyReflectedPropertyKind(Test, Context, ParameterProperty, Expected);
		}

		return bPassed;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCompilerDelegateTests,
	"Angelscript.TestModule.Compiler.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(DelegateSignatureMetadataRoundTrip)
	{


		const FString ScriptSource = FString::Printf(TEXT(R"AS(
	delegate void %s(int Value);
	event void %s(UClass TypeValue, FString Label);

	UCLASS()
	class UCompilerDelegateMetadataCarrier : UObject
	{
	}
	)AS"), *CompilerDelegateMetadataTest::SingleDelegateName, *CompilerDelegateMetadataTest::MultiDelegateName);

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*CompilerDelegateMetadataTest::ModuleName.ToString());
		};

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			CompilerDelegateMetadataTest::ModuleName,
			CompilerDelegateMetadataTest::ScriptFilename,
			ScriptSource,
			true,
			Summary);

		ASSERT_THAT(IsTrue(bCompiled, TEXT("Delegate signature metadata round-trip input should compile")));
		ASSERT_THAT(IsTrue(Summary.bUsedPreprocessor, TEXT("Delegate signature metadata round-trip input should go through the preprocessor")));
		ASSERT_THAT(AreEqual(ECompileResult::FullyHandled, Summary.CompileResult, TEXT("Delegate signature metadata round-trip input should finish with a fully handled compile result")));
		ASSERT_THAT(AreEqual(0, Summary.Diagnostics.Num(), TEXT("Delegate signature metadata round-trip input should not emit diagnostics")));
		if (!bCompiled)
		{
			return;
		}

		const TSharedPtr<FAngelscriptDelegateDesc> SingleDelegate = Engine.GetDelegate(CompilerDelegateMetadataTest::SingleDelegateName);
		const TSharedPtr<FAngelscriptDelegateDesc> MultiDelegate = Engine.GetDelegate(CompilerDelegateMetadataTest::MultiDelegateName);

		const TArray<CompilerDelegateMetadataTest::FExpectedArgument> SingleArguments = {
			{ TEXT("Value"), TEXT("const int"), CompilerDelegateMetadataTest::EExpectedPropertyKind::Int, nullptr }
		};
		const TArray<CompilerDelegateMetadataTest::FExpectedArgument> MultiArguments = {
			{ TEXT("TypeValue"), TEXT("UClass"), CompilerDelegateMetadataTest::EExpectedPropertyKind::Class, UObject::StaticClass() },
			{ TEXT("Label"), TEXT("const FString&"), CompilerDelegateMetadataTest::EExpectedPropertyKind::String, nullptr }
		};

		CompilerDelegateMetadataTest::VerifyDelegateMetadata(
			*TestRunner,
			TEXT("Single-cast delegate signature metadata round-trip"),
			SingleDelegate,
			false,
			SingleArguments);
		CompilerDelegateMetadataTest::VerifyDelegateMetadata(
			*TestRunner,
			TEXT("Multicast delegate signature metadata round-trip"),
			MultiDelegate,
			true,
			MultiArguments);

		}
	}

};

#endif
