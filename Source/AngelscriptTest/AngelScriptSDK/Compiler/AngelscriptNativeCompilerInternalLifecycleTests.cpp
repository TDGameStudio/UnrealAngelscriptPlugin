#include "Support/AngelscriptNativeBuilderTestSupport.h"
#include "Support/AngelscriptNativeLanguageCaseTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_compiler.h"
#include "source/as_scriptcode.h"
#include "EndAngelscriptHeaders.h"

#include <cstring>
#include <string>

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FCompilerInternalLifecycleTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.InternalLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	class FTrackingStringFactory final : public asIStringFactory
	{
	public:
		const void* GetStringConstant(const char* Data, asUINT Length) override
		{
			++AcquireCount;
			Bytes.assign(Data != nullptr ? Data : "", static_cast<size_t>(Data != nullptr ? Length : 0));
			return &Storage;
		}

		int ReleaseStringConstant(const void* String) override
		{
			if (String != &Storage)
			{
				return asINVALID_ARG;
			}

			++ReleaseCount;
			return asSUCCESS;
		}

		int GetRawStringData(const void* String, char* Data, asUINT* Length) const override
		{
			if (String != &Storage || Length == nullptr)
			{
				return asINVALID_ARG;
			}

			if (Data != nullptr && !Bytes.empty())
			{
				std::memcpy(Data, Bytes.data(), Bytes.size());
			}
			*Length = static_cast<asUINT>(Bytes.size());
			return asSUCCESS;
		}

		int32 AcquireCount = 0;
		int32 ReleaseCount = 0;

	private:
		int32 Storage = 0;
		std::string Bytes;
	};

	class FInspectableCompiler final : public asCCompiler
	{
	public:
		explicit FInspectableCompiler(asCBuilder* Builder)
			: asCCompiler(Builder)
		{
		}

		bool HasConstructionDefaults()
		{
			return builder == nullptr
				&& script == nullptr
				&& variables == nullptr
				&& !isProcessingDeferredParams
				&& !isCompilingDefaultArg
				&& noCodeOutput == 0
				&& !allowEditPropertyAccess
				&& byteCode.GetSize() == 0;
		}

		void SeedResetState(asCObjectType* ExternalType)
		{
			hasCompileErrors = true;
			m_isConstructor = true;
			m_isInitDefaults = true;
			m_isDestructor = true;
			m_isConstructorCalled = true;
			nextLabel = 17;
			numLambdas = 3;
			breakLabels.PushLast(11);
			continueLabels.PushLast(13);
			byteCode.Instr(asBC_SUSPEND);
			ExternalThisType = ExternalType;
			ExternalThisOffset = 29;
		}

		void ResetForTest(asCBuilder* Builder, asCScriptCode* ScriptCode, asCScriptFunction* Function)
		{
			Reset(Builder, ScriptCode, Function);
		}

		bool HasResetState(
			asCBuilder* ExpectedBuilder,
			asCScriptCode* ExpectedScript,
			asCScriptFunction* ExpectedFunction)
		{
			return builder == ExpectedBuilder
				&& engine == ExpectedBuilder->engine
				&& script == ExpectedScript
				&& outFunc == ExpectedFunction
				&& !hasCompileErrors
				&& !m_isConstructor
				&& !m_isInitDefaults
				&& !m_isDestructor
				&& !m_isConstructorCalled
				&& m_classDecl == nullptr
				&& m_globalVar == nullptr
				&& nextLabel == 0
				&& numLambdas == 0
				&& breakLabels.GetLength() == 0
				&& continueLabels.GetLength() == 0
				&& byteCode.GetSize() == 0
				&& ExternalThisType == nullptr
				&& ExternalThisOffset == 0;
		}

		void SeedDestructorState(asCScriptEngine* ScriptEngine, void* StringConstant)
		{
			engine = ScriptEngine;
			AddVariableScope();
			AddVariableScope();
			usedStringConstants.PushLast(StringConstant);
		}

		int32 GetVariableScopeDepth() const
		{
			int32 Depth = 0;
			for (asCVariableScope* Scope = variables; Scope != nullptr; Scope = Scope->parent)
			{
				++Depth;
			}
			return Depth;
		}

		int32 GetOwnedStringConstantCount() const
		{
			return static_cast<int32>(usedStringConstants.GetLength());
		}
	};

public:
	TEST_METHOD(ConstructionResetAndDestructionOwnTransientState)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("COMPILER-INTERNAL-COMPILER-LIFECYCLE",
			ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		const TCHAR* Scenarios[] =
		{
			TEXT("construction_defaults"),
			TEXT("reset_transient_state"),
			TEXT("destructor_owned_resources"),
		};
		for (const TCHAR* Scenario : Scenarios)
		{
			FString ReviewSource;
			AppendGeneratedAsLine(
				ReviewSource,
				FString::Printf(TEXT("// asCCompiler lifecycle scenario: %s"), Scenario));
			PrintGeneratedAsSource(
				*TestRunner,
				MakeNativeCaseId("COMPILER-INTERNAL-COMPILER-LIFECYCLE", { Scenario }),
				TEXT("CompilerInternalLifecycleNativeReview"),
				ReviewSource);
		}

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asCScriptEngine* const ScriptEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Compiler lifecycle test should create a case-owned raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FScopedNativeModuleName ModuleScope(Engine, "CompilerInternalLifecycleReset");
		asCModule* const Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(
			Module,
			TEXT("Compiler lifecycle test should create a case-owned backing module")));
		if (Module == nullptr)
		{
			return;
		}

		const std::string ResetSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return 42;
			}
			)AS");
		PrintGeneratedAsSource(
			*TestRunner,
			MakeNativeCaseId("COMPILER-INTERNAL-COMPILER-LIFECYCLE", { TEXT("reset_transient_state") }),
			TEXT("CompilerInternalLifecycleReset.as"),
			UTF8_TO_TCHAR(ResetSource.c_str()));
		ASSERT_THAT(IsTrue(
			AddBuilderSection(*Module, "CompilerInternalLifecycleReset.as", ResetSource.c_str()),
			TEXT("Compiler lifecycle test should create the transient builder from a review-visible section")));
		asCBuilder* const Builder = Module->builder;
		ASSERT_THAT(IsNotNull(
			Builder,
			TEXT("Compiler lifecycle reset should receive a live builder")));
		if (Builder == nullptr)
		{
			return;
		}

		{
			FInspectableCompiler ConstructionCompiler(Builder);
			ASSERT_THAT(IsTrue(
				ConstructionCompiler.HasConstructionDefaults(),
				TEXT("asCCompiler construction should initialize its documented pointer, flag, and bytecode defaults")));
		}

		asCScriptCode ScriptCode;
		asCObjectType ExternalType(ScriptEngine);
		FInspectableCompiler Compiler(Builder);
		Compiler.SeedResetState(&ExternalType);
		Compiler.ResetForTest(Builder, &ScriptCode, nullptr);
		ASSERT_THAT(IsTrue(
			Compiler.HasResetState(Builder, &ScriptCode, nullptr),
			TEXT("asCCompiler Reset should replace owners and clear every documented transient compilation state")));

		FTrackingStringFactory StringFactory;
		asIStringFactory* const PreviousStringFactory = ScriptEngine->stringFactory;
		ScriptEngine->stringFactory = &StringFactory;
		ON_SCOPE_EXIT
		{
			ScriptEngine->stringFactory = PreviousStringFactory;
		};

		const void* const OwnedString = StringFactory.GetStringConstant("owned", 5);
		{
			FInspectableCompiler DestructorCompiler(Builder);
			DestructorCompiler.SeedDestructorState(
				ScriptEngine,
				const_cast<void*>(OwnedString));
			ASSERT_THAT(AreEqual(
				2,
				DestructorCompiler.GetVariableScopeDepth(),
				TEXT("Destructor fixture should own the complete nested variable-scope chain before teardown")));
			ASSERT_THAT(AreEqual(
				1,
				DestructorCompiler.GetOwnedStringConstantCount(),
				TEXT("Destructor fixture should own one compiler string constant before teardown")));
		}

		ASSERT_THAT(AreEqual(
			1,
			StringFactory.AcquireCount,
			TEXT("Destructor fixture should acquire exactly one tracked string constant")));
		ASSERT_THAT(AreEqual(
			1,
			StringFactory.ReleaseCount,
			TEXT("asCCompiler destruction should release every compiler-owned string constant exactly once")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
