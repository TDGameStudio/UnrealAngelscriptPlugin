#include "AngelscriptNativeTestSupport.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool TestHasMessageContaining(FAutomationTestBase& Test, const AngelscriptNativeTestSupport::FNativeMessageCollector& Messages, const TCHAR* ExpectedText, const TCHAR* Context)
	{
		for (const AngelscriptNativeTestSupport::FNativeMessageEntry& Entry : Messages.Entries)
		{
			if (Entry.Message.Contains(ExpectedText))
			{
				return Test.TestTrue(Context, true);
			}
		}

		Test.AddInfo(AngelscriptNativeTestSupport::CollectMessages(Messages));
		return Test.TestTrue(Context, false);
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
		if (TypeInfo == nullptr)
		{
			return Test.TestNotNull(Context, TypeInfo);
		}

		for (asUINT BehaviourIndex = 0; BehaviourIndex < TypeInfo->GetBehaviourCount(); ++BehaviourIndex)
		{
			asEBehaviours Behaviour = asBEHAVE_CONSTRUCT;
			asIScriptFunction* const Function = TypeInfo->GetBehaviourByIndex(BehaviourIndex, &Behaviour);
			if (Function != nullptr && Behaviour == ExpectedBehaviour)
			{
				return Test.TestTrue(Context, true);
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
		return Test.TestTrue(Context, false);
	}

	bool TestTypeHasConstructorWithParamCount(FAutomationTestBase& Test, asITypeInfo* TypeInfo, asUINT ExpectedParamCount, const TCHAR* Context)
	{
		if (TypeInfo == nullptr)
		{
			return Test.TestNotNull(Context, TypeInfo);
		}

		for (asUINT BehaviourIndex = 0; BehaviourIndex < TypeInfo->GetBehaviourCount(); ++BehaviourIndex)
		{
			asEBehaviours Behaviour = asBEHAVE_CONSTRUCT;
			asIScriptFunction* const Function = TypeInfo->GetBehaviourByIndex(BehaviourIndex, &Behaviour);
			if (Function != nullptr && Behaviour == asBEHAVE_CONSTRUCT && Function->GetParamCount() == ExpectedParamCount)
			{
				return Test.TestTrue(Context, true);
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
		return Test.TestTrue(Context, false);
	}

	bool ExecuteIntFunctionAndCapture(FAutomationTestBase& Test, asIScriptEngine* ScriptEngine, asIScriptModule* Module, const char* Declaration, FScriptExecutionResult& OutResult)
	{
		asIScriptFunction* Function = AngelscriptNativeTestSupport::GetNativeFunctionByExactDecl(Module, Declaration);
		if (!Test.TestNotNull(TEXT("Reference script-class test should resolve the requested function"), Function))
		{
			return false;
		}

		asIScriptContext* Context = ScriptEngine->CreateContext();
		if (!Test.TestNotNull(TEXT("Reference script-class test should create an execution context"), Context))
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
		bool bPassed = true;
		bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s should raise the current isolated native-engine script-class exception"), Context), Result.ExecuteResult, static_cast<int32>(asEXECUTION_EXCEPTION));
		bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s should report the current fork null-pointer exception text"), Context), Result.ExceptionString, FString(TEXT("Null pointer access")));
		bPassed &= Test.TestTrue(*FString::Printf(TEXT("%s should keep a positive exception line, got %d"), Context, Result.ExceptionLine), Result.ExceptionLine > 0);
		bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s should attribute the exception to Entry()"), Context), Result.ExceptionFunctionDeclaration, FString(TEXT("int Entry()")));
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
		if (!TestRunner->TestNotNull(TEXT("Reference script-class constructor test should create a native engine"), ScriptEngine))
		{
			return;
		}

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
		if (!TestRunner->TestNotNull(TEXT("Reference script-class constructor test should build module"), Module))
		{
			return;
		}

		asITypeInfo* CounterType = Module->GetTypeInfoByName("Counter");
		if (!TestRunner->TestNotNull(TEXT("Reference script-class constructor test should expose Counter type metadata"), CounterType))
		{
			return;
		}
		TestRunner->TestTrue(TEXT("Reference script-class constructor test should expose Value property metadata"), TestTypeHasProperty(CounterType, "Value"));
		TestRunner->TestTrue(TEXT("Reference script-class constructor test should expose at least one constructor behaviour"), CounterType->GetBehaviourCount() > 0);
		TestTypeHasBehaviour(*TestRunner, CounterType, asBEHAVE_CONSTRUCT, TEXT("Reference script-class constructor test should expose constructor behaviour metadata"));
		TestRunner->TestNotNull(TEXT("Reference script-class constructor test should expose Get method metadata"), CounterType->GetMethodByDecl("int Get()"));
		TestRunner->TestNotNull(TEXT("Reference script-class constructor test should expose int Entry() without executing script-class instances"),
			AngelscriptNativeTestSupport::GetNativeFunctionByDecl(Module, "int Entry()"));

		FScriptExecutionResult EntryResult;
		if (!ExecuteIntFunctionAndCapture(*TestRunner, ScriptEngine, Module, "int Entry()", EntryResult))
		{
			return;
		}
		TestIsolatedScriptClassInstantiationRaisesNullPointer(
			*TestRunner,
			EntryResult,
			TEXT("Reference script-class constructor test"));
	}

	TEST_METHOD(ConstructorArgumentsAreRejectedForValueStyleConstruction)
	{
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = AngelscriptNativeTestSupport::CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("Reference script-class argument constructor test should create a native engine"), ScriptEngine))
		{
			return;
		}

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
		TestRunner->TestTrue(TEXT("Reference script-class argument constructor should document current value-style construction rejection"), CompileResult < 0);
		TestHasMessageContaining(*TestRunner, Messages, TEXT("Only objects have constructors"), TEXT("Reference script-class argument constructor should report current fork diagnostic"));
	}

	TEST_METHOD(InheritanceMetadataAndIsolatedExecutionException)
	{
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = AngelscriptNativeTestSupport::CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("Reference script-class inheritance test should create a native engine"), ScriptEngine))
		{
			return;
		}

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
		if (!TestRunner->TestNotNull(TEXT("Reference script-class inheritance test should build module"), Module))
		{
			return;
		}

		asITypeInfo* BaseType = Module->GetTypeInfoByName("Base");
		asITypeInfo* DerivedType = Module->GetTypeInfoByName("Derived");
		if (!TestRunner->TestNotNull(TEXT("Reference script-class inheritance test should expose Base type metadata"), BaseType) ||
			!TestRunner->TestNotNull(TEXT("Reference script-class inheritance test should expose Derived type metadata"), DerivedType))
		{
			return;
		}
		TestRunner->TestTrue(TEXT("Reference script-class inheritance test should expose derived-to-base relationship"), DerivedType->DerivesFrom(BaseType));
		TestRunner->TestNotNull(TEXT("Reference script-class inheritance test should expose base method metadata"), BaseType->GetMethodByDecl("int Twice()"));
		TestRunner->TestNotNull(TEXT("Reference script-class inheritance test should expose int Entry() without executing inherited script-class instances"),
			AngelscriptNativeTestSupport::GetNativeFunctionByDecl(Module, "int Entry()"));

		FScriptExecutionResult EntryResult;
		if (!ExecuteIntFunctionAndCapture(*TestRunner, ScriptEngine, Module, "int Entry()", EntryResult))
		{
			return;
		}
		TestIsolatedScriptClassInstantiationRaisesNullPointer(
			*TestRunner,
			EntryResult,
			TEXT("Reference script-class inheritance test"));
	}

	TEST_METHOD(PrivateConstructorBlocksDerivedSuperCall)
	{
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = AngelscriptNativeTestSupport::CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("Reference script-class private constructor test should create a native engine"), ScriptEngine))
		{
			return;
		}

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

		TestRunner->TestTrue(TEXT("Reference private constructor script should fail to compile when a derived class calls super()"), CompileResult < 0);
		TestHasMessageContaining(*TestRunner, Messages, TEXT("Illegal call to private method"), TEXT("Reference private constructor diagnostic should be preserved"));
	}

	TEST_METHOD(ProtectedConstructorAllowsDerivedSuperCall)
	{
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = AngelscriptNativeTestSupport::CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("Reference script-class protected constructor test should create a native engine"), ScriptEngine))
		{
			return;
		}

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

		TestRunner->TestTrue(TEXT("Reference protected constructor script should compile when the derived class calls super(1.4f)"), CompileResult >= 0);
		if (!TestRunner->TestNotNull(TEXT("Reference protected constructor script should expose the compiled module"), Module))
		{
			return;
		}

		asITypeInfo* AType = Module->GetTypeInfoByName("A");
		asITypeInfo* BType = Module->GetTypeInfoByName("B");
		if (!TestRunner->TestNotNull(TEXT("Reference protected constructor script should expose A type metadata"), AType) ||
			!TestRunner->TestNotNull(TEXT("Reference protected constructor script should expose B type metadata"), BType))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("Reference protected constructor script should preserve the derived-to-base relationship"), BType->DerivesFrom(AType));
		TestRunner->TestTrue(TEXT("Reference protected constructor script should expose a constructor behaviour on A"), TestTypeHasBehaviour(*TestRunner, AType, asBEHAVE_CONSTRUCT, TEXT("Reference protected constructor script should expose constructor behaviour metadata on A")));
		TestRunner->TestTrue(TEXT("Reference protected constructor script should expose a default constructor on B"), TestTypeHasConstructorWithParamCount(*TestRunner, BType, 0, TEXT("Reference protected constructor script should expose default constructor metadata on B")));
	}

	TEST_METHOD(BaseWithoutDefaultConstructorGetsAutoGeneratedDefaultConstructor)
	{
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = AngelscriptNativeTestSupport::CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("Reference script-class base-constructor test should create a native engine"), ScriptEngine))
		{
			return;
		}

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

		TestRunner->TestTrue(TEXT("Reference base-constructor script should compile because the fork auto-creates a default constructor"), CompileResult >= 0);
		if (!TestRunner->TestNotNull(TEXT("Reference base-constructor script should expose the compiled module"), Module))
		{
			return;
		}

		asITypeInfo* BaseType = Module->GetTypeInfoByName("Base");
		asITypeInfo* BadDerivedType = Module->GetTypeInfoByName("BadDerived");
		asITypeInfo* GoodDerivedType = Module->GetTypeInfoByName("GoodDerived");
		if (!TestRunner->TestNotNull(TEXT("Reference base-constructor script should expose Base type metadata"), BaseType) ||
			!TestRunner->TestNotNull(TEXT("Reference base-constructor script should expose BadDerived type metadata"), BadDerivedType) ||
			!TestRunner->TestNotNull(TEXT("Reference base-constructor script should expose GoodDerived type metadata"), GoodDerivedType))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("Reference base-constructor script should preserve the derived-to-base relationship for BadDerived"), BadDerivedType->DerivesFrom(BaseType));
		TestRunner->TestTrue(TEXT("Reference base-constructor script should preserve the derived-to-base relationship for GoodDerived"), GoodDerivedType->DerivesFrom(BaseType));
		TestRunner->TestTrue(TEXT("Reference base-constructor script should expose an auto-generated default constructor on Base"), TestTypeHasConstructorWithParamCount(*TestRunner, BaseType, 0, TEXT("Reference base-constructor script should expose default constructor metadata on Base")));
		TestRunner->TestTrue(TEXT("Reference base-constructor script should expose an auto-generated default constructor on BadDerived"), TestTypeHasConstructorWithParamCount(*TestRunner, BadDerivedType, 0, TEXT("Reference base-constructor script should expose default constructor metadata on BadDerived")));
		TestRunner->TestTrue(TEXT("Reference base-constructor script should expose a default constructor on GoodDerived"), TestTypeHasConstructorWithParamCount(*TestRunner, GoodDerivedType, 0, TEXT("Reference base-constructor script should expose default constructor metadata on GoodDerived")));
	}

	TEST_METHOD(MemberInitializationExpressionReportsMissingSymbol)
	{
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = AngelscriptNativeTestSupport::CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("Reference member-init diagnostic test should create a native engine"), ScriptEngine))
		{
			return;
		}

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

		TestRunner->TestTrue(TEXT("Reference member-init diagnostic script should fail to compile"), CompileResult < 0);
		TestHasMessageContaining(*TestRunner, Messages, TEXT("'en_B' is not declared"), TEXT("Reference member-init missing symbol diagnostic should be preserved"));
	}

	TEST_METHOD(DeletedDefaultConstructorIsRejectedOrDocumented)
	{
		AngelscriptNativeTestSupport::FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = AngelscriptNativeTestSupport::CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("Reference script-class deleted constructor test should create a native engine"), ScriptEngine))
		{
			return;
		}

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

		TestRunner->TestTrue(TEXT("Reference deleted default constructor should not allow default construction"), CompileResult < 0);
		TestRunner->TestTrue(TEXT("Reference deleted default constructor should report at least one diagnostic"), Messages.Entries.Num() > 0);
	}
};

#endif
