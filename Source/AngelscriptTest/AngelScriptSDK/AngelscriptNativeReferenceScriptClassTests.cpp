#include "AngelscriptNativeTestSupport.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool TestHasMessageContaining(FAutomationTestBase& Test, const AngelscriptNativeTestSupport::FNativeMessageCollector& Messages, const TCHAR* ExpectedText, const TCHAR* Context)
	{
		FNoDiscardAsserter Assert(Test);

		for (const AngelscriptNativeTestSupport::FNativeMessageEntry& Entry : Messages.Entries)
		{
			if (Entry.Message.Contains(ExpectedText))
			{
				return Assert.IsTrue(true, Context);
			}
		}

		Test.AddInfo(AngelscriptNativeTestSupport::CollectMessages(Messages));
		return Assert.IsTrue(false, Context);
	}

	bool TestTypeHasProperty(asITypeInfo* TypeInfo, const char* ExpectedName)
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

	bool TestTypeHasBehaviour(FAutomationTestBase& Test, asITypeInfo* TypeInfo, asEBehaviours ExpectedBehaviour, const TCHAR* Context)
	{
		FNoDiscardAsserter Assert(Test);

		if (TypeInfo == nullptr)
		{
			return Assert.IsNotNull(TypeInfo, Context);
		}

		for (asUINT BehaviourIndex = 0; BehaviourIndex < TypeInfo->GetBehaviourCount(); ++BehaviourIndex)
		{
			asEBehaviours Behaviour = asBEHAVE_CONSTRUCT;
			asIScriptFunction* const Function = TypeInfo->GetBehaviourByIndex(BehaviourIndex, &Behaviour);
			if (Function != nullptr && Behaviour == ExpectedBehaviour)
			{
				return Assert.IsTrue(true, Context);
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
		return Assert.IsTrue(false, Context);
	}

	bool TestTypeHasConstructorWithParamCount(FAutomationTestBase& Test, asITypeInfo* TypeInfo, asUINT ExpectedParamCount, const TCHAR* Context)
	{
		FNoDiscardAsserter Assert(Test);

		if (TypeInfo == nullptr)
		{
			return Assert.IsNotNull(TypeInfo, Context);
		}

		for (asUINT BehaviourIndex = 0; BehaviourIndex < TypeInfo->GetBehaviourCount(); ++BehaviourIndex)
		{
			asEBehaviours Behaviour = asBEHAVE_CONSTRUCT;
			asIScriptFunction* const Function = TypeInfo->GetBehaviourByIndex(BehaviourIndex, &Behaviour);
			if (Function != nullptr && Behaviour == asBEHAVE_CONSTRUCT && Function->GetParamCount() == ExpectedParamCount)
			{
				return Assert.IsTrue(true, Context);
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
		return Assert.IsTrue(false, Context);
	}

	bool ExecuteIntFunctionAndCapture(FAutomationTestBase& Test, asIScriptEngine* ScriptEngine, asIScriptModule* Module, const char* Declaration, FScriptExecutionResult& OutResult)
	{
		FNoDiscardAsserter Assert(Test);

		asIScriptFunction* Function = AngelscriptNativeTestSupport::GetNativeFunctionByExactDecl(Module, Declaration);
		if (!Assert.IsNotNull(Function, TEXT("Reference script-class test should resolve the requested function")))
		{
			return false;
		}

		asIScriptContext* Context = ScriptEngine->CreateContext();
		if (!Assert.IsNotNull(Context, TEXT("Reference script-class test should create an execution context")))
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

	bool TestIsolatedScriptClassInstantiationRaisesNullPointer(FAutomationTestBase& Test, const FScriptExecutionResult& Result, const TCHAR* Context)
	{
		FNoDiscardAsserter Assert(Test);
		bool bPassed = true;
		bPassed &= Assert.AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), Result.ExecuteResult, *FString::Printf(TEXT("%s should raise the current isolated native-engine script-class exception"), Context));
		bPassed &= Assert.AreEqual(FString(TEXT("Null pointer access")), Result.ExceptionString, *FString::Printf(TEXT("%s should report the current fork null-pointer exception text"), Context));
		bPassed &= Assert.IsTrue(Result.ExceptionLine > 0, *FString::Printf(TEXT("%s should keep a positive exception line, got %d"), Context, Result.ExceptionLine));
		bPassed &= Assert.AreEqual(FString(TEXT("int Entry()")), Result.ExceptionFunctionDeclaration, *FString::Printf(TEXT("%s should attribute the exception to Entry()"), Context));
		return bPassed;
	}

	asIScriptModule* BuildScriptClassModule(FAutomationTestBase& Test, asIScriptEngine* ScriptEngine, const char* ModuleName, const char* Source, AngelscriptNativeTestSupport::FNativeMessageCollector& Messages)
	{
		asIScriptModule* Module = AngelscriptNativeTestSupport::BuildNativeModule(ScriptEngine, ModuleName, Source);
		if (Module == nullptr)
		{
			Test.AddInfo(AngelscriptNativeTestSupport::CollectMessages(Messages));
		}

		return Module;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptNativeReferenceScriptClassTests,
	"Angelscript.TestModule.AngelScriptSDK.Reference.ScriptClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ConstructorMetadataAndIsolatedExecutionException)
	{
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = AngelscriptNativeTestSupport::CreateNativeEngine(&Messages);
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Reference script-class constructor test should create a native engine")));

		ON_SCOPE_EXIT
		{
			AngelscriptNativeTestSupport::DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildScriptClassModule(*TestRunner, ScriptEngine, "ReferenceScriptClassConstructor", R"(
class Counter
{
	int Value = 10;

	Counter()
	{
		Value += 32;
	}

	int Get()
	{
		return Value;
	}
}

int Entry()
{
	Counter C;
	return C.Get();
}
)",
			Messages);
		ASSERT_THAT(IsNotNull(Module, TEXT("Reference script-class constructor test should build module")));

		asITypeInfo* CounterType = Module->GetTypeInfoByName("Counter");
		ASSERT_THAT(IsNotNull(CounterType, TEXT("Reference script-class constructor test should expose Counter type metadata")));
		ASSERT_THAT(IsTrue(TestTypeHasProperty(CounterType, "Value"), TEXT("Reference script-class constructor test should expose Value property metadata")));
		ASSERT_THAT(IsTrue(CounterType->GetBehaviourCount() > 0, TEXT("Reference script-class constructor test should expose at least one constructor behaviour")));
		if (!TestTypeHasBehaviour(*TestRunner, CounterType, asBEHAVE_CONSTRUCT, TEXT("Reference script-class constructor test should expose constructor behaviour metadata")))
		{
			return;
		}
		ASSERT_THAT(IsNotNull(CounterType->GetMethodByDecl("int Get()"), TEXT("Reference script-class constructor test should expose Get method metadata")));
		ASSERT_THAT(IsNotNull(AngelscriptNativeTestSupport::GetNativeFunctionByDecl(Module, "int Entry()"), TEXT("Reference script-class constructor test should expose int Entry() without executing script-class instances")));

		FScriptExecutionResult EntryResult;
		if (!ExecuteIntFunctionAndCapture(*TestRunner, ScriptEngine, Module, "int Entry()", EntryResult))
		{
			return;
		}
		ASSERT_THAT(IsTrue(TestIsolatedScriptClassInstantiationRaisesNullPointer(
			*TestRunner,
			EntryResult,
			TEXT("Reference script-class constructor test"))));
	}

	TEST_METHOD(ConstructorArgumentsAreRejectedForValueStyleConstruction)
	{
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = AngelscriptNativeTestSupport::CreateNativeEngine(&Messages);
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Reference script-class argument constructor test should create a native engine")));

		ON_SCOPE_EXIT
		{
			AngelscriptNativeTestSupport::DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = nullptr;
		const int CompileResult = AngelscriptNativeTestSupport::CompileNativeModule(ScriptEngine, "ReferenceScriptClassConstructorArgs", R"(
class Pair
{
	int A;
	int B;

	Pair(int InA, int InB)
	{
		A = InA;
		B = InB;
	}

	int Sum()
	{
		return A + B;
	}
}

int Entry()
{
	Pair P(20, 22);
	return P.Sum();
}
)",
			Module);
		ASSERT_THAT(IsTrue(CompileResult < 0, TEXT("Reference script-class argument constructor should document current value-style construction rejection")));
		if (!TestHasMessageContaining(*TestRunner, Messages, TEXT("Only objects have constructors"), TEXT("Reference script-class argument constructor should report current fork diagnostic")))
		{
			return;
		}
	}

	TEST_METHOD(InheritanceMetadataAndIsolatedExecutionException)
	{
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = AngelscriptNativeTestSupport::CreateNativeEngine(&Messages);
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Reference script-class inheritance test should create a native engine")));

		ON_SCOPE_EXIT
		{
			AngelscriptNativeTestSupport::DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildScriptClassModule(*TestRunner, ScriptEngine, "ReferenceScriptClassInheritance", R"(
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
)",
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

	TEST_METHOD(PrivateConstructorBlocksDerivedSuperCall)
	{
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = AngelscriptNativeTestSupport::CreateNativeEngine(&Messages);
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Reference script-class private constructor test should create a native engine")));

		ON_SCOPE_EXIT
		{
			AngelscriptNativeTestSupport::DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = nullptr;
		const int CompileResult = AngelscriptNativeTestSupport::CompileNativeModule(ScriptEngine, "ReferenceScriptClassPrivateConstructors", R"(
class A
{
	private A()
	{
	}

	A(int Value)
	{
	}

	protected A(float Value)
	{
	}

	A create()
	{
		return A();
	}
}

class B : A
{
	B()
	{
		super();
	}
}
)",
			Module);

		ASSERT_THAT(IsTrue(CompileResult < 0, TEXT("Reference private constructor script should fail to compile when a derived class calls super()")));
		if (!TestHasMessageContaining(*TestRunner, Messages, TEXT("Illegal call to private method"), TEXT("Reference private constructor diagnostic should be preserved")))
		{
			return;
		}
	}

	TEST_METHOD(ProtectedConstructorAllowsDerivedSuperCall)
	{
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = AngelscriptNativeTestSupport::CreateNativeEngine(&Messages);
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Reference script-class protected constructor test should create a native engine")));

		ON_SCOPE_EXIT
		{
			AngelscriptNativeTestSupport::DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = nullptr;
		const int CompileResult = AngelscriptNativeTestSupport::CompileNativeModule(ScriptEngine, "ReferenceScriptClassProtectedConstructors", R"(
class A
{
	protected A(float Value)
	{
	}
}

class B : A
{
	B()
	{
		super(1.4f);
	}
}
)",
			Module);

		ASSERT_THAT(IsTrue(CompileResult >= 0, TEXT("Reference protected constructor script should compile when the derived class calls super(1.4f)")));
		ASSERT_THAT(IsNotNull(Module, TEXT("Reference protected constructor script should expose the compiled module")));

		asITypeInfo* AType = Module->GetTypeInfoByName("A");
		asITypeInfo* BType = Module->GetTypeInfoByName("B");
		ASSERT_THAT(IsNotNull(AType, TEXT("Reference protected constructor script should expose A type metadata")));
		ASSERT_THAT(IsNotNull(BType, TEXT("Reference protected constructor script should expose B type metadata")));

		ASSERT_THAT(IsTrue(BType->DerivesFrom(AType), TEXT("Reference protected constructor script should preserve the derived-to-base relationship")));
		if (!TestTypeHasBehaviour(*TestRunner, AType, asBEHAVE_CONSTRUCT, TEXT("Reference protected constructor script should expose constructor behaviour metadata on A")))
		{
			return;
		}
		if (!TestTypeHasConstructorWithParamCount(*TestRunner, BType, 0, TEXT("Reference protected constructor script should expose default constructor metadata on B")))
		{
			return;
		}
	}

	TEST_METHOD(BaseWithoutDefaultConstructorGetsAutoGeneratedDefaultConstructor)
	{
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = AngelscriptNativeTestSupport::CreateNativeEngine(&Messages);
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Reference script-class base-constructor test should create a native engine")));

		ON_SCOPE_EXIT
		{
			AngelscriptNativeTestSupport::DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = nullptr;
		const int CompileResult = AngelscriptNativeTestSupport::CompileNativeModule(ScriptEngine, "ReferenceScriptClassBaseWithoutDefault", R"(
class Base
{
	Base(int Value)
	{
	}
}

class BadDerived : Base
{
	BadDerived()
	{
	}
}

class GoodDerived : Base
{
	GoodDerived()
	{
		super(42);
	}
}
)",
			Module);

		ASSERT_THAT(IsTrue(CompileResult >= 0, TEXT("Reference base-constructor script should compile because the fork auto-creates a default constructor")));
		ASSERT_THAT(IsNotNull(Module, TEXT("Reference base-constructor script should expose the compiled module")));

		asITypeInfo* BaseType = Module->GetTypeInfoByName("Base");
		asITypeInfo* BadDerivedType = Module->GetTypeInfoByName("BadDerived");
		asITypeInfo* GoodDerivedType = Module->GetTypeInfoByName("GoodDerived");
		ASSERT_THAT(IsNotNull(BaseType, TEXT("Reference base-constructor script should expose Base type metadata")));
		ASSERT_THAT(IsNotNull(BadDerivedType, TEXT("Reference base-constructor script should expose BadDerived type metadata")));
		ASSERT_THAT(IsNotNull(GoodDerivedType, TEXT("Reference base-constructor script should expose GoodDerived type metadata")));

		ASSERT_THAT(IsTrue(BadDerivedType->DerivesFrom(BaseType), TEXT("Reference base-constructor script should preserve the derived-to-base relationship for BadDerived")));
		ASSERT_THAT(IsTrue(GoodDerivedType->DerivesFrom(BaseType), TEXT("Reference base-constructor script should preserve the derived-to-base relationship for GoodDerived")));
		if (!TestTypeHasConstructorWithParamCount(*TestRunner, BaseType, 0, TEXT("Reference base-constructor script should expose default constructor metadata on Base")))
		{
			return;
		}
		if (!TestTypeHasConstructorWithParamCount(*TestRunner, BadDerivedType, 0, TEXT("Reference base-constructor script should expose default constructor metadata on BadDerived")))
		{
			return;
		}
		if (!TestTypeHasConstructorWithParamCount(*TestRunner, GoodDerivedType, 0, TEXT("Reference base-constructor script should expose default constructor metadata on GoodDerived")))
		{
			return;
		}
	}

	TEST_METHOD(MemberInitializationExpressionReportsMissingSymbol)
	{
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = AngelscriptNativeTestSupport::CreateNativeEngine(&Messages);
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Reference member-init diagnostic test should create a native engine")));

		ON_SCOPE_EXIT
		{
			AngelscriptNativeTestSupport::DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = nullptr;
		const int CompileResult = AngelscriptNativeTestSupport::CompileNativeModule(ScriptEngine, "ReferenceScriptClassMemberInitError", R"(
enum SomeEnum
{
	en_A
}

int GetVal(SomeEnum Some)
{
	return 0;
}

class B
{
	int SomeVal = GetVal(en_B);
}
)",
			Module);

		ASSERT_THAT(IsTrue(CompileResult < 0, TEXT("Reference member-init diagnostic script should fail to compile")));
		if (!TestHasMessageContaining(*TestRunner, Messages, TEXT("'en_B' is not declared"), TEXT("Reference member-init missing symbol diagnostic should be preserved")))
		{
			return;
		}
	}

	TEST_METHOD(DeletedDefaultConstructorIsRejectedOrDocumented)
	{
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = AngelscriptNativeTestSupport::CreateNativeEngine(&Messages);
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Reference script-class deleted constructor test should create a native engine")));

		ON_SCOPE_EXIT
		{
			AngelscriptNativeTestSupport::DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = nullptr;
		const int CompileResult = AngelscriptNativeTestSupport::CompileNativeModule(ScriptEngine, "ReferenceScriptClassDeletedConstructor", R"(
class NoDefault
{
	NoDefault() delete;
}

int Entry()
{
	NoDefault Value;
	return 1;
}
)",
			Module);

		ASSERT_THAT(IsTrue(CompileResult < 0, TEXT("Reference deleted default constructor should not allow default construction")));
		ASSERT_THAT(IsTrue(Messages.Entries.Num() > 0, TEXT("Reference deleted default constructor should report at least one diagnostic")));
	}
};

#endif
