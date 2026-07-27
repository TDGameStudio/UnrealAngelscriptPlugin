#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeDebugTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

using AngelscriptNativeTestSupport::AppendGeneratedAsLine;
using namespace AngelscriptNativeTestSupport;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FNativeExceptionCaughtQueryTests,
	"Angelscript.TestModule.AngelScriptSDK.Runtime.Debug.ExceptionCaughtQuery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:

	struct FDepthCase
	{
		const ANSICHAR* CatalogName;
		int32 NestedDepth;
	};

	inline static constexpr FDepthCase DepthCases[] =
	{
		{ "one", 1 },
		{ "two", 2 },
		{ "three", 3 },
		{ "five", 5 },
	};

	static FString FaultFunctionName()
	{
		return TEXT("CaughtQueryFaultLeaf");
	}

	static FString RootFunctionName()
	{
		return TEXT("CaughtQueryFaultRoot");
	}

	static FString RecoveryFunctionName()
	{
		return TEXT("CaughtQueryRecovery");
	}

	static bool DiscardAndConfirmAbsent(asIScriptEngine& ScriptEngine, const FTCHARToUTF8& ModuleNameUtf8)
	{
		ScriptEngine.DiscardModule(ModuleNameUtf8.Get());
		return ScriptEngine.GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS) == nullptr;
	}

	static FString BuildSource(const FDepthCase& DepthCase)
	{
		FString Source;
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("int %s()"), *FaultFunctionName()));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Zero = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 1 / Zero;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);

		for (int32 FrameIndex = 1; FrameIndex < DepthCase.NestedDepth; ++FrameIndex)
		{
			const FString FrameName = FString::Printf(TEXT("CaughtQueryFaultFrame%d"), FrameIndex);
			const FString CalleeName = FrameIndex == 1
				? FaultFunctionName()
				: FString::Printf(TEXT("CaughtQueryFaultFrame%d"), FrameIndex - 1);
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("int %s()"), *FrameName));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn %s();"), *CalleeName));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}

		const FString RootCalleeName = DepthCase.NestedDepth == 1
			? FaultFunctionName()
			: FString::Printf(TEXT("CaughtQueryFaultFrame%d"), DepthCase.NestedDepth - 1);
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("int %s()"), *RootFunctionName()));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn %s();"), *RootCalleeName));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);

		AppendGeneratedAsLine(Source, FString::Printf(TEXT("int %s()"), *RecoveryFunctionName()));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 77;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

public:
	TEST_METHOD(WillExceptionBeCaughtByNestedDepthAndContextReuse)
	{
		AS_NATIVE_PRODUCT("DBG-EXCEPTION-CAUGHT-QUERY",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Debug
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("WillExceptionBeCaught product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		for (const FDepthCase& DepthCase : DepthCases)
		{
			const FNativeCaseContext Case(MakeNativeCaseId(
				"DBG-EXCEPTION-CAUGHT-QUERY",
				{ ANSI_TO_TCHAR(DepthCase.CatalogName) }));
			const FString ModuleName = FString(TEXT("NativeExceptionCaughtQuery_"))
				+ FString(ANSI_TO_TCHAR(DepthCase.CatalogName));
			const FString Source = BuildSource(DepthCase);
			PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);

			const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
			const FTCHARToUTF8 SourceUtf8(*Source);
			asIScriptModule* Module = nullptr;
			const int BuildResult = CompileNativeModule(
				ScriptEngine,
				ModuleNameUtf8.Get(),
				SourceUtf8.Get(),
				Module);
			ASSERT_THAT(AreEqual(asSUCCESS, BuildResult,
				*Case.Describe(TEXT("uncaught-exception source should compile"))));
			ASSERT_THAT(IsNotNull(Module,
				*Case.Describe(TEXT("uncaught-exception source should publish a module"))));
			if (BuildResult < 0 || Module == nullptr)
			{
				ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
				continue;
			}

			asIScriptFunction* const FaultRoot = GetNativeFunctionByExactDecl(
				Module, "int CaughtQueryFaultRoot()");
			asIScriptFunction* const Recovery = GetNativeFunctionByExactDecl(
				Module, "int CaughtQueryRecovery()");
			ASSERT_THAT(IsNotNull(FaultRoot,
				*Case.Describe(TEXT("uncaught-exception root should resolve by exact declaration"))));
			ASSERT_THAT(IsNotNull(Recovery,
				*Case.Describe(TEXT("recovery function should resolve by exact declaration"))));

			asIScriptContext* const Context = ScriptEngine->CreateContext();
			ASSERT_THAT(IsNotNull(Context,
				*Case.Describe(TEXT("uncaught-exception query should create a context"))));
			if (Context != nullptr && FaultRoot != nullptr && Recovery != nullptr)
			{
				ON_SCOPE_EXIT
				{
					Context->Release();
				};

				ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(FaultRoot),
					*Case.Describe(TEXT("uncaught-exception root should prepare"))));
				ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), Context->Execute(),
					*Case.Describe(TEXT("uncaught-exception root should stop with an exception"))));
				asCContext* const RawContext = static_cast<asCContext*>(Context);
				ASSERT_THAT(IsFalse(RawContext->WillExceptionBeCaught(),
					*Case.Describe(TEXT("uncaught-exception context should report no script handler"))));
				ASSERT_THAT(AreEqual(FaultFunctionName(),
					FString(UTF8_TO_TCHAR(Context->GetExceptionFunction() != nullptr
						? Context->GetExceptionFunction()->GetName()
						: "")),
					*Case.Describe(TEXT("uncaught-exception query should retain the fault function"))));
				ASSERT_THAT(AreEqual(FString(TEXT("Divide by zero")),
					FString(UTF8_TO_TCHAR(Context->GetExceptionString() != nullptr
						? Context->GetExceptionString()
						: "")),
					*Case.Describe(TEXT("uncaught-exception query should retain the diagnostic text"))));
				ASSERT_THAT(IsTrue(Context->GetCallstackSize() >= static_cast<asUINT>(DepthCase.NestedDepth + 1),
					*Case.Describe(TEXT("uncaught-exception query should retain the generated nested stack"))));
				ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(),
					*Case.Describe(TEXT("uncaught-exception context should unprepare before reuse"))));
				ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Recovery),
					*Case.Describe(TEXT("same context should prepare a recovery function"))));
				ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(),
					*Case.Describe(TEXT("same context should execute normally after exception cleanup"))));
				ASSERT_THAT(AreEqual(77, static_cast<int32>(Context->GetReturnDWord()),
					*Case.Describe(TEXT("same context should preserve the recovery result"))));
				ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(),
					*Case.Describe(TEXT("recovery context should unprepare cleanly"))));
			}

			ASSERT_THAT(IsTrue(DiscardAndConfirmAbsent(*ScriptEngine, ModuleNameUtf8),
				*Case.Describe(TEXT("uncaught-exception module should be discarded after context reuse"))));
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
