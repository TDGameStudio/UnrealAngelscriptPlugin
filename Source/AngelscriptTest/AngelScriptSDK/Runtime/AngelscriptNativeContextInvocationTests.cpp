#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeCaseTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

// Raw SDK context invocation coverage.

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FContextInvocationTests,
	"Angelscript.TestModule.AngelScriptSDK.Runtime.ContextInvocation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static int32 LastObservedValue = -1;

	static void RecordObservedValue(const int32 Value)
	{
		LastObservedValue = Value;
	}

	static bool BuildModuleForExecution(
		FAutomationTestBase& Test,
		FNoDiscardAsserter& Assert,
		AngelscriptNativeTestSupport::FNativeTestEngine& NativeEngine,
		const char* ModuleName,
		const char* Source,
		asIScriptModule*& OutModule)
	{
		using namespace AngelscriptNativeTestSupport;

		asIScriptEngine* const ScriptEngine = NativeEngine.Get();
		if (!Assert.IsNotNull(ScriptEngine,
			TEXT("Native execution tests should create a standalone AngelScript engine")))
		{
			return false;
		}

		OutModule = BuildNativeModule(ScriptEngine, ModuleName, Source);
		if (!Assert.IsNotNull(OutModule,
			TEXT("Native execution tests should compile the requested module from memory")))
		{
			Test.AddInfo(NativeEngine.GetMessagesText());
			return false;
		}

		return true;
	}

public:
	TEST_METHOD(InvocationByArityAndReturnShape)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("RT-CTX-INVOCATION-ARITY-RETURN",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		struct FInvocationCase
		{
			const TCHAR* ArityId;
			int32 Arity;
			const TCHAR* ReturnShapeId;
			bool bReturnsValue;
		};

		const FInvocationCase Cases[] =
		{
			{ TEXT("zero"), 0, TEXT("void"), false },
			{ TEXT("zero"), 0, TEXT("int"), true },
			{ TEXT("one"), 1, TEXT("void"), false },
			{ TEXT("one"), 1, TEXT("int"), true },
			{ TEXT("two"), 2, TEXT("void"), false },
			{ TEXT("two"), 2, TEXT("int"), true },
			{ TEXT("three"), 3, TEXT("void"), false },
			{ TEXT("three"), 3, TEXT("int"), true },
		};

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Context invocation product should create a case-owned raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		const ASAutoCaller::FunctionCaller RecordCaller =
			ASAutoCaller::MakeFunctionCaller(RecordObservedValue);
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterGlobalFunction(
				"void RecordObserved(int Value)",
				asFUNCTION(RecordObservedValue),
				asCALL_CDECL,
				*(asFunctionCaller*)&RecordCaller) >= 0,
			TEXT("Context invocation product should register its native observation callback")));

		for (const FInvocationCase& Case : Cases)
		{
			const FString CaseId = MakeNativeCaseId(
				"RT-CTX-INVOCATION-ARITY-RETURN",
				{ Case.ArityId, Case.ReturnShapeId });
			const FString ModuleName = FString::Printf(
				TEXT("NativeContextInvocation_%s_%s"),
				Case.ArityId,
				Case.ReturnShapeId);

			TArray<FString> ParameterDeclarations;
			TArray<FString> ArgumentNames;
			for (int32 ArgumentIndex = 0; ArgumentIndex < Case.Arity; ++ArgumentIndex)
			{
				ParameterDeclarations.Add(FString::Printf(
					TEXT("int A%d"),
					ArgumentIndex));
				ArgumentNames.Add(FString::Printf(TEXT("A%d"), ArgumentIndex));
			}

			const FString ParameterList = FString::Join(ParameterDeclarations, TEXT(", "));
			const FString SumExpression = Case.Arity == 0
				? TEXT("42")
				: FString::Join(ArgumentNames, TEXT(" + "));
			FString Source;
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%s Invoke(%s)"),
				Case.bReturnsValue ? TEXT("int") : TEXT("void"),
				*ParameterList));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\tRecordObserved(%s);"),
				*SumExpression));
			if (Case.bReturnsValue)
			{
				AppendGeneratedAsLine(Source, FString::Printf(
					TEXT("\treturn %s;"),
					*SumExpression));
			}
			AppendGeneratedAsLine(Source, TEXT("}"));

			PrintGeneratedAsSource(*TestRunner, CaseId, ModuleName, Source);
			FTCHARToUTF8 SourceUtf8(*Source);
			FScopedNativeModule Module(
				*TestRunner,
				Engine,
				TCHAR_TO_UTF8(*ModuleName),
				SourceUtf8.Get());
			ASSERT_THAT(IsTrue(
				Module.IsValid(),
				*FString::Printf(TEXT("%s should compile"), *CaseId)));
			if (!Module.IsValid())
			{
				continue;
			}

			const FString Declaration = FString::Printf(
				TEXT("%s Invoke(%s)"),
				Case.bReturnsValue ? TEXT("int") : TEXT("void"),
				*FString::JoinBy(
					ParameterDeclarations,
					TEXT(", "),
					[](const FString&)
					{
						return FString(TEXT("const int"));
					}));
			FTCHARToUTF8 DeclarationUtf8(*Declaration);
			asIScriptFunction* const Function = GetNativeFunctionByExactDecl(
				Module,
				DeclarationUtf8.Get());
			ASSERT_THAT(IsNotNull(
				Function,
				*FString::Printf(TEXT("%s should resolve its exact normalized declaration"), *CaseId)));
			if (Function == nullptr)
			{
				continue;
			}
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(Case.Arity),
				Function->GetParamCount(),
				*FString::Printf(TEXT("%s should publish the selected parameter count"), *CaseId)));

			asIScriptContext* const Context = ScriptEngine->CreateContext();
			ASSERT_THAT(IsNotNull(
				Context,
				*FString::Printf(TEXT("%s should create a context"), *CaseId)));
			if (Context == nullptr)
			{
				continue;
			}
			ON_SCOPE_EXIT
			{
				Context->Release();
			};

			ASSERT_THAT(AreEqual(
				static_cast<int32>(asSUCCESS),
				Context->Prepare(Function),
				*FString::Printf(TEXT("%s should prepare"), *CaseId)));
			int32 ExpectedValue = Case.Arity == 0 ? 42 : 0;
			for (int32 ArgumentIndex = 0; ArgumentIndex < Case.Arity; ++ArgumentIndex)
			{
				const int32 ArgumentValue = ArgumentIndex == 0
					? 10
					: (ArgumentIndex == 1 ? 20 : 12);
				ExpectedValue += ArgumentValue;
				ASSERT_THAT(AreEqual(
					static_cast<int32>(asSUCCESS),
					Context->SetArgDWord(
						static_cast<asUINT>(ArgumentIndex),
						static_cast<asDWORD>(ArgumentValue)),
					*FString::Printf(
						TEXT("%s should set argument %d"),
						*CaseId,
						ArgumentIndex)));
			}
			LastObservedValue = -1;
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asEXECUTION_FINISHED),
				Context->Execute(),
				*FString::Printf(TEXT("%s should execute"), *CaseId)));
			ASSERT_THAT(AreEqual(
				ExpectedValue,
				LastObservedValue,
				*FString::Printf(TEXT("%s should consume every supplied argument"), *CaseId)));
			if (Case.bReturnsValue)
			{
				ASSERT_THAT(AreEqual(
					ExpectedValue,
					static_cast<int32>(Context->GetReturnDWord()),
					*FString::Printf(TEXT("%s should preserve its integer return"), *CaseId)));
			}
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asSUCCESS),
				Context->Unprepare(),
				*FString::Printf(TEXT("%s should unprepare cleanly"), *CaseId)));
		}
	}

	TEST_METHOD(ContextInvocationVoidFunction)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT("AggregateSupport",
			"The zero-argument void compatibility path is independently retained under RT-CTX-INVOCATION-ARITY-RETURN.");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* ScriptEngine = Engine.Get();
		asIScriptModule* Module = nullptr;
		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "NativeExecuteVoid");
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			void Test()
			{
			}
			)AS");
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("RT-CTX-INVOCATION-ARITY-RETURN-ZERO-VOID-COMPAT"),
			TEXT("NativeExecuteVoid"),
			UTF8_TO_TCHAR(ScriptSource.c_str()));
		if (!BuildModuleForExecution(*TestRunner, this->Assert, Engine, "NativeExecuteVoid", ScriptSource.c_str(), Module))
		{
			return;
		}

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "void Test()");
		ASSERT_THAT(IsNotNull(Function,
			TEXT("Native void execution test should resolve the entry function")));

		asIScriptContext* Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			TEXT("Native void execution test should create a context")));
		ON_SCOPE_EXIT { Context->Release(); };

		const int ExecuteResult = PrepareAndExecute(Context, Function);
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
			TEXT("Native void execution test should finish successfully")));
	}

	TEST_METHOD(ContextInvocationOneArg)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT("AggregateSupport",
			"The one-argument integer compatibility path is independently retained under RT-CTX-INVOCATION-ARITY-RETURN.");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* ScriptEngine = Engine.Get();
		asIScriptModule* Module = nullptr;
		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "NativeExecuteOneArg");
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Test(int Value)
			{
				return Value * 2;
			}
			)AS");
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("RT-CTX-INVOCATION-ARITY-RETURN-ONE-INT-COMPAT"),
			TEXT("NativeExecuteOneArg"),
			UTF8_TO_TCHAR(ScriptSource.c_str()));
		if (!BuildModuleForExecution(*TestRunner, this->Assert, Engine, "NativeExecuteOneArg", ScriptSource.c_str(), Module))
		{
			return;
		}

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "int Test(const int)");
		ASSERT_THAT(IsNotNull(Function,
			TEXT("Native one-arg execution test should resolve the entry function")));

		asIScriptContext* Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			TEXT("Native one-arg execution test should create a context")));
		ON_SCOPE_EXIT { Context->Release(); };

		const int PrepareResult = Context->Prepare(Function);
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), PrepareResult,
			TEXT("Native one-arg execution test should prepare the function")));

		Context->SetArgDWord(0, 21);
		const int ExecuteResult = Context->Execute();
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
			TEXT("Native one-arg execution test should finish successfully")));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()),
			TEXT("Native one-arg execution test should preserve the provided input")));
	}

	TEST_METHOD(ContextInvocationTwoArgs)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT("AggregateSupport",
			"The two-argument integer compatibility path is independently retained under RT-CTX-INVOCATION-ARITY-RETURN.");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* ScriptEngine = Engine.Get();
		asIScriptModule* Module = nullptr;
		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "NativeExecuteTwoArgs");
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Test(int A, int B)
			{
				return A + B;
			}
			)AS");
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("RT-CTX-INVOCATION-ARITY-RETURN-TWO-INT-COMPAT"),
			TEXT("NativeExecuteTwoArgs"),
			UTF8_TO_TCHAR(ScriptSource.c_str()));
		if (!BuildModuleForExecution(*TestRunner, this->Assert, Engine, "NativeExecuteTwoArgs", ScriptSource.c_str(), Module))
		{
			return;
		}

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "int Test(const int, const int)");
		ASSERT_THAT(IsNotNull(Function,
			TEXT("Native two-arg execution test should resolve the entry function")));

		asIScriptContext* Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			TEXT("Native two-arg execution test should create a context")));
		ON_SCOPE_EXIT { Context->Release(); };

		const int PrepareResult = Context->Prepare(Function);
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), PrepareResult,
			TEXT("Native two-arg execution test should prepare the function")));

		Context->SetArgDWord(0, 20);
		Context->SetArgDWord(1, 22);
		const int ExecuteResult = Context->Execute();
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
			TEXT("Native two-arg execution test should finish successfully")));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()),
			TEXT("Native two-arg execution test should sum both arguments")));
	}

	TEST_METHOD(ContextInvocationThreeArgs)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT("AggregateSupport",
			"The three-argument integer compatibility path is independently retained under RT-CTX-INVOCATION-ARITY-RETURN.");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* ScriptEngine = Engine.Get();
		asIScriptModule* Module = nullptr;
		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "NativeExecuteThreeArgs");
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Test(int A, int B, int C)
			{
				return A + B + C;
			}
			)AS");
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("RT-CTX-INVOCATION-ARITY-RETURN-THREE-INT-COMPAT"),
			TEXT("NativeExecuteThreeArgs"),
			UTF8_TO_TCHAR(ScriptSource.c_str()));
		if (!BuildModuleForExecution(*TestRunner, this->Assert, Engine, "NativeExecuteThreeArgs", ScriptSource.c_str(), Module))
		{
			return;
		}

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "int Test(const int, const int, const int)");
		ASSERT_THAT(IsNotNull(Function,
			TEXT("Native three-arg execution test should resolve the entry function")));

		asIScriptContext* Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			TEXT("Native three-arg execution test should create a context")));
		ON_SCOPE_EXIT { Context->Release(); };

		const int PrepareResult = Context->Prepare(Function);
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), PrepareResult,
			TEXT("Native three-arg execution test should prepare the function")));

		Context->SetArgDWord(0, 10);
		Context->SetArgDWord(1, 20);
		Context->SetArgDWord(2, 12);
		const int ExecuteResult = Context->Execute();
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
			TEXT("Native three-arg execution test should finish successfully")));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()),
			TEXT("Native three-arg execution test should sum all arguments")));
	}

	TEST_METHOD(ContextInvocationReturnValue)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT("AggregateSupport",
			"The zero-argument integer compatibility path is independently retained under RT-CTX-INVOCATION-ARITY-RETURN.");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* ScriptEngine = Engine.Get();
		asIScriptModule* Module = nullptr;
		FScopedNativeModuleName ModuleScope(Engine, "NativeExecuteReturn");
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Test()
			{
				return 42;
			}
			)AS");
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("RT-CTX-INVOCATION-ARITY-RETURN-ZERO-INT-COMPAT"),
			TEXT("NativeExecuteReturn"),
			UTF8_TO_TCHAR(ScriptSource.c_str()));
		if (!BuildModuleForExecution(*TestRunner, this->Assert, Engine, "NativeExecuteReturn", ScriptSource.c_str(), Module))
		{
			return;
		}

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, "int Test()");
		ASSERT_THAT(IsNotNull(Function,
			TEXT("Native return-value execution test should resolve the entry function")));

		asIScriptContext* Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			TEXT("Native return-value execution test should create a context")));
		ON_SCOPE_EXIT { Context->Release(); };

		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Function),
			TEXT("Native return-value execution test should finish successfully")));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()),
			TEXT("Native return-value execution test should return the expected integer")));
	}
};

#endif
