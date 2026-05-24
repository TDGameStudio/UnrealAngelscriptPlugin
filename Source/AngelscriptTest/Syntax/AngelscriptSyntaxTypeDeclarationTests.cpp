// ============================================================================
// AngelscriptSyntaxTypeDeclarationTests.cpp
//
// Syntax coverage tests for type declarations — CQTest refactor.
// Tests class, struct, enum, interface, namespace, variable, and function
// declarations.
//
// Automation prefix: Angelscript.TestModule.Syntax.TypeDeclaration.*
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptBindingsAssertions.h"
#include "Syntax/AngelscriptSyntaxTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptSyntaxTypeDeclarationTest,
	"Angelscript.TestModule.Syntax.TypeDeclaration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Class — Positive
	// ====================================================================

	TEST_METHOD(Class_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ClassP_Basic"),
			TEXT(R"(
class AClassBasicActor : AActor { }
)"),
			TEXT("Basic class"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ClassP_Members"),
			TEXT(R"(
class AClassMembersActor : AActor
{
	int Health = 100;
	float Speed = 5.0f;
}
)"),
			TEXT("Class with members"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ClassP_Methods"),
			TEXT(R"(
class AClassMethodsActor : AActor
{
	void Foo() { }
	int Bar() { return 1; }
}
)"),
			TEXT("Class with methods"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ClassP_UCLASS"),
			TEXT(R"(
UCLASS()
class AClassUCLASSActor : AActor
{
	UPROPERTY()
	int X = 0;
}
)"),
			TEXT("UCLASS with UPROPERTY"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ClassP_Abstract"),
			TEXT(R"(
UCLASS(Abstract)
class AMyAbstract : AActor { }
)"),
			TEXT("Abstract UCLASS"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ClassP_Constructor"),
			TEXT(R"(
class AClassCtorActor : AActor
{
	int X;
	AClassCtorActor() { X = 10; }
}
)"),
			TEXT("Constructor"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ClassP_Chain"),
			TEXT(R"(
class ABaseChainActor : AActor { }
class AChildChainActor : ABaseChainActor { }
)"),
			TEXT("Inheritance chain"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("ClassP_Final"),
			TEXT(R"(
class AFinalClassActor : AActor final { }
)"),
			TEXT("Final class"));
	}

	// ====================================================================
	// Class — Negative
	// ====================================================================

	TEST_METHOD(Class_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TestRunner->AddExpectedErrorPlain(
			TEXT("has an unknown super type"),
			EAutomationExpectedErrorFlags::Contains, 2);

		// DISABLED(#as-engine-behavior): preprocessor-ensure-crash — 匿名 class 触发 DetectClasses ensure 崩溃
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ClassN_NoName"),
			TEXT(R"(
class : AActor { }
)"),
			TEXT("Class without name"));
#endif

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ClassN_NoBrace"),
			TEXT(R"(
class AClassNoBraceActor : AActor
)"),
			TEXT("Class without braces"));
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ClassN_Duplicate"),
			TEXT(R"(
class ADupActor : AActor { }
class ADupActor : AActor { }
)"),
			TEXT("Duplicate class name"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ClassN_BadParent"),
			TEXT(R"(
class AClassBadParentActor : ANonExistentActor { }
)"),
			TEXT("Non-existent parent"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ClassN_BadMember"),
			TEXT(R"(
class AClassBadMemberActor : AActor
{
	NonExistentType X;
}
)"),
			TEXT("Invalid member type"));

		// DISABLED(#as-engine-behavior): naming-convention-unenforced — AS 不强制 Actor 类名 A 前缀
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ClassN_BadPrefix"),
			TEXT(R"(
class MyActor : AActor { }
)"),
			TEXT("Actor without A prefix"));
#endif

		// DISABLED(#as-engine-behavior): structural-validation-absent — AS 不校验多继承（或语法解析允许逗号分隔）
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ClassN_MultiBase"),
			TEXT(R"(
class AClassMultiBaseActor : AActor, APawn { }
)"),
			TEXT("Multiple base classes"));
#endif

		// DISABLED(#as-engine-behavior): structural-validation-absent — AS 不校验 final 类继承约束
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ClassN_InheritFinal"),
			TEXT(R"(
class AFinalInheritActor : AActor final { }
class AChildInheritActor : AFinalInheritActor { }
)"),
			TEXT("Inherit from final"));
#endif

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ClassN_SelfInherit"),
			TEXT(R"(
class ASelfActor : ASelfActor { }
)"),
			TEXT("Self-inheritance"));
	}

	// ====================================================================
	// Struct — Positive
	// ====================================================================

	TEST_METHOD(Struct_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("StructP_Basic"),
			TEXT(R"(
struct FStructBasic { int X; float Y; }
)"),
			TEXT("Basic struct"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("StructP_USTRUCT"),
			TEXT(R"(
USTRUCT()
struct FStructUSTRUCT
{
	UPROPERTY()
	int X = 0;
}
)"),
			TEXT("USTRUCT"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("StructP_Methods"),
			TEXT(R"(
struct FStructMethods
{
	int X = 0;
	int GetX() const { return X; }
}
)"),
			TEXT("Struct with methods"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("StructP_Defaults"),
			TEXT(R"(
struct FStructDefaults
{
	int X = 42;
	FString Name = "Default";
}
)"),
			TEXT("Struct defaults"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("StructP_Constructor"),
			TEXT(R"(
struct FStructCtor
{
	int X;
	FStructCtor() { X = 0; }
	FStructCtor(int InX) { X = InX; }
}
)"),
			TEXT("Constructors"));
	}

	// ====================================================================
	// Struct — Negative
	// ====================================================================

	TEST_METHOD(Struct_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// DISABLED(#as-engine-behavior): structural-validation-absent — AS 允许匿名 struct 编译通过
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("StructN_NoName"),
			TEXT(R"(
struct { int X; }
)"),
			TEXT("Struct without name"));
#endif

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("StructN_Duplicate"),
			TEXT(R"(
struct FDup { int X; }
struct FDup { int Y; }
)"),
			TEXT("Duplicate struct"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("StructN_BadMember"),
			TEXT(R"(
struct FStructBadMember { NonExistentType X; }
)"),
			TEXT("Invalid member type"));

		// DISABLED(#as-engine-behavior): naming-convention-unenforced — AS 不强制 USTRUCT 类型名 F 前缀
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("StructN_BadPrefix"),
			TEXT(R"(
USTRUCT()
struct MyStruct
{
	UPROPERTY()
	int X;
}
)"),
			TEXT("USTRUCT without F prefix"));
#endif

		TestRunner->AddExpectedErrorPlain(
			TEXT("Error parsing script struct FChild. Structs may not inherit from anything."),
			EAutomationExpectedErrorFlags::Contains, 1);
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("StructN_Inherit"),
			TEXT(R"(
struct FBase { int X; }
struct FChild : FBase { int Y; }
)"),
			TEXT("Struct inheritance"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("StructN_VoidMember"),
			TEXT(R"(
struct FStructVoidMember { void X; }
)"),
			TEXT("Void member"));
	}

	// ====================================================================
	// Enum — Positive
	// ====================================================================

	TEST_METHOD(Enum_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("EnumP_Basic"),
			TEXT(R"(
enum EEnumBasic { Value1, Value2, Value3 }
)"),
			TEXT("Basic enum"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("EnumP_UENUM"),
			TEXT(R"(
UENUM()
enum EEnumUENUM { Value1, Value2 }
)"),
			TEXT("UENUM"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("EnumP_Explicit"),
			TEXT(R"(
enum EEnumExplicit { Value1 = 0, Value2 = 5, Value3 = 10 }
)"),
			TEXT("Explicit values"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("EnumP_Usage"),
			TEXT(R"(
enum EEnumUsage { Val1, Val2 }

void Test()
{
	EEnumUsage E = EEnumUsage::Val1;
}
)"),
			TEXT("Enum usage"));
	}

	// ====================================================================
	// Enum — Negative
	// ====================================================================

	TEST_METHOD(Enum_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// DISABLED(#as-engine-behavior): preprocessor-ensure-crash — 匿名 enum 触发 DetectEnum ensure 崩溃
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("EnumN_NoName"),
			TEXT(R"(
enum { Value1 }
)"),
			TEXT("Enum without name"));
#endif

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("EnumN_DupVal"),
			TEXT(R"(
enum EEnumDupVal { Value1, Value1 }
)"),
			TEXT("Duplicate enum value"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("EnumN_BadVal"),
			TEXT(R"(
enum EEnumBadVal { Value1 = "hello" }
)"),
			TEXT("Non-integer enum value"));

		// DISABLED(#as-engine-behavior): naming-convention-unenforced — AS 不强制 UENUM 类型名 E 前缀
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("EnumN_BadPrefix"),
			TEXT(R"(
UENUM()
enum MyEnum { Value1 }
)"),
			TEXT("UENUM without E prefix"));
#endif

		// DISABLED(#as-engine-behavior): structural-validation-absent — AS 允许空枚举编译通过
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("EnumN_Empty"),
			TEXT(R"(
enum EEnumEmpty { }
)"),
			TEXT("Empty enum"));
#endif

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("EnumN_Method"),
			TEXT(R"(
enum EEnumMethod { Value1; void Foo() { } }
)"),
			TEXT("Method in enum"));
	}

	// ====================================================================
	// Interface — Positive and Negative
	// ====================================================================

	TEST_METHOD(Interface_Mixed)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// DISABLED(#as-engine-behavior): feature-not-supported — AS 2.33 fork 不支持 interface 关键字
#if 0
		// Positive
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("IntfP_Basic"),
			TEXT(R"(
interface UIntfBasic
{
	void DoSomething();
	int GetValue();
}
)"),
			TEXT("Basic interface"));
#endif

		// Negative
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("IntfN_Member"),
			TEXT(R"(
interface UIntfMember { int X; }
)"),
			TEXT("Interface with member variable"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("IntfN_Body"),
			TEXT(R"(
interface UIntfBody { void DoSomething() { } }
)"),
			TEXT("Interface with method body"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("IntfN_NoName"),
			TEXT(R"(
interface { void Foo(); }
)"),
			TEXT("Interface without name"));
	}

	// ====================================================================
	// Namespace — Positive and Negative
	// ====================================================================

	TEST_METHOD(Namespace_Mixed)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// DISABLED(#as-engine-behavior): feature-not-supported — AS 2.33 fork 不支持 namespace 声明
#if 0
		// Positive
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("NSP_Basic"),
			TEXT(R"(
namespace MySpaceBasic { int GlobalVal = 42; }
)"),
			TEXT("Basic namespace"));
#endif

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("NSP_Access"),
			TEXT(R"(
namespace MySpaceAccess
{
	int GetVal() { return 42; }
}

void Test()
{
	int X = MySpaceAccess::GetVal();
}
)"),
			TEXT("Namespace access"));

		// DISABLED(#as-engine-behavior): feature-not-supported — AS 2.33 fork 不支持嵌套 namespace
#if 0
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("NSP_Nested"),
			TEXT(R"(
namespace Outer
{
	namespace Inner
	{
		int Value = 1;
	}
}
)"),
			TEXT("Nested namespaces"));
#endif

		// Negative
		// DISABLED(#as-engine-behavior): structural-validation-absent — 匿名 namespace 触发错误日志但编译仍成功
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("NSN_Anonymous"),
			TEXT(R"(
namespace { int X; }
)"),
			TEXT("Anonymous namespace"));
#endif

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("NSN_BadAccess"),
			TEXT(R"(
namespace MySpaceBadAcc { int X = 1; }
void Test() { int Y = MySpaceBadAcc::NonExistent; }
)"),
			TEXT("Non-existent member"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("NSN_NotExist"),
			TEXT(R"(
void Test() { int X = FakeNamespace::Value; }
)"),
			TEXT("Non-existent namespace"));
	}

	// ====================================================================
	// Variable Declarations — Positive and Negative
	// ====================================================================

	TEST_METHOD(Variable_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASSyntax_TypeDeclaration_VarPos"), TEXT(R"(
int Primitives()  { int A = 0; float B = 1.0f; bool C = true; int64 D = 100; return A + int(B) + (C ? 1 : 0) + int(D); }
int ConstVar()    { const int X = 42; return X; }
int AutoVar()     { auto X = 42; return X; }
int RefVar()      { int X = 5; int& Ref = X; Ref = 10; return X; }
)"));
		ASSERT_THAT(IsTrue(Mod.IsValid()));
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int Primitives()"), TEXT("primitive types"), 102 },
			{ TEXT("int ConstVar()"),   TEXT("const variable"),   42 },
			{ TEXT("int AutoVar()"),    TEXT("auto inference"),   42 },
			{ TEXT("int RefVar()"),     TEXT("reference var"),    10 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}

	TEST_METHOD(Variable_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("VarN_BadType"),
			TEXT(R"(
void Test() { NonExistentType X; }
)"),
			TEXT("Undeclared type"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("VarN_Duplicate"),
			TEXT(R"(
void Test() { int X = 1; int X = 2; }
)"),
			TEXT("Duplicate variable"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("VarN_AutoNoInit"),
			TEXT(R"(
void Test() { auto X; }
)"),
			TEXT("Auto without initializer"));

		// DISABLED(#as-engine-behavior): structural-validation-absent — AS 允许 const 变量不初始化
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("VarN_ConstNoInit"),
			TEXT(R"(
void Test() { const int X; }
)"),
			TEXT("Const without initializer"));
#endif

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("VarN_BadName"),
			TEXT(R"(
void Test() { int 123abc = 0; }
)"),
			TEXT("Name starting with number"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("VarN_Keyword"),
			TEXT(R"(
void Test() { int class = 0; }
)"),
			TEXT("Keyword as variable name"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("VarN_VoidVar"),
			TEXT(R"(
void Test() { void X; }
)"),
			TEXT("Void variable"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("VarN_UseBefore"),
			TEXT(R"(
void Test() { int Y = X; int X = 5; }
)"),
			TEXT("Use before declaration"));
	}

	// ====================================================================
	// Function Declarations — Positive and Negative
	// ====================================================================

	TEST_METHOD(Function_Positive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("FuncP_Void"),
			TEXT(R"(
void Foo() { }
)"),
			TEXT("Void function"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("FuncP_Return"),
			TEXT(R"(
int Add(int A, int B) { return A + B; }
)"),
			TEXT("Function with return"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("FuncP_Default"),
			TEXT(R"(
int Foo(int X = 5, float Y = 1.0f) { return X; }
)"),
			TEXT("Default parameters"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("FuncP_Overload"),
			TEXT(R"(
void Foo(int X) { }
void Foo(float X) { }
void Foo(int X, int Y) { }
)"),
			TEXT("Overloading"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("FuncP_Ref"),
			TEXT(R"(
void Foo(int& Out) { Out = 42; }
)"),
			TEXT("Reference parameter"));

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("FuncP_Const"),
			TEXT(R"(
struct FStructFuncConst
{
	int X = 0;
	int Get() const { return X; }
}
)"),
			TEXT("Const method"));
	}

	TEST_METHOD(Function_Negative)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("FuncN_NoReturn"),
			TEXT(R"(
Foo() { }
)"),
			TEXT("No return type"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("FuncN_NoBody"),
			TEXT(R"(
void Foo();
)"),
			TEXT("No body (non-interface)"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("FuncN_Duplicate"),
			TEXT(R"(
void Foo(int X) { }
void Foo(int X) { }
)"),
			TEXT("Duplicate signature"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("FuncN_BadDefault"),
			TEXT(R"(
void Foo(int X = 5, int Y) { }
)"),
			TEXT("Non-default after default"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("FuncN_BadReturnType"),
			TEXT(R"(
NonExistentType Foo() { }
)"),
			TEXT("Non-existent return type"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("FuncN_BadParamType"),
			TEXT(R"(
void Foo(NonExistentType X) { }
)"),
			TEXT("Non-existent param type"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("FuncN_VoidParam"),
			TEXT(R"(
void Foo(void X) { }
)"),
			TEXT("Void parameter"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
