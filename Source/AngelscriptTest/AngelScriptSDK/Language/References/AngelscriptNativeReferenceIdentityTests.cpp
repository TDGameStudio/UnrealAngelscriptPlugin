#include "AngelscriptNativeReferenceTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FReferenceIdentityTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.References.Identity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext =
		AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeTestEngine =
		AngelscriptNativeTestSupport::FNativeTestEngine;
	using FReferenceState =
		AngelscriptNativeReferenceTestSupport::FReferenceState;

	enum class EReferenceSource : uint8
	{
		NewLocal,
		Field,
		Parameter,
		Return,
		BaseView,
		DerivedView,
		NativeObject,
		Null,
	};

	enum class EReferenceOperation : uint8
	{
		Initialize,
		Assign,
		Pass,
		Return,
		Identity,
		NullCompare,
		Cast,
		MemberAccess,
		AliasMutation,
	};

	enum class EReferenceQualifier : uint8
	{
		Mutable,
		ConstObject,
		ConstInput,
		ConstRemovalInvalid,
	};

	struct FSourceCase
	{
		const ANSICHAR* CatalogName;
		EReferenceSource Source;
		int32 Value;
	};

	struct FOperationCase
	{
		const ANSICHAR* CatalogName;
		EReferenceOperation Operation;
	};

	struct FQualifierCase
	{
		const ANSICHAR* CatalogName;
		EReferenceQualifier Qualifier;
	};

	inline static constexpr FSourceCase SourceCases[] =
	{
		{ "new_local", EReferenceSource::NewLocal, 11 },
		{ "field", EReferenceSource::Field, 12 },
		{ "parameter", EReferenceSource::Parameter, 13 },
		{ "return", EReferenceSource::Return, 14 },
		{ "base_view", EReferenceSource::BaseView, 15 },
		{ "derived_view", EReferenceSource::DerivedView, 16 },
		{ "native_object", EReferenceSource::NativeObject, 17 },
		{ "null", EReferenceSource::Null, 0 },
	};

	inline static constexpr FOperationCase OperationCases[] =
	{
		{ "initialize", EReferenceOperation::Initialize },
		{ "assign", EReferenceOperation::Assign },
		{ "pass", EReferenceOperation::Pass },
		{ "return", EReferenceOperation::Return },
		{ "identity", EReferenceOperation::Identity },
		{ "null_compare", EReferenceOperation::NullCompare },
		{ "cast", EReferenceOperation::Cast },
		{ "member_access", EReferenceOperation::MemberAccess },
		{ "alias_mutation", EReferenceOperation::AliasMutation },
	};

	inline static constexpr FQualifierCase QualifierCases[] =
	{
		{ "mutable", EReferenceQualifier::Mutable },
		{ "const_object", EReferenceQualifier::ConstObject },
		{ "const_input", EReferenceQualifier::ConstInput },
		{
			"const_removal_invalid",
			EReferenceQualifier::ConstRemovalInvalid,
		},
	};

	static bool IsDerivedStaticView(
		const FSourceCase& Source)
	{
		return Source.Source
			== EReferenceSource::DerivedView;
	}

	static bool IsDynamicallyDerived(
		const FSourceCase& Source)
	{
		return Source.Source
				== EReferenceSource::BaseView
			|| Source.Source
				== EReferenceSource::DerivedView;
	}

	static bool IsNullSource(
		const FSourceCase& Source)
	{
		return Source.Source
			== EReferenceSource::Null;
	}

	static bool IsCompileFailure(
		const FOperationCase& Operation,
		const FQualifierCase& Qualifier)
	{
		if (Qualifier.Qualifier
			== EReferenceQualifier::ConstRemovalInvalid)
		{
			return true;
		}
		return Operation.Operation
				== EReferenceOperation::AliasMutation
			&& Qualifier.Qualifier
				!= EReferenceQualifier::Mutable;
	}

	static bool IsRuntimeNullFailure(
		const FSourceCase& Source,
		const FOperationCase& Operation,
		const FQualifierCase& Qualifier)
	{
		if (IsCompileFailure(
			Operation,
			Qualifier))
		{
			return false;
		}
		return IsNullSource(Source)
			&& (Operation.Operation
					== EReferenceOperation::MemberAccess
				|| Operation.Operation
					== EReferenceOperation::Cast
				|| Operation.Operation
					== EReferenceOperation::AliasMutation);
	}

	static FString StaticType(
		const FSourceCase& Source)
	{
		return IsDerivedStaticView(Source)
			? TEXT("FRefDerived")
			: TEXT("FRefRoot");
	}

	static FString ParameterDeclaration(
		const FSourceCase& Source,
		const FQualifierCase& Qualifier)
	{
		const FString Type = StaticType(Source);
		switch (Qualifier.Qualifier)
		{
		case EReferenceQualifier::Mutable:
			return Type + TEXT(" Source");
		case EReferenceQualifier::ConstObject:
			return TEXT("const ") + Type
				+ TEXT(" Source");
		case EReferenceQualifier::ConstInput:
		case EReferenceQualifier::ConstRemovalInvalid:
			return TEXT("const ") + Type
				+ TEXT("& in Source");
		default:
			return Type + TEXT(" Source");
		}
	}

	static FString LocalViewType(
		const FSourceCase& Source,
		const FQualifierCase& Qualifier)
	{
		return Qualifier.Qualifier
				== EReferenceQualifier::Mutable
			? StaticType(Source)
			: TEXT("const ") + StaticType(Source);
	}

	static void AppendCommonHelpers(
		FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			TEXT("int ObserveReference(const FRefRoot Object)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\treturn Object == nullptr"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\t? -1"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\t: Object.GetValue();"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("FRefRoot ReturnReference(FRefRoot Object)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\treturn Object;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("const FRefRoot ReturnConstReference(const FRefRoot Object)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\treturn Object;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("void RequireMutable(FRefRoot& inout Object)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tif (Object == nullptr)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tObject = MakeRefRoot(81);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\telse"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\tObject.SetValue(82);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendSourceProvider(
		FString& Source,
		const FSourceCase& SourceCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (SourceCase.Source
			== EReferenceSource::Return)
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("FRefRoot ProvideReference()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(
				Source,
				FString::Printf(
					TEXT("\treturn MakeRefRoot(%d);"),
					SourceCase.Value));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		if (SourceCase.Source
			== EReferenceSource::Field)
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("struct FReferenceFieldOwner"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\tFRefRoot Field;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
	}

	static FString SourceExpression(
		const FSourceCase& Source)
	{
		switch (Source.Source)
		{
		case EReferenceSource::NewLocal:
			return FString::Printf(
				TEXT("MakeRefRoot(%d)"),
				Source.Value);
		case EReferenceSource::Field:
			return TEXT("Owner.Field");
		case EReferenceSource::Parameter:
			return TEXT("ParameterSource");
		case EReferenceSource::Return:
			return TEXT("ProvideReference()");
		case EReferenceSource::BaseView:
			return FString::Printf(
				TEXT("MakeRefDerivedAsRoot(%d)"),
				Source.Value);
		case EReferenceSource::DerivedView:
			return FString::Printf(
				TEXT("MakeRefDerived(%d)"),
				Source.Value);
		case EReferenceSource::NativeObject:
			return FString::Printf(
				TEXT("GetNativeRef(%d)"),
				Source.Value);
		case EReferenceSource::Null:
			return TEXT("nullptr");
		default:
			return TEXT("nullptr");
		}
	}

	static void AppendOperation(
		FString& Source,
		const FSourceCase& SourceCase,
		const FOperationCase& Operation,
		const FQualifierCase& Qualifier)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString Type = LocalViewType(
			SourceCase,
			Qualifier);
		switch (Operation.Operation)
		{
		case EReferenceOperation::Initialize:
			AppendGeneratedAsLine(
				Source,
				TEXT("\t") + Type
					+ TEXT(" Alias = Source;"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\treturn SameReference(Source, Alias)"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\t\t? 1"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\t\t: 0;"));
			break;
		case EReferenceOperation::Assign:
			AppendGeneratedAsLine(
				Source,
				TEXT("\t") + Type
					+ (IsDerivedStaticView(SourceCase)
						? TEXT(" Alias = MakeRefDerived(91);")
						: TEXT(" Alias = MakeRefRoot(91);")));
			AppendGeneratedAsLine(
				Source,
				TEXT("\tAlias = Source;"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\treturn SameReference(Source, Alias)"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\t\t? 1"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\t\t: 0;"));
			break;
		case EReferenceOperation::Pass:
			AppendGeneratedAsLine(
				Source,
				IsNullSource(SourceCase)
					? TEXT("\treturn ObserveReference(Source) == -1 ? 1 : 0;")
					: FString::Printf(
						TEXT("\treturn ObserveReference(Source) == %d ? 1 : 0;"),
						SourceCase.Value));
			break;
		case EReferenceOperation::Return:
			AppendGeneratedAsLine(
				Source,
				Qualifier.Qualifier
						== EReferenceQualifier::Mutable
					? TEXT("\tFRefRoot Returned = ReturnReference(Source);")
					: TEXT("\tconst FRefRoot Returned = ReturnConstReference(Source);"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\treturn SameReference(Source, Returned)"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\t\t? 1"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\t\t: 0;"));
			break;
		case EReferenceOperation::Identity:
			AppendGeneratedAsLine(
				Source,
				TEXT("\t") + Type
					+ TEXT(" Alias = Source;"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\treturn SameReference(Source, Alias)"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\t\t&& Source == Alias"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\t\t? 1"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\t\t: 0;"));
			break;
		case EReferenceOperation::NullCompare:
			AppendGeneratedAsLine(
				Source,
				IsNullSource(SourceCase)
					? TEXT("\treturn Source == nullptr ? 1 : 0;")
					: TEXT("\treturn Source != nullptr ? 1 : 0;"));
			break;
		case EReferenceOperation::Cast:
			if (IsDerivedStaticView(SourceCase))
			{
				AppendGeneratedAsLine(
					Source,
					Qualifier.Qualifier
							== EReferenceQualifier::Mutable
						? TEXT("\tFRefRoot Casted = Source.opImplCast();")
						: TEXT("\tconst FRefRoot Casted = Source.opImplCast();"));
				AppendGeneratedAsLine(
					Source,
					TEXT("\treturn Casted != nullptr"));
				AppendGeneratedAsLine(
					Source,
					TEXT("\t\t&& Casted.GetKind() == 202"));
				AppendGeneratedAsLine(
					Source,
					TEXT("\t\t&& SameReference(Source, Casted)"));
				AppendGeneratedAsLine(
					Source,
					TEXT("\t\t? 1"));
				AppendGeneratedAsLine(
					Source,
					TEXT("\t\t: 0;"));
			}
			else
			{
				AppendGeneratedAsLine(
					Source,
					Qualifier.Qualifier
							== EReferenceQualifier::Mutable
						? TEXT("\tFRefDerived Casted = Source.opCast();")
						: TEXT("\tconst FRefDerived Casted = Source.opCast();"));
				if (IsDynamicallyDerived(SourceCase))
				{
					AppendGeneratedAsLine(
						Source,
						TEXT("\treturn Casted != nullptr"));
					AppendGeneratedAsLine(
						Source,
						TEXT("\t\t&& Casted.GetKind() == 202"));
					AppendGeneratedAsLine(
						Source,
						TEXT("\t\t&& SameReference(Source, Casted)"));
					AppendGeneratedAsLine(
						Source,
						TEXT("\t\t? 1"));
					AppendGeneratedAsLine(
						Source,
						TEXT("\t\t: 0;"));
				}
				else
				{
					AppendGeneratedAsLine(
						Source,
						TEXT("\treturn Casted == nullptr ? 1 : 0;"));
				}
			}
			break;
		case EReferenceOperation::MemberAccess:
			AppendGeneratedAsLine(
				Source,
				FString::Printf(
					TEXT("\treturn Source.GetValue() == %d ? 1 : 0;"),
					SourceCase.Value));
			break;
		case EReferenceOperation::AliasMutation:
			AppendGeneratedAsLine(
				Source,
				TEXT("\t") + Type
					+ TEXT(" Alias = Source;"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\tAlias.SetValue(77);"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\treturn Source.GetValue() == 77"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\t\t&& SameReference(Source, Alias)"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\t\t? 1"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\t\t: 0;"));
			break;
		default:
			AppendGeneratedAsLine(Source, TEXT("\treturn 0;"));
			break;
		}
	}

	static void AppendExerciseFunction(
		FString& Source,
		const FSourceCase& SourceCase,
		const FOperationCase& Operation,
		const FQualifierCase& Qualifier)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			TEXT("int ExerciseReference(")
				+ ParameterDeclaration(
					SourceCase,
					Qualifier)
				+ TEXT(")"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendOperation(
			Source,
			SourceCase,
			Operation,
			Qualifier);
		if (Qualifier.Qualifier
			== EReferenceQualifier::ConstRemovalInvalid)
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("\tRequireMutable(Source);"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendEntry(
		FString& Source,
		const FSourceCase& SourceCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			TEXT("int RunReferenceIdentity()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (SourceCase.Source
			== EReferenceSource::Field)
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("\tFReferenceFieldOwner Owner;"));
			AppendGeneratedAsLine(
				Source,
				FString::Printf(
					TEXT("\tOwner.Field = MakeRefRoot(%d);"),
					SourceCase.Value));
		}
		else if (SourceCase.Source
			== EReferenceSource::Parameter)
		{
			AppendGeneratedAsLine(
				Source,
				FString::Printf(
					TEXT("\tFRefRoot ParameterSource = MakeRefRoot(%d);"),
					SourceCase.Value));
		}
		AppendGeneratedAsLine(
			Source,
			TEXT("\treturn ExerciseReference(")
				+ SourceExpression(SourceCase)
				+ TEXT(");"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("int RecoverReferenceIdentity()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 913;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static FString BuildReferenceIdentitySource(
		const FSourceCase& SourceCase,
		const FOperationCase& Operation,
		const FQualifierCase& Qualifier)
	{
		FString Source;
		AppendCommonHelpers(Source);
		AppendSourceProvider(
			Source,
			SourceCase);
		AppendExerciseFunction(
			Source,
			SourceCase,
			Operation,
			Qualifier);
		AppendEntry(
			Source,
			SourceCase);
		return Source;
	}

	static FString BuildReferenceIdentityRecoverySource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(
			Source,
			TEXT("int RecoverReferenceIdentity()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 913;"));
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
		AngelscriptNativeTestSupport::PrintGeneratedAsSource(
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

	void VerifyExerciseMetadata(
		const FNativeCaseContext& Case,
		const FSourceCase& SourceCase,
		const FQualifierCase& Qualifier,
		asIScriptModule& Module)
	{
		const FString Declaration =
			TEXT("int ExerciseReference(")
			+ ParameterDeclaration(
				SourceCase,
				Qualifier)
			+ TEXT(")");
		const FTCHARToUTF8 DeclarationUtf8(
			*Declaration);
		asIScriptFunction* const Function =
			Module.GetFunctionByDecl(
				DeclarationUtf8.Get());
		ASSERT_THAT(IsNotNull(Function,
			*Case.Describe(TEXT("reference operation should publish the exact qualified exercise declaration"))));
		if (Function == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			1u,
			Function->GetParamCount(),
			*Case.Describe(TEXT("reference exercise should publish one source parameter"))));
		const char* ParameterName = nullptr;
		int TypeId = asTYPEID_VOID;
		asDWORD Modifier = asTM_NONE;
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Function->GetParam(
				0,
				&TypeId,
				&Modifier,
				&ParameterName),
			*Case.Describe(TEXT("reference exercise parameter metadata should be readable"))));
		ASSERT_THAT(AreEqual(
			FString(TEXT("Source")),
			FString(UTF8_TO_TCHAR(
				ParameterName != nullptr
					? ParameterName
					: "")),
			*Case.Describe(TEXT("reference exercise should retain the source parameter name"))));
		const bool bInputReference =
			Qualifier.Qualifier
					== EReferenceQualifier::ConstInput
				|| Qualifier.Qualifier
					== EReferenceQualifier::ConstRemovalInvalid;
		ASSERT_THAT(AreEqual(
			static_cast<asDWORD>(
				bInputReference
					? asTM_INREF
					: asTM_NONE),
			Modifier,
			*Case.Describe(TEXT("reference exercise should retain the requested parameter direction"))));
		ASSERT_THAT(AreEqual(
			asTYPEID_INT32,
			Function->GetReturnTypeId(),
			*Case.Describe(TEXT("reference exercise should retain its int result type"))));
	}

	void ExecuteRecovery(
		const FNativeCaseContext& Case,
		asIScriptContext& Context,
		asIScriptFunction& Recovery)
	{
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context.Prepare(&Recovery),
			*Case.Describe(TEXT("reference identity recovery should prepare on the same context"))));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			Context.Execute(),
			*Case.Describe(TEXT("reference identity recovery should finish"))));
		ASSERT_THAT(AreEqual(
			913,
			static_cast<int32>(
				Context.GetReturnDWord()),
			*Case.Describe(TEXT("reference identity recovery should return its sentinel"))));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context.Unprepare(),
			*Case.Describe(TEXT("reference identity recovery should unprepare cleanly"))));
	}

	void VerifyLifecycleCleanup(
		const FNativeCaseContext& Case,
		FReferenceState& State)
	{
		State.ReleaseRetainedNativeObject();
		ASSERT_THAT(AreEqual(
			0,
			State.LiveObjects,
			*Case.DescribeResult(
				TEXT("reference identity cleanup"),
				TEXT("Live=0 after module discard"),
				DescribeReferenceState(State))));
		ASSERT_THAT(AreEqual(
			State.Created,
			State.Destroyed,
			*Case.Describe(TEXT("reference identity cell should destroy every created object exactly once"))));
		TArray<int32> Created =
			State.CreatedIdentities;
		TArray<int32> Destroyed =
			State.DestroyedIdentities;
		Created.Sort();
		Destroyed.Sort();
		ASSERT_THAT(AreEqual(
			Created,
			Destroyed,
			*Case.Describe(TEXT("reference identity cell should destroy the exact created identities"))));
		ASSERT_THAT(IsTrue(
			State.ReleaseCalls <= State.AddRefCalls
				+ State.Created,
			*Case.Describe(TEXT("reference identity releases should never exceed owned references"))));
	}

	void CompileAndRunRecoveryModule(
		const FNativeCaseContext& Case,
		FNativeTestEngine& Engine,
		asIScriptEngine& ScriptEngine,
		const FString& ModuleName)
	{
		const FString RecoverySource =
			BuildReferenceIdentityRecoverySource();
		Engine.ResetMessages();
		asIScriptModule* RecoveryModule = nullptr;
		ASSERT_THAT(IsTrue(
			CompileAndReport(
				*TestRunner,
				ScriptEngine,
				Case.GetId() + TEXT("-RECOVERY"),
				ModuleName,
				RecoverySource,
				RecoveryModule) >= 0,
			*Case.Describe(TEXT("reference identity failed-build recovery should compile"))));
		ASSERT_THAT(IsNotNull(RecoveryModule,
			*Case.Describe(TEXT("reference identity failed-build recovery should publish a module"))));
		ASSERT_THAT(IsFalse(
			AngelscriptNativeReferenceTestSupport::HasAnyError(
				Engine),
			*Case.Describe(TEXT("reference identity recovery should emit no errors"))));
		if (RecoveryModule != nullptr)
		{
			asIScriptFunction* const Recovery =
				RecoveryModule->GetFunctionByDecl(
					"int RecoverReferenceIdentity()");
			asIScriptContext* const Context =
				ScriptEngine.CreateContext();
			ASSERT_THAT(IsNotNull(Recovery,
				*Case.Describe(TEXT("reference identity recovery should publish its exact entry"))));
			ASSERT_THAT(IsNotNull(Context,
				*Case.Describe(TEXT("reference identity recovery should create a context"))));
			if (Recovery != nullptr
				&& Context != nullptr)
			{
				ExecuteRecovery(
					Case,
					*Context,
					*Recovery);
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

	void RunCell(
		const FSourceCase& SourceCase,
		const FOperationCase& Operation,
		const FQualifierCase& Qualifier)
	{
		using namespace AngelscriptNativeReferenceTestSupport;
		using namespace AngelscriptNativeTestSupport;

		const FNativeCaseContext Case(MakeNativeCaseId(
			"LANG-REF-SOURCE-OP",
			{
				ANSI_TO_TCHAR(Operation.CatalogName),
				ANSI_TO_TCHAR(Qualifier.CatalogName),
				ANSI_TO_TCHAR(SourceCase.CatalogName),
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
			*Case.Describe(TEXT("reference identity cell should create a raw engine"))));
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
			*Case.Describe(TEXT("reference identity cell should register core reference fixtures"))));

		const FString ModuleName = FString::Printf(
			TEXT("ReferenceIdentity_%s"),
			*Case.GetId());
		const FString Source =
			BuildReferenceIdentitySource(
				SourceCase,
				Operation,
				Qualifier);
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
		if (IsCompileFailure(
			Operation,
			Qualifier))
		{
			ASSERT_THAT(IsTrue(
				BuildResult < 0,
				*Case.DescribeResult(
					TEXT("invalid reference qualifier source"),
					TEXT("negative build result"),
					DescribeReferenceBuild(
						Engine,
						BuildResult))));
			if (Module != nullptr)
			{
				ASSERT_THAT(IsNull(
					Module->GetFunctionByDecl(
						"int RunReferenceIdentity()"),
					*Case.Describe(TEXT("invalid reference qualifier module shell should publish no callable entry"))));
			}
			ASSERT_THAT(IsTrue(
				HasDiagnosticContaining(
					Engine,
					TEXT("const"))
					|| HasDiagnosticContaining(
						Engine,
						TEXT("reference"))
					|| HasDiagnosticContaining(
						Engine,
						TEXT("method")),
				*Case.DescribeResult(
					TEXT("invalid reference qualifier diagnostic"),
					TEXT("located const/reference diagnostic"),
					DescribeReferenceBuild(
						Engine,
						BuildResult))));
			DiscardReferenceModule(
				*ScriptEngine,
				ModuleName);
			CompileAndRunRecoveryModule(
				Case,
				Engine,
				*ScriptEngine,
				ModuleName);
			VerifyLifecycleCleanup(
				Case,
				State);
			return;
		}

		ASSERT_THAT(IsTrue(
			BuildResult >= 0,
			*Case.DescribeResult(
				TEXT("legal reference identity source"),
				TEXT("successful build"),
				DescribeReferenceBuild(
					Engine,
					BuildResult))));
		ASSERT_THAT(IsNotNull(Module,
			*Case.Describe(TEXT("legal reference identity source should publish a module"))));
		ASSERT_THAT(IsFalse(
			HasAnyError(Engine),
			*Case.Describe(TEXT("legal reference identity source should emit no errors"))));
		if (Module == nullptr)
		{
			VerifyLifecycleCleanup(
				Case,
				State);
			return;
		}
		VerifyExerciseMetadata(
			Case,
			SourceCase,
			Qualifier,
			*Module);
		asIScriptFunction* const Entry =
			Module->GetFunctionByDecl(
				"int RunReferenceIdentity()");
		asIScriptFunction* const Recovery =
			Module->GetFunctionByDecl(
				"int RecoverReferenceIdentity()");
		ASSERT_THAT(IsNotNull(Entry,
			*Case.Describe(TEXT("reference identity source should publish its exact entry"))));
		ASSERT_THAT(IsNotNull(Recovery,
			*Case.Describe(TEXT("reference identity source should publish same-context recovery"))));
		asIScriptContext* const Context =
			ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("reference identity source should create a context"))));
		if (Entry != nullptr
			&& Recovery != nullptr
			&& Context != nullptr)
		{
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Context->Prepare(Entry),
				*Case.Describe(TEXT("reference identity entry should prepare"))));
			const int ExecuteResult =
				Context->Execute();
			if (IsRuntimeNullFailure(
				SourceCase,
				Operation,
				Qualifier))
			{
				ASSERT_THAT(AreEqual(
					static_cast<int32>(asEXECUTION_EXCEPTION),
					ExecuteResult,
					*Case.Describe(TEXT("null reference member operation should raise an execution exception"))));
				ASSERT_THAT(IsTrue(
					FString(UTF8_TO_TCHAR(
						Context->GetExceptionString() != nullptr
							? Context->GetExceptionString()
							: "")).Contains(
								TEXT("Null pointer"),
								ESearchCase::IgnoreCase),
					*Case.Describe(TEXT("null reference member operation should own the null-pointer exception"))));
				ASSERT_THAT(IsTrue(
					Context->GetExceptionLineNumber() > 0,
					*Case.Describe(TEXT("null reference exception should retain a source line"))));
			}
			else
			{
				ASSERT_THAT(AreEqual(
					static_cast<int32>(asEXECUTION_FINISHED),
					ExecuteResult,
					*Case.Describe(TEXT("legal reference identity operation should finish"))));
				ASSERT_THAT(AreEqual(
					1,
					static_cast<int32>(
						Context->GetReturnDWord()),
					*Case.Describe(TEXT("legal reference identity operation should prove its semantic invariant"))));
			}
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Context->Unprepare(),
				*Case.Describe(TEXT("reference identity entry should unprepare before recovery"))));
			ExecuteRecovery(
				Case,
				*Context,
				*Recovery);
		}
		if (Context != nullptr)
		{
			Context->Release();
		}
		DiscardReferenceModule(
			*ScriptEngine,
			ModuleName);
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(
				TCHAR_TO_UTF8(*ModuleName),
				asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("reference identity module should discard cleanly"))));
		VerifyLifecycleCleanup(
			Case,
			State);
	}

public:
	TEST_METHOD(SourcesByOperationAndQualifier)
	{
		AS_NATIVE_PRODUCT("LANG-REF-SOURCE-OP",
			AngelscriptNativeTestSupport::ENativeEvidence::Compile
				| AngelscriptNativeTestSupport::ENativeEvidence::Diagnostic
				| AngelscriptNativeTestSupport::ENativeEvidence::Runtime
				| AngelscriptNativeTestSupport::ENativeEvidence::Metadata
				| AngelscriptNativeTestSupport::ENativeEvidence::Lifecycle);

		for (const FOperationCase& Operation
			: OperationCases)
		{
			for (const FQualifierCase& Qualifier
				: QualifierCases)
			{
				for (const FSourceCase& SourceCase
					: SourceCases)
				{
					RunCell(
						SourceCase,
						Operation,
						Qualifier);
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
