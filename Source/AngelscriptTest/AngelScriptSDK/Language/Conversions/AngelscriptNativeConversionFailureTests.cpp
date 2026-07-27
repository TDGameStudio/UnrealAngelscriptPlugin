#include "AngelscriptNativeConversionTestSupport.h"

#include "CQTest.h"

using AngelscriptNativeTestSupport::CompileNativeModule;
using AngelscriptNativeTestSupport::FNativeMessageCollector;
using AngelscriptNativeTestSupport::FNativeMessageEntry;
using AngelscriptNativeTestSupport::FNativeTestEngine;
using AngelscriptNativeTestSupport::PrintGeneratedAsSource;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FConversionFailureTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Conversions.Failures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:

	struct FFailureCase
	{
		const ANSICHAR* CatalogName;
		bool bRuntimeFailure;
		const TCHAR* ExpectedException;
	};

	struct FRecoveryCase
	{
		const ANSICHAR* CatalogName;
	};

	inline static constexpr FFailureCase FailureCases[] =
	{
		{ "implicit_narrowing", false, nullptr },
		{ "numeric_to_enum", false, nullptr },
		{ "unrelated_reference", false, nullptr },
		{ "bad_downcast", false, nullptr },
		{ "null_value_target", true, TEXT("Null pointer access") },
		{ "ambiguous_constructor", false, nullptr },
		{ "ambiguous_operator", false, nullptr },
		{ "explicit_only_implicit_use", false, nullptr },
		{ "conversion_exception", true, TEXT("Divide by zero") },
		{ "constructor_exception", true, TEXT("Divide by zero") },
		{ "abi_mismatch", false, nullptr },
		{ "conditional_no_common_type", false, nullptr },
	};

	inline static constexpr FRecoveryCase RecoveryCases[] =
	{
		{ "fresh_module" },
		{ "same_module_or_context" },
	};

	static FString BuildFailureSource(const FFailureCase& FailureCase)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		if (EqualAnsi(FailureCase.CatalogName, "implicit_narrowing")
			|| EqualAnsi(FailureCase.CatalogName, "numeric_to_enum"))
		{
			AppendGeneratedAsLine(Source, TEXT("enum EConversionFailure"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tZero = 0,"));
			AppendGeneratedAsLine(Source, TEXT("\tOne = 1"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int RequireFailureEnum(EConversionFailure Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn int(Value);"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (EqualAnsi(FailureCase.CatalogName, "unrelated_reference"))
		{
			AppendGeneratedAsLine(Source, TEXT("class FUnrelatedConversionLeft"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("class FUnrelatedConversionRight"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int RequireUnrelatedReference(FUnrelatedConversionRight Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 1;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (EqualAnsi(FailureCase.CatalogName, "bad_downcast"))
		{
			AppendGeneratedAsLine(Source, TEXT("class FConversionBase"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("class FConversionDerived : FConversionBase"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int RequireDerivedReference(FConversionDerived Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 1;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (EqualAnsi(FailureCase.CatalogName, "ambiguous_constructor"))
		{
			AppendGeneratedAsLine(Source, TEXT("int SelectAmbiguousConstructor(int64 Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 1;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int SelectAmbiguousConstructor(uint64 Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 2;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (EqualAnsi(FailureCase.CatalogName, "ambiguous_operator"))
		{
			AppendGeneratedAsLine(Source, TEXT("struct FAmbiguousConversionOperator"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint opAdd(int64 Value) const"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 1;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tint opAdd(uint64 Value) const"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 2;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (EqualAnsi(FailureCase.CatalogName, "explicit_only_implicit_use"))
		{
			AppendGeneratedAsLine(Source, TEXT("struct FExplicitOnlyConversion"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value;"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFExplicitOnlyConversion(int InValue)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tValue = InValue;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tint opConv() const"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Value;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (EqualAnsi(FailureCase.CatalogName, "conversion_exception"))
		{
			AppendGeneratedAsLine(Source, TEXT("struct FThrowingConversion"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value;"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFThrowingConversion(int InValue)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tValue = InValue;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tint opImplConv() const"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tint Zero = 0;"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Value / Zero;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (EqualAnsi(FailureCase.CatalogName, "constructor_exception"))
		{
			AppendGeneratedAsLine(Source, TEXT("struct FThrowingConversionConstructor"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value;"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFThrowingConversionConstructor(int InValue)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tint Zero = 0;"));
			AppendGeneratedAsLine(Source, TEXT("\t\tValue = InValue / Zero;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (EqualAnsi(FailureCase.CatalogName, "null_value_target"))
		{
			AppendGeneratedAsLine(Source, TEXT("class FNullConversionTarget"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (EqualAnsi(FailureCase.CatalogName, "abi_mismatch")
			|| EqualAnsi(FailureCase.CatalogName, "conditional_no_common_type"))
		{
			AppendGeneratedAsLine(Source, TEXT("struct FConditionalConversionValue"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		AppendGeneratedAsLine(Source, TEXT("int RunConversionFailure()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (EqualAnsi(FailureCase.CatalogName, "implicit_narrowing"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tfloat SourceValue = 1.5;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn RequireFailureEnum(SourceValue); // CONVERSION_CAUSE"));
		}
		else if (EqualAnsi(FailureCase.CatalogName, "numeric_to_enum"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tint SourceValue = 1;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn RequireFailureEnum(SourceValue); // CONVERSION_CAUSE"));
		}
		else if (EqualAnsi(FailureCase.CatalogName, "unrelated_reference"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFUnrelatedConversionLeft SourceValue = nullptr;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn RequireUnrelatedReference(SourceValue); // CONVERSION_CAUSE"));
		}
		else if (EqualAnsi(FailureCase.CatalogName, "bad_downcast"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFConversionBase SourceValue = nullptr;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn RequireDerivedReference(SourceValue); // CONVERSION_CAUSE"));
		}
		else if (EqualAnsi(FailureCase.CatalogName, "null_value_target"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFNullConversionTarget Target = nullptr;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Target.Value; // CONVERSION_CAUSE"));
		}
		else if (EqualAnsi(FailureCase.CatalogName, "ambiguous_constructor"))
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn SelectAmbiguousConstructor(int8(1)); // CONVERSION_CAUSE"));
		}
		else if (EqualAnsi(FailureCase.CatalogName, "ambiguous_operator"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFAmbiguousConversionOperator SourceValue;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn SourceValue + int8(1); // CONVERSION_CAUSE"));
		}
		else if (EqualAnsi(FailureCase.CatalogName, "explicit_only_implicit_use"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFExplicitOnlyConversion SourceValue(9);"));
			AppendGeneratedAsLine(Source, TEXT("\tint ConvertedValue = SourceValue; // CONVERSION_CAUSE"));
			AppendGeneratedAsLine(Source, TEXT("\treturn ConvertedValue;"));
		}
		else if (EqualAnsi(FailureCase.CatalogName, "conversion_exception"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFThrowingConversion SourceValue(9);"));
			AppendGeneratedAsLine(Source, TEXT("\tint ConvertedValue = SourceValue; // CONVERSION_CAUSE"));
			AppendGeneratedAsLine(Source, TEXT("\treturn ConvertedValue;"));
		}
		else if (EqualAnsi(FailureCase.CatalogName, "constructor_exception"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFThrowingConversionConstructor SourceValue(9); // CONVERSION_CAUSE"));
			AppendGeneratedAsLine(Source, TEXT("\treturn SourceValue.Value;"));
		}
		else if (EqualAnsi(FailureCase.CatalogName, "abi_mismatch"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tdouble SourceValue = 1.5;"));
			AppendGeneratedAsLine(Source, TEXT("\tFConditionalConversionValue ConvertedValue = SourceValue; // CONVERSION_CAUSE"));
			AppendGeneratedAsLine(Source, TEXT("\treturn ConvertedValue.Value;"));
		}
		else if (EqualAnsi(FailureCase.CatalogName, "conditional_no_common_type"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tbool bUseNumber = true;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn bUseNumber ? 1 : FConditionalConversionValue(); // CONVERSION_CAUSE"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static FString BuildRecoverySource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int RunConversionRecovery()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 77;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static bool HasDiagnosticAtConversionCause(
		const FNativeMessageCollector& Messages,
		const FString& ModuleName,
		const FString& Source)
	{
		const int32 CauseOffset = Source.Find(TEXT("CONVERSION_CAUSE"));
		if (CauseOffset == INDEX_NONE)
		{
			return false;
		}

		int32 CauseRow = 1;
		for (int32 CharacterIndex = 0; CharacterIndex < CauseOffset; ++CharacterIndex)
		{
			if (Source[CharacterIndex] == TEXT('\n'))
			{
				++CauseRow;
			}
		}
		return Messages.Entries.ContainsByPredicate([&ModuleName, CauseRow](const FNativeMessageEntry& Entry)
		{
			return Entry.Type == asMSGTYPE_ERROR
				&& Entry.Section == ModuleName
				&& Entry.Row == CauseRow
				&& Entry.Column > 0;
		});
	}

	static bool HasAnyError(const FNativeMessageCollector& Messages)
	{
		for (const FNativeMessageEntry& Entry : Messages.Entries)
		{
			if (Entry.Type == asMSGTYPE_ERROR)
			{
				return true;
			}
		}
		return false;
	}

	static int CompileAndReport(
		FNativeTestEngine& Engine,
		FAutomationTestBase& Test,
		const FString& SourceId,
		const FString& ModuleName,
		const FString& Source,
		asIScriptModule*& OutModule)
	{
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		Engine.Reset(Test);
		PrintGeneratedAsSource(Test, SourceId, ModuleName, Source);
		return CompileNativeModule(Engine.Get(), ModuleNameUtf8.Get(), SourceUtf8.Get(), OutModule);
	}

public:
	TEST_METHOD(FailuresByRecovery)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-CONV-FAILURE",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		AngelscriptNativeTestSupport::FNativeTestEngine NativeEngine;
		NativeEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			NativeEngine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = NativeEngine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Conversion failure product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		for (const FFailureCase& FailureCase : FailureCases)
		{
			for (const FRecoveryCase& RecoveryCase : RecoveryCases)
			{
				const FNativeCaseContext Case(MakeNativeCaseId(
					"LANG-CONV-FAILURE",
					{ ANSI_TO_TCHAR(FailureCase.CatalogName), ANSI_TO_TCHAR(RecoveryCase.CatalogName) }));
				const FString FailureModuleName = TEXT("ConversionFailure_") + Case.GetId();
				const FString FailureSource = BuildFailureSource(FailureCase);
				asIScriptModule* FailureModule = nullptr;
				const int BuildResult = CompileAndReport(
						NativeEngine,
					*TestRunner,
					Case.GetId(),
					FailureModuleName,
					FailureSource,
					FailureModule);
				const FTCHARToUTF8 FailureModuleNameUtf8(*FailureModuleName);

				if (FailureCase.bRuntimeFailure)
				{
					ASSERT_THAT(IsTrue(BuildResult >= 0,
						*Case.Describe(TEXT("runtime conversion failure should compile its causal source"))));
					asIScriptFunction* const Entry = FindNoArgumentEntry(FailureModule, TEXT("int"), TEXT("RunConversionFailure"));
					ASSERT_THAT(IsNotNull(Entry,
						*Case.Describe(TEXT("runtime conversion failure should publish an exact entry declaration"))));
					if (Entry != nullptr)
					{
						asIScriptContext* const Context = ScriptEngine->CreateContext();
						ASSERT_THAT(IsNotNull(Context,
							*Case.Describe(TEXT("runtime conversion failure should create a context for exception inspection"))));
						if (Context != nullptr)
						{
							ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION),
								PrepareAndExecute(Context, Entry),
								*Case.Describe(TEXT("runtime conversion failure should expose its exact execution boundary"))));
							ASSERT_THAT(AreEqual(FString(FailureCase.ExpectedException),
								FString(UTF8_TO_TCHAR(Context->GetExceptionString())),
								*Case.Describe(TEXT("runtime conversion failure should retain its causal exception text"))));
							Context->Release();
						}
					}
				}
				else
				{
					const bool bPrefersSignedIntegerOverUnsignedTie = EqualAnsi(
						FailureCase.CatalogName,
						"ambiguous_constructor")
						|| EqualAnsi(FailureCase.CatalogName, "ambiguous_operator");
					if (bPrefersSignedIntegerOverUnsignedTie)
					{
						ASSERT_THAT(IsTrue(BuildResult >= 0,
							*Case.Describe(TEXT("signed integer conversion should select the higher-priority int64 overload"))));
						ASSERT_THAT(IsFalse(HasAnyError(NativeEngine.GetMessages()),
							*Case.Describe(TEXT("signed integer conversion selection should emit no compiler error"))));
						asIScriptFunction* const Entry = FindNoArgumentEntry(
							FailureModule,
							TEXT("int"),
							TEXT("RunConversionFailure"));
						ASSERT_THAT(IsNotNull(Entry,
							*Case.Describe(TEXT("signed integer conversion selection should publish its exact entry"))));
						if (Entry != nullptr)
						{
							asIScriptContext* const Context = ScriptEngine->CreateContext();
							ASSERT_THAT(IsNotNull(Context,
								*Case.Describe(TEXT("signed integer conversion selection should create a raw context"))));
							if (Context != nullptr)
							{
								ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
									PrepareAndExecute(Context, Entry),
									*Case.Describe(TEXT("signed integer conversion selection should execute to completion"))));
								ASSERT_THAT(AreEqual(1,
									static_cast<int32>(Context->GetReturnDWord()),
									*Case.Describe(TEXT("signed integer conversion selection should choose the int64 marker"))));
								Context->Release();
							}
						}
					}
					else
					{
						ASSERT_THAT(IsTrue(BuildResult < 0,
							*Case.Describe(TEXT("compile conversion failure should not publish a callable"))));
						ASSERT_THAT(IsTrue(HasOwnedLocatedDiagnostic(NativeEngine.GetMessages(), FailureModuleName),
							*Case.Describe(TEXT("compile conversion failure should own one located diagnostic"))));
						const bool bDiagnosticAtConversionCause = HasDiagnosticAtConversionCause(
							NativeEngine.GetMessages(),
							FailureModuleName,
							FailureSource);
						if (!bDiagnosticAtConversionCause)
						{
							TestRunner->AddInfo(*Case.Describe(*FString::Printf(
								TEXT("conversion failure diagnostic trace: %s"),
							*CollectMessages(NativeEngine.GetMessages()))));
						}
						ASSERT_THAT(IsTrue(bDiagnosticAtConversionCause,
							*Case.Describe(TEXT("compile conversion failure should locate its conversion expression rather than an undefined helper"))));
					}
				}

				ASSERT_THAT(IsTrue(DiscardAndConfirmAbsent(*ScriptEngine, FailureModuleNameUtf8),
					*Case.Describe(TEXT("conversion scenario should discard its isolated module"))));

				const FString RecoveryModuleName = EqualAnsi(RecoveryCase.CatalogName, "same_module_or_context")
					? FailureModuleName
					: FailureModuleName + TEXT("_Fresh");
				const FString RecoverySource = BuildRecoverySource();
				asIScriptModule* RecoveryModule = nullptr;
				const int RecoveryBuildResult = CompileAndReport(
					NativeEngine,
					*TestRunner,
					Case.GetId() + TEXT("-RECOVERY"),
					RecoveryModuleName,
					RecoverySource,
					RecoveryModule);
				const FTCHARToUTF8 RecoveryModuleNameUtf8(*RecoveryModuleName);
				ASSERT_THAT(IsTrue(RecoveryBuildResult >= 0,
					*Case.Describe(TEXT("recovery should compile after the isolated conversion failure"))));
				asIScriptFunction* const RecoveryEntry = FindNoArgumentEntry(RecoveryModule, TEXT("int"), TEXT("RunConversionRecovery"));
				ASSERT_THAT(IsNotNull(RecoveryEntry,
					*Case.Describe(TEXT("recovery should resolve its exact entry declaration"))));
				if (RecoveryEntry != nullptr)
				{
					asIScriptContext* const Context = ScriptEngine->CreateContext();
					ASSERT_THAT(IsNotNull(Context,
						*Case.Describe(TEXT("recovery should create a clean context"))));
					if (Context != nullptr)
					{
						ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
							PrepareAndExecute(Context, RecoveryEntry),
							*Case.Describe(TEXT("recovery should execute after the selected conversion route"))));
						ASSERT_THAT(AreEqual(77, static_cast<int32>(Context->GetReturnDWord()),
							*Case.Describe(TEXT("recovery should return its independent marker"))));
						Context->Release();
					}
				}

				ASSERT_THAT(IsTrue(DiscardAndConfirmAbsent(*ScriptEngine, RecoveryModuleNameUtf8),
					*Case.Describe(TEXT("recovery should discard its module and context-owned state"))));
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
