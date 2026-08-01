#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "Preprocessor/AngelscriptPreprocessorTestHelpers.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptPreprocessorRangeForTests,
	"Angelscript.TestModule.Preprocessor.RangeFor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
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

	TEST_METHOD(RangeForRewritePreservesUEBehavior)
	{
		using namespace PreprocessorTestHelpers;

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		FScopedModuleCleanEngine ModuleClean(Engine);

		FFixtureFile File(
			TEXT("Tests/Preprocessor/RangeFor/PreservesUEBehavior.as"),
			ASTEST_AS(R"AS(
				void Iterate(TArray<int>& Values, TMap<FString, int>& Lookup)
				{
					FString Preserved = "for (int Fake : Values)";
					// for (int Commented : Values) {}

					for (int Value : Values)
					{
						Print(f"{Value}");
					}

					for (auto Element : Lookup)
					{
						Print(Element.GetKey());
					}

					for (int Index = 0; Index < 1; ++Index)
					{
						Print(f"{Index}");
					}
				}
				)AS"));

		const FPreprocessResult Result = RunPreprocess(Engine, File);
		AssertPreprocessSucceeded(*TestRunner, Result);
		const FAngelscriptModuleDesc* Module = AssertModuleExists(
			*TestRunner,
			Result,
			TEXT("Tests.Preprocessor.RangeFor.PreservesUEBehavior"));
		ASSERT_THAT(IsNotNull(Module, TEXT("Range-for fixture should emit its module")));
		if (Module == nullptr)
		{
			return;
		}

		const FString ProcessedCode = Result.JoinedCode(*Module);
		ASSERT_THAT(IsTrue(
			ProcessedCode.Contains(TEXT("Values.Iterator();"))
				&& ProcessedCode.Contains(TEXT("_Iterator.CanProceed;"))
				&& ProcessedCode.Contains(TEXT("_Iterator.Proceed();")),
			TEXT("Array range-for should lower to the UE iterator protocol while preserving source whitespace")));
		ASSERT_THAT(IsTrue(
			ProcessedCode.Contains(TEXT("int __auto_constref_type Value = _Iterator.Proceed();")),
			TEXT("Array range-for should preserve the historical const-ref element marker")));
		ASSERT_THAT(IsTrue(
			ProcessedCode.Contains(TEXT("auto __auto_constref_type Element = _Iterator.Proceed();")),
			TEXT("Map range-for should preserve auto iterator element lowering")));
		ASSERT_THAT(IsTrue(
			ProcessedCode.Contains(TEXT("for (int Index = 0; Index < 1; ++Index)")),
			TEXT("Classic for loops should remain unchanged")));
		ASSERT_THAT(IsTrue(
			ProcessedCode.Contains(TEXT("\"for (int Fake : Values)\"")),
			TEXT("Range-for spelling inside strings should remain unchanged")));
		ASSERT_THAT(IsTrue(
			ProcessedCode.Contains(TEXT("// for (int Commented : Values) {}")),
			TEXT("Range-for spelling inside comments should remain unchanged")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
