#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "Support/AngelscriptNativeCaseTestSupport.h"
#include "AngelscriptTestMacros.h"

// Raw SDK default-trait coverage.

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FDefaultTraitTests,
	"Angelscript.TestModule.AngelScriptSDK.TypeSystem.DefaultTrait",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(DefaultTraitModifiers)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		AS_NATIVE_PRODUCT("TYPE-DEFAULT-TRAIT-METADATA-RUNTIME",
			ENativeEvidence::Compile
				| ENativeEvidence::Metadata
				| ENativeEvidence::Runtime
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK default-trait modifier test should create a standalone engine")));

		const FString Source = ASTEST_AS(R"AS(
			int DefaultsOnlyValue() defaults
			{
				return 7;
			}

			int UnsafeConstructionValue() unsafe_during_construction
			{
				return 5;
			}

			int Entry()
			{
				return 1;
			}
		)AS");
		const TCHAR* Modifiers[] =
		{
			TEXT("defaults"),
			TEXT("unsafe-during-construction"),
		};
		const TCHAR* Observations[] =
		{
			TEXT("metadata"),
			TEXT("direct-context"),
		};
		for (const TCHAR* Modifier : Modifiers)
		{
			for (const TCHAR* Observation : Observations)
			{
				PrintGeneratedAsSource(
					*TestRunner,
					MakeNativeCaseId(
						"TYPE-DEFAULT-TRAIT-METADATA-RUNTIME",
						{ Modifier, Observation }),
					TEXT("TypeDefaultTraitModifiers"),
					Source);
			}
		}

		const FTCHARToUTF8 SourceUtf8(*Source);
		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"SDKDefaultTraitModifiers",
			SourceUtf8.Get());
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptFunction* const DefaultsFunction =
			GetNativeFunctionByExactDecl(Module, "int DefaultsOnlyValue()");
		asIScriptFunction* const UnsafeFunction =
			GetNativeFunctionByExactDecl(Module, "int UnsafeConstructionValue()");
		asIScriptFunction* const ControlFunction =
			GetNativeFunctionByExactDecl(Module, "int Entry()");
		ASSERT_THAT(IsNotNull(
			DefaultsFunction,
			TEXT("Default-trait product should publish its defaults-only function")));
		ASSERT_THAT(IsNotNull(
			UnsafeFunction,
			TEXT("Default-trait product should publish its unsafe-construction function")));
		ASSERT_THAT(IsNotNull(
			ControlFunction,
			TEXT("Default-trait product should publish its ordinary control function")));
		if (DefaultsFunction == nullptr || UnsafeFunction == nullptr || ControlFunction == nullptr)
		{
			return;
		}

		const asCScriptFunction* const InternalDefaults =
			static_cast<const asCScriptFunction*>(DefaultsFunction);
		const asCScriptFunction* const InternalUnsafe =
			static_cast<const asCScriptFunction*>(UnsafeFunction);
		const asCScriptFunction* const InternalControl =
			static_cast<const asCScriptFunction*>(ControlFunction);
		ASSERT_THAT(IsTrue(
			InternalDefaults->traits.GetTrait(asTRAIT_DEFAULTS_ONLY),
			TEXT("defaults modifier should publish the defaults-only internal trait")));
		ASSERT_THAT(IsFalse(
			InternalDefaults->traits.GetTrait(asTRAIT_UNSAFE_DURING_CONSTRUCTION),
			TEXT("defaults modifier should not publish the unsafe-construction trait")));
		ASSERT_THAT(IsTrue(
			InternalUnsafe->traits.GetTrait(asTRAIT_UNSAFE_DURING_CONSTRUCTION),
			TEXT("unsafe modifier should publish the unsafe-construction internal trait")));
		ASSERT_THAT(IsFalse(
			InternalUnsafe->traits.GetTrait(asTRAIT_DEFAULTS_ONLY),
			TEXT("unsafe modifier should not publish the defaults-only trait")));
		ASSERT_THAT(IsFalse(
			InternalControl->traits.GetTrait(asTRAIT_DEFAULTS_ONLY),
			TEXT("Ordinary control function should not inherit the defaults-only trait")));
		ASSERT_THAT(IsFalse(
			InternalControl->traits.GetTrait(asTRAIT_UNSAFE_DURING_CONSTRUCTION),
			TEXT("Ordinary control function should not inherit the unsafe-construction trait")));

		{
			FSdkFunctionInvoker DefaultsInvoker(
				*TestRunner,
				ScriptEngine,
				Module,
				"int DefaultsOnlyValue()");
			FSdkFunctionInvoker UnsafeInvoker(
				*TestRunner,
				ScriptEngine,
				Module,
				"int UnsafeConstructionValue()");
			FSdkFunctionInvoker ControlInvoker(
				*TestRunner,
				ScriptEngine,
				Module,
				"int Entry()");
			ASSERT_THAT(IsTrue(
				DefaultsInvoker.IsValid(),
				TEXT("Default-trait product should prepare its exact defaults-only declaration")));
			ASSERT_THAT(IsTrue(
				UnsafeInvoker.IsValid(),
				TEXT("Default-trait product should prepare its exact unsafe declaration")));
			ASSERT_THAT(IsTrue(
				ControlInvoker.IsValid(),
				TEXT("Default-trait product should prepare its exact ordinary control declaration")));
			if (DefaultsInvoker.IsValid())
			{
				ASSERT_THAT(AreEqual(
					7,
					DefaultsInvoker.CallAndReturn<int32>(INDEX_NONE),
					TEXT("Direct context execution should preserve defaults-only function bytecode")));
			}
			if (UnsafeInvoker.IsValid())
			{
				ASSERT_THAT(AreEqual(
					5,
					UnsafeInvoker.CallAndReturn<int32>(INDEX_NONE),
					TEXT("Direct context execution should preserve unsafe-construction function bytecode")));
			}
			if (ControlInvoker.IsValid())
			{
				ASSERT_THAT(AreEqual(
					1,
					ControlInvoker.CallAndReturn<int32>(INDEX_NONE),
					TEXT("Ordinary control execution should remain independent from both trait modifiers")));
			}
		}

		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Module.Discard(),
			TEXT("Default-trait product should explicitly discard its module")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(
				"SDKDefaultTraitModifiers",
				asGM_ONLY_IF_EXISTS),
			TEXT("Default-trait module should be absent after cleanup")));
	}
};

#endif
