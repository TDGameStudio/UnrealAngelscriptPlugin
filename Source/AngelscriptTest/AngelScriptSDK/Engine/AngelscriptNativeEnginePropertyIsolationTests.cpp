#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FEnginePropertyIsolationTests,
	"Angelscript.TestModule.AngelScriptSDK.Engine.PropertyIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FPropertyCase
	{
		const char* Name;
		asEEngineProp Property;
	};

	static constexpr FPropertyCase PropertyCases[] =
	{
		{ "allow_unsafe_references", asEP_ALLOW_UNSAFE_REFERENCES },
		{ "allow_multiline_strings", asEP_ALLOW_MULTILINE_STRINGS },
		{ "script_scanner", asEP_SCRIPT_SCANNER },
		{ "optimize_bytecode", asEP_OPTIMIZE_BYTECODE },
		{ "auto_garbage_collect", asEP_AUTO_GARBAGE_COLLECT },
		{ "alter_syntax_named_args", asEP_ALTER_SYNTAX_NAMED_ARGS },
		{ "disallow_value_assign_for_ref_type", asEP_DISALLOW_VALUE_ASSIGN_FOR_REF_TYPE },
		{ "allow_implicit_handle_types", asEP_ALLOW_IMPLICIT_HANDLE_TYPES },
		{ "require_enum_scope", asEP_REQUIRE_ENUM_SCOPE },
		{ "always_impl_default_construct", asEP_ALWAYS_IMPL_DEFAULT_CONSTRUCT },
		{ "always_impl_default_copy", asEP_ALWAYS_IMPL_DEFAULT_COPY },
		{ "always_impl_default_copy_construct", asEP_ALWAYS_IMPL_DEFAULT_COPY_CONSTRUCT },
		{ "member_init_mode", asEP_MEMBER_INIT_MODE },
		{ "allow_double_type", asEP_ALLOW_DOUBLE_TYPE }
	};

	static FString BuildProbeSource(const FPropertyCase& PropertyCase, const int32 AppliedValue)
	{
		FString Source;
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(
			Source,
			FString::Printf(TEXT("int Read_%hs()"), PropertyCase.Name));
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(Source, TEXT("{"));
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(
			Source,
			FString::Printf(TEXT("\tint Value = %d;"), AppliedValue));
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(Source, TEXT("\treturn Value + 1;"));
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static bool ExecuteProbe(
		FAutomationTestBase& Test,
		asIScriptEngine* ScriptEngine,
		asIScriptFunction* Function,
		const int32 ExpectedValue)
	{
		using namespace AngelscriptNativeTestSupport;
		FNoDiscardAsserter Assert(Test);
		asIScriptContext* const Context = ScriptEngine != nullptr ? ScriptEngine->CreateContext() : nullptr;
		ON_SCOPE_EXIT
		{
			if (Context != nullptr)
			{
				Context->Release();
			}
		};
		if (!Assert.IsNotNull(Context, TEXT("Engine property probe should create a context")))
		{
			return false;
		}

		const int ExecuteResult = PrepareAndExecute(Context, Function);
		const int32 ActualValue = static_cast<int32>(Context->GetReturnDWord());
		return Assert.AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			ExecuteResult,
			TEXT("Engine property probe should finish execution"))
			&& Assert.AreEqual(ExpectedValue, ActualValue,
				TEXT("Engine property probe should return the generated value"));
	}

public:
	TEST_METHOD(PropertyValuesStayIndependentAcrossGeneratedCases)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engines[3];
		Engines[0].Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engines[0].Destroy();
		};
		Engines[1].Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engines[1].Destroy();
		};
		Engines[2].Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engines[2].Destroy();
		};

		AS_NATIVE_PRODUCT("ENG-PROPERTY-ISOLATION",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		asIScriptEngine* const Primary = Engines[0].Get();
		asIScriptEngine* const Control = Engines[1].Get();
		asIScriptEngine* const Third = Engines[2].Get();
		ASSERT_THAT(IsNotNull(Primary, TEXT("Primary property engine should exist")));
		ASSERT_THAT(IsNotNull(Control, TEXT("Control property engine should exist")));
		ASSERT_THAT(IsNotNull(Third, TEXT("Third property engine should exist")));
		if (Primary == nullptr || Control == nullptr || Third == nullptr)
		{
			return;
		}

		for (const FPropertyCase& PropertyCase : PropertyCases)
		{
			const int32 ControlValue = Control->GetEngineProperty(PropertyCase.Property);
			const int32 ThirdValue = Third->GetEngineProperty(PropertyCase.Property);

			for (const int32 AppliedValue : { 0, 1 })
			{
				const FString CaseId = MakeNativeCaseId(
					"ENG-PROPERTY-ISOLATION",
					{ ANSI_TO_TCHAR(PropertyCase.Name), AppliedValue == 0 ? TEXT("zero") : TEXT("one") });
				const FNativeCaseContext Case(CaseId);
				const FString ModuleName = Case.MakeModuleName(TEXT("NativeEngineProperty"));
				const FString Source = BuildProbeSource(PropertyCase, AppliedValue);
				PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);

				ASSERT_THAT(AreEqual(
					asSUCCESS,
					Primary->SetEngineProperty(PropertyCase.Property, AppliedValue),
					*Case.Describe(TEXT("Engine property should accept both generated values"))));
				ASSERT_THAT(AreEqual(
					AppliedValue,
					Primary->GetEngineProperty(PropertyCase.Property),
					*Case.Describe(TEXT("Primary engine should expose the applied property value"))));
				ASSERT_THAT(AreEqual(
					ControlValue,
					Control->GetEngineProperty(PropertyCase.Property),
					*Case.Describe(TEXT("Control engine should retain its independent property value"))));
				ASSERT_THAT(AreEqual(
					ThirdValue,
					Third->GetEngineProperty(PropertyCase.Property),
					*Case.Describe(TEXT("Third engine should retain its independent property value"))));

				const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
				const FTCHARToUTF8 SourceUtf8(*Source);
				asIScriptModule* Module = nullptr;
				const int BuildResult = CompileNativeModule(
					Primary,
					ModuleNameUtf8.Get(),
					SourceUtf8.Get(),
					Module);
				ASSERT_THAT(AreEqual(
					asSUCCESS,
					BuildResult,
					*Case.Describe(TEXT("Generated property probe should compile"))));
				ASSERT_THAT(IsNotNull(Module, *Case.Describe(TEXT("Generated property probe should publish a module"))));
				if (Module == nullptr)
				{
					continue;
				}

				ON_SCOPE_EXIT
				{
					if (Primary->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS) != nullptr)
					{
						Primary->DiscardModule(ModuleNameUtf8.Get());
					}
				};

				const FString Declaration = FString::Printf(
					TEXT("int Read_%hs()"),
					PropertyCase.Name);
				const FTCHARToUTF8 DeclarationUtf8(*Declaration);
				asIScriptFunction* const Function = GetNativeFunctionByExactDecl(Module, DeclarationUtf8.Get());
				ASSERT_THAT(IsNotNull(Function, *Case.Describe(TEXT("Generated property probe should resolve its exact declaration"))));
				if (Function != nullptr)
				{
					ASSERT_THAT(IsTrue(ExecuteProbe(*TestRunner, Primary, Function, AppliedValue + 1),
						*Case.Describe(TEXT("Generated property probe should execute with the selected engine"))));
				}
				ASSERT_THAT(AreEqual(
					asSUCCESS,
					Primary->DiscardModule(ModuleNameUtf8.Get()),
					*Case.Describe(TEXT("Generated property probe should explicitly discard its module"))));
				ASSERT_THAT(IsNull(
					Primary->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
					*Case.Describe(TEXT("Generated property probe should be absent before the next cell"))));
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
