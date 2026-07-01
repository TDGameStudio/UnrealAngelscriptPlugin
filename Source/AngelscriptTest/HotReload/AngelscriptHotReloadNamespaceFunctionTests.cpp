#include "CQTest.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Async/Async.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UObjectGlobals.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadNamespaceFunctionTests,
	"Angelscript.TestModule.HotReload.NamespaceFunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FSingleIntParamAndReturnValue
	{
		int32 X = 0;
		int32 ReturnValue = 0;
	};

	struct FTwoIntParamsAndReturnValue
	{
		int32 X = 0;
		int32 Y = 0;
		int32 ReturnValue = 0;
	};

	static bool IsHandledReloadResult(const ECompileResult ReloadResult)
	{
		return ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled;
	}

	static bool ExecuteGeneratedIntFunctionOnGameThread(
		FAngelscriptEngine& Engine,
		UObject* Object,
		UFunction* Function,
		const int32 InputValue,
		int32& OutResult)
	{
		if (!::IsValid(Object) || Function == nullptr)
		{
			return false;
		}

		auto Invoke = [&Engine, Object, Function, InputValue, &OutResult]()
		{
			FSingleIntParamAndReturnValue Params;
			Params.X = InputValue;

			FAngelscriptEngineScope EngineScope(Engine, Object);
			Object->ProcessEvent(Function, &Params);
			OutResult = Params.ReturnValue;
		};

		if (IsInGameThread())
		{
			Invoke();
			return true;
		}

		FEvent* CompletedEvent = FPlatformProcess::GetSynchEventFromPool(true);
		AsyncTask(ENamedThreads::GameThread, [Invoke, CompletedEvent]() mutable
		{
			Invoke();
			CompletedEvent->Trigger();
		});

		CompletedEvent->Wait();
		FPlatformProcess::ReturnSynchEventToPool(CompletedEvent);
		return true;
	}

	static bool ExecuteGeneratedIntFunctionOnGameThread(
		FAngelscriptEngine& Engine,
		UObject* Object,
		UFunction* Function,
		const int32 FirstInputValue,
		const int32 SecondInputValue,
		int32& OutResult)
	{
		if (!::IsValid(Object) || Function == nullptr)
		{
			return false;
		}

		auto Invoke = [&Engine, Object, Function, FirstInputValue, SecondInputValue, &OutResult]()
		{
			FTwoIntParamsAndReturnValue Params;
			Params.X = FirstInputValue;
			Params.Y = SecondInputValue;

			FAngelscriptEngineScope EngineScope(Engine, Object);
			Object->ProcessEvent(Function, &Params);
			OutResult = Params.ReturnValue;
		};

		if (IsInGameThread())
		{
			Invoke();
			return true;
		}

		FEvent* CompletedEvent = FPlatformProcess::GetSynchEventFromPool(true);
		AsyncTask(ENamedThreads::GameThread, [Invoke, CompletedEvent]() mutable
		{
			Invoke();
			CompletedEvent->Trigger();
		});

		CompletedEvent->Wait();
		FPlatformProcess::ReturnSynchEventToPool(CompletedEvent);
		return true;
	}

	static FAngelscriptEngine* GetProductionEngine(FAutomationTestBase& Test)
	{
		return RequireRunningProductionEngine(
			Test,
			TEXT("HotReload namespace function tests require a production engine."));
	}

	static bool CompileInitialScript(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FName ModuleName,
		const FString& Filename,
		const FString& ScriptV1,
		const TCHAR* Context)
	{
		ECompileResult InitialCompileResult = ECompileResult::Error;
		return Test.TestTrue(
			*FString::Printf(TEXT("%s should compile V1 on the full reload path"), Context),
			CompileModuleWithResult(
				&Engine,
				ECompileType::FullReload,
				ModuleName,
				Filename,
				ScriptV1,
				InitialCompileResult));
	}

	static bool CompileSoftReloadScript(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FName ModuleName,
		const FString& Filename,
		const FString& ScriptV2,
		const TCHAR* Context)
	{
		ECompileResult ReloadCompileResult = ECompileResult::Error;
		if (!Test.TestTrue(
				*FString::Printf(TEXT("%s should compile V2 on the soft reload path"), Context),
				CompileModuleWithResult(
					&Engine,
					ECompileType::SoftReloadOnly,
					ModuleName,
					Filename,
					ScriptV2,
					ReloadCompileResult)))
		{
			return false;
		}

		return Test.TestTrue(
			*FString::Printf(TEXT("%s should stay on a handled reload path"), Context),
			IsHandledReloadResult(ReloadCompileResult));
	}

	static UClass* FindClassAndFunction(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FName ClassName,
		const FName FunctionName,
		UFunction*& OutFunction,
		const TCHAR* Context)
	{
		UClass* ScriptClass = FindGeneratedClass(&Engine, ClassName);
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("%s should expose the generated class"), Context),
				ScriptClass))
		{
			return nullptr;
		}

		OutFunction = FindGeneratedFunction(ScriptClass, FunctionName);
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("%s should expose the expected function"), Context),
				OutFunction))
		{
			return nullptr;
		}

		return ScriptClass;
	}

	static UObject* CreateCarrierObject(FAutomationTestBase& Test, UClass* ScriptClass, const TCHAR* Context)
	{
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("%s should have a carrier class"), Context),
				ScriptClass))
		{
			return nullptr;
		}

		UObject* Object = NewObject<UObject>(GetTransientPackage(), ScriptClass);
		Test.TestNotNull(
			*FString::Printf(TEXT("%s should create a carrier object"), Context),
			Object);
		return Object;
	}

	static bool ExpectSingleIntResult(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		UObject* Object,
		UFunction* Function,
		const int32 InputValue,
		const int32 ExpectedValue,
		const TCHAR* Context)
	{
		int32 ActualValue = 0;
		if (!Test.TestTrue(
				*FString::Printf(TEXT("%s should execute the generated function"), Context),
				ExecuteGeneratedIntFunctionOnGameThread(Engine, Object, Function, InputValue, ActualValue)))
		{
			return false;
		}

		return Test.TestEqual(
			*FString::Printf(TEXT("%s should return the expected value"), Context),
			ActualValue,
			ExpectedValue);
	}

	static bool ExpectTwoIntResult(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		UObject* Object,
		UFunction* Function,
		const int32 FirstInputValue,
		const int32 SecondInputValue,
		const int32 ExpectedValue,
		const TCHAR* Context)
	{
		int32 ActualValue = 0;
		if (!Test.TestTrue(
				*FString::Printf(TEXT("%s should execute the generated function"), Context),
				ExecuteGeneratedIntFunctionOnGameThread(Engine, Object, Function, FirstInputValue, SecondInputValue, ActualValue)))
		{
			return false;
		}

		return Test.TestEqual(
			*FString::Printf(TEXT("%s should return the expected value"), Context),
			ActualValue,
			ExpectedValue);
	}

public:
	TEST_METHOD(SoftReloadUpdatesNamespaceFunctionDispatch)
	{
		FAngelscriptEngine* ProductionEngine = GetProductionEngine(*TestRunner);
		if (ProductionEngine == nullptr)
		{
			return;
		}

		FAngelscriptEngine& Engine = *ProductionEngine;
		const FName ModuleName(TEXT("HotReloadNamespaceFunctionBasic"));
		const FString Filename(TEXT("HotReloadNamespaceFunctionBasic.as"));
		const FName ClassName(TEXT("UHotReloadNamespaceFunctionBasicCarrier"));
		const FName FunctionName(TEXT("ComputeSquare"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptV1 = ASTEST_AS(R"AS(
			namespace HotReloadNamespaceFunctionBasic
			{
				int Square(int X)
				{
					return X * X;
				}
			}

			UCLASS()
			class UHotReloadNamespaceFunctionBasicCarrier : UObject
			{
				UFUNCTION()
				int ComputeSquare(int X)
				{
					return HotReloadNamespaceFunctionBasic::Square(X);
				}
			}
			)AS");

		const FString ScriptV2 = ASTEST_AS(R"AS(
			namespace HotReloadNamespaceFunctionBasic
			{
				int Square(int X)
				{
					return X * X + 1;
				}
			}

			UCLASS()
			class UHotReloadNamespaceFunctionBasicCarrier : UObject
			{
				UFUNCTION()
				int ComputeSquare(int X)
				{
					return HotReloadNamespaceFunctionBasic::Square(X);
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileInitialScript(*TestRunner, Engine, ModuleName, Filename, ScriptV1, TEXT("Basic namespace function reload"))));

		UFunction* FunctionBeforeReload = nullptr;
		UClass* ClassBeforeReload = FindClassAndFunction(*TestRunner, Engine, ClassName, FunctionName, FunctionBeforeReload, TEXT("Basic namespace function before reload"));
		ASSERT_THAT(IsNotNull(ClassBeforeReload));

		UObject* ExistingObject = CreateCarrierObject(*TestRunner, ClassBeforeReload, TEXT("Basic namespace function before reload"));
		ASSERT_THAT(IsNotNull(ExistingObject));
		ASSERT_THAT(IsTrue(ExpectSingleIntResult(*TestRunner, Engine, ExistingObject, FunctionBeforeReload, 3, 9, TEXT("Basic namespace function before reload"))));

		ASSERT_THAT(IsTrue(CompileSoftReloadScript(*TestRunner, Engine, ModuleName, Filename, ScriptV2, TEXT("Basic namespace function reload"))));

		UFunction* FunctionAfterReload = nullptr;
		UClass* ClassAfterReload = FindClassAndFunction(*TestRunner, Engine, ClassName, FunctionName, FunctionAfterReload, TEXT("Basic namespace function after reload"));
		ASSERT_THAT(IsNotNull(ClassAfterReload));

		UObject* NewObjectAfterReload = CreateCarrierObject(*TestRunner, ClassAfterReload, TEXT("Basic namespace function after reload"));
		ASSERT_THAT(IsNotNull(NewObjectAfterReload));
		ASSERT_THAT(IsTrue(ExpectSingleIntResult(*TestRunner, Engine, NewObjectAfterReload, FunctionAfterReload, 3, 10, TEXT("Basic namespace function new object after reload"))));
		ASSERT_THAT(IsTrue(ExpectSingleIntResult(*TestRunner, Engine, ExistingObject, FunctionAfterReload, 3, 10, TEXT("Basic namespace function existing object after reload"))));
	}

	TEST_METHOD(SoftReloadUpdatesNestedNamespaceFunctionDispatch)
	{
		FAngelscriptEngine* ProductionEngine = GetProductionEngine(*TestRunner);
		if (ProductionEngine == nullptr)
		{
			return;
		}

		FAngelscriptEngine& Engine = *ProductionEngine;
		const FName ModuleName(TEXT("HotReloadNamespaceFunctionNested"));
		const FString Filename(TEXT("HotReloadNamespaceFunctionNested.as"));
		const FName ClassName(TEXT("UHotReloadNamespaceFunctionNestedCarrier"));
		const FName FunctionName(TEXT("ApplyNestedRule"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptV1 = ASTEST_AS(R"AS(
			namespace HotReloadNamespaceFunctionNested
			{
				namespace Math
				{
					int Apply(int X)
					{
						return X + 4;
					}
				}
			}

			UCLASS()
			class UHotReloadNamespaceFunctionNestedCarrier : UObject
			{
				UFUNCTION()
				int ApplyNestedRule(int X)
				{
					return HotReloadNamespaceFunctionNested::Math::Apply(X);
				}
			}
			)AS");

		const FString ScriptV2 = ASTEST_AS(R"AS(
			namespace HotReloadNamespaceFunctionNested
			{
				namespace Math
				{
					int Apply(int X)
					{
						return X + 14;
					}
				}
			}

			UCLASS()
			class UHotReloadNamespaceFunctionNestedCarrier : UObject
			{
				UFUNCTION()
				int ApplyNestedRule(int X)
				{
					return HotReloadNamespaceFunctionNested::Math::Apply(X);
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileInitialScript(*TestRunner, Engine, ModuleName, Filename, ScriptV1, TEXT("Nested namespace function reload"))));

		UFunction* FunctionBeforeReload = nullptr;
		UClass* ClassBeforeReload = FindClassAndFunction(*TestRunner, Engine, ClassName, FunctionName, FunctionBeforeReload, TEXT("Nested namespace function before reload"));
		ASSERT_THAT(IsNotNull(ClassBeforeReload));

		UObject* ExistingObject = CreateCarrierObject(*TestRunner, ClassBeforeReload, TEXT("Nested namespace function before reload"));
		ASSERT_THAT(IsNotNull(ExistingObject));
		ASSERT_THAT(IsTrue(ExpectSingleIntResult(*TestRunner, Engine, ExistingObject, FunctionBeforeReload, 6, 10, TEXT("Nested namespace function before reload"))));

		ASSERT_THAT(IsTrue(CompileSoftReloadScript(*TestRunner, Engine, ModuleName, Filename, ScriptV2, TEXT("Nested namespace function reload"))));

		UFunction* FunctionAfterReload = nullptr;
		UClass* ClassAfterReload = FindClassAndFunction(*TestRunner, Engine, ClassName, FunctionName, FunctionAfterReload, TEXT("Nested namespace function after reload"));
		ASSERT_THAT(IsNotNull(ClassAfterReload));
		ASSERT_THAT(IsTrue(ExpectSingleIntResult(*TestRunner, Engine, ExistingObject, FunctionAfterReload, 6, 20, TEXT("Nested namespace function existing object after reload"))));
	}

	TEST_METHOD(SoftReloadPreservesNamespaceOverloadDispatch)
	{
		FAngelscriptEngine* ProductionEngine = GetProductionEngine(*TestRunner);
		if (ProductionEngine == nullptr)
		{
			return;
		}

		FAngelscriptEngine& Engine = *ProductionEngine;
		const FName ModuleName(TEXT("HotReloadNamespaceFunctionOverload"));
		const FString Filename(TEXT("HotReloadNamespaceFunctionOverload.as"));
		const FName ClassName(TEXT("UHotReloadNamespaceFunctionOverloadCarrier"));
		const FName SingleFunctionName(TEXT("UseSingle"));
		const FName PairFunctionName(TEXT("UsePair"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptV1 = ASTEST_AS(R"AS(
			namespace HotReloadNamespaceFunctionOverload
			{
				int Mix(int X)
				{
					return X + 1;
				}

				int Mix(int X, int Y)
				{
					return X + Y;
				}
			}

			UCLASS()
			class UHotReloadNamespaceFunctionOverloadCarrier : UObject
			{
				UFUNCTION()
				int UseSingle(int X)
				{
					return HotReloadNamespaceFunctionOverload::Mix(X);
				}

				UFUNCTION()
				int UsePair(int X, int Y)
				{
					return HotReloadNamespaceFunctionOverload::Mix(X, Y);
				}
			}
			)AS");

		const FString ScriptV2 = ASTEST_AS(R"AS(
			namespace HotReloadNamespaceFunctionOverload
			{
				int Mix(int X)
				{
					return X + 11;
				}

				int Mix(int X, int Y)
				{
					return X * Y;
				}
			}

			UCLASS()
			class UHotReloadNamespaceFunctionOverloadCarrier : UObject
			{
				UFUNCTION()
				int UseSingle(int X)
				{
					return HotReloadNamespaceFunctionOverload::Mix(X);
				}

				UFUNCTION()
				int UsePair(int X, int Y)
				{
					return HotReloadNamespaceFunctionOverload::Mix(X, Y);
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileInitialScript(*TestRunner, Engine, ModuleName, Filename, ScriptV1, TEXT("Namespace overload reload"))));

		UFunction* SingleFunctionBeforeReload = nullptr;
		UClass* ClassBeforeReload = FindClassAndFunction(*TestRunner, Engine, ClassName, SingleFunctionName, SingleFunctionBeforeReload, TEXT("Namespace overload single before reload"));
		ASSERT_THAT(IsNotNull(ClassBeforeReload));

		UFunction* PairFunctionBeforeReload = FindGeneratedFunction(ClassBeforeReload, PairFunctionName);
		ASSERT_THAT(IsNotNull(PairFunctionBeforeReload, TEXT("Namespace overload pair before reload should expose UsePair")));

		UObject* ExistingObject = CreateCarrierObject(*TestRunner, ClassBeforeReload, TEXT("Namespace overload before reload"));
		ASSERT_THAT(IsNotNull(ExistingObject));
		ASSERT_THAT(IsTrue(ExpectSingleIntResult(*TestRunner, Engine, ExistingObject, SingleFunctionBeforeReload, 5, 6, TEXT("Namespace overload single before reload"))));
		ASSERT_THAT(IsTrue(ExpectTwoIntResult(*TestRunner, Engine, ExistingObject, PairFunctionBeforeReload, 5, 3, 8, TEXT("Namespace overload pair before reload"))));

		ASSERT_THAT(IsTrue(CompileSoftReloadScript(*TestRunner, Engine, ModuleName, Filename, ScriptV2, TEXT("Namespace overload reload"))));

		UFunction* SingleFunctionAfterReload = nullptr;
		UClass* ClassAfterReload = FindClassAndFunction(*TestRunner, Engine, ClassName, SingleFunctionName, SingleFunctionAfterReload, TEXT("Namespace overload single after reload"));
		ASSERT_THAT(IsNotNull(ClassAfterReload));

		UFunction* PairFunctionAfterReload = FindGeneratedFunction(ClassAfterReload, PairFunctionName);
		ASSERT_THAT(IsNotNull(PairFunctionAfterReload, TEXT("Namespace overload pair after reload should expose UsePair")));

		ASSERT_THAT(IsTrue(ExpectSingleIntResult(*TestRunner, Engine, ExistingObject, SingleFunctionAfterReload, 5, 16, TEXT("Namespace overload single existing object after reload"))));
		ASSERT_THAT(IsTrue(ExpectTwoIntResult(*TestRunner, Engine, ExistingObject, PairFunctionAfterReload, 5, 3, 15, TEXT("Namespace overload pair existing object after reload"))));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
