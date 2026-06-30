#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptGlobalFunctionInvoker.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"
#include "Syntax/AngelscriptSyntaxTestHelpers.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageConstTests
// -----------------------------------------------------------------------------
// Coverage landing file for const correctness: local/global const values, const
// methods, readonly input references, const iteration, and negative compile
// checks for illegal mutation.
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageConstTest,
	"Angelscript.TestModule.Coverage.Const",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	template <typename T>
	void ExpectGlobalReturn(FAngelscriptEngine& Engine, asIScriptModule* Module, const TCHAR* Declaration, const T& Expected, const TCHAR* Message)
	{
		if (Module == nullptr)
		{
			TestRunner->AddError(FString::Printf(TEXT("%s: backing module failed to build"), Message));
			return;
		}

		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
		const T Result = Invoker.CallAndReturn<T>();
		TestRunner->TestEqual(Message, Result, Expected);
	}

	TEST_METHOD(ConstValuesMethodsAndReferences)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCoverageConst_ValuesMethodsRefs", ASTEST_AS(R"AS(
			const int GlobalLimit = 12;

			int LocalConstValue()
			{
				const int LocalLimit = 5;
				return LocalLimit + GlobalLimit;
			}

			int AddReadonly(const int&in Amount)
			{
				return 30 + Amount;
			}

			int ConstInRefRead()
			{
				const int Bonus = 34;
				return AddReadonly(Bonus);
			}

			int SumConstArray(const TArray<int>&in Values)
			{
				int Sum = 0;
				for (const int& Value : Values)
				{
					Sum += Value;
				}
				return Sum;
			}

			int ConstContainerRead()
			{
				TArray<int> Values;
				Values.Add(3);
				Values.Add(4);
				Values.Add(5);
				return SumConstArray(Values);
			}
			)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int LocalConstValue()"), 17, TEXT("local and global const values should read"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int ConstInRefRead()"), 64, TEXT("const &in should read caller state"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int ConstContainerRead()"), 12, TEXT("const array reference and const foreach should read elements"));
	}

	TEST_METHOD(PlainScriptClassConstMethodBoundary)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCoverageConst_PlainClassBoundary", ASTEST_AS(R"AS(
			class ConstCounter
			{
				int Value = 0;

				int GetValue() const
				{
					return Value;
				}

				int AddReadonly(const int&in Amount) const
				{
					return Value + Amount;
				}
			}

			int ConstMethodPlainClassBoundary()
			{
				ConstCounter Counter;
				Counter.Value = 30;
				const int Bonus = 4;
				return Counter.GetValue() + Counter.AddReadonly(Bonus);
			}
			)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ASSERT_THAT(IsNotNull(Module, TEXT("plain script class const boundary module should compile")));
		if (Module == nullptr)
		{
			return;
		}

		TestRunner->AddExpectedError(TEXT("Null pointer access"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("ASCoverageConst_PlainClassBoundary"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("ConstMethodPlainClassBoundary"), EAutomationExpectedErrorFlags::Contains, 1);

		ASSERT_THAT(IsTrue(ExecuteAndExpectException(
			*TestRunner,
			Engine,
			*Module,
			TEXT("int ConstMethodPlainClassBoundary()"),
			TEXT("plain script class const method member access currently remains a runtime boundary"),
			TEXT("Null pointer access"))));
	}

	TEST_METHOD(ConstUFunctionAndPropertyReflection)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageConst_UFunctionProperty"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageConstUFunctionProperty.as"),
			ASTEST_AS(R"AS(
				UCLASS()
				class ACoverageConstActor : AActor
				{
					UPROPERTY()
					int Value = 21;

					UPROPERTY()
					int Observed = 0;

					UFUNCTION()
					int GetValue() const
					{
						return Value;
					}

					UFUNCTION()
					int AddConstParam(const int&in Amount) const
					{
						return Value + Amount;
					}

					UFUNCTION(BlueprintOverride)
					void BeginPlay()
					{
						const int Bonus = 8;
						Observed = GetValue() + AddConstParam(Bonus);
					}
				}
				)AS"),
			TEXT("ACoverageConstActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("const actor should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("const actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Observed"), 50, TEXT("const UFUNCTIONs should execute without mutating state"));
	}

	TEST_METHOD(ConstViolationNegativeCompile)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ModifyLocalSource = ASTEST_AS(R"AS(
			void Test()
			{
				const int Value = 1;
				Value = 2;
			}
			)AS");
		SyntaxTestHelpers::AssertFailsToCompile(
			*TestRunner,
			Engine,
			TEXT("ASCoverageConst_ModifyLocal"),
			*ModifyLocalSource,
			TEXT("modifying a const local should fail"));

		const FString ModifyParamSource = ASTEST_AS(R"AS(
			void Test(const int Value)
			{
				Value = 2;
			}
			)AS");
		SyntaxTestHelpers::AssertFailsToCompile(
			*TestRunner,
			Engine,
			TEXT("ASCoverageConst_ModifyParam"),
			*ModifyParamSource,
			TEXT("modifying a const value parameter should fail"));

		const FString ConstMethodMutatesMemberSource = ASTEST_AS(R"AS(
			class ConstMutationProbe
			{
				int Value = 0;

				void Mutate() const
				{
					Value = 2;
				}
			}
			)AS");
		SyntaxTestHelpers::AssertFailsToCompile(
			*TestRunner,
			Engine,
			TEXT("ASCoverageConst_ConstMethodMutatesMember"),
			*ConstMethodMutatesMemberSource,
			TEXT("mutating a member from const method should fail"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
