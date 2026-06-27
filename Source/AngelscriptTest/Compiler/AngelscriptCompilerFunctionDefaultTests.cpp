#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "Containers/StringConv.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CompilerFunctionDefaultTest
{
	static const FName ModuleName(TEXT("Tests.Compiler.FunctionDefaultMetadataRoundTrip"));
	static const FString ScriptFilename(TEXT("Tests/Compiler/FunctionDefaultMetadataRoundTrip.as"));

	bool VerifyParamMetadata(
		FAutomationTestBase& Test,
		asIScriptFunction& Function,
		const asUINT ParamIndex,
		const TCHAR* ExpectedName,
		const TCHAR* ExpectedDefaultArg)
	{
		const char* RawName = nullptr;
		const char* RawDefaultArg = reinterpret_cast<const char*>(1);
		const int Result = Function.GetParam(ParamIndex, nullptr, nullptr, &RawName, &RawDefaultArg);

		FNoDiscardAsserter LocalAssert(Test);
		bool bPassed = true;
		bPassed &= LocalAssert.AreEqual(
			static_cast<int32>(asSUCCESS),
			Result,
			*FString::Printf(TEXT("Function default metadata round-trip should inspect parameter %u successfully"), static_cast<uint32>(ParamIndex)));

		const FString ActualName = RawName != nullptr ? FString(UTF8_TO_TCHAR(RawName)) : FString();
		bPassed &= LocalAssert.AreEqual(
			FString(ExpectedName),
			ActualName,
			*FString::Printf(TEXT("Function default metadata round-trip should preserve parameter %u name"), static_cast<uint32>(ParamIndex)));

		if (ExpectedDefaultArg == nullptr)
		{
			bPassed &= LocalAssert.IsTrue(
				RawDefaultArg == nullptr,
				*FString::Printf(TEXT("Function default metadata round-trip should keep parameter %u without a defaultArg"), static_cast<uint32>(ParamIndex)));
		}
		else
		{
			bPassed &= LocalAssert.IsNotNull(
				RawDefaultArg,
				*FString::Printf(TEXT("Function default metadata round-trip should expose a defaultArg for parameter %u"), static_cast<uint32>(ParamIndex)));
			if (RawDefaultArg != nullptr)
			{
				bPassed &= LocalAssert.AreEqual(
					FString(ExpectedDefaultArg),
					FString(UTF8_TO_TCHAR(RawDefaultArg)),
					*FString::Printf(TEXT("Function default metadata round-trip should preserve parameter %u defaultArg text"), static_cast<uint32>(ParamIndex)));
			}
		}

		return bPassed;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCompilerFunctionDefaultTests,
	"Angelscript.TestModule.Compiler.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(FunctionDefaultMetadataRoundTrip)
	{


		const FString ScriptSource = TEXT(R"AS(
	int SumWithDefaults(int Required, int Value = 21, int Extra = 7)
	{
		return Required + Value + Extra;
	}

	int Entry()
	{
		return SumWithDefaults(14);
	}
	)AS");

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*CompilerFunctionDefaultTest::ModuleName.ToString());
		};

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			CompilerFunctionDefaultTest::ModuleName,
			CompilerFunctionDefaultTest::ScriptFilename,
			ScriptSource,
			false,
			Summary);

		ASSERT_THAT(IsTrue(
			bCompiled,
			TEXT("Function default metadata round-trip should compile successfully")));
		ASSERT_THAT(IsFalse(
			Summary.bUsedPreprocessor,
			TEXT("Function default metadata round-trip should stay on the plain-source path without the preprocessor")));
		ASSERT_THAT(IsTrue(
			Summary.bCompileSucceeded,
			TEXT("Function default metadata round-trip should mark compile succeeded in the summary")));
		ASSERT_THAT(AreEqual(
			0,
			Summary.Diagnostics.Num(),
			TEXT("Function default metadata round-trip should keep diagnostics empty")));
		if (!bCompiled)
		{
			return;
		}

		int32 EntryResult = 0;
		const bool bExecuted = ExecuteIntFunction(
			&Engine,
			CompilerFunctionDefaultTest::ModuleName,
			TEXT("int Entry()"),
			EntryResult);
		ASSERT_THAT(IsTrue(
			bExecuted,
			TEXT("Function default metadata round-trip should execute Entry successfully")));
		if (bExecuted)
		{
			ASSERT_THAT(AreEqual(
				42,
				EntryResult,
				TEXT("Function default metadata round-trip should honor omitted default arguments at runtime")));
		}

		const TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByModuleName(
			CompilerFunctionDefaultTest::ModuleName.ToString());
		if (!this->Assert.IsTrue(
				ModuleDesc.IsValid(),
				TEXT("Function default metadata round-trip should register the module by name")))
		{
			return;
		}

		if (!this->Assert.IsNotNull(
				ModuleDesc->ScriptModule,
				TEXT("Function default metadata round-trip should expose the compiled script module")))
		{
			return;
		}

		asIScriptFunction* SumWithDefaults = GetFunctionByDecl(
			*TestRunner,
			*ModuleDesc->ScriptModule,
			TEXT("int SumWithDefaults(int, int, int)"));
		if (!this->Assert.IsNotNull(
				SumWithDefaults,
				TEXT("Function default metadata round-trip should resolve SumWithDefaults by its exact declaration")))
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			3,
			static_cast<int32>(SumWithDefaults->GetParamCount()),
			TEXT("Function default metadata round-trip should keep the exact parameter count")));
		CompilerFunctionDefaultTest::VerifyParamMetadata(
			*TestRunner,
			*SumWithDefaults,
			0,
			TEXT("Required"),
			nullptr);
		CompilerFunctionDefaultTest::VerifyParamMetadata(
			*TestRunner,
			*SumWithDefaults,
			1,
			TEXT("Value"),
			TEXT("21"));
		CompilerFunctionDefaultTest::VerifyParamMetadata(
			*TestRunner,
			*SumWithDefaults,
			2,
			TEXT("Extra"),
			TEXT("7"));

		}

	}

};

#endif
