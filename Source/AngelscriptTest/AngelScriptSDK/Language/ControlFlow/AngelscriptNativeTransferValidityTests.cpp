#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"

using AngelscriptNativeTestSupport::AppendGeneratedAsLine;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTransferValidityTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.ControlFlow.TransferValidity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FNamedCase
	{
		const ANSICHAR* CatalogName;
	};

	inline static constexpr FNamedCase PlacementCases[] =
	{
		{ "function" },
		{ "branch" },
		{ "switch" },
		{ "loop" },
	};

	inline static constexpr FNamedCase TransferCases[] =
	{
		{ "break" },
		{ "continue" },
	};


	static bool IsCase(const FNamedCase& Case, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(Case.CatalogName, Name) == 0;
	}

	static bool IsAccepted(const FNamedCase& PlacementCase, const FNamedCase& TransferCase)
	{
		return IsCase(PlacementCase, "loop")
			|| (IsCase(PlacementCase, "switch") && IsCase(TransferCase, "break"));
	}

	static int32 ExpectedResult(const FNamedCase& PlacementCase, const FNamedCase& TransferCase)
	{
		if (IsCase(PlacementCase, "switch"))
		{
			return 3;
		}
		if (IsCase(PlacementCase, "loop") && IsCase(TransferCase, "break"))
		{
			return 3;
		}
		return 5;
	}

	static void AppendTransfer(FString& Source, const FNamedCase& TransferCase, const TCHAR* Indent)
	{
		AppendGeneratedAsLine(Source, FString(Indent) + (IsCase(TransferCase, "break") ? TEXT("break;") : TEXT("continue;")));
	}

	static FString BuildSource(const FNamedCase& PlacementCase, const FNamedCase& TransferCase)
	{
		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int Entry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));

		if (IsCase(PlacementCase, "function"))
		{
			AppendTransfer(Source, TransferCase, TEXT("\t"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 0;"));
		}
		else if (IsCase(PlacementCase, "branch"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tif (true)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendTransfer(Source, TransferCase, TEXT("\t\t"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 0;"));
		}
		else if (IsCase(PlacementCase, "switch"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tswitch (1)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\tcase 1:"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendTransfer(Source, TransferCase, TEXT("\t\t"));
			AppendGeneratedAsLine(Source, TEXT("\t\tbreak;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\tdefault:"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tbreak;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 3;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\tfor (int Index = 0; Index < 2; ++Index)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tif (Index == 0)"));
			AppendGeneratedAsLine(Source, TEXT("\t\t{"));
			AppendTransfer(Source, TransferCase, TEXT("\t\t\t"));
			AppendGeneratedAsLine(Source, TEXT("\t\t}"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 5;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 3;"));
		}

		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

public:
	TEST_METHOD(TransfersByOwningPlacementAndKind)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-CF-TRANSFER-VALIDITY",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Runtime
			| ENativeEvidence::Cleanup);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		FNoDiscardAsserter Assertions(*TestRunner);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (!Assertions.IsNotNull(ScriptEngine, TEXT("Transfer validity product should create a raw SDK engine")))
		{
			return;
		}

		int32 ObservedCaseCount = 0;
		for (const FNamedCase& PlacementCase : PlacementCases)
		{
			for (const FNamedCase& TransferCase : TransferCases)
			{
				++ObservedCaseCount;
				const FNativeCaseContext Case(MakeNativeCaseId(
					"LANG-CF-TRANSFER-VALIDITY",
					{ ANSI_TO_TCHAR(PlacementCase.CatalogName), ANSI_TO_TCHAR(TransferCase.CatalogName) }));
				const FString ModuleName = FString::Printf(
					TEXT("TransferValidity_%s_%s"),
					ANSI_TO_TCHAR(PlacementCase.CatalogName),
					ANSI_TO_TCHAR(TransferCase.CatalogName));
				const FString Source = BuildSource(PlacementCase, TransferCase);
				PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);

				const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
				const FTCHARToUTF8 SourceUtf8(*Source);
				Engine.ResetMessages();
				asIScriptModule* Module = nullptr;
				const int BuildResult = CompileNativeModule(ScriptEngine, ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
				const bool bAccepted = IsAccepted(PlacementCase, TransferCase);
				if (!bAccepted)
				{
					const FString Description = FString::Printf(
						TEXT("%s; result=%d messages=%s"),
						*Case.Describe(TEXT("transfer outside an owning loop or switch should be rejected")),
						BuildResult,
						*Engine.GetMessagesText());
					(void)Assertions.IsTrue(BuildResult < 0, *Description);
					(void)Assertions.IsTrue(Engine.GetMessages().Entries.Num() > 0,
						*Case.Describe(TEXT("invalid transfer placement should retain a diagnostic")));
					if (Module != nullptr)
					{
						(void)Assertions.IsNull(Module->GetFunctionByDecl("int Entry()"),
							*Case.Describe(TEXT("rejected transfer source should not publish executable Entry")));
					}
				}
				else
				{
					const FString Description = FString::Printf(
						TEXT("%s; result=%d messages=%s"),
						*Case.Describe(TEXT("valid transfer placement should compile")),
						BuildResult,
						*Engine.GetMessagesText());
					const bool bBuildSucceeded = Assertions.AreEqual(asSUCCESS, BuildResult, *Description);
					asIScriptFunction* const Entry = GetNativeFunctionByExactDecl(Module, "int Entry()");
					const bool bEntryAvailable = Assertions.IsNotNull(Entry,
						*Case.Describe(TEXT("valid transfer placement should publish exact Entry")));
					if (bBuildSucceeded && bEntryAvailable)
					{
						asIScriptContext* const Context = ScriptEngine->CreateContext();
						const bool bContextAvailable = Assertions.IsNotNull(Context,
							*Case.Describe(TEXT("valid transfer placement should create a context")));
						if (bContextAvailable)
						{
							const int ExecutionResult = PrepareAndExecute(Context, Entry);
							(void)Assertions.AreEqual(asEXECUTION_FINISHED, ExecutionResult,
								*Case.Describe(TEXT("valid transfer placement should execute")));
							(void)Assertions.AreEqual(ExpectedResult(PlacementCase, TransferCase),
								static_cast<int32>(Context->GetReturnDWord()),
								*Case.Describe(TEXT("valid transfer should target its owning construct")));
							Context->Release();
						}
					}
				}

				ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
				(void)Assertions.IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
					*Case.Describe(TEXT("transfer validity cell should discard its module")));
			}
		}

		(void)Assertions.AreEqual(8, ObservedCaseCount,
			TEXT("LANG-CF-TRANSFER-VALIDITY must execute every placement and transfer cell"));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
