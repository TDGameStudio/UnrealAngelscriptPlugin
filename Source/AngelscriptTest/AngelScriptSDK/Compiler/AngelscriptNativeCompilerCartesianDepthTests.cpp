#include "Support/AngelscriptNativeBuilderTestSupport.h"
#include "Support/AngelscriptNativeLanguageCaseTestSupport.h"
#include "Support/AngelscriptNativeExecutionTestSupport.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCompilerCartesianDepthPrivate
{
	inline void AppendLine(FString& Source, const TCHAR* Line = TEXT(""))
	{
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(Source, Line != nullptr ? FString(Line) : FString());
	}

	inline FString MakeBuilderSource(const int32 Shape, const int32 FailureKind)
	{
		FString Source;
		if (FailureKind == 1)
		{
			if (Shape == 0)
			{
				AppendLine(Source, TEXT("int Broken(  "));
				AppendLine(Source, TEXT("{"));
				AppendLine(Source, TEXT("\treturn 1;"));
				AppendLine(Source, TEXT("}"));
			}
			else if (Shape == 1)
			{
				AppendLine(Source, TEXT("namespace BuilderDepth"));
				AppendLine(Source, TEXT("{"));
				AppendLine(Source, TEXT("\tint Broken(  "));
				AppendLine(Source, TEXT("\t{"));
				AppendLine(Source, TEXT("\t\treturn 1;"));
				AppendLine(Source, TEXT("\t}"));
				AppendLine(Source, TEXT("}"));
			}
			else if (Shape == 2)
			{
				AppendLine(Source, TEXT("const int BaseValue = 40;"));
				AppendLine(Source);
				AppendLine(Source, TEXT("int Broken(  "));
				AppendLine(Source, TEXT("{"));
				AppendLine(Source, TEXT("\treturn BaseValue;"));
				AppendLine(Source, TEXT("}"));
			}
			else
			{
				AppendLine(Source, TEXT("class BuilderCarrier"));
				AppendLine(Source, TEXT("{"));
				AppendLine(Source, TEXT("\tint Value;"));
				AppendLine(Source);
				AppendLine(Source, TEXT("\tint Broken(  "));
				AppendLine(Source, TEXT("\t{"));
				AppendLine(Source, TEXT("\t\treturn Value;"));
				AppendLine(Source, TEXT("\t}"));
				AppendLine(Source, TEXT("}"));
			}
			return Source;
		}

		if (FailureKind == 2)
		{
			if (Shape == 0)
			{
				AppendLine(Source, TEXT("int Entry(MissingType Value)"));
				AppendLine(Source, TEXT("{"));
				AppendLine(Source, TEXT("\treturn 1;"));
				AppendLine(Source, TEXT("}"));
			}
			else if (Shape == 1)
			{
				AppendLine(Source, TEXT("namespace BuilderDepth"));
				AppendLine(Source, TEXT("{"));
				AppendLine(Source, TEXT("\tint Entry(MissingType Value)"));
				AppendLine(Source, TEXT("\t{"));
				AppendLine(Source, TEXT("\t\treturn 1;"));
				AppendLine(Source, TEXT("\t}"));
				AppendLine(Source, TEXT("}"));
			}
			else if (Shape == 2)
			{
				AppendLine(Source, TEXT("const MissingType BaseValue;"));
				AppendLine(Source);
				AppendLine(Source, TEXT("int Entry()"));
				AppendLine(Source, TEXT("{"));
				AppendLine(Source, TEXT("\treturn 1;"));
				AppendLine(Source, TEXT("}"));
			}
			else
			{
				AppendLine(Source, TEXT("class MissingMemberType"));
				AppendLine(Source, TEXT("{"));
				AppendLine(Source, TEXT("\tMissingType Value;"));
				AppendLine(Source, TEXT("}"));
				AppendLine(Source);
				AppendLine(Source, TEXT("int Entry()"));
				AppendLine(Source, TEXT("{"));
				AppendLine(Source, TEXT("\treturn 1;"));
				AppendLine(Source, TEXT("}"));
			}
			return Source;
		}

		if (FailureKind == 3)
		{
			if (Shape == 0)
			{
				AppendLine(Source, TEXT("int Entry()"));
				AppendLine(Source, TEXT("{"));
				AppendLine(Source, TEXT("\treturn 1;"));
			}
			else if (Shape == 1)
			{
				AppendLine(Source, TEXT("namespace BrokenNamespace"));
				AppendLine(Source, TEXT("{"));
				AppendLine(Source, TEXT("\tint Entry()"));
				AppendLine(Source, TEXT("\t{"));
				AppendLine(Source, TEXT("\t\treturn 1;"));
				AppendLine(Source, TEXT("\t}"));
			}
			else if (Shape == 2)
			{
				AppendLine(Source, TEXT("const int BaseValue = 40;"));
				AppendLine(Source);
				AppendLine(Source, TEXT("int Entry()"));
				AppendLine(Source, TEXT("{"));
				AppendLine(Source, TEXT("\treturn BaseValue;"));
			}
			else
			{
				AppendLine(Source, TEXT("class BuilderCarrier"));
				AppendLine(Source, TEXT("{"));
				AppendLine(Source, TEXT("\tint Value;"));
				AppendLine(Source);
				AppendLine(Source, TEXT("\tint Read()"));
				AppendLine(Source, TEXT("\t{"));
				AppendLine(Source, TEXT("\t\treturn Value;"));
			}
			return Source;
		}

		switch (Shape)
		{
		case 0:
			AppendLine(Source, TEXT("int Entry()"));
			AppendLine(Source, TEXT("{"));
			AppendLine(Source, TEXT("\treturn 40 + 2;"));
			AppendLine(Source, TEXT("}"));
			break;
		case 1:
			AppendLine(Source, TEXT("namespace BuilderDepth"));
			AppendLine(Source, TEXT("{"));
			AppendLine(Source, TEXT("\tint Add(int A, int B)"));
			AppendLine(Source, TEXT("\t{"));
			AppendLine(Source, TEXT("\t\treturn A + B;"));
			AppendLine(Source, TEXT("\t}"));
			AppendLine(Source, TEXT("}"));
			AppendLine(Source);
			AppendLine(Source, TEXT("int Entry()"));
			AppendLine(Source, TEXT("{"));
			AppendLine(Source, TEXT("\treturn BuilderDepth::Add(40, 2);"));
			AppendLine(Source, TEXT("}"));
			break;
		case 2:
			AppendLine(Source, TEXT("const int BaseValue = 40;"));
			AppendLine(Source);
			AppendLine(Source, TEXT("int Entry()"));
			AppendLine(Source, TEXT("{"));
			AppendLine(Source, TEXT("\treturn BaseValue + 2;"));
			AppendLine(Source, TEXT("}"));
			break;
		default:
			AppendLine(Source, TEXT("class BuilderCarrier"));
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
			// Keep this shape focused on class declaration/layout publication. The
			// current fork intentionally rejects local script-class value construction
			// at runtime; that behavior is covered by the dedicated language tests.
			AppendLine(Source, TEXT("\treturn 40 + 2;"));
			AppendLine(Source, TEXT("}"));
			break;
		}

		return Source;
	}

	inline FString MakeCaseId(const int32 Shape, const int32 FailureKind)
	{
		return FString::Printf(TEXT("COMPILER-BUILDER-SHAPE-%d-FAILURE-%d"), Shape, FailureKind);
	}

	inline int32 GetExpectedFailureRow(const int32 Shape, const int32 FailureKind)
	{
		static constexpr int32 SyntaxRows[] = { 2, 4, 4, 5 };
		static constexpr int32 MissingTypeRows[] = { 1, 3, 1, 3 };
		static constexpr int32 MissingBraceRows[] = { 4, 7, 6, 8 };
		if (Shape < 0 || Shape >= UE_ARRAY_COUNT(SyntaxRows))
		{
			return INDEX_NONE;
		}

		if (FailureKind == 1)
		{
			return SyntaxRows[Shape];
		}
		if (FailureKind == 2)
		{
			return MissingTypeRows[Shape];
		}
		if (FailureKind == 3)
		{
			return MissingBraceRows[Shape];
		}
		return INDEX_NONE;
	}

	inline const TCHAR* GetExpectedFailureFragment(const int32 Shape, const int32 FailureKind)
	{
		if (FailureKind == 1)
		{
			return Shape == 3
				? TEXT("Expected method or property")
				: TEXT("Expected data type");
		}
		if (FailureKind == 2)
		{
			return TEXT("MissingType");
		}
		if (FailureKind == 3)
		{
			return TEXT("Unexpected end of file");
		}
		return TEXT("");
	}

	inline const TCHAR* GetExpectedFailureStage(const int32 FailureKind)
	{
		return FailureKind == 2 ? TEXT("Functions") : TEXT("Parse");
	}

	inline void ReportFailureDiagnostics(
		FAutomationTestBase& Test,
		const AngelscriptNativeTestSupport::FNativeMessageCollector& Messages,
		const FString& CaseId)
	{
		for (const AngelscriptNativeTestSupport::FNativeMessageEntry& Entry : Messages.Entries)
		{
			Test.AddInfo(FString::Printf(
				TEXT("[CompilerBuilderDiagnostic] case=%s type=%d section=%s row=%d column=%d message=%s"),
				*CaseId,
				static_cast<int32>(Entry.Type),
				*Entry.Section,
				Entry.Row,
				Entry.Column,
				*Entry.Message));
		}
	}

	inline bool HasExactFailureDiagnostic(
		const AngelscriptNativeTestSupport::FNativeMessageCollector& Messages,
		const TCHAR* Section,
		const int32 Row,
		const TCHAR* MessageFragment)
	{
		for (const AngelscriptNativeTestSupport::FNativeMessageEntry& Entry : Messages.Entries)
		{
			if (Entry.Type == asMSGTYPE_ERROR
				&& Entry.Section == Section
				&& Entry.Row == Row
				&& Entry.Message.Contains(MessageFragment))
			{
				return true;
			}
		}
		return false;
	}

}

TEST_CLASS_WITH_FLAGS(FCompilerBuilderCartesianDepthTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.Builder.CartesianDepth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(BuilderStagesPublishOrRejectAcrossShapeAndFailureInputs)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("COMPILER-BUILDER-SHAPE-FAILURE",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Metadata
			| ENativeEvidence::Runtime
			| ENativeEvidence::Cleanup);

		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptSDKTestSupport;

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Builder depth test should create a standalone SDK engine")));
		int32 CleanupSentinel = 42;
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->RegisterGlobalProperty("int CompilerBuilderCleanupSentinel", &CleanupSentinel),
			TEXT("Builder depth test should register its independent cleanup sentinel")));

		for (int32 Shape = 0; Shape < 4; ++Shape)
		{
			for (int32 FailureKind = 0; FailureKind < 4; ++FailureKind)
			{
				const bool bExpectedSuccess = FailureKind == 0;
				const FString CaseId = AngelscriptCompilerCartesianDepthPrivate::MakeCaseId(Shape, FailureKind);
				const FString ModuleName = FString::Printf(TEXT("CompilerBuilderDepth_%d_%d"), Shape, FailureKind);
				const FString Source = AngelscriptCompilerCartesianDepthPrivate::MakeBuilderSource(Shape, FailureKind);
				Engine.ResetMessages();
				PrintGeneratedAsSource(*TestRunner, CaseId, ModuleName, Source);

				FTCHARToUTF8 SourceUtf8(*Source);
				FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
				FScopedNativeModuleName ModuleScope(Engine, ModuleNameUtf8.Get());
				asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
				ASSERT_THAT(IsNotNull(Module, TEXT("Builder depth test should create a raw builder module")));
				if (Module == nullptr)
				{
					continue;
				}

				ASSERT_THAT(IsTrue(AddBuilderSectionWithLog(*TestRunner, *Module, "CompilerBuilderDepth.as", SourceUtf8.Get(), CaseId),
					TEXT("Builder depth test should add the generated source section")));
				asCBuilder* const Builder = Module->builder;
				ASSERT_THAT(IsNotNull(Builder, TEXT("Builder depth test should expose the internal builder")));
				if (Builder == nullptr)
				{
					continue;
				}

				FString FailedStage;
				const auto RunTrackedStage = [&](const TCHAR* StageName, int (asCBuilder::*StageFunction)())
				{
					const bool bSucceeded = RunBuilderStage(
						*TestRunner,
						*Builder,
						CaseId + TEXT(".") + StageName,
						StageFunction,
						Module);
					if (!bSucceeded && FailedStage.IsEmpty())
					{
						FailedStage = StageName;
					}
					return bSucceeded;
				};

				const bool bParsed = RunTrackedStage(TEXT("Parse"), &asCBuilder::BuildParallelParseScripts);
				bool bPipelineSucceeded = bParsed;
				if (bPipelineSucceeded)
				{
					bPipelineSucceeded = RunTrackedStage(TEXT("Types"), &asCBuilder::BuildGenerateTypes);
				}
				if (bPipelineSucceeded)
				{
					bPipelineSucceeded = RunTrackedStage(TEXT("Functions"), &asCBuilder::BuildGenerateFunctions);
				}
				if (bPipelineSucceeded)
				{
					bPipelineSucceeded = RunTrackedStage(TEXT("ClassLayout"), &asCBuilder::BuildLayoutClasses);
				}
				if (bPipelineSucceeded)
				{
					Builder->BuildAllocateGlobalVariables();
					bPipelineSucceeded = RunTrackedStage(TEXT("FunctionLayout"), &asCBuilder::BuildLayoutFunctions);
				}
				if (bPipelineSucceeded)
				{
					bPipelineSucceeded = RunTrackedStage(TEXT("CompileCode"), &asCBuilder::BuildCompileCode);
				}

				if (bExpectedSuccess)
				{
					ASSERT_THAT(IsTrue(bPipelineSucceeded, TEXT("Every valid builder shape should complete all publication stages")));
					ASSERT_THAT(IsNotNull(GetNativeFunctionByDecl(Module, "int Entry()"),
						TEXT("Every valid builder shape should publish the exact Entry declaration")));

					int32 Result = 0;
					if (bPipelineSucceeded && ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
					{
						ASSERT_THAT(AreEqual(42, Result, TEXT("Every valid builder shape should execute to the same semantic result")));
					}
					if (Shape == 3)
					{
						ASSERT_THAT(IsNotNull(Module->GetTypeInfoByDecl("BuilderCarrier"),
							TEXT("Class builder shape should publish its class layout metadata")));
						TestRunner->AddInfo(TEXT("[AS-FORK-LIMITATION] Builder class shape records class layout only; local script-class value construction remains deferred for this fork"));
					}
					else if (Shape == 2)
					{
						ASSERT_THAT(AreEqual(1, static_cast<int32>(Module->GetGlobalVarCount()),
							TEXT("Const-global builder shape should publish exactly one global declaration")));
					}
					else if (Shape == 1)
					{
						ASSERT_THAT(IsNotNull(GetNativeFunctionByDecl(Module, "int BuilderDepth::Add(const int, const int)"),
							TEXT("Namespace builder shape should publish Add by its exact normalized declaration")));
					}
				}
				else
				{
					AngelscriptCompilerCartesianDepthPrivate::ReportFailureDiagnostics(
						*TestRunner,
						Engine.GetMessages(),
						CaseId);
					ASSERT_THAT(IsFalse(bPipelineSucceeded, TEXT("Every invalid builder shape should be rejected by one of the publication stages")));
					bool bPublishedExecutableFunction = false;
					for (asUINT FunctionIndex = 0; FunctionIndex < Module->GetFunctionCount(); ++FunctionIndex)
					{
						asIScriptFunction* const Function = Module->GetFunctionByIndex(FunctionIndex);
						asUINT BytecodeLength = 0;
						if (Function != nullptr && Function->GetByteCode(&BytecodeLength) != nullptr && BytecodeLength > 0)
						{
							bPublishedExecutableFunction = true;
						}
					}
					ASSERT_THAT(IsFalse(bPublishedExecutableFunction,
						TEXT("Rejected builder inputs must not publish executable bytecode")));
					if (Module->GetFunctionCount() > 0)
					{
						TestRunner->AddInfo(FString::Printf(
							TEXT("[AS-FORK-LIMITATION] %s retained %u non-executable function declarations after rejection"),
							*CaseId,
							static_cast<uint32>(Module->GetFunctionCount())));
					}
					ASSERT_THAT(AreEqual(
						FString(AngelscriptCompilerCartesianDepthPrivate::GetExpectedFailureStage(FailureKind)),
						FailedStage,
						TEXT("Rejected builder inputs should fail at the exact owning stage")));
					ASSERT_THAT(IsTrue(
						AngelscriptCompilerCartesianDepthPrivate::HasExactFailureDiagnostic(
							Engine.GetMessages(),
							TEXT("CompilerBuilderDepth.as"),
							AngelscriptCompilerCartesianDepthPrivate::GetExpectedFailureRow(Shape, FailureKind),
							AngelscriptCompilerCartesianDepthPrivate::GetExpectedFailureFragment(Shape, FailureKind)),
						TEXT("Rejected builder inputs should retain the exact section, row, and symbol diagnostic")));
				}

				ASSERT_THAT(AreEqual(
					asSUCCESS,
					ScriptEngine->DiscardModule(ModuleNameUtf8.Get()),
					TEXT("Every builder shape should discard its exact module")));
				ASSERT_THAT(IsNull(
					ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
					TEXT("Every builder shape should leave no named module publication")));
				ASSERT_THAT(AreEqual(
					42,
					CleanupSentinel,
					TEXT("Builder shape cleanup should preserve independent native storage")));
			}
		}
	}

	};

TEST_CLASS_WITH_FLAGS(FCompilerBuilderRecoveryDepthTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.BuilderCartesianDepth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(InvalidBuildsRecoverAcrossShapeAndEngineRoute)
	{
		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptSDKTestSupport;
		AS_NATIVE_PRODUCT("COMPILER-BUILDER-REBUILD-RECOVERY",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Metadata
			| ENativeEvidence::Runtime
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		struct FShapeCase
		{
			const TCHAR* Id;
			int32 Shape;
		};
		struct FFailureCase
		{
			const TCHAR* Id;
			int32 FailureKind;
		};
		struct FRecoveryRouteCase
		{
			const TCHAR* Id;
			bool bFreshEngine;
		};

		const FShapeCase Shapes[] =
		{
			{ TEXT("literal"), 0 },
			{ TEXT("namespace"), 1 },
		};
		const FFailureCase Failures[] =
		{
			{ TEXT("syntax"), 1 },
			{ TEXT("missing_type"), 2 },
			{ TEXT("missing_brace"), 3 },
		};
		const FRecoveryRouteCase RecoveryRoutes[] =
		{
			{ TEXT("same_engine"), false },
			{ TEXT("fresh_engine"), true },
		};

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("Builder recovery owner should create a standalone SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		int32 ObservedCaseCount = 0;
		for (const FShapeCase& Shape : Shapes)
		{
			for (const FFailureCase& Failure : Failures)
			{
				for (const FRecoveryRouteCase& RecoveryRoute : RecoveryRoutes)
				{
					const FString CaseId = MakeNativeCaseId(
						"COMPILER-BUILDER-REBUILD-RECOVERY",
						{ Shape.Id, Failure.Id, RecoveryRoute.Id });
					const FString ModuleName = FString::Printf(
						TEXT("CompilerBuilderRecovery_%s_%s_%s"),
						Shape.Id,
						Failure.Id,
						RecoveryRoute.Id);
					const FString InvalidSource = AngelscriptCompilerCartesianDepthPrivate::MakeBuilderSource(Shape.Shape, Failure.FailureKind);
					const FString RecoverySource = AngelscriptCompilerCartesianDepthPrivate::MakeBuilderSource(Shape.Shape, 0);
					PrintGeneratedAsSource(*TestRunner, CaseId + TEXT("-INVALID"), ModuleName + TEXT("_Invalid"), InvalidSource);
					PrintGeneratedAsSource(*TestRunner, CaseId + TEXT("-RECOVERY"), ModuleName + TEXT("_Recovery"), RecoverySource);

					const auto RunPipeline = [&](const FString& Source, const bool bExpectSuccess, const FString& SourceId)
					{
						Engine.ResetMessages();
						const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
						const FTCHARToUTF8 SourceUtf8(*Source);
						FScopedNativeModuleName ModuleScope(Engine, ModuleNameUtf8.Get());
						asCModule* Module = CreateBuilderModule(ScriptEngine, ModuleScope.Get());
						ASSERT_THAT(IsNotNull(Module, *FString::Printf(TEXT("%s should create its builder module"), *SourceId)));
						if (Module == nullptr)
						{
							return;
						}

						ASSERT_THAT(IsTrue(
							AddBuilderSectionWithLog(*TestRunner, *Module, "CompilerBuilderRecovery.as", SourceUtf8.Get(), SourceId),
							*FString::Printf(TEXT("%s should add its builder section"), *SourceId)));
						asCBuilder* const Builder = Module->builder;
						ASSERT_THAT(IsNotNull(Builder, *FString::Printf(TEXT("%s should expose its builder"), *SourceId)));
						if (Builder == nullptr)
						{
							return;
						}

						FString FailedStage;
						const auto RunTrackedStage = [&](const TCHAR* StageName, int (asCBuilder::*StageFunction)())
						{
							const bool bSucceeded = RunBuilderStage(
								*TestRunner,
								*Builder,
								SourceId + TEXT(".") + StageName,
								StageFunction,
								Module);
							if (!bSucceeded && FailedStage.IsEmpty())
							{
								FailedStage = StageName;
							}
							return bSucceeded;
						};

						bool bPipelineSucceeded = RunTrackedStage(TEXT("Parse"), &asCBuilder::BuildParallelParseScripts);
						if (bPipelineSucceeded)
						{
							bPipelineSucceeded = RunTrackedStage(TEXT("Types"), &asCBuilder::BuildGenerateTypes);
						}
						if (bPipelineSucceeded)
						{
							bPipelineSucceeded = RunTrackedStage(TEXT("Functions"), &asCBuilder::BuildGenerateFunctions);
						}
						if (bPipelineSucceeded)
						{
							bPipelineSucceeded = RunTrackedStage(TEXT("ClassLayout"), &asCBuilder::BuildLayoutClasses);
						}
						if (bPipelineSucceeded)
						{
							Builder->BuildAllocateGlobalVariables();
							bPipelineSucceeded = RunTrackedStage(TEXT("FunctionLayout"), &asCBuilder::BuildLayoutFunctions);
						}
						if (bPipelineSucceeded)
						{
							bPipelineSucceeded = RunTrackedStage(TEXT("CompileCode"), &asCBuilder::BuildCompileCode);
						}

						if (bExpectSuccess)
						{
							ASSERT_THAT(IsTrue(bPipelineSucceeded,
								*FString::Printf(TEXT("%s should retain its expected successful builder result"), *SourceId)));
						}
						else
						{
							ASSERT_THAT(IsFalse(bPipelineSucceeded,
								*FString::Printf(TEXT("%s should retain its expected rejected builder result"), *SourceId)));
							bool bPublishedExecutableFunction = false;
							for (asUINT FunctionIndex = 0; FunctionIndex < Module->GetFunctionCount(); ++FunctionIndex)
							{
								asIScriptFunction* const Function = Module->GetFunctionByIndex(FunctionIndex);
								asUINT BytecodeLength = 0;
								if (Function != nullptr
									&& Function->GetByteCode(&BytecodeLength) != nullptr
									&& BytecodeLength > 0)
								{
									bPublishedExecutableFunction = true;
									break;
								}
							}
							ASSERT_THAT(IsFalse(bPublishedExecutableFunction,
								*FString::Printf(TEXT("%s should not publish executable bytecode after rejection"), *SourceId)));
						}
						if (bExpectSuccess)
						{
							asIScriptFunction* const Entry = GetNativeFunctionByDecl(Module, "int Entry()");
							ASSERT_THAT(IsNotNull(Entry, *FString::Printf(TEXT("%s should publish Entry after recovery"), *SourceId)));
							int32 Result = 0;
							if (Entry != nullptr)
							{
								ASSERT_THAT(IsTrue(
									ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result),
									*FString::Printf(TEXT("%s should execute after recovery"), *SourceId)));
								ASSERT_THAT(AreEqual(42, Result,
									*FString::Printf(TEXT("%s should preserve the valid recovery result"), *SourceId)));
							}
						}
						else
						{
							AngelscriptCompilerCartesianDepthPrivate::ReportFailureDiagnostics(
								*TestRunner,
								Engine.GetMessages(),
								SourceId);
							ASSERT_THAT(AreEqual(
								FString(AngelscriptCompilerCartesianDepthPrivate::GetExpectedFailureStage(Failure.FailureKind)),
								FailedStage,
								*FString::Printf(TEXT("%s should fail at the exact owning stage"), *SourceId)));
							ASSERT_THAT(IsTrue(
								AngelscriptCompilerCartesianDepthPrivate::HasExactFailureDiagnostic(
									Engine.GetMessages(),
									TEXT("CompilerBuilderRecovery.as"),
									AngelscriptCompilerCartesianDepthPrivate::GetExpectedFailureRow(Shape.Shape, Failure.FailureKind),
									AngelscriptCompilerCartesianDepthPrivate::GetExpectedFailureFragment(Shape.Shape, Failure.FailureKind)),
								*FString::Printf(TEXT("%s should retain the exact section, row, and symbol diagnostic"), *SourceId)));
						}

						ASSERT_THAT(AreEqual(
							asSUCCESS,
							ScriptEngine->DiscardModule(ModuleNameUtf8.Get()),
							*FString::Printf(TEXT("%s should discard its exact module"), *SourceId)));
						ASSERT_THAT(IsNull(
							ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
							*FString::Printf(TEXT("%s should leave no named module publication"), *SourceId)));
					};

					Engine.Reset(*TestRunner);
					RunPipeline(InvalidSource, false, CaseId + TEXT("-INVALID"));
					if (RecoveryRoute.bFreshEngine)
					{
						Engine.Destroy();
						Engine.Create(*TestRunner);
						ScriptEngine = Engine.Get();
					}
					else
					{
						Engine.Reset(*TestRunner);
					}
					RunPipeline(RecoverySource, true, CaseId + TEXT("-RECOVERY"));
					++ObservedCaseCount;
				}
			}
		}

		ASSERT_THAT(AreEqual(
			12,
			ObservedCaseCount,
			TEXT("Builder shape × failure × recovery route should execute every cell")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
