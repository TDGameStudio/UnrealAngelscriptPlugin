#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "ClassGenerator/ASClass.h"
#include "Misc/ScopeExit.h"
#include "UObject/StructOnScope.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_UNITTESTS

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
		FString ScriptSource;
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
		FNoDiscardAsserter LocalAssert(Test);
		UASFunction* Function = Cast<UASFunction>(::FindGeneratedFunction(OwnerClass, TestCase.FunctionName));
		if (!LocalAssert.IsNotNull(
				Function,
				*FString::Printf(TEXT("AllocateFunctionFor matrix should expose '%s'"), TestCase.FunctionName)))
		{
			return false;
		}

		return LocalAssert.IsTrue(
			Function->GetClass() == TestCase.ExpectedFunctionClass,
			*FString::Printf(
				TEXT("AllocateFunctionFor %s should select %s (actual: %s)"),
				TestCase.CaseLabel,
				*GetNameSafe(TestCase.ExpectedFunctionClass),
				*DescribeActualFunctionClass(Function)));
	}

	UASClass* CompileMatrixClass(FAutomationTestBase& Test, FAngelscriptEngine& Engine)
	{
		const FString ScriptSource = ASTEST_AS(R"AS(
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

		return Cast<UASClass>(AngelscriptFunctionalTestUtils::CompileScriptModule(
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

	TEST_METHOD(AllocateFunctionForSelectsCorrectThreadSafeDispatchSubclass)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		const TArray<ASFunctionDispatchTests::FDispatchCase> Cases =
		{
			{
				TEXT("ASFunctionDispatchDefault"),
				TEXT("ASFunctionDispatchDefault.as"),
				TEXT("UASFunctionDispatchDefault"),
				TEXT("default non-thread-safe"),
				ASTEST_AS(R"AS(
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
				ASTEST_AS(R"AS(
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
				ASTEST_AS(R"AS(
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
		};

		for (const ASFunctionDispatchTests::FDispatchCase& TestCase : Cases)
		{
			UClass* ScriptClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
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

			UASFunction* GeneratedFunction = Cast<UASFunction>(::FindGeneratedFunction(ScriptClass, TEXT("GetValue")));
			if (!this->Assert.IsNotNull(
					GeneratedFunction,
					*FString::Printf(TEXT("AllocateFunctionFor %s case should generate GetValue"), TestCase.CaseLabel)))
			{
				return;
			}

			ASSERT_THAT(IsTrue(
				ASFunctionDispatchTests::MatchesExpectedFunctionClass(*GeneratedFunction, TestCase),
				*FString::Printf(
					TEXT("AllocateFunctionFor %s case should select %s (actual: %s)"),
					TestCase.CaseLabel,
					*ASFunctionDispatchTests::DescribeExpectedFunctionClasses(TestCase),
					*ASFunctionDispatchTests::DescribeActualFunctionClass(GeneratedFunction))));

			UObject* Instance = NewObject<UObject>(GetTransientPackage(), ScriptClass);
			if (!this->Assert.IsNotNull(
					Instance,
					*FString::Printf(TEXT("AllocateFunctionFor %s case should instantiate the generated class"), TestCase.CaseLabel)))
			{
				return;
			}

			int32 Result = 0;
			if (!this->Assert.IsTrue(
					::ExecuteGeneratedIntEventOnGameThread(&Engine, Instance, GeneratedFunction, Result),
					*FString::Printf(TEXT("AllocateFunctionFor %s case should execute the generated function"), TestCase.CaseLabel)))
			{
				return;
			}

			ASSERT_THAT(AreEqual(
				1,
				Result,
				*FString::Printf(TEXT("AllocateFunctionFor %s case should keep GetValue returning 1"), TestCase.CaseLabel)));
		}

	}

	TEST_METHOD(AllocateFunctionForSelectsRepresentativeDispatchMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ASFunctionDispatchTests::MatrixModuleName.ToString());
		};

		UASClass* ScriptClass = ASFunctionDispatchTests::CompileMatrixClass(*TestRunner, Engine);
		if (!this->Assert.IsNotNull(ScriptClass, TEXT("AllocateFunctionFor matrix should compile a script class")))
		{
			return;
		}

		UClass* StaticsClass = ::FindGeneratedClass(&Engine, ASFunctionDispatchTests::MatrixStaticsClassName);
		if (!this->Assert.IsNotNull(StaticsClass, TEXT("AllocateFunctionFor matrix should generate the module statics class")))
		{
			return;
		}

		const TArray<ASFunctionDispatchTests::FMatrixCase> InstanceCases =
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

		for (const ASFunctionDispatchTests::FMatrixCase& TestCase : InstanceCases)
		{
			if (!ASFunctionDispatchTests::ExpectFunctionClass(*TestRunner, ScriptClass, TestCase))
			{
				return;
			}
		}

		const ASFunctionDispatchTests::FMatrixCase StaticCase =
		{
			TEXT("StaticReturn"),
			UASFunction_NotThreadSafe::StaticClass(),
			TEXT("module-level static generic fallback")
		};
		if (!ASFunctionDispatchTests::ExpectFunctionClass(*TestRunner, StaticsClass, StaticCase))
		{
			return;
		}

		UASFunction* ReturnDWordFunction = Cast<UASFunction>(::FindGeneratedFunction(ScriptClass, TEXT("ReturnDWord")));
		UObject* Instance = NewObject<UObject>(GetTransientPackage(), ScriptClass, TEXT("ASFunctionDispatchMatrixInstance"));
		if (!this->Assert.IsNotNull(Instance, TEXT("AllocateFunctionFor matrix should instantiate the generated class"))
			|| !this->Assert.IsNotNull(ReturnDWordFunction, TEXT("AllocateFunctionFor matrix should expose ReturnDWord")))
		{
			return;
		}

		int32 Result = 0;
		if (!this->Assert.IsTrue(
				::ExecuteGeneratedIntEventOnGameThread(&Engine, Instance, ReturnDWordFunction, Result),
				TEXT("AllocateFunctionFor matrix should execute a representative return wrapper")))
		{
			return;
		}

		ASSERT_THAT(AreEqual(11, Result, TEXT("Representative dispatch matrix wrapper should preserve script behavior")));

	}
};

#endif
