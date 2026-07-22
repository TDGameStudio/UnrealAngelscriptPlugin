#include "../Support/AngelscriptNativeExecutionTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FInterfaceSemanticsTests,
	"Angelscript.TestModule.AngelScriptSDK.Conformance.Interfaces",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(InterfaceSemanticsInterfaceBridge)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Interface bridge test should create a standalone engine")));

		const int InterfaceResult = ScriptEngine->RegisterInterface("appintf");
		const int MethodResult = InterfaceResult >= 0 ? ScriptEngine->RegisterInterfaceMethod("appintf", "void test()") : InterfaceResult;
		ASSERT_THAT(IsTrue(InterfaceResult >= 0 && MethodResult >= 0, TEXT("Interface bridge test should register an application interface and method")));
		if (InterfaceResult < 0)
		{
			return;
		}

		ASSERT_THAT(AreEqual(FString(TEXT("appintf")), FString(UTF8_TO_TCHAR(ScriptEngine->GetTypeDeclaration(InterfaceResult))),
			TEXT("Interface bridge test should preserve the registered declaration")));
		asITypeInfo* const InterfaceType = ScriptEngine->GetTypeInfoByName("appintf");
		ASSERT_THAT(IsNotNull(InterfaceType, TEXT("Interface bridge test should expose the registered type")));
		if (InterfaceType != nullptr)
		{
			ASSERT_THAT(AreEqual(1, static_cast<int32>(InterfaceType->GetMethodCount()), TEXT("Interface bridge test should expose the registered method")));
		}
	}

	TEST_METHOD(InheritedInterfaceMethod)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Inherited member test should create a standalone engine")));

		FScopedNativeModule Module(*TestRunner, Engine, "InheritedInterfaceMethod", R"(
class B { bool touched = false; void Touch() { touched = true; } }
class D : B {}
bool TouchInheritedMember() { D value = D(); value.Touch(); return value.touched; }
)");
		if (!Module.IsValid())
		{
			return;
		}

		asITypeInfo* const BaseType = Module->GetTypeInfoByName("B");
		asITypeInfo* const DerivedType = Module->GetTypeInfoByName("D");
		ASSERT_THAT(IsNotNull(BaseType, TEXT("Inherited member test should expose the base script type")));
		ASSERT_THAT(IsNotNull(DerivedType, TEXT("Inherited member test should expose the derived script type")));
		if (DerivedType != nullptr)
		{
			ASSERT_THAT(AreEqual(BaseType, DerivedType->GetBaseType(), TEXT("Inherited member test should preserve the base-type relationship")));
		}
		asIScriptFunction* const EntryFunction = GetNativeFunctionByExactDecl(Module, "bool TouchInheritedMember()");
		ASSERT_THAT(IsNotNull(EntryFunction, TEXT("Inherited member test should resolve the compiled inheritance entry declaration")));
		// Executing a script-object construction in an isolated raw engine reaches the
		// UE class allocator without its host class context and asserts the editor.
		// Language/Inheritance owns that current-fork execution limitation; this
		// conformance test keeps the safe compile-and-metadata contract executable.
	}
};

#endif
