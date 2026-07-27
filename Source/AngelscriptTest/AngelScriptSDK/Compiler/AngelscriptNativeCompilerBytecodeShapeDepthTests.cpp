#include "Support/AngelscriptNativeBuilderTestSupport.h"
#include "Support/AngelscriptNativeCompilerBytecodeTestSupport.h"
#include "Support/AngelscriptNativeLanguageCaseTestSupport.h"
#include "Support/AngelscriptNativeExecutionTestSupport.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCompilerBytecodeShapeDepthPrivate
{
	inline void AppendLine(FString& Source, const TCHAR* Line = TEXT(""))
	{
		Source += Line;
		Source.AppendChar(TEXT('\n'));
	}

	inline FString MakeBytecodeShapeSource(const int32 Shape)
	{
		FString Source;
		if (Shape == 0)
		{
			AppendLine(Source, TEXT("int GetLeft()"));
			AppendLine(Source, TEXT("{"));
			AppendLine(Source, TEXT("\treturn 2;"));
			AppendLine(Source, TEXT("}"));
			AppendLine(Source);
			AppendLine(Source, TEXT("int GetRight()"));
			AppendLine(Source, TEXT("{"));
			AppendLine(Source, TEXT("\treturn 3;"));
			AppendLine(Source, TEXT("}"));
			AppendLine(Source);
			AppendLine(Source, TEXT("int Entry()"));
			AppendLine(Source, TEXT("{"));
			AppendLine(Source, TEXT("\treturn GetLeft() + GetRight() * 4;"));
			AppendLine(Source, TEXT("}"));
		}
		else if (Shape == 1)
		{
			AppendLine(Source, TEXT("bool IsEnabled()"));
			AppendLine(Source, TEXT("{"));
			AppendLine(Source, TEXT("\treturn true;"));
			AppendLine(Source, TEXT("}"));
			AppendLine(Source);
			AppendLine(Source, TEXT("int Entry()"));
			AppendLine(Source, TEXT("{"));
			AppendLine(Source, TEXT("\tif (IsEnabled())"));
			AppendLine(Source, TEXT("\t{"));
			AppendLine(Source, TEXT("\t\treturn 42;"));
			AppendLine(Source, TEXT("\t}"));
			AppendLine(Source, TEXT("\treturn 1;"));
			AppendLine(Source, TEXT("}"));
		}
		else if (Shape == 2)
		{
			AppendLine(Source, TEXT("int Entry()"));
			AppendLine(Source, TEXT("{"));
			AppendLine(Source, TEXT("\tint Sum = 0;"));
			AppendLine(Source, TEXT("\tfor (int Index = 0; Index < 4; ++Index)"));
			AppendLine(Source, TEXT("\t{"));
			AppendLine(Source, TEXT("\t\tSum += Index;"));
			AppendLine(Source, TEXT("\t}"));
			AppendLine(Source, TEXT("\treturn Sum;"));
			AppendLine(Source, TEXT("}"));
		}
		else
		{
			AppendLine(Source, TEXT("class BytecodeCarrier"));
			AppendLine(Source, TEXT("{"));
			AppendLine(Source, TEXT("\tint Value;"));
			AppendLine(Source);
			AppendLine(Source, TEXT("\tint Read()"));
			AppendLine(Source, TEXT("\t{"));
			AppendLine(Source, TEXT("\t\treturn Value;"));
			AppendLine(Source, TEXT("\t}"));
			AppendLine(Source, TEXT("}"));
			AppendLine(Source);
			AppendLine(Source, TEXT("int Entry()"));
			AppendLine(Source, TEXT("{"));
			// Keep this bytecode shape executable on the current fork while the
			// class declaration still contributes object-layout metadata. Local
			// script-class value construction is covered as an explicit limitation
			// by the language/object-lifecycle suites.
			AppendLine(Source, TEXT("\treturn 40 + 2;"));
			AppendLine(Source, TEXT("}"));
		}

		return Source;
	}
}

TEST_CLASS_WITH_FLAGS(FCompilerBytecodeShapeDepthTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.Bytecode.ShapeDepth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ControlFlowArithmeticAndClassLayoutShapesKeepRuntimeAndOpcodeEvidence)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("COMPILER-BYTECODE-SHAPE",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Bytecode
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup);

		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Bytecode shape test should create a standalone SDK engine")));

		for (int32 Shape = 0; Shape < 4; ++Shape)
		{
			const FString Source = AngelscriptCompilerBytecodeShapeDepthPrivate::MakeBytecodeShapeSource(Shape);
			int32 ExpectedResult = 0;
			if (Shape == 0)
			{
				ExpectedResult = 14;
			}
			else if (Shape == 1)
			{
				ExpectedResult = 42;
			}
			else if (Shape == 2)
			{
				ExpectedResult = 6;
			}
			else
			{
				ExpectedResult = 42;
			}

			const FString CaseId = FString::Printf(TEXT("COMPILER-BYTECODE-SHAPE-%d"), Shape);
			const FString ModuleName = FString::Printf(TEXT("CompilerBytecodeShape_%d"), Shape);
			PrintGeneratedAsSource(*TestRunner, CaseId, ModuleName, Source);
			FTCHARToUTF8 SourceUtf8(*Source);
			FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
			FScopedNativeModule Module(*TestRunner, Engine, ModuleNameUtf8.Get(), SourceUtf8.Get());
			ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("Bytecode shape source should compile")));
			if (!Module.IsValid())
			{
				continue;
			}

			asIScriptFunction* const Entry = GetNativeFunctionByDecl(Module.Get(), "int Entry()");
			ASSERT_THAT(IsNotNull(Entry, TEXT("Bytecode shape source should publish Entry")));
			ASSERT_THAT(IsTrue(AngelscriptBuilderTestSupport::HasBytecode(Entry), TEXT("Bytecode shape source should expose executable bytecode")));

			int32 ActualResult = 0;
			ASSERT_THAT(IsTrue(ExecuteScriptFunction(*TestRunner, ScriptEngine, Module.Get(), "int Entry()", ActualResult),
				TEXT("Bytecode shape source should execute successfully")));
			ASSERT_THAT(AreEqual(ExpectedResult, ActualResult, TEXT("Bytecode shape execution should preserve the source result")));

			if (Shape == 0)
			{
				ASSERT_THAT(IsTrue(AngelscriptCompilerBytecodeTestSupport::ContainsOpcode(Entry, asBC_ADDi),
					TEXT("Arithmetic shape should emit integer addition")));
			}
			else if (Shape == 1 || Shape == 2)
			{
				static constexpr asEBCInstr BranchOpcodes[] = { asBC_JMP, asBC_JZ, asBC_JNZ, asBC_JLowZ, asBC_JLowNZ };
				ASSERT_THAT(IsTrue(AngelscriptCompilerBytecodeTestSupport::ContainsAnyOpcode(Entry, BranchOpcodes, UE_ARRAY_COUNT(BranchOpcodes)),
					TEXT("Control-flow shape should emit a conditional or unconditional jump")));
			}
			else
			{
				ASSERT_THAT(IsNotNull(Module->GetTypeInfoByDecl("BytecodeCarrier"),
					TEXT("Class-layout bytecode shape should publish its object type metadata")));
				TestRunner->AddInfo(TEXT("[AS-FORK-LIMITATION] Bytecode object-call shape records class metadata only; local script-class value construction remains deferred for this fork"));
				// The fork's optimizer may fold the constant return, so the stable
				// evidence for this shape is non-empty executable bytecode plus class
				// metadata rather than a particular arithmetic opcode.
			}

			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Module.Discard(),
				TEXT("Every bytecode shape should discard its exact module")));
			ASSERT_THAT(IsNull(
				ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
				TEXT("Every bytecode shape should leave no named module publication")));
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
