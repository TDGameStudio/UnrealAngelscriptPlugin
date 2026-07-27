#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

using AngelscriptNativeTestSupport::AppendGeneratedAsLine;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FSwitchTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.ControlFlow.Switch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FNamedCase
	{
		const ANSICHAR* CatalogName;
		bool bInvalid = false;
	};

	struct FSwitchExpectation
	{
		int32 ReturnValue = 0;
		bool bThrows = false;
	};

	inline static constexpr FNamedCase SelectorCases[] =
	{
		{ "int8" }, { "int16" }, { "int" }, { "int64" }, { "uint8" }, { "uint16" }, { "uint" }, { "uint64" }, { "enum" }, { "typedef" }, { "boundary" }, { "unsupported", true },
	};
	inline static constexpr FNamedCase CaseCases[] =
	{
		{ "first" }, { "middle" }, { "last" }, { "default" }, { "no_match" }, { "fallthrough" }, { "grouped" }, { "duplicate", true }, { "non_constant", true },
	};
	inline static constexpr FNamedCase ExitCases[] =
	{
		{ "break" }, { "fallthrough" }, { "return" }, { "exception" },
	};


	static bool IsCase(const FNamedCase& Case, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(Case.CatalogName, Name) == 0;
	}

	static int32 SelectorValueFor(const FNamedCase& CaseCase)
	{
		if (IsCase(CaseCase, "first") || IsCase(CaseCase, "fallthrough"))
		{
			return 1;
		}
		if (IsCase(CaseCase, "middle") || IsCase(CaseCase, "grouped"))
		{
			return 2;
		}
		if (IsCase(CaseCase, "last"))
		{
			return 3;
		}
		if (IsCase(CaseCase, "default"))
		{
			return 4;
		}
		return 5;
	}

	static int32 TraceSeedFor(const FNamedCase& CaseCase)
	{
		if (IsCase(CaseCase, "first"))
		{
			return 11;
		}
		if (IsCase(CaseCase, "middle"))
		{
			return 12;
		}
		if (IsCase(CaseCase, "last"))
		{
			return 13;
		}
		if (IsCase(CaseCase, "default"))
		{
			return 14;
		}
		if (IsCase(CaseCase, "no_match"))
		{
			return 15;
		}
		if (IsCase(CaseCase, "fallthrough"))
		{
			return 16;
		}
		return 17;
	}

	static FString TypedLiteral(const FNamedCase& SelectorCase, const int32 Value)
	{
		if (IsCase(SelectorCase, "enum"))
		{
			switch (Value)
			{
			case 0: return TEXT("ESelector::Zero");
			case 1: return TEXT("ESelector::One");
			case 2: return TEXT("ESelector::Two");
			case 3: return TEXT("ESelector::Three");
			case 4: return TEXT("ESelector::Four");
			case 5: return TEXT("ESelector::Five");
			default: return TEXT("ESelector::Six");
			}
		}
		if (IsCase(SelectorCase, "boundary"))
		{
			return FString::FromInt(2147483641 + Value);
		}
		if (IsCase(SelectorCase, "typedef"))
		{
			return FString::Printf(TEXT("SelectorAlias(%d)"), Value);
		}
		if (!IsCase(SelectorCase, "unsupported"))
		{
			return FString::Printf(TEXT("%hs(%d)"), SelectorCase.CatalogName, Value);
		}
		return FString::FromInt(Value);
	}

	static void AppendSelectorDeclaration(FString& Source, const FNamedCase& SelectorCase, const FNamedCase& CaseCase)
	{
		if (IsCase(SelectorCase, "enum"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tESelector Value = %s;"), *TypedLiteral(SelectorCase, SelectorValueFor(CaseCase))));
		}
		else if (IsCase(SelectorCase, "typedef"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tSelectorAlias Value = SelectorAlias(%d);"), SelectorValueFor(CaseCase)));
		}
		else if (IsCase(SelectorCase, "boundary"))
		{
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("\tint Value = %s;"),
					*TypedLiteral(SelectorCase, SelectorValueFor(CaseCase))));
		}
		else if (IsCase(SelectorCase, "unsupported"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFUnsupported Value;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%hs Value = %hs(%d);"),
				SelectorCase.CatalogName,
				SelectorCase.CatalogName,
				SelectorValueFor(CaseCase)));
		}
	}

	static FString CaseLabel(const FNamedCase& SelectorCase, const int32 Value)
	{
		return FString::Printf(TEXT("\tcase %s:"), *TypedLiteral(SelectorCase, Value));
	}

	static void AppendSelectedExit(
		FString& Source,
		const FNamedCase& SelectorCase,
		const FNamedCase& ExitCase,
		const FString& Indent)
	{
		if (IsCase(ExitCase, "break"))
		{
			AppendGeneratedAsLine(Source, Indent + TEXT("break;"));
		}
		else if (IsCase(ExitCase, "fallthrough"))
		{
			AppendGeneratedAsLine(Source, Indent.LeftChop(1) + FString::Printf(TEXT("case %s:"), *TypedLiteral(SelectorCase, 6)));
			AppendGeneratedAsLine(Source, Indent + TEXT("Trace += 100;"));
			AppendGeneratedAsLine(Source, Indent + TEXT("break;"));
		}
		else if (IsCase(ExitCase, "return"))
		{
			AppendGeneratedAsLine(Source, Indent + TEXT("return Trace + 200;"));
		}
		else
		{
			// The current fork does not recognize string literals in throw(); use a
			// runtime divide-by-zero fault so the switch exit remains executable.
			AppendGeneratedAsLine(Source, Indent + TEXT("{"));
			AppendGeneratedAsLine(Source, Indent + TEXT("\tint ExceptionDivisor = 0;"));
			AppendGeneratedAsLine(Source, Indent + TEXT("\tTrace += 1 / ExceptionDivisor;"));
			AppendGeneratedAsLine(Source, Indent + TEXT("}"));
			AppendGeneratedAsLine(Source, Indent + TEXT("break;"));
		}
	}

	static void AppendSelectedCase(FString& Source, const FNamedCase& SelectorCase, const FNamedCase& CaseCase, const FNamedCase& ExitCase)
	{
		const int32 Seed = TraceSeedFor(CaseCase);
		if (IsCase(CaseCase, "duplicate"))
		{
			AppendGeneratedAsLine(Source, CaseLabel(SelectorCase, 1));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t\tTrace += %d;"), Seed));
			AppendSelectedExit(Source, SelectorCase, ExitCase, TEXT("\t\t"));
			AppendGeneratedAsLine(Source, CaseLabel(SelectorCase, 1));
			AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 1000;"));
			AppendGeneratedAsLine(Source, TEXT("\t\tbreak;"));
			return;
		}
		if (IsCase(CaseCase, "non_constant"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tcase DynamicCase:"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t\tTrace += %d;"), Seed));
			AppendSelectedExit(Source, SelectorCase, ExitCase, TEXT("\t\t"));
			return;
		}
		if (IsCase(CaseCase, "default") || IsCase(CaseCase, "no_match"))
		{
			AppendGeneratedAsLine(Source, CaseLabel(SelectorCase, 0));
			AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 1000;"));
			AppendGeneratedAsLine(Source, TEXT("\t\tbreak;"));
			AppendGeneratedAsLine(Source, TEXT("\tdefault:"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t\tTrace += %d;"), Seed));
			AppendSelectedExit(Source, SelectorCase, ExitCase, TEXT("\t\t"));
			return;
		}
		if (IsCase(CaseCase, "grouped"))
		{
			AppendGeneratedAsLine(Source, CaseLabel(SelectorCase, 1));
			AppendGeneratedAsLine(Source, CaseLabel(SelectorCase, 2));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t\tTrace += %d;"), Seed));
			AppendSelectedExit(Source, SelectorCase, ExitCase, TEXT("\t\t"));
			return;
		}
		if (IsCase(CaseCase, "fallthrough"))
		{
			AppendGeneratedAsLine(Source, CaseLabel(SelectorCase, 1));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t\tTrace += %d;"), Seed));
			AppendGeneratedAsLine(Source, CaseLabel(SelectorCase, 2));
			AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 1;"));
			AppendSelectedExit(Source, SelectorCase, ExitCase, TEXT("\t\t"));
			return;
		}

		AppendGeneratedAsLine(Source, CaseLabel(SelectorCase, SelectorValueFor(CaseCase)));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t\tTrace += %d;"), Seed));
		AppendSelectedExit(Source, SelectorCase, ExitCase, TEXT("\t\t"));
	}

	static void AppendBoundaryControl(FString& Source)
	{
		AppendGeneratedAsLine(Source, TEXT("int BoundaryControl()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Trace = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tint Lower = 2147483642;"));
		AppendGeneratedAsLine(Source, TEXT("\tswitch (Lower)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\tcase 2147483642:"));
		AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 1;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tbreak;"));
		AppendGeneratedAsLine(Source, TEXT("\tdefault:"));
		AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 100;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tbreak;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("\tint Trigger = 2147483643;"));
		AppendGeneratedAsLine(Source, TEXT("\tswitch (Trigger)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\tcase 2147483643:"));
		AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 2;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tbreak;"));
		AppendGeneratedAsLine(Source, TEXT("\tdefault:"));
		AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 200;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tbreak;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("\tint Dense = 2147483647;"));
		AppendGeneratedAsLine(Source, TEXT("\tswitch (Dense)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\tcase 2147483645:"));
		AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 400;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tbreak;"));
		AppendGeneratedAsLine(Source, TEXT("\tcase 2147483646:"));
		AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 500;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tbreak;"));
		AppendGeneratedAsLine(Source, TEXT("\tcase 2147483647:"));
		AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 3;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tbreak;"));
		AppendGeneratedAsLine(Source, TEXT("\tdefault:"));
		AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 800;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tbreak;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Trace;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static FString BuildSwitchSource(
		const FNamedCase& SelectorCase,
		const FNamedCase& CaseCase,
		const FNamedCase& ExitCase)
	{
		FString Source;
		AppendGeneratedAsLine(Source, TEXT("enum ESelector"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tZero = 0,"));
		AppendGeneratedAsLine(Source, TEXT("\tOne = 1,"));
		AppendGeneratedAsLine(Source, TEXT("\tTwo = 2,"));
		AppendGeneratedAsLine(Source, TEXT("\tThree = 3,"));
		AppendGeneratedAsLine(Source, TEXT("\tFour = 4,"));
		AppendGeneratedAsLine(Source, TEXT("\tFive = 5,"));
		AppendGeneratedAsLine(Source, TEXT("\tSix = 6"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("// SelectorAlias is registered through the raw SDK before each module build."));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("class FUnsupported"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		if (IsCase(SelectorCase, "boundary"))
		{
			AppendBoundaryControl(Source);
		}
		AppendGeneratedAsLine(Source, TEXT("int Entry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, IsCase(SelectorCase, "boundary")
			? TEXT("\tint Trace = BoundaryControl();")
			: TEXT("\tint Trace = 0;"));
		AppendSelectorDeclaration(Source, SelectorCase, CaseCase);
		if (IsCase(CaseCase, "non_constant"))
		{
			if (IsCase(SelectorCase, "enum"))
			{
				AppendGeneratedAsLine(Source, TEXT("\tESelector DynamicCase = ESelector::One;"));
			}
			else
			{
				AppendGeneratedAsLine(Source, TEXT("\tint DynamicCase = 1;"));
			}
		}
		AppendGeneratedAsLine(Source, TEXT("\tswitch (Value)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendSelectedCase(Source, SelectorCase, CaseCase, ExitCase);
		if (!IsCase(CaseCase, "default") && !IsCase(CaseCase, "no_match"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tdefault:"));
			AppendGeneratedAsLine(Source, TEXT("\t\tTrace += 5000;"));
			AppendGeneratedAsLine(Source, TEXT("\t\tbreak;"));
		}
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Trace;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static FSwitchExpectation ExpectedResult(const FNamedCase& SelectorCase, const FNamedCase& CaseCase, const FNamedCase& ExitCase)
	{
		FSwitchExpectation Result;
		Result.ReturnValue = TraceSeedFor(CaseCase) + (IsCase(CaseCase, "fallthrough") ? 1 : 0);
		if (IsCase(SelectorCase, "boundary"))
		{
			Result.ReturnValue += 6;
		}
		if (IsCase(ExitCase, "exception"))
		{
			Result.bThrows = true;
		}
		else if (IsCase(ExitCase, "fallthrough"))
		{
			Result.ReturnValue += 100;
		}
		else if (IsCase(ExitCase, "return"))
		{
			Result.ReturnValue += 200;
		}
		return Result;
	}

public:
	TEST_METHOD(SelectorsByCaseAndExit)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-CF-SWITCH",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Runtime
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Bytecode);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Switch product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		const int AliasResult = ScriptEngine->RegisterTypedef("SelectorAlias", "int");
		ASSERT_THAT(IsTrue(AliasResult >= 0,
			*FString::Printf(TEXT("Switch product should register its raw selector alias. Result=%d Messages={%s}"),
				AliasResult, *Engine.GetMessagesText())));

		for (const FNamedCase& SelectorCase : SelectorCases)
		{
			for (const FNamedCase& CaseCase : CaseCases)
			{
				for (const FNamedCase& ExitCase : ExitCases)
				{
					const FNativeCaseContext Case(MakeNativeCaseId("LANG-CF-SWITCH",
						{ ANSI_TO_TCHAR(SelectorCase.CatalogName), ANSI_TO_TCHAR(CaseCase.CatalogName), ANSI_TO_TCHAR(ExitCase.CatalogName) }));
					const FString ModuleName = TEXT("Switch_") + Case.GetId().RightChop(15).Replace(TEXT("-"), TEXT("_"));
					const FString Source = BuildSwitchSource(SelectorCase, CaseCase, ExitCase);
					PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);
					const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
					const FTCHARToUTF8 SourceUtf8(*Source);
					Engine.ResetMessages();
					asIScriptModule* Module = nullptr;
					const int BuildResult = CompileNativeModule(ScriptEngine, ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
					// The current fork requires default to be the final case label. A
					// default/no-match fallthrough combination therefore remains an
					// intentional negative product instead of being silently omitted.
					const bool bInvalid = SelectorCase.bInvalid
						|| CaseCase.bInvalid
						|| ((IsCase(CaseCase, "default") || IsCase(CaseCase, "no_match"))
							&& IsCase(ExitCase, "fallthrough"));
					if (bInvalid)
					{
						ASSERT_THAT(IsTrue(BuildResult < 0, *Case.Describe(TEXT("invalid switch cell should be rejected"))));
						ASSERT_THAT(IsTrue(Engine.GetMessages().Entries.Num() > 0, *Case.Describe(TEXT("invalid switch cell should report a diagnostic"))));
					}
					else
					{
						const FSwitchExpectation Expected = ExpectedResult(SelectorCase, CaseCase, ExitCase);
						ASSERT_THAT(AreEqual(asSUCCESS, BuildResult,
							*FString::Printf(TEXT("%s switch cell should compile. BuildResult=%d Messages={%s}"),
								*Case.GetId(), BuildResult, *Engine.GetMessagesText())));
						asIScriptFunction* const Entry = GetNativeFunctionByExactDecl(Module, "int Entry()");
						ASSERT_THAT(IsNotNull(Entry, *Case.Describe(TEXT("switch cell should publish exact Entry declaration"))));
						if (Entry != nullptr)
						{
							asIScriptContext* const Context = ScriptEngine->CreateContext();
							ASSERT_THAT(IsNotNull(Context, *Case.Describe(TEXT("switch cell should create a context"))));
							if (Context != nullptr)
							{
								ASSERT_THAT(AreEqual(Expected.bThrows ? asEXECUTION_EXCEPTION : asEXECUTION_FINISHED, PrepareAndExecute(Context, Entry),
									*Case.Describe(TEXT("switch execution should match selected exit"))));
								if (Expected.bThrows)
								{
									ASSERT_THAT(AreEqual(FString(TEXT("Divide by zero")), FString(UTF8_TO_TCHAR(Context->GetExceptionString())),
										*Case.Describe(TEXT("switch exception path should preserve the fork's exact divide-by-zero text"))));
								}
								else
								{
									const int32 ActualReturnValue = static_cast<int32>(Context->GetReturnDWord());
									ASSERT_THAT(AreEqual(Expected.ReturnValue, ActualReturnValue,
										*FString::Printf(TEXT("%s switch should return the exact selected trace. Expected=%d Actual=%d"),
											*Case.GetId(), Expected.ReturnValue, ActualReturnValue)));
								}
								asUINT BytecodeLength = 0;
								Entry->GetByteCode(&BytecodeLength);
								ASSERT_THAT(IsTrue(BytecodeLength > 0, *Case.Describe(TEXT("switch entry should expose bytecode"))));
								Context->Release();
							}
						}
					}
					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS), *Case.Describe(TEXT("switch cell should discard its module"))));
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
