#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

using AngelscriptNativeTestSupport::AppendGeneratedAsLine;
using AngelscriptNativeTestSupport::CompileNativeModule;
using AngelscriptNativeTestSupport::FindNativeFunctionsByName;
using AngelscriptNativeTestSupport::GetNativeFunctionByExactDecl;
using AngelscriptNativeTestSupport::PrepareAndExecute;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FFunctionIndirectCallTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Functions.IndirectCalls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{

private:
	using FNativeCaseContext = AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeMessageCollector = AngelscriptNativeTestSupport::FNativeMessageCollector;
	using FNativeMessageEntry = AngelscriptNativeTestSupport::FNativeMessageEntry;
	using FNativeTestEngine = AngelscriptNativeTestSupport::FNativeTestEngine;

	struct FMechanismCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FScenarioCase
	{
		const ANSICHAR* CatalogName;
	};

	inline static constexpr FMechanismCase RegisteredFuncdefMechanism = { "registered_funcdef" };
	inline static constexpr FMechanismCase ScriptFuncdefMechanism = { "script_funcdef" };
	inline static constexpr FMechanismCase MixinMechanism = { "mixin" };
	inline static constexpr FMechanismCase ImportedMechanism = { "imported" };

	inline static constexpr FScenarioCase ScenarioCases[] =
	{
		{ "declaration_metadata" },
		{ "compatible_direct" },
		{ "compatible_nested" },
		{ "null_or_unbound" },
		{ "incompatible_signature" },
		{ "rebuild_or_rebind" },
	};

	static bool IsScenario(const FScenarioCase& Case, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(Case.CatalogName, Name) == 0;
	}

	static FString MakeSuffix(const FMechanismCase& MechanismCase, const FScenarioCase& ScenarioCase)
	{
		return FString::Printf(TEXT("%hs_%hs"), MechanismCase.CatalogName, ScenarioCase.CatalogName);
	}

	static bool HasLocatedError(const FNativeMessageCollector& Messages, const FString& Section)
	{
		return Messages.Entries.ContainsByPredicate([&Section](const FNativeMessageEntry& Entry)
		{
			return Entry.Type == asMSGTYPE_ERROR
				&& Entry.Section == Section
				&& Entry.Row > 0
				&& Entry.Column > 0
				&& !Entry.Message.IsEmpty();
		});
	}

	static bool ExecuteIntNoArgs(
		FAutomationTestBase& Test,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module,
		const ANSICHAR* Declaration,
		int32& OutResult)
	{
		AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(
			Test,
			&ScriptEngine,
			&Module,
			Declaration);
		if (!Invoker.IsValid())
		{
			return false;
		}
		OutResult = Invoker.CallAndReturn<int32>(INDEX_NONE);
		return OutResult != INDEX_NONE;
	}

	static FString BuildRegisteredTargetSource(const FString& FunctionName, const bool bNested, const int32 Marker)
	{
		FString Source;
		if (bNested)
		{
			AppendGeneratedAsLine(Source, TEXT("int IndirectHelper(int Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn Value + %d;"), Marker));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("int %s(int Value)"), *FunctionName));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, bNested
			? TEXT("\treturn IndirectHelper(Value);")
			: FString::Printf(TEXT("\treturn Value + %d;"), Marker));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static FString BuildIncompatibleRegisteredTargetSource(const FString& FunctionName)
	{
		FString Source;
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("double %s(double Value)"), *FunctionName));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	void RunRegisteredFuncdefCase(
		FAutomationTestBase& Test,
		FNativeTestEngine& Engine,
		const FMechanismCase& MechanismCase,
		const FScenarioCase& ScenarioCase,
		const FNativeCaseContext& Case)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		const FString Suffix = MakeSuffix(MechanismCase, ScenarioCase);
		const FString FuncdefName = TEXT("FCallback_") + Suffix;
		const FString FuncdefDeclaration = FString::Printf(TEXT("int %s(int Value)"), *FuncdefName);
		const asUINT BeforeCount = ScriptEngine->GetFuncdefCount();
		const int TypeId = ScriptEngine->RegisterFuncdef(TCHAR_TO_ANSI(*FuncdefDeclaration));
		ASSERT_THAT(IsTrue(TypeId >= 0,
			*Case.Describe(TEXT("registered funcdef should register a unique core signature"))));
		ASSERT_THAT(AreEqual(static_cast<int32>(BeforeCount + 1), static_cast<int32>(ScriptEngine->GetFuncdefCount()),
			*Case.Describe(TEXT("registered funcdef count should increase exactly once"))));
		asITypeInfo* const FuncdefType = ScriptEngine->GetFuncdefByIndex(BeforeCount);
		ASSERT_THAT(IsNotNull(FuncdefType,
			*Case.Describe(TEXT("registered funcdef should expose type metadata by index"))));
		asIScriptFunction* const Signature = FuncdefType != nullptr ? FuncdefType->GetFuncdefSignature() : nullptr;
		ASSERT_THAT(IsNotNull(Signature,
			*Case.Describe(TEXT("registered funcdef should expose signature metadata"))));
		if (FuncdefType == nullptr || Signature == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(FuncdefName, FString(UTF8_TO_TCHAR(FuncdefType->GetName())),
			*Case.Describe(TEXT("registered funcdef type should preserve its unique name"))));
		ASSERT_THAT(AreEqual(TypeId, ScriptEngine->GetTypeIdByDecl(TCHAR_TO_ANSI(*FuncdefName)),
			*Case.Describe(TEXT("registered funcdef type ID should round-trip by declaration"))));

		if (IsScenario(ScenarioCase, "declaration_metadata"))
		{
			ASSERT_THAT(AreEqual(1, static_cast<int32>(Signature->GetParamCount()),
				*Case.Describe(TEXT("registered funcdef signature should preserve parameter count"))));
			ASSERT_THAT(AreEqual(ScriptEngine->GetTypeIdByDecl("int"), Signature->GetReturnTypeId(),
				*Case.Describe(TEXT("registered funcdef signature should preserve return type"))));
			return;
		}

		if (IsScenario(ScenarioCase, "null_or_unbound"))
		{
			asIScriptContext* const Context = ScriptEngine->CreateContext();
			ASSERT_THAT(IsNotNull(Context,
				*Case.Describe(TEXT("registered funcdef null-target cell should create a context"))));
			if (Context != nullptr)
			{
				ASSERT_THAT(AreEqual(asNO_FUNCTION, Context->Prepare(nullptr),
					*Case.Describe(TEXT("raw context should reject a null indirect function target safely"))));
				Context->Release();
			}
			return;
		}

		const FString ModuleName = TEXT("RegisteredFuncdefTarget_") + Suffix;
		const FString FunctionName = TEXT("Target_") + Suffix;
		const bool bNested = IsScenario(ScenarioCase, "compatible_nested");
		const int32 FirstMarker = IsScenario(ScenarioCase, "rebuild_or_rebind") ? 10 : 1;
		const FString FirstSource = IsScenario(ScenarioCase, "incompatible_signature")
			? BuildIncompatibleRegisteredTargetSource(FunctionName)
			: BuildRegisteredTargetSource(FunctionName, bNested, FirstMarker);
		AngelscriptNativeTestSupport::PrintGeneratedAsSource(Test, Case.GetId(), ModuleName, FirstSource);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 FirstSourceUtf8(*FirstSource);
		asIScriptModule* TargetModule = nullptr;
		ASSERT_THAT(IsTrue(CompileNativeModule(ScriptEngine, ModuleNameUtf8.Get(), FirstSourceUtf8.Get(), TargetModule) >= 0,
			*Case.Describe(TEXT("registered funcdef compatibility target should compile"))));
		ASSERT_THAT(IsNotNull(TargetModule,
			*Case.Describe(TEXT("registered funcdef compatibility target should publish a module"))));
		if (TargetModule == nullptr)
		{
			return;
		}

		const FString FirstDeclaration = IsScenario(ScenarioCase, "incompatible_signature")
			? FString::Printf(TEXT("double %s(double)"), *FunctionName)
			: FString::Printf(TEXT("int %s(const int)"), *FunctionName);
		asIScriptFunction* Target = GetNativeFunctionByExactDecl(TargetModule, TCHAR_TO_ANSI(*FirstDeclaration));
		if (Target == nullptr && IsScenario(ScenarioCase, "incompatible_signature"))
		{
			const int DoubleTypeId = ScriptEngine->GetTypeIdByDecl("double");
			const TArray<asIScriptFunction*> Candidates = FindNativeFunctionsByName(TargetModule, TCHAR_TO_ANSI(*FunctionName));
			for (asIScriptFunction* const Candidate : Candidates)
			{
				int ParameterTypeId = 0;
				if (Candidate != nullptr && Candidate->GetParamCount() == 1
					&& Candidate->GetParam(0, &ParameterTypeId) >= 0 && ParameterTypeId == DoubleTypeId)
				{
					Target = Candidate;
					break;
				}
			}
		}
		if (Target == nullptr && IsScenario(ScenarioCase, "incompatible_signature"))
		{
			const int DoubleTypeId = ScriptEngine->GetTypeIdByDecl("double");
			const TArray<asIScriptFunction*> Candidates = FindNativeFunctionsByName(TargetModule, TCHAR_TO_ANSI(*FunctionName));
			for (asIScriptFunction* const Candidate : Candidates)
			{
				int ParameterTypeId = 0;
				if (Candidate != nullptr
					&& Candidate->GetParamCount() == 1
					&& Candidate->GetParam(0, &ParameterTypeId) >= 0
					&& ParameterTypeId == DoubleTypeId)
				{
					Target = Candidate;
					break;
				}
			}
		}
		ASSERT_THAT(IsNotNull(Target,
			*Case.Describe(TEXT("registered funcdef target should resolve by exact declaration"))));
		if (Target != nullptr)
		{
			const bool bExpectedCompatible = !IsScenario(ScenarioCase, "incompatible_signature");
			ASSERT_THAT(AreEqual(bExpectedCompatible, Target->IsCompatibleWithTypeId(TypeId),
				*Case.Describe(TEXT("registered funcdef compatibility should match the exact target signature"))));
		}

		if (Target != nullptr && !IsScenario(ScenarioCase, "incompatible_signature"))
		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(
				Test,
				ScriptEngine,
				TargetModule,
				TCHAR_TO_ANSI(*FirstDeclaration));
			ASSERT_THAT(IsTrue(Invoker.IsValid(),
				*Case.Describe(TEXT("compatible funcdef target should create an exact raw invoker"))));
			if (Invoker.IsValid())
			{
				Invoker.AddArg(static_cast<int32>(41));
				ASSERT_THAT(AreEqual(41 + FirstMarker, Invoker.CallAndReturn<int32>(INDEX_NONE),
					*Case.Describe(TEXT("compatible funcdef target should execute direct or nested behavior"))));
			}
		}

		ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
		if (IsScenario(ScenarioCase, "rebuild_or_rebind"))
		{
			const FString RebuiltSource = BuildRegisteredTargetSource(FunctionName, false, 20);
			AngelscriptNativeTestSupport::PrintGeneratedAsSource(
				Test,
				Case.GetId() + TEXT("-REBUILD"),
				ModuleName,
				RebuiltSource);
			const FTCHARToUTF8 RebuiltSourceUtf8(*RebuiltSource);
			TargetModule = nullptr;
			ASSERT_THAT(IsTrue(CompileNativeModule(ScriptEngine, ModuleNameUtf8.Get(), RebuiltSourceUtf8.Get(), TargetModule) >= 0,
				*Case.Describe(TEXT("registered funcdef compatible target should rebuild"))));
			Target = GetNativeFunctionByExactDecl(TargetModule, TCHAR_TO_ANSI(*FirstDeclaration));
			ASSERT_THAT(IsNotNull(Target,
				*Case.Describe(TEXT("rebuilt funcdef target should resolve by exact declaration"))));
			if (Target != nullptr)
			{
				ASSERT_THAT(IsTrue(Target->IsCompatibleWithTypeId(TypeId),
					*Case.Describe(TEXT("funcdef compatibility should survive module rebuild"))));
				AngelscriptSDKTestSupport::FSdkFunctionInvoker RebuiltInvoker(
					Test,
					ScriptEngine,
					TargetModule,
					TCHAR_TO_ANSI(*FirstDeclaration));
				ASSERT_THAT(IsTrue(RebuiltInvoker.IsValid(),
					*Case.Describe(TEXT("rebuilt funcdef target should create an invoker"))));
				if (RebuiltInvoker.IsValid())
				{
					RebuiltInvoker.AddArg(static_cast<int32>(41));
					ASSERT_THAT(AreEqual(61, RebuiltInvoker.CallAndReturn<int32>(INDEX_NONE),
						*Case.Describe(TEXT("rebuilt compatible target should expose changed implementation"))));
				}
			}
			ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
		}
		ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("registered funcdef target module should be discarded"))));
	}

	static FString BuildScriptFuncdefSource(const FScenarioCase& ScenarioCase, const FString& Suffix)
	{
		const FString FuncdefName = TEXT("FScriptCallback_") + Suffix;
		FString Source;
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("funcdef int %s(int Value);"), *FuncdefName));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int Target(int Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, IsScenario(ScenarioCase, "compatible_nested")
			? TEXT("\treturn Value + 1;")
			: TEXT("\treturn Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		if (!IsScenario(ScenarioCase, "declaration_metadata"))
		{
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int Run()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, IsScenario(ScenarioCase, "null_or_unbound")
				? TEXT("\treturn 0;")
				: TEXT("\treturn Target(42);"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		AppendGeneratedAsLine(Source);
		return Source;
	}

	void RunScriptFuncdefCase(
		FAutomationTestBase& Test,
		FNativeTestEngine& Engine,
		const FMechanismCase& MechanismCase,
		const FScenarioCase& ScenarioCase,
		const FNativeCaseContext& Case)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		const FString Suffix = MakeSuffix(MechanismCase, ScenarioCase);
		const FString ModuleName = TEXT("ScriptFuncdef_") + Suffix;
		const FString Source = BuildScriptFuncdefSource(ScenarioCase, Suffix);
		AngelscriptNativeTestSupport::PrintGeneratedAsSource(Test, Case.GetId(), ModuleName, Source);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		Engine.ResetMessages();
		asIScriptModule* Module = nullptr;
		const int BuildResult = CompileNativeModule(ScriptEngine, ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
		ASSERT_THAT(IsTrue(BuildResult < 0,
			*Case.Describe(TEXT("script-level funcdef should preserve the current fork parser rejection"))));
		ASSERT_THAT(IsTrue(HasLocatedError(Engine.GetMessages(), ModuleName),
			*Case.Describe(TEXT("script-level funcdef rejection should report a located diagnostic"))));
		ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("rejected script funcdef should leave no retained module"))));
	}

	static FString BuildMixinSource(const FScenarioCase& ScenarioCase, const int32 Marker)
	{
		FString Source;
		AppendGeneratedAsLine(Source, TEXT("struct FMixinReceiver"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 40;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("mixin int AddValue(FMixinReceiver& Self, int Delta)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn Self.Value + Delta + %d;"), Marker));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		if (IsScenario(ScenarioCase, "compatible_nested"))
		{
			AppendGeneratedAsLine(Source, TEXT("int InvokeMixin(FMixinReceiver& Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value.AddValue(2);"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		AppendGeneratedAsLine(Source, TEXT("int Run()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (IsScenario(ScenarioCase, "null_or_unbound"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFMixinReceiver Value;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value.MissingMixin(2);"));
		}
		else if (IsScenario(ScenarioCase, "incompatible_signature"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tint Value = 40;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value.AddValue(2);"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\tFMixinReceiver Value;"));
			AppendGeneratedAsLine(Source, IsScenario(ScenarioCase, "compatible_nested")
				? TEXT("\treturn InvokeMixin(Value);")
				: TEXT("\treturn Value.AddValue(2);"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	void RunMixinCase(
		FAutomationTestBase& Test,
		FNativeTestEngine& Engine,
		const FMechanismCase& MechanismCase,
		const FScenarioCase& ScenarioCase,
		const FNativeCaseContext& Case)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		const FString Suffix = MakeSuffix(MechanismCase, ScenarioCase);
		const FString ModuleName = TEXT("MixinIndirect_") + Suffix;
		const FString Source = BuildMixinSource(ScenarioCase, 0);
		AngelscriptNativeTestSupport::PrintGeneratedAsSource(Test, Case.GetId(), ModuleName, Source);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		Engine.ResetMessages();
		asIScriptModule* Module = nullptr;
		const int BuildResult = CompileNativeModule(ScriptEngine, ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
		const bool bShouldCompile = !IsScenario(ScenarioCase, "null_or_unbound")
			&& !IsScenario(ScenarioCase, "incompatible_signature");
		if (!bShouldCompile)
		{
			ASSERT_THAT(IsTrue(BuildResult < 0,
				*Case.Describe(TEXT("missing or incompatible mixin receiver should fail compilation"))));
			ASSERT_THAT(IsTrue(HasLocatedError(Engine.GetMessages(), ModuleName),
				*Case.Describe(TEXT("mixin resolution failure should report a located diagnostic"))));
			ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
			return;
		}

		ASSERT_THAT(IsTrue(BuildResult >= 0,
			*Case.Describe(TEXT("compatible mixin cell should compile"))));
		ASSERT_THAT(IsNotNull(Module,
			*Case.Describe(TEXT("compatible mixin cell should publish a module"))));
		if (Module != nullptr)
		{
			const TArray<asIScriptFunction*> Mixins = FindNativeFunctionsByName(Module, "AddValue");
			ASSERT_THAT(AreEqual(1, Mixins.Num(),
				*Case.Describe(TEXT("mixin declaration should publish one stable function identity"))));
			int32 Result = INDEX_NONE;
			ASSERT_THAT(IsTrue(ExecuteIntNoArgs(Test, *ScriptEngine, *Module, "int Run()", Result),
				*Case.Describe(TEXT("compatible mixin entry should execute"))));
			ASSERT_THAT(AreEqual(42, Result,
				*Case.Describe(TEXT("direct or nested mixin dispatch should preserve receiver and arguments"))));
		}
		ScriptEngine->DiscardModule(ModuleNameUtf8.Get());

		if (IsScenario(ScenarioCase, "rebuild_or_rebind"))
		{
			const FString RebuiltSource = BuildMixinSource(ScenarioCase, 10);
			AngelscriptNativeTestSupport::PrintGeneratedAsSource(
				Test,
				Case.GetId() + TEXT("-REBUILD"),
				ModuleName,
				RebuiltSource);
			const FTCHARToUTF8 RebuiltSourceUtf8(*RebuiltSource);
			Module = nullptr;
			ASSERT_THAT(IsTrue(CompileNativeModule(ScriptEngine, ModuleNameUtf8.Get(), RebuiltSourceUtf8.Get(), Module) >= 0,
				*Case.Describe(TEXT("mixin module should rebuild with changed implementation"))));
			if (Module != nullptr)
			{
				int32 Result = INDEX_NONE;
				ASSERT_THAT(IsTrue(ExecuteIntNoArgs(Test, *ScriptEngine, *Module, "int Run()", Result),
					*Case.Describe(TEXT("rebuilt mixin entry should execute"))));
				ASSERT_THAT(AreEqual(52, Result,
					*Case.Describe(TEXT("rebuilt mixin should expose the new implementation"))));
			}
			ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
		}
		ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("mixin cell should discard its module"))));
	}

	static FString BuildImportProviderSource(const bool bNested, const int32 Marker, const bool bIncompatible)
	{
		FString Source;
		if (bNested)
		{
			AppendGeneratedAsLine(Source, TEXT("int ProviderHelper()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn %d;"), Marker));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		AppendGeneratedAsLine(Source, bIncompatible ? TEXT("int SharedValue(int Value)") : TEXT("int SharedValue()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, bIncompatible
			? TEXT("\treturn Value;")
			: bNested
				? TEXT("\treturn ProviderHelper();")
				: FString::Printf(TEXT("\treturn %d;"), Marker));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static FString BuildImportConsumerSource(const FString& ProviderName, const bool bNested)
	{
		FString Source;
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("import int SharedValue() from \"%s\";"), *ProviderName));
		AppendGeneratedAsLine(Source);
		if (bNested)
		{
			AppendGeneratedAsLine(Source, TEXT("int InvokeImport()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn SharedValue();"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		AppendGeneratedAsLine(Source, TEXT("int Run()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, bNested ? TEXT("\treturn InvokeImport();") : TEXT("\treturn SharedValue();"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	void RunImportedCase(
		FAutomationTestBase& Test,
		FNativeTestEngine& Engine,
		const FMechanismCase& MechanismCase,
		const FScenarioCase& ScenarioCase,
		const FNativeCaseContext& Case)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		const FString Suffix = MakeSuffix(MechanismCase, ScenarioCase);
		const FString ProviderName = TEXT("ImportProvider_") + Suffix;
		const FString ProviderBName = TEXT("ImportProviderB_") + Suffix;
		const FString ConsumerName = TEXT("ImportConsumer_") + Suffix;
		const FTCHARToUTF8 ProviderNameUtf8(*ProviderName);
		const FTCHARToUTF8 ProviderBNameUtf8(*ProviderBName);
		const FTCHARToUTF8 ConsumerNameUtf8(*ConsumerName);
		const bool bNested = IsScenario(ScenarioCase, "compatible_nested");
		const bool bIncompatible = IsScenario(ScenarioCase, "incompatible_signature");

		asIScriptModule* Provider = nullptr;
		if (!IsScenario(ScenarioCase, "declaration_metadata") && !IsScenario(ScenarioCase, "null_or_unbound"))
		{
			const FString ProviderSource = BuildImportProviderSource(bNested, 42, bIncompatible);
			AngelscriptNativeTestSupport::PrintGeneratedAsSource(
				Test,
				Case.GetId() + TEXT("-PROVIDER"),
				ProviderName,
				ProviderSource);
			const FTCHARToUTF8 ProviderSourceUtf8(*ProviderSource);
			ASSERT_THAT(IsTrue(CompileNativeModule(ScriptEngine, ProviderNameUtf8.Get(), ProviderSourceUtf8.Get(), Provider) >= 0,
				*Case.Describe(TEXT("import provider should compile"))));
		}

		const FString ConsumerSource = BuildImportConsumerSource(ProviderName, bNested);
		AngelscriptNativeTestSupport::PrintGeneratedAsSource(
			Test,
			Case.GetId() + TEXT("-CONSUMER"),
			ConsumerName,
			ConsumerSource);
		const FTCHARToUTF8 ConsumerSourceUtf8(*ConsumerSource);
		asIScriptModule* Consumer = nullptr;
		ASSERT_THAT(IsTrue(CompileNativeModule(ScriptEngine, ConsumerNameUtf8.Get(), ConsumerSourceUtf8.Get(), Consumer) >= 0,
			*Case.Describe(TEXT("import consumer should compile before binding"))));
		ASSERT_THAT(IsNotNull(Consumer,
			*Case.Describe(TEXT("import consumer should publish a module"))));
		if (Consumer == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Consumer->GetImportedFunctionCount()),
			*Case.Describe(TEXT("import consumer should expose one import"))));
		ASSERT_THAT(AreEqual(0, Consumer->GetImportedFunctionIndexByDecl("int SharedValue()"),
			*Case.Describe(TEXT("import declaration should resolve to its exact index"))));
		ASSERT_THAT(AreEqual(ProviderName, FString(UTF8_TO_TCHAR(Consumer->GetImportedFunctionSourceModule(0))),
			*Case.Describe(TEXT("import metadata should preserve the provider module name"))));

		if (IsScenario(ScenarioCase, "declaration_metadata"))
		{
			ASSERT_THAT(AreEqual(FString(TEXT("int SharedValue()")), FString(UTF8_TO_TCHAR(Consumer->GetImportedFunctionDeclaration(0))),
				*Case.Describe(TEXT("import metadata should preserve the exact declaration"))));
		}
		else if (IsScenario(ScenarioCase, "null_or_unbound"))
		{
			asIScriptFunction* const Entry = GetNativeFunctionByExactDecl(Consumer, "int Run()");
			asIScriptContext* const Context = ScriptEngine->CreateContext();
			ASSERT_THAT(IsNotNull(Entry,
				*Case.Describe(TEXT("unbound import entry should resolve exactly"))));
			ASSERT_THAT(IsNotNull(Context,
				*Case.Describe(TEXT("unbound import should create a context"))));
			if (Entry != nullptr && Context != nullptr)
			{
				ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), PrepareAndExecute(Context, Entry),
					*Case.Describe(TEXT("calling an unbound import should raise an execution exception"))));
				ASSERT_THAT(IsTrue(Context->GetExceptionString() != nullptr && Context->GetExceptionString()[0] != '\0',
					*Case.Describe(TEXT("unbound import should expose exception text"))));
			}
			if (Context != nullptr)
			{
				Context->Release();
			}
		}
		else if (bIncompatible)
		{
			asIScriptFunction* const SourceFunction = GetNativeFunctionByExactDecl(Provider, "int SharedValue(const int)");
			ASSERT_THAT(IsNotNull(SourceFunction,
				*Case.Describe(TEXT("incompatible import provider should expose its exact function"))));
			ASSERT_THAT(AreEqual(static_cast<int32>(asINVALID_INTERFACE), Consumer->BindImportedFunction(0, SourceFunction),
				*Case.Describe(TEXT("import binding should reject an incompatible signature"))));
		}
		else if (IsScenario(ScenarioCase, "rebuild_or_rebind"))
		{
			const FString ProviderBSource = BuildImportProviderSource(false, 29, false);
			AngelscriptNativeTestSupport::PrintGeneratedAsSource(
				Test,
				Case.GetId() + TEXT("-REPLACEMENT-PROVIDER"),
				ProviderBName,
				ProviderBSource);
			const FTCHARToUTF8 ProviderBSourceUtf8(*ProviderBSource);
			asIScriptModule* ProviderB = nullptr;
			ASSERT_THAT(IsTrue(CompileNativeModule(ScriptEngine, ProviderBNameUtf8.Get(), ProviderBSourceUtf8.Get(), ProviderB) >= 0,
				*Case.Describe(TEXT("second import provider should compile"))));
			asIScriptFunction* const FirstFunction = GetNativeFunctionByExactDecl(Provider, "int SharedValue()");
			asIScriptFunction* const SecondFunction = GetNativeFunctionByExactDecl(ProviderB, "int SharedValue()");
			ASSERT_THAT(IsNotNull(FirstFunction,
				*Case.Describe(TEXT("first import provider should expose its exact function"))));
			ASSERT_THAT(IsNotNull(SecondFunction,
				*Case.Describe(TEXT("second import provider should expose its exact function"))));
			if (FirstFunction != nullptr && SecondFunction != nullptr)
			{
				ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Consumer->BindImportedFunction(0, FirstFunction),
					*Case.Describe(TEXT("consumer should bind the first provider"))));
				int32 FirstResult = INDEX_NONE;
				ASSERT_THAT(IsTrue(ExecuteIntNoArgs(Test, *ScriptEngine, *Consumer, "int Run()", FirstResult),
					*Case.Describe(TEXT("consumer should execute the first provider"))));
				ASSERT_THAT(AreEqual(42, FirstResult,
					*Case.Describe(TEXT("first import binding should preserve its implementation"))));
				ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Consumer->UnbindImportedFunction(0),
					*Case.Describe(TEXT("consumer should unbind the first provider"))));
				ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Consumer->BindImportedFunction(0, SecondFunction),
					*Case.Describe(TEXT("consumer should bind the replacement provider"))));
				int32 SecondResult = INDEX_NONE;
				ASSERT_THAT(IsTrue(ExecuteIntNoArgs(Test, *ScriptEngine, *Consumer, "int Run()", SecondResult),
					*Case.Describe(TEXT("consumer should execute the replacement provider"))));
				ASSERT_THAT(AreEqual(29, SecondResult,
					*Case.Describe(TEXT("rebound import should expose replacement behavior"))));
			}
			ScriptEngine->DiscardModule(ProviderBNameUtf8.Get());
		}
		else
		{
			asIScriptFunction* const SourceFunction = GetNativeFunctionByExactDecl(Provider, "int SharedValue()");
			ASSERT_THAT(IsNotNull(SourceFunction,
				*Case.Describe(TEXT("compatible import provider should expose its exact function"))));
			if (SourceFunction != nullptr)
			{
				ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Consumer->BindImportedFunction(0, SourceFunction),
					*Case.Describe(TEXT("compatible import should bind"))));
				int32 Result = INDEX_NONE;
				ASSERT_THAT(IsTrue(ExecuteIntNoArgs(Test, *ScriptEngine, *Consumer, "int Run()", Result),
					*Case.Describe(TEXT("compatible import should execute direct or nested call"))));
				ASSERT_THAT(AreEqual(42, Result,
					*Case.Describe(TEXT("compatible import should preserve provider result"))));
			}
		}

		ScriptEngine->DiscardModule(ConsumerNameUtf8.Get());
		ScriptEngine->DiscardModule(ProviderNameUtf8.Get());
		ASSERT_THAT(IsNull(ScriptEngine->GetModule(ConsumerNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("import consumer should be discarded"))));
		ASSERT_THAT(IsNull(ScriptEngine->GetModule(ProviderNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("import provider should be discarded"))));
	}

public:
	TEST_METHOD(RegisteredFuncdefScenarios)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-FN-INDIRECT-REGISTERED-FUNCDEF",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup);

		AngelscriptNativeTestSupport::FNativeTestEngine NativeEngine;
		NativeEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			NativeEngine.Destroy();
		};

		ASSERT_THAT(IsNotNull(NativeEngine.Get(), TEXT("Registered funcdef scenarios should create a raw SDK engine")));
		if (NativeEngine.Get() == nullptr)
		{
			return;
		}

		for (const FScenarioCase& ScenarioCase : ScenarioCases)
		{
			const FNativeCaseContext Case(MakeNativeCaseId(
				"LANG-FN-INDIRECT-REGISTERED-FUNCDEF",
				{ ANSI_TO_TCHAR(ScenarioCase.CatalogName) }));
			NativeEngine.ResetMessages();
			RunRegisteredFuncdefCase(
				*TestRunner,
				NativeEngine,
				RegisteredFuncdefMechanism,
				ScenarioCase,
				Case);
		}
	}

	TEST_METHOD(ScriptFuncdefBoundaryScenarios)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-FN-INDIRECT-SCRIPT-FUNCDEF",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup);

		AngelscriptNativeTestSupport::FNativeTestEngine NativeEngine;
		NativeEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			NativeEngine.Destroy();
		};

		ASSERT_THAT(IsNotNull(NativeEngine.Get(), TEXT("Script funcdef boundary scenarios should create a raw SDK engine")));
		if (NativeEngine.Get() == nullptr)
		{
			return;
		}

		for (const FScenarioCase& ScenarioCase : ScenarioCases)
		{
			const FNativeCaseContext Case(MakeNativeCaseId(
				"LANG-FN-INDIRECT-SCRIPT-FUNCDEF",
				{ ANSI_TO_TCHAR(ScenarioCase.CatalogName) }));
			NativeEngine.ResetMessages();
			RunScriptFuncdefCase(
				*TestRunner,
				NativeEngine,
				ScriptFuncdefMechanism,
				ScenarioCase,
				Case);
		}
	}

	TEST_METHOD(MixinScenarios)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-FN-INDIRECT-MIXIN",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup);

		AngelscriptNativeTestSupport::FNativeTestEngine NativeEngine;
		NativeEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			NativeEngine.Destroy();
		};

		ASSERT_THAT(IsNotNull(NativeEngine.Get(), TEXT("Mixin scenarios should create a raw SDK engine")));
		if (NativeEngine.Get() == nullptr)
		{
			return;
		}

		for (const FScenarioCase& ScenarioCase : ScenarioCases)
		{
			const FNativeCaseContext Case(MakeNativeCaseId(
				"LANG-FN-INDIRECT-MIXIN",
				{ ANSI_TO_TCHAR(ScenarioCase.CatalogName) }));
			NativeEngine.ResetMessages();
			RunMixinCase(
				*TestRunner,
				NativeEngine,
				MixinMechanism,
				ScenarioCase,
				Case);
		}
	}

	TEST_METHOD(ImportedRebindScenarios)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-FN-INDIRECT-IMPORTED",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup);

		AngelscriptNativeTestSupport::FNativeTestEngine NativeEngine;
		NativeEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			NativeEngine.Destroy();
		};

		ASSERT_THAT(IsNotNull(NativeEngine.Get(), TEXT("Imported function scenarios should create a raw SDK engine")));
		if (NativeEngine.Get() == nullptr)
		{
			return;
		}

		for (const FScenarioCase& ScenarioCase : ScenarioCases)
		{
			const FNativeCaseContext Case(MakeNativeCaseId(
				"LANG-FN-INDIRECT-IMPORTED",
				{ ANSI_TO_TCHAR(ScenarioCase.CatalogName) }));
			NativeEngine.ResetMessages();
			RunImportedCase(
				*TestRunner,
				NativeEngine,
				ImportedMechanism,
				ScenarioCase,
				Case);
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
