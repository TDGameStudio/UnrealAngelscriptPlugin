#include "AngelscriptNativeCaseTestSupport.h"
#include "AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptNativeDiagnosticTestSupport.h"
#include "AngelscriptNativeDebugTestSupport.h"
#include "AngelscriptNativeExecutionTestSupport.h"
#include "AngelscriptNativeFixtureTestSupport.h"
#include "AngelscriptNativeLifecycleTestSupport.h"

#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FNativeCaseSupportTests,
	"Angelscript.TestModule.AngelScriptSDK.Support.Cases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CoreTypeCatalogIsUniqueAndComplete)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT("Infrastructure",
			"Validates the deterministic test type catalog consumed by multiple semantic products; it is not an AngelScript language contract.");

		ASSERT_THAT(AreEqual(18, static_cast<int32>(UE_ARRAY_COUNT(NativeTypeCases)),
			TEXT("Native SDK case support should define every approved core value/reference/null category exactly once")));

		TSet<FString> Names;
		for (const FNativeTypeCase& TypeCase : NativeTypeCases)
		{
			const FString Name = UTF8_TO_TCHAR(TypeCase.CatalogName);
			ASSERT_THAT(IsFalse(Names.Contains(Name),
				*FString::Printf(TEXT("Native SDK type catalog should not duplicate '%s'"), *Name)));
			Names.Add(Name);
			ASSERT_THAT(IsTrue(TypeCase.ScriptType != nullptr && TypeCase.ScriptType[0] != '\0',
				*FString::Printf(TEXT("Native SDK type '%s' should have a script declaration"), *Name)));
		}
	}

	TEST_METHOD(CaseContextReportsIdDeclarationExpectedAndActual)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT("Infrastructure",
			"Validates per-case failure-message observability in the native SDK test harness rather than a core SDK behavior.");

		const FNativeCaseContext Case(TEXT("LANG-FN-PARAM-DIRECTION-INT-INOUT"));
		const FString Message = Case.DescribeResult("int Probe(int& inout)", TEXT("43"), TEXT("42"));
		ASSERT_THAT(IsTrue(Message.Contains(Case.GetId(), ESearchCase::CaseSensitive),
			TEXT("Per-case failure messages should include the stable case ID")));
		ASSERT_THAT(IsTrue(Message.Contains(TEXT("int Probe(int& inout)"), ESearchCase::CaseSensitive),
			TEXT("Per-case failure messages should include the exact declaration")));
		ASSERT_THAT(IsTrue(Message.Contains(TEXT("Expected='43'"), ESearchCase::CaseSensitive),
			TEXT("Per-case failure messages should include the expected value")));
		ASSERT_THAT(IsTrue(Message.Contains(TEXT("Actual='42'"), ESearchCase::CaseSensitive),
			TEXT("Per-case failure messages should include the actual value")));
	}

	TEST_METHOD(ExactDeclarationLookupRejectsNameOnlyAndAcceptsNormalizedConst)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT("Infrastructure",
			"Validates the suite exact-declaration lookup helper that guards semantic owners against ambiguous name-only execution.");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Select(int Value)
			{
				return Value + 1;
			}

			int Select(double Value)
			{
				return int(Value) + 2;
			}

			namespace Lookup
			{
				int Named(int Value)
				{
					return Value + 3;
				}
			}
			)AS");
		FScopedNativeModule Module(*TestRunner, Engine, "ExactDeclarationLookup", ScriptSource);
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("Exact declaration support test should compile its overload module")));
		if (!Module.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsNotNull(GetNativeFunctionByExactDecl(Module, "int Select(const int)"),
			TEXT("Exact declaration lookup should resolve the fork-normalized const integer parameter")));
		ASSERT_THAT(IsNotNull(GetNativeFunctionByExactDecl(Module, "int Lookup::Named(const int)"),
			TEXT("Exact declaration lookup should compare the complete namespace-inclusive declaration")));
		ASSERT_THAT(IsNull(GetNativeFunctionByExactDecl(Module, "Select"),
			TEXT("Exact declaration lookup should never accept a name-only request")));
		ASSERT_THAT(IsNull(GetNativeFunctionByExactDecl(Module, "int Select(const bool)"),
			TEXT("Exact declaration lookup should reject an unavailable overload")));
	}

	TEST_METHOD(DoubleSyntaxNormalizesToFloatAndUsesFloat64Accessors)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT("Infrastructure",
			"Validates the suite invoker ABI adapter for this fork's float64 mode; language conversion products own the public semantic contract.");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			double AddFraction(double Value)
			{
				return Value + 0.125;
			}
			)AS");
		FScopedNativeModule Module(*TestRunner, Engine, "DoubleBackedParameter", ScriptSource);
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("Double syntax normalization support test should compile")));
		if (!Module.IsValid())
		{
			return;
		}

		AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(
			*TestRunner,
			Engine.Get(),
			Module,
			"float AddFraction(const float)");
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("Double syntax normalization support test should resolve the current fork's exact float declaration")));
		if (Invoker.IsValid())
		{
			Invoker.AddArg(2.5);
			const double ActualValue = Invoker.CallAndReturn<double>(0.0);
			if (!FMath::IsNearlyEqual(2.625, ActualValue, 0.000001))
			{
				TestRunner->AddInfo(FString::Printf(
					TEXT("Double syntax normalization returned %.17g through the float64 ABI"),
					ActualValue));
			}
			ASSERT_THAT(IsNear(2.625, ActualValue, 0.000001,
				TEXT("Double syntax normalization support should use the fork's float64 argument and return ABI")));
		}
	}

	TEST_METHOD(ModuleScopeDiscardsIndependentCaseModule)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT("Infrastructure",
			"Validates RAII cleanup supplied by the native test harness; module lifecycle products own raw SDK module semantics.");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Module cleanup support test should create an engine")));

		{
			const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
				int IndependentEntry()
				{
					return 42;
				}
				)AS");
			FScopedNativeModule Module(*TestRunner, Engine, "IndependentCaseModule", ScriptSource);
			ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("Module cleanup support test should compile its independent case")));
		}

		ASSERT_THAT(IsNull(ScriptEngine->GetModule("IndependentCaseModule", asGM_ONLY_IF_EXISTS),
			TEXT("Module cleanup support should discard the case-owned module at scope exit")));
	}

	TEST_METHOD(DiagnosticMatcherRequiresAllStableFields)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT("Infrastructure",
			"Validates strict diagnostic comparison used by negative semantic products rather than compiler diagnostic behavior itself.");

		FNativeMessageCollector Messages;
		Messages.Entries.Add({ TEXT("BrokenSection"), 4, 7, asMSGTYPE_ERROR, TEXT("Expected expression") });
		const FExpectedNativeDiagnostic Expected
		{
			asINVALID_DECLARATION,
			asMSGTYPE_ERROR,
			TEXT("BrokenSection"),
			4,
			7,
			TEXT("Expected expression")
		};
		FString Difference;
		ASSERT_THAT(IsTrue(MatchesNativeDiagnostic(asINVALID_DECLARATION, Messages, Expected, Difference),
			TEXT("Diagnostic support should require and match return code, severity, section, row, column, and stable text")));

		FExpectedNativeDiagnostic WrongColumn = Expected;
		WrongColumn.Column = 8;
		ASSERT_THAT(IsFalse(MatchesNativeDiagnostic(asINVALID_DECLARATION, Messages, WrongColumn, Difference),
			TEXT("Diagnostic support should reject contradictory source columns")));
		ASSERT_THAT(IsTrue(Difference.Contains(TEXT("No diagnostic matched"), ESearchCase::CaseSensitive),
			TEXT("Diagnostic support should explain the exact unmatched contract")));
	}

	TEST_METHOD(LifecycleRecorderPreservesCopyAssignAndDestructOrder)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT("Infrastructure",
			"Validates the host-side lifecycle recorder oracle consumed by value and object lifetime products.");

		FNativeLifecycleRecorder Recorder;
		Recorder.Reset();
		{
			FNativeTrackedValue First(&Recorder, 41);
			FNativeTrackedValue Second(First);
			Second = First;
		}

		ASSERT_THAT(IsTrue(Recorder.HasExactEventOrder({
			ENativeLifecycleEvent::ValueConstruct,
			ENativeLifecycleEvent::CopyConstruct,
			ENativeLifecycleEvent::Assign,
			ENativeLifecycleEvent::Destruct,
			ENativeLifecycleEvent::Destruct }),
			TEXT("Lifecycle support should preserve exact construction, copy, assignment, and reverse destruction order")));
		ASSERT_THAT(AreEqual(0, Recorder.GetLiveObjectCount(),
			TEXT("Lifecycle support should return the live-object count to zero")));
	}

	TEST_METHOD(LocalCoreFixturesRegisterWithoutExternalAddons)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT("Infrastructure",
			"Validates local raw fixtures used by semantic products and enforces that the harness does not depend on external add-ons.");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Local native fixture support test should create an engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FNativeLifecycleRecorder Recorder;
		Recorder.Reset();
		ASSERT_THAT(IsTrue(RegisterNativeCaseValue(*ScriptEngine, Recorder),
			TEXT("Local native fixture support should register its value type and lifecycle behaviours")));
		ASSERT_THAT(IsTrue(RegisterNativeCaseReference(*ScriptEngine, &Recorder),
			TEXT("Local native fixture support should register its automatic-reference type and reference-count behaviours")));
		ASSERT_THAT(IsTrue(RegisterNativeScriptLifecycleBridge(*ScriptEngine, Recorder),
			TEXT("Local native fixture support should register script-owned lifecycle observation callbacks")));
		ASSERT_THAT(IsTrue(RegisterNativeCaseRange(*ScriptEngine, Recorder),
			TEXT("Local native fixture support should register its foreach protocol type")));
		ASSERT_THAT(IsNotNull(ScriptEngine->GetTypeInfoByName("FNativeCaseValue"),
			TEXT("Local native value fixture should be visible through raw type metadata")));
		ASSERT_THAT(IsNotNull(ScriptEngine->GetTypeInfoByName("FNativeCaseReference"),
			TEXT("Local native reference fixture should be visible through raw type metadata")));
		asIScriptFunction* const ReferenceFactory =
			GetNativeGlobalFunctionByPublishedDeclaration(
				ScriptEngine,
				"FNativeCaseReference CreateNativeCaseReference(int)");
		if (ReferenceFactory == nullptr)
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("Native reference factory lookup did not resolve its requested declaration; engine exposes {%s}"),
				*CollectGlobalFunctionDeclarations(ScriptEngine)));
		}
		ASSERT_THAT(IsNotNull(ReferenceFactory,
			TEXT("Local native reference fixture should expose its exact lifecycle-aware construction function")));
		asIScriptFunction* const BeginLifecycle =
			GetNativeGlobalFunctionByPublishedDeclaration(
				ScriptEngine,
				"int BeginNativeScriptLifecycle(int)");
		asIScriptFunction* const CopyLifecycle =
			GetNativeGlobalFunctionByPublishedDeclaration(
				ScriptEngine,
				"int CopyNativeScriptLifecycle(int, int)");
		asIScriptFunction* const AssignLifecycle =
			GetNativeGlobalFunctionByPublishedDeclaration(
				ScriptEngine,
				"void AssignNativeScriptLifecycle(int, int, int)");
		asIScriptFunction* const EndLifecycle =
			GetNativeGlobalFunctionByPublishedDeclaration(
				ScriptEngine,
				"void EndNativeScriptLifecycle(int, int)");
		if (BeginLifecycle == nullptr
			|| CopyLifecycle == nullptr
			|| AssignLifecycle == nullptr
			|| EndLifecycle == nullptr)
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("Native script-lifecycle bridge lookup did not resolve its normalized declarations; engine exposes {%s}"),
				*CollectGlobalFunctionDeclarations(ScriptEngine)));
		}
		ASSERT_THAT(IsNotNull(BeginLifecycle,
			TEXT("Script lifecycle bridge should expose its published construction callback declaration")));
		ASSERT_THAT(IsNotNull(CopyLifecycle,
			TEXT("Script lifecycle bridge should expose its published copy callback declaration")));
		ASSERT_THAT(IsNotNull(AssignLifecycle,
			TEXT("Script lifecycle bridge should expose its published assignment callback declaration")));
		ASSERT_THAT(IsNotNull(EndLifecycle,
			TEXT("Script lifecycle bridge should expose its published destruction callback declaration")));
		ASSERT_THAT(IsNotNull(ScriptEngine->GetTypeInfoByName("FNativeCaseRange"),
			TEXT("Local native foreach fixture should be visible through raw type metadata")));
		asITypeInfo* const RangeType = ScriptEngine->GetTypeInfoByName("FNativeCaseRange");
		ASSERT_THAT(IsTrue(RangeType != nullptr && RangeType->GetPropertyCount() == 1,
			TEXT("Local native foreach fixture should expose its Count setup property exactly once")));
	}

	TEST_METHOD(DebugRecorderPreservesInstructionEvidence)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_NON_PRODUCT("Infrastructure",
			"Validates the host-side debug observation recorder; Runtime.Debug products own the raw SDK callback semantics.");

		FNativeDebugRecorder Recorder;
		asSVMInstructionInfo Info;
		Info.Phase = asVM_AFTER_INSTRUCTION;
		Info.Instruction = 17;
		Info.InstructionName = "CALL";
		Info.BytecodeOffset = 23;
		Info.CallstackDepth = 3;
		CaptureNativeInstruction(nullptr, &Info, &Recorder);

		ASSERT_THAT(AreEqual(1, Recorder.Num(ENativeDebugEventKind::Instruction),
			TEXT("Native debug recorder should retain one instruction event")));
		const FNativeDebugEvent& Event = Recorder.GetEvents()[0];
		ASSERT_THAT(AreEqual(static_cast<int32>(asVM_AFTER_INSTRUCTION), static_cast<int32>(Event.InstructionPhase),
			TEXT("Native debug recorder should retain the instruction phase")));
		ASSERT_THAT(AreEqual(17, static_cast<int32>(Event.Instruction),
			TEXT("Native debug recorder should retain the opcode")));
		ASSERT_THAT(AreEqual(FString(TEXT("CALL")), Event.Text,
			TEXT("Native debug recorder should retain the instruction name")));
		ASSERT_THAT(AreEqual(23, Event.BytecodeOffset,
			TEXT("Native debug recorder should retain the bytecode offset")));
		ASSERT_THAT(AreEqual(3, static_cast<int32>(Event.CallstackDepth),
			TEXT("Native debug recorder should retain the callstack depth")));
	}

	TEST_METHOD(PreserveLinesHelperKeepsBlankLinesAndNewlineMode)
	{
		AS_NATIVE_NON_PRODUCT("Infrastructure",
			"Validates inline-source normalization used to preserve reviewable source layout and diagnostic line identity.");

		const FString LineFeed = FString::Chr(10);
		const FString CarriageReturnLineFeed = FString::Chr(13) + LineFeed;
		const FString VisualSource = CarriageReturnLineFeed
			+ TEXT("\tint First()") + CarriageReturnLineFeed
			+ TEXT("\t{") + CarriageReturnLineFeed
			+ TEXT("\t\treturn 1;") + CarriageReturnLineFeed
			+ TEXT("\t}") + CarriageReturnLineFeed
			+ CarriageReturnLineFeed
			+ TEXT("\tint Second()") + CarriageReturnLineFeed
			+ TEXT("\t{") + CarriageReturnLineFeed
			+ TEXT("\t\treturn 2;") + CarriageReturnLineFeed
			+ TEXT("\t}") + CarriageReturnLineFeed
			+ TEXT("\t");
		const FString Preserved = AngelscriptTest::NormalizeInlineASSourcePreserveLines(*VisualSource);

		ASSERT_THAT(IsTrue(Preserved.Contains(CarriageReturnLineFeed, ESearchCase::CaseSensitive),
			TEXT("Preserve-lines support should retain CRLF newline mode")));
		ASSERT_THAT(IsTrue(Preserved.Contains(CarriageReturnLineFeed + CarriageReturnLineFeed, ESearchCase::CaseSensitive),
			TEXT("Preserve-lines support should retain intentional blank lines")));
		ASSERT_THAT(IsTrue(Preserved.StartsWith(TEXT("int First()"), ESearchCase::CaseSensitive),
			TEXT("Preserve-lines support should remove only the raw-string envelope and common visual indentation")));
		ASSERT_THAT(IsTrue(Preserved.EndsWith(TEXT("}"), ESearchCase::CaseSensitive),
			TEXT("Preserve-lines support should remove the closing-delimiter indentation line")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
