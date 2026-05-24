// ============================================================================
// AngelscriptSyntaxDelegateEventTests.cpp
//
// Syntax coverage tests for delegate and event declarations in AngelScript
// — CQTest edition. Tests delegate/event declaration syntax, binding, and
// invocation.
//
// Automation prefix: Angelscript.TestModule.Syntax.DelegateEvent.*
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptBindingsModuleBuilder.h"
#include "Shared/AngelscriptBindingsAssertions.h"
#include "Syntax/AngelscriptSyntaxTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptTestBindings;

// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptSyntaxDelegateEventTest,
	"Angelscript.TestModule.Syntax.DelegateEvent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Declaration — Positive
	// ====================================================================

	TEST_METHOD(Declaration_Positive_DelegateVoid)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("DelDeclBasic"),
			TEXT(R"(
delegate void FOnActionBasic();

class ADelDeclBasicActor : AActor
{
	UPROPERTY()
	FOnActionBasic OnAction;
}
)"),
			TEXT("Basic delegate void declaration"));
	}

	TEST_METHOD(Declaration_Positive_DelegateWithParams)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("DelDeclParams"),
			TEXT(R"(
delegate void FOnDamage(int Amount, AActor Instigator);

class ADelDeclParamActor : AActor
{
	UPROPERTY()
	FOnDamage OnDamage;
}
)"),
			TEXT("Delegate with parameters"));
	}

	TEST_METHOD(Declaration_Positive_DelegateWithReturn)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("DelDeclReturn"),
			TEXT(R"(
delegate bool FValidateAction(int ActionId);

class ADelDeclReturnActor : AActor
{
	UPROPERTY()
	FValidateAction Validator;
}
)"),
			TEXT("Delegate with return type"));
	}

	TEST_METHOD(Declaration_Positive_EventMulticast)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("DelDeclEvent"),
			TEXT(R"(
event void FOnHealthChanged(float NewHealth);

class ADelDeclEventActor : AActor
{
	UPROPERTY()
	FOnHealthChanged OnHealthChanged;
}
)"),
			TEXT("Basic event (multicast delegate) declaration"));
	}

	TEST_METHOD(Declaration_Positive_EventMultiParam)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("DelDeclEventMulti"),
			TEXT(R"(
event void FOnGameEvent(FString EventName, int Data, bool bImportant);

class ADelDeclEventMultiActor : AActor
{
	UPROPERTY()
	FOnGameEvent OnGameEvent;
}
)"),
			TEXT("Event with multiple parameters"));
	}

	// ====================================================================
	// Declaration — Negative
	// ====================================================================

	TEST_METHOD(Declaration_Negative_WithoutFPrefix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// DISABLED(#as-engine-behavior): naming-convention-unenforced — AS 不强制 delegate 类型名 F 前缀
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("DelDeclNoPrefix"),
			TEXT(R"(
delegate void OnAction();
)"),
			TEXT("Delegate without F prefix should fail"));
#endif
	}

	TEST_METHOD(Declaration_Negative_NoName)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("DelDeclNoName"),
			TEXT(R"(
delegate void ();
)"),
			TEXT("Delegate without name should fail"));
	}

	TEST_METHOD(Declaration_Negative_InvalidParamType)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("DelDeclBadParam"),
			TEXT(R"(
delegate void FOnActionBadParam(NonExistentType X);
)"),
			TEXT("Delegate with invalid parameter type should fail"));
	}

	TEST_METHOD(Declaration_Negative_EventWithoutFPrefix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// DISABLED(#as-engine-behavior): naming-convention-unenforced — AS 不强制 event 类型名 F 前缀
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("DelDeclEventNoPrefix"),
			TEXT(R"(
event void OnChanged(int X);
)"),
			TEXT("Event without F prefix should fail"));
#endif
	}

	TEST_METHOD(Declaration_Negative_DuplicateDelegate)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("DelDeclDup"),
			TEXT(R"(
delegate void FOnActionDup();
delegate void FOnActionDup(int X);
)"),
			TEXT("Duplicate delegate name should fail"));
	}

	TEST_METHOD(Declaration_Negative_MissingSemicolon)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("DelDeclNoSemicolon"),
			TEXT(R"(
delegate void FOnActionNoSemi()
)"),
			TEXT("Missing semicolon after delegate should fail"));
	}

	TEST_METHOD(Declaration_Negative_DelegateWithBody)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("DelDeclWithBody"),
			TEXT(R"(
delegate void FOnActionBody() { }
)"),
			TEXT("Delegate with body should fail"));
	}

	TEST_METHOD(Declaration_Negative_NestedDelegate)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("DelDeclNested"),
			TEXT(R"(
class ADelNestedActor : AActor
{
	delegate void FOnActionNested();
}
)"),
			TEXT("Nested delegate inside class should fail"));
	}

	TEST_METHOD(Declaration_Negative_DelegateAsLocalType)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("DelDeclLocal"),
			TEXT(R"(
class ADelLocalActor : AActor
{
	void Foo()
	{
		delegate void FOnActionLocal();
	}
}
)"),
			TEXT("Delegate as local type inside function should fail"));
	}

	TEST_METHOD(Declaration_Negative_VoidDelegateWithReturn)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("DelDeclEventReturn"),
			TEXT(R"(
event int FOnChangedReturn();
)"),
			TEXT("Event (multicast) with non-void return should fail"));
	}

	TEST_METHOD(Declaration_Negative_EventWithInvalidParamType)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("DelDeclEventBadParam"),
			TEXT(R"(
event void FOnChangedBadParam(NonExistentType X);
)"),
			TEXT("Event with invalid parameter type should fail"));
	}

	TEST_METHOD(Declaration_Negative_DelegateEmptyParens)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("DelDeclNoParens"),
			TEXT(R"(
delegate void FOnActionNoParens;
)"),
			TEXT("Delegate without parentheses should fail"));
	}

	// ====================================================================
	// Binding — Positive
	// ====================================================================

	TEST_METHOD(Binding_Positive_BindUFunction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("DelBindUFunc"),
			TEXT(R"(
delegate void FOnActionBind();

class ADelBindActor : AActor
{
	UPROPERTY()
	FOnActionBind OnAction;

	void HandleAction() { }

	void Setup()
	{
		OnAction.BindUFunction(this, n"HandleAction");
	}
}
)"),
			TEXT("Bind unicast delegate with BindUFunction"));
	}

	TEST_METHOD(Binding_Positive_AddUFunction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("DelBindAddUFunc"),
			TEXT(R"(
event void FOnChangedBind(int Val);

class ADelBindAddActor : AActor
{
	UPROPERTY()
	FOnChangedBind OnChanged;

	void HandleChanged(int Val) { }

	void Setup()
	{
		OnChanged.AddUFunction(this, n"HandleChanged");
	}
}
)"),
			TEXT("AddUFunction to multicast event"));
	}

	TEST_METHOD(Binding_Positive_ExecuteIfBound)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("DelBindExecute"),
			TEXT(R"(
delegate void FOnActionExec();

class ADelExecActor : AActor
{
	UPROPERTY()
	FOnActionExec OnAction;

	void Fire()
	{
		OnAction.ExecuteIfBound();
	}
}
)"),
			TEXT("ExecuteIfBound on unicast delegate"));
	}

	TEST_METHOD(Binding_Positive_Broadcast)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("DelBindBroadcast"),
			TEXT(R"(
event void FOnChangedBroadcast(int Val);

class ADelBroadcastActor : AActor
{
	UPROPERTY()
	FOnChangedBroadcast OnChanged;

	void Fire()
	{
		OnChanged.Broadcast(42);
	}
}
)"),
			TEXT("Broadcast multicast event"));
	}

	// ====================================================================
	// Binding — Negative
	// ====================================================================

	TEST_METHOD(Binding_Negative_WrongArgCountBroadcast)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("DelBindBadArgCount"),
			TEXT(R"(
event void FOnChangedBadArgCnt(int Val);

class ADelBadArgCntActor : AActor
{
	UPROPERTY()
	FOnChangedBadArgCnt OnChanged;

	void Fire()
	{
		OnChanged.Broadcast();
	}
}
)"),
			TEXT("Broadcast with wrong argument count should fail"));
	}

	TEST_METHOD(Binding_Negative_WrongArgTypeBroadcast)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("DelBindBadArgType"),
			TEXT(R"(
event void FOnChangedBadArgType(int Val);

class ADelBadArgTypeActor : AActor
{
	UPROPERTY()
	FOnChangedBadArgType OnChanged;

	void Fire()
	{
		OnChanged.Broadcast("hello");
	}
}
)"),
			TEXT("Broadcast with wrong argument type should fail"));
	}

	TEST_METHOD(Binding_Negative_UndeclaredDelegateType)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("DelBindUndeclared"),
			TEXT(R"(
class ADelUndeclaredActor : AActor
{
	UPROPERTY()
	FNonExistentDelegate OnAction;
}
)"),
			TEXT("Using undeclared delegate type should fail"));
	}

	TEST_METHOD(Binding_Negative_BindToNonExistentFunction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// DISABLED(#as-engine-behavior): structural-validation-absent — AS 的 BindUFunction 是运行时动态绑定，编译期不校验函数名是否存在
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("DelBindBadFunc"),
			TEXT(R"(
delegate void FOnActionBadFunc();

class ADelBadFuncActor : AActor
{
	UPROPERTY()
	FOnActionBadFunc OnAction;

	void Setup()
	{
		OnAction.BindUFunction(this, n"NonExistentHandler");
	}
}
)"),
			TEXT("Bind to non-existent function should fail"));
#endif
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
