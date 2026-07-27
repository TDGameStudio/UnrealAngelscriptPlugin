#include "../Support/AngelscriptNativeCaseTestSupport.h"
#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

using AngelscriptNativeTestSupport::AppendGeneratedAsLine;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS_AND_TAGS(FSelected238DesiredBehaviorTests,
	"Angelscript.TestModule.AngelScriptSDK.Conformance.Selected238DesiredBehavior",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::Disabled,
	TEXT("#as-v238-backport"))
{
private:
	enum class ESelected238EvidenceLayer : uint8
	{
		Parse,
		Compile,
		Metadata,
		Runtime,
		Cleanup,
	};

	struct FFeatureCase
	{
		const ANSICHAR* CatalogName;
		bool bExpectedBuildFailure = false;
	};

	struct FLayerCase
	{
		const ANSICHAR* CatalogName;
		ESelected238EvidenceLayer EvidenceLayer;
	};

	inline static constexpr FFeatureCase FeatureCases[] =
	{
		{ "using_namespace" },
		{ "member_initialization" },
		{ "default_special_members" },
		{ "bool_context" },
		{ "lambda" },
		{ "variadic_function" },
		{ "function_template" },
		{ "try_catch_rethrow" },
		{ "script_property_accessors" },
		{ "indexed_setter_ambiguity", true },
		{ "abstract_class_modifier" },
		{ "final_class_modifier" },
		{ "null_handle_syntax" },
	};

	inline static constexpr FLayerCase LayerCases[] =
	{
		{ "parse", ESelected238EvidenceLayer::Parse },
		{ "compile", ESelected238EvidenceLayer::Compile },
		{ "metadata", ESelected238EvidenceLayer::Metadata },
		{ "runtime", ESelected238EvidenceLayer::Runtime },
		{ "cleanup", ESelected238EvidenceLayer::Cleanup },
	};

	static void AppendFeatureSource(FString& Source, const FFeatureCase& FeatureCase)
	{
		if (FCStringAnsi::Strcmp(FeatureCase.CatalogName, "using_namespace") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("namespace FutureNamespace"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 42;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("using namespace FutureNamespace;"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int FeatureEntry()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value();"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (FCStringAnsi::Strcmp(FeatureCase.CatalogName, "member_initialization") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("class FutureMember"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value;"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFutureMember() : Value(42)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int FeatureEntry()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFutureMember Member;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Member.Value;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (FCStringAnsi::Strcmp(FeatureCase.CatalogName, "default_special_members") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("class FutureSpecial"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value = 42;"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFutureSpecial() = default;"));
			AppendGeneratedAsLine(Source, TEXT("\tFutureSpecial(const FutureSpecial& in Other) = default;"));
			AppendGeneratedAsLine(Source, TEXT("\tFutureSpecial& opAssign(const FutureSpecial& in Other) = delete;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int FeatureEntry()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFutureSpecial First;"));
			AppendGeneratedAsLine(Source, TEXT("\tFutureSpecial Second(First);"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Second.Value;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (FCStringAnsi::Strcmp(FeatureCase.CatalogName, "bool_context") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("class FutureFlag"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Remaining = 1;"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tbool opImplConv() const"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Remaining > 0;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int FeatureEntry()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFutureFlag Flag;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Flag ? 42 : 0;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (FCStringAnsi::Strcmp(FeatureCase.CatalogName, "lambda") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("funcdef int FutureUnary(int);"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int FutureLambda()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFutureUnary@ AddTwo = function(int Value)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Value + 2;"));
			AppendGeneratedAsLine(Source, TEXT("\t};"));
			AppendGeneratedAsLine(Source, TEXT("\treturn AddTwo(40);"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int FeatureEntry()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn FutureLambda();"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (FCStringAnsi::Strcmp(FeatureCase.CatalogName, "variadic_function") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("int FutureVariadic(int First, ...)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn First + arguments.length();"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int FeatureEntry()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn FutureVariadic(40, 1, 2);"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (FCStringAnsi::Strcmp(FeatureCase.CatalogName, "function_template") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("T FutureIdentity<T>(T Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int FeatureEntry()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn FutureIdentity<int>(42);"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (FCStringAnsi::Strcmp(FeatureCase.CatalogName, "try_catch_rethrow") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("void FutureRethrow()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\ttry"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tthrow(\"future rethrow\");"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\tcatch"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tthrow;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int FeatureEntry()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\ttry"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tFutureRethrow();"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\tcatch"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 42;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 0;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (FCStringAnsi::Strcmp(FeatureCase.CatalogName, "script_property_accessors") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("class FutureProperty"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Storage;"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tint Value"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tget"));
			AppendGeneratedAsLine(Source, TEXT("\t\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\treturn Storage;"));
			AppendGeneratedAsLine(Source, TEXT("\t\t}"));
			AppendGeneratedAsLine(Source, TEXT("\t\tset"));
			AppendGeneratedAsLine(Source, TEXT("\t\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\tStorage = Value;"));
			AppendGeneratedAsLine(Source, TEXT("\t\t}"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int FeatureEntry()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFutureProperty Property;"));
			AppendGeneratedAsLine(Source, TEXT("\tProperty.Value = 42;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Property.Value;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (FCStringAnsi::Strcmp(FeatureCase.CatalogName, "indexed_setter_ambiguity") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("class FutureIndexed"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint opIndex(int Index)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Index;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tvoid opIndex(int Index, int Value)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tvoid opIndex(int Index, uint Value)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int FeatureEntry()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFutureIndexed Indexed;"));
			AppendGeneratedAsLine(Source, TEXT("\tIndexed[0] = 42;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Indexed[0];"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (FCStringAnsi::Strcmp(FeatureCase.CatalogName, "abstract_class_modifier") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("abstract class FutureAbstract"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tabstract int Value();"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("class FutureConcrete : FutureAbstract"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 42;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int FeatureEntry()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFutureAbstract@ Value = FutureConcrete();"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value.Value();"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (FCStringAnsi::Strcmp(FeatureCase.CatalogName, "null_handle_syntax") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("class FutureNullHandle"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int FeatureEntry()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFutureNullHandle@ Value = FutureNullHandle();"));
			AppendGeneratedAsLine(Source, TEXT("\tValue = null;"));
			AppendGeneratedAsLine(Source, TEXT("\tif (Value is null)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 42;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 0;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("final class FutureFinal"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value = 42;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int FeatureEntry()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFutureFinal Value;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value.Value;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
	}

	static void AppendLayerEvidenceSource(FString& Source, const FLayerCase& LayerCase)
	{
		switch (LayerCase.EvidenceLayer)
		{
		case ESelected238EvidenceLayer::Parse:
			// The public raw SDK has no parser-only entry point: Build() owns parsing.
			// Keep this source to the feature itself so the observation is the section
			// admission plus Build's syntax result, without a synthetic caller or runtime.
			return;

		case ESelected238EvidenceLayer::Compile:
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int CompileEvidence()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn FeatureEntry();"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			return;

		case ESelected238EvidenceLayer::Metadata:
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("class MetadataEvidence"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint GetFeatureValue()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn FeatureEntry();"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			return;

		case ESelected238EvidenceLayer::Runtime:
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int RuntimeEvidence()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn FeatureEntry();"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			return;

		case ESelected238EvidenceLayer::Cleanup:
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("class CleanupEvidence"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value;"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tCleanupEvidence()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tValue = FeatureEntry();"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			return;
		}
	}

	static FString BuildSelected238Source(const FFeatureCase& FeatureCase, const FLayerCase& LayerCase)
	{
		FString Source;
		AppendFeatureSource(Source, FeatureCase);
		AppendLayerEvidenceSource(Source, LayerCase);
		return Source;
	}

	static int BuildSelected238Module(
		asIScriptEngine* ScriptEngine,
		const char* ModuleName,
		const char* Source,
		asIScriptModule*& OutModule,
		int& OutAddSectionResult)
	{
		OutModule = nullptr;
		OutAddSectionResult = asINVALID_ARG;
		if (ScriptEngine == nullptr || ModuleName == nullptr || Source == nullptr)
		{
			return asINVALID_ARG;
		}

		OutModule = ScriptEngine->GetModule(ModuleName, asGM_ALWAYS_CREATE);
		if (OutModule == nullptr)
		{
			return asERROR;
		}

		OutAddSectionResult = OutModule->AddScriptSection(ModuleName, Source);
		if (OutAddSectionResult < 0)
		{
			return OutAddSectionResult;
		}

		return OutModule->Build();
	}

public:
	TEST_METHOD(FeaturesByEvidenceLayer)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("V238-DESIRED-BEHAVIOR",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Selected 2.38 desired-behavior product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		for (const FFeatureCase& FeatureCase : FeatureCases)
		{
			for (const FLayerCase& LayerCase : LayerCases)
			{
				const FNativeCaseContext Case(MakeNativeCaseId("V238-DESIRED-BEHAVIOR", { ANSI_TO_TCHAR(FeatureCase.CatalogName), ANSI_TO_TCHAR(LayerCase.CatalogName) }));
				const FString ModuleName = TEXT("Selected238_") + Case.GetId().RightChop(22).Replace(TEXT("-"), TEXT("_"));
				const FString Source = BuildSelected238Source(FeatureCase, LayerCase);
				PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);
				const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
				const FTCHARToUTF8 SourceUtf8(*Source);
				Engine.ResetMessages();
				asIScriptModule* Module = nullptr;
				int AddSectionResult = asINVALID_ARG;
				const int BuildResult = BuildSelected238Module(ScriptEngine, ModuleNameUtf8.Get(), SourceUtf8.Get(), Module, AddSectionResult);
				ASSERT_THAT(AreEqual(asSUCCESS, AddSectionResult, *Case.Describe(TEXT("selected 2.38 source should be admitted as one raw SDK section before the evidence-specific build observation"))));
				ASSERT_THAT(IsNotNull(Module, *Case.Describe(TEXT("selected 2.38 source should retain its raw module while the selected observation is made"))));
				if (Module == nullptr)
				{
					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS), *Case.Describe(TEXT("selected 2.38 cell without a module should still leave no module registration"))));
					continue;
				}

				if (FeatureCase.bExpectedBuildFailure)
				{
					ASSERT_THAT(IsTrue(BuildResult < 0, *Case.Describe(TEXT("selected 2.38 ambiguous indexed setters should reject the conflicting setter family"))));
					switch (LayerCase.EvidenceLayer)
					{
					case ESelected238EvidenceLayer::Parse:
						ASSERT_THAT(IsTrue(Engine.GetMessages().Entries.Num() > 0, *Case.Describe(TEXT("selected 2.38 parse observation should retain the indexed-setter ambiguity diagnostic from Build"))));
						break;

					case ESelected238EvidenceLayer::Compile:
						ASSERT_THAT(IsNull(Module->GetFunctionByDecl("int CompileEvidence()"), *Case.Describe(TEXT("selected 2.38 compile observation should not publish a caller whose ambiguous feature dependency failed"))));
						break;

					case ESelected238EvidenceLayer::Metadata:
						ASSERT_THAT(IsNull(Module->GetTypeInfoByName("MetadataEvidence"), *Case.Describe(TEXT("selected 2.38 metadata observation should not publish a type from an ambiguous indexed-setter source"))));
						break;

					case ESelected238EvidenceLayer::Runtime:
						ASSERT_THAT(IsNull(Module->GetFunctionByDecl("int RuntimeEvidence()"), *Case.Describe(TEXT("selected 2.38 runtime observation should not expose an executable entry when its feature is rejected"))));
						break;

					case ESelected238EvidenceLayer::Cleanup:
						ASSERT_THAT(IsNotNull(Module, *Case.Describe(TEXT("selected 2.38 cleanup observation should retain the failed module until explicit discard"))));
						break;
					}
				}
				else
				{
					ASSERT_THAT(AreEqual(asSUCCESS, BuildResult, *Case.Describe(TEXT("selected 2.38 desired syntax should compile after its targeted backport"))));
					switch (LayerCase.EvidenceLayer)
					{
					case ESelected238EvidenceLayer::Parse:
						ASSERT_THAT(IsTrue(Engine.GetMessages().Entries.IsEmpty(), *Case.Describe(TEXT("selected 2.38 parse observation should complete feature syntax without diagnostics"))));
						break;

					case ESelected238EvidenceLayer::Compile:
						ASSERT_THAT(IsNotNull(Module->GetFunctionByDecl("int CompileEvidence()"), *Case.Describe(TEXT("selected 2.38 compile observation should type-check a dedicated caller of the feature"))));
						break;

					case ESelected238EvidenceLayer::Metadata:
					{
						asITypeInfo* const EvidenceType = Module->GetTypeInfoByName("MetadataEvidence");
						ASSERT_THAT(IsNotNull(EvidenceType, *Case.Describe(TEXT("selected 2.38 metadata observation should publish its feature-dependent evidence type"))));
						if (EvidenceType != nullptr)
						{
							ASSERT_THAT(IsNotNull(EvidenceType->GetMethodByDecl("int GetFeatureValue()"), *Case.Describe(TEXT("selected 2.38 metadata observation should publish the feature-dependent method declaration"))));
						}
						break;
					}

					case ESelected238EvidenceLayer::Runtime:
					{
						asIScriptFunction* const RuntimeEvidence = GetNativeFunctionByExactDecl(Module, "int RuntimeEvidence()");
						ASSERT_THAT(IsNotNull(RuntimeEvidence, *Case.Describe(TEXT("selected 2.38 runtime observation should publish its exact feature-dependent entry"))));
						if (RuntimeEvidence != nullptr)
						{
							asIScriptContext* const Context = ScriptEngine->CreateContext();
							ASSERT_THAT(IsNotNull(Context, *Case.Describe(TEXT("selected 2.38 runtime observation should create a raw context"))));
							if (Context != nullptr)
							{
								ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, PrepareAndExecute(Context, RuntimeEvidence), *Case.Describe(TEXT("selected 2.38 runtime observation should execute the selected feature"))));
								ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()), *Case.Describe(TEXT("selected 2.38 runtime observation should retain the feature result"))));
								Context->Release();
							}
						}
						break;
					}

					case ESelected238EvidenceLayer::Cleanup:
						ASSERT_THAT(IsNotNull(Module->GetTypeInfoByName("CleanupEvidence"), *Case.Describe(TEXT("selected 2.38 cleanup observation should create a feature-dependent script type before discard"))));
						break;
					}
				}
				ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
				ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS), *Case.Describe(TEXT("selected 2.38 desired cell should discard its module"))));
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
