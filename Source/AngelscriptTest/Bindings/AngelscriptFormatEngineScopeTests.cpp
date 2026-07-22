#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestEngineAcquisition.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptFormatEngineScopeTests,
	"Angelscript.TestModule.Bindings.FormatEngineScope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool ExpectFormatArgumentsResolveForEngine(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const TCHAR* ModuleName)
	{
		FAngelscriptEngineScope Scope(Engine);
		FNoDiscardAsserter LocalAssert(Test);
		FScopedAngelscriptModule Module(Test, Engine, ModuleName, ASTEST_AS(R"AS(
			int FStringFormatLiteralArg()
			{
				FString Result = FString::Format("{0}", "Hello");
				return Result == "Hello" ? 1 : 0;
			}

			int FStringFormatValueArg()
			{
				FString Value = "World";
				FString Result = FString::Format("{0}", Value);
				return Result == "World" ? 1 : 0;
			}

			int FTextFormatValueArg()
			{
				FText FormatValue = FText::FromString("{0}");
				FText Value = FText::FromString("Text");
				FText Result = FText::Format(FormatValue, Value);
				return Result.ToString() == "Text" ? 1 : 0;
			}
			)AS"));
		if (!LocalAssert.IsTrue(
			Module.IsValid(),
			*FString::Printf(TEXT("%s should compile"), ModuleName)))
		{
			return false;
		}

		const FExpectedInt Cases[] = {
			{ TEXT("int FStringFormatLiteralArg()"), TEXT("FString::Format should accept literal FString argument"), 1 },
			{ TEXT("int FStringFormatValueArg()"), TEXT("FString::Format should accept FString value argument"), 1 },
			{ TEXT("int FTextFormatValueArg()"), TEXT("FText::Format should accept FText value argument"), 1 },
		};
		return ExecuteBatchAndExpectInt(Test, Engine, Module.GetModule(), Cases);
	}

public:
	TEST_METHOD(FormatUsesCurrentEngineTypeInfoWhenMultipleEnginesAreLive)
	{
		TUniquePtr<FAngelscriptEngine> FirstEngine = CreateIsolatedFullEngine();
		ASSERT_THAT(IsNotNull(FirstEngine.Get()));

		TUniquePtr<FAngelscriptEngine> SecondEngine = CreateIsolatedFullEngine();
		ASSERT_THAT(IsNotNull(SecondEngine.Get()));

		ASSERT_THAT(IsTrue(ExpectFormatArgumentsResolveForEngine(
			*TestRunner,
			*FirstEngine,
			TEXT("ASFormatEngineScope_First"))));

		ASSERT_THAT(IsTrue(ExpectFormatArgumentsResolveForEngine(
			*TestRunner,
			*SecondEngine,
			TEXT("ASFormatEngineScope_Second"))));
	}
};

#endif
