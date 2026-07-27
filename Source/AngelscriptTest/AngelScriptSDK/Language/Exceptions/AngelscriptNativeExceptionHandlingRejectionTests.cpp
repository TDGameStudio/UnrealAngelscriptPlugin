#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

using AngelscriptNativeTestSupport::AppendGeneratedAsLine;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FExceptionHandlingRejectionTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Exceptions.HandlingRejection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FNamedCase
	{
		const ANSICHAR* CatalogName;
	};

	inline static constexpr FNamedCase FeatureCases[] =
	{
		{ "try_catch" },
		{ "try_without_catch" },
		{ "catch_without_try" },
		{ "rethrow" },
	};

	inline static constexpr FNamedCase PlacementCases[] =
	{
		{ "entry_body" },
		{ "nested_function" },
		{ "after_valid_declaration" },
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

	static void AppendInvalidFeature(FString& Source, const FNamedCase& FeatureCase)
	{
		if (IsCase(FeatureCase, "try_catch"))
		{
			AppendGeneratedAsLine(Source, TEXT("\ttry"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\tcatch"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else if (IsCase(FeatureCase, "try_without_catch"))
		{
			AppendGeneratedAsLine(Source, TEXT("\ttry"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else if (IsCase(FeatureCase, "catch_without_try"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tcatch"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\tthrow;"));
		}
	}

	static void AppendInvalidFunction(FString& Source, const FNamedCase& FeatureCase, const FNamedCase& PlacementCase)
	{
		if (IsCase(PlacementCase, "nested_function"))
		{
			AppendGeneratedAsLine(Source, TEXT("void Inner()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendInvalidFeature(Source, FeatureCase);
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("void Entry()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tInner();"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			return;
		}

		if (IsCase(PlacementCase, "after_valid_declaration"))
		{
			AppendGeneratedAsLine(Source, TEXT("void Before()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}

		AppendGeneratedAsLine(Source, TEXT("void Entry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendInvalidFeature(Source, FeatureCase);
		AppendGeneratedAsLine(Source, TEXT("}"));
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
		const FNamedCase& FeatureCase,
		const FNamedCase& PlacementCase,
		const FNamedCase& LineEndingCase)
	{
		FString Source;
		AppendInvalidFunction(Source, FeatureCase, PlacementCase);
		return ApplyLineEnding(Source, LineEndingCase);
	}

	static FString BuildRecoverySource(const FNamedCase& LineEndingCase)
	{
		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int Entry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 42;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return ApplyLineEnding(Source, LineEndingCase);
	}

	static FString MakeModuleName(const AngelscriptNativeTestSupport::FNativeCaseContext& Case)
	{
		return TEXT("ExceptionHandlingRejection_") + Case.GetId().Replace(TEXT("-"), TEXT("_"));
	}

public:
	TEST_METHOD(UnsupportedHandlersByPlacementAndLineEnding)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-EX-HANDLER-REJECTION",
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
		if (!Assertions.IsNotNull(ScriptEngine, TEXT("Exception handler rejection product should create a raw SDK engine")))
		{
			return;
		}

		for (const FNamedCase& FeatureCase : FeatureCases)
		{
			for (const FNamedCase& PlacementCase : PlacementCases)
			{
				for (const FNamedCase& LineEndingCase : LineEndingCases)
				{
					const FNativeCaseContext Case(MakeNativeCaseId("LANG-EX-HANDLER-REJECTION",
						{ ANSI_TO_TCHAR(FeatureCase.CatalogName), ANSI_TO_TCHAR(PlacementCase.CatalogName), ANSI_TO_TCHAR(LineEndingCase.CatalogName) }));
					const FString ModuleName = MakeModuleName(Case);
					const FString InvalidSource = BuildInvalidSource(FeatureCase, PlacementCase, LineEndingCase);
					PrintGeneratedAsSource(*TestRunner, Case.GetId() + TEXT("-INVALID"), ModuleName, InvalidSource);

					const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
					const FTCHARToUTF8 InvalidSourceUtf8(*InvalidSource);
					Engine.ResetMessages();
					asIScriptModule* InvalidModule = nullptr;
					const int InvalidBuildResult = CompileNativeModule(ScriptEngine, ModuleNameUtf8.Get(), InvalidSourceUtf8.Get(), InvalidModule);
					(void)Assertions.IsTrue(InvalidBuildResult < 0,
						*Case.Describe(TEXT("current fork should reject the unsupported exception-handling syntax")));
					(void)Assertions.IsTrue(Engine.GetMessages().Entries.Num() > 0,
						*Case.Describe(TEXT("unsupported exception-handling syntax should retain a compiler diagnostic")));
					(void)Assertions.IsNull(GetNativeFunctionByExactDecl(InvalidModule, "void Entry()"),
						*Case.Describe(TEXT("rejected exception-handling source should not publish Entry")));

					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					(void)Assertions.IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
						*Case.Describe(TEXT("rejected exception-handling module should be discardable")));

					const FString RecoverySource = BuildRecoverySource(LineEndingCase);
					PrintGeneratedAsSource(*TestRunner, Case.GetId() + TEXT("-RECOVERY"), ModuleName, RecoverySource);
					const FTCHARToUTF8 RecoverySourceUtf8(*RecoverySource);
					Engine.ResetMessages();
					asIScriptModule* RecoveryModule = nullptr;
					const int RecoveryBuildResult = CompileNativeModule(ScriptEngine, ModuleNameUtf8.Get(), RecoverySourceUtf8.Get(), RecoveryModule);
					const bool bRecoveryBuilt = Assertions.AreEqual(asSUCCESS, RecoveryBuildResult,
						*FString::Printf(TEXT("%s; result=%d messages=%s"), *Case.Describe(TEXT("same-name recovery should compile after rejected syntax")), RecoveryBuildResult, *Engine.GetMessagesText()));
					asIScriptFunction* const RecoveryEntry = GetNativeFunctionByExactDecl(RecoveryModule, "int Entry()");
					const bool bRecoveryEntryAvailable = Assertions.IsNotNull(RecoveryEntry,
						*Case.Describe(TEXT("same-name recovery should publish the exact Entry declaration")));
					if (bRecoveryBuilt && bRecoveryEntryAvailable)
					{
						asIScriptContext* const Context = ScriptEngine->CreateContext();
						const bool bContextAvailable = Assertions.IsNotNull(Context,
							*Case.Describe(TEXT("same-name recovery should create a context")));
						if (bContextAvailable)
						{
							(void)Assertions.AreEqual(asEXECUTION_FINISHED, PrepareAndExecute(Context, RecoveryEntry),
								*Case.Describe(TEXT("same-name recovery should execute normally")));
							(void)Assertions.AreEqual(42, static_cast<int32>(Context->GetReturnDWord()),
								*Case.Describe(TEXT("same-name recovery should return its independent sentinel")));
							(void)Assertions.AreEqual(asSUCCESS, Context->Unprepare(),
								*Case.Describe(TEXT("same-name recovery context should unprepare cleanly")));
							Context->Release();
						}
					}

					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					(void)Assertions.IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
						*Case.Describe(TEXT("recovered exception-handling module should discard cleanly")));
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
