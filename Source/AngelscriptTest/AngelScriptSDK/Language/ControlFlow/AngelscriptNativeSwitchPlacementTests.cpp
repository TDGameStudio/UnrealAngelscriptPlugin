#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

using AngelscriptNativeTestSupport::AppendGeneratedAsLine;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FSwitchPlacementTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.ControlFlow.SwitchPlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FPlacement
	{
		const ANSICHAR* Name;
	};

	struct FForm
	{
		const ANSICHAR* Name;
	};

	inline static constexpr FPlacement Placements[] =
	{
		{ "function" },
		{ "branch" },
		{ "loop" },
		{ "after_switch" },
	};

	inline static constexpr FForm Forms[] =
	{
		{ "case_outside" },
		{ "default_outside" },
		{ "duplicate_default" },
		{ "case_after_default" },
	};


	static bool IsForm(const FForm& Form, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(Form.Name, Name) == 0;
	}

	static bool IsOutsideForm(const FForm& Form)
	{
		return IsForm(Form, "case_outside") || IsForm(Form, "default_outside");
	}

	static void AppendValidSwitch(FString& Source)
	{
		AppendGeneratedAsLine(Source, TEXT("\tswitch (Value)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\tcase 0:"));
		AppendGeneratedAsLine(Source, TEXT("\t\tValue += 1;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tbreak;"));
		AppendGeneratedAsLine(Source, TEXT("\tdefault:"));
		AppendGeneratedAsLine(Source, TEXT("\t\tValue += 2;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tbreak;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
	}

	static void AppendInvalidLabels(FString& Source, const FForm& Form, const FString& Indent)
	{
		if (IsForm(Form, "case_outside"))
		{
			AppendGeneratedAsLine(Source, Indent + TEXT("case 1:"));
			AppendGeneratedAsLine(Source, Indent + TEXT("\tValue += 10;"));
		}
		else if (IsForm(Form, "default_outside"))
		{
			AppendGeneratedAsLine(Source, Indent + TEXT("default:"));
			AppendGeneratedAsLine(Source, Indent + TEXT("\tValue += 20;"));
		}
		else if (IsForm(Form, "duplicate_default"))
		{
			AppendGeneratedAsLine(Source, Indent + TEXT("default:"));
			AppendGeneratedAsLine(Source, Indent + TEXT("\tValue += 30;"));
			AppendGeneratedAsLine(Source, Indent + TEXT("default:"));
			AppendGeneratedAsLine(Source, Indent + TEXT("\tValue += 40;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, Indent + TEXT("default:"));
			AppendGeneratedAsLine(Source, Indent + TEXT("\tValue += 50;"));
			AppendGeneratedAsLine(Source, Indent + TEXT("case 1:"));
			AppendGeneratedAsLine(Source, Indent + TEXT("\tValue += 60;"));
		}
	}

	static FString BuildSource(const FPlacement& Placement, const FForm& Form)
	{
		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int Entry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));

		const bool bOutside = IsOutsideForm(Form);
		if (FCStringAnsi::Strcmp(Placement.Name, "function") == 0)
		{
			if (bOutside)
			{
				AppendInvalidLabels(Source, Form, TEXT("\t"));
			}
			else
			{
				AppendGeneratedAsLine(Source, TEXT("\tswitch (Value)"));
				AppendGeneratedAsLine(Source, TEXT("\t{"));
				AppendInvalidLabels(Source, Form, TEXT("\t"));
				AppendGeneratedAsLine(Source, TEXT("\t}"));
			}
		}
		else if (FCStringAnsi::Strcmp(Placement.Name, "branch") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("\tif (true)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			if (bOutside)
			{
				AppendInvalidLabels(Source, Form, TEXT("\t\t"));
			}
			else
			{
				AppendGeneratedAsLine(Source, TEXT("\t\tswitch (Value)"));
				AppendGeneratedAsLine(Source, TEXT("\t\t{"));
				AppendInvalidLabels(Source, Form, TEXT("\t\t"));
				AppendGeneratedAsLine(Source, TEXT("\t\t}"));
			}
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else if (FCStringAnsi::Strcmp(Placement.Name, "loop") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("\twhile (false)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			if (bOutside)
			{
				AppendInvalidLabels(Source, Form, TEXT("\t\t"));
			}
			else
			{
				AppendGeneratedAsLine(Source, TEXT("\t\tswitch (Value)"));
				AppendGeneratedAsLine(Source, TEXT("\t\t{"));
				AppendInvalidLabels(Source, Form, TEXT("\t\t"));
				AppendGeneratedAsLine(Source, TEXT("\t\t}"));
			}
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else
		{
			AppendValidSwitch(Source);
			if (bOutside)
			{
				AppendInvalidLabels(Source, Form, TEXT("\t"));
			}
			else
			{
				AppendGeneratedAsLine(Source, TEXT("\tswitch (Value)"));
				AppendGeneratedAsLine(Source, TEXT("\t{"));
				AppendInvalidLabels(Source, Form, TEXT("\t"));
				AppendGeneratedAsLine(Source, TEXT("\t}"));
			}
		}

		AppendGeneratedAsLine(Source, TEXT("\treturn Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

public:
	TEST_METHOD(InvalidCaseAndDefaultFormsByOwningPlacement)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-CF-SWITCH-PLACEMENT",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Cleanup);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Switch placement product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		for (const FPlacement& Placement : Placements)
		{
			for (const FForm& Form : Forms)
			{
				const FNativeCaseContext Case(MakeNativeCaseId("LANG-CF-SWITCH-PLACEMENT",
					{ ANSI_TO_TCHAR(Placement.Name), ANSI_TO_TCHAR(Form.Name) }));
				const FString ModuleName = TEXT("SwitchPlacement_") + Case.GetId().RightChop(24).Replace(TEXT("-"), TEXT("_"));
				const FString Source = BuildSource(Placement, Form);
				PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);
				const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
				const FTCHARToUTF8 SourceUtf8(*Source);
				Engine.ResetMessages();
				asIScriptModule* Module = nullptr;
				const int BuildResult = CompileNativeModule(ScriptEngine, ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
				ASSERT_THAT(IsTrue(BuildResult < 0,
					*Case.Describe(TEXT("case/default placement form should be rejected by the raw compiler"))));
				ASSERT_THAT(IsTrue(Engine.GetMessages().Entries.Num() > 0,
					*Case.Describe(TEXT("rejected case/default placement should retain a diagnostic"))));
				if (Module != nullptr)
				{
					ASSERT_THAT(IsNull(GetNativeFunctionByExactDecl(Module, "int Entry()"),
						*Case.Describe(TEXT("rejected placement should not publish an executable Entry"))));
				}
				ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
				ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
					*Case.Describe(TEXT("switch placement cell should discard its module"))));
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
