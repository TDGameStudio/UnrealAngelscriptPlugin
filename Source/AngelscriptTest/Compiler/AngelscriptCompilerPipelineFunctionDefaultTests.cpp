#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptTestMacros.h"

#include "Containers/StringConv.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CompilerPipelineFunctionDefaultTest
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

		bool bPassed = true;
		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("Function default metadata round-trip should inspect parameter %u successfully"), static_cast<uint32>(ParamIndex)),
			Result,
			static_cast<int32>(asSUCCESS));

		const FString ActualName = RawName != nullptr ? FString(UTF8_TO_TCHAR(RawName)) : FString();
		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("Function default metadata round-trip should preserve parameter %u name"), static_cast<uint32>(ParamIndex)),
			ActualName,
			FString(ExpectedName));

		if (ExpectedDefaultArg == nullptr)
		{
			bPassed &= Test.TestTrue(
				*FString::Printf(TEXT("Function default metadata round-trip should keep parameter %u without a defaultArg"), static_cast<uint32>(ParamIndex)),
				RawDefaultArg == nullptr);
		}
		else
		{
			bPassed &= Test.TestNotNull(
				*FString::Printf(TEXT("Function default metadata round-trip should expose a defaultArg for parameter %u"), static_cast<uint32>(ParamIndex)),
				RawDefaultArg);
			if (RawDefaultArg != nullptr)
			{
				bPassed &= Test.TestEqual(
					*FString::Printf(TEXT("Function default metadata round-trip should preserve parameter %u defaultArg text"), static_cast<uint32>(ParamIndex)),
					FString(UTF8_TO_TCHAR(RawDefaultArg)),
					FString(ExpectedDefaultArg));
			}
		}

		return bPassed;
	}
}

using namespace CompilerPipelineFunctionDefaultTest;

TEST_CLASS_WITH_FLAGS(FCompilerPipelineFunctionDefaultTests,
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
			Engine.DiscardModule(*CompilerPipelineFunctionDefaultTest::ModuleName.ToString());
		};

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			CompilerPipelineFunctionDefaultTest::ModuleName,
			CompilerPipelineFunctionDefaultTest::ScriptFilename,
			ScriptSource,
			false,
			Summary);

		TestRunner->TestTrue(
			TEXT("Function default metadata round-trip should compile successfully"),
			bCompiled);
		TestRunner->TestFalse(
			TEXT("Function default metadata round-trip should stay on the plain-source path without the preprocessor"),
			Summary.bUsedPreprocessor);
		TestRunner->TestTrue(
			TEXT("Function default metadata round-trip should mark compile succeeded in the summary"),
			Summary.bCompileSucceeded);
		TestRunner->TestEqual(
			TEXT("Function default metadata round-trip should keep diagnostics empty"),
			Summary.Diagnostics.Num(),
			0);
		if (!bCompiled)
		{
			return;
		}

		int32 EntryResult = 0;
		const bool bExecuted = ExecuteIntFunction(
			&Engine,
			CompilerPipelineFunctionDefaultTest::ModuleName,
			TEXT("int Entry()"),
			EntryResult);
		TestRunner->TestTrue(
			TEXT("Function default metadata round-trip should execute Entry successfully"),
			bExecuted);
		if (bExecuted)
		{
			TestRunner->TestEqual(
				TEXT("Function default metadata round-trip should honor omitted default arguments at runtime"),
				EntryResult,
				42);
		}

		const TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByModuleName(
			CompilerPipelineFunctionDefaultTest::ModuleName.ToString());
		if (!TestRunner->TestTrue(
				TEXT("Function default metadata round-trip should register the module by name"),
				ModuleDesc.IsValid()))
		{
			return;
		}

		if (!TestRunner->TestNotNull(
				TEXT("Function default metadata round-trip should expose the compiled script module"),
				ModuleDesc->ScriptModule))
		{
			return;
		}

		asIScriptFunction* SumWithDefaults = GetFunctionByDecl(
			*TestRunner,
			*ModuleDesc->ScriptModule,
			TEXT("int SumWithDefaults(int, int, int)"));
		if (!TestRunner->TestNotNull(
				TEXT("Function default metadata round-trip should resolve SumWithDefaults by its exact declaration"),
				SumWithDefaults))
		{
			return;
		}

		TestRunner->TestEqual(
			TEXT("Function default metadata round-trip should keep the exact parameter count"),
			static_cast<int32>(SumWithDefaults->GetParamCount()),
			3);
		CompilerPipelineFunctionDefaultTest::VerifyParamMetadata(
			*TestRunner,
			*SumWithDefaults,
			0,
			TEXT("Required"),
			nullptr);
		CompilerPipelineFunctionDefaultTest::VerifyParamMetadata(
			*TestRunner,
			*SumWithDefaults,
			1,
			TEXT("Value"),
			TEXT("21"));
		CompilerPipelineFunctionDefaultTest::VerifyParamMetadata(
			*TestRunner,
			*SumWithDefaults,
			2,
			TEXT("Extra"),
			TEXT("7"));

		}

	}

};

#endif
