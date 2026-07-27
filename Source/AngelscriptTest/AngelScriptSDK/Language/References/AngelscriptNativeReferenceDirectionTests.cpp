#include "AngelscriptNativeReferenceTestSupport.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FReferenceDirectionTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.References.Direction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext =
		AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeDirectionCase =
		AngelscriptNativeTestSupport::FNativeDirectionCase;
	using FNativeTestEngine =
		AngelscriptNativeTestSupport::FNativeTestEngine;
	using FReferenceRoot =
		AngelscriptNativeReferenceTestSupport::FReferenceRoot;
	using FReferenceState =
		AngelscriptNativeReferenceTestSupport::FReferenceState;

	inline static constexpr asPWORD DirectionSnapshotUserDataSlot =
		static_cast<asPWORD>(0x524546444952534Eull);

	struct FDirectionSnapshots
	{
		int32 BeforeSource = 0;
		int32 BeforeTarget = 0;
		int32 InsideStart = 0;
		int32 InsideEnd = 0;
		int32 AfterSource = 0;
		int32 AfterTarget = 0;
		int32 AfterSourceValue = -1;
		int32 AfterTargetValue = -1;

		void Record(
			const int32 Stage,
			const FReferenceRoot* const Object)
		{
			const int32 Identity =
				Object != nullptr
					? Object->GetIdentity()
					: 0;
			const int32 Value =
				Object != nullptr
					? Object->GetValue()
					: -1;
			switch (Stage)
			{
			case 0:
				BeforeSource = Identity;
				break;
			case 1:
				BeforeTarget = Identity;
				break;
			case 2:
				InsideStart = Identity;
				break;
			case 3:
				InsideEnd = Identity;
				break;
			case 4:
				AfterSource = Identity;
				break;
			case 5:
				AfterTarget = Identity;
				break;
			case 6:
				AfterSourceValue = Value;
				break;
			case 7:
				AfterTargetValue = Value;
				break;
			default:
				break;
			}
		}
	};

	static FString DescribeSnapshots(
		const FDirectionSnapshots& Snapshots)
	{
		return FString::Printf(
			TEXT("Before={Source=%d Target=%d} Inside={Start=%d End=%d} After={Source=%d Target=%d SourceValue=%d TargetValue=%d}"),
			Snapshots.BeforeSource,
			Snapshots.BeforeTarget,
			Snapshots.InsideStart,
			Snapshots.InsideEnd,
			Snapshots.AfterSource,
			Snapshots.AfterTarget,
			Snapshots.AfterSourceValue,
			Snapshots.AfterTargetValue);
	}

	enum class EAliasRelation : uint8
	{
		SameTwoNames,
		Distinct,
		SelfAssignment,
		BaseDerivedViews,
		OutReplacement,
		InOutMutation,
	};

	enum class ENullState : uint8
	{
		NonNull,
		NullInput,
		NullOutput,
		NullReturn,
	};

	struct FRelationCase
	{
		const ANSICHAR* CatalogName;
		EAliasRelation Relation;
		int32 InitialValue;
	};

	struct FNullCase
	{
		const ANSICHAR* CatalogName;
		ENullState State;
	};

	inline static constexpr FRelationCase RelationCases[] =
	{
		{
			"same_two_names",
			EAliasRelation::SameTwoNames,
			21,
		},
		{ "distinct", EAliasRelation::Distinct, 22 },
		{
			"self_assignment",
			EAliasRelation::SelfAssignment,
			23,
		},
		{
			"base_derived_views",
			EAliasRelation::BaseDerivedViews,
			24,
		},
		{
			"out_replacement",
			EAliasRelation::OutReplacement,
			25,
		},
		{
			"inout_mutation",
			EAliasRelation::InOutMutation,
			26,
		},
	};

	inline static constexpr FNullCase NullCases[] =
	{
		{ "non_null", ENullState::NonNull },
		{ "null_input", ENullState::NullInput },
		{ "null_output", ENullState::NullOutput },
		{ "null_return", ENullState::NullReturn },
	};

	static bool WritesCallerReference(
		const FNativeDirectionCase& Direction)
	{
		return Direction.TypeModifier == asTM_OUTREF
			|| Direction.TypeModifier
				== asTM_INOUTREF;
	}

	static bool StartsNullInside(
		const FNativeDirectionCase& Direction,
		const FNullCase& NullState)
	{
		return NullState.State
			== ENullState::NullInput;
	}

	static bool EndsNullInside(
		const FNullCase& NullState)
	{
		return NullState.State
				== ENullState::NullOutput
			|| NullState.State
				== ENullState::NullReturn;
	}

	static bool ReplacesInside(
		const FNativeDirectionCase& Direction,
		const FRelationCase& Relation,
		const FNullCase& NullState,
		const bool bStartsNull)
	{
		if (EndsNullInside(NullState))
		{
			return false;
		}
		if (Relation.Relation
			== EAliasRelation::InOutMutation)
		{
			return bStartsNull;
		}
		return Direction.TypeModifier == asTM_OUTREF
			|| Relation.Relation
				== EAliasRelation::OutReplacement;
	}

	static FString DirectionParameter(
		const FNativeDirectionCase& Direction)
	{
		const FString Suffix =
			UTF8_TO_TCHAR(Direction.DeclarationSuffix);
		return Suffix.IsEmpty()
			? TEXT("FRefRoot Target")
			: TEXT("FRefRoot ")
				+ Suffix
				+ TEXT(" Target");
	}

	static void AppendDirectionHelpers(
		FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			TEXT("int ReferenceIdentity(const FRefRoot Object)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\treturn Object == nullptr"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\t? 0"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\t: Object.GetIdentity();"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("int ReferenceValue(const FRefRoot Object)"));
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
			TEXT("FRefRoot ReturnNullReference()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\treturn nullptr;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static FString WorkingName(
		const FNativeDirectionCase& Direction)
	{
		return Direction.TypeModifier == asTM_INREF
			? TEXT("Working")
			: TEXT("Target");
	}

	static void AppendDirectionAction(
		FString& Source,
		const FNativeDirectionCase& Direction,
		const FRelationCase& Relation,
		const FNullCase& NullState)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString Working =
			WorkingName(Direction);
		if (Direction.TypeModifier == asTM_INREF)
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("\tFRefRoot Working = Target;"));
		}
		if (NullState.State
			== ENullState::NullOutput)
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("\t") + Working
					+ TEXT(" = nullptr;"));
			return;
		}
		if (NullState.State
			== ENullState::NullReturn)
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("\t") + Working
					+ TEXT(" = ReturnNullReference();"));
			return;
		}
		if (Direction.TypeModifier == asTM_OUTREF
			&& Relation.Relation
				!= EAliasRelation::OutReplacement
			&& Relation.Relation
				!= EAliasRelation::InOutMutation)
		{
			AppendGeneratedAsLine(
				Source,
				Relation.Relation
						== EAliasRelation::BaseDerivedViews
					? FString::Printf(
						TEXT("\tTarget = MakeRefDerivedAsRoot(%d);"),
						Relation.InitialValue)
					: FString::Printf(
						TEXT("\tTarget = MakeRefRoot(%d);"),
						Relation.InitialValue));
		}
		switch (Relation.Relation)
		{
		case EAliasRelation::SameTwoNames:
			AppendGeneratedAsLine(
				Source,
				TEXT("\tif (") + Working
					+ TEXT(" != nullptr)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\t\t") + Working
					+ TEXT(".SetValue(61);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			break;
		case EAliasRelation::Distinct:
			AppendGeneratedAsLine(
				Source,
				TEXT("\tif (") + Working
					+ TEXT(" != nullptr)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\t\t") + Working
					+ TEXT(".SetValue(62);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			break;
		case EAliasRelation::SelfAssignment:
			AppendGeneratedAsLine(
				Source,
				TEXT("\t") + Working
					+ TEXT(" = ") + Working
					+ TEXT(";"));
			break;
		case EAliasRelation::BaseDerivedViews:
			AppendGeneratedAsLine(
				Source,
				TEXT("\tif (") + Working
					+ TEXT(" != nullptr)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\t\t") + Working
					+ TEXT(".SetValue(63);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			break;
		case EAliasRelation::OutReplacement:
			AppendGeneratedAsLine(
				Source,
				TEXT("\t") + Working
					+ TEXT(" = MakeRefRoot(71);"));
			break;
		case EAliasRelation::InOutMutation:
			AppendGeneratedAsLine(
				Source,
				TEXT("\tif (") + Working
					+ TEXT(" == nullptr)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\t\t") + Working
					+ TEXT(" = MakeRefRoot(72);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\telse"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\t\t") + Working
					+ TEXT(".SetValue(72);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			break;
		default:
			break;
		}
	}

	static void AppendDirectionFunction(
		FString& Source,
		const FNativeDirectionCase& Direction,
		const FRelationCase& Relation,
		const FNullCase& NullState)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			TEXT("void ApplyDirection(")
				+ DirectionParameter(Direction)
				+ TEXT(")"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tCaptureDirectionSnapshot(2, Target);"));
		AppendDirectionAction(
			Source,
			Direction,
			Relation,
			NullState);
		AppendGeneratedAsLine(
			Source,
			TEXT("\tCaptureDirectionSnapshot(3, ")
				+ WorkingName(Direction)
				+ TEXT(");"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendInitialReferences(
		FString& Source,
		const FRelationCase& Relation,
		const FNullCase& NullState)
	{
		using namespace AngelscriptNativeTestSupport;

		if (Relation.Relation
			== EAliasRelation::BaseDerivedViews)
		{
			AppendGeneratedAsLine(
				Source,
				FString::Printf(
					TEXT("\tFRefDerived Derived = MakeRefDerived(%d);"),
					Relation.InitialValue));
			AppendGeneratedAsLine(
				Source,
				TEXT("\tFRefRoot Source = Derived.opImplCast();"));
		}
		else
		{
			AppendGeneratedAsLine(
				Source,
				FString::Printf(
					TEXT("\tFRefRoot Source = MakeRefRoot(%d);"),
					Relation.InitialValue));
		}
		if (NullState.State
			== ENullState::NullInput)
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("\tFRefRoot Target = nullptr;"));
		}
		else if (Relation.Relation
			== EAliasRelation::Distinct)
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("\tFRefRoot Target = MakeRefRoot(32);"));
		}
		else
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("\tFRefRoot Target = Source;"));
		}
	}

	static void AppendDirectionEntry(
		FString& Source,
		const FRelationCase& Relation,
		const FNullCase& NullState)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			TEXT("int RunReferenceDirection()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendInitialReferences(
			Source,
			Relation,
			NullState);
		AppendGeneratedAsLine(
			Source,
			TEXT("\tCaptureDirectionSnapshot(0, Source);"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tCaptureDirectionSnapshot(1, Target);"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tApplyDirection(Target);"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tCaptureDirectionSnapshot(4, Source);"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tCaptureDirectionSnapshot(5, Target);"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tCaptureDirectionSnapshot(6, Source);"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tCaptureDirectionSnapshot(7, Target);"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 1;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("int RecoverReferenceDirection()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 917;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static FString BuildReferenceDirectionSource(
		const FNativeDirectionCase& Direction,
		const FRelationCase& Relation,
		const FNullCase& NullState)
	{
		FString Source;
		AppendDirectionHelpers(Source);
		AppendDirectionFunction(
			Source,
			Direction,
			Relation,
			NullState);
		AppendDirectionEntry(
			Source,
			Relation,
			NullState);
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

	static FDirectionSnapshots* GetDirectionSnapshots(
		asIScriptGeneric& Generic)
	{
		return Generic.GetEngine() != nullptr
			? static_cast<FDirectionSnapshots*>(
				Generic.GetEngine()->GetUserData(
					DirectionSnapshotUserDataSlot))
			: nullptr;
	}

	static void GenericCaptureDirectionSnapshot(
		asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FDirectionSnapshots* const Snapshots =
			GetDirectionSnapshots(*Generic);
		if (Snapshots != nullptr)
		{
			FReferenceRoot* Object = nullptr;
			if (FReferenceRoot** const HandleAddress =
				static_cast<FReferenceRoot**>(
					Generic->GetArgAddress(1)))
			{
				Object = *HandleAddress;
			}
			Snapshots->Record(
				static_cast<int32>(Generic->GetArgDWord(0)),
				Object);
		}
	}

	void VerifyDirectionMetadata(
		const FNativeCaseContext& Case,
		const FNativeDirectionCase& Direction,
		asIScriptModule& Module)
	{
		const FString Declaration =
			TEXT("void ApplyDirection(")
			+ DirectionParameter(Direction)
			+ TEXT(")");
		const FTCHARToUTF8 DeclarationUtf8(
			*Declaration);
		asIScriptFunction* const Function =
			Module.GetFunctionByDecl(
				DeclarationUtf8.Get());
		ASSERT_THAT(IsNotNull(Function,
			*Case.Describe(TEXT("reference direction should publish its exact parameter declaration"))));
		if (Function == nullptr)
		{
			return;
		}
		int TypeId = asTYPEID_VOID;
		asDWORD Modifier = asTM_NONE;
		const char* Name = nullptr;
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Function->GetParam(
				0,
				&TypeId,
				&Modifier,
				&Name),
			*Case.Describe(TEXT("reference direction parameter metadata should be readable"))));
		ASSERT_THAT(AreEqual(
			Direction.TypeModifier,
			Modifier,
			*Case.Describe(TEXT("reference direction metadata should retain the exact value/in/out/inout modifier"))));
		ASSERT_THAT(AreEqual(
			FString(TEXT("Target")),
			FString(UTF8_TO_TCHAR(
				Name != nullptr
					? Name
					: "")),
			*Case.Describe(TEXT("reference direction should retain the target parameter name"))));
	}

	void VerifySnapshots(
		const FNativeCaseContext& Case,
		const FNativeDirectionCase& Direction,
		const FRelationCase& Relation,
		const FNullCase& NullState,
		const FDirectionSnapshots& Snapshots)
	{
		const int32 BeforeSource = Snapshots.BeforeSource;
		const int32 BeforeTarget = Snapshots.BeforeTarget;
		const int32 InsideStart = Snapshots.InsideStart;
		const int32 InsideEnd = Snapshots.InsideEnd;
		const int32 AfterSource = Snapshots.AfterSource;
		const int32 AfterTarget = Snapshots.AfterTarget;
		const int32 AfterSourceValue = Snapshots.AfterSourceValue;
		const int32 AfterTargetValue = Snapshots.AfterTargetValue;
		const FString SnapshotDescription =
			DescribeSnapshots(Snapshots);
		ASSERT_THAT(IsTrue(
			BeforeSource > 0,
			*Case.Describe(TEXT("reference direction should begin with one owned source identity"))));
		ASSERT_THAT(AreEqual(
			BeforeSource,
			AfterSource,
			*Case.Describe(TEXT("reference direction should preserve the independently owned source identity"))));
		const bool bStartsNull =
			StartsNullInside(
				Direction,
				NullState);
		ASSERT_THAT(AreEqual(
			bStartsNull ? 0 : BeforeTarget,
			InsideStart,
			*Case.DescribeResult(
				TEXT("reference direction entry state"),
				TEXT("the caller target, except an explicit null input"),
				SnapshotDescription)));
		if (EndsNullInside(NullState))
		{
			ASSERT_THAT(AreEqual(
				0,
				InsideEnd,
				*Case.Describe(TEXT("null-output/null-return route should end with a null working reference"))));
		}
		else if (ReplacesInside(
			Direction,
			Relation,
			NullState,
			bStartsNull))
		{
			ASSERT_THAT(IsTrue(
				InsideEnd > 0
					&& InsideEnd != InsideStart,
				*Case.Describe(TEXT("replacement route should publish a distinct non-null working identity"))));
		}
		else
		{
			ASSERT_THAT(AreEqual(
				InsideStart,
				InsideEnd,
				*Case.Describe(TEXT("non-rebinding route should preserve the working identity"))));
		}
		ASSERT_THAT(AreEqual(
			WritesCallerReference(Direction)
				? InsideEnd
				: BeforeTarget,
			AfterTarget,
			*Case.Describe(TEXT("out/inout should write caller identity while value/in should preserve it"))));

		const bool bMutatingRelation =
			Relation.Relation
					== EAliasRelation::SameTwoNames
				|| Relation.Relation
					== EAliasRelation::Distinct
				|| Relation.Relation
					== EAliasRelation::BaseDerivedViews
				|| Relation.Relation
					== EAliasRelation::InOutMutation;
		const bool bCanMutateExisting =
			bMutatingRelation
			&& !bStartsNull
			&& !EndsNullInside(NullState)
			&& (Direction.TypeModifier != asTM_OUTREF
				|| Relation.Relation
					== EAliasRelation::InOutMutation);
		const bool bTargetAliasesSource =
			BeforeTarget == BeforeSource
			&& BeforeTarget > 0;
		if (bCanMutateExisting
			&& bTargetAliasesSource)
		{
			const int32 ExpectedValue =
				Relation.Relation
						== EAliasRelation::SameTwoNames
					? 61
					: Relation.Relation
							== EAliasRelation::Distinct
						? 62
						: Relation.Relation
								== EAliasRelation::BaseDerivedViews
							? 63
							: 72;
			ASSERT_THAT(AreEqual(
				ExpectedValue,
				AfterSourceValue,
				*Case.DescribeResult(
					TEXT("mutation through an alias should be visible from the independently retained source"),
					FString::Printf(
						TEXT("source value %d"),
						ExpectedValue),
					SnapshotDescription)));
		}
		if (AfterTarget == 0)
		{
			ASSERT_THAT(AreEqual(
				-1,
				AfterTargetValue,
				*Case.Describe(TEXT("null caller target should expose the null value sentinel"))));
		}
	}

	void ExecuteAndRecover(
		const FNativeCaseContext& Case,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module,
		const FNativeDirectionCase& Direction,
		const FRelationCase& Relation,
		const FNullCase& NullState,
		const FDirectionSnapshots& Snapshots)
	{
		asIScriptFunction* const Entry =
			Module.GetFunctionByDecl(
				"int RunReferenceDirection()");
		asIScriptFunction* const Recovery =
			Module.GetFunctionByDecl(
				"int RecoverReferenceDirection()");
		asIScriptContext* const Context =
			ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Entry,
			*Case.Describe(TEXT("reference direction should publish its exact entry"))));
		ASSERT_THAT(IsNotNull(Recovery,
			*Case.Describe(TEXT("reference direction should publish same-context recovery"))));
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("reference direction should create a context"))));
		if (Entry != nullptr
			&& Recovery != nullptr
			&& Context != nullptr)
		{
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Context->Prepare(Entry),
				*Case.Describe(TEXT("reference direction entry should prepare"))));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asEXECUTION_FINISHED),
				Context->Execute(),
				*Case.Describe(TEXT("reference direction entry should finish"))));
			ASSERT_THAT(AreEqual(
				1,
				static_cast<int32>(
					Context->GetReturnDWord()),
				*Case.Describe(TEXT("reference direction entry should return its completion marker"))));
			VerifySnapshots(
				Case,
				Direction,
				Relation,
				NullState,
				Snapshots);
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Context->Unprepare(),
				*Case.Describe(TEXT("reference direction entry should unprepare"))));
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Context->Prepare(Recovery),
				*Case.Describe(TEXT("reference direction recovery should prepare on the same context"))));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asEXECUTION_FINISHED),
				Context->Execute(),
				*Case.Describe(TEXT("reference direction recovery should finish"))));
			ASSERT_THAT(AreEqual(
				917,
				static_cast<int32>(
					Context->GetReturnDWord()),
				*Case.Describe(TEXT("reference direction recovery should return its sentinel"))));
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Context->Unprepare(),
				*Case.Describe(TEXT("reference direction recovery should unprepare"))));
		}
		if (Context != nullptr)
		{
			Context->Release();
		}
	}

	void RunCell(
		const FNativeDirectionCase& Direction,
		const FRelationCase& Relation,
		const FNullCase& NullState)
	{
		using namespace AngelscriptNativeReferenceTestSupport;
		using namespace AngelscriptNativeTestSupport;

		const FNativeCaseContext Case(MakeNativeCaseId(
			"LANG-REF-DIRECTION",
			{
				ANSI_TO_TCHAR(Direction.CatalogName),
				ANSI_TO_TCHAR(NullState.CatalogName),
				ANSI_TO_TCHAR(Relation.CatalogName),
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
			*Case.Describe(TEXT("reference direction cell should create a raw engine"))));
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
			*Case.Describe(TEXT("reference direction cell should register core reference fixtures"))));
		FDirectionSnapshots Snapshots;
		ScriptEngine->SetUserData(
			&Snapshots,
			DirectionSnapshotUserDataSlot);
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterGlobalFunction(
				"void CaptureDirectionSnapshot(int Stage, const FRefRoot&in Object)",
				asFUNCTION(GenericCaptureDirectionSnapshot),
				asCALL_GENERIC) >= 0,
			*Case.Describe(TEXT("reference direction cell should register its native snapshot callback"))));
		const FString ModuleName = FString::Printf(
			TEXT("ReferenceDirection_%s"),
			*Case.GetId());
		const FString Source =
			BuildReferenceDirectionSource(
				Direction,
				Relation,
				NullState);
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
		ASSERT_THAT(IsTrue(
			BuildResult >= 0,
			*Case.DescribeResult(
				TEXT("reference direction source"),
				TEXT("successful build"),
				DescribeReferenceBuild(
					Engine,
					BuildResult))));
		ASSERT_THAT(IsNotNull(Module,
			*Case.Describe(TEXT("reference direction source should publish a module"))));
		ASSERT_THAT(IsFalse(
			HasAnyError(Engine),
			*Case.Describe(TEXT("reference direction source should emit no errors"))));
		if (Module != nullptr)
		{
			VerifyDirectionMetadata(
				Case,
				Direction,
				*Module);
			ExecuteAndRecover(
				Case,
				*ScriptEngine,
				*Module,
				Direction,
				Relation,
				NullState,
				Snapshots);
		}
		DiscardReferenceModule(
			*ScriptEngine,
			ModuleName);
		State.ReleaseRetainedNativeObject();
		ASSERT_THAT(AreEqual(
			0,
			State.LiveObjects,
			*Case.DescribeResult(
				TEXT("reference direction cleanup"),
				TEXT("Live=0 after module discard"),
				DescribeReferenceState(State))));
		ASSERT_THAT(AreEqual(
			State.Created,
			State.Destroyed,
			*Case.Describe(TEXT("reference direction cell should destroy every created identity"))));
		TArray<int32> Created =
			State.CreatedIdentities;
		TArray<int32> Destroyed =
			State.DestroyedIdentities;
		Created.Sort();
		Destroyed.Sort();
		ASSERT_THAT(AreEqual(
			Created,
			Destroyed,
			*Case.Describe(TEXT("reference direction cell should destroy the exact identities it created"))));
	}

public:
	TEST_METHOD(DirectionsByAliasAndNullState)
	{
		AS_NATIVE_PRODUCT("LANG-REF-DIRECTION",
			AngelscriptNativeTestSupport::ENativeEvidence::Compile
				| AngelscriptNativeTestSupport::ENativeEvidence::Runtime
				| AngelscriptNativeTestSupport::ENativeEvidence::Metadata
				| AngelscriptNativeTestSupport::ENativeEvidence::Lifecycle
				| AngelscriptNativeTestSupport::ENativeEvidence::Cleanup);

		for (const FNativeDirectionCase& Direction
			: AngelscriptNativeTestSupport::NativeDirectionCases)
		{
			for (const FNullCase& NullState
				: NullCases)
			{
				for (const FRelationCase& Relation
					: RelationCases)
				{
					RunCell(
						Direction,
						Relation,
						NullState);
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
