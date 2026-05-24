#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "Core/AngelscriptEngine.h"
#include "Core/AngelscriptType.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CompilerPipelineDelegateMetadataTest
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
		switch (Expected.PropertyKind)
		{
		case EExpectedPropertyKind::Int:
			return Test.TestNotNull(*FString::Printf(TEXT("%s should materialize '%s' as FIntProperty"), *Context, *Expected.Name.ToString()), CastField<FIntProperty>(Property));

		case EExpectedPropertyKind::String:
			return Test.TestNotNull(*FString::Printf(TEXT("%s should materialize '%s' as FStrProperty"), *Context, *Expected.Name.ToString()), CastField<FStrProperty>(Property));

		case EExpectedPropertyKind::Class:
		{
			FClassProperty* ClassProperty = CastField<FClassProperty>(Property);
			const bool bHasClassProperty = Test.TestNotNull(
				*FString::Printf(TEXT("%s should materialize '%s' as FClassProperty"), *Context, *Expected.Name.ToString()),
				ClassProperty);
			if (!bHasClassProperty || Expected.ExpectedMetaClass == nullptr)
			{
				return bHasClassProperty;
			}

			return Test.TestTrue(
				*FString::Printf(TEXT("%s should materialize '%s' with the expected MetaClass"), *Context, *Expected.Name.ToString()),
				ClassProperty->MetaClass == Expected.ExpectedMetaClass);
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

		bPassed &= Test.TestTrue(
			*FString::Printf(TEXT("%s should expose delegate metadata"), *Context),
			DelegateDesc.IsValid());
		if (!DelegateDesc.IsValid())
		{
			return false;
		}

		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("%s should preserve the multicast flag"), *Context),
			DelegateDesc->bIsMulticast,
			bExpectedMulticast);
		bPassed &= Test.TestTrue(
			*FString::Printf(TEXT("%s should keep a signature description"), *Context),
			DelegateDesc->Signature.IsValid());
		bPassed &= Test.TestNotNull(
			*FString::Printf(TEXT("%s should materialize a UDelegateFunction"), *Context),
			DelegateDesc->Function);
		if (!DelegateDesc->Signature.IsValid() || DelegateDesc->Function == nullptr)
		{
			return false;
		}

		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("%s should preserve a void return type in the signature description"), *Context),
			DelegateDesc->Signature->ReturnType.GetAngelscriptDeclaration(),
			FString(TEXT("void")));
		bPassed &= Test.TestNull(
			*FString::Printf(TEXT("%s should not generate a reflected return property for void"), *Context),
			DelegateDesc->Function->GetReturnProperty());
		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("%s should preserve the expected argument count in the signature description"), *Context),
			DelegateDesc->Signature->Arguments.Num(),
			ExpectedArguments.Num());

		TArray<FProperty*> ParameterProperties = GetOrderedParameterProperties(DelegateDesc->Function);
		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("%s should materialize the expected reflected parameter count"), *Context),
			ParameterProperties.Num(),
			ExpectedArguments.Num());
		if (DelegateDesc->Signature->Arguments.Num() != ExpectedArguments.Num() || ParameterProperties.Num() != ExpectedArguments.Num())
		{
			return false;
		}

		for (int32 ArgumentIndex = 0; ArgumentIndex < ExpectedArguments.Num(); ++ArgumentIndex)
		{
			const FExpectedArgument& Expected = ExpectedArguments[ArgumentIndex];
			const FAngelscriptArgumentDesc& ActualArgument = DelegateDesc->Signature->Arguments[ArgumentIndex];
			FProperty* ParameterProperty = ParameterProperties[ArgumentIndex];

			bPassed &= Test.TestEqual(
				*FString::Printf(TEXT("%s should preserve signature argument %d name"), *Context, ArgumentIndex),
				FName(*ActualArgument.ArgumentName),
				Expected.Name);
			bPassed &= Test.TestEqual(
				*FString::Printf(TEXT("%s should preserve signature argument %d type"), *Context, ArgumentIndex),
				ActualArgument.Type.GetAngelscriptDeclaration(),
				Expected.TypeDeclaration);
			bPassed &= Test.TestNotNull(
				*FString::Printf(TEXT("%s should expose reflected parameter %d"), *Context, ArgumentIndex),
				ParameterProperty);
			if (ParameterProperty == nullptr)
			{
				continue;
			}

			bPassed &= Test.TestEqual(
				*FString::Printf(TEXT("%s should preserve reflected parameter %d name order"), *Context, ArgumentIndex),
				ParameterProperty->GetFName(),
				Expected.Name);
			bPassed &= Test.TestTrue(
				*FString::Printf(TEXT("%s should keep signature argument '%s' compatible with the reflected property"), *Context, *Expected.Name.ToString()),
				ActualArgument.Type.MatchesProperty(ParameterProperty, FAngelscriptType::EPropertyMatchType::OverrideArgument));
			bPassed &= VerifyReflectedPropertyKind(Test, Context, ParameterProperty, Expected);
		}

		return bPassed;
	}
}

using namespace CompilerPipelineDelegateMetadataTest;

TEST_CLASS_WITH_FLAGS(FCompilerPipelineDelegateTests,
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
	)AS"), *CompilerPipelineDelegateMetadataTest::SingleDelegateName, *CompilerPipelineDelegateMetadataTest::MultiDelegateName);

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*CompilerPipelineDelegateMetadataTest::ModuleName.ToString());
		};

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			CompilerPipelineDelegateMetadataTest::ModuleName,
			CompilerPipelineDelegateMetadataTest::ScriptFilename,
			ScriptSource,
			true,
			Summary);

		TestRunner->TestTrue(TEXT("Delegate signature metadata round-trip input should compile"), bCompiled);
		TestRunner->TestTrue(TEXT("Delegate signature metadata round-trip input should go through the preprocessor"), Summary.bUsedPreprocessor);
		TestRunner->TestEqual(TEXT("Delegate signature metadata round-trip input should finish with a fully handled compile result"), Summary.CompileResult, ECompileResult::FullyHandled);
		TestRunner->TestEqual(TEXT("Delegate signature metadata round-trip input should not emit diagnostics"), Summary.Diagnostics.Num(), 0);
		if (!bCompiled)
		{
			return;
		}

		const TSharedPtr<FAngelscriptDelegateDesc> SingleDelegate = Engine.GetDelegate(CompilerPipelineDelegateMetadataTest::SingleDelegateName);
		const TSharedPtr<FAngelscriptDelegateDesc> MultiDelegate = Engine.GetDelegate(CompilerPipelineDelegateMetadataTest::MultiDelegateName);

		const TArray<CompilerPipelineDelegateMetadataTest::FExpectedArgument> SingleArguments = {
			{ TEXT("Value"), TEXT("const int"), CompilerPipelineDelegateMetadataTest::EExpectedPropertyKind::Int, nullptr }
		};
		const TArray<CompilerPipelineDelegateMetadataTest::FExpectedArgument> MultiArguments = {
			{ TEXT("TypeValue"), TEXT("UClass"), CompilerPipelineDelegateMetadataTest::EExpectedPropertyKind::Class, UObject::StaticClass() },
			{ TEXT("Label"), TEXT("const FString&"), CompilerPipelineDelegateMetadataTest::EExpectedPropertyKind::String, nullptr }
		};

		CompilerPipelineDelegateMetadataTest::VerifyDelegateMetadata(
			*TestRunner,
			TEXT("Single-cast delegate signature metadata round-trip"),
			SingleDelegate,
			false,
			SingleArguments);
		CompilerPipelineDelegateMetadataTest::VerifyDelegateMetadata(
			*TestRunner,
			TEXT("Multicast delegate signature metadata round-trip"),
			MultiDelegate,
			true,
			MultiArguments);

		}
	}

};

#endif
