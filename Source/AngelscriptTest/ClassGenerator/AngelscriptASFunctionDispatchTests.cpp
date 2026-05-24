#include "Shared/AngelscriptFunctionalTestUtils.h"
#include "Shared/AngelscriptTestMacros.h"

#include "CQTest.h"
#include "ClassGenerator/ASClass.h"
#include "Misc/ScopeExit.h"
#include "UObject/StructOnScope.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

namespace ASFunctionDispatchTests
{
	static const FName MatrixModuleName(TEXT("ASFunctionDispatchMatrix"));
	static const FString MatrixFilename(TEXT("ASFunctionDispatchMatrix.as"));
	static const FName MatrixClassName(TEXT("UASFunctionDispatchMatrix"));
	static const FName MatrixStaticsClassName(TEXT("UModule_ASFunctionDispatchMatrixStatics"));

	struct FDispatchCase
	{
		FName ModuleName;
		FString Filename;
		FName GeneratedClassName;
		const TCHAR* CaseLabel = TEXT("");
		const TCHAR* ScriptSource = TEXT("");
		UClass* ExpectedFunctionClass = nullptr;
		UClass* ExpectedJitFunctionClass = nullptr;
	};

	struct FMatrixCase
	{
		const TCHAR* FunctionName = TEXT("");
		UClass* ExpectedFunctionClass = nullptr;
		const TCHAR* CaseLabel = TEXT("");
	};

	bool MatchesExpectedFunctionClass(const UFunction& Function, const FDispatchCase& TestCase)
	{
		const UClass* ActualFunctionClass = Function.GetClass();
		return ActualFunctionClass == TestCase.ExpectedFunctionClass || ActualFunctionClass == TestCase.ExpectedJitFunctionClass;
	}

	FString DescribeExpectedFunctionClasses(const FDispatchCase& TestCase)
	{
		return FString::Printf(
			TEXT("%s or %s"),
			*GetNameSafe(TestCase.ExpectedFunctionClass),
			*GetNameSafe(TestCase.ExpectedJitFunctionClass));
	}

	FString DescribeActualFunctionClass(const UFunction* Function)
	{
		return Function != nullptr ? GetNameSafe(Function->GetClass()) : TEXT("<null>");
	}

	bool ExpectFunctionClass(FAutomationTestBase& Test, UClass* OwnerClass, const FMatrixCase& TestCase)
	{
		UASFunction* Function = Cast<UASFunction>(FindGeneratedFunction(OwnerClass, TestCase.FunctionName));
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("AllocateFunctionFor matrix should expose '%s'"), TestCase.FunctionName),
				Function))
		{
			return false;
		}

		return Test.TestTrue(
			*FString::Printf(
				TEXT("AllocateFunctionFor %s should select %s (actual: %s)"),
				TestCase.CaseLabel,
				*GetNameSafe(TestCase.ExpectedFunctionClass),
				*DescribeActualFunctionClass(Function)),
			Function->GetClass() == TestCase.ExpectedFunctionClass);
	}

	UASClass* CompileMatrixClass(FAutomationTestBase& Test, FAngelscriptEngine& Engine)
	{
		const FString ScriptSource = TEXT(R"AS(
UCLASS()
class UASFunctionDispatchMatrix : UObject
{
	UPROPERTY()
	int StoredValue = 0;

	UFUNCTION()
	void NoParams()
	{
		StoredValue = 1;
	}

	UFUNCTION()
	void TakeByte(uint8 Value)
	{
		StoredValue = Value;
	}

	UFUNCTION()
	void TakeBool(bool bValue)
	{
		StoredValue = bValue ? 1 : 0;
	}

	UFUNCTION()
	void TakeDWord(uint32 Value)
	{
		StoredValue = int(Value);
	}

	UFUNCTION()
	void TakeQWord(uint64 Value)
	{
		StoredValue = int(Value);
	}

	UFUNCTION()
	void TakeFloat(float32 Value)
	{
		StoredValue = int(Value);
	}

	UFUNCTION()
	void TakeDouble(float64 Value)
	{
		StoredValue = int(Value);
	}

	UFUNCTION()
	void TakeReference(int& Value)
	{
		Value += 1;
		StoredValue = Value;
	}

	UFUNCTION()
	uint8 ReturnByte()
	{
		return 7;
	}

	UFUNCTION()
	int ReturnDWord()
	{
		return 11;
	}

	UFUNCTION()
	float32 ReturnFloat()
	{
		return 12.0f;
	}

	UFUNCTION()
	float64 ReturnDouble()
	{
		return 13.0;
	}

	UFUNCTION()
	UObject ReturnObject()
	{
		return this;
	}

	UFUNCTION()
	int GenericTwoArgs(int A, int B)
	{
		return A + B;
	}

	UFUNCTION(meta = (BlueprintThreadSafe))
	int ThreadSafeReturn()
	{
		return 17;
	}

	UFUNCTION(BlueprintEvent)
	int VirtualReturn()
	{
		return 19;
	}
}

UFUNCTION(BlueprintCallable)
int StaticReturn()
{
	return 23;
}
)AS");

		return Cast<UASClass>(CompileScriptModule(
			Test,
			Engine,
			MatrixModuleName,
			MatrixFilename,
			ScriptSource,
			MatrixClassName));
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptASFunctionDispatchTests,
	"Angelscript.TestModule.ClassGenerator.ASFunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(AllocateFunctionForSelectsCorrectThreadSafeDispatchSubclass)
	{
		using namespace ASFunctionDispatchTests;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		const TArray<ASFunctionDispatchTests::FDispatchCase> Cases =
		{
			{
				TEXT("ASFunctionDispatchDefault"),
				TEXT("ASFunctionDispatchDefault.as"),
				TEXT("UASFunctionDispatchDefault"),
				TEXT("default non-thread-safe"),
				TEXT(R"AS(
UCLASS()
class UASFunctionDispatchDefault : UObject
{
	UFUNCTION()
	int GetValue()
	{
		return 1;
	}
}
)AS"),
				UASFunction_DWordReturn::StaticClass(),
				UASFunction_DWordReturn_JIT::StaticClass()
			},
			{
				TEXT("ASFunctionDispatchBlueprintThreadSafeFunction"),
				TEXT("ASFunctionDispatchBlueprintThreadSafeFunction.as"),
				TEXT("UASFunctionDispatchBlueprintThreadSafeFunction"),
				TEXT("function-level BlueprintThreadSafe"),
				TEXT(R"AS(
UCLASS()
class UASFunctionDispatchBlueprintThreadSafeFunction : UObject
{
	UFUNCTION(meta = (BlueprintThreadSafe))
	int GetValue()
	{
		return 1;
	}
}
)AS"),
				UASFunction::StaticClass(),
				UASFunction_JIT::StaticClass()
			},
			{
				TEXT("ASFunctionDispatchClassThreadSafeWithOverride"),
				TEXT("ASFunctionDispatchClassThreadSafeWithOverride.as"),
				TEXT("UASFunctionDispatchClassThreadSafeWithOverride"),
				TEXT("class-level BlueprintThreadSafe with function-level NotBlueprintThreadSafe"),
				TEXT(R"AS(
UCLASS(meta = (BlueprintThreadSafe))
class UASFunctionDispatchClassThreadSafeWithOverride : UObject
{
	UFUNCTION(meta = (NotBlueprintThreadSafe))
	int GetValue()
	{
		return 1;
	}
}
)AS"),
				UASFunction_DWordReturn::StaticClass(),
				UASFunction_DWordReturn_JIT::StaticClass()
			}
		};

		ON_SCOPE_EXIT
		{
			for (const ASFunctionDispatchTests::FDispatchCase& TestCase : Cases)
			{
				Engine.DiscardModule(*TestCase.ModuleName.ToString());
			}
			ASTEST_RESET_ENGINE(Engine);
		};

		for (const ASFunctionDispatchTests::FDispatchCase& TestCase : Cases)
		{
			UClass* ScriptClass = CompileScriptModule(
				*TestRunner,
				Engine,
				TestCase.ModuleName,
				TestCase.Filename,
				TestCase.ScriptSource,
				TestCase.GeneratedClassName);
			if (ScriptClass == nullptr)
			{
				return;
			}

			UASFunction* GeneratedFunction = Cast<UASFunction>(FindGeneratedFunction(ScriptClass, TEXT("GetValue")));
			if (!TestRunner->TestNotNull(
					*FString::Printf(TEXT("AllocateFunctionFor %s case should generate GetValue"), TestCase.CaseLabel),
					GeneratedFunction))
			{
				return;
			}

			TestRunner->TestTrue(
				*FString::Printf(
					TEXT("AllocateFunctionFor %s case should select %s (actual: %s)"),
					TestCase.CaseLabel,
					*ASFunctionDispatchTests::DescribeExpectedFunctionClasses(TestCase),
					*ASFunctionDispatchTests::DescribeActualFunctionClass(GeneratedFunction)),
				ASFunctionDispatchTests::MatchesExpectedFunctionClass(*GeneratedFunction, TestCase));

			UObject* Instance = NewObject<UObject>(GetTransientPackage(), ScriptClass);
			if (!TestRunner->TestNotNull(
					*FString::Printf(TEXT("AllocateFunctionFor %s case should instantiate the generated class"), TestCase.CaseLabel),
					Instance))
			{
				return;
			}

			int32 Result = 0;
			if (!TestRunner->TestTrue(
					*FString::Printf(TEXT("AllocateFunctionFor %s case should execute the generated function"), TestCase.CaseLabel),
					ExecuteGeneratedIntEventOnGameThread(&Engine, Instance, GeneratedFunction, Result)))
			{
				return;
			}

			TestRunner->TestEqual(
				*FString::Printf(TEXT("AllocateFunctionFor %s case should keep GetValue returning 1"), TestCase.CaseLabel),
				Result,
				1);
		}

		}
	}

	TEST_METHOD(AllocateFunctionForSelectsRepresentativeDispatchMatrix)
	{
		using namespace ASFunctionDispatchTests;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*MatrixModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UASClass* ScriptClass = ASFunctionDispatchTests::CompileMatrixClass(*TestRunner, Engine);
		if (!TestRunner->TestNotNull(TEXT("AllocateFunctionFor matrix should compile a script class"), ScriptClass))
		{
			return;
		}

		UClass* StaticsClass = FindGeneratedClass(&Engine, MatrixStaticsClassName);
		if (!TestRunner->TestNotNull(TEXT("AllocateFunctionFor matrix should generate the module statics class"), StaticsClass))
		{
			return;
		}

		const TArray<FMatrixCase> InstanceCases =
		{
			{ TEXT("NoParams"), UASFunction_NoParams::StaticClass(), TEXT("void no-param instance function") },
			{ TEXT("TakeByte"), UASFunction_ByteArg::StaticClass(), TEXT("single byte argument") },
			{ TEXT("TakeBool"), UASFunction_ByteArg::StaticClass(), TEXT("single bool argument") },
			{ TEXT("TakeDWord"), UASFunction_DWordArg::StaticClass(), TEXT("single dword argument") },
			{ TEXT("TakeQWord"), UASFunction_QWordArg::StaticClass(), TEXT("single qword argument") },
			{ TEXT("TakeFloat"), UASFunction_FloatArg::StaticClass(), TEXT("single float32 argument") },
			{ TEXT("TakeDouble"), UASFunction_DoubleArg::StaticClass(), TEXT("single float64 argument") },
			{ TEXT("TakeReference"), UASFunction_ReferenceArg::StaticClass(), TEXT("single reference argument") },
			{ TEXT("ReturnByte"), UASFunction_ByteReturn::StaticClass(), TEXT("byte return") },
			{ TEXT("ReturnDWord"), UASFunction_DWordReturn::StaticClass(), TEXT("dword return") },
			{ TEXT("ReturnFloat"), UASFunction_FloatReturn::StaticClass(), TEXT("float32 return") },
			{ TEXT("ReturnDouble"), UASFunction_DoubleReturn::StaticClass(), TEXT("float64 return") },
			{ TEXT("ReturnObject"), UASFunction_ObjectReturn::StaticClass(), TEXT("object return") },
			{ TEXT("GenericTwoArgs"), UASFunction_NotThreadSafe::StaticClass(), TEXT("multi-argument generic fallback") },
			{ TEXT("ThreadSafeReturn"), UASFunction::StaticClass(), TEXT("thread-safe generic wrapper") },
			{ TEXT("VirtualReturn"), UASFunction_DWordReturn::StaticClass(), TEXT("virtual non-JIT wrapper") },
		};

		for (const FMatrixCase& TestCase : InstanceCases)
		{
			if (!ASFunctionDispatchTests::ExpectFunctionClass(*TestRunner, ScriptClass, TestCase))
			{
				return;
			}
		}

		const FMatrixCase StaticCase =
		{
			TEXT("StaticReturn"),
			UASFunction_NotThreadSafe::StaticClass(),
			TEXT("module-level static generic fallback")
		};
		if (!ASFunctionDispatchTests::ExpectFunctionClass(*TestRunner, StaticsClass, StaticCase))
		{
			return;
		}

		UASFunction* ReturnDWordFunction = Cast<UASFunction>(FindGeneratedFunction(ScriptClass, TEXT("ReturnDWord")));
		UObject* Instance = NewObject<UObject>(GetTransientPackage(), ScriptClass, TEXT("ASFunctionDispatchMatrixInstance"));
		if (!TestRunner->TestNotNull(TEXT("AllocateFunctionFor matrix should instantiate the generated class"), Instance)
			|| !TestRunner->TestNotNull(TEXT("AllocateFunctionFor matrix should expose ReturnDWord"), ReturnDWordFunction))
		{
			return;
		}

		int32 Result = 0;
		if (!TestRunner->TestTrue(
				TEXT("AllocateFunctionFor matrix should execute a representative return wrapper"),
				ExecuteGeneratedIntEventOnGameThread(&Engine, Instance, ReturnDWordFunction, Result)))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Representative dispatch matrix wrapper should preserve script behavior"), Result, 11);

		}
	}
};

#endif
