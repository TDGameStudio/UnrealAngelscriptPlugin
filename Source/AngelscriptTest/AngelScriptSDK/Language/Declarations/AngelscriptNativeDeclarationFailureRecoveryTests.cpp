#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

using AngelscriptNativeTestSupport::AppendGeneratedAsLine;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FDeclarationFailureRecoveryTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Declarations.FailureRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FNamedCase
	{
		const ANSICHAR* CatalogName;
	};

	inline static constexpr FNamedCase FormCases[] =
	{
		{ "unbalanced_class" },
		{ "bad_parameter_list" },
		{ "unclosed_function_body" },
		{ "missing_type_name" },
		{ "unexpected_handle" },
		{ "unknown_base" },
	};

	inline static constexpr FNamedCase PlacementCases[] =
	{
		{ "entry_body" },
		{ "after_valid_function" },
		{ "inside_namespace" },
	};

	inline static constexpr FNamedCase LineEndingCases[] =
	{
		{ "lf" },
		{ "crlf" },
	};


	static bool IsCase(const FNamedCase& Case, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(Case.CatalogName, Name) == 0;
	}

	static void AppendInvalidForm(FString& Source, const FNamedCase& FormCase)
	{
		if (IsCase(FormCase, "unbalanced_class"))
		{
			AppendGeneratedAsLine(Source, TEXT("class FBroken"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tvoid Run()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else if (IsCase(FormCase, "bad_parameter_list"))
		{
			AppendGeneratedAsLine(Source, TEXT("void Broken(int A,, int B)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (IsCase(FormCase, "unclosed_function_body"))
		{
			AppendGeneratedAsLine(Source, TEXT("int Broken()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 1;"));
		}
		else if (IsCase(FormCase, "missing_type_name"))
		{
			AppendGeneratedAsLine(Source, TEXT("int;"));
		}
		else if (IsCase(FormCase, "unexpected_handle"))
		{
			AppendGeneratedAsLine(Source, TEXT("int @Broken;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("class FBroken : MissingBase"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
	}

	static void AppendValidPrefix(FString& Source, const FNamedCase& PlacementCase)
	{
		if (IsCase(PlacementCase, "after_valid_function"))
		{
			AppendGeneratedAsLine(Source, TEXT("int Before()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 7;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (IsCase(PlacementCase, "inside_namespace"))
		{
			AppendGeneratedAsLine(Source, TEXT("namespace FailureNamespace"));
			AppendGeneratedAsLine(Source, TEXT("{"));
		}
	}

	static void AppendValidSuffix(FString& Source, const FNamedCase& PlacementCase)
	{
		if (IsCase(PlacementCase, "inside_namespace"))
		{
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
	}

	static FString ApplyLineEnding(const FString& Source, const FNamedCase& LineEndingCase)
	{
		if (IsCase(LineEndingCase, "lf"))
		{
			return Source;
		}

		FString Result;
		Result.Reserve(Source.Len() + 32);
		for (int32 Index = 0; Index < Source.Len(); ++Index)
		{
			const TCHAR Character = Source[Index];
			if (Character == TCHAR('\n'))
			{
				Result.AppendChar(TCHAR('\r'));
			}
			Result.AppendChar(Character);
		}
		return Result;
	}

	static FString BuildInvalidSource(
		const FNamedCase& FormCase,
		const FNamedCase& PlacementCase,
		const FNamedCase& LineEndingCase)
	{
		FString Source;
		AppendValidPrefix(Source, PlacementCase);
		AppendInvalidForm(Source, FormCase);
		AppendValidSuffix(Source, PlacementCase);
		return ApplyLineEnding(Source, LineEndingCase);
	}

	static FString BuildRecoverySource(const FNamedCase& LineEndingCase)
	{
		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int Entry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 23;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return ApplyLineEnding(Source, LineEndingCase);
	}

	static FString MakeModuleName(const AngelscriptNativeTestSupport::FNativeCaseContext& Case)
	{
		return TEXT("DeclarationFailureRecovery_") + Case.GetId().Replace(TEXT("-"), TEXT("_"));
	}

public:
	TEST_METHOD(MalformedDeclarationsRecoverAcrossPlacementAndLineEnding)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-DECL-FAILURE-RECOVERY",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		FNoDiscardAsserter Assertions(*TestRunner);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (!Assertions.IsNotNull(ScriptEngine, TEXT("Declaration failure product should create a raw SDK engine")))
		{
			return;
		}

		for (const FNamedCase& FormCase : FormCases)
		{
			for (const FNamedCase& PlacementCase : PlacementCases)
			{
				for (const FNamedCase& LineEndingCase : LineEndingCases)
				{
					const FNativeCaseContext Case(MakeNativeCaseId(
						"LANG-DECL-FAILURE-RECOVERY",
						{
							ANSI_TO_TCHAR(FormCase.CatalogName),
							ANSI_TO_TCHAR(PlacementCase.CatalogName),
							ANSI_TO_TCHAR(LineEndingCase.CatalogName),
						}));
					const FString ModuleName = MakeModuleName(Case);
					const FString InvalidSource = BuildInvalidSource(FormCase, PlacementCase, LineEndingCase);
					PrintGeneratedAsSource(*TestRunner, Case.GetId() + TEXT("-INVALID"), ModuleName, InvalidSource);

					const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
					const FTCHARToUTF8 InvalidSourceUtf8(*InvalidSource);
					Engine.ResetMessages();
					asIScriptModule* InvalidModule = nullptr;
					const int InvalidBuildResult = CompileNativeModule(
						ScriptEngine,
						ModuleNameUtf8.Get(),
						InvalidSourceUtf8.Get(),
						InvalidModule);
					(void)Assertions.IsTrue(InvalidBuildResult < 0,
						*Case.Describe(TEXT("malformed declaration should be rejected by the current parser/compiler")));
					(void)Assertions.IsTrue(Engine.GetMessages().Entries.Num() > 0,
						*Case.Describe(TEXT("malformed declaration should retain at least one diagnostic")));
					(void)Assertions.IsNull(GetNativeFunctionByExactDecl(InvalidModule, "int Entry()"),
						*Case.Describe(TEXT("failed declaration build should not publish the recovery Entry")));

					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					(void)Assertions.IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
						*Case.Describe(TEXT("failed declaration module should be discardable")));

					const FString RecoverySource = BuildRecoverySource(LineEndingCase);
					PrintGeneratedAsSource(*TestRunner, Case.GetId() + TEXT("-RECOVERY"), ModuleName, RecoverySource);
					const FTCHARToUTF8 RecoverySourceUtf8(*RecoverySource);
					Engine.ResetMessages();
					asIScriptModule* RecoveryModule = nullptr;
					const int RecoveryBuildResult = CompileNativeModule(
						ScriptEngine,
						ModuleNameUtf8.Get(),
						RecoverySourceUtf8.Get(),
						RecoveryModule);
					const bool bRecoveryBuilt = Assertions.AreEqual(
						asSUCCESS,
						RecoveryBuildResult,
						*Case.Describe(TEXT("same-name declaration recovery should compile")));
					const bool bRecoveryPublished = Assertions.IsNotNull(
						GetNativeFunctionByExactDecl(RecoveryModule, "int Entry()"),
						*Case.Describe(TEXT("same-name declaration recovery should publish Entry")));
					if (bRecoveryBuilt && bRecoveryPublished)
					{
						asIScriptContext* const Context = ScriptEngine->CreateContext();
						const bool bContextCreated = Assertions.IsNotNull(
							Context,
							*Case.Describe(TEXT("declaration recovery should create an execution context")));
						if (bContextCreated)
						{
							asIScriptFunction* const Entry = GetNativeFunctionByExactDecl(RecoveryModule, "int Entry()");
							(void)Assertions.AreEqual(
								asEXECUTION_FINISHED,
								PrepareAndExecute(Context, Entry),
								*Case.Describe(TEXT("declaration recovery Entry should execute")));
							(void)Assertions.AreEqual(
								23,
								static_cast<int32>(Context->GetReturnDWord()),
								*Case.Describe(TEXT("declaration recovery Entry should return its sentinel")));
							(void)Assertions.AreEqual(
								asSUCCESS,
								Context->Unprepare(),
								*Case.Describe(TEXT("declaration recovery context should unprepare cleanly")));
							Context->Release();
						}
					}

					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					(void)Assertions.IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
						*Case.Describe(TEXT("recovered declaration module should be discarded")));
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
