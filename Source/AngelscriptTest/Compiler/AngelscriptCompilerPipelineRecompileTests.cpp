#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "Core/AngelscriptEngine.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CompilerPipelineRecompileTest
{
	static const FName ModuleName(TEXT("CompilerSuccessfulRecompileReplacesStaleOutputs"));
	static const FString ScriptFilename(TEXT("CompilerSuccessfulRecompileReplacesStaleOutputs.as"));
	static const FName GeneratedClassName(TEXT("URecompileCarrier"));
	static const FName GeneratedFunctionName(TEXT("GetValue"));
	static const FName ScorePropertyName(TEXT("Score"));

	FString MakeScriptSource(int32 Value)
	{
		return FString::Printf(TEXT(R"AS(
UCLASS()
class URecompileCarrier : UObject
{
	UPROPERTY()
	int Score = %d;

	UFUNCTION()
	int GetValue()
	{
		return Score;
	}
}

int Entry()
{
	return %d;
}
)AS"),
			Value,
			Value);
	}

	int32 CountActiveModulesByName(const FAngelscriptEngine& Engine, const FString& ModuleNameString)
	{
		int32 Count = 0;
		for (const TSharedRef<FAngelscriptModuleDesc>& Module : Engine.GetActiveModules())
		{
			if (Module->ModuleName == ModuleNameString)
			{
				++Count;
			}
		}

		return Count;
	}

	bool ExecuteEntryAndExpect(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		int32 ExpectedValue)
	{
		int32 EntryResult = 0;
		const bool bExecuted = ExecuteIntFunction(
			&Engine,
			ScriptFilename,
			ModuleName,
			TEXT("int Entry()"),
			EntryResult);
		const bool bExecutePassed = Test.TestTrue(
			*FString::Printf(TEXT("Successful recompile test case should execute Entry() for value %d"), ExpectedValue),
			bExecuted);
		if (!bExecutePassed)
		{
			return false;
		}

		return Test.TestEqual(
			*FString::Printf(TEXT("Successful recompile test case should observe Entry() == %d"), ExpectedValue),
			EntryResult,
			ExpectedValue);
	}

	bool ExecuteGeneratedValueAndExpect(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		UClass* GeneratedClass,
		UFunction* GeneratedFunction,
		int32 ExpectedValue)
	{
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("Successful recompile test case should materialize a generated class for value %d"), ExpectedValue),
				GeneratedClass)
			|| !Test.TestNotNull(
				*FString::Printf(TEXT("Successful recompile test case should expose GetValue for value %d"), ExpectedValue),
				GeneratedFunction))
		{
			return false;
		}

		FIntProperty* ScoreProperty = FindFProperty<FIntProperty>(GeneratedClass, ScorePropertyName);
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("Successful recompile test case should expose Score for value %d"), ExpectedValue),
				ScoreProperty))
		{
			return false;
		}

		UObject* RuntimeObject = NewObject<UObject>(GetTransientPackage(), GeneratedClass);
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("Successful recompile test case should instantiate the generated class for value %d"), ExpectedValue),
				RuntimeObject))
		{
			return false;
		}

		const int32 DefaultScore = ScoreProperty->GetPropertyValue_InContainer(RuntimeObject);
		const bool bDefaultScoreMatches = Test.TestEqual(
			*FString::Printf(TEXT("Successful recompile test case should materialize Score == %d on new instances"), ExpectedValue),
			DefaultScore,
			ExpectedValue);

		int32 MethodResult = 0;
		const bool bExecuted = ExecuteGeneratedIntEventOnGameThread(&Engine, RuntimeObject, GeneratedFunction, MethodResult);
		const bool bExecutePassed = Test.TestTrue(
			*FString::Printf(TEXT("Successful recompile test case should execute GetValue() for value %d"), ExpectedValue),
			bExecuted);
		if (!bDefaultScoreMatches || !bExecutePassed)
		{
			return false;
		}

		return Test.TestEqual(
			*FString::Printf(TEXT("Successful recompile test case should observe GetValue() == %d"), ExpectedValue),
			MethodResult,
			ExpectedValue);
	}
}

TEST_CLASS_WITH_FLAGS(FCompilerPipelineRecompileTests,
	"Angelscript.TestModule.Compiler.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(SuccessfulRecompileReplacesStaleOutputs)
	{
	using namespace CompilerPipelineRecompileTest;


		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*CompilerPipelineRecompileTest::ModuleName.ToString());
		};

		Engine.ResetDiagnostics();

		FAngelscriptCompileTraceSummary InitialSummary;
		const bool bInitialCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			CompilerPipelineRecompileTest::ModuleName,
			CompilerPipelineRecompileTest::ScriptFilename,
			CompilerPipelineRecompileTest::MakeScriptSource(7),
			true,
			InitialSummary);
		TestRunner->TestTrue(
			TEXT("Successful recompile test case should compile the initial annotated module"),
			bInitialCompiled);
		TestRunner->TestEqual(
			TEXT("Successful recompile test case should report FullyHandled for the initial compile"),
			InitialSummary.CompileResult,
			ECompileResult::FullyHandled);
		TestRunner->TestEqual(
			TEXT("Successful recompile test case should emit no diagnostics for the initial compile"),
			InitialSummary.Diagnostics.Num(),
			0);
		if (!bInitialCompiled)
		{
			return;
		}

		TSharedPtr<FAngelscriptModuleDesc> InitialModuleDesc = Engine.GetModuleByFilenameOrModuleName(
			CompilerPipelineRecompileTest::ScriptFilename,
			CompilerPipelineRecompileTest::ModuleName.ToString());
		if (!TestRunner->TestNotNull(TEXT("Successful recompile test case should publish the initial module descriptor"), InitialModuleDesc.Get()))
		{
			return;
		}

		UClass* InitialClass = FindGeneratedClass(&Engine, CompilerPipelineRecompileTest::GeneratedClassName);
		UFunction* InitialFunction = InitialClass != nullptr
			? FindGeneratedFunction(InitialClass, CompilerPipelineRecompileTest::GeneratedFunctionName)
			: nullptr;
		if (!CompilerPipelineRecompileTest::ExecuteEntryAndExpect(*TestRunner, Engine, 7)
			|| !CompilerPipelineRecompileTest::ExecuteGeneratedValueAndExpect(*TestRunner, Engine, InitialClass, InitialFunction, 7))
		{
			return;
		}

		TestRunner->TestEqual(
			TEXT("Successful recompile test case should keep exactly one active module after the initial compile"),
			CompilerPipelineRecompileTest::CountActiveModulesByName(Engine, CompilerPipelineRecompileTest::ModuleName.ToString()),
			1);

		Engine.ResetDiagnostics();

		FAngelscriptCompileTraceSummary RecompiledSummary;
		const bool bRecompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			CompilerPipelineRecompileTest::ModuleName,
			CompilerPipelineRecompileTest::ScriptFilename,
			CompilerPipelineRecompileTest::MakeScriptSource(42),
			true,
			RecompiledSummary);
		TestRunner->TestTrue(
			TEXT("Successful recompile test case should compile the updated annotated module"),
			bRecompiled);
		TestRunner->TestEqual(
			TEXT("Successful recompile test case should report FullyHandled for the updated compile"),
			RecompiledSummary.CompileResult,
			ECompileResult::FullyHandled);
		TestRunner->TestEqual(
			TEXT("Successful recompile test case should emit no diagnostics for the updated compile"),
			RecompiledSummary.Diagnostics.Num(),
			0);
		if (!bRecompiled)
		{
			return;
		}

		TSharedPtr<FAngelscriptModuleDesc> RecompiledModuleDesc = Engine.GetModuleByFilenameOrModuleName(
			CompilerPipelineRecompileTest::ScriptFilename,
			CompilerPipelineRecompileTest::ModuleName.ToString());
		if (!TestRunner->TestNotNull(TEXT("Successful recompile test case should publish the recompiled module descriptor"), RecompiledModuleDesc.Get()))
		{
			return;
		}

		UClass* RecompiledClass = FindGeneratedClass(&Engine, CompilerPipelineRecompileTest::GeneratedClassName);
		UFunction* RecompiledFunction = RecompiledClass != nullptr
			? FindGeneratedFunction(RecompiledClass, CompilerPipelineRecompileTest::GeneratedFunctionName)
			: nullptr;
		if (!CompilerPipelineRecompileTest::ExecuteEntryAndExpect(*TestRunner, Engine, 42)
			|| !CompilerPipelineRecompileTest::ExecuteGeneratedValueAndExpect(*TestRunner, Engine, RecompiledClass, RecompiledFunction, 42))
		{
			return;
		}

		TestRunner->TestEqual(
			TEXT("Successful recompile test case should keep exactly one active module after the updated compile"),
			CompilerPipelineRecompileTest::CountActiveModulesByName(Engine, CompilerPipelineRecompileTest::ModuleName.ToString()),
			1);
		TestRunner->TestNotEqual(
			TEXT("Successful recompile test case should replace the active module descriptor after the updated compile"),
			RecompiledModuleDesc.Get(),
			InitialModuleDesc.Get());
		TestRunner->TestTrue(
			TEXT("Successful recompile test case should replace the underlying script module after the updated compile"),
			RecompiledModuleDesc->ScriptModule != InitialModuleDesc->ScriptModule);

		if (RecompiledClass == InitialClass && RecompiledFunction == InitialFunction)
		{
			CompilerPipelineRecompileTest::ExecuteGeneratedValueAndExpect(
				*TestRunner,
				Engine,
				InitialClass,
				InitialFunction,
				42);
		}

		}

	}

};

#endif
