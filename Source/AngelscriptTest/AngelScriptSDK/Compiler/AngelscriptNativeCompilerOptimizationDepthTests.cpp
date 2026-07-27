#include "Support/AngelscriptNativeCompilerBytecodeTestSupport.h"
#include "Support/AngelscriptNativeCoreTestSupport.h"
#include "Support/AngelscriptNativeLanguageCaseTestSupport.h"
#include "Support/AngelscriptNativeExecutionTestSupport.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCompilerOptimizationDepthPrivate
{
	inline void AppendLine(FString& Source, const TCHAR* Line = TEXT(""))
	{
		Source += Line;
		Source.AppendChar(TEXT('\n'));
	}

	inline FString MakeOptimizationSource()
	{
		FString Source;
		AppendLine(Source, TEXT("int BranchProbe(int Input)"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Local = Input;"));
		AppendLine(Source, TEXT("\tif (Local > 0)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn Local + 2;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\treturn Local - 2;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, TEXT("int Entry()"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn BranchProbe(40);"));
		AppendLine(Source, TEXT("}"));
		return Source;
	}
}

TEST_CLASS_WITH_FLAGS(FCompilerOptimizationDepthTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.Bytecode.OptimizationDepth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(OptimizationModesPreserveRuntimeSectionAndLocals)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("COMPILER-BYTECODE-OPTIMIZATION",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Bytecode
			| ENativeEvidence::Metadata
			| ENativeEvidence::Debug
			| ENativeEvidence::Cleanup);

		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Optimization depth test should create a standalone SDK engine")));

		const FString Source = AngelscriptCompilerOptimizationDepthPrivate::MakeOptimizationSource();
		for (int32 OptimizationMode = 0; OptimizationMode < 2; ++OptimizationMode)
		{
			const bool bOptimize = OptimizationMode == 1;
			const FString CaseId = FString::Printf(TEXT("COMPILER-BYTECODE-OPTIMIZATION-%s"), bOptimize ? TEXT("ON") : TEXT("OFF"));
			const FString ModuleName = FString::Printf(TEXT("CompilerOptimizationDepth_%s"), bOptimize ? TEXT("On") : TEXT("Off"));
			PrintGeneratedAsSource(*TestRunner, CaseId, ModuleName, Source);
			ASSERT_THAT(AreEqual(asSUCCESS, ScriptEngine->SetEngineProperty(asEP_OPTIMIZE_BYTECODE, bOptimize ? 1 : 0),
				TEXT("Optimization depth test should switch the raw engine optimization property")));

			FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
			FTCHARToUTF8 SourceUtf8(*Source);
			FScopedNativeModule Module(*TestRunner, Engine, ModuleNameUtf8.Get(), SourceUtf8.Get());
			ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("Optimization depth source should compile in both modes")));
			if (!Module.IsValid())
			{
				continue;
			}

			asIScriptFunction* const Entry = GetNativeFunctionByDecl(Module.Get(), "int Entry()");
			ASSERT_THAT(IsNotNull(Entry, TEXT("Optimization depth source should publish Entry")));
			asUINT BytecodeLength = 0;
			ASSERT_THAT(IsNotNull(Entry != nullptr ? Entry->GetByteCode(&BytecodeLength) : nullptr,
				TEXT("Optimization depth source should retain bytecode in both modes")));
			ASSERT_THAT(IsTrue(BytecodeLength > 0, TEXT("Optimization depth source should expose a non-empty bytecode stream")));
			ASSERT_THAT(IsNotNull(Entry != nullptr ? Entry->GetScriptSectionName() : nullptr,
				TEXT("Optimization depth source should retain its source section name")));
			ASSERT_THAT(IsTrue(Entry != nullptr && Entry->FindNextLineWithCode(11) > 0,
				TEXT("Optimization depth source should retain executable debug line markers")));

			int32 Result = 0;
			ASSERT_THAT(IsTrue(ExecuteScriptFunction(*TestRunner, ScriptEngine, Module.Get(), "int Entry()", Result),
				TEXT("Optimization depth source should execute in both modes")));
			ASSERT_THAT(AreEqual(42, Result, TEXT("Optimization on and off should preserve runtime behavior")));
			static constexpr asEBCInstr BranchOpcodes[] = { asBC_JMP, asBC_JZ, asBC_JNZ, asBC_JLowZ, asBC_JLowNZ };
			asIScriptFunction* const BranchProbe = GetNativeFunctionByDecl(Module.Get(), "int BranchProbe(const int)");
			ASSERT_THAT(IsNotNull(BranchProbe,
				TEXT("Optimization depth source should publish BranchProbe by its exact fork-normalized declaration")));
			ASSERT_THAT(IsTrue(BranchProbe != nullptr && BranchProbe->GetVarCount() >= 2,
				TEXT("Optimization depth branch function should retain its parameter and local metadata")));
			ASSERT_THAT(IsTrue(BranchProbe != nullptr
				&& AngelscriptCompilerBytecodeTestSupport::ContainsAnyOpcode(
					BranchProbe,
					BranchOpcodes,
					UE_ARRAY_COUNT(BranchOpcodes)),
				TEXT("Optimization depth branch function should retain control-flow target metadata")));
			const char* const EntrySectionName = Entry != nullptr && Entry->GetScriptSectionName() != nullptr
				? Entry->GetScriptSectionName()
				: "";
			ASSERT_THAT(IsTrue(Entry != nullptr, TEXT("Optimization depth source should retain Entry before logging optimization metadata")));
			TestRunner->AddInfo(FString::Printf(TEXT("[CompilerOptimization] mode=%s bytecodeDwords=%u section=%hs"),
				bOptimize ? TEXT("on") : TEXT("off"),
				BytecodeLength,
				EntrySectionName));
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Module.Discard(),
				TEXT("Every optimization mode should discard its exact module")));
			ASSERT_THAT(IsNull(
				ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
				TEXT("Every optimization mode should leave no named module publication")));
		}
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->SetEngineProperty(asEP_OPTIMIZE_BYTECODE, 1),
			TEXT("Optimization depth test should restore the default optimization property")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
