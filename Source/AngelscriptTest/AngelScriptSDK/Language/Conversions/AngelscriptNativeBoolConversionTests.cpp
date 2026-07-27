#include "AngelscriptNativeConversionTestSupport.h"

#include "CQTest.h"

using AngelscriptNativeTestSupport::EqualAnsi;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FBoolConversionTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Conversions.Bool",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:

	struct FSourceCase
	{
		const ANSICHAR* CatalogName;
		const ANSICHAR* ScriptType;
		bool bAccepted;
	};

	struct FContextCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FValueCase
	{
		const ANSICHAR* CatalogName;
		bool bBranchValue;
	};

	inline static constexpr FSourceCase SourceCases[] =
	{
		{ "int8", "int8", false },
		{ "int16", "int16", false },
		{ "int", "int", false },
		{ "int64", "int64", false },
		{ "uint8", "uint8", false },
		{ "uint16", "uint16", false },
		{ "uint", "uint", false },
		{ "uint64", "uint64", false },
		{ "float32", "float32", false },
		{ "float64", "float64", false },
		{ "bool", "bool", true },
		{ "enum", "EBoolConversion", false },
		{ "typedef", "BoolAlias", false },
	};

	inline static constexpr FContextCase ContextCases[] =
	{
		{ "if" },
		{ "while" },
		{ "ternary_condition" },
		{ "logical_and" },
		{ "logical_or" },
		{ "logical_xor" },
	};

	inline static constexpr FValueCase ValueCases[] =
	{
		{ "zero", false },
		{ "one", true },
		{ "negative", true },
	};

	static FString SourceTypeForCase(
		const FSourceCase& SourceCase,
		const asIScriptEngine& Engine)
	{
		if (EqualAnsi(SourceCase.CatalogName, "float32"))
		{
			return Engine.GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0
				? TEXT("float32")
				: TEXT("float");
		}

		if (EqualAnsi(SourceCase.CatalogName, "float64"))
		{
			return Engine.GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0
				? TEXT("float")
				: TEXT("float64");
		}

		return ANSI_TO_TCHAR(SourceCase.ScriptType);
	}

	static const ANSICHAR* LiteralForCase(
		const FSourceCase& SourceCase,
		const FValueCase& ValueCase)
	{
		if (EqualAnsi(SourceCase.CatalogName, "bool"))
		{
			return ValueCase.bBranchValue ? "true" : "false";
		}

		if (EqualAnsi(SourceCase.CatalogName, "enum"))
		{
			return EqualAnsi(ValueCase.CatalogName, "zero")
				? "EBoolConversion::Zero"
				: EqualAnsi(ValueCase.CatalogName, "one")
					? "EBoolConversion::One"
					: "EBoolConversion::Negative";
		}

		if (EqualAnsi(SourceCase.CatalogName, "typedef"))
		{
			return EqualAnsi(ValueCase.CatalogName, "zero")
				? "BoolAlias(0)"
				: EqualAnsi(ValueCase.CatalogName, "one")
					? "BoolAlias(1)"
					: "BoolAlias(-1)";
		}

		if (EqualAnsi(ValueCase.CatalogName, "zero"))
		{
			return "0";
		}

		if (EqualAnsi(ValueCase.CatalogName, "one"))
		{
			return "1";
		}

		return "-1";
	}

	static FString BuildBoolConversionSource(
		const FString& SourceType,
		const FSourceCase& SourceCase,
		const FContextCase& ContextCase,
		const FValueCase& ValueCase)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		if (EqualAnsi(SourceCase.CatalogName, "enum"))
		{
			AppendGeneratedAsLine(Source, TEXT("enum EBoolConversion"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tNegative = -1,"));
			AppendGeneratedAsLine(Source, TEXT("\tZero = 0,"));
			AppendGeneratedAsLine(Source, TEXT("\tOne = 1"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (EqualAnsi(SourceCase.CatalogName, "typedef"))
		{
			AppendGeneratedAsLine(Source, TEXT("typedef int BoolAlias;"));
			AppendGeneratedAsLine(Source);
		}

		AppendGeneratedAsLine(Source, TEXT("int RunBoolConversion()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s SourceValue = %hs;"), *SourceType, LiteralForCase(SourceCase, ValueCase)));

		if (EqualAnsi(ContextCase.CatalogName, "if"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tif (SourceValue)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 1;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 0;"));
		}
		else if (EqualAnsi(ContextCase.CatalogName, "while"))
		{
			AppendGeneratedAsLine(Source, TEXT("\twhile (SourceValue)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 1;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 0;"));
		}
		else if (EqualAnsi(ContextCase.CatalogName, "ternary_condition"))
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn SourceValue ? 1 : 0;"));
		}
		else if (EqualAnsi(ContextCase.CatalogName, "logical_and"))
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn SourceValue && true ? 1 : 0;"));
		}
		else if (EqualAnsi(ContextCase.CatalogName, "logical_or"))
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn SourceValue || false ? 1 : 0;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn SourceValue ^^ false ? 1 : 0;"));
		}

		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

public:
	TEST_METHOD(SourcesByContextAndValue)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-CONV-BOOL-CONTEXT",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata);

		AngelscriptNativeTestSupport::FNativeTestEngine NativeEngine;
		NativeEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			NativeEngine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = NativeEngine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Boolean conversion product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		for (const FSourceCase& SourceCase : SourceCases)
		{
			for (const FContextCase& ContextCase : ContextCases)
			{
				for (const FValueCase& ValueCase : ValueCases)
				{
					const FNativeCaseContext Case(MakeNativeCaseId(
						"LANG-CONV-BOOL-CONTEXT",
						{ ANSI_TO_TCHAR(SourceCase.CatalogName), ANSI_TO_TCHAR(ContextCase.CatalogName), ANSI_TO_TCHAR(ValueCase.CatalogName) }));
					const FString ModuleName = TEXT("BoolConversion_") + Case.GetId();
					const FString Source = BuildBoolConversionSource(
						SourceTypeForCase(SourceCase, *ScriptEngine),
						SourceCase,
						ContextCase,
						ValueCase);

					const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
					const FTCHARToUTF8 SourceUtf8(*Source);
					NativeEngine.Reset(*TestRunner);
					PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);
					asIScriptModule* Module = nullptr;
					const int BuildResult = CompileNativeModule(
						ScriptEngine,
						ModuleNameUtf8.Get(),
						SourceUtf8.Get(),
						Module);

					if (!SourceCase.bAccepted)
					{
						ASSERT_THAT(IsTrue(BuildResult < 0,
							*Case.Describe(TEXT("non-boolean source should be rejected in the selected boolean context"))));
						ASSERT_THAT(IsTrue(HasOwnedLocatedDiagnostic(NativeEngine.GetMessages(), ModuleName),
							*Case.Describe(TEXT("boolean-context rejection should identify the conversion site"))));
					}
					else
					{
						ASSERT_THAT(IsTrue(BuildResult >= 0,
							*Case.Describe(TEXT("bool source should compile in the selected boolean context"))));
						asIScriptFunction* const Entry = FindNoArgumentEntry(Module, TEXT("int"), TEXT("RunBoolConversion"));
						ASSERT_THAT(IsNotNull(Entry,
							*Case.Describe(TEXT("bool source should publish its exact entry declaration"))));
						if (Entry != nullptr)
						{
							asIScriptContext* const Context = ScriptEngine->CreateContext();
							ASSERT_THAT(IsNotNull(Context,
								*Case.Describe(TEXT("bool source should create an execution context"))));
							if (Context != nullptr)
							{
								ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
									PrepareAndExecute(Context, Entry),
									*Case.Describe(TEXT("bool source should execute the selected context"))));
								ASSERT_THAT(AreEqual(ValueCase.bBranchValue ? 1 : 0,
									static_cast<int32>(Context->GetReturnDWord()),
									*Case.Describe(TEXT("bool source should preserve the expected branch marker"))));
								Context->Release();
							}
						}
					}

					ASSERT_THAT(IsTrue(DiscardAndConfirmAbsent(*ScriptEngine, ModuleNameUtf8),
						*Case.Describe(TEXT("boolean conversion cell should discard its module"))));
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
