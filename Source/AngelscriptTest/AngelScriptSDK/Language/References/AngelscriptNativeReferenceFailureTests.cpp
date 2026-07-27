#include "AngelscriptNativeReferenceTestSupport.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FReferenceFailureTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.References.Failure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext =
		AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeTestEngine =
		AngelscriptNativeTestSupport::FNativeTestEngine;
	using FReferenceState =
		AngelscriptNativeReferenceTestSupport::FReferenceState;

	enum class EReferenceFailure : uint8
	{
		ExplicitHandle,
		ConstRemoval,
		TemporaryOut,
		ExpiredLocalReturn,
		UnrelatedAssignment,
		UnrelatedCast,
		NullMemberAccess,
		StaleModuleObject,
		AmbiguousOverload,
		IncompatibleInOut,
	};

	enum class ERecoveryRoute : uint8
	{
		FreshModule,
		SameModuleOrContext,
	};

	struct FFailureCase
	{
		const ANSICHAR* CatalogName;
		EReferenceFailure Failure;
		const TCHAR* DiagnosticToken;
		const TCHAR* AlternateDiagnosticToken;
	};

	struct FRecoveryCase
	{
		const ANSICHAR* CatalogName;
		ERecoveryRoute Route;
	};

	inline static constexpr FFailureCase FailureCases[] =
	{
		{
			"explicit_handle",
			EReferenceFailure::ExplicitHandle,
			TEXT("@"),
			TEXT("Expected"),
		},
		{
			"const_removal",
			EReferenceFailure::ConstRemoval,
			TEXT("const"),
			TEXT("conversion"),
		},
		{
			"temporary_out",
			EReferenceFailure::TemporaryOut,
			TEXT("reference"),
			TEXT("out"),
		},
		{
			"expired_local_return",
			EReferenceFailure::ExpiredLocalReturn,
			TEXT("reference"),
			TEXT("local"),
		},
		{
			"unrelated_assignment",
			EReferenceFailure::UnrelatedAssignment,
			TEXT("conversion"),
			TEXT("FRefUnrelated"),
		},
		{
			"unrelated_cast",
			EReferenceFailure::UnrelatedCast,
			TEXT("cast"),
			TEXT("FRefUnrelated"),
		},
		{
			"null_member_access",
			EReferenceFailure::NullMemberAccess,
			TEXT("Null pointer"),
			nullptr,
		},
		{
			"stale_module_object",
			EReferenceFailure::StaleModuleObject,
			TEXT("FModuleOwnedReference"),
			TEXT("identifier"),
		},
		{
			"ambiguous_overload",
			EReferenceFailure::AmbiguousOverload,
			TEXT("Multiple matching"),
			TEXT("ambiguous"),
		},
		{
			"incompatible_inout",
			EReferenceFailure::IncompatibleInOut,
			TEXT("No matching signatures"),
			TEXT("MutateDerived"),
		},
	};

	inline static constexpr FRecoveryCase RecoveryCases[] =
	{
		{
			"fresh_module",
			ERecoveryRoute::FreshModule,
		},
		{
			"same_module_or_context",
			ERecoveryRoute::SameModuleOrContext,
		},
	};

	static bool IsRuntimeFailure(
		const FFailureCase& Failure)
	{
		return Failure.Failure
			== EReferenceFailure::NullMemberAccess;
	}

	static bool NeedsProvider(
		const FFailureCase& Failure)
	{
		return Failure.Failure
			== EReferenceFailure::StaleModuleObject;
	}

	static bool NeedsAmbiguousCandidates(
		const FFailureCase& Failure)
	{
		return Failure.Failure
			== EReferenceFailure::AmbiguousOverload;
	}

	static void AppendRecoveryFunction(
		FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			TEXT("int RecoverReferenceFailure()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFRefRoot Value = MakeRefRoot(929);"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\treturn Value.GetValue();"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static FString BuildReferenceFailureSource(
		const FFailureCase& Failure)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		switch (Failure.Failure)
		{
		case EReferenceFailure::ExplicitHandle:
			AppendGeneratedAsLine(
				Source,
				TEXT("int RunReferenceFailure()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\tFRefRoot@ Explicit = MakeRefRoot(1);"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\treturn Explicit.GetIdentity();"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			break;
		case EReferenceFailure::ConstRemoval:
			AppendGeneratedAsLine(
				Source,
				TEXT("int RunReferenceFailure()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\tconst FRefRoot ReadOnly = MakeRefRoot(2);"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\tFRefRoot Mutable = ReadOnly;"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\tMutable.SetValue(3);"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\treturn Mutable.GetValue();"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			break;
		case EReferenceFailure::TemporaryOut:
			AppendGeneratedAsLine(
				Source,
				TEXT("void WriteTemporaryOut(FRefRoot& out Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\tValue = MakeRefRoot(3);"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(
				Source,
				TEXT("int RunReferenceFailure()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\tWriteTemporaryOut(MakeRefRoot(4));"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\treturn 0;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			break;
		case EReferenceFailure::ExpiredLocalReturn:
			AppendGeneratedAsLine(
				Source,
				TEXT("int& ReturnExpiredLocal()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\tint Local = 5;"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\treturn Local;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(
				Source,
				TEXT("int RunReferenceFailure()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\tint& Alias = ReturnExpiredLocal();"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\treturn Alias;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			break;
		case EReferenceFailure::UnrelatedAssignment:
			AppendGeneratedAsLine(
				Source,
				TEXT("int RunReferenceFailure()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\tFRefRoot Root = MakeRefRoot(6);"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\tFRefUnrelated Other = MakeRefUnrelated(7);"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\tRoot = Other;"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\treturn Root.GetValue();"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			break;
		case EReferenceFailure::UnrelatedCast:
			AppendGeneratedAsLine(
				Source,
				TEXT("int RunReferenceFailure()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\tFRefUnrelated Other = MakeRefUnrelated(8);"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\tFRefDerived Result = cast<FRefDerived>(Other);"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\treturn Result.GetValue();"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			break;
		case EReferenceFailure::NullMemberAccess:
			AppendGeneratedAsLine(
				Source,
				TEXT("int RunReferenceFailure()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\tFRefRoot Missing = nullptr;"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\treturn Missing.GetValue();"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendRecoveryFunction(Source);
			break;
		case EReferenceFailure::StaleModuleObject:
			AppendGeneratedAsLine(
				Source,
				TEXT("int RunReferenceFailure(FModuleOwnedReference Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\treturn Value.Value;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			break;
		case EReferenceFailure::AmbiguousOverload:
			AppendGeneratedAsLine(
				Source,
				TEXT("int RunReferenceFailure()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\treturn SelectAmbiguous(nullptr);"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			break;
		case EReferenceFailure::IncompatibleInOut:
			AppendGeneratedAsLine(
				Source,
				TEXT("void MutateDerived(FRefDerived& inout Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\tValue.SetValue(10);"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(
				Source,
				TEXT("int RunReferenceFailure()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\tFRefRoot Root = MakeRefRoot(9);"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\tMutateDerived(Root);"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\treturn Root.GetValue();"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			break;
		default:
			break;
		}
		return Source;
	}

	static FString BuildReferenceFailureRecoverySource()
	{
		FString Source;
		AppendRecoveryFunction(Source);
		return Source;
	}

	static FString BuildStaleProviderSource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(
			Source,
			TEXT("class FModuleOwnedReference"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tint Value = 41;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("int ProviderMarker()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 41;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static int CompileAndReport(
		FAutomationTestBase& Test,
		asIScriptEngine& ScriptEngine,
		const FString& SourceId,
		const FString& ModuleName,
		const FString& Source,
		asIScriptModule*& OutModule)
	{
		AngelscriptNativeTestSupport::
			PrintGeneratedAsSource(
				Test,
				SourceId,
				ModuleName,
				Source);
		const FTCHARToUTF8 ModuleNameUtf8(
			*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		return AngelscriptNativeTestSupport::
			CompileNativeModule(
				&ScriptEngine,
				ModuleNameUtf8.Get(),
				SourceUtf8.Get(),
				OutModule);
	}

	static void GenericAmbiguousMarker(
		asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			Generic->SetReturnDWord(1);
		}
	}

	static bool RegisterAmbiguousCandidates(
		asIScriptEngine& ScriptEngine)
	{
		return ScriptEngine.RegisterGlobalFunction(
			"int SelectAmbiguous(const FRefRoot Value)",
			asFUNCTION(GenericAmbiguousMarker),
			asCALL_GENERIC) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"int SelectAmbiguous(const FRefUnrelated Value)",
				asFUNCTION(GenericAmbiguousMarker),
				asCALL_GENERIC) >= 0;
	}

	static bool HasLocatedDiagnostic(
		const FNativeTestEngine& Engine,
		const FFailureCase& Failure)
	{
		for (const AngelscriptNativeTestSupport::
			FNativeMessageEntry& Message
			: Engine.GetMessages().Entries)
		{
			if (Message.Type != asMSGTYPE_ERROR
				|| Message.Row <= 0)
			{
				continue;
			}
			if (Message.Message.Contains(
				Failure.DiagnosticToken,
				ESearchCase::IgnoreCase)
				|| (Failure.AlternateDiagnosticToken
					!= nullptr
					&& Message.Message.Contains(
						Failure.AlternateDiagnosticToken,
						ESearchCase::IgnoreCase)))
			{
				return true;
			}
		}
		return false;
	}

	void BuildAndDiscardProvider(
		const FNativeCaseContext& Case,
		FNativeTestEngine& Engine,
		asIScriptEngine& ScriptEngine,
		const FString& ProviderName)
	{
		const FString Source =
			BuildStaleProviderSource();
		Engine.ResetMessages();
		asIScriptModule* Module = nullptr;
		ASSERT_THAT(IsTrue(
			CompileAndReport(
				*TestRunner,
				ScriptEngine,
				Case.GetId()
					+ TEXT("-PROVIDER"),
				ProviderName,
				Source,
				Module) >= 0,
			*Case.Describe(TEXT("stale-module boundary provider should compile"))));
		ASSERT_THAT(IsNotNull(Module,
			*Case.Describe(TEXT("stale-module boundary provider should publish a module"))));
		if (Module != nullptr)
		{
			asITypeInfo* const Type =
				Module->GetTypeInfoByName(
					"FModuleOwnedReference");
			ASSERT_THAT(IsNotNull(Type,
				*Case.Describe(TEXT("stale-module boundary provider should publish its module-owned type"))));
			ASSERT_THAT(IsNotNull(
				Module->GetFunctionByDecl(
					"int ProviderMarker()"),
				*Case.Describe(TEXT("stale-module boundary provider should publish its marker"))));
		}
		AngelscriptNativeReferenceTestSupport::
			DiscardReferenceModule(
				ScriptEngine,
				ProviderName);
		ASSERT_THAT(IsNull(
			ScriptEngine.GetModule(
				TCHAR_TO_UTF8(*ProviderName),
				asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("stale-module boundary should remove the provider before the consumer compiles"))));
	}

	void ExecuteRecoveryModule(
		const FNativeCaseContext& Case,
		FNativeTestEngine& Engine,
		asIScriptEngine& ScriptEngine,
		const FString& ModuleName)
	{
		const FString Source =
			BuildReferenceFailureRecoverySource();
		Engine.ResetMessages();
		asIScriptModule* Module = nullptr;
		ASSERT_THAT(IsTrue(
			CompileAndReport(
				*TestRunner,
				ScriptEngine,
				Case.GetId()
					+ TEXT("-RECOVERY"),
				ModuleName,
				Source,
				Module) >= 0,
			*Case.Describe(TEXT("reference failure recovery source should compile"))));
		ASSERT_THAT(IsNotNull(Module,
			*Case.Describe(TEXT("reference failure recovery should publish a module"))));
		ASSERT_THAT(IsFalse(
			AngelscriptNativeReferenceTestSupport::
				HasAnyError(Engine),
			*Case.Describe(TEXT("reference failure recovery should emit no errors"))));
		if (Module != nullptr)
		{
			asIScriptFunction* const Recovery =
				Module->GetFunctionByDecl(
					"int RecoverReferenceFailure()");
			asIScriptContext* const Context =
				ScriptEngine.CreateContext();
			ASSERT_THAT(IsNotNull(Recovery,
				*Case.Describe(TEXT("reference failure recovery should publish its exact function"))));
			ASSERT_THAT(IsNotNull(Context,
				*Case.Describe(TEXT("reference failure recovery should create a context"))));
			if (Recovery != nullptr
				&& Context != nullptr)
			{
				ASSERT_THAT(AreEqual(
					asSUCCESS,
					Context->Prepare(Recovery),
					*Case.Describe(TEXT("reference failure recovery should prepare"))));
				ASSERT_THAT(AreEqual(
					static_cast<int32>(asEXECUTION_FINISHED),
					Context->Execute(),
					*Case.Describe(TEXT("reference failure recovery should finish"))));
				ASSERT_THAT(AreEqual(
					929,
					static_cast<int32>(
						Context->GetReturnDWord()),
					*Case.Describe(TEXT("reference failure recovery should return its sentinel"))));
				ASSERT_THAT(AreEqual(
					asSUCCESS,
					Context->Unprepare(),
					*Case.Describe(TEXT("reference failure recovery should unprepare"))));
			}
			if (Context != nullptr)
			{
				Context->Release();
			}
		}
		AngelscriptNativeReferenceTestSupport::
			DiscardReferenceModule(
				ScriptEngine,
				ModuleName);
	}

	void ExecuteNullFailureAndRecover(
		const FNativeCaseContext& Case,
		const FRecoveryCase& RecoveryRoute,
		FNativeTestEngine& Engine,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module,
		const FString& ModuleName)
	{
		asIScriptFunction* const Failure =
			Module.GetFunctionByDecl(
				"int RunReferenceFailure()");
		asIScriptFunction* const Recovery =
			Module.GetFunctionByDecl(
				"int RecoverReferenceFailure()");
		asIScriptContext* const Context =
			ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Failure,
			*Case.Describe(TEXT("null reference failure should publish its exact entry"))));
		ASSERT_THAT(IsNotNull(Recovery,
			*Case.Describe(TEXT("null reference failure should publish same-context recovery"))));
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("null reference failure should create a context"))));
		if (Failure != nullptr
			&& Context != nullptr)
		{
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Context->Prepare(Failure),
				*Case.Describe(TEXT("null reference failure should prepare"))));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asEXECUTION_EXCEPTION),
				Context->Execute(),
				*Case.Describe(TEXT("null reference member access should raise an execution exception"))));
			ASSERT_THAT(IsTrue(
				FString(UTF8_TO_TCHAR(
					Context->GetExceptionString()
							!= nullptr
						? Context
							->GetExceptionString()
						: "")).Contains(
							TEXT("Null pointer"),
							ESearchCase::IgnoreCase),
				*Case.Describe(TEXT("null reference member access should own the null-pointer exception"))));
			ASSERT_THAT(IsTrue(
				Context->GetExceptionLineNumber() > 0,
				*Case.Describe(TEXT("null reference exception should retain a source line"))));
			ASSERT_THAT(AreEqual(
				Failure,
				Context->GetExceptionFunction(),
				*Case.Describe(TEXT("null reference exception should retain the failing function"))));
		}
		if (RecoveryRoute.Route
			== ERecoveryRoute::SameModuleOrContext)
		{
			if (Recovery != nullptr
				&& Context != nullptr)
			{
				ASSERT_THAT(AreEqual(
					asSUCCESS,
					Context->Unprepare(),
					*Case.Describe(TEXT("null reference failure should unprepare before same-context recovery"))));
				ASSERT_THAT(AreEqual(
					asSUCCESS,
					Context->Prepare(Recovery),
					*Case.Describe(TEXT("null reference recovery should prepare on the same context"))));
				ASSERT_THAT(AreEqual(
					static_cast<int32>(asEXECUTION_FINISHED),
					Context->Execute(),
					*Case.Describe(TEXT("null reference same-context recovery should finish"))));
				ASSERT_THAT(AreEqual(
					929,
					static_cast<int32>(
						Context->GetReturnDWord()),
					*Case.Describe(TEXT("null reference same-context recovery should return its sentinel"))));
				Context->Unprepare();
			}
			if (Context != nullptr)
			{
				Context->Release();
			}
			AngelscriptNativeReferenceTestSupport::
				DiscardReferenceModule(
					ScriptEngine,
					ModuleName);
		}
		else
		{
			if (Context != nullptr)
			{
				Context->Release();
			}
			AngelscriptNativeReferenceTestSupport::
				DiscardReferenceModule(
					ScriptEngine,
					ModuleName);
			ExecuteRecoveryModule(
				Case,
				Engine,
				ScriptEngine,
				ModuleName
					+ TEXT("_Fresh"));
		}
	}

	void RunCell(
		const FFailureCase& Failure,
		const FRecoveryCase& RecoveryRoute)
	{
		using namespace AngelscriptNativeReferenceTestSupport;
		using namespace AngelscriptNativeTestSupport;

		const FNativeCaseContext Case(MakeNativeCaseId(
			"LANG-REF-FAILURE",
			{
				ANSI_TO_TCHAR(Failure.CatalogName),
				ANSI_TO_TCHAR(RecoveryRoute.CatalogName),
			}));
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const ScriptEngine =
			Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			*Case.Describe(TEXT("reference failure cell should create a raw engine"))));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		FReferenceState State;
		State.ResetCounters();
		ASSERT_THAT(IsTrue(
			RegisterReferenceFixtures(
				*ScriptEngine,
				State),
			*Case.Describe(TEXT("reference failure cell should register core reference fixtures"))));
		if (NeedsAmbiguousCandidates(Failure))
		{
			ASSERT_THAT(IsTrue(
				RegisterAmbiguousCandidates(
					*ScriptEngine),
				*Case.Describe(TEXT("ambiguous reference failure should register both unrelated candidates"))));
		}
		const FString ModuleName = FString::Printf(
			TEXT("ReferenceFailure_%s"),
			*Case.GetId());
		if (NeedsProvider(Failure))
		{
			BuildAndDiscardProvider(
				Case,
				Engine,
				*ScriptEngine,
				ModuleName
					+ TEXT("_Provider"));
		}
		const FString Source =
			BuildReferenceFailureSource(Failure);
		Engine.ResetMessages();
		asIScriptModule* Module = nullptr;
		const int BuildResult =
			CompileAndReport(
				*TestRunner,
				*ScriptEngine,
				Case.GetId(),
				ModuleName,
				Source,
				Module);
		if (IsRuntimeFailure(Failure))
		{
			ASSERT_THAT(IsTrue(
				BuildResult >= 0,
				*Case.DescribeResult(
					TEXT("runtime reference failure source"),
					TEXT("successful build"),
					DescribeReferenceBuild(
						Engine,
						BuildResult))));
			ASSERT_THAT(IsNotNull(Module,
				*Case.Describe(TEXT("runtime reference failure source should publish a module"))));
			ASSERT_THAT(IsFalse(
				HasAnyError(Engine),
				*Case.Describe(TEXT("runtime reference failure source should emit no build errors"))));
			if (Module != nullptr)
			{
				ExecuteNullFailureAndRecover(
					Case,
					RecoveryRoute,
					Engine,
					*ScriptEngine,
					*Module,
					ModuleName);
			}
		}
		else
		{
			ASSERT_THAT(IsTrue(
				BuildResult < 0,
				*Case.DescribeResult(
					TEXT("compile-time reference failure source"),
					TEXT("negative build result"),
					DescribeReferenceBuild(
						Engine,
						BuildResult))));
			ASSERT_THAT(IsTrue(
				HasLocatedDiagnostic(
					Engine,
					Failure),
				*Case.DescribeResult(
					TEXT("compile-time reference failure diagnostic"),
					TEXT("located causal diagnostic"),
					DescribeReferenceBuild(
						Engine,
						BuildResult))));
			ASSERT_THAT(AreEqual(
				0,
				State.Created,
				*Case.Describe(TEXT("compile-time reference failure should execute no factory"))));
			DiscardReferenceModule(
				*ScriptEngine,
				ModuleName);
			const FString RecoveryName =
				RecoveryRoute.Route
						== ERecoveryRoute::FreshModule
					? ModuleName
						+ TEXT("_Fresh")
					: ModuleName;
			ExecuteRecoveryModule(
				Case,
				Engine,
				*ScriptEngine,
				RecoveryName);
		}
		State.BreakAllCycles();
		State.ReleaseRetainedNativeObject();
		ASSERT_THAT(AreEqual(
			0,
			State.LiveObjects,
			*Case.DescribeResult(
				TEXT("reference failure cleanup"),
				TEXT("Live=0 after recovery"),
				DescribeReferenceState(State))));
		ASSERT_THAT(AreEqual(
			State.Created,
			State.Destroyed,
			*Case.Describe(TEXT("reference failure and recovery should destroy every created identity"))));
	}

public:
	TEST_METHOD(FailuresByRecovery)
	{
		AS_NATIVE_PRODUCT("LANG-REF-FAILURE",
			AngelscriptNativeTestSupport::ENativeEvidence::Compile
				| AngelscriptNativeTestSupport::ENativeEvidence::Diagnostic
				| AngelscriptNativeTestSupport::ENativeEvidence::Runtime
				| AngelscriptNativeTestSupport::ENativeEvidence::Metadata
				| AngelscriptNativeTestSupport::ENativeEvidence::Lifecycle
				| AngelscriptNativeTestSupport::ENativeEvidence::Cleanup
				| AngelscriptNativeTestSupport::ENativeEvidence::Isolation);

		for (const FFailureCase& Failure
			: FailureCases)
		{
			for (const FRecoveryCase& Recovery
				: RecoveryCases)
			{
				RunCell(
					Failure,
					Recovery);
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
