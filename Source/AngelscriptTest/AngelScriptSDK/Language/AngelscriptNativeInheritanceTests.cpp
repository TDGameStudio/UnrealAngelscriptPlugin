#include "Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FInheritanceTests, "Angelscript.TestModule.AngelScriptSDK.Language.Inheritance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool TestHasMessageContaining(FAutomationTestBase& Test, const AngelscriptNativeTestSupport::FNativeMessageCollector& Messages, const TCHAR* ExpectedText, const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);

		for (const AngelscriptNativeTestSupport::FNativeMessageEntry& Entry : Messages.Entries)
		{
			if (Entry.Message.Contains(ExpectedText))
			{
				return LocalAssert.IsTrue(true, Context);
			}
		}

		Test.AddInfo(AngelscriptNativeTestSupport::CollectMessages(Messages));
		return LocalAssert.IsTrue(false, Context);
	}

	static bool TestTypeHasProperty(asITypeInfo* TypeInfo, const char* ExpectedName)
	{
		if (TypeInfo == nullptr || ExpectedName == nullptr)
		{
			return false;
		}

		for (asUINT PropertyIndex = 0; PropertyIndex < TypeInfo->GetPropertyCount(); ++PropertyIndex)
		{
			const char* PropertyName = nullptr;
			if (TypeInfo->GetProperty(PropertyIndex, &PropertyName) >= 0 && PropertyName != nullptr && FCStringAnsi::Strcmp(PropertyName, ExpectedName) == 0)
			{
				return true;
			}
		}

		return false;
	}

	struct FScriptExecutionResult
	{
		int ExecuteResult = asERROR;
		int32 ReturnValue = 0;
		int32 ExceptionLine = 0;
		FString ExceptionString;
		FString ExceptionFunctionDeclaration;
	};

	static bool TestTypeHasBehaviour(FAutomationTestBase& Test, asITypeInfo* TypeInfo, asEBehaviours ExpectedBehaviour, const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);

		if (TypeInfo == nullptr)
		{
			return LocalAssert.IsNotNull(TypeInfo, Context);
		}

		for (asUINT BehaviourIndex = 0; BehaviourIndex < TypeInfo->GetBehaviourCount(); ++BehaviourIndex)
		{
			asEBehaviours Behaviour = asBEHAVE_CONSTRUCT;
			asIScriptFunction* const Function = TypeInfo->GetBehaviourByIndex(BehaviourIndex, &Behaviour);
			if (Function != nullptr && Behaviour == ExpectedBehaviour)
			{
				return LocalAssert.IsTrue(true, Context);
			}
		}

		FString Details = FString::Printf(
			TEXT("%s behaviour count=%u"),
			Context,
			TypeInfo->GetBehaviourCount());
		for (asUINT BehaviourIndex = 0; BehaviourIndex < TypeInfo->GetBehaviourCount(); ++BehaviourIndex)
		{
			asEBehaviours Behaviour = asBEHAVE_CONSTRUCT;
			asIScriptFunction* const Function = TypeInfo->GetBehaviourByIndex(BehaviourIndex, &Behaviour);
			Details += FString::Printf(
				TEXT("\n  [%u] type=%d decl=%s"),
				BehaviourIndex,
				static_cast<int32>(Behaviour),
				Function != nullptr ? UTF8_TO_TCHAR(Function->GetDeclaration()) : TEXT("<null>"));
		}
		Test.AddInfo(Details);
		return LocalAssert.IsTrue(false, Context);
	}

	static bool TestTypeHasConstructorWithParamCount(FAutomationTestBase& Test, asITypeInfo* TypeInfo, asUINT ExpectedParamCount, const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);

		if (TypeInfo == nullptr)
		{
			return LocalAssert.IsNotNull(TypeInfo, Context);
		}

		for (asUINT BehaviourIndex = 0; BehaviourIndex < TypeInfo->GetBehaviourCount(); ++BehaviourIndex)
		{
			asEBehaviours Behaviour = asBEHAVE_CONSTRUCT;
			asIScriptFunction* const Function = TypeInfo->GetBehaviourByIndex(BehaviourIndex, &Behaviour);
			if (Function != nullptr && Behaviour == asBEHAVE_CONSTRUCT && Function->GetParamCount() == ExpectedParamCount)
			{
				return LocalAssert.IsTrue(true, Context);
			}
		}

		FString Details = FString::Printf(
			TEXT("%s behaviour count=%u"),
			Context,
			TypeInfo->GetBehaviourCount());
		for (asUINT BehaviourIndex = 0; BehaviourIndex < TypeInfo->GetBehaviourCount(); ++BehaviourIndex)
		{
			asEBehaviours Behaviour = asBEHAVE_CONSTRUCT;
			asIScriptFunction* const Function = TypeInfo->GetBehaviourByIndex(BehaviourIndex, &Behaviour);
			Details += FString::Printf(
				TEXT("\n  [%u] type=%d params=%u decl=%s"),
				BehaviourIndex,
				static_cast<int32>(Behaviour),
				Function != nullptr ? Function->GetParamCount() : 0,
				Function != nullptr ? UTF8_TO_TCHAR(Function->GetDeclaration()) : TEXT("<null>"));
		}
		Test.AddInfo(Details);
		return LocalAssert.IsTrue(false, Context);
	}

	static bool ExecuteIntFunctionAndCapture(FAutomationTestBase& Test, asIScriptEngine* ScriptEngine, asIScriptModule* Module, const char* Declaration, FScriptExecutionResult& OutResult)
	{
		FNoDiscardAsserter LocalAssert(Test);

		asIScriptFunction* Function = AngelscriptNativeTestSupport::GetNativeFunctionByExactDecl(Module, Declaration);
		if (!LocalAssert.IsNotNull(Function, TEXT("Reference script-class test should resolve the requested function")))
		{
			return false;
		}

		asIScriptContext* Context = ScriptEngine->CreateContext();
		if (!LocalAssert.IsNotNull(Context, TEXT("Reference script-class test should create an execution context")))
		{
			return false;
		}

		const int ExecuteResult = AngelscriptNativeTestSupport::PrepareAndExecute(Context, Function);
		OutResult.ExecuteResult = ExecuteResult;
		OutResult.ReturnValue = static_cast<int32>(Context->GetReturnDWord());
		OutResult.ExceptionLine = Context->GetExceptionLineNumber();
		OutResult.ExceptionString = Context->GetExceptionString() != nullptr ? UTF8_TO_TCHAR(Context->GetExceptionString()) : TEXT("");
		if (asIScriptFunction* ExceptionFunction = Context->GetExceptionFunction())
		{
			OutResult.ExceptionFunctionDeclaration = UTF8_TO_TCHAR(ExceptionFunction->GetDeclaration());
		}
		Context->Release();
		return true;
	}

	static bool TestIsolatedScriptClassInstantiationRaisesNullPointer(FAutomationTestBase& Test, const FScriptExecutionResult& Result, const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		bool bPassed = true;
		bPassed &= LocalAssert.AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), Result.ExecuteResult, *FString::Printf(TEXT("%s should raise the current isolated native-engine script-class exception"), Context));
		bPassed &= LocalAssert.AreEqual(FString(TEXT("Null pointer access")), Result.ExceptionString, *FString::Printf(TEXT("%s should report the current fork null-pointer exception text"), Context));
		bPassed &= LocalAssert.IsTrue(Result.ExceptionLine > 0, *FString::Printf(TEXT("%s should keep a positive exception line, got %d"), Context, Result.ExceptionLine));
		bPassed &= LocalAssert.AreEqual(FString(TEXT("int Entry()")), Result.ExceptionFunctionDeclaration, *FString::Printf(TEXT("%s should attribute the exception to Entry()"), Context));
		return bPassed;
	}

	static asIScriptModule* BuildScriptClassModule(FAutomationTestBase& Test, asIScriptEngine* ScriptEngine, const char* ModuleName, const char* Source, AngelscriptNativeTestSupport::FNativeMessageCollector& Messages)
	{
		asIScriptModule* Module = AngelscriptNativeTestSupport::BuildNativeModule(ScriptEngine, ModuleName, Source);
		if (Module == nullptr)
		{
			Test.AddInfo(AngelscriptNativeTestSupport::CollectMessages(Messages));
		}

		return Module;
	}
public:
	TEST_METHOD(InheritanceMetadataAndIsolatedExecutionException)
	{
		AS_NATIVE_NON_PRODUCT(
			"AggregateSupport",
			"LANG-INH-DISPATCH, LANG-INH-ACCESS, and LANG-INH-CLASS-RULE own inheritance metadata, method/property visibility, dispatch, and the isolated script-class null-instance boundary; this method retains one focused witness");

		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = AngelscriptNativeTestSupport::CreateNativeEngine(&Messages);
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Reference script-class inheritance test should create a native engine")));

		ON_SCOPE_EXIT
		{
			AngelscriptNativeTestSupport::DestroyNativeEngine(ScriptEngine);
		};

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class Base
			{
				int Value = 21;

				int Twice()
				{
					return Value * 2;
				}
			}

			class Derived : Base
			{
			}

			int Entry()
			{
				Derived D;
				return D.Twice();
			}
			)AS");
		asIScriptModule* Module = BuildScriptClassModule(*TestRunner, ScriptEngine, "ReferenceScriptClassInheritance", ScriptSource.c_str(),
			Messages);
		ASSERT_THAT(IsNotNull(Module, TEXT("Reference script-class inheritance test should build module")));

		asITypeInfo* BaseType = Module->GetTypeInfoByName("Base");
		asITypeInfo* DerivedType = Module->GetTypeInfoByName("Derived");
		ASSERT_THAT(IsNotNull(BaseType, TEXT("Reference script-class inheritance test should expose Base type metadata")));
		ASSERT_THAT(IsNotNull(DerivedType, TEXT("Reference script-class inheritance test should expose Derived type metadata")));
		ASSERT_THAT(IsTrue(DerivedType->DerivesFrom(BaseType), TEXT("Reference script-class inheritance test should expose derived-to-base relationship")));
		ASSERT_THAT(IsNotNull(BaseType->GetMethodByDecl("int Twice()"), TEXT("Reference script-class inheritance test should expose base method metadata")));
		ASSERT_THAT(IsNotNull(AngelscriptNativeTestSupport::GetNativeFunctionByDecl(Module, "int Entry()"), TEXT("Reference script-class inheritance test should expose int Entry() without executing inherited script-class instances")));

		FScriptExecutionResult EntryResult;
		if (!ExecuteIntFunctionAndCapture(*TestRunner, ScriptEngine, Module, "int Entry()", EntryResult))
		{
			return;
		}
		ASSERT_THAT(IsTrue(TestIsolatedScriptClassInstantiationRaisesNullPointer(
			*TestRunner,
			EntryResult,
			TEXT("Reference script-class inheritance test"))));
	}
};

#endif
