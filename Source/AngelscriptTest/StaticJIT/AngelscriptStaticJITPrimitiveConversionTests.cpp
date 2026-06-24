#include "CQTest.h"

#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "StaticJIT/StaticJITHeader.h"

#if WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITPrimitiveConversionTests,
	"Angelscript.TestModule.StaticJIT.PrimitiveConversions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool VerifyCurrentEngine(FAutomationTestBase& Test, const TCHAR* CaseLabel, FAngelscriptEngine& Engine);
	static bool RunPrimitiveBitCastRoundTrip(FAutomationTestBase& Test);
	static bool RunPrimitiveZeroExtendParity(FAutomationTestBase& Test);
	static bool RunPrimitiveNumericConversionParity(FAutomationTestBase& Test);

public:
	TEST_METHOD(BitCastFloatRoundTrip)
	{
		ASSERT_THAT(IsTrue(RunPrimitiveBitCastRoundTrip(*TestRunner)));
	}

	TEST_METHOD(ZeroExtendParity)
	{
		ASSERT_THAT(IsTrue(RunPrimitiveZeroExtendParity(*TestRunner)));
	}

	TEST_METHOD(BitCastAndNumericParity)
	{
		ASSERT_THAT(IsTrue(RunPrimitiveNumericConversionParity(*TestRunner)));
	}
};

bool FAngelscriptStaticJITPrimitiveConversionTests::VerifyCurrentEngine(
	FAutomationTestBase& Test,
	const TCHAR* CaseLabel,
	FAngelscriptEngine& Engine)
{
	return Test.TestTrue(
		*FString::Printf(TEXT("%s should run with the current Angelscript engine installed"), CaseLabel),
		FAngelscriptEngine::TryGetCurrentEngine() == &Engine);
}

bool FAngelscriptStaticJITPrimitiveConversionTests::RunPrimitiveBitCastRoundTrip(FAutomationTestBase& Test)
{
	bool bPassed = false;
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

	do
	{
		if (!VerifyCurrentEngine(
				Test,
				TEXT("StaticJIT.PrimitiveConversions.BitCastFloatRoundTrip"),
				Engine))
		{
			break;
		}

		const asDWORD ExpectedBits = 0x3FC00000u;
		const float FloatValue = value_as<float>(ExpectedBits);
		if (!Test.TestEqual(
				TEXT("StaticJIT.PrimitiveConversions.BitCastFloatRoundTrip should reinterpret 0x3FC00000 as 1.5f"),
				FloatValue,
				1.5f))
		{
			break;
		}

		asDWORD RoundTripBits = 0u;
		value_assign_safe<asDWORD>(&RoundTripBits, FloatValue);
		if (!Test.TestEqual(
				TEXT("StaticJIT.PrimitiveConversions.BitCastFloatRoundTrip should preserve the float bit pattern when writing back to asDWORD"),
				value_read<asDWORD>(&RoundTripBits),
				ExpectedBits))
		{
			break;
		}

		if (!Test.TestEqual(
				TEXT("StaticJIT.PrimitiveConversions.BitCastFloatRoundTrip should support the reverse float-to-dword bit cast"),
				value_as<asDWORD>(FloatValue),
				ExpectedBits))
		{
			break;
		}

		bPassed = true;
	}
	while (false);

	}
	return bPassed;
}

bool FAngelscriptStaticJITPrimitiveConversionTests::RunPrimitiveZeroExtendParity(FAutomationTestBase& Test)
{
	bool bPassed = false;
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

	do
	{
		if (!VerifyCurrentEngine(
				Test,
				TEXT("StaticJIT.PrimitiveConversions.ZeroExtendParity"),
				Engine))
		{
			break;
		}

		const asDWORD NarrowValue = 0x89ABCDEFu;
		asQWORD WideValue = 0xFFFFFFFFFFFFFFFFull;
		value_assign_safe<asQWORD>(&WideValue, NarrowValue);

		if (!Test.TestEqual(
				TEXT("StaticJIT.PrimitiveConversions.ZeroExtendParity should zero-extend a dword into a qword without leaving dirty high bits"),
				value_read<asQWORD>(&WideValue),
				static_cast<asQWORD>(0x0000000089ABCDEFull)))
		{
			break;
		}

		if (!Test.TestEqual(
				TEXT("StaticJIT.PrimitiveConversions.ZeroExtendParity should preserve the original low 32-bit payload after widening"),
				value_read<asDWORD>(&WideValue),
				NarrowValue))
		{
			break;
		}

		bPassed = true;
	}
	while (false);

	}
	return bPassed;
}

bool FAngelscriptStaticJITPrimitiveConversionTests::RunPrimitiveNumericConversionParity(FAutomationTestBase& Test)
{
	bool bPassed = false;
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

	do
	{
		if (!VerifyCurrentEngine(
				Test,
				TEXT("StaticJIT.PrimitiveConversions.BitCastAndNumericParity"),
				Engine))
		{
			break;
		}

		const double SignedConverted = ConvertPrimitiveValue<double, int>(-1);
		const double UnsignedConverted = ConvertPrimitiveValue<double, asDWORD>(0xFFFFFFFFu);

		if (!Test.TestEqual(
				TEXT("StaticJIT.PrimitiveConversions.BitCastAndNumericParity should keep signed -1 converting to double as -1.0"),
				SignedConverted,
				-1.0))
		{
			break;
		}

		if (!Test.TestEqual(
				TEXT("StaticJIT.PrimitiveConversions.BitCastAndNumericParity should keep unsigned 0xFFFFFFFF converting to double as 4294967295.0"),
				UnsignedConverted,
				4294967295.0))
		{
			break;
		}

		if (!Test.TestFalse(
				TEXT("StaticJIT.PrimitiveConversions.BitCastAndNumericParity should keep signed and unsigned conversion paths distinct"),
				SignedConverted == UnsignedConverted))
		{
			break;
		}

		bPassed = true;
	}
	while (false);

	}
	return bPassed;
}

#endif
