#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptGlobalFunctionInvoker.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestWorld.h"
#include "Syntax/AngelscriptSyntaxTestHelpers.h"

#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageNamespaceTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript namespaces and scopes:
//   - namespace declaration (nested namespaces)
//   - using directive
//   - fully qualified names (Namespace::Symbol)
//   - variable scope (function, block, for loop)
//   - variable shadowing
//
// Test patterns: Pattern B/F (global functions)
// -----------------------------------------------------------------------------

#if WITH_ANGELSCRIPT_UNITTESTS

using namespace AngelscriptFunctionalTestUtils;

namespace AngelscriptCoverageNamespaceTests_NS
{
	TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageNamespaceTest,
		"Angelscript.TestModule.Coverage.Namespace",
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

		// Helper
		template <typename T, typename... ArgTypes>
		void ExpectGlobalReturn(
			FAngelscriptEngine& Engine,
			asIScriptModule* Module,
			const TCHAR* Declaration,
			const T& Expected,
			const TCHAR* Message,
			ArgTypes... Args)
		{
			if (Module == nullptr)
			{
				TestRunner->AddError(FString::Printf(TEXT("%s: backing module failed to build"), Message));
				return;
			}

			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("namespace global function should resolve and prepare")));
			if (!Invoker.IsValid())
			{
				return;
			}

			(Invoker.AddArg(Args), ...);
			const T Result = Invoker.ExecuteAndGet<T>();
			ASSERT_THAT(AreEqual(Expected, Result, Message));
		}

		// -------------------------------------------------------------------------
		// namespace declaration
		// -------------------------------------------------------------------------
		TEST_METHOD(NamespaceDeclaration)
		{
			FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
			FAngelscriptEngineScope Scope(Engine);

			asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovNamespace_Declaration", ASTEST_AS(R"AS(
			// Global namespace
			int GlobalFunction()
			{
				return 100;
			}

			// Declare namespace
			namespace MyNamespace
			{
				int NamespacedFunction()
				{
					return 200;
				}

				const int NamespacedConstant = 42;

				int GetConstant()
				{
					return NamespacedConstant;
				}
			}

			// Another namespace
			namespace OtherNamespace
			{
				int OtherFunction()
				{
					return 300;
				}
			}

			// Use namespaced functions
			int UseNamespacedFunctions()
			{
				return MyNamespace::NamespacedFunction() + OtherNamespace::OtherFunction();
			}

			// Access global from namespace
			namespace AccessGlobal
			{
				int CallGlobalFunction()
				{
					return GlobalFunction();
				}
			}
			)AS"));
			ON_SCOPE_EXIT
			{
				if (Module != nullptr)
				{
					Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
				}
			};

			ExpectGlobalReturn<int>(Engine, Module, TEXT("int GlobalFunction()"), 100, TEXT("global function"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int UseNamespacedFunctions()"), 500, TEXT("use namespaced functions"));
		}

		// -------------------------------------------------------------------------
		// Nested namespaces
		// -------------------------------------------------------------------------
		TEST_METHOD(NamespaceNested)
		{
			FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
			FAngelscriptEngineScope Scope(Engine);

			asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovNamespace_Nested", ASTEST_AS(R"AS(
			// Nested namespaces
			namespace Outer
			{
				const int OuterValue = 10;

				int OuterFunction()
				{
					return 100;
				}

				namespace Inner
				{
					const int InnerValue = 20;

					int InnerFunction()
					{
						return 200;
					}

					// Access outer from inner
					int AccessOuter()
					{
						return Outer::OuterFunction();
					}
				}

				// Access inner from outer
				int AccessInner()
				{
					return Inner::InnerFunction();
				}
			}

			// Access nested namespace from global
			int AccessNested()
			{
				return Outer::Inner::InnerFunction() + Outer::OuterFunction();
			}

			// Multiple nested levels
			namespace Level1
			{
				namespace Level2
				{
					namespace Level3
					{
						int DeepFunction()
						{
							return 999;
						}
					}
				}
			}

			int AccessDeepNested()
			{
				return Level1::Level2::Level3::DeepFunction();
			}
			)AS"));
			ON_SCOPE_EXIT
			{
				if (Module != nullptr)
				{
					Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
				}
			};

			ExpectGlobalReturn<int>(Engine, Module, TEXT("int AccessNested()"), 300, TEXT("access nested namespace"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int AccessDeepNested()"), 999, TEXT("access deep nested namespace"));
		}

		// -------------------------------------------------------------------------
		// using directive
		// -------------------------------------------------------------------------
		TEST_METHOD(NamespaceUsing)
		{
			FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
			FAngelscriptEngineScope Scope(Engine);

			const FString UsingSpecificSource = ASTEST_AS(R"AS(
				namespace Math
				{
					int Add(int A, int B)
					{
						return A + B;
					}
				}

				int UseSpecificFunction()
				{
					using Math::Add;
					return Add(10, 20);
				}
				)AS");
			ASSERT_THAT(IsTrue(SyntaxTestHelpers::AssertFailsWithError(
				*TestRunner,
				Engine,
				TEXT("ASCovNamespace_UsingSpecificUnsupported"),
				*UsingSpecificSource,
				TEXT("Expected ';'"),
				TEXT("using Namespace::Symbol is not supported by this fork"))));

			const FString UsingNamespaceSource = ASTEST_AS(R"AS(
				namespace Math
				{
					int Add(int A, int B)
					{
						return A + B;
					}
				}

				int UseNamespace()
				{
					using namespace Math;
					return Add(10, 20);
				}
				)AS");
			ASSERT_THAT(IsTrue(SyntaxTestHelpers::AssertFailsWithError(
				*TestRunner,
				Engine,
				TEXT("ASCovNamespace_UsingNamespaceUnsupported"),
				*UsingNamespaceSource,
				TEXT("Expected ';'"),
				TEXT("using namespace is not supported by this fork"))));
		}

		// -------------------------------------------------------------------------
		// Fully qualified names
		// -------------------------------------------------------------------------
		TEST_METHOD(NamespaceQualifiedName)
		{
			FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
			FAngelscriptEngineScope Scope(Engine);

			asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovNamespace_QualifiedName", ASTEST_AS(R"AS(
			namespace Alpha
			{
				const int Value = 100;

				int GetValue()
				{
					return Value;
				}
			}

			namespace Beta
			{
				const int Value = 200;

				int GetValue()
				{
					return Value;
				}
			}

			// Use fully qualified names to disambiguate
			int UseQualifiedNames()
			{
				return Alpha::GetValue() + Beta::GetValue();
			}

			// Access constants with qualified names
			namespace Constants
			{
				const int MAX_SIZE = 100;
				const int MIN_SIZE = 10;
			}

			int UseConstants()
			{
				return Constants::MAX_SIZE + Constants::MIN_SIZE;
			}

			// Same name in different namespaces
			namespace NS1
			{
				int Calculate(int X)
				{
					return X * 2;
				}
			}

			namespace NS2
			{
				int Calculate(int X)
				{
					return X * 3;
				}
			}

			int UseSameNameDifferentNamespace()
			{
				return NS1::Calculate(10) + NS2::Calculate(10);
			}
			)AS"));
			ON_SCOPE_EXIT
			{
				if (Module != nullptr)
				{
					Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
				}
			};

			ExpectGlobalReturn<int>(Engine, Module, TEXT("int UseQualifiedNames()"), 300, TEXT("use qualified names"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int UseConstants()"), 110, TEXT("use constants with qualified names"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int UseSameNameDifferentNamespace()"), 50, TEXT("same name different namespace"));
		}

		// -------------------------------------------------------------------------
		// Variable scope
		// -------------------------------------------------------------------------
		TEST_METHOD(ScopeVariables)
		{
			FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
			FAngelscriptEngineScope Scope(Engine);

			asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovNamespace_ScopeVariables", ASTEST_AS(R"AS(
			const int GlobalScopeValue = 7;

			namespace ScopeNS
			{
				const int NamespacedScopeValue = 11;

				int ReadNamespacedScope()
				{
					return NamespacedScopeValue;
				}
			}

			// Function scope
			int FunctionScope()
			{
				int X = 10;
				int Y = 20;
				return X + Y;
			}

			// Block scope
			int BlockScope()
			{
				int X = 10;
				{
					int Y = 20;
					X = X + Y;
				}
				// Y is not accessible here
				return X;
			}

			// For loop scope
			int ForLoopScope()
			{
				int Sum = 0;
				for (int i = 0; i < 5; i++)
				{
					// i is only accessible in loop
					Sum += i;
				}
				// i is not accessible here
				return Sum;
			}

			// Multiple block scopes
			int MultipleBlockScopes()
			{
				int Result = 0;
				{
					int X = 10;
					Result += X;
				}
				{
					int X = 20;  // Different X
					Result += X;
				}
				return Result;
			}

			// Nested block scopes
			int NestedBlockScopes()
			{
				int X = 10;
				{
					int Y = 20;
					{
						int Z = 30;
						X = X + Y + Z;
					}
				}
				return X;
			}

			// If statement scope
			int IfStatementScope(bool Condition)
			{
				int Result = 0;
				if (Condition)
				{
					int X = 100;
					Result = X;
				}
				else
				{
					int X = 200;  // Different X
					Result = X;
				}
				return Result;
			}

			// While loop scope
			int WhileLoopScope()
			{
				int Sum = 0;
				int i = 0;
				while (i < 5)
				{
					int Temp = i * 2;
					Sum += Temp;
					i++;
				}
				// Temp is not accessible here
				return Sum;
			}

			int GlobalAndNamespaceScope()
			{
				return GlobalScopeValue + ScopeNS::ReadNamespacedScope();
			}
			)AS"));
			ON_SCOPE_EXIT
			{
				if (Module != nullptr)
				{
					Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
				}
			};

			ExpectGlobalReturn<int>(Engine, Module, TEXT("int FunctionScope()"), 30, TEXT("function scope"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int BlockScope()"), 30, TEXT("block scope"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int ForLoopScope()"), 10, TEXT("for loop scope"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int MultipleBlockScopes()"), 30, TEXT("multiple block scopes"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int NestedBlockScopes()"), 60, TEXT("nested block scopes"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int IfStatementScope(bool)"), 100, TEXT("if statement scope"), true);
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int WhileLoopScope()"), 20, TEXT("while loop scope"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int GlobalAndNamespaceScope()"), 18, TEXT("global and namespace scope"));
		}

		// -------------------------------------------------------------------------
		// Variable shadowing
		// -------------------------------------------------------------------------
		TEST_METHOD(ScopeShadowing)
		{
			FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
			FAngelscriptEngineScope Scope(Engine);

			TestRunner->AddExpectedError(
				TEXT("shadows another variable of same name in outer scope"),
				EAutomationExpectedErrorFlags::Contains,
				0);

			asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovNamespace_ScopeShadowing", ASTEST_AS(R"AS(
			UCLASS()
			class AScopeShadowMemberActor : AActor
			{
				UPROPERTY()
				int Value = 100;

				UFUNCTION()
				int ParameterShadowsMember(int Value)
				{
					return Value + this.Value;
				}
			}

			// Global constant
			const int GlobalValue = 100;

			// Local shadows global
			int LocalShadowsGlobal()
			{
				int GlobalValue = 50;  // Shadows global
				return GlobalValue;
			}

			// Inner block shadows outer
			int InnerShadowsOuter()
			{
				int X = 10;
				{
					int X = 20;  // Shadows outer X
					return X;
				}
			}

			// Multiple levels of shadowing
			int MultipleShadowLevels()
			{
				int Value = 1;
				{
					int Value = 2;  // Shadow level 1
					{
						int Value = 3;  // Shadow level 2
						return Value;
					}
				}
			}

			// Shadowing in loop
			int ShadowingInLoop()
			{
				int i = 100;
				int Sum = 0;
				for (int i = 0; i < 5; i++)  // Shadows outer i
				{
					Sum += i;
				}
				return i;  // Returns outer i (100)
			}

			// Shadowing with same type
			int ShadowingSameType()
			{
				int Value = 10;
				{
					int Value = 20;
					{
						int Value = 30;
						return Value;
					}
				}
			}

			// Access outer after inner scope
			int AccessOuterAfterInner()
			{
				int X = 10;
				{
					int X = 20;
					// Inner X is 20
				}
				return X;  // Outer X is still 10
			}

			// Shadowing in conditional
			int ShadowingInConditional(bool Flag)
			{
				int Value = 100;
				if (Flag)
				{
					int Value = 200;  // Shadows outer
					return Value;
				}
				return Value;  // Outer value
			}
			)AS"));
			ON_SCOPE_EXIT
			{
				if (Module != nullptr)
				{
					Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
				}
			};

			ExpectGlobalReturn<int>(Engine, Module, TEXT("int LocalShadowsGlobal()"), 50, TEXT("local shadows global"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int InnerShadowsOuter()"), 20, TEXT("inner shadows outer"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int MultipleShadowLevels()"), 3, TEXT("multiple shadow levels"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int ShadowingInLoop()"), 100, TEXT("shadowing in loop"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int ShadowingSameType()"), 30, TEXT("shadowing same type"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int AccessOuterAfterInner()"), 10, TEXT("access outer after inner"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int ShadowingInConditional(bool)"), 200, TEXT("shadowing in conditional"), true);

			UClass* ScriptClass = FindGeneratedClass(&Engine, TEXT("AScopeShadowMemberActor"));
			ASSERT_THAT(IsNotNull(ScriptClass, TEXT("scope shadowing actor class should be generated")));
			if (ScriptClass == nullptr)
			{
				return;
			}
			FAngelscriptTestWorld World(*TestRunner, Engine);
			ASSERT_THAT(IsTrue(World.IsValid()));
			if (!World.IsValid())
			{
				return;
			}

			AActor* ScriptObject = World.SpawnActorOfClass<AActor>(ScriptClass);
			ASSERT_THAT(IsNotNull(ScriptObject, TEXT("scope shadowing actor should spawn")));
			if (ScriptObject == nullptr)
			{
				return;
			}

			FFunctionInvoker Invoker(*TestRunner, ScriptObject, FName(TEXT("ParameterShadowsMember")));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("parameter/member shadowing UFUNCTION should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddParam<int32>(5);
			const int32 ShadowResult = Invoker.CallAndReturn<int32>(INDEX_NONE);
			ASSERT_THAT(AreEqual(105, ShadowResult, TEXT("parameter should shadow member and this.Value should access the member")));
		}

		TEST_METHOD(ScopeLifecycle)
		{
			FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
			FAngelscriptEngineScope Scope(Engine);

			asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovNamespace_ScopeLifecycle", ASTEST_AS(R"AS(
			int ScopeEntryConstructsLocals()
			{
				FVector Local = FVector(1, 2, 3);
				TArray<int> Values;
				Values.Add(int(Local.X));
				Values.Add(int(Local.Y));
				Values.Add(int(Local.Z));
				return Values[0] * 100 + Values[1] * 10 + Values[2];
			}

			int BlockExitKeepsOuterState()
			{
				int Result = 0;
				{
					TArray<int> Temp;
					Temp.Add(4);
					Temp.Add(5);
					Result = Temp[0] * 10 + Temp[1];
				}

				TArray<int> Temp;
				Temp.Add(6);
				return Result * 10 + Temp[0];
			}

			int LocalContainerDestroyedAfterFunction()
			{
				int Total = 0;
				for (int Iteration = 0; Iteration < 3; ++Iteration)
				{
					TArray<int> Values;
					Values.Add(Iteration);
					Values.Add(Iteration + 1);
					Total += Values.Num();
				}
				return Total;
			}

			UCLASS()
			class AScopeLifecycleActor : AActor
			{
				UPROPERTY()
				int BeginPlayCount = 0;

				UPROPERTY()
				int EndPlayCount = 0;

				UPROPERTY()
				int DestroyedCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					BeginPlayCount += 1;
				}

				UFUNCTION(BlueprintOverride)
				void EndPlay(EEndPlayReason Reason)
				{
					EndPlayCount += 1;
				}

				UFUNCTION(BlueprintOverride)
				void Destroyed()
				{
					DestroyedCount += 1;
				}
			}
			)AS"));
			ON_SCOPE_EXIT
			{
				if (Module != nullptr)
				{
					Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
				}
			};

			ExpectGlobalReturn<int>(Engine, Module, TEXT("int ScopeEntryConstructsLocals()"), 123, TEXT("scope entry should initialize local values"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int BlockExitKeepsOuterState()"), 456, TEXT("block exit should clean inner locals and preserve outer state"));
			ExpectGlobalReturn<int>(Engine, Module, TEXT("int LocalContainerDestroyedAfterFunction()"), 6, TEXT("function-local containers should not leak between loop iterations"));

			UClass* ScriptClass = FindGeneratedClass(&Engine, TEXT("AScopeLifecycleActor"));
			ASSERT_THAT(IsNotNull(ScriptClass, TEXT("scope lifecycle actor class should be generated")));
			if (ScriptClass == nullptr)
			{
				return;
			}

			FAngelscriptTestWorld World(*TestRunner, Engine);
			ASSERT_THAT(IsTrue(World.IsValid()));
			if (!World.IsValid())
			{
				return;
			}

			AActor* Actor = World.SpawnActorOfClass<AActor>(ScriptClass);
			ASSERT_THAT(IsNotNull(Actor, TEXT("scope lifecycle actor should spawn")));
			if (Actor == nullptr)
			{
				return;
			}

			World.BeginPlay(*Actor);
			World.DestroyAndDrain(*Actor);

			int32 BeginPlayCount = 0;
			int32 EndPlayCount = 0;
			int32 DestroyedCount = 0;
			ASSERT_THAT(IsTrue(ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("BeginPlayCount"), BeginPlayCount)));
			ASSERT_THAT(IsTrue(ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("EndPlayCount"), EndPlayCount)));
			ASSERT_THAT(IsTrue(ReadPropertyValue<FIntProperty>(*TestRunner, Actor, TEXT("DestroyedCount"), DestroyedCount)));

			ASSERT_THAT(AreEqual(1, BeginPlayCount, TEXT("BeginPlay should run once before object cleanup")));
			ASSERT_THAT(AreEqual(1, EndPlayCount, TEXT("EndPlay should run once during object cleanup")));
			ASSERT_THAT(AreEqual(1, DestroyedCount, TEXT("Destroyed should run once during object cleanup")));
		}

		// -------------------------------------------------------------------------
		// Namespace with classes and enums
		// -------------------------------------------------------------------------
		TEST_METHOD(NamespaceWithTypes)
		{
			FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
			FAngelscriptEngineScope Scope(Engine);

			asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCovNamespace_WithTypes", ASTEST_AS(R"AS(
			namespace Types
			{
				class MyClass
				{
					int Value;

					int GetValue()
					{
						return Value;
					}
				}

				enum MyEnum
				{
					First,
					Second,
					Third
				}

				int UseEnum(MyEnum E)
				{
					switch (E)
					{
						case MyEnum::First:
							return 1;
						case MyEnum::Second:
							return 2;
						case MyEnum::Third:
							return 3;
					}
				}
			}

			int UseNamespacedClass()
			{
				Types::MyClass Obj;
				Obj.Value = 42;
				return Obj.GetValue();
			}

			int UseNamespacedEnum()
			{
				return Types::UseEnum(Types::MyEnum::Second);
			}
			)AS"));
			ON_SCOPE_EXIT
			{
				if (Module != nullptr)
				{
					Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
				}
			};

			ExpectGlobalReturn<int>(Engine, Module, TEXT("int UseNamespacedEnum()"), 2, TEXT("use namespaced enum"));

			TestRunner->AddExpectedError(TEXT("Null pointer access"), EAutomationExpectedErrorFlags::Contains, 0);
			TestRunner->AddExpectedError(TEXT("ASCovNamespace_WithTypes"), EAutomationExpectedErrorFlags::Contains, 0);
			TestRunner->AddExpectedError(TEXT("UseNamespacedClass"), EAutomationExpectedErrorFlags::Contains, 0);

			ASSERT_THAT(IsTrue(ExecuteFunctionExpectingScriptException(
				*TestRunner,
				Engine,
				*Module,
				TEXT("int UseNamespacedClass()"),
				TEXT("namespaced script-class local instance remains a runtime execution boundary"),
				TEXT("Null pointer access"))));
		}
	};
}

#endif // WITH_ANGELSCRIPT_UNITTESTS
