#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

using namespace AngelscriptNativeTestSupport;

TEST_CLASS_WITH_FLAGS(FFunctionParameterDirectionTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Functions.ParameterDirections",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{

private:
	struct FDirectionCase
	{
		const ANSICHAR* CatalogName;
		const ANSICHAR* DeclarationSuffix;
		bool bWritesValue;
		bool bReadsValue;
	};

	inline static constexpr FDirectionCase DirectionCases[] =
	{
		{ "value", "", false, true },
		{ "in", "& in", false, true },
		{ "out", "& out", true, false },
		{ "inout", "& inout", true, true },
	};

	static void AppendGeneratedLine(FString& Source, const FString& Line = FString())
	{
		Source += Line;
		Source.AppendChar(TEXT('\n'));
	}

	static bool IsValueObject(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category == ENativeValueCategory::ScriptValue
			|| TypeCase.Category == ENativeValueCategory::NativeValue;
	}

	static const ANSICHAR* TypeDeclaration(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category == ENativeValueCategory::FloatingPoint
			? TypeCase.CatalogName
			: TypeCase.ScriptType;
	}

	static FString MakeReadExpression(const FNativeTypeCase& TypeCase, const TCHAR* VariableName)
	{
		if (IsValueObject(TypeCase))
		{
			return FString::Printf(TEXT("%s.Value == 1"), VariableName);
		}
		return FString::Printf(TEXT("%s == %hs"), VariableName, TypeCase.OneLiteral);
	}

	static FString MakeAssignStatement(const FNativeTypeCase& TypeCase, const TCHAR* VariableName)
	{
		if (IsValueObject(TypeCase))
		{
			return FString::Printf(TEXT("%s.Value = 1;"), VariableName);
		}
		return FString::Printf(TEXT("%s = %hs;"), VariableName, TypeCase.OneLiteral);
	}

	static FString MakeInitialStatement(const FNativeTypeCase& TypeCase)
	{
		if (IsValueObject(TypeCase))
		{
			return FString::Printf(TEXT("%hs Value(1);"), TypeDeclaration(TypeCase));
		}
		return FString::Printf(TEXT("%hs Value = %hs;"), TypeDeclaration(TypeCase), TypeCase.OneLiteral);
	}

	static FString MakeDefaultStatement(const FNativeTypeCase& TypeCase)
	{
		if (IsValueObject(TypeCase))
		{
			return FString::Printf(TEXT("%hs Value;"), TypeDeclaration(TypeCase));
		}
		return FString::Printf(TEXT("%hs Value = %hs;"), TypeDeclaration(TypeCase), TypeCase.ZeroLiteral);
	}

	static FString MakeFunctionName(const FNativeTypeCase& TypeCase, const FDirectionCase& DirectionCase)
	{
		return FString::Printf(TEXT("Probe_%hs_%hs"), TypeCase.CatalogName, DirectionCase.CatalogName);
	}

	static FString MakeEntryName(const FNativeTypeCase& TypeCase, const FDirectionCase& DirectionCase)
	{
		return FString::Printf(TEXT("Run_%hs_%hs"), TypeCase.CatalogName, DirectionCase.CatalogName);
	}

	static FString BuildParameterDirectionSource()
	{
		FString Source;
		AppendGeneratedLine(Source, TEXT("enum ENativeCaseEnum"));
		AppendGeneratedLine(Source, TEXT("{"));
		AppendGeneratedLine(Source, TEXT("\tMinimum = -1,"));
		AppendGeneratedLine(Source, TEXT("\tZero = 0,"));
		AppendGeneratedLine(Source, TEXT("\tOne = 1,"));
		AppendGeneratedLine(Source, TEXT("\tNearMaximum = 126,"));
		AppendGeneratedLine(Source, TEXT("\tMaximum = 127"));
		AppendGeneratedLine(Source, TEXT("}"));
		AppendGeneratedLine(Source);
		AppendGeneratedLine(Source, TEXT("struct FScriptCaseValue"));
		AppendGeneratedLine(Source, TEXT("{"));
		AppendGeneratedLine(Source, TEXT("\tint Value = 0;"));
		AppendGeneratedLine(Source);
		AppendGeneratedLine(Source, TEXT("\tFScriptCaseValue()"));
		AppendGeneratedLine(Source, TEXT("\t{"));
		AppendGeneratedLine(Source, TEXT("\t}"));
		AppendGeneratedLine(Source);
		AppendGeneratedLine(Source, TEXT("\tFScriptCaseValue(int InValue)"));
		AppendGeneratedLine(Source, TEXT("\t{"));
		AppendGeneratedLine(Source, TEXT("\t\tValue = InValue;"));
		AppendGeneratedLine(Source, TEXT("\t}"));
		AppendGeneratedLine(Source, TEXT("}"));
		AppendGeneratedLine(Source);

		for (const FNativeTypeCase& TypeCase : NativeTypeCases)
		{
			if (TypeCase.Category == ENativeValueCategory::ScriptReference
				|| TypeCase.Category == ENativeValueCategory::NativeReference
				|| TypeCase.Category == ENativeValueCategory::Null)
			{
				continue;
			}

			for (const FDirectionCase& DirectionCase : DirectionCases)
			{
				const FString FunctionName = MakeFunctionName(TypeCase, DirectionCase);
				const FString EntryName = MakeEntryName(TypeCase, DirectionCase);
				if (DirectionCase.bWritesValue)
				{
					AppendGeneratedLine(Source, FString::Printf(TEXT("void %s(%hs%s Value)"), *FunctionName, TypeDeclaration(TypeCase), ANSI_TO_TCHAR(DirectionCase.DeclarationSuffix)));
					AppendGeneratedLine(Source, TEXT("{"));
					if (DirectionCase.bReadsValue)
					{
						AppendGeneratedLine(Source, FString::Printf(TEXT("\tif (!(%s))"), *MakeReadExpression(TypeCase, TEXT("Value"))));
						AppendGeneratedLine(Source, TEXT("\t{"));
						AppendGeneratedLine(Source, TEXT("\t\tValue = Value;"));
						AppendGeneratedLine(Source, TEXT("\t}"));
					}
					AppendGeneratedLine(Source, TEXT("\t") + MakeAssignStatement(TypeCase, TEXT("Value")));
					AppendGeneratedLine(Source, TEXT("}"));
				}
				else
				{
					AppendGeneratedLine(Source, FString::Printf(TEXT("bool %s(%hs%s Value)"), *FunctionName, TypeDeclaration(TypeCase), ANSI_TO_TCHAR(DirectionCase.DeclarationSuffix)));
					AppendGeneratedLine(Source, TEXT("{"));
					AppendGeneratedLine(Source, FString::Printf(TEXT("\treturn %s;"), *MakeReadExpression(TypeCase, TEXT("Value"))));
					AppendGeneratedLine(Source, TEXT("}"));
				}
				AppendGeneratedLine(Source);

				AppendGeneratedLine(Source, FString::Printf(TEXT("bool %s()"), *EntryName));
				AppendGeneratedLine(Source, TEXT("{"));
				AppendGeneratedLine(Source, TEXT("\t") + (DirectionCase.bReadsValue ? MakeInitialStatement(TypeCase) : MakeDefaultStatement(TypeCase)));
				if (DirectionCase.bWritesValue)
				{
					AppendGeneratedLine(Source, FString::Printf(TEXT("\t%s(Value);"), *FunctionName));
					AppendGeneratedLine(Source, FString::Printf(TEXT("\treturn %s;"), *MakeReadExpression(TypeCase, TEXT("Value"))));
				}
				else
				{
					AppendGeneratedLine(Source, FString::Printf(TEXT("\treturn %s(Value) && (%s);"), *FunctionName, *MakeReadExpression(TypeCase, TEXT("Value"))));
				}
				AppendGeneratedLine(Source, TEXT("}"));
				AppendGeneratedLine(Source);
			}
		}

		return Source;
	}

public:
	TEST_METHOD(ParameterTypesByDirection)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-FN-PARAM-DIRECTION",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Cleanup);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Function parameter-direction product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(RegisterCoreLanguageTypedef(*ScriptEngine),
			TEXT("Function parameter-direction product should register its core typedef through the raw SDK API")));

		FNativeLifecycleRecorder Lifecycle;
		Lifecycle.Reset();
		ASSERT_THAT(IsTrue(RegisterNativeCaseValue(*ScriptEngine, Lifecycle),
			TEXT("Function parameter-direction product should register its local native value fixture")));

		const FString GeneratedSource = BuildParameterDirectionSource();
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("LANG-FN-PARAM-DIRECTION"),
			TEXT("FunctionParameterDirections"),
			GeneratedSource);
		const FTCHARToUTF8 GeneratedSourceUtf8(*GeneratedSource);
		{
			FScopedNativeModule Module(
				*TestRunner,
				Engine,
				"FunctionParameterDirections",
				std::string(GeneratedSourceUtf8.Get(), GeneratedSourceUtf8.Length()));
			ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("Function parameter-direction product should compile all type and direction cells")));
			if (!Module.IsValid())
			{
				TestRunner->AddInfo(GeneratedSource);
				return;
			}

			for (const FNativeTypeCase& TypeCase : NativeTypeCases)
			{
				if (TypeCase.Category == ENativeValueCategory::ScriptReference
					|| TypeCase.Category == ENativeValueCategory::NativeReference
					|| TypeCase.Category == ENativeValueCategory::Null)
				{
					continue;
				}

				for (const FDirectionCase& DirectionCase : DirectionCases)
				{
					const FString CaseId = MakeNativeCaseId(
						"LANG-FN-PARAM-DIRECTION",
						{ ANSI_TO_TCHAR(DirectionCase.CatalogName), ANSI_TO_TCHAR(TypeCase.CatalogName) });
					const FNativeCaseContext Case(CaseId);
					const FString ProbeName = MakeFunctionName(TypeCase, DirectionCase);
					const FString ProbeDeclaration = FString::Printf(
						TEXT("%s %s(%hs%s Value)"),
						DirectionCase.bWritesValue ? TEXT("void") : TEXT("bool"),
						*ProbeName,
						TypeDeclaration(TypeCase),
						ANSI_TO_TCHAR(DirectionCase.DeclarationSuffix));
					asIScriptFunction* const Probe =
						Module->GetFunctionByDecl(TCHAR_TO_ANSI(*ProbeDeclaration));
					ASSERT_THAT(IsNotNull(Probe, *Case.Describe(TEXT("generated probe should be published"))));
					if (Probe != nullptr)
					{
						ASSERT_THAT(AreEqual(1, static_cast<int32>(Probe->GetParamCount()),
							*Case.Describe(TEXT("probe metadata should expose exactly one parameter"))));
						const char* ExactDeclaration = Probe->GetDeclaration();
						ASSERT_THAT(AreEqual(Probe, ScriptEngine->GetFunctionById(Probe->GetId()),
							*Case.DescribeResult(ExactDeclaration, TEXT("same function pointer"), TEXT("function-ID round-trip result"))));
					}

					const FString EntryName = MakeEntryName(TypeCase, DirectionCase);
					const FString EntryDeclaration = FString::Printf(TEXT("bool %s()"), *EntryName);
					AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(
						*TestRunner,
						ScriptEngine,
						Module,
						TCHAR_TO_ANSI(*EntryDeclaration));
					ASSERT_THAT(IsTrue(Invoker.IsValid(),
						*Case.DescribeResult(TCHAR_TO_ANSI(*EntryDeclaration), TEXT("valid invoker"), TEXT("lookup result"))));
					if (Invoker.IsValid())
					{
						ASSERT_THAT(IsTrue(Invoker.CallAndReturn<bool>(false),
							*Case.DescribeResult(TCHAR_TO_ANSI(*EntryDeclaration), TEXT("true"), TEXT("false"))));
					}
				}
			}
		}

		ASSERT_THAT(IsNull(ScriptEngine->GetModule("FunctionParameterDirections", asGM_ONLY_IF_EXISTS),
			TEXT("Function parameter-direction product should discard its generated module")));
		ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
			TEXT("Function parameter-direction product should release every native value after execution")));
		ASSERT_THAT(IsTrue(Lifecycle.Num(ENativeLifecycleEvent::DefaultConstruct) > 0,
			TEXT("Function parameter-direction product should observe native default construction")));
		ASSERT_THAT(IsTrue(Lifecycle.Num(ENativeLifecycleEvent::Destruct) > 0,
			TEXT("Function parameter-direction product should observe native destruction")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
