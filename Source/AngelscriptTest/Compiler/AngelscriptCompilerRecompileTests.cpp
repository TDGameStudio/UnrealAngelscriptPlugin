#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "Core/AngelscriptEngine.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace CompilerRecompileTest
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
		FNoDiscardAsserter LocalAssert(Test);
		const bool bExecutePassed = LocalAssert.IsTrue(
			bExecuted,
			*FString::Printf(TEXT("Successful recompile test case should execute Entry() for value %d"), ExpectedValue));
		if (!bExecutePassed)
		{
			return false;
		}

		return LocalAssert.AreEqual(
			ExpectedValue,
			EntryResult,
			*FString::Printf(TEXT("Successful recompile test case should observe Entry() == %d"), ExpectedValue));
	}

	bool ExecuteGeneratedValueAndExpect(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		UClass* GeneratedClass,
		UFunction* GeneratedFunction,
		int32 ExpectedValue)
	{
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(
				GeneratedClass,
				*FString::Printf(TEXT("Successful recompile test case should materialize a generated class for value %d"), ExpectedValue))
			|| !LocalAssert.IsNotNull(
				GeneratedFunction,
				*FString::Printf(TEXT("Successful recompile test case should expose GetValue for value %d"), ExpectedValue)))
		{
			return false;
		}

		FIntProperty* ScoreProperty = FindFProperty<FIntProperty>(GeneratedClass, ScorePropertyName);
		if (!LocalAssert.IsNotNull(
				ScoreProperty,
				*FString::Printf(TEXT("Successful recompile test case should expose Score for value %d"), ExpectedValue)))
		{
			return false;
		}

		UObject* RuntimeObject = NewObject<UObject>(GetTransientPackage(), GeneratedClass);
		if (!LocalAssert.IsNotNull(
				RuntimeObject,
				*FString::Printf(TEXT("Successful recompile test case should instantiate the generated class for value %d"), ExpectedValue)))
		{
			return false;
		}

		const int32 DefaultScore = ScoreProperty->GetPropertyValue_InContainer(RuntimeObject);
		const bool bDefaultScoreMatches = LocalAssert.AreEqual(
			ExpectedValue,
			DefaultScore,
			*FString::Printf(TEXT("Successful recompile test case should materialize Score == %d on new instances"), ExpectedValue));

		int32 MethodResult = 0;
		const bool bExecuted = ExecuteGeneratedIntEventOnGameThread(&Engine, RuntimeObject, GeneratedFunction, MethodResult);
		const bool bExecutePassed = LocalAssert.IsTrue(
			bExecuted,
			*FString::Printf(TEXT("Successful recompile test case should execute GetValue() for value %d"), ExpectedValue));
		if (!bDefaultScoreMatches || !bExecutePassed)
		{
			return false;
		}

		return LocalAssert.AreEqual(
			ExpectedValue,
			MethodResult,
			*FString::Printf(TEXT("Successful recompile test case should observe GetValue() == %d"), ExpectedValue));
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCompilerRecompileTests,
	"Angelscript.TestModule.Compiler.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(SuccessfulRecompileReplacesStaleOutputs)
	{
	using namespace CompilerRecompileTest;


		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*CompilerRecompileTest::ModuleName.ToString());
		};

		Engine.ResetDiagnostics();

		FAngelscriptCompileTraceSummary InitialSummary;
		const bool bInitialCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			CompilerRecompileTest::ModuleName,
			CompilerRecompileTest::ScriptFilename,
			CompilerRecompileTest::MakeScriptSource(7),
			true,
			InitialSummary);
		ASSERT_THAT(IsTrue(
			bInitialCompiled,
			TEXT("Successful recompile test case should compile the initial annotated module")));
		ASSERT_THAT(AreEqual(
			ECompileResult::FullyHandled,
			InitialSummary.CompileResult,
			TEXT("Successful recompile test case should report FullyHandled for the initial compile")));
		ASSERT_THAT(AreEqual(
			0,
			InitialSummary.Diagnostics.Num(),
			TEXT("Successful recompile test case should emit no diagnostics for the initial compile")));
		if (!bInitialCompiled)
		{
			return;
		}

		TSharedPtr<FAngelscriptModuleDesc> InitialModuleDesc = Engine.GetModuleByFilenameOrModuleName(
			CompilerRecompileTest::ScriptFilename,
			CompilerRecompileTest::ModuleName.ToString());
		if (!this->Assert.IsNotNull(InitialModuleDesc.Get(), TEXT("Successful recompile test case should publish the initial module descriptor")))
		{
			return;
		}

		UClass* InitialClass = FindGeneratedClass(&Engine, CompilerRecompileTest::GeneratedClassName);
		UFunction* InitialFunction = InitialClass != nullptr
			? FindGeneratedFunction(InitialClass, CompilerRecompileTest::GeneratedFunctionName)
			: nullptr;
		if (!CompilerRecompileTest::ExecuteEntryAndExpect(*TestRunner, Engine, 7)
			|| !CompilerRecompileTest::ExecuteGeneratedValueAndExpect(*TestRunner, Engine, InitialClass, InitialFunction, 7))
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			1,
			CompilerRecompileTest::CountActiveModulesByName(Engine, CompilerRecompileTest::ModuleName.ToString()),
			TEXT("Successful recompile test case should keep exactly one active module after the initial compile")));

		Engine.ResetDiagnostics();

		FAngelscriptCompileTraceSummary RecompiledSummary;
		const bool bRecompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			CompilerRecompileTest::ModuleName,
			CompilerRecompileTest::ScriptFilename,
			CompilerRecompileTest::MakeScriptSource(42),
			true,
			RecompiledSummary);
		ASSERT_THAT(IsTrue(
			bRecompiled,
			TEXT("Successful recompile test case should compile the updated annotated module")));
		ASSERT_THAT(AreEqual(
			ECompileResult::FullyHandled,
			RecompiledSummary.CompileResult,
			TEXT("Successful recompile test case should report FullyHandled for the updated compile")));
		ASSERT_THAT(AreEqual(
			0,
			RecompiledSummary.Diagnostics.Num(),
			TEXT("Successful recompile test case should emit no diagnostics for the updated compile")));
		if (!bRecompiled)
		{
			return;
		}

		TSharedPtr<FAngelscriptModuleDesc> RecompiledModuleDesc = Engine.GetModuleByFilenameOrModuleName(
			CompilerRecompileTest::ScriptFilename,
			CompilerRecompileTest::ModuleName.ToString());
		if (!this->Assert.IsNotNull(RecompiledModuleDesc.Get(), TEXT("Successful recompile test case should publish the recompiled module descriptor")))
		{
			return;
		}

		UClass* RecompiledClass = FindGeneratedClass(&Engine, CompilerRecompileTest::GeneratedClassName);
		UFunction* RecompiledFunction = RecompiledClass != nullptr
			? FindGeneratedFunction(RecompiledClass, CompilerRecompileTest::GeneratedFunctionName)
			: nullptr;
		if (!CompilerRecompileTest::ExecuteEntryAndExpect(*TestRunner, Engine, 42)
			|| !CompilerRecompileTest::ExecuteGeneratedValueAndExpect(*TestRunner, Engine, RecompiledClass, RecompiledFunction, 42))
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			1,
			CompilerRecompileTest::CountActiveModulesByName(Engine, CompilerRecompileTest::ModuleName.ToString()),
			TEXT("Successful recompile test case should keep exactly one active module after the updated compile")));
		ASSERT_THAT(AreNotEqual(
			InitialModuleDesc.Get(),
			RecompiledModuleDesc.Get(),
			TEXT("Successful recompile test case should replace the active module descriptor after the updated compile")));
		ASSERT_THAT(IsTrue(
			RecompiledModuleDesc->ScriptModule != InitialModuleDesc->ScriptModule,
			TEXT("Successful recompile test case should replace the underlying script module after the updated compile")));

		if (RecompiledClass == InitialClass && RecompiledFunction == InitialFunction)
		{
			CompilerRecompileTest::ExecuteGeneratedValueAndExpect(
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
