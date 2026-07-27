#include "../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FFunctionsTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Functions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(FunctionsMixinNamespace)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-FN-MIXIN-DIRECT-DISPATCH",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);
		AS_NATIVE_PRODUCT("LANG-FN-MIXIN-FREE-CALL-REJECTION",
			ENativeEvidence::Compile
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Metadata
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		struct FMixinCase
		{
			const TCHAR* NamespaceName;
			const TCHAR* InvocationName;
			bool bNestedNamespace;
			bool bMemberInvocation;
		};

		const FMixinCase Cases[] =
		{
			{ TEXT("global"), TEXT("member"), false, true },
			{ TEXT("global"), TEXT("free"), false, false },
			{ TEXT("nested"), TEXT("member"), true, true },
			{ TEXT("nested"), TEXT("free"), true, false },
		};

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Function mixin test should create a standalone engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		int32 IsolationControl = 17;
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterGlobalProperty(
				"int MixinIsolationControl",
				&IsolationControl) >= 0,
			TEXT("Function mixin product should register an independent isolation sentinel")));

		for (const FMixinCase& MixinCase : Cases)
		{
			const TCHAR* const Invocation = MixinCase.bMemberInvocation
				? TEXT("Value.AddToCounter(3);")
				: TEXT("AddToCounter(Value, 3);");
			FString Source = MixinCase.bNestedNamespace
				? ASTEST_AS(R"AS(
					namespace Tools
					{
						struct Counter
						{
							int Value = 0;
						}

						mixin void AddToCounter(Counter& Self, int Delta)
						{
							Self.Value += Delta;
						}

						bool Entry()
						{
							Counter Value;
							%s
							return Value.Value == 3;
						}
					}
					)AS")
				: ASTEST_AS(R"AS(
					struct Counter
					{
						int Value = 0;
					}

					mixin void AddToCounter(Counter& Self, int Delta)
					{
						Self.Value += Delta;
					}

					bool Entry()
					{
						Counter Value;
						%s
						return Value.Value == 3;
					}
					)AS");
			ASSERT_THAT(AreEqual(
				1,
				Source.ReplaceInline(TEXT("%s"), Invocation, ESearchCase::CaseSensitive),
				TEXT("Mixin source should replace its single invocation placeholder")));
			const char* const ProductId = MixinCase.bMemberInvocation
				? "LANG-FN-MIXIN-DIRECT-DISPATCH"
				: "LANG-FN-MIXIN-FREE-CALL-REJECTION";
			const FNativeCaseContext Case(MakeNativeCaseId(
				ProductId,
				{ MixinCase.NamespaceName }));
			const FString ModuleName = Case.MakeModuleName(TEXT("FunctionMixin"));
			PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);
			const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
			const FTCHARToUTF8 SourceUtf8(*Source);

			if (!MixinCase.bMemberInvocation)
			{
				FScopedNativeModuleName ModuleScope(Engine, ModuleNameUtf8.Get());
				Engine.ResetMessages();
				asIScriptModule* RejectedModule = nullptr;
				const int BuildResult = CompileNativeModule(
					ScriptEngine,
					ModuleScope.Get(),
					SourceUtf8.Get(),
					RejectedModule);
				ASSERT_THAT(IsTrue(
					BuildResult < 0,
					*Case.Describe(TEXT("mixin free-call source should be rejected by the current fork"))));
				const FString Diagnostics = Engine.GetMessagesText();
				ASSERT_THAT(IsTrue(
					Diagnostics.Contains(TEXT("No matching signatures"))
						&& Diagnostics.Contains(TEXT("AddToCounter")),
					*Case.Describe(TEXT("mixin free-call rejection should retain the exact target and mismatch diagnostic"))));
				ASSERT_THAT(IsTrue(
					RejectedModule == nullptr || RejectedModule->GetFunctionByDecl(
						MixinCase.bNestedNamespace ? "bool Tools::Entry()" : "bool Entry()") == nullptr,
					*Case.Describe(TEXT("mixin free-call rejection should not publish Entry"))));
				if (ScriptEngine->GetModule(ModuleScope.Get(), asGM_ONLY_IF_EXISTS) != nullptr)
				{
					ASSERT_THAT(AreEqual(
						asSUCCESS,
						ScriptEngine->DiscardModule(ModuleScope.Get()),
						*Case.Describe(TEXT("mixin free-call rejection should explicitly discard its failed module"))));
				}
				ASSERT_THAT(IsNull(
					ScriptEngine->GetModule(ModuleScope.Get(), asGM_ONLY_IF_EXISTS),
					*Case.Describe(TEXT("mixin free-call rejected module should be absent before the next cell"))));
				ASSERT_THAT(AreEqual(
					17,
					IsolationControl,
					*Case.Describe(TEXT("mixin free-call rejection should preserve the independent isolation sentinel"))));
				continue;
			}

			FScopedNativeModule Module(
				*TestRunner,
				Engine,
				ModuleNameUtf8.Get(),
				SourceUtf8.Get());
			ASSERT_THAT(IsTrue(
				Module.IsValid(),
				*Case.Describe(TEXT("mixin source should compile"))));
			if (!Module.IsValid())
			{
				continue;
			}

			const char* const EntryDeclaration = MixinCase.bNestedNamespace
				? "bool Tools::Entry()"
				: "bool Entry()";
			{
				AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(
					*TestRunner,
					ScriptEngine,
					Module,
					EntryDeclaration);
				ASSERT_THAT(IsTrue(
					Invoker.IsValid(),
					*Case.Describe(TEXT("mixin cell should resolve its exact entry declaration"))));
				if (Invoker.IsValid())
				{
					ASSERT_THAT(IsTrue(
						Invoker.CallAndReturn<bool>(false),
						*Case.Describe(TEXT("mixin cell should mutate and observe only its receiver"))));
				}
			}
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Module.Discard(),
				*Case.Describe(TEXT("mixin member-dispatch cell should explicitly discard its module"))));
			ASSERT_THAT(IsNull(
				ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
				*Case.Describe(TEXT("mixin member-dispatch module should be absent before the next cell"))));
			ASSERT_THAT(AreEqual(
				17,
				IsolationControl,
				*Case.Describe(TEXT("mixin member dispatch should preserve the independent isolation sentinel"))));
		}
	}

	TEST_METHOD(FunctionsOverloadDefault)
	{
		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"LANG-FN-ARITY-TARGET, LANG-FN-DEFAULTS, and LANG-FN-TYPED-DEFAULTS supersede this fixed overload/default sample across arity, call target, omission pattern, type, and runtime");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK function overload/default test should create a standalone engine")));

		AngelscriptNativeTestSupport::FScopedNativeModule Module(*TestRunner, Engine, "SDKFunctionOverloadDefault", ASTEST_AS_ANSI(R"AS(
			int AddOne(int Value)
			{
				return Value + 1;
			}

			int AddPair(int Left, int Right)
			{
				return Left + Right;
			}

			int AddWithDefault(int Left, int Right = 10)
			{
				return Left + Right;
			}

			int AddWithDefaultImplicit()
			{
				return AddWithDefault(5);
			}

			int AddWithDefaultExplicit()
			{
				return AddWithDefault(3, 2);
			}
		)AS"));
		if (!Module.IsValid())
		{
			return;
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int AddOne(const int)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(static_cast<int32>(2));
			ASSERT_THAT(AreEqual(3, Invoker.CallAndReturn<int32>(INDEX_NONE),
				TEXT("SDK function overload/default test should call AddOne directly")));
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int AddPair(const int, const int)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(static_cast<int32>(2)).AddArg(static_cast<int32>(5));
			ASSERT_THAT(AreEqual(7, Invoker.CallAndReturn<int32>(INDEX_NONE),
				TEXT("SDK function overload/default test should call AddPair directly")));
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int AddWithDefaultImplicit()");
			if (!Invoker.IsValid())
			{
				return;
			}
			ASSERT_THAT(AreEqual(15, Invoker.CallAndReturn<int32>(INDEX_NONE),
				TEXT("SDK function overload/default test should preserve default arguments")));
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int AddWithDefaultExplicit()");
			if (!Invoker.IsValid())
			{
				return;
			}
			ASSERT_THAT(AreEqual(5, Invoker.CallAndReturn<int32>(INDEX_NONE),
				TEXT("SDK function overload/default test should allow explicit default-argument override")));
		}
	}

	TEST_METHOD(TypeBasedOverload)
	{
		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"LANG-FN-OVERLOAD supersedes this three-type overload predecessor across discriminator families, success/ambiguity/rejection outcomes, metadata, runtime, and cleanup");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK function type-overload test should create a standalone engine")));

		AngelscriptNativeTestSupport::FScopedNativeModule Module(*TestRunner, Engine, "SDKFunctionTypeOverload", ASTEST_AS_ANSI(R"AS(
			int Describe(int Value)
			{
				return 1;
			}

			int Describe(double Value)
			{
				return 2;
			}

			int Describe(bool Value)
			{
				return 3;
			}

			int DescribeInt()
			{
				return Describe(10);
			}

			int DescribeDouble()
			{
				return Describe(3.14);
			}

			int DescribeBool()
			{
				return Describe(true);
			}
		)AS"));
		if (!Module.IsValid())
		{
			return;
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int DescribeInt()");
			if (!Invoker.IsValid())
			{
				return;
			}
			ASSERT_THAT(AreEqual(1, Invoker.CallAndReturn<int32>(INDEX_NONE),
				TEXT("SDK function type-overload test should resolve int overloads by argument type")));
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int DescribeDouble()");
			if (!Invoker.IsValid())
			{
				return;
			}
			ASSERT_THAT(AreEqual(2, Invoker.CallAndReturn<int32>(INDEX_NONE),
				TEXT("SDK function type-overload test should resolve double overloads by argument type")));
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int DescribeBool()");
			if (!Invoker.IsValid())
			{
				return;
			}
			ASSERT_THAT(AreEqual(3, Invoker.CallAndReturn<int32>(INDEX_NONE),
				TEXT("SDK function type-overload test should resolve bool overloads by argument type")));
		}
	}

	TEST_METHOD(FunctionsRecursion)
	{
		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"LANG-FN-RECURSION supersedes these factorial/fibonacci samples across recursion depth, scalar type, success/overflow outcome, metadata, runtime, recovery, and cleanup");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK function recursion test should create a standalone engine")));

		AngelscriptNativeTestSupport::FScopedNativeModule Module(*TestRunner, Engine, "SDKFunctionRecursion", ASTEST_AS_ANSI(R"AS(
			int Factorial(int N)
			{
				if (N <= 1) return 1;
				return N * Factorial(N - 1);
			}

			int Fib(int N)
			{
				if (N < 2) return N;
				return Fib(N - 1) + Fib(N - 2);
			}
		)AS"));
		if (!Module.IsValid())
		{
			return;
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int Factorial(const int)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(static_cast<int32>(5));
			ASSERT_THAT(AreEqual(120, Invoker.CallAndReturn<int32>(INDEX_NONE),
				TEXT("SDK function recursion test should compute factorial correctly")));
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int Factorial(const int)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(static_cast<int32>(0));
			ASSERT_THAT(AreEqual(1, Invoker.CallAndReturn<int32>(INDEX_NONE),
				TEXT("SDK function recursion test should handle the factorial base case")));
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int Fib(const int)");
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddArg(static_cast<int32>(10));
			ASSERT_THAT(AreEqual(55, Invoker.CallAndReturn<int32>(INDEX_NONE),
				TEXT("SDK function recursion test should compute fibonacci correctly")));
		}
	}
};

#endif
