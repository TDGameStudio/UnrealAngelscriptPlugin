#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FPropertyInitializationTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Properties.Initialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using ENativeLifecycleEvent = AngelscriptNativeTestSupport::ENativeLifecycleEvent;
	using ENativeValueCategory = AngelscriptNativeTestSupport::ENativeValueCategory;
	using FNativeCaseContext = AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeLifecycleRecorder = AngelscriptNativeTestSupport::FNativeLifecycleRecorder;
	using FNativeTypeCase = AngelscriptNativeTestSupport::FNativeTypeCase;

	enum class EInitializationEventKind : uint8
	{
		Marker,
		Checkpoint,
	};

	struct FSourceCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FPositionCase
	{
		const ANSICHAR* CatalogName;
		bool bTargetInBase;
		int32 OwnerPropertyIndex;
	};

	struct FObservationCase
	{
		const ANSICHAR* CatalogName;
		int32 Stage;
	};

	struct FInitializationCheckpoint
	{
		int32 Stage = INDEX_NONE;
		bool bAvailable = false;
		int32 Value = 0;
	};

	struct FInitializationEvent
	{
		EInitializationEventKind Kind = EInitializationEventKind::Marker;
		int32 Code = INDEX_NONE;
	};

	struct FPropertyInitializationState
	{
		TArray<int32> Markers;
		TArray<FInitializationCheckpoint> Checkpoints;
		TArray<FInitializationEvent> Events;

		void Reset()
		{
			Markers.Reset();
			Checkpoints.Reset();
			Events.Reset();
		}
	};

	inline static constexpr FSourceCase SourceCases[] =
	{
		{ "default_value" },
		{ "declaration_initializer" },
		{ "owner_literal_assignment" },
		{ "owner_source_assignment" },
		{ "derived_reassignment" },
	};

	inline static constexpr FPositionCase PositionCases[] =
	{
		{ "base_first", true, 0 },
		{ "base_middle", true, 2 },
		{ "base_last", true, 4 },
		{ "derived_first", false, 0 },
		{ "derived_last", false, 4 },
	};

	inline static constexpr FObservationCase ObservationCases[] =
	{
		{ "base_entry", 0 },
		{ "base_exit", 1 },
		{ "derived_entry", 2 },
		{ "derived_exit", 3 },
	};

	inline static constexpr asPWORD PropertyInitializationStateUserDataSlot =
		static_cast<asPWORD>(0x50524F50494E4954ull);

	static bool IsSource(const FSourceCase& SourceCase, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(SourceCase.CatalogName, Name) == 0;
	}

	static bool IsObjectValueType(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category == ENativeValueCategory::ScriptValue
			|| TypeCase.Category == ENativeValueCategory::NativeValue;
	}

	static bool HasDeclarationInitializer(const FSourceCase& SourceCase)
	{
		return IsSource(SourceCase, "declaration_initializer")
			|| IsSource(SourceCase, "derived_reassignment");
	}

	static bool UsesCurrentForkScriptValueDeclarationAssign(
		const FSourceCase& SourceCase,
		const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category == ENativeValueCategory::ScriptValue
			&& IsSource(SourceCase, "declaration_initializer");
	}

	static FString MakeSuffix(
		const FSourceCase& SourceCase,
		const FPositionCase& PositionCase,
		const FNativeTypeCase& TypeCase)
	{
		return FString::Printf(
			TEXT("%hs_%hs_%hs"),
			SourceCase.CatalogName,
			PositionCase.CatalogName,
			TypeCase.CatalogName);
	}

	static FString MakeTypedValue(const FNativeTypeCase& TypeCase, const int32 Value)
	{
		if (TypeCase.Category == ENativeValueCategory::Boolean)
		{
			return Value == 0 ? TEXT("false") : TEXT("true");
		}
		if (TypeCase.Category == ENativeValueCategory::Enum)
		{
			return Value == 0 ? TEXT("ENativeCaseEnum::Zero") : TEXT("ENativeCaseEnum::One");
		}
		if (TypeCase.Category == ENativeValueCategory::Typedef)
		{
			return FString::Printf(TEXT("NativeCaseAlias(%d)"), Value);
		}
		return FString::Printf(TEXT("%hs(%d)"), TypeCase.ScriptType, Value);
	}

	static FPropertyInitializationState* GetActivePropertyInitializationState()
	{
		asIScriptContext* const Context = asGetActiveContext();
		return Context != nullptr && Context->GetEngine() != nullptr
			? static_cast<FPropertyInitializationState*>(
				Context->GetEngine()->GetUserData(PropertyInitializationStateUserDataSlot))
			: nullptr;
	}

	static int32 RecordPropertyInitializationMarker(const int32 Marker)
	{
		if (FPropertyInitializationState* const State = GetActivePropertyInitializationState())
		{
			State->Markers.Add(Marker);
			State->Events.Add({ EInitializationEventKind::Marker, Marker });
		}
		return Marker;
	}

	static void RecordPropertyInitializationCheckpoint(
		const int32 Stage,
		const bool bAvailable,
		const int32 Value)
	{
		if (FPropertyInitializationState* const State = GetActivePropertyInitializationState())
		{
			State->Checkpoints.Add({ Stage, bAvailable, Value });
			State->Events.Add({ EInitializationEventKind::Checkpoint, 1000 + Stage });
		}
	}

	static bool RegisterPropertyInitializationBridge(
		asIScriptEngine& ScriptEngine,
		FPropertyInitializationState& State)
	{
		ScriptEngine.SetUserData(&State, PropertyInitializationStateUserDataSlot);
		const ASAutoCaller::FunctionCaller MarkerCaller =
			ASAutoCaller::MakeFunctionCaller(RecordPropertyInitializationMarker);
		const ASAutoCaller::FunctionCaller CheckpointCaller =
			ASAutoCaller::MakeFunctionCaller(RecordPropertyInitializationCheckpoint);
		return ScriptEngine.RegisterGlobalFunction(
			"int RecordPropertyInitializationMarker(int Marker)",
			asFUNCTION(RecordPropertyInitializationMarker),
			asCALL_CDECL,
			*(asFunctionCaller*)&MarkerCaller) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"void RecordPropertyInitializationCheckpoint(int Stage, bool Available, int Value)",
				asFUNCTION(RecordPropertyInitializationCheckpoint),
				asCALL_CDECL,
				*(asFunctionCaller*)&CheckpointCaller) >= 0;
	}

	static void AppendTypeDeclarations(FString& Source, const FNativeTypeCase& TypeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		// NativeCaseAlias is registered through RegisterCoreLanguageTypedef because this fork rejects script typedef syntax.
		if (TypeCase.Category == ENativeValueCategory::Enum)
		{
			AppendGeneratedAsLine(Source, TEXT("enum ENativeCaseEnum"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tZero = 0,"));
			AppendGeneratedAsLine(Source, TEXT("\tOne = 1"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (TypeCase.Category == ENativeValueCategory::ScriptValue)
		{
			AppendGeneratedAsLine(Source, TEXT("struct FScriptCaseValue"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
			AppendGeneratedAsLine(Source, TEXT("\tint ObjectId = -1;"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFScriptCaseValue()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tObjectId = BeginNativeScriptLifecycle(Value);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFScriptCaseValue(int InValue)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tValue = InValue;"));
			AppendGeneratedAsLine(Source, TEXT("\t\tObjectId = BeginNativeScriptLifecycle(Value);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFScriptCaseValue(const FScriptCaseValue& Other)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tValue = Other.Value;"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\t\tObjectId = CopyNativeScriptLifecycle(Other.ObjectId, Value);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFScriptCaseValue& opAssign(const FScriptCaseValue& Other)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tValue = Other.Value;"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\t\tAssignNativeScriptLifecycle(ObjectId, Other.ObjectId, Value);"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn this;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\t~FScriptCaseValue()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tEndNativeScriptLifecycle(ObjectId, Value);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
	}

	static void AppendObservationFunction(FString& Source, const FNativeTypeCase& TypeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (IsObjectValueType(TypeCase))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("int ObservePropertyInitialization(const %hs& in Value)"),
				TypeCase.ScriptType));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value.Value;"));
		}
		else if (TypeCase.Category == ENativeValueCategory::Boolean)
		{
			AppendGeneratedAsLine(Source, TEXT("int ObservePropertyInitialization(bool Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value ? 1 : 0;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("int ObservePropertyInitialization(%hs Value)"),
				TypeCase.ScriptType));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn int(Value);"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendInitializerFunction(
		FString& Source,
		const FSourceCase& SourceCase,
		const FPositionCase& PositionCase,
		const FNativeTypeCase& TypeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (!HasDeclarationInitializer(SourceCase))
		{
			return;
		}
		const int32 Marker = PositionCase.bTargetInBase
			? 100 + PositionCase.OwnerPropertyIndex
			: 200 + PositionCase.OwnerPropertyIndex;
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("%hs InitializePropertyValue()"),
			TypeCase.ScriptType));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\tRecordPropertyInitializationMarker(%d);"),
			Marker));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\treturn %s;"),
			*MakeTypedValue(TypeCase, 1)));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendOwnerFields(
		FString& Source,
		const bool bBaseOwner,
		const FSourceCase& SourceCase,
		const FPositionCase& PositionCase,
		const FNativeTypeCase& TypeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		const bool bTargetOwner = PositionCase.bTargetInBase == bBaseOwner;
		for (int32 PropertyIndex = 0; PropertyIndex < 5; ++PropertyIndex)
		{
			if (bTargetOwner && PropertyIndex == PositionCase.OwnerPropertyIndex)
			{
				if (HasDeclarationInitializer(SourceCase))
				{
					AppendGeneratedAsLine(Source, FString::Printf(
						TEXT("\t%hs Observed = InitializePropertyValue();"),
						TypeCase.ScriptType));
				}
				else
				{
					AppendGeneratedAsLine(Source, FString::Printf(
						TEXT("\t%hs Observed;"),
						TypeCase.ScriptType));
				}
				continue;
			}

			const int32 Marker = (bBaseOwner ? 10 : 20) + PropertyIndex;
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\tint %sField%d = RecordPropertyInitializationMarker(%d);"),
				bBaseOwner ? TEXT("Base") : TEXT("Derived"),
				PropertyIndex,
				Marker));
		}
	}

	static int32 AssignmentMarker(
		const FSourceCase& SourceCase,
		const bool bTargetInBase)
	{
		if (IsSource(SourceCase, "owner_literal_assignment"))
		{
			return bTargetInBase ? 300 : 301;
		}
		if (IsSource(SourceCase, "owner_source_assignment"))
		{
			return bTargetInBase ? 310 : 311;
		}
		if (IsSource(SourceCase, "derived_reassignment"))
		{
			return 320;
		}
		return INDEX_NONE;
	}

	static void AppendAssignment(
		FString& Source,
		const TCHAR* Prefix,
		const FSourceCase& SourceCase,
		const FPositionCase& PositionCase,
		const FNativeTypeCase& TypeCase,
		const bool bInBaseConstructor)
	{
		using namespace AngelscriptNativeTestSupport;

		const bool bOwnerAssignment =
			(IsSource(SourceCase, "owner_literal_assignment")
				|| IsSource(SourceCase, "owner_source_assignment"))
			&& PositionCase.bTargetInBase == bInBaseConstructor;
		const bool bDerivedReassignment =
			IsSource(SourceCase, "derived_reassignment")
			&& !bInBaseConstructor;
		if (!bOwnerAssignment && !bDerivedReassignment)
		{
			return;
		}

		const int32 Marker = AssignmentMarker(SourceCase, PositionCase.bTargetInBase);
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("%sRecordPropertyInitializationMarker(%d);"),
			Prefix,
			Marker));
		if (IsSource(SourceCase, "owner_source_assignment"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%s%hs SourceValue = %s;"),
				Prefix,
				TypeCase.ScriptType,
				*MakeTypedValue(TypeCase, 1)));
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sObserved = SourceValue;"),
				Prefix));
			return;
		}

		const int32 AssignedValue = bDerivedReassignment ? 0 : 1;
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("%sObserved = %s;"),
			Prefix,
			*MakeTypedValue(TypeCase, AssignedValue)));
	}

	static void AppendCheckpoint(
		FString& Source,
		const int32 Stage,
		const bool bAvailable,
		const TCHAR* Prefix)
	{
		using namespace AngelscriptNativeTestSupport;

		if (bAvailable)
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sRecordPropertyInitializationCheckpoint(%d, true, ObservePropertyInitialization(Observed));"),
				Prefix,
				Stage));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sRecordPropertyInitializationCheckpoint(%d, false, -777);"),
				Prefix,
				Stage));
		}
	}

	static void AppendBaseType(
		FString& Source,
		const FSourceCase& SourceCase,
		const FPositionCase& PositionCase,
		const FNativeTypeCase& TypeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FPropertyInitializationBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendOwnerFields(Source, true, SourceCase, PositionCase, TypeCase);
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFPropertyInitializationBase()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendCheckpoint(Source, 0, PositionCase.bTargetInBase, TEXT("\t\t"));
		AppendAssignment(
			Source,
			TEXT("\t\t"),
			SourceCase,
			PositionCase,
			TypeCase,
			true);
		AppendCheckpoint(Source, 1, PositionCase.bTargetInBase, TEXT("\t\t"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendDerivedType(
		FString& Source,
		const FSourceCase& SourceCase,
		const FPositionCase& PositionCase,
		const FNativeTypeCase& TypeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			TEXT("class FPropertyInitializationDerived : FPropertyInitializationBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendOwnerFields(Source, false, SourceCase, PositionCase, TypeCase);
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFPropertyInitializationDerived()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tsuper();"));
		AppendCheckpoint(Source, 2, true, TEXT("\t\t"));
		AppendAssignment(
			Source,
			TEXT("\t\t"),
			SourceCase,
			PositionCase,
			TypeCase,
			false);
		AppendCheckpoint(Source, 3, true, TEXT("\t\t"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static FString BuildPropertyInitializationSource(
		const FSourceCase& SourceCase,
		const FPositionCase& PositionCase,
		const FNativeTypeCase& TypeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendTypeDeclarations(Source, TypeCase);
		AppendObservationFunction(Source, TypeCase);
		AppendInitializerFunction(Source, SourceCase, PositionCase, TypeCase);
		AppendBaseType(Source, SourceCase, PositionCase, TypeCase);
		AppendDerivedType(Source, SourceCase, PositionCase, TypeCase);
		AppendGeneratedAsLine(Source, TEXT("int RunPropertyInitialization()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFPropertyInitializationDerived Receiver = FPropertyInitializationDerived();"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\treturn ObservePropertyInitialization(Receiver.Observed);"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunPropertyInitializationRecovery()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 97;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static FString MakeObservationCaseId(
		const FObservationCase& ObservationCase,
		const FPositionCase& PositionCase,
		const FSourceCase& SourceCase,
		const FNativeTypeCase& TypeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		return MakeNativeCaseId(
			"LANG-PROP-INIT-ORDER",
			{
				ANSI_TO_TCHAR(ObservationCase.CatalogName),
				ANSI_TO_TCHAR(PositionCase.CatalogName),
				ANSI_TO_TCHAR(SourceCase.CatalogName),
				ANSI_TO_TCHAR(TypeCase.CatalogName),
			});
	}

	static bool ExpectedAvailability(
		const FPositionCase& PositionCase,
		const FObservationCase& ObservationCase)
	{
		return PositionCase.bTargetInBase || ObservationCase.Stage >= 2;
	}

	static int32 ExpectedCheckpointValue(
		const FSourceCase& SourceCase,
		const FPositionCase& PositionCase,
		const FObservationCase& ObservationCase)
	{
		if (!ExpectedAvailability(PositionCase, ObservationCase))
		{
			return -777;
		}
		if (IsSource(SourceCase, "default_value"))
		{
			return 0;
		}
		if (IsSource(SourceCase, "declaration_initializer"))
		{
			return 1;
		}
		if (IsSource(SourceCase, "owner_literal_assignment")
			|| IsSource(SourceCase, "owner_source_assignment"))
		{
			if (PositionCase.bTargetInBase)
			{
				return ObservationCase.Stage == 0 ? 0 : 1;
			}
			return ObservationCase.Stage == 2 ? 0 : 1;
		}
		return ObservationCase.Stage == 3 ? 0 : 1;
	}

	static int32 ExpectedFinalValue(
		const FSourceCase& SourceCase)
	{
		return IsSource(SourceCase, "default_value")
			|| IsSource(SourceCase, "derived_reassignment")
			? 0
			: 1;
	}

	static void AppendExpectedOwnerMarkers(
		TArray<int32>& ExpectedMarkers,
		const bool bBaseOwner,
		const FSourceCase& SourceCase,
		const FPositionCase& PositionCase)
	{
		const bool bTargetOwner = PositionCase.bTargetInBase == bBaseOwner;
		for (int32 PropertyIndex = 0; PropertyIndex < 5; ++PropertyIndex)
		{
			if (bTargetOwner && PropertyIndex == PositionCase.OwnerPropertyIndex)
			{
				if (HasDeclarationInitializer(SourceCase))
				{
					ExpectedMarkers.Add((bBaseOwner ? 100 : 200) + PropertyIndex);
				}
				continue;
			}
			ExpectedMarkers.Add((bBaseOwner ? 10 : 20) + PropertyIndex);
		}
	}

	static TArray<int32> MakeExpectedMarkers(
		const FSourceCase& SourceCase,
		const FPositionCase& PositionCase)
	{
		TArray<int32> Expected;
		AppendExpectedOwnerMarkers(Expected, true, SourceCase, PositionCase);
		if ((IsSource(SourceCase, "owner_literal_assignment")
			|| IsSource(SourceCase, "owner_source_assignment"))
			&& PositionCase.bTargetInBase)
		{
			Expected.Add(AssignmentMarker(SourceCase, true));
		}
		AppendExpectedOwnerMarkers(Expected, false, SourceCase, PositionCase);
		if (((IsSource(SourceCase, "owner_literal_assignment")
			|| IsSource(SourceCase, "owner_source_assignment"))
			&& !PositionCase.bTargetInBase)
			|| IsSource(SourceCase, "derived_reassignment"))
		{
			Expected.Add(AssignmentMarker(SourceCase, PositionCase.bTargetInBase));
		}
		return Expected;
	}

	static TArray<int32> MakeExpectedEventCodes(
		const FSourceCase& SourceCase,
		const FPositionCase& PositionCase)
	{
		TArray<int32> Expected;
		TArray<int32> BaseMarkers;
		AppendExpectedOwnerMarkers(BaseMarkers, true, SourceCase, PositionCase);
		Expected.Append(BaseMarkers);
		Expected.Add(1000);
		if ((IsSource(SourceCase, "owner_literal_assignment")
			|| IsSource(SourceCase, "owner_source_assignment"))
			&& PositionCase.bTargetInBase)
		{
			Expected.Add(AssignmentMarker(SourceCase, true));
		}
		Expected.Add(1001);
		TArray<int32> DerivedMarkers;
		AppendExpectedOwnerMarkers(DerivedMarkers, false, SourceCase, PositionCase);
		Expected.Append(DerivedMarkers);
		Expected.Add(1002);
		if (((IsSource(SourceCase, "owner_literal_assignment")
			|| IsSource(SourceCase, "owner_source_assignment"))
			&& !PositionCase.bTargetInBase)
			|| IsSource(SourceCase, "derived_reassignment"))
		{
			Expected.Add(AssignmentMarker(SourceCase, PositionCase.bTargetInBase));
		}
		Expected.Add(1003);
		return Expected;
	}

	void VerifyMetadata(
		const FNativeCaseContext& Case,
		asIScriptModule& Module,
		const FPositionCase& PositionCase,
		const FNativeTypeCase& TypeCase)
	{
		asITypeInfo* const BaseType = Module.GetTypeInfoByName("FPropertyInitializationBase");
		asITypeInfo* const DerivedType = Module.GetTypeInfoByName("FPropertyInitializationDerived");
		ASSERT_THAT(IsNotNull(BaseType,
			*Case.Describe(TEXT("property-initialization module should publish its base owner"))));
		ASSERT_THAT(IsNotNull(DerivedType,
			*Case.Describe(TEXT("property-initialization module should publish its derived owner"))));
		if (BaseType == nullptr || DerivedType == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(BaseType, DerivedType->GetBaseType(),
			*Case.Describe(TEXT("property-initialization owner should preserve its exact base relation"))));
		ASSERT_THAT(AreEqual(5, static_cast<int32>(BaseType->GetPropertyCount()),
			*Case.Describe(TEXT("base owner should expose exactly five ordered fields"))));
		ASSERT_THAT(AreEqual(10, static_cast<int32>(DerivedType->GetPropertyCount()),
			*Case.Describe(TEXT("derived owner should expose five inherited and five declared fields"))));

		asITypeInfo* const OwnerType = PositionCase.bTargetInBase ? BaseType : DerivedType;
		// This fork returns derived declarations before inherited fields in the
		// flattened property list, so each owner's declaration index is preserved.
		const asUINT ReflectedIndex = static_cast<asUINT>(PositionCase.OwnerPropertyIndex);
		const char* PropertyName = nullptr;
		int TypeId = asTYPEID_VOID;
		ASSERT_THAT(IsTrue(OwnerType->GetProperty(ReflectedIndex, &PropertyName, &TypeId) >= 0,
			*Case.Describe(TEXT("target owner should expose the requested reflected field index"))));
		ASSERT_THAT(IsTrue(PropertyName != nullptr
			&& FCStringAnsi::Strcmp(PropertyName, "Observed") == 0,
			*Case.Describe(TEXT("requested reflected index should identify Observed exactly"))));
		ASSERT_THAT(AreEqual(Module.GetTypeIdByDecl(TypeCase.ScriptType), TypeId,
			*Case.Describe(TEXT("Observed metadata should preserve the exact catalog type"))));
		if (!PositionCase.bTargetInBase)
		{
			const char* FirstInheritedName = nullptr;
			ASSERT_THAT(IsTrue(DerivedType->GetProperty(5, &FirstInheritedName) >= 0,
				*Case.Describe(TEXT("derived reflection should expose the first inherited field after declared fields"))));
			ASSERT_THAT(IsTrue(FirstInheritedName != nullptr
				&& FCStringAnsi::Strcmp(FirstInheritedName, "BaseField0") == 0,
				*Case.Describe(TEXT("derived reflection should retain declared-before-inherited field ordering"))));
		}

		asIScriptFunction* const Entry =
			Module.GetFunctionByDecl("int RunPropertyInitialization()");
		asIScriptFunction* const Recovery =
			Module.GetFunctionByDecl("int RunPropertyInitializationRecovery()");
		ASSERT_THAT(IsNotNull(Entry,
			*Case.Describe(TEXT("property-initialization module should expose its exact entry"))));
		ASSERT_THAT(IsNotNull(Recovery,
			*Case.Describe(TEXT("property-initialization module should expose its exact recovery entry"))));
	}

	void VerifyCheckpoints(
		const FSourceCase& SourceCase,
		const FPositionCase& PositionCase,
		const FNativeTypeCase& TypeCase,
		const FPropertyInitializationState& State)
	{
		ASSERT_THAT(AreEqual(4, State.Checkpoints.Num(),
			*FString::Printf(
				TEXT("[%hs/%hs/%hs] construction should record exactly four checkpoints"),
				SourceCase.CatalogName,
				PositionCase.CatalogName,
				TypeCase.CatalogName)));
		for (const FObservationCase& ObservationCase : ObservationCases)
		{
			const FNativeCaseContext Case(MakeObservationCaseId(
				ObservationCase,
				PositionCase,
				SourceCase,
				TypeCase));
			if (!State.Checkpoints.IsValidIndex(ObservationCase.Stage))
			{
				ASSERT_THAT(IsTrue(false,
					*Case.Describe(TEXT("requested construction checkpoint should exist"))));
				continue;
			}
			const FInitializationCheckpoint& Actual = State.Checkpoints[ObservationCase.Stage];
			ASSERT_THAT(AreEqual(ObservationCase.Stage, Actual.Stage,
				*Case.Describe(TEXT("construction checkpoints should retain base-to-derived order"))));
			ASSERT_THAT(AreEqual(
				ExpectedAvailability(PositionCase, ObservationCase),
				Actual.bAvailable,
				*Case.Describe(TEXT("checkpoint should report whether the target owner is constructed"))));
			ASSERT_THAT(AreEqual(
				ExpectedCheckpointValue(SourceCase, PositionCase, ObservationCase),
				Actual.Value,
				*Case.Describe(TEXT("checkpoint should preserve the source-specific property value"))));
		}
	}

	void VerifyEventOrder(
		const FNativeCaseContext& Case,
		const FSourceCase& SourceCase,
		const FPositionCase& PositionCase,
		const FPropertyInitializationState& State)
	{
		const TArray<int32> ExpectedMarkers = MakeExpectedMarkers(SourceCase, PositionCase);
		ASSERT_THAT(AreEqual(ExpectedMarkers.Num(), State.Markers.Num(),
			*Case.Describe(TEXT("property initialization should record every expected field and assignment marker"))));
		for (int32 Index = 0; Index < FMath::Min(ExpectedMarkers.Num(), State.Markers.Num()); ++Index)
		{
			ASSERT_THAT(AreEqual(ExpectedMarkers[Index], State.Markers[Index],
				*Case.Describe(TEXT("field and assignment markers should follow declaration and constructor order"))));
		}

		const TArray<int32> ExpectedEvents = MakeExpectedEventCodes(SourceCase, PositionCase);
		ASSERT_THAT(AreEqual(ExpectedEvents.Num(), State.Events.Num(),
			*Case.Describe(TEXT("property initialization should record the complete ordered event trace"))));
		for (int32 Index = 0; Index < FMath::Min(ExpectedEvents.Num(), State.Events.Num()); ++Index)
		{
			ASSERT_THAT(AreEqual(ExpectedEvents[Index], State.Events[Index].Code,
				*Case.Describe(TEXT("field initialization and checkpoint events should retain object-graph order"))));
			const bool bExpectedCheckpoint = ExpectedEvents[Index] >= 1000;
			ASSERT_THAT(AreEqual(
				bExpectedCheckpoint,
				State.Events[Index].Kind == EInitializationEventKind::Checkpoint,
				*Case.Describe(TEXT("ordered event trace should preserve marker/checkpoint identity"))));
		}
	}

	void VerifyLifecycle(
		const FNativeCaseContext& Case,
		const FSourceCase& SourceCase,
		const FNativeTypeCase& TypeCase,
		const FNativeLifecycleRecorder& Lifecycle)
	{
		ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
			*Case.Describe(TEXT("property initialization should release every tracked value"))));
		if (!IsObjectValueType(TypeCase))
		{
			ASSERT_THAT(AreEqual(0, Lifecycle.GetEntries().Num(),
				*Case.Describe(TEXT("primitive property initialization should not report object lifecycle events"))));
			return;
		}

		TSet<int32> ConstructedIds;
		TSet<int32> DestructedIds;
		for (const AngelscriptNativeTestSupport::FNativeLifecycleEntry& Entry : Lifecycle.GetEntries())
		{
			if (Entry.Event == ENativeLifecycleEvent::DefaultConstruct
				|| Entry.Event == ENativeLifecycleEvent::ValueConstruct
				|| Entry.Event == ENativeLifecycleEvent::CopyConstruct)
			{
				ASSERT_THAT(IsFalse(ConstructedIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("each initialized property value should own one lifecycle identity"))));
				ConstructedIds.Add(Entry.ObjectId);
			}
			else if (Entry.Event == ENativeLifecycleEvent::Destruct)
			{
				ASSERT_THAT(IsTrue(ConstructedIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("property destruction should reference a constructed identity"))));
				ASSERT_THAT(IsFalse(DestructedIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("property storage should be destroyed no more than once"))));
				DestructedIds.Add(Entry.ObjectId);
			}
		}
		ASSERT_THAT(IsTrue(ConstructedIds.Num() > 0,
			*Case.Describe(TEXT("object-value property should construct observable storage"))));
		ASSERT_THAT(AreEqual(ConstructedIds.Num(), DestructedIds.Num(),
			*Case.Describe(TEXT("every object-value construction should have one destructor"))));
		const bool bAssignmentSource = IsSource(SourceCase, "owner_literal_assignment")
			|| IsSource(SourceCase, "owner_source_assignment")
			|| IsSource(SourceCase, "derived_reassignment");
		if (bAssignmentSource || UsesCurrentForkScriptValueDeclarationAssign(SourceCase, TypeCase))
		{
			ASSERT_THAT(IsTrue(Lifecycle.Num(ENativeLifecycleEvent::Assign) > 0,
				*Case.Describe(TEXT("assignment or current-fork script declaration initialization should invoke opAssign"))));
		}
		else
		{
			ASSERT_THAT(AreEqual(0, Lifecycle.Num(ENativeLifecycleEvent::Assign),
				*Case.Describe(TEXT("default or declaration initialization should not invoke opAssign"))));
		}
		if (IsSource(SourceCase, "owner_source_assignment"))
		{
			ASSERT_THAT(IsTrue(ConstructedIds.Num() >= 2,
				*Case.Describe(TEXT("source assignment should construct target and separate source storage"))));
		}
	}

	void ExecuteCell(
		const FNativeCaseContext& GroupCase,
		const FSourceCase& SourceCase,
		const FPositionCase& PositionCase,
		const FNativeTypeCase& TypeCase,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module,
		FPropertyInitializationState& State,
		FNativeLifecycleRecorder& Lifecycle)
	{
		using namespace AngelscriptNativeTestSupport;

		VerifyMetadata(GroupCase, Module, PositionCase, TypeCase);
		asIScriptFunction* const Entry =
			Module.GetFunctionByDecl("int RunPropertyInitialization()");
		asIScriptFunction* const Recovery =
			Module.GetFunctionByDecl("int RunPropertyInitializationRecovery()");
		if (Entry == nullptr || Recovery == nullptr)
		{
			return;
		}

		asIScriptContext* const Context = ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*GroupCase.Describe(TEXT("property-initialization cell should create a reusable context"))));
		if (Context == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Entry),
			*GroupCase.Describe(TEXT("property-initialization entry should finish"))));
		ASSERT_THAT(AreEqual(ExpectedFinalValue(SourceCase), static_cast<int32>(Context->GetReturnDWord()),
			*GroupCase.Describe(TEXT("property-initialization entry should return the final target value"))));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(),
			*GroupCase.Describe(TEXT("property-initialization context should release constructed storage"))));

		VerifyCheckpoints(SourceCase, PositionCase, TypeCase, State);
		VerifyEventOrder(GroupCase, SourceCase, PositionCase, State);
		VerifyLifecycle(GroupCase, SourceCase, TypeCase, Lifecycle);

		const int32 MarkerCountBeforeRecovery = State.Markers.Num();
		const int32 CheckpointCountBeforeRecovery = State.Checkpoints.Num();
		ASSERT_THAT(IsTrue(Context->Prepare(Recovery) >= 0,
			*GroupCase.Describe(TEXT("property-initialization context should prepare its recovery entry"))));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(),
			*GroupCase.Describe(TEXT("property-initialization recovery should finish in the same context"))));
		ASSERT_THAT(AreEqual(97, static_cast<int32>(Context->GetReturnDWord()),
			*GroupCase.Describe(TEXT("property-initialization recovery should return its clean sentinel"))));
		ASSERT_THAT(AreEqual(MarkerCountBeforeRecovery, State.Markers.Num(),
			*GroupCase.Describe(TEXT("recovery should not replay property initialization markers"))));
		ASSERT_THAT(AreEqual(CheckpointCountBeforeRecovery, State.Checkpoints.Num(),
			*GroupCase.Describe(TEXT("recovery should not replay construction checkpoints"))));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(),
			*GroupCase.Describe(TEXT("property-initialization recovery should unprepare cleanly"))));
		Context->Release();
	}

public:
	TEST_METHOD(TypesBySourcePositionAndObservation)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-PROP-INIT-ORDER",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("Property-initialization product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FPropertyInitializationState State;
		FNativeLifecycleRecorder Lifecycle;
		ASSERT_THAT(IsTrue(RegisterPropertyInitializationBridge(*ScriptEngine, State),
			TEXT("Property-initialization product should register its event bridge")));
		ASSERT_THAT(IsTrue(RegisterNativeCaseValue(*ScriptEngine, Lifecycle),
			TEXT("Property-initialization product should register its tracked native value")));
		ASSERT_THAT(IsTrue(RegisterNativeScriptLifecycleBridge(*ScriptEngine, Lifecycle),
			TEXT("Property-initialization product should register its script lifecycle bridge")));
		ASSERT_THAT(IsTrue(RegisterCoreLanguageTypedef(*ScriptEngine),
			TEXT("Property-initialization product should register its core typedef alias")));

		for (const FSourceCase& SourceCase : SourceCases)
		{
			for (const FPositionCase& PositionCase : PositionCases)
			{
				for (const FNativeTypeCase& TypeCase : NativeTypeCases)
				{
					if (!IsCoreValueTypeCase(TypeCase))
					{
						continue;
					}

					State.Reset();
					Lifecycle.Reset();
					const FString GroupId = MakeNativeCaseId(
						"LANG-PROP-INIT-ORDER",
						{
							TEXT("all_checkpoints"),
							ANSI_TO_TCHAR(PositionCase.CatalogName),
							ANSI_TO_TCHAR(SourceCase.CatalogName),
							ANSI_TO_TCHAR(TypeCase.CatalogName),
						});
					const FNativeCaseContext GroupCase(GroupId);
					const FString Suffix = MakeSuffix(SourceCase, PositionCase, TypeCase);
					const FString ModuleName = TEXT("PropertyInitialization_") + Suffix;
					const FString Source = BuildPropertyInitializationSource(
						SourceCase,
						PositionCase,
						TypeCase);
					PrintGeneratedAsSource(*TestRunner, GroupCase.GetId(), ModuleName, Source);
					const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
					const FTCHARToUTF8 SourceUtf8(*Source);
					Engine.ResetMessages();
					asIScriptModule* Module = nullptr;
					const int BuildResult = CompileNativeModule(
						ScriptEngine,
						ModuleNameUtf8.Get(),
						SourceUtf8.Get(),
						Module);
					ASSERT_THAT(IsTrue(BuildResult >= 0,
						*FString::Printf(TEXT("%s. BuildResult=%d Messages={%s}"),
							*GroupCase.Describe(TEXT("property-initialization cell should compile")),
							BuildResult,
							*Engine.GetMessagesText())));
					ASSERT_THAT(IsNotNull(Module,
						*GroupCase.Describe(TEXT("property-initialization cell should publish a module"))));
					if (BuildResult >= 0 && Module != nullptr)
					{
						ExecuteCell(
							GroupCase,
							SourceCase,
							PositionCase,
							TypeCase,
							*ScriptEngine,
							*Module,
								State,
								Lifecycle);
							if (UsesCurrentForkScriptValueDeclarationAssign(SourceCase, TypeCase))
							{
								TestRunner->AddInfo(FString::Printf(
									TEXT("[AS-FORK-LIMITATION] Id=%s script-value declaration initialization invokes opAssign after construction"),
									*GroupCase.GetId()));
							}
						}

					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					ASSERT_THAT(IsNull(ScriptEngine->GetModule(
						ModuleNameUtf8.Get(),
						asGM_ONLY_IF_EXISTS),
						*GroupCase.Describe(TEXT("property-initialization cell should discard its module"))));
					ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
						*GroupCase.Describe(TEXT("discarded property-initialization cell should leave no live value"))));
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
