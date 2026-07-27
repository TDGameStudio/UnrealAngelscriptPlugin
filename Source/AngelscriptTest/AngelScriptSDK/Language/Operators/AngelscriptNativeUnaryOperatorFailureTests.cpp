#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeDiagnosticTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FUnaryOperatorFailureTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Operators.UnaryFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext = AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeTestEngine = AngelscriptNativeTestSupport::FNativeTestEngine;

	enum class EUnaryCategory : uint8
	{
		MutableLValue,
		ConstLValue,
		Temporary,
		Field,
		Alias,
	};

	struct FCategoryCase
	{
		const ANSICHAR* CatalogName;
		EUnaryCategory Category;
	};

	struct FFailureCase
	{
		const ANSICHAR* CatalogName;
		const ANSICHAR* TypeName;
		const TCHAR* Token;
	};

	inline static constexpr FCategoryCase CategoryCases[] = {
		{"mutable_lvalue", EUnaryCategory::MutableLValue},
		{"const_lvalue", EUnaryCategory::ConstLValue},
		{"temporary", EUnaryCategory::Temporary},
		{"field", EUnaryCategory::Field},
		{"alias", EUnaryCategory::Alias},
	};

	inline static constexpr FFailureCase FailureCases[] = {
		{"positive_bool", "bool", TEXT("+")},
		{"negative_bool", "bool", TEXT("-")},
		{"bit_not_bool", "bool", TEXT("~")},
		{"logical_not_signed", "int", TEXT("!")},
		{"logical_not_unsigned", "uint", TEXT("!")},
		{"logical_not_float", "float64", TEXT("!")},
		{"bit_not_float32", "float32", TEXT("~")},
		{"bit_not_float64", "float64", TEXT("~")},
	};

	static FString ScriptType(const FFailureCase& FailureCase, const asIScriptEngine& Engine)
	{
		if (AngelscriptNativeTestSupport::EqualAnsi(FailureCase.TypeName, "float32"))
		{
			return Engine.GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0 ? TEXT("float32")
																		: TEXT("float");
		}
		if (AngelscriptNativeTestSupport::EqualAnsi(FailureCase.TypeName, "float64"))
		{
			return Engine.GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0 ? TEXT("float")
																		: TEXT("float64");
		}
		return ANSI_TO_TCHAR(FailureCase.TypeName);
	}

	static FString ModuleName(const FCategoryCase& CategoryCase, const FFailureCase& FailureCase)
	{
		return FString::Printf(TEXT("ASNativeUnaryFailure_%hs_%hs"),
			CategoryCase.CatalogName,
			FailureCase.CatalogName);
	}

	static FString BuildUnaryFailureSource(asIScriptEngine& Engine,
		const FCategoryCase& CategoryCase,
		const FFailureCase& FailureCase)
	{
		using namespace AngelscriptNativeTestSupport;
		const FString Type = ScriptType(FailureCase, Engine);
		FString Source;
		if (CategoryCase.Category == EUnaryCategory::Temporary)
		{
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("%s MakeInvalidUnaryTemporary(%s Input)"), *Type, *Type));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Input;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (CategoryCase.Category == EUnaryCategory::Field)
		{
			AppendGeneratedAsLine(Source, TEXT("struct FInvalidUnaryOwner"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s Value;"), *Type));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		const FString Parameter = CategoryCase.Category == EUnaryCategory::Alias
									  ? FString::Printf(TEXT("%s& in Input"), *Type)
									  : FString::Printf(TEXT("%s Input"), *Type);
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("int InvalidUnary(%s)"), *Parameter));
		AppendGeneratedAsLine(Source, TEXT("{"));
		FString Expression;
		switch (CategoryCase.Category)
		{
		case EUnaryCategory::MutableLValue:
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s Value = Input;"), *Type));
			Expression = FString::Printf(TEXT("%sValue"), FailureCase.Token);
			break;
		case EUnaryCategory::ConstLValue:
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("\tconst %s Value = Input;"), *Type));
			Expression = FString::Printf(TEXT("%sValue"), FailureCase.Token);
			break;
		case EUnaryCategory::Temporary:
			Expression =
				FString::Printf(TEXT("%sMakeInvalidUnaryTemporary(Input)"), FailureCase.Token);
			break;
		case EUnaryCategory::Field:
			AppendGeneratedAsLine(Source, TEXT("\tFInvalidUnaryOwner Owner;"));
			AppendGeneratedAsLine(Source, TEXT("\tOwner.Value = Input;"));
			Expression = FString::Printf(TEXT("%sOwner.Value"), FailureCase.Token);
			break;
		case EUnaryCategory::Alias:
			Expression = FString::Printf(TEXT("%sInput"), FailureCase.Token);
			break;
		}
		AppendGeneratedAsLine(
			Source, FString::Printf(TEXT("\treturn int(%s); // UNARY_CAUSE"), *Expression));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static FString BuildUnaryFailureRecoverySource()
	{
		using namespace AngelscriptNativeTestSupport;
		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int RecoverUnary()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 401;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static asIScriptModule* CompileAndReport(FNativeTestEngine& Engine,
		FAutomationTestBase& Test,
		const FString& SourceId,
		const FString& CurrentModuleName,
		const FString& Source,
		int32& OutBuildResult)
	{
		using namespace AngelscriptNativeTestSupport;
		PrintGeneratedAsSource(Test, SourceId, CurrentModuleName, Source);
		Engine.Reset(Test);
		const FTCHARToUTF8 ModuleNameUtf8(*CurrentModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		asIScriptModule* Module = nullptr;
		OutBuildResult =
			CompileNativeModule(Engine.Get(), ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
		return Module;
	}

	static int32 LastSourceLineContaining(const FString& Source, const FString& Token)
	{
		TArray<FString> Lines;
		Source.ParseIntoArrayLines(Lines, false);
		for (int32 LineIndex = Lines.Num() - 1; LineIndex >= 0; --LineIndex)
		{
			if (Lines[LineIndex].Contains(Token, ESearchCase::CaseSensitive))
			{
				return LineIndex + 1;
			}
		}
		return INDEX_NONE;
	}

	static TArray<AngelscriptNativeTestSupport::FNativeMessageEntry> ErrorMessages(
		const AngelscriptNativeTestSupport::FNativeMessageCollector& Messages)
	{
		TArray<AngelscriptNativeTestSupport::FNativeMessageEntry> Errors;
		for (const AngelscriptNativeTestSupport::FNativeMessageEntry& Entry : Messages.Entries)
		{
			if (Entry.Type == asMSGTYPE_ERROR)
			{
				Errors.Add(Entry);
			}
		}
		return Errors;
	}

	static bool HasFunctionNamed(asIScriptModule& Module, const ANSICHAR* Name)
	{
		for (asUINT FunctionIndex = 0; FunctionIndex < Module.GetFunctionCount(); ++FunctionIndex)
		{
			asIScriptFunction* const Function = Module.GetFunctionByIndex(FunctionIndex);
			if (Function != nullptr && FCStringAnsi::Strcmp(Function->GetName(), Name) == 0)
			{
				return true;
			}
		}
		return false;
	}

	static bool ExecuteRecovery(FAutomationTestBase& Test,
		asIScriptEngine& Engine,
		asIScriptModule& Module,
		const FNativeCaseContext& Case)
	{
		using namespace AngelscriptNativeTestSupport;
		FNoDiscardAsserter Assert(Test);
		asIScriptFunction* const Function =
			GetNativeFunctionByExactDecl(&Module, "int RecoverUnary()");
		if (!Assert.IsNotNull(
				Function, *Case.Describe(TEXT("unary recovery should resolve exactly"))))
		{
			return false;
		}
		asIScriptContext* const Context = Engine.CreateContext();
		if (!Assert.IsNotNull(
				Context, *Case.Describe(TEXT("unary recovery should create a context"))))
		{
			return false;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};
		const int ExecuteResult = PrepareAndExecute(Context, Function);
		return Assert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
				   ExecuteResult,
				   *Case.Describe(TEXT("unary recovery should execute"))) &&
			   Assert.AreEqual(401,
				   static_cast<int32>(Context->GetReturnDWord()),
				   *Case.Describe(TEXT("unary recovery should return its exact marker")));
	}

public:
	TEST_METHOD(FailuresByCategory)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("LANG-OP-UNARY-REJECTION",
			ENativeEvidence::Compile | ENativeEvidence::Diagnostic | ENativeEvidence::Metadata |
				ENativeEvidence::Isolation);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		ASSERT_THAT(IsNotNull(
			Engine.Get(), TEXT("unary rejection product should create a standalone engine")));
		if (Engine.Get() == nullptr)
		{
			return;
		}

		TArray<FString> ConstructedIds;
		TSet<FString> UniqueIds;
		for (const FCategoryCase& CategoryCase : CategoryCases)
		{
			for (const FFailureCase& FailureCase : FailureCases)
			{
				const FNativeCaseContext Case(MakeNativeCaseId("LANG-OP-UNARY-REJECTION",
					{ANSI_TO_TCHAR(CategoryCase.CatalogName),
							ANSI_TO_TCHAR(FailureCase.CatalogName)}));
				ConstructedIds.Add(Case.GetId());
				const bool bUniqueCaseId = !UniqueIds.Contains(Case.GetId());
				UniqueIds.Add(Case.GetId());
				ASSERT_THAT(IsTrue(bUniqueCaseId,
					*Case.Describe(TEXT("unary rejection case ID should be unique"))));

				const FString CurrentModuleName = ModuleName(CategoryCase, FailureCase);
				const FString FailureSource =
					BuildUnaryFailureSource(*Engine.Get(), CategoryCase, FailureCase);
				int32 BuildResult = asSUCCESS;
				asIScriptModule* Module = CompileAndReport(Engine,
					*TestRunner,
					Case.GetId(),
					CurrentModuleName,
					FailureSource,
					BuildResult);
				ASSERT_THAT(IsTrue(BuildResult < 0,
					*Case.DescribeResult("<failure build>",
						TEXT("negative build result"),
						FString::Printf(
							TEXT("%d Messages={%s}"), BuildResult, *Engine.GetMessagesText()))));
				const TArray<FNativeMessageEntry> Errors = ErrorMessages(Engine.GetMessages());
				ASSERT_THAT(AreEqual(1,
					Errors.Num(),
					*Case.DescribeResult("<failure diagnostic>",
						TEXT("one illegal-operation error"),
						Engine.GetMessagesText())));
				const int32 ExpectedLine =
					LastSourceLineContaining(FailureSource, TEXT("UNARY_CAUSE"));
				ASSERT_THAT(IsTrue(ExpectedLine > 0,
					*Case.Describe(TEXT("failure source should retain its causal marker"))));
				if (Errors.Num() == 1)
				{
					ASSERT_THAT(AreEqual(ExpectedLine,
						Errors[0].Row,
						*Case.Describe(
							TEXT("unary rejection should report the exact causal source line"))));
					ASSERT_THAT(IsTrue(
						Errors[0].Message.Contains(
							TEXT("Illegal operation on this datatype"), ESearchCase::CaseSensitive),
						*Case.Describe(
							TEXT("unary rejection should preserve current-fork diagnostic text"))));
				}
				ASSERT_THAT(IsTrue(Module == nullptr || !HasFunctionNamed(*Module, "InvalidUnary"),
					*Case.Describe(
						TEXT("failed unary source should publish no callable invalid entry"))));

				const FTCHARToUTF8 ModuleNameUtf8(*CurrentModuleName);
				Engine.Get()->DiscardModule(ModuleNameUtf8.Get());
				const FString RecoverySource = BuildUnaryFailureRecoverySource();
				int32 RecoveryBuildResult = asERROR;
				Module = CompileAndReport(Engine,
					*TestRunner,
					Case.GetId() + TEXT("-RECOVERY"),
					CurrentModuleName,
					RecoverySource,
					RecoveryBuildResult);
				ASSERT_THAT(IsTrue(RecoveryBuildResult >= 0,
					*Case.DescribeResult("<recovery build>",
						TEXT("successful same-name recovery"),
						Engine.GetMessagesText())));
				ASSERT_THAT(IsNotNull(
					Module, *Case.Describe(TEXT("unary recovery should publish its module"))));
				if (RecoveryBuildResult >= 0 && Module != nullptr)
				{
					ExecuteRecovery(*TestRunner, *Engine.Get(), *Module, Case);
				}
				Engine.Get()->DiscardModule(ModuleNameUtf8.Get());
				ASSERT_THAT(IsNull(
					Engine.Get()->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
					*Case.Describe(TEXT("unary rejection module should discard after recovery"))));
			}
		}

		ASSERT_THAT(AreEqual(40,
			ConstructedIds.Num(),
			TEXT("unary rejection product should construct all forty catalog IDs")));
		ASSERT_THAT(AreEqual(ConstructedIds.Num(),
			UniqueIds.Num(),
			TEXT("unary rejection product should construct no duplicate IDs")));
	}
};

#endif
