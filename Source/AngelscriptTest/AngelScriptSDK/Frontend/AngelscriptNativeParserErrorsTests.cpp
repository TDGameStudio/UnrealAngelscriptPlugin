#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeCaseTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

// Parser error recovery coverage.
#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_builder.h"
#include "source/as_module.h"
#include "source/as_parser.h"
#include "source/as_scriptcode.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptnode.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FParserErrorsTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.Parser.Errors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static int ParseScriptWithResult(asCScriptEngine* ScriptEngine, const char* ModuleName, const char* Source)
	{
		using namespace AngelscriptNativeTestSupport;

		asCModule* Module = CreateSdkModule(ScriptEngine, ModuleName);
		asCBuilder Builder(ScriptEngine, Module);
		Builder.silent = true;

		asCScriptCode Code;
		Code.SetCode(ModuleName, Source, true);

		AngelscriptNativeTestSupport::FParserAccessor Parser(&Builder);
		return Parser.ParseScript(&Code);
	}

	static bool DiscardParserModule(
		FNoDiscardAsserter& Assert,
		asCScriptEngine* ScriptEngine,
		const char* ModuleName,
		const FString& Context)
	{
		const int DiscardResult = ScriptEngine->DiscardModule(ModuleName);
		const bool bDiscarded = Assert.AreEqual(
			asSUCCESS,
			DiscardResult,
			FString::Printf(TEXT("%s should discard its parser module"), *Context));
		const bool bLookupCleared = Assert.IsNull(
			ScriptEngine->GetModule(ModuleName, asGM_ONLY_IF_EXISTS),
			FString::Printf(TEXT("%s should leave no name-visible parser module"), *Context));
		return bDiscarded && bLookupCleared;
	}

public:
	TEST_METHOD(ParseStageClassificationByMalformedShape)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("FRONTEND-PARSER-MALFORMED-STAGE-CLASSIFICATION",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		struct FParseCase
		{
			const TCHAR* Id;
			const TCHAR* Source;
			bool bExpectedParserSuccess;
		};
		const FParseCase Cases[] =
		{
			{ TEXT("missing_semicolon"), TEXT("int A = 1 int B = 2;"), true },
			{ TEXT("unbalanced_braces"), TEXT("class FBroken { void Run() { }"), false },
			{ TEXT("unclosed_string"), TEXT("const string Name = \"unterminated;"), false },
			{ TEXT("bad_operator_sequence"), TEXT("int Read() { return 1 + * 2; }"), true },
			{ TEXT("bad_parameter_list"), TEXT("void Bad(int A,, int B) { }"), false },
			{ TEXT("multiple_malformed"), TEXT("void Bad( { }\nclass FBroken { int ; }\nint A = ;"), false },
		};

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("Malformed parser-stage product should use the class-owned raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		int32 ObservedCells = 0;
		for (const FParseCase& Case : Cases)
		{
			const FString Source = Case.Source;
			const FString SourceId = FString::Printf(
				TEXT("FRONTEND-PARSER-MALFORMED-STAGE-CLASSIFICATION-%s"),
				Case.Id);
			const FString ModuleName = FString::Printf(
				TEXT("ParserMalformedStage_%s"),
				Case.Id);
			PrintGeneratedAsSource(*TestRunner, SourceId, ModuleName, Source);

			const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
			const FTCHARToUTF8 SourceUtf8(*Source);
			const int ParseResult = ParseScriptWithResult(
				ScriptEngine,
				ModuleNameUtf8.Get(),
				SourceUtf8.Get());
			if (Case.bExpectedParserSuccess)
			{
				ASSERT_THAT(AreEqual(0, ParseResult,
					FString::Printf(TEXT("%s should retain the current fork parser-stage acceptance"), *SourceId)));
			}
			else
			{
				ASSERT_THAT(IsTrue(ParseResult < 0,
					FString::Printf(TEXT("%s should fail at parser stage"), *SourceId)));
			}
			ASSERT_THAT(IsTrue(DiscardParserModule(
				this->Assert,
				ScriptEngine,
				ModuleNameUtf8.Get(),
				SourceId),
				FString::Printf(TEXT("%s should release its malformed parser state"), *SourceId)));
			++ObservedCells;
		}

		ASSERT_THAT(AreEqual(6, ObservedCells,
			TEXT("Malformed parser-stage product should execute every source shape")));

		const FString ControlSourceId = TEXT("FRONTEND-PARSER-MALFORMED-STAGE-CLASSIFICATION-isolation-control");
		const FString ControlModuleName = TEXT("ParserMalformedStageIsolationControl");
		const FString ControlSource = TEXT("int Control() { return 5; }");
		PrintGeneratedAsSource(*TestRunner, ControlSourceId, ControlModuleName, ControlSource);
		const FTCHARToUTF8 ControlModuleNameUtf8(*ControlModuleName);
		const FTCHARToUTF8 ControlSourceUtf8(*ControlSource);
		ASSERT_THAT(AreEqual(
			0,
			ParseScriptWithResult(ScriptEngine, ControlModuleNameUtf8.Get(), ControlSourceUtf8.Get()),
			TEXT("Malformed parser-stage isolation control should parse after every rejected and recovered shape")));
		ASSERT_THAT(IsTrue(DiscardParserModule(
			this->Assert,
			ScriptEngine,
			ControlModuleNameUtf8.Get(),
			ControlSourceId),
			TEXT("Malformed parser-stage isolation control should release its independent state")));
	}

	TEST_METHOD(ResetRecoveryByMalformedShapeAndParserLifetime)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("FRONTEND-PARSER-RESET-RECOVERY",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		struct FRecoveryCase
		{
			const TCHAR* Id;
			const TCHAR* InvalidSource;
		};
		const FRecoveryCase Cases[] =
		{
			{ TEXT("unbalanced_braces"), TEXT("class FBroken { void Run() { }") },
			{ TEXT("unclosed_string"), TEXT("const string Name = \"unterminated;") },
			{ TEXT("bad_parameter_list"), TEXT("void Bad(int A,, int B) { }") },
			{ TEXT("multiple_malformed"), TEXT("void Bad( { }\nclass FBroken { int ; }\nint A = ;") },
		};
		const TCHAR* RecoveryModeIds[] =
		{
			TEXT("same_parser_reset"),
			TEXT("fresh_parser"),
		};
		const FString ValidSource = TEXT("int Recovered() { return 7; }");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("Parser recovery product should use the class-owned raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		int32 ObservedCells = 0;
		for (const FRecoveryCase& Case : Cases)
		{
			for (int32 RecoveryModeIndex = 0; RecoveryModeIndex < UE_ARRAY_COUNT(RecoveryModeIds); ++RecoveryModeIndex)
			{
				const FString SourceId = FString::Printf(
					TEXT("FRONTEND-PARSER-RESET-RECOVERY-%s-%s"),
					Case.Id,
					RecoveryModeIds[RecoveryModeIndex]);
				const FString ModuleName = FString::Printf(
					TEXT("ParserRecovery_%s_%s"),
					Case.Id,
					RecoveryModeIds[RecoveryModeIndex]);
				const FString InvalidSource = Case.InvalidSource;
				PrintGeneratedAsSource(*TestRunner, SourceId, ModuleName, InvalidSource);

				const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
				const FTCHARToUTF8 InvalidSourceUtf8(*InvalidSource);
				const FTCHARToUTF8 ValidSourceUtf8(*ValidSource);
				if (RecoveryModeIndex == 0)
				{
					asCModule* Module = CreateSdkModule(ScriptEngine, ModuleNameUtf8.Get());
					ASSERT_THAT(IsNotNull(Module,
						FString::Printf(TEXT("%s should create a parser module"), *SourceId)));
					if (Module == nullptr)
					{
						continue;
					}

					{
						asCBuilder Builder(ScriptEngine, Module);
						Builder.silent = true;
						FParserAccessor Parser(&Builder);

						asCScriptCode InvalidCode;
						InvalidCode.SetCode(ModuleNameUtf8.Get(), InvalidSourceUtf8.Get(), true);
						ASSERT_THAT(IsTrue(Parser.ParseScript(&InvalidCode) < 0,
							FString::Printf(TEXT("%s should fail its invalid pass"), *SourceId)));

						Parser.ResetParser();

						asCScriptCode ValidCode;
						ValidCode.SetCode(ModuleNameUtf8.Get(), ValidSourceUtf8.Get(), true);
						ASSERT_THAT(AreEqual(0, Parser.ParseScript(&ValidCode),
							FString::Printf(TEXT("%s should parse valid source after Reset"), *SourceId)));
						ASSERT_THAT(IsNotNull(
							Parser.GetScriptNode(),
							FString::Printf(TEXT("%s should publish only the recovered valid tree"), *SourceId)));
					}
					ASSERT_THAT(IsTrue(DiscardParserModule(
						this->Assert,
						ScriptEngine,
						ModuleNameUtf8.Get(),
						SourceId),
						FString::Printf(TEXT("%s should release its reset parser state"), *SourceId)));
				}
				else
				{
					const FString InvalidModuleName = ModuleName + TEXT("_Invalid");
					const FString ValidModuleName = ModuleName + TEXT("_Valid");
					const FTCHARToUTF8 InvalidModuleNameUtf8(*InvalidModuleName);
					const FTCHARToUTF8 ValidModuleNameUtf8(*ValidModuleName);
					ASSERT_THAT(IsTrue(ParseScriptWithResult(
						ScriptEngine,
						InvalidModuleNameUtf8.Get(),
						InvalidSourceUtf8.Get()) < 0,
						FString::Printf(TEXT("%s should fail its invalid fresh-parser pass"), *SourceId)));
					ASSERT_THAT(AreEqual(0, ParseScriptWithResult(
						ScriptEngine,
						ValidModuleNameUtf8.Get(),
						ValidSourceUtf8.Get()),
						FString::Printf(TEXT("%s should isolate the valid fresh parser"), *SourceId)));
					ASSERT_THAT(IsTrue(DiscardParserModule(
						this->Assert,
						ScriptEngine,
						InvalidModuleNameUtf8.Get(),
						SourceId + TEXT(" invalid module")),
						FString::Printf(TEXT("%s should release its invalid fresh-parser module"), *SourceId)));
					ASSERT_THAT(IsTrue(DiscardParserModule(
						this->Assert,
						ScriptEngine,
						ValidModuleNameUtf8.Get(),
						SourceId + TEXT(" valid module")),
						FString::Printf(TEXT("%s should release its valid fresh-parser module"), *SourceId)));
				}
				++ObservedCells;
			}
		}

		ASSERT_THAT(AreEqual(8, ObservedCells,
			TEXT("Parser recovery product should execute every malformed shape and parser-lifetime cell")));

		const FString ControlSourceId = TEXT("FRONTEND-PARSER-RESET-RECOVERY-isolation-control");
		const FString ControlModuleName = TEXT("ParserRecoveryIsolationControl");
		const FString ControlSource = TEXT("int Control() { return 11; }");
		PrintGeneratedAsSource(*TestRunner, ControlSourceId, ControlModuleName, ControlSource);
		const FTCHARToUTF8 ControlModuleNameUtf8(*ControlModuleName);
		const FTCHARToUTF8 ControlSourceUtf8(*ControlSource);
		ASSERT_THAT(AreEqual(
			0,
			ParseScriptWithResult(ScriptEngine, ControlModuleNameUtf8.Get(), ControlSourceUtf8.Get()),
			TEXT("Parser recovery isolation control should parse after every malformed/recovery cell")));
		ASSERT_THAT(IsTrue(DiscardParserModule(
			this->Assert,
			ScriptEngine,
			ControlModuleNameUtf8.Get(),
			ControlSourceId),
			TEXT("Parser recovery isolation control should release its independent state")));
	}

	TEST_METHOD(MissingSemicolonRecovers)
	{
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained missing-semicolon parser-stage smoke; FRONTEND-PARSER-MALFORMED-STAGE-CLASSIFICATION owns every malformed source shape and exact current-fork parse-stage outcome.");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asCScriptEngine* BareEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser error test should use the case-owned raw SDK engine")));
		if (BareEngine == nullptr)
		{
			return;
		}

		const int ParseResult = ParseScriptWithResult(BareEngine, "ParserErrorMissingSemicolon", "int A = 1 int B = 2;");
		ASSERT_THAT(AreEqual(0, ParseResult,
			TEXT("Parser currently accepts adjacent declarations without an explicit semicolon error in this recovery path")));
	}

	TEST_METHOD(UnbalancedBracesError)
	{
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained unbalanced-brace parser smoke; FRONTEND-PARSER-MALFORMED-STAGE-CLASSIFICATION and FRONTEND-PARSER-RESET-RECOVERY own rejection and recovery lifetimes.");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asCScriptEngine* BareEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser error test should use the case-owned raw SDK engine")));
		if (BareEngine == nullptr)
		{
			return;
		}

		const int ParseResult = ParseScriptWithResult(BareEngine, "ParserErrorUnbalancedBraces", "class FBroken { void Run() { }");
		ASSERT_THAT(IsTrue(ParseResult < 0, TEXT("Unbalanced braces should fail parser validation")));
	}

	TEST_METHOD(UnclosedStringInDeclaration)
	{
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained unclosed-string parser smoke; FRONTEND-PARSER-MALFORMED-STAGE-CLASSIFICATION and FRONTEND-PARSER-RESET-RECOVERY own rejection and recovery lifetimes.");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asCScriptEngine* BareEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser error test should use the case-owned raw SDK engine")));
		if (BareEngine == nullptr)
		{
			return;
		}

		const int ParseResult = ParseScriptWithResult(BareEngine, "ParserErrorUnclosedString", "const string Name = \"unterminated;");
		ASSERT_THAT(IsTrue(ParseResult < 0, TEXT("Unclosed string should fail parser validation")));
	}

	TEST_METHOD(BadOperatorSequenceError)
	{
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained bad-operator parser-stage smoke; FRONTEND-PARSER-MALFORMED-STAGE-CLASSIFICATION owns exact current-fork acceptance and rejection by malformed shape.");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asCScriptEngine* BareEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser error test should use the case-owned raw SDK engine")));
		if (BareEngine == nullptr)
		{
			return;
		}

		const int ParseResult = ParseScriptWithResult(BareEngine, "ParserErrorBadOperatorSequence", "int Read() { return 1 + * 2; }");
		ASSERT_THAT(AreEqual(0, ParseResult,
			TEXT("Parser currently accepts this operator sequence at syntax-tree construction time")));
	}

	TEST_METHOD(BadParameterListError)
	{
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained bad-parameter-list parser smoke; FRONTEND-PARSER-MALFORMED-STAGE-CLASSIFICATION and FRONTEND-PARSER-RESET-RECOVERY own rejection and recovery lifetimes.");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asCScriptEngine* BareEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser error test should use the case-owned raw SDK engine")));
		if (BareEngine == nullptr)
		{
			return;
		}

		const int ParseResult = ParseScriptWithResult(BareEngine, "ParserErrorBadParameterList", "void Bad(int A,, int B) { }");
		ASSERT_THAT(IsTrue(ParseResult < 0, TEXT("Bad parameter list should fail parser validation")));
	}

	TEST_METHOD(ResetClearsErrorState)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained single reset-after-error smoke; FRONTEND-PARSER-RESET-RECOVERY owns malformed shape and same-parser versus fresh-parser recovery combinations.");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asCScriptEngine* BareEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser error test should use the case-owned raw SDK engine")));
		if (BareEngine == nullptr)
		{
			return;
		}

		asCModule* Module = CreateSdkModule(BareEngine, "ParserErrorReset");
		ASSERT_THAT(IsNotNull(Module, TEXT("Parser reset test should create a module")));

		asCBuilder Builder(BareEngine, Module);
		Builder.silent = true;
		AngelscriptNativeTestSupport::FParserAccessor Parser(&Builder);

		asCScriptCode InvalidCode;
		InvalidCode.SetCode("ParserErrorResetInvalid", "void Broken( { }", true);
		ASSERT_THAT(IsTrue(Parser.ParseScript(&InvalidCode) < 0, TEXT("Invalid parser pass should fail")));

		Parser.ResetParser();

		asCScriptCode ValidCode;
		ValidCode.SetCode("ParserErrorResetValid", "void Fixed() { }", true);
		ASSERT_THAT(AreEqual(0, Parser.ParseScript(&ValidCode),
			TEXT("Parser should accept valid input after explicit Reset")));
	}

	TEST_METHOD(MultipleErrorsAccumulated)
	{
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"Retained multiple-malformed parser smoke; FRONTEND-PARSER-MALFORMED-STAGE-CLASSIFICATION and FRONTEND-PARSER-RESET-RECOVERY own rejection and recovery lifetimes.");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asCScriptEngine* BareEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(BareEngine, TEXT("Parser error test should use the case-owned raw SDK engine")));
		if (BareEngine == nullptr)
		{
			return;
		}

		const int ParseResult = ParseScriptWithResult(BareEngine, "ParserErrorMultiple", "void Bad( { }\nclass Broken { int ; }\nint A = ;");
		ASSERT_THAT(IsTrue(ParseResult < 0, TEXT("Multiple malformed declarations should fail parser validation")));
	}
};

#endif
