#include "../Support/AngelscriptNativeExecutionTestSupport.h"
#include "AngelscriptTestMacros.h"

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

		AS_NATIVE_PRODUCT("CONF-APPLICATION-INTERFACE-REGISTRATION",
			ENativeEvidence::Metadata
				| ENativeEvidence::Isolation);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Interface bridge test should create a standalone engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const int InterfaceResult = ScriptEngine->RegisterInterface("appintf");
		ASSERT_THAT(IsTrue(
			InterfaceResult >= 0,
			TEXT("Interface bridge test should register an application interface")));
		if (InterfaceResult < 0)
		{
			return;
		}

		const int MethodResult =
			ScriptEngine->RegisterInterfaceMethod("appintf", "void test()");
		ASSERT_THAT(IsTrue(
			MethodResult >= 0,
			TEXT("Interface bridge test should register the application interface method")));

		ASSERT_THAT(AreEqual(
			InterfaceResult,
			ScriptEngine->GetTypeIdByDecl("appintf"),
			TEXT("Interface bridge test should resolve the registered type by declaration")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("appintf")),
			FString(UTF8_TO_TCHAR(ScriptEngine->GetTypeDeclaration(InterfaceResult))),
			TEXT("Interface bridge test should preserve the registered declaration")));
		asITypeInfo* const InterfaceType = ScriptEngine->GetTypeInfoByName("appintf");
		ASSERT_THAT(IsNotNull(InterfaceType, TEXT("Interface bridge test should expose the registered type")));
		if (InterfaceType != nullptr)
		{
			ASSERT_THAT(AreEqual(
				InterfaceType,
				ScriptEngine->GetTypeInfoById(InterfaceResult),
				TEXT("Interface bridge type ID should preserve TypeInfo identity")));
			ASSERT_THAT(AreEqual(
				static_cast<asQWORD>(asOBJ_REF | asOBJ_SCRIPT_OBJECT | asOBJ_SHARED),
				InterfaceType->GetFlags(),
				TEXT("Application interface should publish the exact raw SDK object flags")));
			ASSERT_THAT(AreEqual(
				0,
				static_cast<int32>(InterfaceType->GetSize()),
				TEXT("Application interface should not publish instantiable storage")));
			ASSERT_THAT(AreEqual(
				1,
				static_cast<int32>(InterfaceType->GetMethodCount()),
				TEXT("Interface bridge test should expose exactly one registered method")));

			asIScriptFunction* const Method =
				InterfaceType->GetMethodByDecl("void test()");
			ASSERT_THAT(IsNotNull(
				Method,
				TEXT("Interface bridge test should resolve the exact registered method declaration")));
			if (Method != nullptr)
			{
				ASSERT_THAT(AreEqual(
					MethodResult,
					Method->GetId(),
					TEXT("Registered interface method ID should preserve function identity")));
				ASSERT_THAT(AreEqual(
					static_cast<int32>(asFUNC_INTERFACE),
					static_cast<int32>(Method->GetFuncType()),
					TEXT("Registered interface method should publish interface function kind")));
				ASSERT_THAT(AreEqual(
					InterfaceType,
					Method->GetObjectType(),
					TEXT("Registered interface method should retain its owning TypeInfo")));
			}
		}

		FNativeTestEngine ControlEngine;
		ControlEngine.Create(*TestRunner);
		ON_SCOPE_EXIT { ControlEngine.Destroy(); };
		ASSERT_THAT(IsNotNull(
			ControlEngine.Get(),
			TEXT("Interface isolation test should create an independent control engine")));
		if (ControlEngine.Get() != nullptr)
		{
			ASSERT_THAT(IsNull(
				ControlEngine.Get()->GetTypeInfoByName("appintf"),
				TEXT("Application interface registration should remain isolated to its owning engine")));
		}
	}

	TEST_METHOD(InheritedInterfaceMethod)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"LANG-INH-CLASS-RULE supersedes this metadata-only concrete-base predecessor with compile, diagnostic, metadata, runtime, and cleanup observations");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Inherited member test should create a standalone engine")));

		FScopedNativeModule Module(*TestRunner, Engine, "InheritedInterfaceMethod", ASTEST_AS_ANSI(R"AS(
			class B
			{
				bool touched = false;
				void Touch()
				{
					touched = true;
				}
			}

			class D : B
			{
			}

			bool TouchInheritedMember()
			{
				D value = D();
				value.Touch();
				return value.touched;
			}
		)AS"));
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
