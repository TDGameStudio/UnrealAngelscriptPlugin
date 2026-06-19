#include "AngelscriptBindString.h"

#include "CQTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_Core_AngelscriptBindStringTests_Private
{
	bool ExpectBindStringState(
		FAutomationTestBase& Test,
		const TCHAR* Context,
		const FBindString& BindString,
		const bool bExpectedEmpty,
		const TCHAR* ExpectedUnreal,
		const ANSICHAR* ExpectedAnsi)
	{
		FNoDiscardAsserter Assert(Test);
		bool bOk = true;
		bOk &= Assert.AreEqual(
			bExpectedEmpty,
			BindString.IsEmpty(),
			*FString::Printf(TEXT("%s should report the expected empty state"), Context));
		bOk &= Assert.AreEqual(
			FString(ExpectedUnreal),
			BindString.ToFString(),
			*FString::Printf(TEXT("%s should round-trip to FString"), Context));
		bOk &= Assert.AreEqual(
			FString(ExpectedUnreal),
			FString(ANSI_TO_TCHAR(BindString.ToCString())),
			*FString::Printf(TEXT("%s should round-trip to ANSI text"), Context));
		bOk &= Assert.AreEqual(
			0,
			FCStringAnsi::Strcmp(BindString.ToCString(), ExpectedAnsi),
			*FString::Printf(TEXT("%s should preserve the expected ANSI payload"), Context));
		return bOk;
	}
}


TEST_CLASS_WITH_FLAGS(FAngelscriptBindStringTests,
	"Angelscript.TestModule.Engine.BindString",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(EmptyAndRoundTripAcrossConstantDynamicAndUnrealSources)
	{
		using namespace AngelscriptTest_Core_AngelscriptBindStringTests_Private;
		FBindString ConstantEmpty("");
		if (!ExpectBindStringState(*TestRunner, TEXT("BindString constant empty"), ConstantEmpty, true, TEXT(""), ""))
		{
			return;
		}

		if (!this->Assert.AreEqual(
				0,
				FCStringAnsi::Strcmp(ConstantEmpty.ToCString_EnsureConstant(), ""),
				TEXT("BindString constant empty should expose the same constant ANSI pointer content")))
		{
			return;
		}

		FBindString ConstantValue("Constant::Value");
		if (!ExpectBindStringState(*TestRunner, TEXT("BindString constant value"), ConstantValue, false, TEXT("Constant::Value"), "Constant::Value"))
		{
			return;
		}

		if (!this->Assert.AreEqual(
				0,
				FCStringAnsi::Strcmp(ConstantValue.ToCString_EnsureConstant(), "Constant::Value"),
				TEXT("BindString constant value should keep the constant ANSI source available")))
		{
			return;
		}

		FBindString UnrealEmpty{ FString() };
		if (!ExpectBindStringState(*TestRunner, TEXT("BindString FString empty"), UnrealEmpty, true, TEXT(""), ""))
		{
			return;
		}

		FBindString UnrealValue{ FString(TEXT("UnrealValue")) };
		if (!ExpectBindStringState(*TestRunner, TEXT("BindString FString value"), UnrealValue, false, TEXT("UnrealValue"), "UnrealValue"))
		{
			return;
		}

		FBindString DynamicValue;
		DynamicValue.SetDynamic("");
		if (!ExpectBindStringState(*TestRunner, TEXT("BindString dynamic empty"), DynamicValue, true, TEXT(""), ""))
		{
			return;
		}

		DynamicValue.SetDynamic("Namespace::Value");
		if (!ExpectBindStringState(*TestRunner, TEXT("BindString dynamic value"), DynamicValue, false, TEXT("Namespace::Value"), "Namespace::Value"))
		{
			return;
		}

		FBindString SwappedValue("Before");
		if (!ExpectBindStringState(*TestRunner, TEXT("BindString swapped initial constant"), SwappedValue, false, TEXT("Before"), "Before"))
		{
			return;
		}

		SwappedValue.SetDynamic("DynamicAfterConstant");
		if (!ExpectBindStringState(*TestRunner, TEXT("BindString swapped dynamic"), SwappedValue, false, TEXT("DynamicAfterConstant"), "DynamicAfterConstant"))
		{
			return;
		}

		SwappedValue = FString(TEXT("FinalUnreal"));
		ExpectBindStringState(*TestRunner, TEXT("BindString swapped FString"), SwappedValue, false, TEXT("FinalUnreal"), "FinalUnreal");
	}
};

#endif
