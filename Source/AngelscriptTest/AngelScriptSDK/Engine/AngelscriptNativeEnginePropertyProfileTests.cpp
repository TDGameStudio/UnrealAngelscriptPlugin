#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FEnginePropertyProfileTests,
	"Angelscript.TestModule.AngelScriptSDK.Engine.PropertyProfiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FPropertyCase
	{
		const ANSICHAR* Name;
		asEEngineProp Property;
	};

	struct FProfileCase
	{
		const TCHAR* Name;
		asIScriptEngine* Primary;
		asIScriptEngine* Control;
	};

	inline static constexpr FPropertyCase PropertyCases[] =
	{
		{ "allow_unsafe_references", asEP_ALLOW_UNSAFE_REFERENCES },
		{ "use_character_literals", asEP_USE_CHARACTER_LITERALS },
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
		{ "typecheck_switch_enums", asEP_TYPECHECK_SWITCH_ENUMS },
		{ "allow_double_type", asEP_ALLOW_DOUBLE_TYPE },
	};

	inline static constexpr int32 AppliedValues[] = { 0, 1 };

	static FString BuildSource(const FPropertyCase& PropertyCase, const int32 AppliedValue)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("// property=%hs applied=%d"),
			PropertyCase.Name,
			AppliedValue));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("int Read_%hs()"), PropertyCase.Name));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tint AppliedValue = %d;"), AppliedValue));
		AppendGeneratedAsLine(Source, TEXT("\treturn AppliedValue + 1;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static FString MakeModuleName(const AngelscriptNativeTestSupport::FNativeCaseContext& Case)
	{
		return TEXT("EnginePropertyProfile_") + Case.GetId().Replace(TEXT("-"), TEXT("_"));
	}

public:
	TEST_METHOD(ProfilesByPropertyAndAppliedValue)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine ForkEngine;
		ForkEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			ForkEngine.Destroy();
		};
		FNativeTestEngine ForkControlEngine;
		ForkControlEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			ForkControlEngine.Destroy();
		};
		asIScriptEngine* BareEngine = CreateBareSdkEngine(TestRunner);
		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(BareEngine);
		};
		asIScriptEngine* BareControlEngine = CreateBareSdkEngine(TestRunner);
		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(BareControlEngine);
		};

		AS_NATIVE_PRODUCT("ENG-PROPERTY-PROFILE",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);
		FNoDiscardAsserter Assertions(*TestRunner);

		if (!Assertions.IsNotNull(ForkEngine.Get(), TEXT("Fork profile engine should exist"))
			|| !Assertions.IsNotNull(ForkControlEngine.Get(), TEXT("Fork profile control engine should exist"))
			|| !Assertions.IsNotNull(BareEngine, TEXT("Bare profile engine should exist"))
			|| !Assertions.IsNotNull(BareControlEngine, TEXT("Bare profile control engine should exist")))
		{
			return;
		}

		const FProfileCase Profiles[] =
		{
			{ TEXT("bare_sdk"), BareEngine, BareControlEngine },
			{ TEXT("fork_configured"), ForkEngine.Get(), ForkControlEngine.Get() },
		};

		for (const FProfileCase& Profile : Profiles)
		{
			for (const FPropertyCase& PropertyCase : PropertyCases)
			{
				const int32 PrimaryDefault = Profile.Primary->GetEngineProperty(PropertyCase.Property);
				const int32 ControlDefault = Profile.Control->GetEngineProperty(PropertyCase.Property);

				for (const int32 AppliedValue : AppliedValues)
				{
					const FNativeCaseContext Case(MakeNativeCaseId(
						"ENG-PROPERTY-PROFILE",
						{
							Profile.Name,
							ANSI_TO_TCHAR(PropertyCase.Name),
							AppliedValue == 0 ? TEXT("zero") : TEXT("one"),
						}));
					const FString ModuleName = MakeModuleName(Case);
					const FString Source = BuildSource(PropertyCase, AppliedValue);
					const FString Declaration = FString::Printf(TEXT("int Read_%hs()"), PropertyCase.Name);

					const int SetResult = Profile.Primary->SetEngineProperty(PropertyCase.Property, AppliedValue);
					const FString SetDescription = FString::Printf(
						TEXT("%s; profile=%s property=%hs applied=%d default=%d"),
						*Case.Describe(TEXT("profile primary should accept the applied property value")),
						Profile.Name,
						PropertyCase.Name,
						AppliedValue,
						PrimaryDefault);
					const bool bSetSucceeded = Assertions.AreEqual(asSUCCESS, SetResult, *SetDescription);
					if (bSetSucceeded)
					{
						(void)Assertions.AreEqual(
							AppliedValue,
							Profile.Primary->GetEngineProperty(PropertyCase.Property),
							*Case.Describe(TEXT("profile primary should read back the applied property value")));
					}
					(void)Assertions.AreEqual(
						ControlDefault,
						Profile.Control->GetEngineProperty(PropertyCase.Property),
						*Case.Describe(TEXT("independent profile control should retain its baseline property value")));

					if (bSetSucceeded)
					{
						PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);
						const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
						const FTCHARToUTF8 SourceUtf8(*Source);
						asIScriptModule* Module = nullptr;
						const int BuildResult = CompileNativeModule(
							Profile.Primary,
							ModuleNameUtf8.Get(),
							SourceUtf8.Get(),
							Module);
						const FString BuildDescription = FString::Printf(
							TEXT("%s; profile=%s result=%d"),
							*Case.Describe(TEXT("profile probe should compile")),
							Profile.Name,
							BuildResult);
						const bool bBuildSucceeded = Assertions.AreEqual(asSUCCESS, BuildResult, *BuildDescription);
						const FTCHARToUTF8 DeclarationUtf8(*Declaration);
						asIScriptFunction* const Function = bBuildSucceeded
							? GetNativeFunctionByExactDecl(Module, DeclarationUtf8.Get())
							: nullptr;
						const bool bFunctionAvailable = Assertions.IsNotNull(
							Function,
							*Case.Describe(TEXT("profile probe should publish its exact declaration")));
						if (bBuildSucceeded && bFunctionAvailable)
						{
							asIScriptContext* const Context = Profile.Primary->CreateContext();
							ON_SCOPE_EXIT
							{
								if (Context != nullptr)
								{
									Context->Release();
								}
							};
							const bool bContextAvailable = Assertions.IsNotNull(
								Context,
								*Case.Describe(TEXT("profile probe should create a context")));
							if (bContextAvailable)
							{
								const int ExecuteResult = PrepareAndExecute(Context, Function);
								(void)Assertions.AreEqual(
									asEXECUTION_FINISHED,
									ExecuteResult,
									*Case.Describe(TEXT("profile probe should execute")));
								(void)Assertions.AreEqual(
									AppliedValue + 1,
									static_cast<int32>(Context->GetReturnDWord()),
									*Case.Describe(TEXT("profile probe should return the selected value marker")));
								(void)Assertions.AreEqual(
									asSUCCESS,
									Context->Unprepare(),
									*Case.Describe(TEXT("profile probe context should unprepare")));
							}
						}
						Profile.Primary->DiscardModule(ModuleNameUtf8.Get());
						(void)Assertions.IsNull(
							Profile.Primary->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
							*Case.Describe(TEXT("profile probe module should be discarded")));
					}

					const int RestoreResult = Profile.Primary->SetEngineProperty(PropertyCase.Property, PrimaryDefault);
					(void)Assertions.AreEqual(
						asSUCCESS,
						RestoreResult,
						*Case.Describe(TEXT("profile primary should restore its baseline property value")));
					(void)Assertions.AreEqual(
						PrimaryDefault,
						Profile.Primary->GetEngineProperty(PropertyCase.Property),
						*Case.Describe(TEXT("profile primary should expose its restored baseline")));
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
